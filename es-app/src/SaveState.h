#pragma once

#include <string>
#include "utils/TimeUtil.h"
#include "SaveStateConfigFile.h"

class FileData;

struct SaveState
{
	friend class SaveStateRepository;

	bool isSlotValid() const { return slot != -99; }
	
	std::string rom;
	std::string fileName;
	std::string screenshot;
	std::string fileGenerator;
  	std::string imageGenerator;
  	bool hasAutosave;
  	bool racommands;
	std::string getScreenShot() const;
	int slot;

	void remove() const;
	bool copyToSlot(int slot, bool move = false) const;

	Utils::Time::DateTime creationDate;

	std::shared_ptr<SaveStateConfig> config;

public:
	virtual std::string makeStateFilename(int slot, bool fullPath = true, bool useImageGenerator = false) const;

	std::string setupSaveState(FileData* game, const std::string& command);
	void onGameEnded(FileData* game);

private:
	SaveState()
	{
		slot = -99;
		hasAutosave = false;
		racommands = false;
	}


	SaveState(int slotId)
	{
		slot = slotId;
		hasAutosave = false;
		racommands = false;
	}

	std::string mAutoFileBackup;
	std::string mAutoImageBackup;


	std::string mNewSlotFile;	
	std::string mNewSlotCheckSum;
};
