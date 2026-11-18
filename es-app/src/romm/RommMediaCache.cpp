#include "romm/RommMediaCache.h"
#include "romm/RommClient.h"
#include "romm/RommConfig.h"
#include "romm/RommSync.h"
#include "romm/RommWork.h"

#include "FileData.h"
#include "Log.h"
#include "SystemData.h"
#include "Window.h"
#include "views/ViewController.h"
#include "views/gamelist/IGameListView.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#include <set>
#include <unordered_map>

#include <cstdio>
#include <cstdlib>
#include <ctime>

#ifdef WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/statvfs.h>
#endif

namespace Romm
{
	static const unsigned long long MB = 1024ULL * 1024ULL;
	static const unsigned long long GB = 1024ULL * MB;

	// ---------------------------------------------------------------------------------------------
	// Size
	// ---------------------------------------------------------------------------------------------

	bool diskSpace(const std::string& path, unsigned long long& total, unsigned long long& available)
	{
#ifdef WIN32
		ULARGE_INTEGER freeToCaller, totalBytes, freeBytes;
		if (!GetDiskFreeSpaceExA(path.c_str(), &freeToCaller, &totalBytes, &freeBytes))
			return false;

		total = totalBytes.QuadPart;
		available = freeToCaller.QuadPart;
		return true;
#else
		struct statvfs st;
		if (statvfs(path.c_str(), &st) != 0)
			return false;

		total = (unsigned long long)st.f_blocks * st.f_frsize;
		available = (unsigned long long)st.f_bavail * st.f_frsize;
		return true;
#endif
	}

	unsigned long long mediaCacheUsage(const std::string& baseUrl)
	{
		unsigned long long usage = 0;
		for (auto& m : allMedia())
		{
			std::string dir = getMediaCacheDir(baseUrl, m.type);
			if (Utils::FileSystem::isDirectory(dir))
				for (auto file : Utils::FileSystem::getDirContent(dir, false, false))
					usage += Utils::FileSystem::getFileSize(file);
		}

		return usage;
	}

	unsigned long long mediaCacheLimit(const RommConfig& config, unsigned long long usageNow)
	{
		unsigned long long total = 0, available = 0;
		std::string where = getCacheRoot();
		Utils::FileSystem::createDirectory(where);
		bool known = diskSpace(where, total, available);

		unsigned long long limit;
		if (config.cacheSize == "unlimited")
			limit = ~0ULL;
		else if (config.cacheSize == "auto" || config.cacheSize.empty() || atoll(config.cacheSize.c_str()) <= 0)
			limit = known ? std::min(total / 10, 10 * GB) : 2 * GB;
		else
			limit = (unsigned long long)atoll(config.cacheSize.c_str()) * MB;

		if (!known)
			return limit;

		// Free space for ROM downloads comes first
		unsigned long long keepFree = std::min(5 * GB, total / 10);
		unsigned long long room = available > keepFree ? available - keepFree : 0;
		return std::min(limit, usageNow + room);
	}

	// ---------------------------------------------------------------------------------------------
	// Ranking
	// ---------------------------------------------------------------------------------------------

	int mediaTier(Media type, const RommConfig& config)
	{
		Media image;
		if ((config.imageMedia(image) && image == type) || type == Media::Cover)
			return 6;

		switch (type)
		{
		case Media::Marquee: case Media::Wheel: case Media::Box:
			return 5;
		case Media::BoxBack: case Media::BoxSide: case Media::Box3d: case Media::TitleShot: case Media::Cartridge: case Media::Screenshot:
			return 4;
		case Media::FanArt: case Media::Mix: case Media::MixV2: case Media::CoverLarge: case Media::Bezel:
			return 3;
		case Media::Manual: case Media::Walkthrough:
			return 2;
		case Media::Video:
			return 1;
		default:
			return 3;
		}
	}

	// Rough sizes of what is not cached yet (refined by the average of the cached files of the type)
	static unsigned long long defaultSize(Media type)
	{
		switch (type)
		{
		case Media::Video: return 10 * MB;
		case Media::Manual: return 8 * MB;
		case Media::Walkthrough: return 3 * MB;
		case Media::Bezel: case Media::FanArt: case Media::CoverLarge: return 600 * 1024;
		case Media::Mix: case Media::MixV2: case Media::Box3d: return 500 * 1024;
		case Media::Marquee: case Media::Wheel: case Media::BoxSide: return 100 * 1024;
		default: return 250 * 1024;
		}
	}

