#include "SaveStateConfigFile.h"
#include "SystemData.h"
#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"
#include "Paths.h"
#include "Log.h"
#include <mutex>

static std::mutex mLock;
static SaveStateConfigFile* mInstance;

SaveStateConfigFile* SaveStateConfigFile::Instance()
{
	std::unique_lock<std::mutex> lock(mLock);

	if (mInstance == nullptr)
		mInstance = new SaveStateConfigFile();

	return mInstance;
}

bool SaveStateConfigFile::isEnabled()
{
	return Instance()->mRegSaveStatesMode;
}

RegSaveState* SaveStateConfigFile::getRegSaveState(const std::string& emulator, const std::string& core)
{			
	return Instance()->internalGetRegSaveState(emulator, core);
}

RegSaveState* SaveStateConfigFile::getRegSaveState(SystemData* system)
{
	return Instance()->internalGetRegSaveState(system->getEmulator(), system->getCore());
}

SaveStateConfigFile::SaveStateConfigFile()
{
	mRegSaveStatesMode = false;

	std::string path = Paths::getUserEmulationStationPath() + "/es_savestates.cfg";
	if (!Utils::FileSystem::exists(path))
		path = Paths::getEmulationStationPath() + "/es_savestates.cfg";
	if (!Utils::FileSystem::exists(path))
		return;

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(WINSTRINGW(path).c_str());

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
			bool        attr_enabled = true; // default to true
			std::string attr_directory = "";
			std::string attr_file = "";
			std::string attr_image = "";
			bool        attr_nofileextension = false;
			int         attr_firstslot = 0;
			bool        attr_autosave = false;
			std::string attr_autosave_file = "";
			std::string attr_autosave_image = "";

			auto attr = emulatorNode.attribute("enabled");
			if (attr)
				if (attr.value() == "false") attr_enabled = false;

			attr = emulatorNode.attribute("directory");
			if (attr)
				attr_directory = attr.value();

			attr = emulatorNode.attribute("file");
			if (attr)
				attr_file = attr.value();

			attr = emulatorNode.attribute("image");
			if (attr)
				attr_image = attr.value();

			attr = emulatorNode.attribute("nofileextension");
			if (attr) {
				if (strcmp(attr.value(), "true") == 0) attr_nofileextension = true;
			}

			attr = emulatorNode.attribute("firstslot");
			if (attr)
				attr_firstslot = Utils::String::toInteger(attr.value());

			attr = emulatorNode.attribute("autosave");
			if (attr)
				if (strcmp(attr.value(), "true") == 0) attr_autosave = true;

			attr = emulatorNode.attribute("autosave_file");
			if (attr)
				attr_autosave_file = attr.value();

			attr = emulatorNode.attribute("autosave_image");
			if (attr)
				attr_autosave_image = attr.value();

			RegSaveState* regsavestate = new RegSaveState();
			regsavestate->enabled = attr_enabled;
			regsavestate->directory = attr_directory;
			regsavestate->file = attr_file;
			regsavestate->romGroup = 1; // should be computed (but always 1)
			regsavestate->slotGroup = 2; // should be computed (but always 2)
			regsavestate->image = attr_image;
			regsavestate->nofileextension = attr_nofileextension;
			regsavestate->firstslot = attr_firstslot;
			regsavestate->autosave = attr_autosave;
			regsavestate->autosave_file = attr_autosave_file;
			regsavestate->autosave_image = attr_autosave_image;
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
					bool        attrcore_enabled = attr_enabled;
					std::string attrcore_directory = attr_directory;
					std::string attrcore_file = attr_file;
					std::string attrcore_image = attr_image;
					bool        attrcore_nofileextension = attr_nofileextension;
					int         attrcore_firstslot = attr_firstslot;
					bool        attrcore_autosave = attr_autosave;
					std::string attrcore_autosave_file = attr_autosave_file;
					std::string attrcore_autosave_image = attr_autosave_image;

					attr = coreNode.attribute("enabled");
					if (attr)
						if (attr.value() == "false") attrcore_enabled = false;

					attr = coreNode.attribute("directory");
					if (attr)
						attrcore_directory = attr.value();

					attr = coreNode.attribute("file");
					if (attr)
						attrcore_file = attr.value();

					attr = coreNode.attribute("image");
					if (attr)
						attrcore_image = attr.value();

					attr = coreNode.attribute("nofileextension");
					if (attr) {
						if (strcmp(attr.value(), "true") == 0) attrcore_nofileextension = true;
					}

					attr = coreNode.attribute("firstslot");
					if (attr)
						attrcore_firstslot = Utils::String::toInteger(attr.value());

					attr = coreNode.attribute("autosave");
					if (attr)
						if (strcmp(attr.value(), "true") == 0) attrcore_autosave = true;

					attr = coreNode.attribute("autosave_file");
					if (attr)
						attrcore_autosave_file = attr.value();

					attr = coreNode.attribute("autosave_image");
					if (attr)
						attrcore_autosave_image = attr.value();

					regsavestate = new RegSaveState();
					regsavestate->enabled = attrcore_enabled;
					regsavestate->directory = attrcore_directory;
					regsavestate->file = attrcore_file;
					regsavestate->romGroup = 1; // should be computed (but always 1)
					regsavestate->slotGroup = 2; // should be computed (but always 2)
					regsavestate->image = attrcore_image;
					regsavestate->nofileextension = attrcore_nofileextension;
					regsavestate->firstslot = attrcore_firstslot;
					regsavestate->autosave = attrcore_autosave;
					regsavestate->autosave_file = attrcore_autosave_file;
					regsavestate->autosave_image = attrcore_autosave_image;
					mRegSaveStates[emulatorname + "_" + corename] = regsavestate;
				}
			}
		}
	}

	for (const auto& kv : mRegSaveStates) 
	{
		RegSaveState* rs = kv.second;

		if (!rs->file.empty())
		{
			std::string restr = rs->file;
			restr = Utils::String::replace(restr, "\\", "\\\\");
			restr = Utils::String::replace(restr, ".", "\\.");
			restr = Utils::String::replace(restr, "{{romfilename}}", "(.*)");
			restr = Utils::String::replace(restr, "{{slot}}", "($|[0-9]+)");
			restr = Utils::String::replace(restr, "{{slot0}}", "([0-9]+)");
			restr = Utils::String::replace(restr, "{{slot00}}", "([0-9]+)");
			restr = Utils::String::replace(restr, "{{slot2d}}", "([0-9]+)");
			restr = "^" + restr + "$";

			rs->slotFileRegEx = std::regex(restr);
		}

		if (rs->autosave && !rs->autosave_file.empty())
		{
			std::string autorestr = rs->autosave_file;
			autorestr = Utils::String::replace(autorestr, "\\", "\\\\");
			autorestr = Utils::String::replace(autorestr, ".", "\\.");
			autorestr = Utils::String::replace(autorestr, "{{romfilename}}", "(.*)");
			autorestr = "^" + autorestr + "$";
			rs->autoFileRegEx = std::regex(autorestr);
		}
	}
}

