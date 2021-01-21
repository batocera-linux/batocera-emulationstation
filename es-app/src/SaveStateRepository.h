#pragma once

#include <string>
#include <vector>
#include <map>

#include "SaveState.h"

class SystemData;
class FileData;

class SaveStateRepository
{
public:
	SaveStateRepository(SystemData* system);
	~SaveStateRepository();

	static bool isEnabled(FileData* game);
	static int	getNextFreeSlot(FileData* game);
	
	bool copyToSlot(const SaveState& state, int slot);
	void deleteSaveState(const SaveState& state) const;

	bool hasSaveStates(FileData* game);

	std::vector<SaveState*> getSaveStates(FileData* game);

	std::string getSavesPath();

	void clear();
	void refresh();

private:
	SystemData* mSystem;
	std::map<std::string, std::vector<SaveState*>> mStates;
};
