#include "SaveState.h"
#include "SystemData.h"
#include "FileData.h"
#include "utils/StringUtil.h"
#include "ApiSystem.h"
#include "FileData.h"
#include "SaveStateRepository.h"
#include "SystemConf.h"

std::string SaveState::makeStateFilename(int slot, bool fullPath, bool useImageGenerator) const
{
	std::string ret = useImageGenerator ? imageGenerator : fileGenerator;

	if (slot < 0) 
	{
		// only for the default behavior !?
		ret = Utils::String::replace(ret, "{{slot}}", ".auto");
		ret = Utils::String::replace(ret, "{{slot0}}", ".auto");
		ret = Utils::String::replace(ret, "{{slot00}}", ".auto");
		ret = Utils::String::replace(ret, "{{slot2d}}", ".auto");
	}
	else 
	{
		ret = Utils::String::replace(ret, "{{slot}}", slot == 0 ? "" : std::to_string(slot));
		ret = Utils::String::replace(ret, "{{slot0}}", std::to_string(slot));
		ret = Utils::String::replace(ret, "{{slot00}}", Utils::String::padLeft(std::to_string(slot), 2, '0'));
		ret = Utils::String::replace(ret, "{{slot2d}}", Utils::String::padLeft(std::to_string(slot), 2, '0'));	
	}

	if (fullPath)
		return Utils::FileSystem::combine(Utils::FileSystem::getParent(fileName), ret);

	return ret;
}

std::string SaveState::getScreenShot() const
{
  return screenshot;
}

static std::string _changeCommandlineArgument(const std::string& commandLine, const std::string& parameter, const std::string& value)
{
	size_t corePos = commandLine.find(parameter.c_str());
	if (corePos != std::string::npos) 
	{
		corePos += parameter.length();

		while (corePos < commandLine.length() && commandLine[corePos] == ' ')
			corePos++;

		int count = 0;
		while (corePos + count < commandLine.length() && commandLine[corePos+ count] != ' ')
			count++;

		std::string argument = commandLine;

		if (count > 0)
			argument = argument.erase(corePos, count);

		argument = argument.insert(corePos, value);
		return argument;
	}

	return commandLine;
}

std::string SaveState::setupSaveState(FileData* game, const std::string& command)
{
	if (game == nullptr)
		return command;

	std::string cmd = command;

	// Savestate has core information ? Setup correct emulator/core
	if (slot >= -1 && this->config != nullptr && !config->isActiveConfig(game) && !racommands)
	{
		cmd = _changeCommandlineArgument(cmd, "-emulator", config->emulator);
		cmd = _changeCommandlineArgument(cmd, "-core", config->core);
	}	

	bool supportsIncremental = config != nullptr ? config->incremental : game->getSourceFileData()->getSystem()->getSaveStateRepository()->supportsIncrementalSaveStates();

	// We start games with new slots : If the users saves the game, we don't loose the previous save
	int nextSlot = SaveStateRepository::getNextFreeSlot(game, this->config);

	if (!isSlotValid())
	{
		if (nextSlot > 0 && !SystemConf::getIncrementalSaveStatesUseCurrentSlot() && supportsIncremental)
		{
			// We start a game normally but there are saved games : Start game on next free slot to avoid loosing a saved game
			return cmd + " -state_slot " + std::to_string(nextSlot);
		}

		return cmd;
	}

	bool incrementalSaveStates = SystemConf::getIncrementalSaveStates() && /*hasAutosave && */supportsIncremental;

	std::string path = Utils::FileSystem::getParent(fileName);

	if (slot == -1) // Run current AutoSave
	{
		if (racommands || fileName.empty())
			cmd = cmd + " -autosave 1 -state_slot " + std::to_string(nextSlot);
		else
			cmd = cmd + " -state_slot " + std::to_string(nextSlot) + " -state_file \"" + fileName + "\"";
	}
	else
	{
		if (slot == -2) // Run new game without AutoSave
		{
			cmd = cmd + " -autosave 0 -state_slot " + std::to_string(nextSlot);
		}
		else if (incrementalSaveStates)
		{
			if (racommands)
			{
				cmd = cmd + " -state_slot " + std::to_string(nextSlot); // slot

				// Run game, and activate AutoSave to load it
				if (!fileName.empty())
					cmd = cmd + " -autosave 1";
			}
			else
				cmd = cmd + " -state_slot " + std::to_string(nextSlot) + " -state_file \"" + fileName + "\"";
		}
		else
		{
			if (racommands)
			{
				cmd = cmd + " -state_slot " + std::to_string(slot);

				// Run game, and activate AutoSave to load it
				if (!fileName.empty())
					cmd = cmd + " -autosave 1";
			}
			else
				cmd = cmd + " -state_slot " + std::to_string(slot) + " -state_file \"" + fileName + "\"";
		}

		if (racommands) 
		{
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
	if (racommands)
	{
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
	}

	if (this->config != nullptr && this->config->incremental)
		SaveStateRepository::renumberSlots(game, this->config);
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
	std::string destStateImage = makeStateFilename(slot, true, true);
	
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

