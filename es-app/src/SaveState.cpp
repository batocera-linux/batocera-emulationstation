#include "SaveState.h"
#include "SystemData.h"
#include "FileData.h"
#include "utils/StringUtil.h"
#include "ApiSystem.h"
#include "FileData.h"
#include "SaveStateRepository.h"
#include "SystemConf.h"

std::string SaveState::makeStateFilename(int slot, bool fullPath) const
{
  std::string ret;

  if(slot < 0) {
    // only for the default behavior
    ret = this->rom + ".state" + (slot < 0 ? ".auto" : (slot == 0 ? "" : std::to_string(slot)));
  } else {
    ret = fileGenerator;
    ret = Utils::String::replace(ret, "{{slot}}", std::to_string(slot));
    if(slot < 10) {
      ret = Utils::String::replace(ret, "{{slot2d}}", "0" + std::to_string(slot));
    } else {
      ret = Utils::String::replace(ret, "{{slot2d}}", std::to_string(slot));
    }
  }
  
  if (fullPath) 
    return Utils::FileSystem::combine(Utils::FileSystem::getParent(fileName), ret);

  return ret;
}

std::string SaveState::getScreenShot() const
{
  return screenshot;
}

std::string SaveState::setupSaveState(FileData* game, const std::string& command)
{
	if (game == nullptr)
		return command;
	
	// We start games with new slots : If the users saves the game, we don't loose the previous save
	int nextSlot = SaveStateRepository::getNextFreeSlot(game);

	if (!isSlotValid())
	{
		if (nextSlot > 0 && !SystemConf::getIncrementalSaveStatesUseCurrentSlot())
		{
			// We start a game normally but there are saved games : Start game on next free slot to avoid loosing a saved game
		  if(racommands) {
		    return command + " -state_slot " + std::to_string(nextSlot);
		  } else {
		    return command + " --state_slot " + std::to_string(nextSlot) + " --state_file \"" + rom + "\"";
		  }
		}

		return command;
	}

	bool incrementalSaveStates;

	incrementalSaveStates = SystemConf::getIncrementalSaveStates() && hasAutosave;
	
	std::string path = Utils::FileSystem::getParent(fileName);

	std::string cmd = command;
	if (slot == -1) // Run current AutoSave
	  if(racommands) {
	    cmd = cmd + " -autosave 1 -state_slot " + std::to_string(nextSlot);
	  } else {
	    cmd = cmd + " --state_slot autosave";
	  }
	else
	{
		if (slot == -2) // Run new game without AutoSave
		{
		  if(racommands) {
		    cmd = cmd + " -autosave 0 -state_slot " + std::to_string(nextSlot);
		  } else {
		    cmd = cmd + " --state_slot " + std::to_string(nextSlot) + " --state_file \"" + rom + "\"";
		  }
		}
		else if (incrementalSaveStates)
		{
		  if(racommands) {
		    cmd = cmd + " -state_slot " + std::to_string(nextSlot); // slot

		    // Run game, and activate AutoSave to load it
		    if (!fileName.empty())
		      cmd = cmd + " -autosave 1";
		  } else {
		    cmd = cmd + " --state_slot " + std::to_string(nextSlot) + " --state_file \"" + rom + "\" --autosave";
		  }
		}
		else
		{
		  if(racommands) {
		    cmd = cmd + " -state_slot " + std::to_string(slot);

		    // Run game, and activate AutoSave to load it
		    if (!fileName.empty())
		      cmd = cmd + " -autosave 1";
		  } else {
		    cmd = cmd + " --state_slot " + std::to_string(nextSlot) + " --state_file \"" + rom + "\"";
		  }
		}

		if(racommands) {
		  // Copy to state.auto file
		  auto autoFilename = makeStateFilename(-1);
		  if (Utils::FileSystem::exists(autoFilename))
		    {
		      Utils::FileSystem::removeFile(autoFilename + ".bak");
		      Utils::FileSystem::renameFile(autoFilename, autoFilename + ".bak");				
		    }

		  // Copy to state.auto.png file
		  auto autoImage = autoFilename + ".png";
		  if (Utils::FileSystem::exists(autoImage))
		    {
		      Utils::FileSystem::removeFile(autoImage + ".bak");
		      Utils::FileSystem::renameFile(autoImage, autoImage + ".bak");				
		    }

		  mAutoImageBackup = autoImage;
		  mAutoFileBackup = autoFilename;

		  if (!fileName.empty())
		    {
		      Utils::FileSystem::copyFile(fileName, autoFilename);
		      
		      if (incrementalSaveStates && nextSlot >= 0 && slot + 1 != nextSlot)
			{
			  // Copy file to new slot, if the users want to reload the saved game in the slot directly from retroach
			  mNewSlotFile = makeStateFilename(nextSlot);
			  Utils::FileSystem::removeFile(mNewSlotFile);
			  if (Utils::FileSystem::copyFile(fileName, mNewSlotFile))
			    mNewSlotCheckSum = ApiSystem::getInstance()->getMD5(fileName, false);
			}
		    }
		}
	}
	
	return cmd;
}

void SaveState::onGameEnded(FileData* game)
{
	if(racommands == false) return;
  
	if (slot < 0)
		return;

	if (!mNewSlotCheckSum.empty() && Utils::FileSystem::exists(mNewSlotFile))
	{
		// Check if the file in the slot has changed. If it's the same, then delete it & clear the slot
		auto fileChecksum = ApiSystem::getInstance()->getMD5(mNewSlotFile, false);
		if (fileChecksum == mNewSlotCheckSum)
			Utils::FileSystem::removeFile(mNewSlotFile);
	}

	if (!mAutoFileBackup.empty())
	{
		Utils::FileSystem::removeFile(mAutoFileBackup);
		Utils::FileSystem::renameFile(mAutoFileBackup + ".bak", mAutoFileBackup);
	}

	if (!mAutoImageBackup.empty())
	{
		Utils::FileSystem::removeFile(mAutoImageBackup);
		Utils::FileSystem::renameFile(mAutoImageBackup + ".bak", mAutoImageBackup);
	}

	if (SystemConf::getIncrementalSaveStates())
		SaveStateRepository::renumberSlots(game);
}

void SaveState::remove() const
{
	if (!isSlotValid())
		return;

	if (!fileName.empty())
		Utils::FileSystem::removeFile(fileName);

	if (!getScreenShot().empty())
		Utils::FileSystem::removeFile(getScreenShot());
}

bool SaveState::copyToSlot(int slot, bool move) const
{
	if (slot < 0)
		return false;

	if (!Utils::FileSystem::exists(fileName))
		return false;

	std::string destState = makeStateFilename(slot);
	std::string destStateImage = imageGenerator;
	destStateImage = Utils::String::replace(destStateImage, "{{slot}}", std::to_string(slot));
	if(slot < 10) {
	  destStateImage = Utils::String::replace(destStateImage, "{{slot2d}}", "0" + std::to_string(slot));
	} else {
	  destStateImage = Utils::String::replace(destStateImage, "{{slot2d}}", std::to_string(slot));
	}
	
	if (move)
	{
		Utils::FileSystem::renameFile(fileName, destState);
		if (!getScreenShot().empty())
			Utils::FileSystem::renameFile(getScreenShot(), destStateImage);

	}
	else
	{
		Utils::FileSystem::copyFile(fileName, destState);
		if (!getScreenShot().empty())
			Utils::FileSystem::copyFile(getScreenShot(), destStateImage);
	}

	return true;
}

