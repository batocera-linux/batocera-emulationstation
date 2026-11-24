#include "romm/RommSaveLayout.h"

#include "FileData.h"
#include "Log.h"
#include "Paths.h"
#include "SaveStateConfigFile.h"
#include "SystemData.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "utils/ZipFile.h"
#include "utils/md5.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>

// libretro-common, built in libcheevos
extern "C" bool rzipstream_read_file(const char* path, void** buf, int64_t* len);

namespace Romm
{
	// Saves named after the rom, by emulator (states: es_savestates.cfg)
#ifdef BATOCERA
	struct SaveRule
	{
		const char* emulator;
		const char* folder;                     // under the saves path
		std::vector<std::string> extensions;    // <rom stem><ext>
		bool folderSave;                        // <folder>/<rom stem>/
	};

	static const std::vector<SaveRule>& saveRules()
	{
		static const std::vector<SaveRule> rules =
		{
			// RetroArch: /userdata/saves/<system>
			{ "libretro", "{{system}}", { ".srm", ".sav", ".rtc", ".dsv", ".eep", ".sra", ".fla", ".mpk", ".mcr", ".brm", ".bkr", ".bcr" }, false },
			{ "melonds", "nds", { ".sav" }, false },
			{ "mame", "mame/nvram", { }, true },
		};

		return rules;
	}
#endif

	// Utils::FileSystem reads text mode on Windows and drops a BOM
	std::string readBinaryFile(const std::string& path)
	{
#if defined(_WIN32)
		std::ifstream file(Utils::String::convertToWideString(path), std::ios::binary);
#else
		std::ifstream file(path, std::ios::binary);
#endif
		std::ostringstream data;
		data << file.rdbuf();
		return data.str();
	}

	bool writeBinaryFile(const std::string& path, const std::string& bytes)
	{
		std::string tmp = path + ".tmp";
		{
#if defined(_WIN32)
			std::ofstream file(Utils::String::convertToWideString(tmp), std::ios::binary | std::ios::trunc);
#else
			std::ofstream file(tmp, std::ios::binary | std::ios::trunc);
#endif
			if (!file)
				return false;

			file.write(bytes.data(), (std::streamsize)bytes.size());
			if (!file)
				return false;
		}

		return Utils::FileSystem::renameFile(tmp, path, true) && Utils::FileSystem::exists(path);
	}

	static std::string md5OfFile(const std::string& path)
	{
		return Utils::String::toLower(Utils::FileSystem::getFileMd5(path));
	}

	// Other sync tools' leftovers
	static bool isForeignCopy(const std::string& fileName)
	{
		return fileName.find(".sync-conflict-") != std::string::npos || Utils::String::endsWith(fileName, ".tmp") || Utils::String::endsWith(fileName, ".romm-part");
	}

	std::string LocalSaveUnit::fingerprint() const
	{
		std::string text = "save:" + saveHash;
		for (auto& state : states)
			text += "|" + state.first + ":" + state.second.hash;

		return MD5(text).hexdigest();
	}

	SaveLayout RommSaveLayout::forGame(FileData* game)
	{
		SaveLayout layout;

#ifndef BATOCERA
		// The rules above are Batocera's: none for RetroBat yet
		(void)game;
		return layout;
#else
		if (game == nullptr || game->getType() != GAME)
			return layout;

		FileData* source = game->getSourceFileData();
		SystemData* system = source->getSystem();
		if (system == nullptr)
			return layout;

		layout.emulator = source->getEmulator();
		layout.core = source->getCore();
		layout.bankCore = layout.emulator == "libretro" && !layout.core.empty() ? libretroCoreName(layout.core) : layout.emulator;
		if (layout.emulator == "libretro" && layout.bankCore != layout.core)
			layout.bankAlias = layout.core;
		if (layout.bankCore.empty())
			return layout;

		const std::string path = source->getPath();
		const std::string savesPath = Paths::getSavesPath();

		for (auto& rule : saveRules())
		{
			if (layout.emulator != rule.emulator)
				continue;

			layout.saveDir = Utils::FileSystem::combine(savesPath, Utils::String::replace(rule.folder, "{{system}}", system->getName()));
			layout.saveStem = Utils::FileSystem::getStem(path);
			layout.saveExtensions = rule.extensions;
			layout.saveIsFolder = rule.folderSave;
			break;
		}

		// es_savestates.cfg: this emulator and core, else the emulator
		std::shared_ptr<SaveStateConfig> states;
		for (auto& conf : SaveStateConfigFile::getSaveStateConfigs(system))
			if (conf->emulator == layout.emulator && conf->core == layout.core)
				states = conf;

		if (states == nullptr)
			for (auto& conf : SaveStateConfigFile::getSaveStateConfigs(system))
				if (conf->emulator == layout.emulator && states == nullptr)
					states = conf;

		if (states != nullptr && !states->file.empty())
		{
			layout.states = states;
			layout.statesDir = states->getDirectory(system);
			layout.statesRomName = states->nofileextension ? Utils::FileSystem::getStem(path) : Utils::FileSystem::getFileName(path);
		}

		layout.valid = layout.hasSave() || layout.hasStates();
		return layout;
#endif
	}

