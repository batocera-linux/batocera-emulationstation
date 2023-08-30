#include "SaveStateRepository.h"
#include "SystemData.h"
#include "FileData.h"
#include "utils/StringUtil.h"
#include <regex>
#include "Log.h"

#include <time.h>
#include "Paths.h"

#if WIN32
#include "Win32ApiSystem.h"
#endif

bool SaveStateRepository::mRegSaveStatesLoaded = false;
bool SaveStateRepository::mRegSaveStatesMode   = false;
std::map<std::string, RegSaveState*> SaveStateRepository::mRegSaveStates;

SaveStateRepository::SaveStateRepository(SystemData* system)
{
	mSystem = system;
	refresh();
}

SaveStateRepository::~SaveStateRepository()
{
	clear();
}

void SaveStateRepository::loadSaveStateConfig()
{
	if(mRegSaveStatesLoaded)
	  return;
	mRegSaveStatesLoaded = true;
  
  	std::string path = Paths::getUserEmulationStationPath() + "/es_savestates.cfg";
	if (!Utils::FileSystem::exists(path))
		path = Paths::getEmulationStationPath() + "/es_savestates.cfg";
	if (!Utils::FileSystem::exists(path))
		return;

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(path.c_str());

	if (!res)
	{
		LOG(LogError) << "Could not parse es_savestates.cfg file!";
		LOG(LogError) << res.description();
		return;
	}

	pugi::xml_node savestates = doc.child("savestates");
	if (!savestates)
	{
		LOG(LogError) << "es_savestates.cfg is missing the <savestates> tag!";
		return;
	}

	mRegSaveStatesMode = true;

	for (pugi::xml_node emulatorNode = savestates.first_child(); emulatorNode; emulatorNode = emulatorNode.next_sibling())
	{
	  std::string emulatornodename = emulatorNode.name();
	  if (emulatornodename == "emulator")
	    {
	      if (!emulatorNode.attribute("name"))
		continue;

	      std::string emulatorname = emulatorNode.attribute("name").value();
	      bool        attr_enabled         = true; // default to true
	      std::string attr_directory       = "";
	      std::string attr_file  	       = "";
	      std::string attr_image 	       = "";
	      bool        attr_nofileextension = false;
	      int         attr_firstslot       = 0;
	      bool        attr_autosave        = false;
	      std::string attr_autosave_file   = "";
	      std::string attr_autosave_image  = "";

	      auto attr = emulatorNode.attribute("enabled");
	      if(attr)
		if(attr.value() == "false") attr_enabled = false;

	      attr = emulatorNode.attribute("directory");
	      if(attr)
		attr_directory = attr.value();

	      attr = emulatorNode.attribute("file");
	      if(attr)
		attr_file = attr.value();

	      attr = emulatorNode.attribute("image");
	      if(attr)
		attr_image = attr.value();

	      attr = emulatorNode.attribute("nofileextension");
	      if(attr) {
		if(strcmp(attr.value(), "true") == 0) attr_nofileextension = true;
	      }

	      attr = emulatorNode.attribute("firstslot");
	      if(attr)
		attr_firstslot = Utils::String::toInteger(attr.value());

	      attr = emulatorNode.attribute("autosave");
	      if(attr)
		if(strcmp(attr.value(), "true") == 0) attr_autosave = true;

	      attr = emulatorNode.attribute("autosave_file");
	      if(attr)
		attr_autosave_file = attr.value();

	      attr = emulatorNode.attribute("autosave_image");
	      if(attr)
		attr_autosave_image = attr.value();

	      RegSaveState* regsavestate = new RegSaveState();
	      regsavestate->enabled         = attr_enabled;
	      regsavestate->directory 	    = attr_directory;
	      regsavestate->file     	    = attr_file;
	      regsavestate->romGroup        = 1; // should be computed (but always 1)
	      regsavestate->slotGroup       = 2; // should be computed (but always 2)
	      regsavestate->image      	    = attr_image;
	      regsavestate->nofileextension = attr_nofileextension;
	      regsavestate->firstslot       = attr_firstslot;
	      regsavestate->autosave        = attr_autosave;
	      regsavestate->autosave_file   = attr_autosave_file;
	      regsavestate->autosave_image  = attr_autosave_image;
	      mRegSaveStates[emulatorname] = regsavestate;

	      for (pugi::xml_node coreNode = emulatorNode.first_child(); coreNode; coreNode = coreNode.next_sibling())
		{
		  std::string corenodename = coreNode.name();
		  if (corenodename == "core")
		    {
		      if (!emulatorNode.attribute("name"))
			continue;

		      //
		      std::string corename = coreNode.attribute("name").value();
		      bool        attrcore_enabled         = attr_enabled;
		      std::string attrcore_directory       = attr_directory;
		      std::string attrcore_file  	   = attr_file;
		      std::string attrcore_image 	   = attr_image;
		      bool        attrcore_nofileextension = attr_nofileextension;
		      int         attrcore_firstslot       = attr_firstslot;
		      bool        attrcore_autosave        = attr_autosave;
		      std::string attrcore_autosave_file   = attr_autosave_file;
		      std::string attrcore_autosave_image  = attr_autosave_image;

		      attr = coreNode.attribute("enabled");
		      if(attr)
			if(attr.value() == "false") attrcore_enabled = false;
		      
		      attr = coreNode.attribute("directory");
		      if(attr)
			attrcore_directory = attr.value();

		      attr = coreNode.attribute("file");
		      if(attr)
			attrcore_file = attr.value();

		      attr = coreNode.attribute("image");
		      if(attr)
			attrcore_image = attr.value();

		      attr = coreNode.attribute("nofileextension");
		      if(attr) {
			if(strcmp(attr.value(), "true") == 0) attrcore_nofileextension = true;
		      }

		      attr = coreNode.attribute("firstslot");
		      if(attr)
			attrcore_firstslot = Utils::String::toInteger(attr.value());

		      attr = coreNode.attribute("autosave");
		      if(attr)
			if(strcmp(attr.value(), "true") == 0) attrcore_autosave = true;

		      attr = coreNode.attribute("autosave_file");
		      if(attr)
			attrcore_autosave_file = attr.value();

		      attr = coreNode.attribute("autosave_image");
		      if(attr)
			attrcore_autosave_image = attr.value();

		      regsavestate = new RegSaveState();
		      regsavestate->enabled         = attrcore_enabled;
		      regsavestate->directory 	    = attrcore_directory;
		      regsavestate->file     	    = attrcore_file;
		      regsavestate->romGroup        = 1; // should be computed (but always 1)
		      regsavestate->slotGroup       = 2; // should be computed (but always 2)
		      regsavestate->image      	    = attrcore_image;
		      regsavestate->nofileextension = attrcore_nofileextension;
		      regsavestate->firstslot       = attrcore_firstslot;
		      regsavestate->autosave        = attrcore_autosave;
		      regsavestate->autosave_file   = attrcore_autosave_file;
		      regsavestate->autosave_image  = attrcore_autosave_image;
		      mRegSaveStates[emulatorname + "_" + corename] = regsavestate;
		    }
		}
	    }
	}
}

