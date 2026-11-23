#pragma once
#ifndef ES_APP_ROMM_ROMM_SAVE_LAYOUT_H
#define ES_APP_ROMM_ROMM_SAVE_LAYOUT_H

#include <map>
#include <memory>
#include <string>
#include <vector>

class FileData;
class SaveStateConfig;

namespace Romm
{
	// One state slot on disk ("auto", "0", "1"...)
	struct LocalState
	{
		std::string slot;
		std::string path;
		std::string screenshot; // empty: none
		std::string hash;       // md5
	};

	// Where a game's save (table of emulators, RommSaveLayout.cpp) and states (es_savestates.cfg) are
	struct SaveLayout
	{
		bool valid = false;
		std::string emulator;
		std::string core;
		std::string bankCore;  // RomM states bank: the libretro core, else the emulator
		std::string bankAlias; // Batocera's name for that core, when it differs (read only)

		// <saveDir>/<saveStem><ext>, or the folder <saveDir>/<saveStem>/
		std::string saveDir;
		std::string saveStem;
		std::vector<std::string> saveExtensions;
		bool saveIsFolder = false;

		// States
		std::shared_ptr<SaveStateConfig> states;
		std::string statesDir;
		std::string statesRomName;

		bool hasSave() const { return !saveDir.empty(); }
		bool hasStates() const { return states != nullptr; }
	};

	// What is on disk
	struct LocalSaveUnit
	{
		std::vector<std::string> saveFiles; // absolute paths
		std::string saveHash;               // md5 of what is sent
		std::string saveShape;              // SINGLE, MULTI, FOLDER
		std::map<std::string, LocalState> states; // by slot

		bool hasSave() const { return !saveFiles.empty(); }
		bool empty() const { return saveFiles.empty() && states.empty(); }

		// Save and state hashes
		std::string fingerprint() const;
	};

	// Binary safe; writes go through a .tmp file
	std::string readBinaryFile(const std::string& path);
	bool writeBinaryFile(const std::string& path, const std::string& bytes);

	class RommSaveLayout
	{
	public:
		static SaveLayout forGame(FileData* game);

		static LocalSaveUnit readLocal(const SaveLayout& layout);

		// The file (SINGLE), else a zip of the files
		static bool packSave(const SaveLayout& layout, const LocalSaveUnit& unit, std::string& bytes);

		// serverFileName gives a single file its extension
		static bool placeSave(const SaveLayout& layout, const std::string& downloaded, const std::string& shape, const std::string& serverFileName);

		// Slot "auto", "0", "1"...
		static std::string stateFile(const SaveLayout& layout, const std::string& slot);
		// Core data size in a RetroArch state (RZIP, RASTATE), -1 for any other file
		static long long stateCoreSize(const std::string& path);
		// genesisplusgx -> genesis_plus_gx, as the web player names it; other names unchanged
		static std::string libretroCoreName(const std::string& core);
		static std::string stateScreenshot(const SaveLayout& layout, const std::string& slot);

		// Copies the unit into dir
		static void backup(const SaveLayout& layout, const LocalSaveUnit& unit, const std::string& dir);

		// Stored zip: sorted members, fixed dates
		static bool writeZip(const std::vector<std::pair<std::string, std::string>>& members, std::string& bytes);
	};
}

#endif
