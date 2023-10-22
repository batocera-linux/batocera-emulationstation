#include "SaveStateConfigFile.h"
#include "SystemData.h"
#include "FileData.h"
#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"
#include "Paths.h"
#include "Log.h"
#include <mutex>

static std::mutex mLock;
static SaveStateConfigFile* mInstance;

static std::shared_ptr<SaveStateConfig> _default;

SaveStateConfig::SaveStateConfig()
{
	enabled = true;
	nofileextension = true;
	firstslot = 0;
	romGroup = 1; // should be computed (but always 1)
	slotGroup = 2; // should be computed (but always 2)
	incremental = false;
	autosave = false;
	racommands = false;
}

std::shared_ptr<SaveStateConfig> SaveStateConfig::Default()
{
	if (_default == nullptr)
	{
		_default = std::make_shared<SaveStateConfig>();
		_default->directory = "{{system}}";
		_default->file = "{{romfilename}}.state{{slot}}";
		_default->image = "{{romfilename}}.state{{slot}}.png";
		_default->nofileextension = true;
		_default->firstslot = 0;
		_default->romGroup = 1; // should be computed (but always 1)
		_default->slotGroup = 2; // should be computed (but always 2)
		_default->incremental = true;
		_default->autosave = true;
		_default->autosave_file = "{{romfilename}}.state.auto";			
		_default->autosave_image = "{{romfilename}}.state.auto.png";
		_default->racommands = true;

		_default->SetupRegEx();
	}

	return _default;
}

std::string SaveStateConfig::getDirectory(SystemData* system)
{
	std::string path = Utils::String::replace(directory, "{{system}}", system->getName());
	path = Utils::String::replace(path, "{{emulator}}", emulator);
	path = Utils::String::replace(path, "{{core}}", core.empty() ? emulator : core);

	if (!Utils::String::startsWith(path, "/"))
		path = Utils::FileSystem::combine(Paths::getSavesPath(), path);

	return path;
}

void SaveStateConfig::SetupRegEx()
{
	if (!this->file.empty())
	{
		std::string restr = this->file;
		restr = Utils::String::replace(restr, "\\", "\\\\");
		restr = Utils::String::replace(restr, ".", "\\.");
		restr = Utils::String::replace(restr, "{{romfilename}}", "(.*)");
		restr = Utils::String::replace(restr, "{{slot}}", "($|[0-9]+)");
		restr = Utils::String::replace(restr, "{{slot0}}", "([0-9]+)");
		restr = Utils::String::replace(restr, "{{slot00}}", "([0-9]+)");
		restr = Utils::String::replace(restr, "{{slot2d}}", "([0-9]+)");
		restr = "^" + restr + "$";

		this->slotFileRegEx = std::regex(restr);
	}

	if (autosave && !autosave_file.empty())
	{
		std::string autorestr = this->autosave_file;
		autorestr = Utils::String::replace(autorestr, "\\", "\\\\");
		autorestr = Utils::String::replace(autorestr, ".", "\\.");
		autorestr = Utils::String::replace(autorestr, "{{romfilename}}", "(.*)");
		autorestr = "^" + autorestr + "$";
		this->autoFileRegEx = std::regex(autorestr);
	}
}

bool SaveStateConfig::matchSlotFile(const std::string& filename, std::string& romName, int& slot)
{
	if (file.empty())
		return false;

	std::cmatch matches;
	if (!std::regex_match(filename.c_str(), matches, slotFileRegEx))
		return false;

	if (matches.size() - 1 < (size_t) romGroup)
		return false;

	romName = matches[romGroup];

	if (matches.size() - 1 < (size_t) slotGroup)
		return false;

	slot = Utils::String::toInteger(matches[slotGroup]);

	return !romName.empty();
}

bool SaveStateConfig::matchAutoFile(const std::string& filename, std::string& romName)
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

//////////////////////////////////////////////////////////////////////////////////////

SaveStateConfigFile* SaveStateConfigFile::Instance()
{
	std::unique_lock<std::mutex> lock(mLock);

	if (mInstance == nullptr)
		mInstance = new SaveStateConfigFile();

	return mInstance;
}


bool SaveStateConfigFile::isEnabled()
{
	return Instance()->mSaveStateConfigMode;
}

bool SaveStateConfig::isDefaultConfig(SystemData* system)
{
	auto defaultEmulator = system->getDefaultEmulator();
	auto defaultCore = system->getDefaultCore();

	if (core.empty())
		return emulator == defaultEmulator;

	return emulator == defaultEmulator && core == defaultCore;
}

bool SaveStateConfig::isActiveConfig(FileData* game)
{
	auto defaultEmulator = game->getEmulator();
	auto defaultCore = game->getCore();

	if (core.empty())
		return emulator == defaultEmulator;

	return emulator == defaultEmulator && core == defaultCore;
}

static std::string readXmlValue(const pugi::xml_node& node, const std::string& name, const std::string& defaultValue = "")
{
	auto attr = node.attribute(name.c_str());
	if (attr)
		return attr.value();

	auto child = node.child(name.c_str());
	if (child)
		return child.text().get();
		
	return defaultValue;
}