	static std::string slotText(const std::string& pattern, const std::string& romName, const std::string& slot)
	{
		int n = Utils::String::toInteger(slot);
		std::string text = Utils::String::replace(pattern, "{{romfilename}}", romName);
		text = Utils::String::replace(text, "{{slot}}", n == 0 ? "" : std::to_string(n));
		text = Utils::String::replace(text, "{{slot0}}", std::to_string(n));
		text = Utils::String::replace(text, "{{slot00}}", Utils::String::padLeft(std::to_string(n), 2, '0'));
		text = Utils::String::replace(text, "{{slot2d}}", Utils::String::padLeft(std::to_string(n), 2, '0'));
		return text;
	}

	std::string RommSaveLayout::stateFile(const SaveLayout& layout, const std::string& slot)
	{
		if (!layout.hasStates())
			return "";

		std::string name = slot == "auto" ? slotText(layout.states->autosave_file, layout.statesRomName, "0") : slotText(layout.states->file, layout.statesRomName, slot);
		return name.empty() ? "" : Utils::FileSystem::combine(layout.statesDir, name);
	}

	std::string RommSaveLayout::libretroCoreName(const std::string& core)
	{
#ifdef BATOCERA
		// Batocera's renamed cores (other builds such as genesisplusgx-wide keep their name)
		static const std::map<std::string, std::string> renamed =
		{
			{ "genesisplusgx", "genesis_plus_gx" },
			{ "mupen64plus-next", "mupen64plus_next" },
			{ "pce", "mednafen_pce" },
			{ "pce_fast", "mednafen_pce_fast" },
			{ "pcfx", "mednafen_pcfx" },
			{ "vb", "mednafen_vb" },
			{ "beetle-saturn", "mednafen_saturn_hw" },
			{ "mame078plus", "mame2003_plus" },
			{ "imame4all", "mame2000" },
			{ "fbalpha", "fbalpha2012" },
			{ "pocketsnes", "snes9x2002" },
			{ "snes9x_next", "snes9x2010" },
			{ "vba-m", "vbam" },
			{ "bsnes_hd", "bsnes_hd_beta" },
		};

		auto it = renamed.find(core);
		return it == renamed.cend() ? core : it->second;
#else
		return core;
#endif
	}

	long long RommSaveLayout::stateCoreSize(const std::string& path)
	{
		void* buf = nullptr;
		int64_t len = 0;
		if (!rzipstream_read_file(path.c_str(), &buf, &len) || buf == nullptr)
			return -1;

		// Other states may vary in size
		const unsigned char* data = (const unsigned char*)buf;
		long long size = -1;
		if (len >= 16 && memcmp(data, "RASTATE", 7) == 0)
		{
			int64_t at = 8;
			while (at + 8 <= len)
			{
				uint32_t block = data[at + 4] | (data[at + 5] << 8) | (data[at + 6] << 16) | ((uint32_t)data[at + 7] << 24);
				if (memcmp(data + at, "MEM ", 4) == 0)
				{
					size = block;
					break;
				}
				if (memcmp(data + at, "END ", 4) == 0)
					break;
				at += 8 + ((block + 7) & ~(int64_t)7);
			}
		}

		free(buf);
		return size;
	}

	std::string RommSaveLayout::stateScreenshot(const SaveLayout& layout, const std::string& slot)
	{
		if (!layout.hasStates())
			return "";

		std::string pattern = slot == "auto" ? layout.states->autosave_image : layout.states->image;
		if (pattern.empty())
			return stateFile(layout, slot) + ".png";

		return Utils::FileSystem::combine(layout.statesDir, slotText(pattern, layout.statesRomName, slot));
	}

