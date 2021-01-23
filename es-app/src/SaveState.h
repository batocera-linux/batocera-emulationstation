#pragma once

#include <string>
#include "utils/TimeUtil.h"

class FileData;

struct SaveState
{
	SaveState()
	{
		slot = -99;
	}

	SaveState(int slotId)
	{
		slot = slotId;
	}
	
	bool isSlotValid() const { return slot != -99; }
	
	std::string rom;
	std::string fileName;
	std::string getScreenShot() const;
	int slot;

	Utils::Time::DateTime creationDate;

public:
	std::string setupSaveState(FileData* game, const std::string& command);
	void onGameEnded();

private:
	std::string mAutoFileBackup;
	std::string mAutoImageBackup;


	std::string mNewSlotFile;	
	std::string mNewSlotCheckSum;
};
