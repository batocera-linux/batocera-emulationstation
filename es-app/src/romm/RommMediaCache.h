#pragma once
#ifndef ES_APP_ROMM_ROMM_MEDIA_CACHE_H
#define ES_APP_ROMM_ROMM_MEDIA_CACHE_H

#include "romm/RommCatalog.h"
#include "romm/RommMedia.h"

#include <set>
#include <string>
#include <vector>

class FileData;

namespace Romm
{
	struct RommConfig;

	// ---------------------------------------------------------------------------------------------
	// Size of the media cache (romm.cachesize: "auto", megabytes, "unlimited")
	// ---------------------------------------------------------------------------------------------

	// Total and free bytes of the file system holding path
	bool diskSpace(const std::string& path, unsigned long long& total, unsigned long long& available);

	// Bytes of media in the cache of this server
	unsigned long long mediaCacheUsage(const std::string& baseUrl);

	// What the media cache may use now: the setting (AUTOMATIC: 10 % of the partition, at most 10 GB),
	// and whatever the setting, never eating into the free space ROM downloads need (the smaller of
	// 5 GB and 10 % of the partition stays free)
	unsigned long long mediaCacheLimit(const RommConfig& config, unsigned long long usageNow);

	// ---------------------------------------------------------------------------------------------
	// Which media stay: one ranking for eviction and for downloads (no churn between syncs)
	//   1. pinned games (RomM favorites, selected collections)
	//   2. media type: the image shown and covers, then marquees / wheels / box fronts, then other
	//      artwork, then large artwork, then manuals and walkthroughs, then videos
	//   3. the most recently viewed game (selected in a list)
	// ---------------------------------------------------------------------------------------------

	int mediaTier(Media type, const RommConfig& config); // higher: kept first

	void noteGameViewed(int romId);                        // UI thread, cheap
	long long gameLastViewed(int romId);                    // 0: never
	void loadViewed(const std::string& baseUrl);
	void saveViewed();

	struct MediaItem
	{
		const CatalogRecord* record = nullptr;
		Media type = Media::Cover;
		bool cached = false;
		unsigned long long size = 0; // actual when cached, estimated otherwise
		bool pinned = false;
		long long viewed = 0;
		int tier = 0;
	};

	struct MediaPlan
	{
		std::vector<MediaItem> download;   // best first
		std::vector<std::string> evict;    // files to remove
		unsigned long long limit = 0;      // may use
		unsigned long long admit = 0;      // downloads stop here (90 % of the limit)
		unsigned long long usage = 0;      // before the plan
	};

	// games: RomM games not on the device, of visible systems. Pinned: their rom ids.
	MediaPlan planMediaCache(const RommConfig& config, const std::vector<const CatalogRecord*>& games, const std::set<int>& pinnedRoms);

	// ---------------------------------------------------------------------------------------------
	// Fetch on demand: a RomM game selected whose media are not cached gets them in the background
	// ---------------------------------------------------------------------------------------------

	void configureOnDemand(const RommConfig& config);   // RommCatalog::prepare
	void onGameShown(FileData* game);                    // the game a list settled on (UI thread)
	void stopOnDemand();                                 // ES shutdown (also saves the viewed times)
}

#endif
