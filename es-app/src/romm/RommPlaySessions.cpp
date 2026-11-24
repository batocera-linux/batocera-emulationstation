#include "romm/RommPlaySessions.h"
#include "romm/RommCatalog.h"
#include "romm/RommClient.h"
#include "romm/RommConfig.h"

#include "Log.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <rapidjson/document.h>

#include <atomic>
#include <mutex>
#include <set>
#include <thread>
#include <vector>
#include <algorithm>
#include <cstdio>

namespace Romm
{
	static const size_t MAX_BATCH = 100; // RomM refuses bigger batches

	static std::mutex sQueueMutex;     // the queue file
	static std::mutex sThreadMutex;    // the sender thread
	static std::thread sThread;
	static std::atomic<bool> sSending(false);
	static std::atomic<bool> sMissingPermission(false);
	static std::atomic<bool> sCancel(false);

	struct Session
	{
		int romId = 0;
		long long start = 0; // seconds since epoch, UTC
		long long end = 0;
	};

	std::string RommPlaySessions::getQueuePath()
	{
		return getConfigDirectory() + "/play-sessions.txt";
	}

	static std::vector<Session> readQueue()
	{
		std::vector<Session> sessions;
		std::string path = RommPlaySessions::getQueuePath();
		if (!Utils::FileSystem::exists(path, false))
			return sessions;

		for (auto& line : Utils::String::split(Utils::FileSystem::readAllText(path), '\n', true))
		{
			Session s;
			if (sscanf(line.c_str(), "%d %lld %lld", &s.romId, &s.start, &s.end) == 3 && s.romId > 0 && s.end > s.start)
				sessions.push_back(s);
		}

		return sessions;
	}

	static void writeQueue(const std::vector<Session>& sessions)
	{
		std::string path = RommPlaySessions::getQueuePath();
		if (sessions.empty())
		{
			Utils::FileSystem::removeFile(path);
			return;
		}

		std::string text;
		for (auto& s : sessions)
			text += std::to_string(s.romId) + " " + std::to_string(s.start) + " " + std::to_string(s.end) + "\n";

		Utils::FileSystem::createDirectory(getConfigDirectory());
		writeFileAtomic(path, text);
	}

	static std::string isoUtc(long long t)
	{
		time_t tt = (time_t)t;
		struct tm tmv;
#ifdef WIN32
		gmtime_s(&tmv, &tt);
#else
		gmtime_r(&tt, &tmv);
#endif
		char buf[32];
		strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmv);
		return buf;
	}

	void RommPlaySessions::record(const std::string& gamePath, time_t start, time_t end)
	{
		RommConfig config = RommConfig::load();
		if (!config.syncPlaySessions || !config.isUsable())
			return;

		int romId = RommCatalog::getRomId(gamePath);
		if (romId <= 0 || end <= start)
			return; // not a RomM game

		{
			std::unique_lock<std::mutex> lock(sQueueMutex);
			std::vector<Session> sessions = readQueue();
			sessions.push_back(Session{ romId, (long long)start, (long long)end });
			writeQueue(sessions);
		}

		LOG(LogInfo) << "RomM: play session queued, rom " << romId << ", " << (end - start) << "s";
		flushAsync();
	}

	void RommPlaySessions::flush()
	{
		RommConfig config = RommConfig::load();
		if (!config.syncPlaySessions || !config.isUsable())
			return;

		std::vector<Session> queue;
		{
			std::unique_lock<std::mutex> lock(sQueueMutex);
			queue = readQueue();
		}

		if (queue.empty())
			return;

		RommClient client(config);
		std::set<size_t> done; // sent (created or duplicate) or refused for good

		for (size_t first = 0; first < queue.size(); first += MAX_BATCH)
		{
			size_t last = std::min(queue.size(), first + MAX_BATCH);

			std::string body = "{\"sessions\":[";
			for (size_t i = first; i < last; i++)
			{
				const Session& s = queue[i];
				if (i > first)
					body += ",";

				body += "{\"rom_id\":" + std::to_string(s.romId) +
					",\"start_time\":\"" + isoUtc(s.start) + "\"" +
					",\"end_time\":\"" + isoUtc(s.end) + "\"" +
					",\"duration_ms\":" + std::to_string((s.end - s.start) * 1000LL) + "}";
			}
			body += "]}";

			HttpResult res = client.postJson("/api/play-sessions", body, &sCancel);

			if (res.status == 403)
			{
				// Paired before play sessions were requested: the token lacks roms.user.write
				sMissingPermission = true;
				LOG(LogWarning) << "RomM: this device is not allowed to send play sessions, pair it again";
				break;
			}

			if (!res.ok())
				break; // keep the queue, retried later

			sMissingPermission = false;

			rapidjson::Document doc;
			doc.Parse(res.body.c_str());

			// "created" and "duplicate" are done; "error" (e.g. the game no longer exists) would never succeed
			for (size_t i = first; i < last; i++)
				done.insert(i);

			if (!doc.HasParseError() && doc.IsObject())
			{
				LOG(LogInfo) << "RomM: play sessions sent, " << Json::integer(doc, "created_count", 0) << " created, " << Json::integer(doc, "skipped_count", 0) << " skipped";
			}
		}

		if (done.empty())
			return;

		// Remove what was handled; keep what was queued meanwhile
		std::unique_lock<std::mutex> lock(sQueueMutex);
		std::vector<Session> current = readQueue();
		std::vector<Session> remaining;

		for (auto& s : current)
		{
			bool handled = false;
			for (auto i : done)
			{
				const Session& d = queue[i];
				if (d.romId == s.romId && d.start == s.start && d.end == s.end)
				{
					handled = true;
					break;
				}
			}

			if (!handled)
				remaining.push_back(s);
		}

		writeQueue(remaining);
	}

	void RommPlaySessions::flushAsync()
	{
		std::unique_lock<std::mutex> lock(sThreadMutex);
		if (sSending)
			return; // the running send picks up what is queued now at its next pass

		if (sThread.joinable())
			sThread.join();

		sSending = true;
		sThread = std::thread([]
		{
			flush();
			sSending = false;
		});
	}

	void RommPlaySessions::stop()
	{
		sCancel = true;

		std::thread worker;
		{
			std::unique_lock<std::mutex> lock(sThreadMutex);
			worker = std::move(sThread);
		}

		if (worker.joinable())
			worker.join();

		sCancel = false; // a later send (e.g. after a reload) must not be cancelled
	}

	size_t RommPlaySessions::pendingCount()
	{
		std::unique_lock<std::mutex> lock(sQueueMutex);
		return readQueue().size();
	}

	bool RommPlaySessions::missingPermission()
	{
		return sMissingPermission;
	}
}
