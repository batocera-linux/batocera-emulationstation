#pragma once

#include <string>
#include <vector>
#include <map>
#include <regex>

class SystemData;
class FileData;

struct SaveStateCoreConfig
{
	SaveStateCoreConfig() { enabled = true; }

	bool enabled;
	std::string name;
	std::string directory;
	std::string system;
};

class SaveStateConfig
{
public:
	SaveStateConfig();

	static std::shared_ptr<SaveStateConfig> Default();

	friend class SaveStateConfigFile;

	bool matchSlotFile(const std::string& filename, std::string& romName, int& slot);
	bool matchAutoFile(const std::string& filename, std::string& romName);
	std::string getDirectory(SystemData* system);

	bool enabled;
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

	std::string emulator;
	std::string core;

	bool racommands;

	bool equals(std::shared_ptr<SaveStateConfig> pOther)
	{
		return pOther->directory == directory && pOther->emulator == emulator && pOther->core == core;
	}

	bool isDefaultConfig(SystemData* system);
	bool isActiveConfig(FileData* system);

private:	
	std::vector<SaveStateCoreConfig> coreConfigs;

	std::string directory;
	std::string defaultCoreDirectory;

	std::regex slotFileRegEx;
	std::regex autoFileRegEx;

	void SetupRegEx();
};

class SaveStateConfigFile
{
public:
	static bool isEnabled();
	static std::vector<std::shared_ptr<SaveStateConfig>> getSaveStateConfigs(SystemData* system);

private:
	SaveStateConfigFile();
	std::shared_ptr<SaveStateConfig> getSaveStateConfig(const std::string& emulator);

	static SaveStateConfigFile* Instance();

private:
	bool mSaveStateConfigMode;
	std::map<std::string, std::shared_ptr<SaveStateConfig>> mSaveStateConfigs;
};