#include "SaveStateRepository.h"
#include "SystemData.h"
#include "FileData.h"
#include "utils/StringUtil.h"

#include <time.h>

#if WIN32
#include "Win32ApiSystem.h"
#endif

SaveStateRepository::SaveStateRepository(SystemData* system)
{
	mSystem = system;
	refresh();
}

SaveStateRepository::~SaveStateRepository()
{
	clear();
}

void SaveStateRepository::clear()
{
	for (auto item : mStates)
		for (auto state : item.second)
			delete state;

	mStates.clear();
}

std::string SaveStateRepository::getSavesPath()
{
#if WIN32
	std::string path = Win32ApiSystem::getEmulatorLauncherPath("saves");
	if (path.empty())
		return "";

	return Utils::FileSystem::combine(path, mSystem->getName());
#endif

#ifdef _ENABLEEMUELEC
	return Utils::FileSystem::combine("/storage/roms/savestates/", mSystem->getName());
#else
	return Utils::FileSystem::combine("/userdata/saves/", mSystem->getName());
#endif
}
	
void SaveStateRepository::refresh()
{
	clear();

	auto path = getSavesPath();
	if (!Utils::FileSystem::exists(path))
		return;

	auto files = Utils::FileSystem::getDirectoryFiles(path);
	for (auto file : files)
	{
		if (file.hidden || file.directory)
			continue;

		std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(file.path));

		if (ext == ".bak")
		{
			// TODO RESTORE BAK FILE !? If board was turned off during a game ?
		}

		if (ext != ".auto" && !Utils::String::startsWith(ext, ".state"))
			continue;

		SaveState* state = new SaveState();

		if (ext == ".auto")
			state->slot = -1;
		else if (Utils::String::startsWith(ext, ".state"))
			state->slot = Utils::String::toInteger(ext.substr(6));

		auto stem = Utils::FileSystem::getStem(file.path);
		if (Utils::String::endsWith(stem, ".state"))
			stem = Utils::FileSystem::getStem(stem);
				
		state->rom = stem;
		state->fileName = file.path;
		state->creationDate = Utils::FileSystem::getFileModificationDate(state->fileName);

		mStates[stem].push_back(state);
	}
}

bool SaveStateRepository::hasSaveStates(FileData* game)
{
	if (mStates.size())
	{
		if (game->getSourceFileData()->getSystem() != mSystem)
			return false;

		auto it = mStates.find(Utils::FileSystem::getStem(game->getPath()));
		if (it != mStates.cend())
			return true;
	}

	return false;
}
std::vector<SaveState*> SaveStateRepository::getSaveStates(FileData* game)
{
	if (isEnabled(game) && game->getSourceFileData()->getSystem() == mSystem)
	{
		auto it = mStates.find(Utils::FileSystem::getStem(game->getPath()));
		if (it != mStates.cend())
			return it->second;
	}

	return std::vector<SaveState*>();
}

bool SaveStateRepository::isEnabled(FileData* game)
{
	auto emulatorName = game->getEmulator();
	if (emulatorName != "libretro" && emulatorName != "angle" && !Utils::String::startsWith(emulatorName, "lr-"))
		return false;

	if (!game->isFeatureSupported(EmulatorFeatures::autosave))
		return false;

	auto system = game->getSourceFileData()->getSystem();
	if (system->getSaveStateRepository()->getSavesPath().empty())
		return false;
	
	if (system->hasPlatformId(PlatformIds::IMAGEVIEWER))
		return false;

	return true;
}

int SaveStateRepository::getNextFreeSlot(FileData* game)
{
	if (!isEnabled(game))
		return -99;
	
	auto repo = game->getSourceFileData()->getSystem()->getSaveStateRepository();
	auto states = repo->getSaveStates(game);
	if (states.size() == 0)
		return 0;

	for (int i = 0; i < 99999; i++)
	{
		auto it = std::find_if(states.cbegin(), states.cend(), [i](const SaveState* x) { return x->slot == i; });
		if (it == states.cend())
			return i;
	}	

	return -99;
}


bool SaveStateRepository::copyToSlot(const SaveState& state, int slot)
{
	if (slot < 0)
		return false;
	
	if (!Utils::FileSystem::exists(state.fileName))
		return false;
	
	auto path = getSavesPath();

	std::string destState = Utils::FileSystem::combine(path, state.rom + ".state" + (slot == 0 ? "" : std::to_string(slot)));

	Utils::FileSystem::copyFile(state.fileName, destState);
	Utils::FileSystem::copyFile(state.getScreenShot(), destState + ".png");
	return true;
}

void SaveStateRepository::deleteSaveState(const SaveState& state) const
{
	if (!state.isSlotValid())
		return;

	if (!state.fileName.empty())
		Utils::FileSystem::removeFile(state.fileName);

	if (!state.getScreenShot().empty())
		Utils::FileSystem::removeFile(state.getScreenShot());
}
