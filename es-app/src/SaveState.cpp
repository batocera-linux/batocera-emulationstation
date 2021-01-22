#include "SaveState.h"
#include "SystemData.h"
#include "FileData.h"
#include "utils/StringUtil.h"
#include "ApiSystem.h"
#include "FileData.h"
#include "SaveStateRepository.h"

std::string SaveState::setupSaveState(FileData* game, const std::string& command)
{
	if (game == nullptr)
		return command;

	auto path = game->getSourceFileData()->getSystem()->getSaveStateRepository()->getSavesPath();
	if (path.empty())
		return command;
	
	// We start games with new slots : If the users saves the game, we don't loose the previous save
	int nextSlot = SaveStateRepository::getNextFreeSlot(game);

	if (!isSlotValid())
	{
		if (nextSlot > 0)
		{
			// We start a game normally but there are saved games : Start game on next free slot to avoid loosing a saved game
			return command + " -state_slot " + std::to_string(nextSlot);
		}

		return command;
	}

	std::string cmd = command;
	if (slot == -1) // Run current AutoSave
		cmd = cmd + " -autosave 1 -state_slot " + std::to_string(nextSlot);
	else
	{
		if (slot == -2) // Run new game without AutoSave
			cmd = cmd + " -autosave 0 -state_slot " + std::to_string(nextSlot);
		else
		{
			cmd = cmd + " -state_slot " + std::to_string(nextSlot); // slot

			// Run game, and activate AutoSave to load it
			if (!fileName.empty())
				cmd = cmd + " -autosave 1";
		}

		// Copy to state.auto file
		auto autoFilename = Utils::FileSystem::combine(path, Utils::FileSystem::getStem(game->getPath()) + ".state.auto");
		if (Utils::FileSystem::exists(autoFilename))
		{
			Utils::FileSystem::removeFile(autoFilename + ".bak");
			Utils::FileSystem::renameFile(autoFilename, autoFilename + ".bak");				
		}

		// Copy to state.auto.png file
		auto autoImage = Utils::FileSystem::combine(path, Utils::FileSystem::getStem(game->getPath()) + ".state.auto.png");
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

			if (nextSlot >= 0)
			{
				// Copy file to new slot, if the users want to reload the saved game in the slot directly from retroach
				mNewSlotFile = Utils::FileSystem::combine(path, Utils::FileSystem::getStem(game->getPath()) + ".state" + std::string(nextSlot == 0 ? "" : std::to_string(nextSlot)));
				Utils::FileSystem::removeFile(mNewSlotFile);
				if (Utils::FileSystem::copyFile(fileName, mNewSlotFile))
					mNewSlotCheckSum = ApiSystem::getInstance()->getMD5(fileName, false);
			}
		}
	}
	
	return cmd;
}

void SaveState::onGameEnded()
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

std::string SaveState::getScreenShot() const
{
	if (!fileName.empty() && Utils::FileSystem::exists(fileName + ".png"))
		return fileName + ".png";

	return "";
}