SaveStateConfigFile::SaveStateConfigFile()
{
	mSaveStateConfigMode = false;

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

	mSaveStateConfigMode = true;

	for (pugi::xml_node emulatorNode = savestates.first_child(); emulatorNode; emulatorNode = emulatorNode.next_sibling())
	{
		std::string emulatorNodeName = emulatorNode.name();
		if (emulatorNodeName == "emulator")
		{
			if (!emulatorNode.attribute("name"))
				continue;
			
			auto emul = std::make_shared<SaveStateConfig>();
			emul->emulator = readXmlValue(emulatorNode, "name");
			emul->enabled = readXmlValue(emulatorNode, "enabled") != "false";
			emul->directory = readXmlValue(emulatorNode, "directory", "{{system}}");
			emul->defaultCoreDirectory = readXmlValue(emulatorNode, "defaultCoreDirectory");			
			emul->file = readXmlValue(emulatorNode, "file");
			emul->romGroup = Utils::String::toInteger(readXmlValue(emulatorNode, "romGroup", "1"));
			emul->slotGroup = Utils::String::toInteger(readXmlValue(emulatorNode, "slotGroup", "2"));
			emul->image = readXmlValue(emulatorNode, "image");
			emul->nofileextension = readXmlValue(emulatorNode, "nofileextension", "true") == "true";
			emul->firstslot = Utils::String::toInteger(readXmlValue(emulatorNode, "firstslot", "0"));
			emul->autosave = readXmlValue(emulatorNode, "autosave", "false") == "true";
			emul->autosave_file = readXmlValue(emulatorNode, "autosave_file");
			emul->autosave_image = readXmlValue(emulatorNode, "autosave_image");
			emul->incremental = readXmlValue(emulatorNode, "incremental") == "true";
			emul->racommands = false;

			mSaveStateConfigs[emul->emulator] = emul;
			
			for (pugi::xml_node coreNode = emulatorNode.child("core"); coreNode; coreNode = coreNode.next_sibling("core"))
			{
				SaveStateCoreConfig core;
				core.name = readXmlValue(coreNode, "name");
				if (core.name.empty())
					continue;

				core.enabled = readXmlValue(coreNode, "enabled", "true") != "false";
				core.system = readXmlValue(coreNode, "system");				
				core.directory = readXmlValue(coreNode, "directory", "");
				if (core.enabled && core.directory.empty())
					continue;

				emul->coreConfigs.push_back(core);
			}
		}
	}

	for (const auto& kv : mSaveStateConfigs)
		kv.second->SetupRegEx();
}

std::shared_ptr<SaveStateConfig> SaveStateConfigFile::getSaveStateConfig(const std::string& emulator)
{
	auto it = mSaveStateConfigs.find(emulator);
	if (it != mSaveStateConfigs.cend() && it->second->enabled)
		return it->second;
			
	return nullptr;
}

static std::map<std::string, std::vector<std::shared_ptr<SaveStateConfig>>> _cache;

std::vector<std::shared_ptr<SaveStateConfig>> SaveStateConfigFile::getSaveStateConfigs(SystemData* system)
{
	auto it = _cache.find(system->getName());
	if (it != _cache.cend())
		return it->second;
	
	std::vector<std::shared_ptr<SaveStateConfig>> ret;

	if (!isEnabled())
	{
		ret.push_back(SaveStateConfig::Default());
		_cache.insert(std::pair<std::string, std::vector<std::shared_ptr<SaveStateConfig>>>(system->getName(), ret));
		return ret;
	}

	auto systemName = system->getName();
	auto defaultEmulator = system->getDefaultEmulator();
	auto defaultCore = system->getDefaultCore();

	for (const EmulatorData& emul : system->getEmulators())
	{
		auto pInfo = Instance()->getSaveStateConfig(emul.name);
		if (pInfo == nullptr || !pInfo->enabled)
			continue;

		bool splittedCores = pInfo->directory.find("{{core}}") != std::string::npos;

		if (emul.cores.size() == 0 || !splittedCores)
		{
			if (!pInfo->defaultCoreDirectory.empty())
			{
				auto copy = std::make_shared<SaveStateConfig>(*pInfo);
				copy->directory = pInfo->defaultCoreDirectory;
				ret.push_back(copy);
			}
			else
				ret.push_back(pInfo);

			continue;
		}

		for (const CoreData& core : emul.cores)
		{
			auto coreConf = std::find_if(pInfo->coreConfigs.cbegin(), pInfo->coreConfigs.cend(), [core, systemName](const SaveStateCoreConfig& x) { return x.name == core.name && x.system == systemName; });
			if (coreConf == pInfo->coreConfigs.cend())
				coreConf = std::find_if(pInfo->coreConfigs.cbegin(), pInfo->coreConfigs.cend(), [core, systemName](const SaveStateCoreConfig& x) { return x.name == core.name; });
			
			if (coreConf != pInfo->coreConfigs.cend() && !coreConf->enabled)
				continue;
			
			auto copy = std::make_shared<SaveStateConfig>(*pInfo);
			copy->core = core.name;

			if (coreConf != pInfo->coreConfigs.cend() && !coreConf->directory.empty())
				copy->directory = coreConf->directory;
			else if (!pInfo->defaultCoreDirectory.empty() && emul.name == defaultEmulator && core.name == defaultCore)
				copy->directory = pInfo->defaultCoreDirectory;

			ret.push_back(copy);
		}
	}

	_cache.insert(std::pair<std::string, std::vector<std::shared_ptr<SaveStateConfig>>>(system->getName(), ret));
	return ret;
}