	// (name in the zip, file path)
	static std::vector<std::pair<std::string, std::string>> saveMembers(const SaveLayout& layout, const LocalSaveUnit& unit)
	{
		std::string root = layout.saveIsFolder ? Utils::FileSystem::combine(layout.saveDir, layout.saveStem) : layout.saveDir;
		std::vector<std::pair<std::string, std::string>> members;
		for (auto& file : unit.saveFiles)
		{
			std::string relative = Utils::FileSystem::createRelativePath(file, root, false);
			if (Utils::String::startsWith(relative, "./"))
				relative = relative.substr(2);

			members.push_back(std::make_pair(relative, file));
		}
		return members;
	}

	// RomM's hash of a zip: md5 of the "name:md5" lines of its files, sorted by name
	static std::string zipContentHash(std::vector<std::pair<std::string, std::string>> members)
	{
		std::sort(members.begin(), members.end());
		std::string lines;
		for (auto& member : members)
			lines += (lines.empty() ? "" : "\n") + member.first + ":" + md5OfFile(member.second);
		return MD5(lines).hexdigest();
	}

	LocalSaveUnit RommSaveLayout::readLocal(const SaveLayout& layout)
	{
		LocalSaveUnit unit;

		if (layout.hasSave())
		{
			if (layout.saveIsFolder)
			{
				std::string folder = Utils::FileSystem::combine(layout.saveDir, layout.saveStem);
				if (Utils::FileSystem::isDirectory(folder))
					for (auto& file : Utils::FileSystem::getDirContent(folder, true))
						if (!Utils::FileSystem::isDirectory(file) && !isForeignCopy(Utils::FileSystem::getFileName(file)))
							unit.saveFiles.push_back(file);
			}
			else
			{
				for (auto& ext : layout.saveExtensions)
				{
					std::string file = Utils::FileSystem::combine(layout.saveDir, layout.saveStem + ext);
					if (Utils::FileSystem::isRegularFile(file) && Utils::FileSystem::getFileSize(file) > 0)
						unit.saveFiles.push_back(file);
				}
			}

			std::sort(unit.saveFiles.begin(), unit.saveFiles.end());

			if (unit.saveFiles.size() == 1 && !layout.saveIsFolder)
			{
				unit.saveShape = "SINGLE";
				unit.saveHash = md5OfFile(unit.saveFiles[0]);
			}
			else if (!unit.saveFiles.empty())
			{
				unit.saveShape = layout.saveIsFolder ? "FOLDER" : "MULTI";
				unit.saveHash = zipContentHash(saveMembers(layout, unit));
			}
		}

		if (layout.hasStates() && Utils::FileSystem::isDirectory(layout.statesDir))
		{
			for (auto& file : Utils::FileSystem::getDirectoryFiles(layout.statesDir))
			{
				if (file.hidden || file.directory)
					continue;

				std::string fileName = Utils::FileSystem::getFileName(file.path);
				if (isForeignCopy(fileName))
					continue;

				std::string rom;
				int slot = -1;
				std::string slotName;
				if (layout.states->matchSlotFile(fileName, rom, slot))
					slotName = std::to_string(slot);
				else if (layout.states->matchAutoFile(fileName, rom))
					slotName = "auto";
				else
					continue;

				if (rom != layout.statesRomName || Utils::FileSystem::getFileSize(file.path) <= 0)
					continue;

				LocalState state;
				state.slot = slotName;
				state.path = file.path;
				state.hash = md5OfFile(file.path);

				std::string screenshot = stateScreenshot(layout, slotName);
				if (Utils::FileSystem::exists(screenshot))
					state.screenshot = screenshot;
				else if (Utils::FileSystem::exists(file.path + ".png"))
					state.screenshot = file.path + ".png";

				unit.states[slotName] = state;
			}
		}

		return unit;
	}

	// --- Stored zip ------------------------------------------------------------------------

	static void put16(std::string& s, unsigned v) { s += (char)(v & 0xFF); s += (char)((v >> 8) & 0xFF); }
	static void put32(std::string& s, unsigned v) { put16(s, v & 0xFFFF); put16(s, (v >> 16) & 0xFFFF); }