void SaveStateRepository::clear()
{
	for (auto item : mStates)
		for (auto state : item.second)
			delete state;

	mStates.clear();
}

RegSaveState* SaveStateRepository::getRegSaveState(std::string emulator, std::string core) {
  if(mRegSaveStatesMode == false) return NULL;

  // search emulator_core
  auto it = mRegSaveStates.find(emulator + "_" + core);
  if (it != mRegSaveStates.cend()) {
    if(it->second->enabled == false) return NULL;
    return it->second;
  }

  // search emulator only
  auto it2 = mRegSaveStates.find(emulator);
  if (it2 != mRegSaveStates.cend()) {
    if(it2->second->enabled == false) return NULL;
    return it2->second;
  }

  return NULL;
}

RegSaveState* SaveStateRepository::getRegSaveState() const {
  return getRegSaveState(mSystem->getEmulator(), mSystem->getCore());
}

std::string SaveStateRepository::getSavesPath()
{
  if(mRegSaveStatesMode) {
    RegSaveState* rs = getRegSaveState();
    if(rs != NULL) {
      std::string path = rs->directory;
      if(Utils::String::startsWith(path, "/") == false)
	path = Utils::FileSystem::combine(Paths::getSavesPath(), path);
      path = Utils::String::replace(path, "{{system}}", mSystem->getName());
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

	if(mRegSaveStatesMode) {
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

		if(mRegSaveStatesMode) {
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
		if(mRegSaveStatesMode) {
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
		if(mRegSaveStatesMode) {
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
		if(mRegSaveStatesMode) {
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

		if(mRegSaveStatesMode) {
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

		if(mRegSaveStatesMode) {
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
	  if(mRegSaveStatesMode) {
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
	SaveStateRepository::loadSaveStateConfig(); // probably a better place to put it...
  
	auto emulatorName = game->getEmulator();
	auto coreName     = game->getCore();

	if(mRegSaveStatesMode) {
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
	  if(mRegSaveStatesMode) {
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
