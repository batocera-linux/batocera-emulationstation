#pragma once

#include <string>
#include <vector>
#include <map>
#include <regex>

class SystemData;
class FileData;

class RegSaveState
{
public:
	friend class SaveStateConfigFile;

	bool enabled;
	std::string directory;
	std::string file;
	int romGroup;
	int slotGroup;
	std::string image;
	bool nofileextension;
	int firstslot;
	bool autosave;
	std::string autosave_file;
	std::string autosave_image;
	bool incremental;
	
	bool matchSlotFile(const std::string& filename, std::string& romName, int& slot);
	bool matchAutoFile(const std::string& filename, std::string& romName);

private:
	std::regex slotFileRegEx;
	std::regex autoFileRegEx;
};

class SaveStateConfigFile
{
public:
	static bool isEnabled();
	static RegSaveState* getRegSaveState(const std::string& emulator, const std::string& core);
	static RegSaveState* getRegSaveState(SystemData* system);

private:
	SaveStateConfigFile();
	RegSaveState* internalGetRegSaveState(const std::string& emulator, const std::string& core);

	static SaveStateConfigFile* Instance();

private:
	bool mRegSaveStatesMode;
	std::map<std::string, RegSaveState*> mRegSaveStates;
};