	bool RommSaveLayout::writeZip(const std::vector<std::pair<std::string, std::string>>& members, std::string& bytes)
	{
		auto sorted = members;
		std::sort(sorted.begin(), sorted.end());

		std::string central;
		bytes.clear();

		// Fixed date: same files, same hash
		const unsigned dosTime = 0, dosDate = (0 << 9) | (1 << 5) | 1;

		for (auto& member : sorted)
		{
			std::string data = readBinaryFile(member.second);
			if (data.size() >= 0xFFFFFFFFULL)
				return false;

			unsigned crc = Utils::Zip::ZipFile::computeCRC(0, data.data(), data.size());
			unsigned offset = (unsigned)bytes.size();
			const std::string& name = member.first;

			put32(bytes, 0x04034b50); put16(bytes, 20); put16(bytes, 0x0800); put16(bytes, 0);
			put16(bytes, dosTime); put16(bytes, dosDate); put32(bytes, crc);
			put32(bytes, (unsigned)data.size()); put32(bytes, (unsigned)data.size());
			put16(bytes, (unsigned)name.size()); put16(bytes, 0);
			bytes += name;
			bytes += data;

			put32(central, 0x02014b50); put16(central, 20); put16(central, 20); put16(central, 0x0800); put16(central, 0);
			put16(central, dosTime); put16(central, dosDate); put32(central, crc);
			put32(central, (unsigned)data.size()); put32(central, (unsigned)data.size());
			put16(central, (unsigned)name.size()); put16(central, 0); put16(central, 0); put16(central, 0); put16(central, 0);
			put32(central, 0); put32(central, offset);
			central += name;
		}

		unsigned centralOffset = (unsigned)bytes.size();
		bytes += central;
		put32(bytes, 0x06054b50); put16(bytes, 0); put16(bytes, 0);
		put16(bytes, (unsigned)sorted.size()); put16(bytes, (unsigned)sorted.size());
		put32(bytes, (unsigned)central.size()); put32(bytes, centralOffset); put16(bytes, 0);
		return true;
	}

	bool RommSaveLayout::packSave(const SaveLayout& layout, const LocalSaveUnit& unit, std::string& bytes)
	{
		if (!unit.hasSave())
			return false;

		if (unit.saveFiles.size() == 1 && !layout.saveIsFolder)
		{
			bytes = readBinaryFile(unit.saveFiles[0]);
			return true;
		}

		return writeZip(saveMembers(layout, unit), bytes);
	}

	// Never outside the target folder
	static bool safeMember(const std::string& name)
	{
		return !name.empty() && name[0] != '/' && name.find("..") == std::string::npos && name.find('\\') == std::string::npos;
	}

	bool RommSaveLayout::placeSave(const SaveLayout& layout, const std::string& downloaded, const std::string& shape, const std::string& serverFileName)
	{
		if (!layout.hasSave())
			return false;

		Utils::FileSystem::createDirectory(layout.saveDir);

		if (shape == "SINGLE" || shape.empty())
		{
			std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(serverFileName));
			if (layout.saveIsFolder || std::find(layout.saveExtensions.cbegin(), layout.saveExtensions.cend(), ext) == layout.saveExtensions.cend())
				ext = layout.saveExtensions.empty() ? "" : layout.saveExtensions[0];

			if (ext.empty())
				return false;

			std::string target = Utils::FileSystem::combine(layout.saveDir, layout.saveStem + ext);
			Utils::FileSystem::removeFile(target);
			return Utils::FileSystem::renameFile(downloaded, target, true) && Utils::FileSystem::exists(target);
		}

		Utils::Zip::ZipFile zip;
		if (!zip.load(downloaded))
			return false;

		std::string root = layout.saveIsFolder ? Utils::FileSystem::combine(layout.saveDir, layout.saveStem) : layout.saveDir;
		Utils::FileSystem::createDirectory(root);

		for (auto& name : zip.namelist())
		{
			if (Utils::String::endsWith(name, "/") || !safeMember(name))
				continue;

			// Renamed after this rom
			std::string target = name;
			if (!layout.saveIsFolder)
			{
				std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(name));
				if (std::find(layout.saveExtensions.cbegin(), layout.saveExtensions.cend(), ext) == layout.saveExtensions.cend())
					continue;

				target = layout.saveStem + ext;
			}

			std::string full = Utils::FileSystem::combine(root, target);
			Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(full));
			if (!zip.extract(name, full, true))
				return false;
		}

		return true;
	}

	void RommSaveLayout::backup(const SaveLayout& layout, const LocalSaveUnit& unit, const std::string& dir)
	{
		(void)layout;
		if (unit.empty())
			return;

		Utils::FileSystem::createDirectory(dir);

		std::vector<std::string> files = unit.saveFiles;
		for (auto& state : unit.states)
		{
			files.push_back(state.second.path);
			if (!state.second.screenshot.empty())
				files.push_back(state.second.screenshot);
		}

		for (auto& file : files)
			Utils::FileSystem::copyFile(file, Utils::FileSystem::combine(dir, Utils::FileSystem::getFileName(file)));
	}
}
