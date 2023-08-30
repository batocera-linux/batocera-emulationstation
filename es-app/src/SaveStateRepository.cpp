#include "SaveStateRepository.h"
#include "SystemData.h"
#include "FileData.h"
#include "utils/StringUtil.h"
#include <regex>

#include <time.h>
#include "Paths.h"

#if WIN32
#include "Win32ApiSystem.h"
#endif

bool SaveStateRepository::mRegSaveStatesLoaded = false;
std::map<std::string, RegSaveState*> SaveStateRepository::mRegSaveStates;

SaveStateRepository::SaveStateRepository(SystemData* system)
{
	mSystem = system;
#ifdef BATOCERA
	if(mRegSaveStatesLoaded == false) {
	  mRegSaveStatesLoaded = true;
	  RegSaveState* regsavestate = new RegSaveState();
	  regsavestate->directory 	   = "{{savedirectory}}/{{system}}";
	  regsavestate->file     	   = "{{romfilename}}.state{{slot}}";
	  regsavestate->romGroup      	   = 1;
	  regsavestate->slotGroup      	   = 2;
	  regsavestate->image      	   = "{{romfilename}}.state{{slot}}.png";
	  regsavestate->nofileextension    = true;
	  regsavestate->firstslot          = 0;
	  regsavestate->autosave           = true;
	  regsavestate->autosave_file      = "{{romfilename}}.state.auto";
	  regsavestate->autosave_image     = "{{romfilename}}.state.auto.png";
	  mRegSaveStates["libretro_snes9x"] = regsavestate;
	  mRegSaveStates["libretro_fceumm"] = regsavestate;

	  regsavestate = new RegSaveState();
	  regsavestate->directory 	   = "{{savedirectory}}/dolphin-emu/StateSaves";
	  regsavestate->file     	   = "{{romfilename}}.s{{slot2d}}";
	  regsavestate->romGroup      	   = 1;
	  regsavestate->slotGroup      	   = 2;
	  regsavestate->image    	   = "{{romfilename}}.s{{slot2d}}.png";
	  regsavestate->nofileextension    = false;
	  regsavestate->firstslot          = 1;
	  regsavestate->autosave           = false;
	  regsavestate->autosave_file      = "";
	  regsavestate->autosave_image     = "";
	  mRegSaveStates["dolphin_dolphin"] = regsavestate;

	  regsavestate = new RegSaveState();
	  regsavestate->directory 	   = "{{savedirectory}}/n64";
	  regsavestate->file     	   = "{{romfilename}}.st{{slot}}";
	  regsavestate->romGroup      	   = 1;
	  regsavestate->slotGroup      	   = 2;
	  regsavestate->image     	   = "{{romfilename}}.st{{slot}}.png";
	  regsavestate->nofileextension    = false;
	  regsavestate->firstslot          = 0;
	  regsavestate->autosave           = false;
	  regsavestate->autosave_file      = "";
	  regsavestate->autosave_image     = "";
	  mRegSaveStates["mupen64plus_glide64mk2"] = regsavestate;
	}
#endif
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

RegSaveState* SaveStateRepository::getRegSaveState(std::string emulator, std::string core) {
  if(mRegSaveStatesLoaded == false) return NULL;
  auto it = mRegSaveStates.find(emulator + "_" + core);
  if (it != mRegSaveStates.cend())
    return it->second;
  return NULL;
}

RegSaveState* SaveStateRepository::getRegSaveState() const {
  return getRegSaveState(mSystem->getEmulator(), mSystem->getCore());
}

std::string SaveStateRepository::getSavesPath()
{
  if(mRegSaveStatesLoaded) {
    RegSaveState* rs = getRegSaveState();
    if(rs != NULL) {
      std::string path = rs->directory;
      path = Utils::String::replace(path, "{{savedirectory}}", Paths::getSavesPath());
      path = Utils::String::replace(path, "{{system}}",        mSystem->getName());
      return path;
    }
    return "";
  } else {
    // default behavior
    return Utils::FileSystem::combine(Paths::getSavesPath(), mSystem->getName());
  }
}
	
void SaveStateRepository::refresh()
{
	RegSaveState* rs;
	std::regex re;
	std::cmatch matches;
	std::string rom;
	std::regex autore;
	std::cmatch automatches;
	clear();

	auto path = getSavesPath();
	if (!Utils::FileSystem::exists(path))
		return;

	auto files = Utils::FileSystem::getDirectoryFiles(path);

	if(mRegSaveStatesLoaded) {
	  rs = getRegSaveState();
	  std::string restr = rs->file;
	  restr = Utils::String::replace(restr, "\\", "\\\\");
	  restr = Utils::String::replace(restr, ".", "\\.");
	  restr = Utils::String::replace(restr, "{{romfilename}}", "(.*)");
	  restr = Utils::String::replace(restr, "{{slot}}", "([0-9]+)");
	  restr = Utils::String::replace(restr, "{{slot2d}}", "([0-9]+)");
	  restr = "^" + restr + "$";
	  re = std::regex(restr);
	  if(rs->autosave) {
	    std::string autorestr = rs->autosave_file;
	    autorestr = Utils::String::replace(autorestr, "\\", "\\\\");
	    autorestr = Utils::String::replace(autorestr, ".", "\\.");
	    autorestr = Utils::String::replace(autorestr, "{{romfilename}}", "(.*)");
	    autorestr = "^" + autorestr + "$";
	    autore = std::regex(autorestr);
	  }
	}
	
	for (auto file : files)
	{
		bool isAutoFile = false;
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

		if(mRegSaveStatesLoaded) {
		  if(rs == NULL) continue;
		  std::string basename = Utils::FileSystem::getFileName(file.path);
		  if (std::regex_match(basename.c_str(), matches, re)) {
		    if(matches.size()-1 >= rs->romGroup) {
		      rom = matches[rs->romGroup].str();
		    } else {
		      continue;
		    }
		  } else {
		    // check the auto file
		    if(rs->autosave) {
		      std::string basename = Utils::FileSystem::getFileName(file.path);
		      if (std::regex_match(basename.c_str(), automatches, autore)) {
			if(automatches.size()-1 == 1) {
			  rom = automatches[1].str();
			  isAutoFile = true;
			} else {
			  continue;
			}
		      } else {
			continue;
		      }
		    } else {
		      continue;
		    }
		  }

		} else {
		  // default behavior
		  if (ext != ".auto" && !Utils::String::startsWith(ext, ".state"))
		    continue;
		}

		SaveState* state = new SaveState();

		// slot
		if(mRegSaveStatesLoaded) {
		  if(isAutoFile) {
		    state->slot = -1;
		  } else {
		    if(matches.size()-1 >= rs->slotGroup) {
		      state->slot = Utils::String::toInteger(matches[rs->slotGroup].str().c_str());
		    } else {
		      delete state;
		      continue;
		    }
		  }
		} else {
		  // default behavior
		  if (ext == ".auto")
		    state->slot = -1;
		  else if (Utils::String::startsWith(ext, ".state"))
		    state->slot = Utils::String::toInteger(ext.substr(6));

		  auto stem = Utils::FileSystem::getStem(file.path);
		  if (Utils::String::endsWith(stem, ".state"))
		    rom = Utils::FileSystem::getStem(stem);
		}

		state->rom = rom;
		state->fileName = file.path;

		// generator
		if(mRegSaveStatesLoaded) {
		  // generators are the same for autosave and slots
		  state->fileGenerator = rs->file;
		  state->fileGenerator = Utils::String::replace(state->fileGenerator, "{{romfilename}}", rom);
		  state->imageGenerator = Utils::FileSystem::combine(getSavesPath(), rs->image);
		  state->imageGenerator = Utils::String::replace(state->imageGenerator, "{{romfilename}}", rom);
		} else {
		  // default behavior
		  state->fileGenerator  = Utils::FileSystem::combine(getSavesPath(), rom + ".state{{slot}}");
		  state->imageGenerator = Utils::FileSystem::combine(getSavesPath(), rom + ".state{{slot}}.png");
		}

		// screenshot
		if(mRegSaveStatesLoaded) {
		  std::string screenshot = Utils::FileSystem::combine(getSavesPath(), isAutoFile ? rs->autosave_image : rs->image);
		  std::string basename   = Utils::FileSystem::getFileName(file.path);

		  screenshot = Utils::String::replace(screenshot, "{{romfilename}}",       rom);
		  screenshot = Utils::String::replace(screenshot, "{{slot}}",              std::to_string(state->slot));
		  if(state->slot < 10) {
		    screenshot = Utils::String::replace(screenshot, "{{slot2d}}", "0" + std::to_string(state->slot));
		  } else {
		    screenshot = Utils::String::replace(screenshot, "{{slot2d}}", std::to_string(state->slot));
		  }

		  if (Utils::FileSystem::exists(screenshot))
		    state->screenshot = screenshot;
		} else {
		  // default behavior
		  if (Utils::FileSystem::exists(state->fileName + ".png"))
		    state->screenshot = state->fileName + ".png";
		}

		if(mRegSaveStatesLoaded) {
		  state->racommands = false;
		  state->hasAutosave = rs->autosave;
		} else {
		  // retroarch specific commands
		  state->racommands  = true;
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

		if(mRegSaveStatesLoaded) {
		  RegSaveState* rs = getRegSaveState(game->getEmulator(), game->getCore());
		  if(rs == NULL) return false;

		  if(rs->nofileextension) {
		    auto it = mStates.find(Utils::FileSystem::getStem(game->getPath()));
		    if (it != mStates.cend())
		      return true;
		  } else {
		    auto it = mStates.find(Utils::FileSystem::getFileName(game->getPath()));
		    if (it != mStates.cend())
		      return true;
		  }
		} else {
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
	  if(mRegSaveStatesLoaded) {
	    RegSaveState* rs = getRegSaveState(game->getEmulator(), game->getCore());
	    if(rs == NULL) return std::vector<SaveState*>();

	    if(rs->nofileextension) {
	      auto it = mStates.find(Utils::FileSystem::getStem(game->getPath()));
	      if (it != mStates.cend())
		return it->second;
	    } else {
	      auto it = mStates.find(Utils::FileSystem::getFileName(game->getPath()));
	      if (it != mStates.cend())
		return it->second;
	    }
	  } else {
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
	auto coreName     = game->getCore();

	if(mRegSaveStatesLoaded) {
	  RegSaveState* rs = getRegSaveState(emulatorName, coreName);
	  if(rs == NULL)
	    return false;
	} else {
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
	if (states.size() == 0) {
	  if(mRegSaveStatesLoaded) {
	    RegSaveState* rs = getRegSaveState(game->getEmulator(), game->getCore());
	    if(rs == NULL)
	      return 0;
	    if(rs->firstslot >= 0) return rs->firstslot;
	    return 0;
	  } else {
	    // default behavior
	    return 0;
	  }
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

	for (auto state : states)
	{
		if (state->slot < 0)
			continue;

		if (state->slot != slot)
			state->copyToSlot(slot, true);
		
		slot++;
	}	
}
