#include "SaveStateRepository.h"
#include "SystemData.h"
#include "FileData.h"
#include "utils/StringUtil.h"
#include <regex>
#include "Log.h"

#include <time.h>
#include "Paths.h"

#include "SaveStateConfigFile.h"

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
	if (SaveStateConfigFile::isEnabled())
	{
		RegSaveState* rs = SaveStateConfigFile::getRegSaveState(mSystem);
		if (rs != NULL)
		{
			std::string path = Utils::String::replace(rs->directory, "{{system}}", mSystem->getName());

			if (!Utils::String::startsWith(path, "/"))
				path = Utils::FileSystem::combine(Paths::getSavesPath(), path);

			return path;
		}

		return "";
	}

	// default behavior
	return Utils::FileSystem::combine(Paths::getSavesPath(), mSystem->getName());	
}

bool SaveStateRepository::supportsAutoSave()
{
	if (SaveStateConfigFile::isEnabled())
	{
		RegSaveState* rs = SaveStateConfigFile::getRegSaveState(mSystem);
		if (rs == nullptr || !rs->autosave)
			return false;
	}

	return true;
}

bool SaveStateRepository::supportsIncrementalSaveStates()
{
	if (SaveStateConfigFile::isEnabled())
	{
		RegSaveState* rs = SaveStateConfigFile::getRegSaveState(mSystem);
		if (rs == nullptr || !rs->incremental)
			return false;
	}

	return true;
}


void SaveStateRepository::refresh()
{
	clear();

	auto path = getSavesPath();
	if (!Utils::FileSystem::exists(path))
		return;

	RegSaveState* rs = SaveStateConfigFile::getRegSaveState(mSystem);
	if (SaveStateConfigFile::isEnabled() && rs == nullptr)
		return;

	auto files = Utils::FileSystem::getDirectoryFiles(path);
	for (auto file : files)
	{		
		if (file.hidden || file.directory)
			continue;

		std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(file.path));

		if (ext == ".bak")
		{
			// auto ffstem = Utils::FileSystem::combine(Utils::FileSystem::getParent(file.path), Utils::FileSystem::getStem(file.path));
			// Utils::FileSystem::removeFile(ffstem);
			// Utils::FileSystem::renameFile(file.path, ffstem);
			// TODO RESTORE BAK FILE !? If board was turned off during a game ?
		}

		std::string rom;
		int slot = -1;

		if (SaveStateConfigFile::isEnabled())
		{
			std::string fileName = Utils::FileSystem::getFileName(file.path);
			if (!rs->matchSlotFile(fileName, rom, slot) && !rs->matchAutoFile(fileName, rom))
				continue;			
		}
		else 
		{
			// default behavior
			if (ext != ".auto" && !Utils::String::startsWith(ext, ".state"))
				continue;
		}

		SaveState* state = new SaveState();
		state->fileName = file.path;

		if (SaveStateConfigFile::isEnabled())
		{
			state->rom = rom;
			state->slot = slot;

			// generators are the same for autosave and slots
			state->fileGenerator  = Utils::String::replace(rs->file,  "{{romfilename}}", rom);
			state->imageGenerator = Utils::String::replace(rs->image, "{{romfilename}}", rom);

			// screenshot
			if (Utils::FileSystem::exists(state->fileName + ".png"))
				state->screenshot = state->fileName + ".png";
			else
			{				
				std::string screenshot = Utils::FileSystem::combine(getSavesPath(), slot < 0 ? rs->autosave_image : rs->image);
				std::string basename = Utils::FileSystem::getFileName(file.path);

				screenshot = Utils::String::replace(screenshot, "{{romfilename}}", rom);
				screenshot = Utils::String::replace(screenshot, "{{slot}}", state->slot == 0 ? "" : std::to_string(state->slot));
				screenshot = Utils::String::replace(screenshot, "{{slot0}}", std::to_string(state->slot));
				screenshot = Utils::String::replace(screenshot, "{{slot00}}", Utils::String::padLeft(std::to_string(state->slot), 2, '0'));
				screenshot = Utils::String::replace(screenshot, "{{slot2d}}", Utils::String::padLeft(std::to_string(state->slot), 2, '0'));

				if (Utils::FileSystem::exists(screenshot))
					state->screenshot = screenshot;
			}

			// retroarch specific commands
			state->racommands = false;
			state->hasAutosave = rs->autosave;
		}
		else
		{
			// default behavior
			if (ext == ".auto")
				state->slot = -1;
			else if (Utils::String::startsWith(ext, ".state"))
				state->slot = Utils::String::toInteger(ext.substr(6));

			auto stem = Utils::FileSystem::getStem(file.path);
			if (Utils::String::endsWith(stem, ".state"))
				stem = Utils::FileSystem::getStem(stem);

			state->rom = stem;

			// default behavior
			state->fileGenerator  = state->rom + ".state{{slot}}";
			state->imageGenerator = state->rom + ".state{{slot}}.png";

			// default behavior
			if (Utils::FileSystem::exists(state->fileName + ".png"))
				state->screenshot = state->fileName + ".png";

			// retroarch specific commands
			state->racommands = true;
			state->hasAutosave = true;
		}

#if WIN32
		state->creationDate.setTime(file.lastWriteTime);
#else
		state->creationDate = Utils::FileSystem::getFileModificationDate(state->fileName);
#endif
		mStates[state->rom].push_back(state);
	}
}

