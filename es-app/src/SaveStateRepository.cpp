#include "SaveStateRepository.h"
#include "SystemData.h"
#include "FileData.h"
#include "utils/StringUtil.h"
#include <regex>
#include "Log.h"

#include <time.h>
#include "Paths.h"
#include "utils/VectorEx.h"
#include "SaveStateConfigFile.h"

#if WIN32
#include "Win32ApiSystem.h"
#endif

SaveState* SaveStateRepository::_empty = new SaveState(-99);

SaveStateRepository::SaveStateRepository(SystemData* system)
{
	_newGame = nullptr;
	_autosave = nullptr;

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

bool SaveStateRepository::supportsAutoSave()
{
	for (auto rs : SaveStateConfigFile::getSaveStateConfigs(mSystem))
		if (rs->autosave)
			return true;

	return false;
}

bool SaveStateRepository::supportsIncrementalSaveStates()
{
	for (auto rs : SaveStateConfigFile::getSaveStateConfigs(mSystem))
		if (rs->incremental)
			return true;

	return false;
}

void SaveStateRepository::refresh()
{
	clear();

	auto list = SaveStateConfigFile::getSaveStateConfigs(mSystem);
	for (auto rs : list)
	{
		std::string path = rs->getDirectory(mSystem);

		if (!Utils::FileSystem::exists(path))
			continue;

		auto files = Utils::FileSystem::getDirectoryFiles(path);
		for (auto file : files)
		{
			if (file.hidden || file.directory)
				continue;

			std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(file.path));

			std::string rom;
			int slot = -1;

			std::string fileName = Utils::FileSystem::getFileName(file.path);
			if (!rs->matchSlotFile(fileName, rom, slot) && !rs->matchAutoFile(fileName, rom))
				continue;

			SaveState* state = new SaveState();
			state->config = rs;
			state->fileName = file.path;
			state->rom = rom;
			state->slot = slot;

			// generators are the same for autosave and slots
			state->fileGenerator = Utils::String::replace(rs->file, "{{romfilename}}", rom);
			state->imageGenerator = Utils::String::replace(rs->image, "{{romfilename}}", rom);

			// screenshot
			if (Utils::FileSystem::exists(state->fileName + ".png"))
				state->screenshot = state->fileName + ".png";
			else
			{
				std::string screenshot = Utils::FileSystem::combine(path, slot < 0 ? rs->autosave_image : rs->image);
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
			state->racommands = rs->racommands;
			state->hasAutosave = rs->autosave;

#if WIN32
			state->creationDate.setTime(file.lastWriteTime);
#else
			state->creationDate = Utils::FileSystem::getFileModificationDate(state->fileName);
#endif
			mStates[state->rom].push_back(state);
		}
	}
}

bool SaveStateRepository::hasSaveStates(FileData* game)
{
	if (mStates.size())
	{
		if (game->getSourceFileData()->getSystem() != mSystem)
			return false;

		auto name = Utils::FileSystem::getFileName(game->getPath());

		auto it = mStates.find(name);
		if (it != mStates.cend())
			return true;

		for (auto rs : SaveStateConfigFile::getSaveStateConfigs(mSystem))
		{
			if (!rs->nofileextension)
				continue;

			std::string name = Utils::FileSystem::getStem(game->getPath());

			auto it = mStates.find(name);
			if (it != mStates.cend())
				return true;

			// Don't need to test again
			return false;
		}
	}

	return false;
}
std::vector<SaveState*> SaveStateRepository::getSaveStates(FileData* game, std::shared_ptr<SaveStateConfig> config)
{
	if (isEnabled(game) && game->getSourceFileData()->getSystem() == mSystem)
	{
		std::vector<SaveState*> ret;

		for (auto rs : SaveStateConfigFile::getSaveStateConfigs(mSystem))
		{
			if (config != nullptr && !config->equals(rs))
				continue;

			std::string name = rs->nofileextension ? Utils::FileSystem::getStem(game->getPath()) : Utils::FileSystem::getFileName(game->getPath());

			auto it = mStates.find(name);
			if (it != mStates.cend())
			{
				for (auto item : it->second)
					if (rs->equals(item->config))
						ret.push_back(item);
			}
		}

		return ret;
	}

	return std::vector<SaveState*>();
}

bool SaveStateRepository::isEnabled(FileData* game)
{
	auto system = game->getSourceFileData()->getSystem();

	if (system->hasPlatformId(PlatformIds::IMAGEVIEWER))
		return false;

	// Should we really keep this flag ?
	// if (!game->isFeatureSupported(EmulatorFeatures::autosave))
	//	 return false;

	return SaveStateConfigFile::getSaveStateConfigs(system).size() != 0;
}

int SaveStateRepository::getNextFreeSlot(FileData* game, std::shared_ptr<SaveStateConfig> config)
{
	if (!isEnabled(game))
		return -99;

	auto repo = game->getSourceFileData()->getSystem()->getSaveStateRepository();	
	auto states = repo->getSaveStates(game, config);

	if (states.size() == 0)
		return config != nullptr ? config->firstslot : 0;

	for (int i = 99999; i >= 0; i--)
	{
		auto it = std::find_if(states.cbegin(), states.cend(), [i](const SaveState* x) { return x->slot == i; });
		if (it != states.cend())
			return i + 1;
	}

	return -99;
}

void SaveStateRepository::renumberSlots(FileData* game, std::shared_ptr<SaveStateConfig> config)
{
	if (!isEnabled(game))
		return;

	auto repo = game->getSourceFileData()->getSystem()->getSaveStateRepository();	
	repo->refresh();

	auto states = repo->getSaveStates(game, config);
	if (states.size() == 0)
		return;

	std::sort(states.begin(), states.end(), [](const SaveState* file1, const SaveState* file2) { return file1->slot < file2->slot; });

	int slot = config == nullptr ? 0 : config->firstslot;

	for (auto state : states)
	{
		if (state->slot < 0)
			continue;

		if (state->slot != slot)
			state->copyToSlot(slot, true);
		
		slot++;
	}	
}


SaveState* SaveStateRepository::getDefaultAutoSaveSaveState()
{
	if (_autosave == nullptr)
		_autosave = new SaveState(-1);

	return _autosave;
}

SaveState* SaveStateRepository::getDefaultNewGameSaveState()
{
	if (_newGame == nullptr)
		_newGame = new SaveState(-2);
	
	return _newGame;
}

SaveState* SaveStateRepository::getEmptySaveState()
{
	return _empty;
}

SaveState* SaveStateRepository::getGameAutoSave(FileData* game)
{
	std::shared_ptr<SaveStateConfig> current;

	auto emul = game->getEmulator();
	auto core = game->getCore();

	auto confs = SaveStateConfigFile::getSaveStateConfigs(game->getSourceFileData()->getSystem());
	for (auto conf : confs)
	{
		if (conf->emulator == emul && conf->core == core)
		{
			current = conf;
			break;
		}
	}

	if (!current)
	{
		for (auto conf : confs)
		{
			if (conf->emulator == emul)
			{
				current = conf;
				break;
			}
		}
	}

	if (!current && confs.size())
		current = confs[0];

	if (current)
	{
		auto states = getSaveStates(game, current);
		auto it = std::find_if(states.cbegin(), states.cend(), [](const SaveState* x) { return x->slot == -1; });
		if (it != states.cend())
			return *it;
	}

	return getDefaultAutoSaveSaveState();
}