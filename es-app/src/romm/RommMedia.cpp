#include "romm/RommMedia.h"
#include "romm/RommConfig.h"

namespace Romm
{
	const std::vector<MediaInfo>& allMedia()
	{
		// Cache folder names are kept stable: existing caches stay valid
		static const std::vector<MediaInfo> media =
		{
			//  type                 key            label                 ES field               cache folder    legacy setting       save suffix   image only  can be image
			{ Media::Cover,       "cover",       "COVER",              MetaDataId::Image,     "covers",       "romm.covers",       "",           false,      true },
			{ Media::Manual,      "manual",      "MANUAL",             MetaDataId::Manual,    "manuals",      "romm.manuals",      "",           false,      false },
			{ Media::Walkthrough, "walkthrough", "WALKTHROUGH",        MetaDataId::Map,       "walkthroughs", "romm.walkthroughs", "",           false,      false },
			{ Media::Marquee,     "marquee",     "MARQUEE",            MetaDataId::Marquee,   "marquees",     "",                  "",           false,      true },
			{ Media::Bezel,       "bezel",       "BEZEL",              MetaDataId::Bezel,     "bezels",       "",                  "",           false,      false },
			{ Media::Video,       "video",       "VIDEO",              MetaDataId::Video,     "videos",       "",                  "",           false,      false },
			// ScreenScraper media stored by RomM (and media of gamelists imported into RomM),
			// in the ES fields the Batocera ScreenScraper scraper uses for them
			{ Media::Box,         "box",         "BOX (FRONT)",        MetaDataId::Thumbnail, "boxes",        "",                  "",           false,      true },
			{ Media::BoxBack,     "boxback",     "BOX (BACK)",         MetaDataId::BoxBack,   "boxbacks",     "",                  "",           false,      true },
			{ Media::Box3d,       "box3d",       "BOX (3D)",           MetaDataId::BoxArt,    "boxes3d",      "",                  "",           false,      true },
			{ Media::FanArt,      "fanart",      "FAN ART",            MetaDataId::FanArt,    "fanarts",      "",                  "",           false,      true },
			{ Media::TitleShot,   "titleshot",   "TITLE SCREEN",       MetaDataId::TitleShot, "titleshots",   "",                  "",           false,      true },
			{ Media::Wheel,       "wheel",       "WHEEL (LOGO)",       MetaDataId::Wheel,     "wheels",       "",                  "",           false,      true },
			// The ES scraper has no name for mix images (its default, <name>-image, is the cover's)
			{ Media::Mix,         "mix",         "MIX IMAGE",          MetaDataId::Mix,       "mixes",        "",                  "mix",        false,      true },
			{ Media::Cartridge,   "cartridge",   "CARTRIDGE / DISC",   MetaDataId::Cartridge, "cartridges",   "",                  "",           false,      true },
			{ Media::MixV2,       "mixv2",       "MIX IMAGE (V2)",     MetaDataId::Mix,       "mixes2",       "",                  "mixv2",      false,      true },
			// No ES field of their own: only as the image shown
			{ Media::Screenshot,  "screenshot",  "SCREENSHOT",         MetaDataId::Image,     "screenshots",  "",                  "screenshot", true,       true },
			{ Media::BoxSide,     "boxside",     "BOX (SIDE)",         MetaDataId::Image,     "boxsides",     "",                  "boxside",    true,       true },
			{ Media::CoverLarge,  "coverlarge",  "COVER (LARGE)",      MetaDataId::Image,     "coverslarge",  "",                  "",           true,       true },
		};

		return media;
	}

	const MediaInfo& mediaInfo(Media type)
	{
		return allMedia()[mediaIndex(type)];
	}

	std::string getMediaCacheDir(const std::string& baseUrl, Media type)
	{
		return getServerCacheDir(baseUrl) + "/" + mediaInfo(type).cacheDir;
	}
}
