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

	if (!isSlotValid())
		return command;

	auto path = game->getSourceFileData()->getSystem()->getSaveStateRepository()->getSavesPath();
	if (path.empty())
		return command;

	int nextSlot = SaveStateRepository::getNextFreeSlot(game);

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
			Utils::FileSystem::copyFile(fileName, autoFilename);
	}
	
	return cmd;
}

void SaveState::onGameEnded()
{
	if (slot < 0)
		return;

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