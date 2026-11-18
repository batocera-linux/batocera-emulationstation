#pragma once
#ifndef ES_APP_ROMM_ROMM_MEDIA_H
#define ES_APP_ROMM_ROMM_MEDIA_H

#include "MetaData.h"
#include <string>
#include <vector>

namespace Romm
{
	// Media a RomM game can bring to ES. One table drives the sync, the cache, the
	// injection, the download along with the rom, the clean up and the menus.
	enum class Media { Cover = 0, Manual, Walkthrough, Marquee, Bezel, Video,
		Box, BoxBack, Box3d, FanArt, TitleShot, Wheel, Mix, Cartridge,
		MixV2, Screenshot, BoxSide, CoverLarge, Count };

	struct MediaInfo
	{
		Media type;
		const char* key;        // settings: romm.sync.<key>, romm.withrom.<key>; snapshot attributes <key>, <key>File
		const char* label;      // menu label (translated with _())
		MetaDataId metadata;    // ES field it fills
		const char* cacheDir;   // folder in the server cache
		const char* legacyConf; // legacy key, used when romm.sync.<key> is absent ("" if none)
		const char* saveSuffix; // next to a downloaded rom: <name>-<saveSuffix>.<ext> instead of the ES scraper's name ("" : ES name)
		bool imageOnly;         // no ES field of its own: offered only as the IMAGE SHOWN (not in the media lists)
		bool canBeImage;        // offered in IMAGE SHOWN
	};

	// IMAGE SHOWN (romm.image): the media type used as the ES image, or automatic (cover, then the
	// large cover, a screenshot, the title screen, the mix image or the box front)
	static const char* const IMAGE_AUTOMATIC = "auto";

	const std::vector<MediaInfo>& allMedia();
	const MediaInfo& mediaInfo(Media type);
	inline size_t mediaIndex(Media type) { return (size_t)type; }

	std::string getMediaCacheDir(const std::string& baseUrl, Media type);
}

#endif
