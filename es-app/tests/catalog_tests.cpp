// Unit tests of the emulator catalog model (catalog/EmulatorCatalog) and of Utils::Sha256.
// Built with -DES_TESTS=ON, run with ctest.

#include "catalog/CatalogSystemConfig.h"
#include "catalog/EmulatorCatalog.h"
#include "utils/Sha256.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace catalog;

namespace
{
	int failures = 0;

	void check(bool condition, const char* expression, const char* file, int line)
	{
		if (!condition)
		{
			std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", file, line, expression);
			++failures;
		}
	}

	#define CHECK(x) check((x), #x, __FILE__, __LINE__)

	const std::string SHA_A(64, 'a');
	const std::string SHA_B(64, 'b');

	const std::string CATALOG = R"({
	  "format": 1,
	  "generated": "2026-11-06T04:17:00Z",
	  "standalone": [
	    { "id": "pcsx2", "name": "PCSX2", "systems": ["ps2"], "binaries": ["pcsx2-qt", "bad name"],
	      "builds": {
	        "x86_64": [
	          { "type": "appimage", "url": "https://example.org/pcsx2.AppImage", "version": "v2.8.2", "sha256": ")" + SHA_A + R"(", "size": 100 },
	          { "type": "flatpak", "ref": "net.pcsx2.PCSX2" } ] } },
	    { "id": "dolphin", "name": "Dolphin", "systems": ["gamecube", "wii"], "logo": "https://example.org/dolphin.png",
	      "builds": {
	        "x86_64": [ { "type": "flatpak", "ref": "org.DolphinEmu.dolphin-emu" } ],
	        "aarch64": [ { "type": "flatpak", "ref": "org.DolphinEmu.dolphin-emu" } ] } },
	    { "id": "melonds", "name": "melonDS", "systems": ["nds"], "logo": "logos/melonds.png",
	      "builds": { "aarch64": [
	          { "type": "archive", "url": "https://example.org/m.zip", "version": "1.1", "sha256": ")" + SHA_B + R"(", "exec": "\\.AppImage$" } ] } },
	    { "id": "unchecked", "name": "No hash", "builds": { "x86_64": [ { "type": "appimage", "url": "https://example.org/x", "version": "1" } ] } },
	    { "id": "plain-http", "name": "HTTP", "builds": { "x86_64": [ { "type": "appimage", "url": "http://example.org/x", "version": "1", "sha256": ")" + SHA_A + R"(" } ] } },
	    { "id": "future", "name": "Future", "builds": { "x86_64": [ { "type": "snap", "name": "x" } ] } },
	    { "id": "../evil", "name": "Evil", "builds": { "x86_64": [ { "type": "flatpak", "ref": "a.b" } ] } },
	    { "id": "badref", "name": "Bad ref", "builds": { "x86_64": [ { "type": "flatpak", "ref": "a.b; rm -rf ~" } ] } }
	  ],
	  "cores": {
	    "info": "https://buildbot.libretro.com/assets/frontend/info.zip",
	    "items": [
	      { "id": "snes9x", "name": "Snes9x", "systems": ["snes"],
	        "builds": { "x86_64": { "url": "https://buildbot.example/x86_64/snes9x_libretro.so.zip", "version": "2026-11-05" },
	                    "aarch64": { "url": "https://buildbot.example/aarch64/snes9x_libretro.so.zip", "version": "2026-11-04" } } },
	      { "id": "mame", "name": "MAME", "systems": ["mame"],
	        "builds": { "x86_64": { "url": "https://buildbot.example/x86_64/mame_libretro.so.zip", "version": "2026-11-05" } } }
	    ]
	  }
	})";

	const std::string URL = "https://raw.githubusercontent.com/JyuHo/es-re-catalog/main/catalog.json";

	std::string tempDir()
	{
		auto dir = std::filesystem::temp_directory_path() / "es-catalog-tests";
		std::filesystem::remove_all(dir);
		std::filesystem::create_directories(dir);
		return dir.string();
	}

	void writeFile(const std::string& path, const std::string& text)
	{
		std::ofstream out(path, std::ios::binary);
		out << text;
	}

	void testSha256()
	{
		CHECK(Utils::Sha256::ofString("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
		CHECK(Utils::Sha256::ofString("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
		CHECK(Utils::Sha256::ofString("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

		// Fed in pieces that straddle the 64-byte blocks
		std::string million(1000000, 'a');
		Utils::Sha256 sha;
		for (size_t i = 0; i < million.size(); i += 997)
			sha.update(million.data() + i, std::min<size_t>(997, million.size() - i));
		CHECK(sha.hexDigest() == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
	}

	void testParseKeepsWhatRunsHere()
	{
		std::string error;
		auto x86 = parseCatalog(CATALOG, "x86_64", URL, &error);
		CHECK(x86.has_value());
		if (!x86)
			return;

		// Sorted by name; melonDS has no x86_64 build, invalid entries are dropped
		CHECK(x86->emulators.size() == 2);
		CHECK(x86->emulators[0].id == "dolphin");
		CHECK(x86->emulators[1].id == "pcsx2");
		CHECK(x86->findEmulator("unchecked") == nullptr);
		CHECK(x86->findEmulator("plain-http") == nullptr);
		CHECK(x86->findEmulator("future") == nullptr);
		CHECK(x86->findEmulator("../evil") == nullptr);
		CHECK(x86->findEmulator("badref") == nullptr);

		auto pcsx2 = x86->findEmulator("pcsx2");
		CHECK(pcsx2->builds.size() == 2);
		CHECK(pcsx2->builds[0].type == BuildType::AppImage);
		CHECK(pcsx2->builds[0].size == 100);
		CHECK(pcsx2->builds[1].type == BuildType::Flatpak);
		CHECK(pcsx2->builds[1].remote == "flathub");
		CHECK(pcsx2->binaries.size() == 1);

		CHECK(x86->cores.size() == 2);
		CHECK(x86->findCore("snes9x")->version == "2026-11-05");
		CHECK(x86->coreInfoUrl == "https://buildbot.libretro.com/assets/frontend/info.zip");

		auto arm = parseCatalog(CATALOG, "aarch64", URL);
		CHECK(arm->emulators.size() == 2);
		CHECK(arm->emulators[0].id == "dolphin"); // before melonDS, whatever the case
		CHECK(arm->findEmulator("melonds")->builds[0].exec == "\\.AppImage$");
		CHECK(arm->findEmulator("melonds")->logo == "https://raw.githubusercontent.com/JyuHo/es-re-catalog/main/logos/melonds.png");
		CHECK(arm->findEmulator("dolphin")->logo == "https://example.org/dolphin.png");
		CHECK(arm->cores.size() == 1);
		CHECK(arm->findCore("mame") == nullptr);
	}

	void testParseRejectsUnknownFormats()
	{
		std::string error;
		CHECK(!parseCatalog("{\"format\": 2}", "x86_64", URL, &error));
		CHECK(error.find("format") != std::string::npos);
		CHECK(!parseCatalog("[]", "x86_64", URL));
		CHECK(!parseCatalog("not json", "x86_64", URL));
	}

	void testStates()
	{
		auto cat = *parseCatalog(CATALOG, "x86_64", URL);
		auto& pcsx2 = *cat.findEmulator("pcsx2");
		auto& dolphin = *cat.findEmulator("dolphin");
		Installed installed;

		CHECK(emulatorState(pcsx2, installed, true, false) == State::Available);
		CHECK(emulatorState(pcsx2, installed, true, true) == State::System);

		// Without flatpak, Dolphin cannot be installed at all
		CHECK(preferredBuild(dolphin, false) == nullptr);
		CHECK(preferredBuild(dolphin, true)->ref == "org.DolphinEmu.dolphin-emu");
		CHECK(preferredBuild(pcsx2, false)->type == BuildType::AppImage);

		installed.emulators["pcsx2"] = { "appimage", "v2.8.1", "/x/pcsx2", { "/x/pcsx2/pcsx2.AppImage" }, "" };
		CHECK(emulatorState(pcsx2, installed, true, true) == State::UpdateAvailable);
		installed.emulators["pcsx2"].version = "v2.8.2";
		CHECK(emulatorState(pcsx2, installed, true, false) == State::Installed);

		installed.emulators["dolphin"] = { "flatpak", "", "", { "flatpak", "run", "org.DolphinEmu.dolphin-emu" }, "org.DolphinEmu.dolphin-emu" };
		CHECK(emulatorState(dolphin, installed, true, false) == State::Installed);

		auto& snes9x = *cat.findCore("snes9x");
		CHECK(coreState(snes9x, installed, false) == State::Available);
		CHECK(coreState(snes9x, installed, true) == State::System);
		installed.cores["snes9x"] = { "core", "2026-11-01", "/c/snes9x_libretro.so", {}, "" };
		CHECK(coreState(snes9x, installed, false) == State::UpdateAvailable);
		installed.cores["snes9x"].version = "2026-11-05";
		CHECK(coreState(snes9x, installed, false) == State::Installed);
	}

	void testInstalledRoundTrip()
	{
		Installed installed;
		installed.emulators["pcsx2"] = { "appimage", "v2.8.2", "/home/me/.local/share/emulationstation/emulators/pcsx2", { "/home/me/.local/share/emulationstation/emulators/pcsx2/pcsx2.AppImage" }, "" };
		installed.emulators["dolphin"] = { "flatpak", "", "", { "flatpak", "run", "org.DolphinEmu.dolphin-emu" }, "org.DolphinEmu.dolphin-emu" };
		installed.cores["snes9x"] = { "core", "2026-11-05", "/c/snes9x_libretro.so", {}, "" };

		auto back = Installed::parse(installed.toJson());
		CHECK(back.emulators.size() == 2);
		CHECK(back.emulators["pcsx2"].command.size() == 1);
		CHECK(back.emulators["dolphin"].ref == "org.DolphinEmu.dolphin-emu");
		CHECK(back.emulators["dolphin"].command.size() == 3);
		CHECK(back.cores["snes9x"].version == "2026-11-05");

		CHECK(Installed::parse("garbage").emulators.empty());
		CHECK(Installed::parse("{\"emulators\": {\"../x\": {\"type\": \"appimage\"}}}").emulators.empty());
	}

	void testIds()
	{
		CHECK(isSafeId("mupen64plus_next"));
		CHECK(isSafeId("org.DolphinEmu.dolphin-emu"));
		CHECK(!isSafeId(""));
		CHECK(!isSafeId(".."));
		CHECK(!isSafeId("-rf"));
		CHECK(!isSafeId("a/b"));
		CHECK(!isSafeId("a b"));
		CHECK(!isSafeId("a;b"));
	}

	void testSystemDefinitions()
	{
		auto psx = findSystemDefinition("psx");
		CHECK(psx != nullptr);
		if (!psx)
			return;

		CHECK(std::string(psx->fullName) == "Sony PlayStation");
		CHECK(std::string(psx->platform) == "psx");
		CHECK(std::string(psx->extensions).find(".cue") != std::string::npos);

		auto gamecube = findSystemDefinition("gamecube");
		CHECK(gamecube != nullptr);
		if (!gamecube)
			return;

		CHECK(std::string(gamecube->platform) == "gc");
		CHECK(std::string(gamecube->theme) == "gc");

		auto msx = findSystemDefinition("msx2+");
		CHECK(msx != nullptr);
		if (!msx)
			return;

		CHECK(std::string(msx->platform) == "msx2+");

		CHECK(findSystemDefinition("not-a-system") == nullptr);
	}

	void testCatalogSystemsHaveDefinitions()
	{
		// Every system of the standalone emulators in es-re-catalog.
		const char* systems[] = {
			"psx", "ps2", "ps3", "psp", "psvita", "ps4",
			"nds", "3ds", "gamecube", "wii", "wiiu", "switch",
			"xbox", "xbox360", "dreamcast", "naomi", "atomiswave",
			"dos", "scummvm", "atarist",
			"c64", "c128", "c20", "cplus4", "pet",
			"msx1", "msx2", "msx2+", "msxturbor", "colecovision", "spectravideo",
		};

		for (const char* system : systems)
			CHECK(findSystemDefinition(system) != nullptr);
	}

	void testSystemNames()
	{
		CHECK(isSafeSystemName("psx"));
		CHECK(isSafeSystemName("3ds"));
		CHECK(isSafeSystemName("xenia-edge"));
		CHECK(isSafeSystemName("msx2+"));

		CHECK(!isSafeSystemName(""));
		CHECK(!isSafeSystemName("."));
		CHECK(!isSafeSystemName(".."));
		CHECK(!isSafeSystemName("../evil"));
		CHECK(!isSafeSystemName("a/b"));
		CHECK(!isSafeSystemName("a b"));

		CHECK(systemConfigFileName("psx") == "es_systems_psx.cfg");
	}

	void testEmulatorCommand()
	{
		InstalledItem appimage;
		appimage.command = { "/home/me/Emulation Station/duckstation.AppImage" };
		CHECK(buildEmulatorCommand(appimage) == "'/home/me/Emulation Station/duckstation.AppImage' %ROM%");

		InstalledItem flatpak;
		flatpak.command = { "flatpak", "run", "org.duckstation.DuckStation" };
		CHECK(buildEmulatorCommand(flatpak) == "'flatpak' 'run' 'org.duckstation.DuckStation' %ROM%");

		InstalledItem empty;
		CHECK(buildEmulatorCommand(empty).empty());
	}

	void testAppendSystem()
	{
		pugi::xml_document base;
		auto baseList = base.append_child("systemList");
		auto baseSystem = baseList.append_child("system");
		baseSystem.append_child("name").text().set("psx");
		baseSystem.append_child("fullname").text().set("Sony PlayStation");
		baseSystem.append_child("path").text().set("~/ROMs/psx");
		baseSystem.append_child("extension").text().set(".cue .chd");
		baseSystem.append_child("command").text().set("retroarch -L %CORE% %ROM%");
		baseSystem.append_child("platform").text().set("psx");
		auto emulators = baseSystem.append_child("emulators");
		auto libretro = emulators.append_child("emulator");
		libretro.append_attribute("name").set_value("libretro");

		pugi::xml_document out;
		auto outList = out.append_child("systemList");
		auto generated = appendSystem(outList, "psx", findSystemDefinition("psx"), baseSystem, "duckstation", "'/x/duckstation' %ROM%", false);

		CHECK(std::string(generated.child("name").text().get()) == "psx");
		CHECK(!generated.child("path")); // the base entry keeps its path, extensions...
		CHECK(!generated.child("fullname"));

		auto generatedEmulators = generated.child("emulators");
		CHECK(std::string(generatedEmulators.child("emulator").attribute("name").value()) == "libretro");
		auto duckstation = generatedEmulators.child("emulator").next_sibling("emulator");
		CHECK(std::string(duckstation.attribute("name").value()) == "duckstation");
		CHECK(duckstation.attribute("catalog").as_bool());
		CHECK(std::string(duckstation.attribute("command").value()) == "'/x/duckstation' %ROM%");

		// Adding the same emulator again reuses its entry
		addEmulator(generated, "duckstation", "'/x/duckstation' %ROM%");
		int count = 0;
		for (auto emulator = generatedEmulators.child("emulator"); emulator; emulator = emulator.next_sibling("emulator"))
			++count;
		CHECK(count == 2);

		// A system that is in no config gets a complete entry from the definition
		pugi::xml_document full;
		auto fullList = full.append_child("systemList");
		auto gamecube = appendSystem(fullList, "gamecube", findSystemDefinition("gamecube"), {}, "dolphin", "'/x/dolphin' %ROM%", true);
		CHECK(std::string(gamecube.child("fullname").text().get()) == "Nintendo GameCube");
		CHECK(std::string(gamecube.child("path").text().get()) == "~/ROMs/gamecube");
		CHECK(std::string(gamecube.child("platform").text().get()) == "gc");
		CHECK(std::string(gamecube.child("emulators").child("emulator").attribute("name").value()) == "dolphin");
	}

	void testWriteSystemConfigs()
	{
		const std::string dir = tempDir();
		const std::string base = dir + "/es_systems.cfg";
		writeFile(base, R"(<systemList>
  <system>
    <name>psx</name>
    <fullname>Sony PlayStation</fullname>
    <path>~/ROMs/psx</path>
    <extension>.cue .chd</extension>
    <command>retroarch -L %CORE% %ROM%</command>
    <platform>psx</platform>
    <theme>psx</theme>
    <emulators>
      <emulator name="libretro"><cores><core name="mednafen_psx" default="true"/></cores></emulator>
    </emulators>
  </system>
</systemList>)");

		Emulator duckstation;
		duckstation.id = "duckstation";
		duckstation.systems = { "psx", "gamecube" };

		InstalledItem item;
		item.command = { "/home/me/DuckStation.AppImage" };

		SystemConfigPaths paths;
		paths.configDir = dir;
		paths.baseConfigPath = base;
		paths.homePath = dir + "/home";
		paths.roots = { dir };

		std::string error;
		CHECK(writeSystemConfigs(duckstation, item, paths, &error));
		CHECK(error.empty());

		// psx: the base entry keeps its path/extensions, the emulator is added
		pugi::xml_document psx;
		CHECK(psx.load_file((dir + "/es_systems_psx.cfg").c_str()));
		auto psxSystem = psx.child("systemList").child("system");
		CHECK(std::string(psxSystem.child("name").text().get()) == "psx");
		CHECK(!psxSystem.child("path"));
		CHECK(!psxSystem.child("fullname"));
		auto psxEmulators = psxSystem.child("emulators");
		CHECK(std::string(psxEmulators.child("emulator").attribute("name").value()) == "libretro");
		auto duckstationNode = psxEmulators.child("emulator").next_sibling("emulator");
		CHECK(std::string(duckstationNode.attribute("name").value()) == "duckstation");
		CHECK(duckstationNode.attribute("catalog").as_bool());
		CHECK(std::string(duckstationNode.attribute("command").value()) == "'/home/me/DuckStation.AppImage' %ROM%");

		// gamecube: no base entry, a complete definition and its ROM folder
		pugi::xml_document gamecube;
		CHECK(gamecube.load_file((dir + "/es_systems_gamecube.cfg").c_str()));
		auto gamecubeSystem = gamecube.child("systemList").child("system");
		CHECK(std::string(gamecubeSystem.child("fullname").text().get()) == "Nintendo GameCube");
		CHECK(std::string(gamecubeSystem.child("path").text().get()) == "~/ROMs/gamecube");
		CHECK(std::string(gamecubeSystem.child("platform").text().get()) == "gc");
		CHECK(std::filesystem::is_directory(dir + "/home/ROMs/gamecube"));

		// A second emulator for the same system is added to the same file
		Emulator second;
		second.id = "duckstation2";
		second.systems = { "psx" };
		InstalledItem secondItem;
		secondItem.command = { "/home/me/DuckStation2.AppImage" };
		CHECK(writeSystemConfigs(second, secondItem, paths, &error));

		pugi::xml_document both;
		CHECK(both.load_file((dir + "/es_systems_psx.cfg").c_str()));
		auto bothEmulators = both.child("systemList").child("system").child("emulators");
		int emulatorCount = 0;
		for (auto emulator = bothEmulators.child("emulator"); emulator; emulator = emulator.next_sibling("emulator"))
			++emulatorCount;
		CHECK(emulatorCount == 3);

		// Removing one keeps the file while another catalog emulator remains
		CHECK(removeSystemConfigs("duckstation", dir, &error));
		CHECK(std::filesystem::exists(dir + "/es_systems_psx.cfg"));
		CHECK(!std::filesystem::exists(dir + "/es_systems_gamecube.cfg"));

		pugi::xml_document remaining;
		CHECK(remaining.load_file((dir + "/es_systems_psx.cfg").c_str()));
		auto remainingEmulators = remaining.child("systemList").child("system").child("emulators");
		emulatorCount = 0;
		for (auto emulator = remainingEmulators.child("emulator"); emulator; emulator = emulator.next_sibling("emulator"))
			++emulatorCount;
		CHECK(emulatorCount == 2);

		// Removing the last catalog emulator deletes the generated file
		CHECK(removeSystemConfigs("duckstation2", dir, &error));
		CHECK(!std::filesystem::exists(dir + "/es_systems_psx.cfg"));

		// A user file is kept, minus the catalog emulator
		writeFile(dir + "/es_systems_psx.cfg", R"(<systemList>
  <system>
    <name>psx</name>
    <path>~/ROMs/psx</path>
    <extension>.cue</extension>
    <command>retroarch %ROM%</command>
    <emulators>
      <emulator name="duckstation" catalog="true" command="duckstation %ROM%"/>
      <emulator name="libretro"/>
    </emulators>
  </system>
</systemList>)");

		CHECK(removeSystemConfigs("duckstation", dir, &error));
		CHECK(std::filesystem::exists(dir + "/es_systems_psx.cfg"));

		pugi::xml_document user;
		CHECK(user.load_file((dir + "/es_systems_psx.cfg").c_str()));
		auto userEmulators = user.child("systemList").child("system").child("emulators");
		int count = 0;
		for (auto emulator = userEmulators.child("emulator"); emulator; emulator = emulator.next_sibling("emulator"))
			++count;
		CHECK(count == 1);
		CHECK(std::string(userEmulators.child("emulator").attribute("name").value()) == "libretro");

		// A user-managed emulator with the same name is not duplicated
		std::filesystem::remove(dir + "/es_systems_psx.cfg");
		writeFile(base, R"(<systemList>
  <system>
    <name>psx</name>
    <path>~/ROMs/psx</path>
    <extension>.cue</extension>
    <command>duckstation %ROM%</command>
    <emulators>
      <emulator name="duckstation"/>
    </emulators>
  </system>
</systemList>)");

		Emulator userDuckstation;
		userDuckstation.id = "duckstation";
		userDuckstation.systems = { "psx" };
		CHECK(writeSystemConfigs(userDuckstation, item, paths, &error));
		CHECK(!std::filesystem::exists(dir + "/es_systems_psx.cfg"));

		std::filesystem::remove_all(dir);
	}
}

int main()
{
	testSha256();
	testParseKeepsWhatRunsHere();
	testParseRejectsUnknownFormats();
	testStates();
	testInstalledRoundTrip();
	testIds();
	testSystemDefinitions();
	testCatalogSystemsHaveDefinitions();
	testSystemNames();
	testEmulatorCommand();
	testAppendSystem();
	testWriteSystemConfigs();

	if (failures == 0)
		std::printf("catalog tests passed\n");

	return failures == 0 ? 0 : 1;
}