bool SaveStateRepository::hasSaveStates(FileData* game)
{
	if (mStates.size())
	{
		if (game->getSourceFileData()->getSystem() != mSystem)
			return false;

		if (SaveStateConfigFile::isEnabled())
		{
			RegSaveState* rs = SaveStateConfigFile::getRegSaveState(game->getEmulator(), game->getCore());
			if (rs == NULL) 
				return false;

			if (rs->nofileextension) 
			{
				auto it = mStates.find(Utils::FileSystem::getStem(game->getPath()));
				if (it != mStates.cend())
					return true;
			}
			else 
			{
				auto it = mStates.find(Utils::FileSystem::getFileName(game->getPath()));
				if (it != mStates.cend())
					return true;
			}
		}
		else 
		{
			// default behavior
			auto it = mStates.find(Utils::FileSystem::getStem(game->getPath()));
			if (it != mStates.cend())
				return true;
		}
	}

	return false;
}
std::vector<SaveState*> SaveStateRepository::getSaveStates(FileData* game)
{
	if (isEnabled(game) && game->getSourceFileData()->getSystem() == mSystem)
	{
		if (SaveStateConfigFile::isEnabled())
		{
			RegSaveState* rs = SaveStateConfigFile::getRegSaveState(game->getEmulator(), game->getCore());
			if (rs == NULL) 
				return std::vector<SaveState*>();

			if (rs->nofileextension) 
			{
				auto it = mStates.find(Utils::FileSystem::getStem(game->getPath()));
				if (it != mStates.cend())
					return it->second;
			}
			else 
			{
				auto it = mStates.find(Utils::FileSystem::getFileName(game->getPath()));
				if (it != mStates.cend())
					return it->second;
			}
		}
		else 
		{
			// default behavior
			auto it = mStates.find(Utils::FileSystem::getStem(game->getPath()));
			if (it != mStates.cend())
				return it->second;
		}
	}

	return std::vector<SaveState*>();
}

bool SaveStateRepository::isEnabled(FileData* game)
{
	auto emulatorName = game->getEmulator();
	auto coreName = game->getCore();

	if (SaveStateConfigFile::isEnabled())
	{
		RegSaveState* rs = SaveStateConfigFile::getRegSaveState(emulatorName, coreName);
		if (rs == nullptr)
			return false;
	}
	else 
	{
		// default behavior
		if (emulatorName != "libretro" && emulatorName != "angle" && !Utils::String::startsWith(emulatorName, "lr-"))
			return false;

		if (!game->isFeatureSupported(EmulatorFeatures::autosave))
			return false;
	}

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
	{
		if (SaveStateConfigFile::isEnabled())
		{
			RegSaveState* rs = SaveStateConfigFile::getRegSaveState(game->getEmulator(), game->getCore());
			if (rs != nullptr && rs->firstslot >= 0) 
				return rs->firstslot;
		}

		// default behavior
		return 0;		
	}

	for (int i = 99999; i >= 0; i--)
	{
		auto it = std::find_if(states.cbegin(), states.cend(), [i](const SaveState* x) { return x->slot == i; });
		if (it != states.cend())
			return i + 1;
	}

	return -99;
}

void SaveStateRepository::renumberSlots(FileData* game)
{
	if (!isEnabled(game))
		return;

	auto repo = game->getSourceFileData()->getSystem()->getSaveStateRepository();	
	repo->refresh();

	auto states = repo->getSaveStates(game);
	if (states.size() == 0)
		return;

	std::sort(states.begin(), states.end(), [](const SaveState* file1, const SaveState* file2) { return file1->slot < file2->slot; });

	int slot = 0;

	if (SaveStateConfigFile::isEnabled())
	{
		RegSaveState* rs = SaveStateConfigFile::getRegSaveState(game->getEmulator(), game->getCore());
		if (rs != nullptr && rs->firstslot >= 0)
			slot = rs->firstslot;
	}

	for (auto state : states)
	{
		if (state->slot < 0)
			continue;

		if (state->slot != slot)
			state->copyToSlot(slot, true);
		
		slot++;
	}	
}
