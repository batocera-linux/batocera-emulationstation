#pragma once

#include <string>
#include <vector>
#include <map>

#include "SaveState.h"

class SystemData;
class FileData;

class RegSaveState {
public:
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
};

class SaveStateRepository
{
public:
	SaveStateRepository(SystemData* system);
	~SaveStateRepository();

	static bool isEnabled(FileData* game);
	static int	getNextFreeSlot(FileData* game);
	static void renumberSlots(FileData* game);

	bool hasSaveStates(FileData* game);

	std::vector<SaveState*> getSaveStates(FileData* game);

	std::string getSavesPath();

	void clear();
	void refresh();

private:
	SystemData* mSystem;
	std::map<std::string, std::vector<SaveState*>> mStates;

	static std::map<std::string, RegSaveState*> mRegSaveStates;
	static bool mRegSaveStatesLoaded;
	static RegSaveState* getRegSaveState(std::string emulator, std::string core);
	RegSaveState* getRegSaveState() const;

};