	static bool ranksAbove(const MediaItem& a, const MediaItem& b)
	{
		if (a.pinned != b.pinned) return a.pinned;
		if (a.tier != b.tier) return a.tier > b.tier;
		if (a.viewed != b.viewed) return a.viewed > b.viewed;
		if (a.record->romId != b.record->romId) return a.record->romId < b.record->romId;
		return (int)a.type < (int)b.type;
	}

	// ---------------------------------------------------------------------------------------------
	// Viewed (selected) games
	// ---------------------------------------------------------------------------------------------

	static std::mutex sViewedMutex;
	static std::unordered_map<int, long long> sViewed;
	static std::string sViewedFile;
	static bool sViewedChanged = false;

	void noteGameViewed(int romId)
	{
		if (romId <= 0)
			return;

		std::unique_lock<std::mutex> lock(sViewedMutex);
		sViewed[romId] = (long long)time(nullptr);
		sViewedChanged = true;
	}

	long long gameLastViewed(int romId)
	{
		std::unique_lock<std::mutex> lock(sViewedMutex);
		auto it = sViewed.find(romId);
		return it == sViewed.cend() ? 0 : it->second;
	}

	void loadViewed(const std::string& baseUrl)
	{
		std::unique_lock<std::mutex> lock(sViewedMutex);
		std::string file = getServerCacheDir(baseUrl) + "/viewed.txt";
		if (file == sViewedFile)
			return;

		sViewedFile = file;
		sViewed.clear();
		sViewedChanged = false;

		if (Utils::FileSystem::exists(file, false))
		{
			for (auto& line : Utils::String::split(Utils::FileSystem::readAllText(file), '\n', true))
			{
				int romId = 0;
				long long when = 0;
				if (sscanf(line.c_str(), "%d %lld", &romId, &when) == 2 && romId > 0)
					sViewed[romId] = when;
			}
		}
	}

