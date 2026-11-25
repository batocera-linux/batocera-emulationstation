#include "romm/RommSaves.h"

#include "romm/RommCatalog.h"
#include "romm/RommClient.h"
#include "romm/RommConfig.h"

#include "FileData.h"
#include "Log.h"
#include "SystemConf.h"
#include "SystemData.h"
#include "Window.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "utils/TimeUtil.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <map>
#include <mutex>
#include <set>
#include <thread>

namespace Romm
{
	static const char* DEFAULT_CHANNEL = "default";  // the web player's
	static const int CHECK_TIMEOUT_SECONDS = 4;      // then the local save is used
	static const int BACKUPS_KEPT = 5;

	static std::mutex sLedgerMutex;
	static std::mutex sOpMutex;          // one save operation at a time
	static std::mutex sCheckedMutex;
	static std::map<std::string, time_t> sChecked;   // path -> when
	static const int CHECKED_SECONDS = 10;
	static std::string sLedgerOverride;

	// --- The decide table ---------------------------------------------------------------------

	SaveAction decideSave(long long held, const std::string& heldPrint, bool heldByChoice,
		long long current, const std::string& currentPrint, bool localEmpty, const std::string& localPrint)
	{
		// No local save is not a change: refilled, never pushed empty
		if (current == 0)
			return localEmpty ? SaveAction::Nothing : SaveAction::Push;

		if (held == 0)
		{
			if (localEmpty)
				return SaveAction::Download;

			return localPrint == currentPrint ? SaveAction::Adopt : SaveAction::Conflict;
		}

		bool dirty = !localEmpty && localPrint != heldPrint;

		if (held == current)
			return dirty ? SaveAction::Push : SaveAction::Nothing;

		// Restored by the player: kept until played
		if (heldByChoice)
			return dirty ? SaveAction::Push : SaveAction::Nothing;

		return dirty ? SaveAction::Conflict : SaveAction::Download;
	}

	// --- Ledger ----------------------------------------------------------------------------------

	struct LedgerEntry
	{
		std::string channel;
		std::string label = DEFAULT_CHANNEL;
		long long held = 0;
		std::string heldPrint;
		bool byChoice = false;
		bool launched = false;   // not pushed yet
		bool dirty = false;      // push failed, retried
		bool hasConflict = false;
		SaveConflict conflict;

		// Last synced autosave, and whether another device made it
		std::string autoHash;
		bool autoFromCloud = false;
		std::string autoDevice;

		// Held snapshot (heldDevice empty: this device)
		bool heldHardcore = false;
		bool heldSave = false;
		std::string heldDevice;
	};

	void RommSaves::setLedgerPath(const std::string& path)
	{
		std::unique_lock<std::mutex> lock(sLedgerMutex);
		sLedgerOverride = path;
	}

	std::string RommSaves::ledgerPath()
	{
		if (!sLedgerOverride.empty())
			return sLedgerOverride;

		RommConfig config = RommConfig::load();
		std::string server = Utils::FileSystem::getFileName(getServerCacheDir(config.baseUrl()));
		return getConfigDirectory() + "/saves/" + server + ".json";
	}

	static std::map<std::string, LedgerEntry> readLedger(const std::string& file)
	{
		std::map<std::string, LedgerEntry> entries;

		rapidjson::Document doc;
		std::string text = readBinaryFile(file);
		if (text.empty() || doc.Parse(text.c_str()).HasParseError() || !doc.IsObject())
			return entries;

		auto games = doc.FindMember("games");
		if (games == doc.MemberEnd() || !games->value.IsObject())
			return entries;

		for (auto it = games->value.MemberBegin(); it != games->value.MemberEnd(); ++it)
		{
			const rapidjson::Value& v = it->value;
			if (!v.IsObject())
				continue;

			LedgerEntry e;
			e.channel = Json::str(v, "channel");
			e.label = Json::str(v, "label");
			if (e.label.empty())
				e.label = DEFAULT_CHANNEL;
			e.held = Json::integer(v, "held");
			e.heldPrint = Json::str(v, "heldPrint");
			e.byChoice = Json::boolean(v, "byChoice");
			e.launched = Json::boolean(v, "launched");
			e.dirty = Json::boolean(v, "dirty");
			e.autoHash = Json::str(v, "autoHash");
			e.autoFromCloud = Json::boolean(v, "autoFromCloud");
			e.autoDevice = Json::str(v, "autoDevice");
			e.heldHardcore = Json::boolean(v, "heldHardcore");
			e.heldSave = Json::boolean(v, "heldSave");
			e.heldDevice = Json::str(v, "heldDevice");

			auto c = v.FindMember("conflict");
			if (c != v.MemberEnd() && c->value.IsObject())
			{
				e.hasConflict = true;
				e.conflict.afterPlay = Json::boolean(c->value, "afterPlay");
				e.conflict.hardcore = Json::boolean(c->value, "hardcore");
				e.conflict.currentId = Json::integer(c->value, "currentId");
				e.conflict.currentDevice = Json::str(c->value, "currentDevice");
				e.conflict.currentDate = Json::str(c->value, "currentDate");
			}

			entries[it->name.GetString()] = e;
		}

		return entries;
	}

