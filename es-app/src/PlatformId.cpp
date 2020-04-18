#include "PlatformId.h"

#include <map>
#include <string.h>
#include "utils/StringUtil.h"

namespace PlatformIds
{
	static std::map<std::string, PlatformId> Platforms =
	{
		{ "unknown",				PLATFORM_UNKNOWN },
		{ "3do",					THREEDO },
		{ "amiga",					AMIGA },
		{ "amstradcpc",				AMSTRAD_CPC },
		{ "apple2",					APPLE_II },
		{ "arcade",					ARCADE },
		{ "atari800",				ATARI_800 },
		{ "atari2600",				ATARI_2600 },
		{ "atari5200",				ATARI_5200 },
		{ "atari7800",				ATARI_7800 },
		{ "atarilynx",				ATARI_LYNX },
		{ "atarist",				ATARI_ST },
		{ "jaguar",					ATARI_JAGUAR },
		{ "atarijaguar",			ATARI_JAGUAR },
		{ "atarijaguarcd",			ATARI_JAGUAR_CD },
		{ "atarixe",				ATARI_XE },
		{ "colecovision",			COLECOVISION },
		{ "c64",					COMMODORE_64 },
		{ "intellivision",			INTELLIVISION },
		{ "macintosh",				MAC_OS },
		{ "xbox",					XBOX },
		{ "xbox360",				XBOX_360 },
		{ "msx",					MSX },
		{ "neogeo",					NEOGEO },
		{ "ngp",					NEOGEO_POCKET },
		{ "ngpc",					NEOGEO_POCKET_COLOR },
		{ "n3ds",					NINTENDO_3DS },
		{ "n64",					NINTENDO_64 },
		{ "nds",					NINTENDO_DS },
		{ "fds",					FAMICOM_DISK_SYSTEM },
		{ "nes",					NINTENDO_ENTERTAINMENT_SYSTEM },
		{ "gb",						GAME_BOY },
		{ "gba",					GAME_BOY_ADVANCE },
		{ "gbc",					GAME_BOY_COLOR },
		{ "gc",						NINTENDO_GAMECUBE },
		{ "wii",					NINTENDO_WII },
		{ "wiiu",					NINTENDO_WII_U },
		{ "virtualboy",				NINTENDO_VIRTUAL_BOY },
		{ "gameandwatch",			NINTENDO_GAME_AND_WATCH },
		{ "pc",						PC },
		{ "pc98",					PC_98 },
		{ "sega32x",				SEGA_32X },
		{ "segacd",					SEGA_CD },
		{ "dreamcast",				SEGA_DREAMCAST },
		{ "gamegear",				SEGA_GAME_GEAR },
		{ "genesis",				SEGA_GENESIS },
		{ "mastersystem",			SEGA_MASTER_SYSTEM },
		{ "megadrive",				SEGA_MEGA_DRIVE },
		{ "saturn",					SEGA_SATURN },
		{ "sg-1000",				SEGA_SG1000 },
		{ "psx",					PLAYSTATION },
		{ "ps2",					PLAYSTATION_2 },
		{ "ps3",					PLAYSTATION_3 },
		{ "ps4",					PLAYSTATION_4 },
		{ "psvita",					PLAYSTATION_VITA },
		{ "psp",					PLAYSTATION_PORTABLE },
		{ "snes",					SUPER_NINTENDO },
		{ "scummvm",				SCUMMVM },
		{ "x68000",					SHARP_X6800 },
		{ "pcengine",				TURBOGRAFX_16 }, // (aka PC Engine) HuCards onlyy
		{ "pcenginecd",				TURBOGRAFX_CD }, // (aka PC Engine) CD-ROMs onlynly
		{ "wonderswan",				WONDERSWAN },
		{ "wonderswancolor",		WONDERSWAN_COLOR },
		{ "zxspectrum",				ZX_SPECTRUM },
		{ "videopac",				VIDEOPAC_ODYSSEY2 },
		{ "vectrex",				VECTREX },
		{ "trs-80",					TRS80_COLOR_COMPUTER },
		{ "coco",					TANDY },		
		{ "supergrafx",				SUPERGRAFX },
		{ "amigacd32",				AMIGACD32 },
		{ "amigacdtv",				AMIGACDTV },
		{ "atomiswave",				ATOMISWAVE },
		{ "cavestory",				CAVESTORY },
		{ "gx4000",					GX4000 },
		{ "lutro",					LUTRO },
		{ "moonlight",				MOONLIGHT },
		{ "naomi",					NAOMI },
		{ "neogeocd",				NEOGEO_CD },
		{ "pcfx",					PCFX },
		{ "pokemini",				POKEMINI },
		{ "prboom",					PRBOOM },
		{ "satellaview",			SATELLAVIEW },
		{ "sufami",					SUFAMITURBO },
		{ "zx81",					ZX81 },

		// batocera specific names
		{ "gb2players",				GAME_BOY },
		{ "gbc2players",			GAME_BOY_COLOR },
		{ "3ds",					NINTENDO_3DS },
		{ "sg1000",					SEGA_SG1000 },
		{ "odyssey2",				VIDEOPAC_ODYSSEY2 },
		{ "oricatmos",				ORICATMOS },
		{ "tyrquake",				QUAKE },
		{ "mrboom",					MRBOOM },

		// windows specific systems & names
		{ "windows",				MOONLIGHT },
		{ "vpinball",				VISUALPINBALL },
		{ "fpinball",				FUTUREPINBALL },
		{ "o2em",					VIDEOPAC_ODYSSEY2 },

		// Misc systems
		{ "channelf",				CHANNELF },
		{ "oric",					ORICATMOS },
		{ "thomson",				THOMSON_TO_MO },
		{ "samcoupe",				SAMCOUPE },
		{ "openbor",				OPENBOR },
		{ "uzebox",					UZEBOX },
		{ "apple2gs",				APPLE2GS },			
		{ "spectravideo",			SPECTRAVIDEO },
		{ "palm",					PALMOS },
		{ "daphne",					DAPHNEE },
		{ "solarus",				SOLARUS },
						
		{ "ignore",					PLATFORM_IGNORE },
		{ "invalid",				PLATFORM_COUNT }
	};

	PlatformId getPlatformId(const char* str)
	{
		if (str == nullptr)
			return PLATFORM_UNKNOWN;

		auto it = Platforms.find(Utils::String::toLower(str).c_str());
		if (it != Platforms.end())
			return (*it).second;

		return PLATFORM_UNKNOWN;
	}

	std::string getPlatformName(PlatformId id)
	{
		for (auto& it : Platforms)
			if (it.second == id)
				return it.first;

		return "unknown";
	}
}