RegSaveState* SaveStateConfigFile::internalGetRegSaveState(const std::string& emulator, const std::string& core)
{
	if (!mRegSaveStatesMode) 
		return nullptr;

	auto it = mRegSaveStates.find(emulator + "_" + core);
	if (it == mRegSaveStates.cend())
		it = mRegSaveStates.find(emulator);

	if (it != mRegSaveStates.cend() && it->second->enabled)
		return it->second;
			
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////

bool RegSaveState::matchSlotFile(const std::string& filename, std::string& romName, int& slot)
{
	if (file.empty())
		return false;

	std::cmatch matches;
	if (!std::regex_match(filename.c_str(), matches, slotFileRegEx))
		return false;

	if (matches.size() - 1 < romGroup)
		return false;

	romName = matches[romGroup];

	if (matches.size() - 1 < slotGroup)
		return false;
		
	slot = Utils::String::toInteger(matches[slotGroup]);
	
	return !romName.empty();
}

bool RegSaveState::matchAutoFile(const std::string& filename, std::string& romName)
{
	if (!autosave || autosave_file.empty())
		return false;

	std::cmatch matches;
	if (std::regex_match(filename.c_str(), matches, autoFileRegEx))
	{
		if (matches.size() - 1 < 1)
			return false;

		romName = matches[1];
		return !romName.empty();
	}

	return false;
}