	static void writeLedger(const std::string& file, const std::map<std::string, LedgerEntry>& entries)
	{
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> w(buffer);

		w.StartObject();
		w.Key("games");
		w.StartObject();
		for (auto& item : entries)
		{
			const LedgerEntry& e = item.second;
			w.Key(item.first.c_str());
			w.StartObject();
			w.Key("channel"); w.String(e.channel.c_str());
			w.Key("label"); w.String(e.label.c_str());
			w.Key("held"); w.Int64(e.held);
			w.Key("heldPrint"); w.String(e.heldPrint.c_str());
			w.Key("byChoice"); w.Bool(e.byChoice);
			w.Key("launched"); w.Bool(e.launched);
			w.Key("dirty"); w.Bool(e.dirty);
			w.Key("autoHash"); w.String(e.autoHash.c_str());
			w.Key("autoFromCloud"); w.Bool(e.autoFromCloud);
			w.Key("autoDevice"); w.String(e.autoDevice.c_str());
			w.Key("heldHardcore"); w.Bool(e.heldHardcore);
			w.Key("heldSave"); w.Bool(e.heldSave);
			w.Key("heldDevice"); w.String(e.heldDevice.c_str());
			if (e.hasConflict)
			{
				w.Key("conflict");
				w.StartObject();
				w.Key("afterPlay"); w.Bool(e.conflict.afterPlay);
				w.Key("hardcore"); w.Bool(e.conflict.hardcore);
				w.Key("currentId"); w.Int64(e.conflict.currentId);
				w.Key("currentDevice"); w.String(e.conflict.currentDevice.c_str());
				w.Key("currentDate"); w.String(e.conflict.currentDate.c_str());
				w.EndObject();
			}
			w.EndObject();
		}
		w.EndObject();
		w.EndObject();

		Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(file));
		writeFileAtomic(file, buffer.GetString());
	}

	static LedgerEntry getEntry(const std::string& path)
	{
		std::unique_lock<std::mutex> lock(sLedgerMutex);
		auto entries = readLedger(RommSaves::ledgerPath());
		auto it = entries.find(path);
		return it == entries.cend() ? LedgerEntry() : it->second;
	}

	static void putEntry(const std::string& path, const LedgerEntry& entry)
	{
		std::unique_lock<std::mutex> lock(sLedgerMutex);
		std::string file = RommSaves::ledgerPath();
		auto entries = readLedger(file);
		entries[path] = entry;
		writeLedger(file, entries);
	}

	// --- Server side -----------------------------------------------------------------------------

	struct ServerFile
	{
		std::string hash;
		std::string downloadPath;
		std::string fileName;
		std::string shape;
		std::string screenshotPath;
	};

	struct ServerSnapshot
	{
		long long id = 0;
		std::string device;
		std::string date;
		bool hasSave = false;
		bool hardcore = false;                     // no states then
		ServerFile save;
		std::map<std::string, ServerFile> states;  // this core's slots
		std::set<std::string> stateHashes;        // any core
	};

	struct ServerChannel
	{
		bool found = false;
		std::string id;
		std::string label;
		ServerSnapshot current;
		std::set<std::string> ownLabels;
	};

	static ServerSnapshot parseSnapshot(const rapidjson::Value& v, const std::string& bankCore, const std::string& bankAlias)
	{
		ServerSnapshot s;
		decltype(s.states) aliasStates;
		if (!v.IsObject())
			return s;

		s.id = Json::integer(v, "id");
		s.date = Json::str(v, "created_at");
		s.hardcore = Json::boolean(v, "is_hardcore");

		auto device = v.FindMember("device");
		if (device != v.MemberEnd() && device->value.IsObject())
			s.device = Json::str(device->value, "name");

		auto save = v.FindMember("save");
		if (save != v.MemberEnd() && save->value.IsObject())
		{
			s.hasSave = true;
			s.save.hash = Utils::String::toLower(Json::str(save->value, "content_hash"));
			s.save.downloadPath = Json::str(save->value, "download_path");
			s.save.fileName = Json::str(save->value, "file_name");
			s.save.shape = Json::str(save->value, "shape");
		}

		auto states = v.FindMember("states");
		if (states != v.MemberEnd() && states->value.IsObject())
		{
			for (auto core = states->value.MemberBegin(); core != states->value.MemberEnd(); ++core)
			{
				if (!core->value.IsObject())
					continue;

				for (auto slot = core->value.MemberBegin(); slot != core->value.MemberEnd(); ++slot)
				{
					if (!slot->value.IsObject())
						continue;

					ServerFile f;
					f.hash = Utils::String::toLower(Json::str(slot->value, "content_hash"));
					f.downloadPath = Json::str(slot->value, "download_path");
					f.fileName = Json::str(slot->value, "file_name");

					auto shot = slot->value.FindMember("screenshot");
					if (shot != slot->value.MemberEnd() && shot->value.IsObject())
						f.screenshotPath = Json::str(shot->value, "download_path");

					s.stateHashes.insert(f.hash);
					if (bankCore == core->name.GetString())
						s.states[slot->name.GetString()] = f;
					else if (!bankAlias.empty() && bankAlias == core->name.GetString())
						aliasStates[slot->name.GetString()] = f;
				}
			}
		}

		// Pushed earlier under Batocera's core name
		if (s.states.empty())
			s.states = aliasStates;

		return s;
	}

	// LocalSaveUnit::fingerprint of a snapshot
	static std::string snapshotPrint(const ServerSnapshot& s, const SaveLayout& layout)
	{
		LocalSaveUnit unit;
		if (layout.hasSave() && s.hasSave)
			unit.saveHash = s.save.hash;

		for (auto& slot : s.states)
		{
			LocalState state;
			state.hash = slot.second.hash;
			unit.states[slot.first] = state;
		}

		return unit.fingerprint();
	}

	// Plain 404: no save channels on this server (unlike "ROM file not found")
	static bool noChannelsEndpoint(const HttpResult& r)
	{
		return r.status == 404 && errorDetail(r) == "Not Found";
	}

	static bool unreachable(const HttpResult& r)
	{
		return r.status == 3 || r.status == 4 || r.status == -1 || r.status == 0 || r.status == 408 || r.status == 429 || r.status >= 500;
	}

	static bool fetchChannel(RommClient& client, const SaveJob& job, const std::string& label, int timeout, ServerChannel& channel, HttpResult& result)
	{
		result = client.get("/api/channels?rom_file_id=" + std::to_string(job.romFileId), nullptr, timeout);
		if (!result.ok())
			return false;

		rapidjson::Document doc;
		if (doc.Parse(result.body.c_str()).HasParseError() || !doc.IsArray())
		{
			result.status = 0;
			return false;
		}

		for (auto& c : doc.GetArray())
		{
			if (!c.IsObject() || !Json::boolean(c, "is_own"))
				continue;

			std::string name = Json::str(c, "label");
			channel.ownLabels.insert(name);
			if (name != label || channel.found)
				continue;

			channel.found = true;
			channel.id = Json::str(c, "id");
			channel.label = name;

			auto current = c.FindMember("current");
			if (current != c.MemberEnd() && current->value.IsObject())
				channel.current = parseSnapshot(current->value, job.layout.bankCore, job.layout.bankAlias);
		}

		return true;
	}

	static bool createChannel(RommClient& client, const SaveJob& job, const std::string& label, std::string& id)
	{
		HttpResult r = client.postJson("/api/channels", "{\"rom_file_id\":" + std::to_string(job.romFileId) + ",\"label\":\"" + Json::escape(label) + "\"}");
		if (!r.ok())
		{
			LOG(LogWarning) << "RomM: channel " << label << " not created for " << job.path << ": " << errorDetail(r);
			return false;
		}

		rapidjson::Document doc;
		if (doc.Parse(r.body.c_str()).HasParseError() || !doc.IsObject())
			return false;

		id = Json::str(doc, "id");
		return !id.empty();
	}

	enum class PushOutcome { Pushed, Branched, Hardcore, Unreachable, Failed };

	struct PushResult
	{
		PushOutcome outcome = PushOutcome::Failed;
		long long snapshotId = 0;
		ServerSnapshot current;   // when Branched
		std::string error;
	};

	static std::string fileTime(const std::string& path)
	{
		time_t t = Utils::FileSystem::getFileModificationDate(path).getTime();
		char buf[32];
		strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", localtime(&t));
		return buf;
	}

	// expected: the current it builds on (0: empty channel)
	static PushResult push(RommClient& client, const SaveJob& job, const LocalSaveUnit& unit, const std::string& channelId,
		const std::string& label, long long expected, long long parent, bool approveHardcore, const ServerSnapshot* current)
	{
		PushResult result;
		const SaveLayout& layout = job.layout;

		std::string saveBytes;
		bool withSave = layout.hasSave() && unit.hasSave() && RommSaveLayout::packSave(layout, unit, saveBytes);

		bool withStates = !job.hardcore && layout.hasStates() && (!unit.states.empty() || (current != nullptr && !current->states.empty()));

		// A field left out is carried from the parent
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> w(buffer);
		w.StartObject();
		w.Key("rom_file_id"); w.Int(job.romFileId);
		if (!channelId.empty()) { w.Key("channel_id"); w.String(channelId.c_str()); }
		else { w.Key("label"); w.String(label.c_str()); }
		w.Key("expected_current_id"); if (expected > 0) w.Int64(expected); else w.Null();
		w.Key("parent_snapshot_id"); if (parent > 0) w.Int64(parent); else w.Null();
		if (withSave)
		{
			w.Key("save");
			w.StartObject();
			w.Key("hash"); w.String(unit.saveHash.c_str());
			w.Key("shape"); w.String(unit.saveShape.c_str());
			w.Key("format"); w.String("native");
			w.EndObject();
		}
		if (withStates)
		{
			// Replaces the core's bank
			w.Key("states");
			w.StartObject();
			w.Key(layout.bankCore.c_str());
			w.StartObject();
			for (auto& state : unit.states)
			{
				w.Key(state.first.c_str());
				w.String(state.second.hash.c_str());
			}
			w.EndObject();
			w.EndObject();
		}
		w.Key("is_hardcore"); w.Bool(job.hardcore);
		w.Key("approve_hardcore_downgrade"); w.Bool(approveHardcore);
		w.Key("emulator"); w.String(layout.emulator.c_str());
		if (!layout.core.empty()) { w.Key("core"); w.String(layout.emulator == "libretro" ? layout.bankCore.c_str() : layout.core.c_str()); }
		w.EndObject();

		std::string manifest = buffer.GetString();

		// Only what RomM lacks (it asks with a 400)
		for (int attempt = 0; attempt < 2; attempt++)
		{
			bool all = attempt > 0 || current == nullptr;

			MultipartBody body;
			body.addField("manifest", manifest);

			if (withSave && (all || !current->hasSave || current->save.hash != unit.saveHash))
			{
				std::string name = unit.saveShape == "SINGLE" ? Utils::FileSystem::getFileName(unit.saveFiles[0]) : layout.saveStem + ".zip";
				body.addFile("save", name, saveBytes);
			}

			if (withStates)
			{
				for (auto& state : unit.states)
				{
					if (!all && current->stateHashes.count(state.second.hash))
						continue;

					std::string key = "state:" + layout.bankCore + ":" + state.first;
					body.addFile(key, Utils::FileSystem::getFileName(state.second.path), readBinaryFile(state.second.path));
					if (!state.second.screenshot.empty())
						body.addFile(key + ":screenshot", Utils::FileSystem::getFileName(state.second.screenshot), readBinaryFile(state.second.screenshot));
				}
			}

			HttpResult r = client.postMultipart("/api/snapshots", body.finish(), body.boundary());

			rapidjson::Document doc;
			bool json = !r.body.empty() && !doc.Parse(r.body.c_str()).HasParseError() && doc.IsObject();

			if (r.status == 200 || r.status == 201)
			{
				result.outcome = PushOutcome::Pushed;
				result.snapshotId = json ? Json::integer(doc, "id") : 0;
				if (result.snapshotId <= 0)
				{
					result.outcome = PushOutcome::Failed;
					result.error = "no snapshot in RomM's answer";
				}
				return result;
			}

			if (r.status == 400 && json && doc.HasMember("missing") && attempt == 0)
				continue;

			if (r.status == 409 && json && Json::boolean(doc, "hardcore_downgrade"))
			{
				result.outcome = PushOutcome::Hardcore;
				return result;
			}

			if (r.status == 409 && json)
			{
				result.outcome = PushOutcome::Branched;
				auto cur = doc.FindMember("current");
				if (cur != doc.MemberEnd() && cur->value.IsObject())
					result.current = parseSnapshot(cur->value, layout.bankCore, layout.bankAlias);

				auto branch = doc.FindMember("branch");
				if (branch != doc.MemberEnd() && branch->value.IsObject())
					result.snapshotId = Json::integer(branch->value, "id");
				return result;
			}

			result.outcome = unreachable(r) ? PushOutcome::Unreachable : PushOutcome::Failed;
			result.error = errorDetail(r);
			return result;
		}

		result.error = "content still missing";
		return result;
	}

	static void keepBackup(const SaveJob& job, const LocalSaveUnit& unit)
	{
		if (unit.empty())
			return;

		std::string root = getConfigDirectory() + "/save-backups/" + job.system + "/" + Utils::FileSystem::getStem(job.path);

		char stamp[32];
		time_t now = time(nullptr);
		strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", localtime(&now));
		RommSaveLayout::backup(job.layout, unit, root + "/" + stamp);

		std::vector<std::string> dirs;
		for (auto& dir : Utils::FileSystem::getDirContent(root))
			if (Utils::FileSystem::isDirectory(dir))
				dirs.push_back(dir);

		std::sort(dirs.begin(), dirs.end());
		for (size_t i = 0; i + BACKUPS_KEPT < dirs.size(); i++)
			Utils::FileSystem::deleteDirectoryFiles(dirs[i], true);
	}

	static std::string autoHashOf(const LocalSaveUnit& unit)
	{
		auto it = unit.states.find("auto");
		return it == unit.states.cend() ? "" : it->second.hash;
	}

	// Places snapshot id (local save backed up) and holds it
	static bool download(RommClient& client, const SaveJob& job, long long id, LedgerEntry& entry, std::string& error)
	{
		const SaveLayout& layout = job.layout;

		HttpResult r = client.get("/api/snapshots/" + std::to_string(id) + "?hold=true");
		if (r.status == 400 || r.status == 403)
			r = client.get("/api/snapshots/" + std::to_string(id)); // token without a device

		rapidjson::Document doc;
		if (!r.ok() || doc.Parse(r.body.c_str()).HasParseError() || !doc.IsObject())
		{
			error = r.ok() ? std::string("invalid snapshot") : errorDetail(r);
			return false;
		}

		ServerSnapshot s = parseSnapshot(doc, layout.bankCore, layout.bankAlias);
		LocalSaveUnit before = RommSaveLayout::readLocal(layout);
		keepBackup(job, before);

		if (layout.hasSave() && s.hasSave && !s.save.downloadPath.empty() && s.save.hash != before.saveHash)
		{
			Utils::FileSystem::createDirectory(layout.saveDir);
			std::string part = layout.saveDir + "/." + layout.saveStem + ".romm-save";
			HttpResult d = client.download(s.save.downloadPath, part, nullptr, nullptr);
			if (!d.ok() || !RommSaveLayout::placeSave(layout, part, s.save.shape, s.save.fileName))
			{
				Utils::FileSystem::removeFile(part);
				error = "save not downloaded: " + errorDetail(d);
				return false;
			}
			Utils::FileSystem::removeFile(part);
		}

		// A hardcore snapshot has no states: local ones kept
		if (layout.hasStates() && !s.hardcore)
		{
			Utils::FileSystem::createDirectory(layout.statesDir);

			// Another size: another core build. Newer builds mostly load older states, so only logged
			long long ownSize = -1;
			for (auto& local : before.states)
				if ((ownSize = RommSaveLayout::stateCoreSize(local.second.path)) > 0)
					break;

			struct Fetched { std::string slot, part, target; };
			std::vector<Fetched> fetched;
			long long otherSize = -1;

			for (auto& slot : s.states)
			{
				auto local = before.states.find(slot.first);
				if (local != before.states.cend() && local->second.hash == slot.second.hash)
					continue;

				std::string target = RommSaveLayout::stateFile(layout, slot.first);
				if (target.empty() || slot.second.downloadPath.empty())
					continue;

				std::string part = target + ".romm-part";
				HttpResult d = client.download(slot.second.downloadPath, part, nullptr, nullptr);
				if (!d.ok())
				{
					for (auto& f : fetched)
						Utils::FileSystem::removeFile(f.part);
					Utils::FileSystem::removeFile(part);
					error = "state " + slot.first + " not downloaded: " + errorDetail(d);
					return false;
				}

				fetched.push_back({ slot.first, part, target });

				long long size = RommSaveLayout::stateCoreSize(part);
				if (ownSize > 0 && size > 0 && size != ownSize)
					otherSize = size;
			}

			if (otherSize > 0)
				LOG(LogInfo) << "RomM: states of " << job.path << " from " << (s.device.empty() ? std::string("another device") : s.device)
					<< " come from another build of " << layout.bankCore << " (" << otherSize << " vs " << ownSize
					<< " bytes): placed, the previous ones are in the backup";

			{
				for (auto& f : fetched)
				{
					Utils::FileSystem::renameFile(f.part, f.target);

					const ServerFile& state = s.states.at(f.slot);
					if (!state.screenshotPath.empty())
						client.download(state.screenshotPath, RommSaveLayout::stateScreenshot(layout, f.slot), nullptr, nullptr);
				}

				// Slots not in the snapshot (backed up)
				for (auto& local : before.states)
				{
					if (s.states.count(local.first))
						continue;

					Utils::FileSystem::removeFile(local.second.path);
					if (!local.second.screenshot.empty())
						Utils::FileSystem::removeFile(local.second.screenshot);
				}
			}
		}

		Utils::FileSystem::FileSystemCache::reset();

		LocalSaveUnit after = RommSaveLayout::readLocal(layout);
		std::string autoNow = autoHashOf(after);
		if (!autoNow.empty() && autoNow != autoHashOf(before))
		{
			entry.autoFromCloud = true;
			entry.autoDevice = s.device;
		}
		else if (autoNow.empty())
			entry.autoFromCloud = false;
		entry.autoHash = autoNow;

		entry.held = s.id;
		entry.heldPrint = after.fingerprint();
		entry.heldHardcore = s.hardcore;
		entry.heldSave = s.hasSave;
		entry.heldDevice = s.device;
		entry.byChoice = false;
		entry.dirty = false;
		entry.hasConflict = false;
		LOG(LogInfo) << "RomM: save of " << job.path << " downloaded (snapshot " << s.id << ")";
		return true;
	}

	// This device's save is now the held one
	static void heldOwn(LedgerEntry& entry, long long id, const LocalSaveUnit& unit, bool hardcore)
	{
		entry.heldHardcore = hardcore;
		entry.heldSave = unit.hasSave();
		entry.heldDevice.clear();
		entry.held = id;
		entry.heldPrint = unit.fingerprint();
		entry.byChoice = false;
		entry.autoHash = autoHashOf(unit);
		entry.autoFromCloud = false;
		entry.autoDevice.clear();
	}

	static void storeConflict(LedgerEntry& entry, const ServerSnapshot& current, bool afterPlay, bool hardcore)
	{
		entry.hasConflict = true;
		entry.conflict.afterPlay = afterPlay;
		entry.conflict.hardcore = hardcore;
		entry.conflict.currentId = current.id;
		entry.conflict.currentDevice = current.device;
		entry.conflict.currentDate = current.date;
	}

	static void fillConflict(const SaveJob& job, const LedgerEntry& entry, const LocalSaveUnit& unit, SaveConflict& conflict)
	{
		conflict = entry.conflict;
		conflict.gamePath = job.path;
		conflict.gameName = job.name;

		std::string newest;
		for (auto& file : unit.saveFiles)
			newest = std::max(newest, fileTime(file));
		for (auto& state : unit.states)
			newest = std::max(newest, fileTime(state.second.path));
		conflict.localDate = newest;
	}

	// --- Public ------------------------------------------------------------------------------------

	bool RommSaves::isEnabled()
	{
#ifdef BATOCERA
		RommConfig config = RommConfig::load();
		return config.isUsable() && config.syncSaves;
#else
		return false; // Batocera only (RommSaveLayout::forGame)
#endif
	}

	bool RommSaves::prepare(FileData* game, SaveJob& job)
	{
		if (game == nullptr || game->getType() != GAME || game->isRemote() || !isEnabled())
			return false;

		FileData* source = game->getSourceFileData();
		RemoteGame remote;
		if (!RommCatalog::getKnownGame(source->getPath(), remote) || remote.fileId <= 0)
		{
			LOG(LogInfo) << "RomM: saves of " << source->getPath() << " not synced: not a game of the RomM library (not found under this path)";
			return false;
		}

		job.layout = RommSaveLayout::forGame(source);
		if (!job.layout.valid)
		{
			LOG(LogInfo) << "RomM: saves of " << source->getPath() << " not synced: no save layout for emulator "
				<< job.layout.emulator << (job.layout.core.empty() ? "" : "/" + job.layout.core);
			return false;
		}

		job.path = source->getPath();
		job.name = source->getName();
		job.system = source->getSystem() != nullptr ? source->getSystem()->getName() : "";
		job.romFileId = remote.fileId;
		job.hardcore = isHardcoreSession(game);
		return true;
	}

	SaveCheck RommSaves::beforeLaunch(const SaveJob& job, SaveConflict& conflict, std::string& message)
	{
		std::unique_lock<std::mutex> op(sOpMutex);

		LedgerEntry entry = getEntry(job.path);
		LocalSaveUnit unit = RommSaveLayout::readLocal(job.layout);

		if (entry.hasConflict)
		{
			fillConflict(job, entry, unit, conflict);
			return SaveCheck::Conflict;
		}

		RommClient client(RommConfig::load());
		ServerChannel channel;
		HttpResult r;
		if (!fetchChannel(client, job, entry.label, CHECK_TIMEOUT_SECONDS, channel, r))
		{
			if (noChannelsEndpoint(r))
			{
				LOG(LogWarning) << "RomM: this server has no save channels (RomM 5198 needed), saves of " << job.path << " not synced";
				return SaveCheck::Ready;
			}

			entry.launched = true;
			putEntry(job.path, entry);

			if (unreachable(r))
			{
				std::string reason = r.status == -1 || r.status == 0 ? "server not reached in " + std::to_string(CHECK_TIMEOUT_SECONDS) + " s" : "RomM not reached: " + errorDetail(r);
				LOG(LogWarning) << "RomM: saves of " << job.path << " not checked before launch (" << reason << "), local save used";
				return SaveCheck::Offline;
			}

			message = errorDetail(r);
			LOG(LogWarning) << "RomM: saves of " << job.path << " not checked before launch (rom file " << job.romFileId << "): " << message;
			return SaveCheck::Failed;
		}

		if (channel.found)
			entry.channel = channel.id;

		const ServerSnapshot& current = channel.current;
		SaveAction action = decideSave(entry.held, entry.heldPrint, entry.byChoice, current.id, snapshotPrint(current, job.layout), unit.empty(), unit.fingerprint());

		switch (action)
		{
		case SaveAction::Nothing:
			break;

		case SaveAction::Adopt:
			// Identical: held, nothing moves
			client.get("/api/snapshots/" + std::to_string(current.id) + "?hold=true");
			heldOwn(entry, current.id, unit, current.hardcore);
			break;

		case SaveAction::Download:
			if (!download(client, job, current.id, entry, message))
			{
				LOG(LogWarning) << "RomM: save of " << job.path << " not downloaded before launch: " << message;
				entry.launched = true;
				putEntry(job.path, entry);
				return SaveCheck::Failed;
			}
			break;

		case SaveAction::Push:
		{
			// Changed here since the last sync
			PushResult p = push(client, job, unit, entry.channel, entry.label, current.id, entry.held, false, current.id > 0 ? &current : nullptr);
			if (p.outcome == PushOutcome::Pushed)
			{
				heldOwn(entry, p.snapshotId, unit, job.hardcore);
				entry.dirty = false;
			}
			else if (p.outcome == PushOutcome::Branched || p.outcome == PushOutcome::Hardcore)
			{
				storeConflict(entry, p.outcome == PushOutcome::Branched ? p.current : current, false, p.outcome == PushOutcome::Hardcore);
				putEntry(job.path, entry);
				fillConflict(job, entry, unit, conflict);
				return SaveCheck::Conflict;
			}
			else
			{
				entry.dirty = true;
				LOG(LogWarning) << "RomM: save of " << job.path << " not sent before launch: " << p.error;
			}
			break;
		}

		case SaveAction::Conflict:
			storeConflict(entry, current, false, false);
			putEntry(job.path, entry);
			fillConflict(job, entry, unit, conflict);
			LOG(LogInfo) << "RomM: save conflict for " << job.path << " (held " << entry.held << ", current " << current.id << ")";
			return SaveCheck::Conflict;
		}

		entry.launched = true;
		putEntry(job.path, entry);
		return SaveCheck::Ready;
	}

	bool RommSaves::afterGame(const SaveJob& job, std::string& message)
	{
		std::unique_lock<std::mutex> op(sOpMutex);

		LedgerEntry entry = getEntry(job.path);
		LocalSaveUnit unit = RommSaveLayout::readLocal(job.layout);

		bool changed = !unit.empty() && unit.fingerprint() != entry.heldPrint;
		if (!changed)
		{
			entry.launched = false;
			entry.dirty = false;
			putEntry(job.path, entry);
			return true;
		}

		// A hardcore snapshot is the save alone
		if (job.hardcore && !(job.layout.hasSave() && unit.hasSave()))
		{
			LOG(LogInfo) << "RomM: hardcore session of " << job.path << " left no in-game save, nothing sent";
			entry.launched = false;
			putEntry(job.path, entry);
			return true;
		}

		RommClient client(RommConfig::load());
		ServerChannel channel;
		HttpResult r;
		bool fetched = fetchChannel(client, job, entry.label, 0, channel, r);
		if (!fetched && noChannelsEndpoint(r))
			return true; // no save channels on this server

		if (channel.found)
			entry.channel = channel.id;

		// Another device pushed meanwhile: RomM keeps this as a branch (409), asked at the next launch
		PushResult p = push(client, job, unit, entry.channel, entry.label, entry.held, entry.held, false,
			fetched && channel.current.id > 0 ? &channel.current : nullptr);

		switch (p.outcome)
		{
		case PushOutcome::Pushed:
			heldOwn(entry, p.snapshotId, unit, job.hardcore);
			entry.launched = false;
			entry.dirty = false;
			LOG(LogInfo) << "RomM: save of " << job.path << " sent (snapshot " << p.snapshotId << ")";
			break;

		case PushOutcome::Branched:
		case PushOutcome::Hardcore:
			storeConflict(entry, p.outcome == PushOutcome::Branched ? p.current : channel.current, true, p.outcome == PushOutcome::Hardcore);
			entry.launched = false;
			entry.dirty = false;
			LOG(LogWarning) << "RomM: save of " << job.path << " kept by RomM beside the current one (conflict, asked at the next launch)";
			break;

		default:
			entry.dirty = true;
			message = p.error;
			LOG(LogWarning) << "RomM: save of " << job.path << " not sent, retried later: " << p.error;
			break;
		}

		putEntry(job.path, entry);
		return p.outcome == PushOutcome::Pushed;
	}

	bool RommSaves::resolve(const SaveJob& job, SaveChoice choice, std::string& message)
	{
		std::unique_lock<std::mutex> op(sOpMutex);

		LedgerEntry entry = getEntry(job.path);
		LocalSaveUnit unit = RommSaveLayout::readLocal(job.layout);

		RommClient client(RommConfig::load());
		ServerChannel channel;
		HttpResult r;
		if (!fetchChannel(client, job, entry.label, 0, channel, r))
		{
			message = errorDetail(r);
			return false;
		}

		if (channel.found)
			entry.channel = channel.id;

		const ServerSnapshot& current = channel.current;
		bool done = false;

		switch (choice)
		{
		case SaveChoice::UseServer:
			// The local save goes to RomM first, as a branch
			if (!entry.conflict.afterPlay && !unit.empty() && unit.fingerprint() != entry.heldPrint && current.id > 0)
				push(client, job, unit, entry.channel, entry.label, entry.held, entry.held, false, &current);

			done = current.id > 0 && download(client, job, current.id, entry, message);
			break;

		case SaveChoice::KeepLocal:
		case SaveChoice::ApproveHardcore:
		{
			PushResult p = push(client, job, unit, entry.channel, entry.label, current.id, entry.held,
				choice == SaveChoice::ApproveHardcore, current.id > 0 ? &current : nullptr);
			done = p.outcome == PushOutcome::Pushed;
			if (done)
				heldOwn(entry, p.snapshotId, unit, job.hardcore);
			else if (p.outcome == PushOutcome::Hardcore)
				storeConflict(entry, current, entry.conflict.afterPlay, true);
			else
				message = p.error;
			break;
		}

		case SaveChoice::KeepBoth:
		{
			// "<device> <date>"
			char day[16];
			time_t now = time(nullptr);
			strftime(day, sizeof(day), "%Y-%m-%d", localtime(&now));
			std::string label = getDeviceName() + " " + day;
			for (int n = 2; channel.ownLabels.count(label); n++)
				label = getDeviceName() + " " + day + " " + std::to_string(n);

			std::string id;
			if (!createChannel(client, job, label, id))
			{
				message = "channel not created";
				break;
			}

			PushResult p = push(client, job, unit, id, label, 0, entry.held, false, nullptr);
			done = p.outcome == PushOutcome::Pushed;
			if (done)
			{
				entry.channel = id;
				entry.label = label;
				heldOwn(entry, p.snapshotId, unit, job.hardcore);
				LOG(LogInfo) << "RomM: save of " << job.path << " now in channel " << label;
			}
			else
				message = p.error;
			break;
		}
		}

		if (done)
		{
			entry.hasConflict = false;
			entry.dirty = false;
		}

		putEntry(job.path, entry);
		return done;
	}

	bool RommSaves::isHardcoreSession(FileData* game)
	{
#ifdef BATOCERA
		if (game == nullptr || game->getType() != GAME || !isEnabled())
			return false;

		// es_features writes 1 (Batocera) or true (RetroBat)
		std::string hardcore = Utils::String::toLower(game->getCurrentGameSetting("retroachievements.hardcore"));
		return SystemConf::getInstance()->getBool("global.retroachievements") &&
			(hardcore == "1" || hardcore == "true" || hardcore == "on" || hardcore == "enabled") && game->hasCheevos();
#else
		(void)game;
		return false;
#endif
	}

	bool RommSaves::softcoreSaveForHardcore(const SaveJob& job, std::string& device)
	{
		if (!job.hardcore || !job.layout.hasSave())
			return false;

		LedgerEntry entry = getEntry(job.path);
		if (entry.held <= 0 || entry.heldHardcore || !entry.heldSave)
			return false;

		device = entry.heldDevice;
		return true;
	}

	bool RommSaves::replacedCloudAutosave(const SaveJob& job, SaveConflict& conflict)
	{
		LedgerEntry entry = getEntry(job.path);
		if (!entry.autoFromCloud || entry.autoHash.empty() || !job.layout.hasStates())
			return false;

		std::string now = autoHashOf(RommSaveLayout::readLocal(job.layout));
		if (now.empty() || now == entry.autoHash)
			return false;

		conflict = SaveConflict();
		conflict.gamePath = job.path;
		conflict.gameName = job.name;
		conflict.currentDevice = entry.autoDevice;
		return true;
	}

	bool RommSaves::restoreHeldSlot(const SaveJob& job, const std::string& slot, std::string& message)
	{
		std::unique_lock<std::mutex> op(sOpMutex);

		LedgerEntry entry = getEntry(job.path);
		if (entry.held <= 0)
			return false;

		RommClient client(RommConfig::load());
		HttpResult r = client.get("/api/snapshots/" + std::to_string(entry.held));
		rapidjson::Document doc;
		if (!r.ok() || doc.Parse(r.body.c_str()).HasParseError() || !doc.IsObject())
		{
			message = errorDetail(r);
			return false;
		}

		ServerSnapshot s = parseSnapshot(doc, job.layout.bankCore, job.layout.bankAlias);
		auto it = s.states.find(slot);
		if (it == s.states.cend() || it->second.downloadPath.empty())
		{
			message = "no such state in RomM";
			return false;
		}

		keepBackup(job, RommSaveLayout::readLocal(job.layout));

		std::string target = RommSaveLayout::stateFile(job.layout, slot);
		HttpResult d = client.download(it->second.downloadPath, target, nullptr, nullptr);
		if (!d.ok())
		{
			message = errorDetail(d);
			return false;
		}

		if (!it->second.screenshotPath.empty())
			client.download(it->second.screenshotPath, RommSaveLayout::stateScreenshot(job.layout, slot), nullptr, nullptr);

		Utils::FileSystem::FileSystemCache::reset();
		LOG(LogInfo) << "RomM: " << slot << " state of " << job.path << " put back from snapshot " << entry.held;
		return true;
	}

	bool RommSaves::pendingConflict(const std::string& path, SaveConflict& conflict)
	{
		LedgerEntry entry = getEntry(path);
		if (!entry.hasConflict)
			return false;

		conflict = entry.conflict;
		conflict.gamePath = path;
		return true;
	}

	void RommSaves::markChecked(const std::string& path)
	{
		std::unique_lock<std::mutex> lock(sCheckedMutex);
		sChecked[path] = time(nullptr);
	}

	bool RommSaves::consumeChecked(const std::string& path)
	{
		std::unique_lock<std::mutex> lock(sCheckedMutex);
		auto it = sChecked.find(path);
		if (it == sChecked.cend())
			return false;

		bool recent = time(nullptr) - it->second <= CHECKED_SECONDS;
		sChecked.erase(it);
		return recent;
	}

	bool RommSaves::isChecked(const std::string& path)
	{
		std::unique_lock<std::mutex> lock(sCheckedMutex);
		auto it = sChecked.find(path);
		return it != sChecked.cend() && time(nullptr) - it->second <= CHECKED_SECONDS;
	}

	void RommSaves::recoverAsync(Window* window)
	{
		if (window == nullptr || !isEnabled())
			return;

		std::thread([window]
		{
			std::vector<std::string> paths;
			{
				std::unique_lock<std::mutex> lock(sLedgerMutex);
				for (auto& item : readLedger(ledgerPath()))
					if (item.second.launched || item.second.dirty)
						paths.push_back(item.first);
			}

			if (paths.empty())
				return;

			LOG(LogInfo) << "RomM: " << paths.size() << " game(s) played and not synced yet, sending their saves";

			// Games looked up on the UI thread, pushed on a worker
			window->postToUiThread([paths]
			{
				std::vector<SaveJob> jobs;
				for (auto& path : paths)
				{
					for (auto system : SystemData::sSystemVector)
					{
						if (system->isCollection() || !system->isGameSystem())
							continue;

						FileData* game = system->getRootFolder()->FindByPath(path);
						SaveJob job;
						if (game != nullptr && prepare(game, job))
						{
							jobs.push_back(job);
							break;
						}
					}
				}

				std::thread([jobs]
				{
					for (auto& job : jobs)
					{
						std::string message;
						afterGame(job, message);
					}
				}).detach();
			});
		}).detach();
	}
}
