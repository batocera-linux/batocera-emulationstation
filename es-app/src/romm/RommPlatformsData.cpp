// GENERATED from Grout's cfw/batocera/data/platforms.json
// (https://github.com/rommapp/grout, commit 175adab8bd9b271baba9020ea546da5877f234d5)
// Do not edit by hand: regenerate from the upstream file.
//
// MIT License
//
// Copyright (c) 2025 Brandon T. Kowalski, Grout Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "romm/RommPlatforms.h"
#include <map>
#include <string>
#include <vector>

namespace Romm
{
	// RomM platform slug -> Batocera system folders, in order of preference
	const std::map<std::string, std::vector<std::string>>& getGroutPlatformMap()
	{
		static const std::map<std::string, std::vector<std::string>> map =
		{
			{ "3do", { "3do" } },
			{ "3ds", { "3ds" } },
			{ "acpc", { "amstradcpc" } },
			{ "amiga", { "amiga500", "amiga1200" } },
			{ "arcade", { "arcade", "mame", "fbneo", "neogeo" } },
			{ "arduboy", { "arduboy" } },
			{ "atari-st", { "atarist" } },
			{ "atari2600", { "atari2600" } },
			{ "atari5200", { "atari5200" } },
			{ "atari7800", { "atari7800" } },
			{ "c128", { "c128" } },
			{ "c64", { "c64" } },
			{ "cave-story", { "cavestory" } },
			{ "colecovision", { "colecovision" } },
			{ "cpet", { "pet" } },
			{ "dc", { "dreamcast" } },
			{ "dos", { "dos" } },
			{ "fairchild-channel-f", { "channelf" } },
			{ "famicom", { "nes" } },
			{ "fds", { "fds" } },
			{ "g-and-w", { "gameandwatch" } },
			{ "galaksija", {  } },
			{ "gamegear", { "gamegear" } },
			{ "gb", { "gb" } },
			{ "gba", { "gba" } },
			{ "gbc", { "gbc" } },
			{ "genesis", { "megadrive" } },
			{ "intellivision", { "intellivision" } },
			{ "j2me", {  } },
			{ "jaguar", { "jaguar" } },
			{ "lynx", { "lynx" } },
			{ "mega-duck-slash-cougar-boy", { "megaduck" } },
			{ "msx", { "msx1", "msx2", "msx2plus", "msxturbor" } },
			{ "n64", { "n64" } },
			{ "nds", { "nds" } },
			{ "neo-geo-cd", { "neogeocd" } },
			{ "neo-geo-pocket", { "ngp" } },
			{ "neo-geo-pocket-color", { "ngpc" } },
			{ "neogeoaes", { "neogeo" } },
			{ "neogeomvs", { "neogeo" } },
			{ "nes", { "nes" } },
			{ "odyssey", { "odyssey2" } },
			{ "openbor", { "openbor" } },
			{ "pc-8000", { "pc88" } },
			{ "pc-9800-series", { "pc98" } },
			{ "pc-fx", { "pcfx" } },
			{ "philips-cd-i", { "cdi" } },
			{ "pico", { "pico8" } },
			{ "pico-8", { "pico8" } },
			{ "pokemon-mini", { "pokemini" } },
			{ "ps2", { "ps2" } },
			{ "psp", { "psp" } },
			{ "psx", { "psx", "ps1" } },
			{ "saturn", { "saturn" } },
			{ "sega32", { "sega32x" } },
			{ "segacd", { "segacd" } },
			{ "sfam", { "snes" } },
			{ "sg1000", { "sg1000" } },
			{ "sharp-x68000", { "x68000" } },
			{ "sms", { "mastersystem" } },
			{ "snes", { "snes" } },
			{ "supergrafx", { "supergrafx" } },
			{ "supervision", { "supervision" } },
			{ "tg16", { "pcengine" } },
			{ "tic-80", { "tic80" } },
			{ "turbografx-cd", { "pcenginecd" } },
			{ "vectrex", { "vectrex" } },
			{ "vic-20", { "c20" } },
			{ "virtualboy", { "virtualboy" } },
			{ "wonderswan", { "wswan" } },
			{ "wonderswan-color", { "wswanc" } },
			{ "x1", { "x1" } },
			{ "zx81", { "zx81" } },
			{ "zxs", { "zxspectrum" } },
		};

		return map;
	}
}
