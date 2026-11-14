#pragma once
#ifndef ES_APP_ROMM_ROMM_CACHE_H
#define ES_APP_ROMM_ROMM_CACHE_H

#include "romm/RommMedia.h"
#include <vector>

namespace Romm
{
	struct CacheCleanupResult
	{
		int files = 0;
		unsigned long long bytes = 0;
	};

	// CLEAN UP LOCAL CACHE (worker thread). Removes, inside the RomM cache only:
	// - media of types not selected in SYNC FROM LIBRARY
	// - media of games no longer in the library, of hidden systems, or already on the device
	//   (games downloaded through RomM have their media next to the rom)
	// - the caches of other RomM servers, and leftover temporary files
	CacheCleanupResult cleanupCache();

	// DOWNLOAD MEDIA AGAIN: empties the cache of these media types (the next synchronization
	// downloads them again). The game lists and other media are kept.
	CacheCleanupResult clearMediaCache(const std::vector<Media>& types);
}

#endif