	void saveViewed()
	{
		std::unique_lock<std::mutex> lock(sViewedMutex);
		if (!sViewedChanged || sViewedFile.empty())
			return;

		std::string text;
		for (auto& item : sViewed)
			text += std::to_string(item.first) + " " + std::to_string(item.second) + "\n";

		Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(sViewedFile));
		writeFileAtomic(sViewedFile, text);
		sViewedChanged = false;
	}

	// ---------------------------------------------------------------------------------------------
	// Plan
	// ---------------------------------------------------------------------------------------------

	MediaPlan planMediaCache(const RommConfig& config, const std::vector<const CatalogRecord*>& games, const std::set<int>& pinnedRoms)
	{
		MediaPlan plan;
		const std::string baseUrl = config.baseUrl();

		// What is in the cache now, per media type: file name -> size
		std::map<Media, std::map<std::string, unsigned long long>> cached;
		std::map<Media, unsigned long long> typeBytes, typeCount;

		for (auto& m : allMedia())
		{
			std::string dir = getMediaCacheDir(baseUrl, m.type);
			if (!Utils::FileSystem::isDirectory(dir))
				continue;

			for (auto file : Utils::FileSystem::getDirContent(dir, false, false))
			{
				unsigned long long size = Utils::FileSystem::getFileSize(file);
				cached[m.type][Utils::FileSystem::getFileName(file)] = size;
				plan.usage += size;
				typeBytes[m.type] += size;
				typeCount[m.type]++;
			}
		}

		auto estimate = [&](Media type)
		{
			if (typeCount[type] >= 5)
				return typeBytes[type] / typeCount[type];

			return defaultSize(type);
		};

		// Every media the cache could hold: the types fetched from the library, of these games
		std::vector<MediaItem> items;
		std::map<Media, std::set<std::string>> wanted;

		for (auto record : games)
		{
			bool pinned = pinnedRoms.count(record->romId) > 0;
			long long viewed = gameLastViewed(record->romId);

			for (auto& m : allMedia())
			{
				const std::string& file = record->mediaFile[mediaIndex(m.type)];
				if (file.empty() || !config.fetchesFromLibrary(m.type))
					continue;

				MediaItem item;
				item.record = record;
				item.type = m.type;
				item.pinned = pinned;
				item.viewed = viewed;
				item.tier = mediaTier(m.type, config);

				auto c = cached[m.type].find(file);
				item.cached = c != cached[m.type].cend();
				item.size = item.cached ? c->second : estimate(m.type);

				wanted[m.type].insert(file);
				items.push_back(item);
			}
		}

		// Files no game wants any more (type not selected, game gone, downloaded, hidden): evicted
		for (auto& type : cached)
		{
			for (auto& file : type.second)
			{
				if (wanted[type.first].count(file.first) == 0)
				{
					plan.evict.push_back(getMediaCacheDir(baseUrl, type.first) + "/" + file.first);
					plan.usage -= std::min(plan.usage, file.second);
				}
			}
		}

		std::sort(items.begin(), items.end(), ranksAbove);

		plan.limit = mediaCacheLimit(config, plan.usage);
		plan.admit = plan.limit == ~0ULL ? ~0ULL : plan.limit / 10 * 9;

		// Eviction queue: cached items, lowest rank first
		std::deque<const MediaItem*> lowest;
		for (auto it = items.rbegin(); it != items.rend(); ++it)
			if (it->cached)
				lowest.push_back(&*it);

		unsigned long long projected = plan.usage;
		auto evictLowest = [&]()
		{
			const MediaItem* victim = lowest.front();
			lowest.pop_front();
			plan.evict.push_back(getMediaCacheDir(baseUrl, victim->type) + "/" + victim->record->mediaFile[mediaIndex(victim->type)]);
			projected -= std::min(projected, victim->size);
		};

		// Over the limit (setting lowered, disk filling up): down to 90 %
		if (projected > plan.limit)
			while (projected > plan.admit && !lowest.empty())
				evictLowest();

		// Downloads, best first; a missing media may push out cached ones that rank below it, but only
		// when that makes enough room (never evict for something that will not fit anyway)
		for (auto& item : items)
		{
			if (item.cached)
				continue;

			if (projected + item.size > plan.admit)
			{
				unsigned long long freeable = 0;
				for (auto victim : lowest)
				{
					if (!ranksAbove(item, *victim))
						break;

					freeable += victim->size;
					if (projected - std::min(projected, freeable) + item.size <= plan.admit)
						break;
				}

				if (projected - std::min(projected, freeable) + item.size > plan.admit)
					continue; // does not fit, even by pushing out what ranks below it

				while (projected + item.size > plan.admit)
					evictLowest();
			}

			plan.download.push_back(item);
			projected += item.size;
		}

		return plan;
	}

	// ---------------------------------------------------------------------------------------------
	// Fetch on demand
	// ---------------------------------------------------------------------------------------------

	struct OnDemandRequest
	{
		std::string path;
		RemoteGame remote;
		std::vector<Media> types; // missing, best first
	};

	static std::mutex sDemandMutex;
	static std::condition_variable sDemandCv;
	static std::deque<OnDemandRequest> sDemandQueue;
	static std::thread sDemandThread;
	static std::atomic<bool> sDemandStop(false);
	static std::string sDemandBaseUrl;
	static std::vector<Media> sDemandTypes; // fetched from the library, best first
	static int sDemandImage = -1;           // IMAGE SHOWN media index, -1 automatic

	void configureOnDemand(const RommConfig& config)
	{
		std::unique_lock<std::mutex> lock(sDemandMutex);
		sDemandBaseUrl = config.isUsable() ? config.baseUrl() : "";
		sDemandTypes.clear();

		Media image;
		sDemandImage = config.imageMedia(image) ? (int)mediaIndex(image) : -1;

		for (auto& m : allMedia())
			if (config.fetchesFromLibrary(m.type))
				sDemandTypes.push_back(m.type);

		std::stable_sort(sDemandTypes.begin(), sDemandTypes.end(), [&config](Media a, Media b) { return mediaTier(a, config) > mediaTier(b, config); });

		if (!sDemandBaseUrl.empty())
		{
			lock.unlock();
			loadViewed(config.baseUrl());
		}
	}

	// UI thread: the media are in the cache, the game shows them (if it is still loaded)
	static void applyFetched(const std::string& path, Media type, const std::string& file, int imageIndex)
	{
		for (auto system : SystemData::sSystemVector)
		{
			if (!system->isGameSystem() || system->isCollection() || system->getRootFolder() == nullptr)
				continue;

			FileData* game = system->getRootFolder()->FindByPath(path);
			if (game == nullptr || !game->isRemote())
				continue;

			const MediaInfo& m = mediaInfo(type);
			MetaDataList& md = game->getMetadata();

			// The file did not exist when ES last looked: forget that, or the tile would stay empty
			Utils::FileSystem::FileSystemCache::reset(file);

			if (imageIndex == (int)mediaIndex(type))
				md.set(MetaDataId::Image, file);                       // the image shown
			else if (!m.imageOnly && !(type == Media::Cover && imageIndex >= 0 && !md.get(MetaDataId::Image).empty()))
				md.set(m.metadata, file);

			md.resetChangedFlag();

			// Redrawn in place: the list on screen and the game's own system list (kept in memory). The
			// selected entry gets its details redrawn, another entry its tile.
			bool shown = false;
			ViewController* vc = ViewController::get();
			if (vc != nullptr)
			{
				std::set<IGameListView*> done;
				auto refresh = [&](SystemData* listSystem, bool onScreen)
				{
					auto view = vc->getGameListView(listSystem, false);
					if (view == nullptr || !done.insert(view.get()).second)
						return;

					FileData* cursor = view->getCursor();
					if (onScreen && cursor != nullptr && cursor->getSourceFileData() == game)
					{
						view->refreshDetails(file); // the grid also redraws the selected tile
						shown = true;
						return;
					}

					for (auto entry : view->getFileDataEntries())
					{
						if (entry != nullptr && entry->getSourceFileData() == game)
						{
							view->refreshEntry(entry, file);
							shown = shown || onScreen;
							return;
						}
					}
				};

				if (vc->getState().viewing == ViewController::GAME_LIST)
					refresh(vc->getState().getSystem(), true);

				refresh(system, false);
			}

			LOG(LogDebug) << "RomM: " << m.key << " fetched on demand for " << path << (shown ? ", shown" : "");
			return;
		}
	}

	static void onDemandWorker()
	{
		lowerThreadPriority();

		while (true)
		{
			OnDemandRequest request;
			std::string baseUrl;
			int imageIndex;
			{
				std::unique_lock<std::mutex> lock(sDemandMutex);
				sDemandCv.wait(lock, [] { return sDemandStop || !sDemandQueue.empty(); });
				if (sDemandStop)
					return;

				request = sDemandQueue.front(); // the most recent selection
				sDemandQueue.pop_front();
				baseUrl = sDemandBaseUrl;
				imageIndex = sDemandImage;
			}

			RommConfig config = RommConfig::load();
			if (!config.isUsable() || config.baseUrl() != baseUrl)
				continue;

			RommClient client(config);
			for (auto type : request.types)
			{
				if (sDemandStop)
					return;

				size_t i = mediaIndex(type);
				std::string dir = getMediaCacheDir(baseUrl, type);
				std::string file = dir + "/" + request.remote.mediaFile[i];
				if (Utils::FileSystem::exists(file, false))
					continue;

				Utils::FileSystem::createDirectory(dir);

				const std::string& url = request.remote.mediaUrl[i];
				std::string path = Utils::String::startsWith(url, "/api/") ? url : coverPath(url);

				HttpResult res = client.download(path, file, nullptr, &sDemandStop);

				// Failed, but the file is there: the synchronization downloaded it at the same time
				if (!res.ok() && !Utils::FileSystem::exists(file, false))
					continue;

				Window* window = RommSync::getWindow();
				if (window != nullptr)
				{
					std::string gamePath = request.path;
					window->postToUiThread([gamePath, type, file, imageIndex]() { applyFetched(gamePath, type, file, imageIndex); });
				}
			}
		}
	}

	void onGameShown(FileData* game)
	{
		if (game == nullptr || game->getType() != GAME || !RommCatalog::isActive())
			return;

		FileData* source = game->getSourceFileData();
		const std::string path = source->getPath();

		int romId = RommCatalog::getRomId(path);
		if (romId <= 0)
			return;

		noteGameViewed(romId);

		if (!source->isRemote())
			return;

		RemoteGame remote;
		if (!RommCatalog::getRemoteGame(path, remote))
			return;

		OnDemandRequest request;
		request.path = path;
		request.remote = remote;

		{
			std::unique_lock<std::mutex> lock(sDemandMutex);
			if (sDemandBaseUrl.empty())
				return;

			for (auto type : sDemandTypes)
			{
				const std::string& file = remote.mediaFile[mediaIndex(type)];
				if (!file.empty() && !Utils::FileSystem::exists(getMediaCacheDir(sDemandBaseUrl, type) + "/" + file, false))
					request.types.push_back(type);
			}

			if (request.types.empty())
				return;

			// Latest selection first, each game once, up to 50 pending
			sDemandQueue.erase(std::remove_if(sDemandQueue.begin(), sDemandQueue.end(), [&path](const OnDemandRequest& r) { return r.path == path; }), sDemandQueue.end());
			sDemandQueue.push_front(request);
			while (sDemandQueue.size() > 50)
				sDemandQueue.pop_back();

			if (!sDemandThread.joinable())
			{
				sDemandStop = false;
				sDemandThread = std::thread(onDemandWorker);
			}
		}

		sDemandCv.notify_one();
	}

	void stopOnDemand()
	{
		{
			std::unique_lock<std::mutex> lock(sDemandMutex);
			sDemandStop = true;
			sDemandQueue.clear();
		}

		sDemandCv.notify_all();
		if (sDemandThread.joinable())
			sDemandThread.join();

		sDemandStop = false;
		saveViewed();
	}
}
