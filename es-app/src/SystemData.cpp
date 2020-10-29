#include "SystemData.h"

#include "SystemConf.h"
#include "utils/FileSystemUtil.h"
#include "utils/ThreadPool.h"
#include "CollectionSystemManager.h"
#include "FileFilterIndex.h"
#include "FileSorts.h"
#include "Gamelist.h"
#include "Log.h"
#include "platform.h"
#include "Settings.h"
#include "ThemeData.h"
#include "views/UIModeController.h"
#include <fstream>
#include "Window.h"
#include "LocaleES.h"
#include "utils/StringUtil.h"
#include "views/ViewController.h"
#include "ThreadedHasher.h"
#include <unordered_set>
#include <algorithm>

using namespace Utils;

std::vector<SystemData*> SystemData::sSystemVector;

SystemData::SystemData(const SystemMetadata& meta, SystemEnvironmentData* envData, std::vector<EmulatorData>* pEmulators, bool CollectionSystem, bool groupedSystem, bool withTheme) : // batocera
	mMetadata(meta), mEnvData(envData), mIsCollectionSystem(CollectionSystem), mIsGameSystem(true)
{
	mIsGroupSystem = groupedSystem;
	mGameListHash = 0;
	mGameCount = -1;
	mSortId = Settings::getInstance()->getInt(getName() + ".sort");
	mGridSizeOverride = Vector2f(0, 0);

	mFilterIndex = nullptr;

	if (pEmulators != nullptr)
		mEmulators = *pEmulators; // batocera

	// if it's an actual system, initialize it, if not, just create the data structure
	if (!CollectionSystem && !mIsGroupSystem)
	{
		mRootFolder = new FolderData(mEnvData->mStartPath, this);
		mRootFolder->getMetadata().set("name", mMetadata.fullName);

		std::unordered_map<std::string, FileData*> fileMap;
		fileMap[mEnvData->mStartPath] = mRootFolder;

		if (!Settings::getInstance()->getBool("ParseGamelistOnly"))
		{
			populateFolder(mRootFolder, fileMap);
			if (mRootFolder->getChildren().size() == 0)
				return;
		}

		if(!Settings::getInstance()->getBool("IgnoreGamelist") && mMetadata.name != "imageviewer")
			parseGamelist(this, fileMap);
	}
	else
	{
		// virtual systems are updated afterwards, we're just creating the data structure
		mRootFolder = new FolderData("" + mMetadata.name, this);
	}

	mRootFolder->getMetadata().resetChangedFlag();
	
	auto defaultView = Settings::getInstance()->getString(getName() + ".defaultView");
	auto gridSizeOverride = Vector2f::parseString(Settings::getInstance()->getString(getName() + ".gridSize"));
	setSystemViewMode(defaultView, gridSizeOverride, false);

	setIsGameSystemStatus();

	if (withTheme)
		loadTheme();
}

SystemData::~SystemData()
{
	delete mRootFolder;

	if (mFilterIndex != nullptr)
		delete mFilterIndex;
}

void SystemData::setIsGameSystemStatus()
{
	// we exclude non-game systems from specific operations (i.e. the "RetroPie" system, at least)
	// if/when there are more in the future, maybe this can be a more complex method, with a proper list
	// but for now a simple string comparison is more performant
	mIsGameSystem = (mMetadata.name != "retropie");
}

void SystemData::populateFolder(FolderData* folder, std::unordered_map<std::string, FileData*>& fileMap)
{
	const std::string& folderPath = folder->getPath();

	/*
	if(!Utils::FileSystem::isDirectory(folderPath))
	{
		LOG(LogWarning) << "Error - folder with path \"" << folderPath << "\" is not a directory!";
		return;
	}
	
	// [Obsolete] make sure that this isn't a symlink to a thing we already have
	// Deactivated because it's slow & useless : users should to be carefull not to make recursive simlinks
	if (Utils::FileSystem::isSymlink(folderPath))
	{
		//if this symlink resolves to somewhere that's at the beginning of our path, it's gonna recurse
		if(folderPath.find(Utils::FileSystem::getCanonicalPath(folderPath)) == 0)
		{
			LOG(LogWarning) << "Skipping infinitely recursive symlink \"" << folderPath << "\"";
			return;
		}
	}
	*/
	std::string filePath;
	std::string extension;
	bool isGame;
	bool showHidden = Settings::getInstance()->getBool("ShowHiddenFiles");

	auto shv = Settings::getInstance()->getString(getName() + ".ShowHiddenFiles");
	if (shv == "1") showHidden = true;
	else if (shv == "0") showHidden = false;

	Utils::FileSystem::fileList dirContent = Utils::FileSystem::getDirectoryFiles(folderPath);
	for (auto fileInfo : dirContent)
	{
		filePath = fileInfo.path;

		// skip hidden files and folders
		if(!showHidden && fileInfo.hidden)
			continue;

		//this is a little complicated because we allow a list of extensions to be defined (delimited with a space)
		//we first get the extension of the file itself:
		extension = Utils::String::toLower(Utils::FileSystem::getExtension(filePath));

		//fyi, folders *can* also match the extension and be added as games - this is mostly just to support higan
		//see issue #75: https://github.com/Aloshi/EmulationStation/issues/75

		isGame = false;
		if(mEnvData->isValidExtension(extension))
		{
			FileData* newGame = new FileData(GAME, filePath, this);

			// preventing new arcade assets to be added
			if(!newGame->isArcadeAsset())
			{
				folder->addChild(newGame);
				fileMap[filePath] = newGame;
				isGame = true;
			}
		}

		//add directories that also do not match an extension as folders
		if(!isGame && fileInfo.directory)
		{
			std::string fn = Utils::String::toLower(Utils::FileSystem::getFileName(filePath));

			// Don't loose time looking in downloaded_images, downloaded_videos & media folders
			if (fn == "media" || fn == "medias" || fn == "images" || fn == "manuals" || fn == "videos" || fn == "assets" || Utils::String::startsWith(fn, "downloaded_") || Utils::String::startsWith(fn, "."))
				continue;

			// Hardcoded optimisation : WiiU has so many files in content & meta directories
			if (mMetadata.name == "wiiu" && (fn == "content" || fn == "meta"))
				continue;

			FolderData* newFolder = new FolderData(filePath, this);
			populateFolder(newFolder, fileMap);

			//ignore folders that do not contain games
			if(newFolder->getChildren().size() == 0)
				delete newFolder;
			else 
			{
				const std::string& key = newFolder->getPath();
				if (fileMap.find(key) == fileMap.end())
				{
					folder->addChild(newFolder);
					fileMap[key] = newFolder;
				}
			}
		}
	}
}

FileFilterIndex* SystemData::getIndex(bool createIndex)
{
	if (mFilterIndex == nullptr && createIndex)
	{
		mFilterIndex = new FileFilterIndex();
		indexAllGameFilters(mRootFolder);
		mFilterIndex->setUIModeFilters();
	}

	return mFilterIndex;
}

void SystemData::deleteIndex()
{
	if (mFilterIndex != nullptr)
	{
		delete mFilterIndex;
		mFilterIndex = nullptr;
	}
}

void SystemData::indexAllGameFilters(const FolderData* folder)
{
	const std::vector<FileData*>& children = folder->getChildren();

	for(auto it = children.cbegin(); it != children.cend(); ++it)
	{
		switch((*it)->getType())
		{
			case GAME:   { mFilterIndex->addToIndex(*it); } break;
			case FOLDER: { indexAllGameFilters((FolderData*)*it); } break;
		}
	}
}

std::vector<std::string> readList(const std::string& str, const char* delims = " \t\r\n,")
{
	std::vector<std::string> ret;

	size_t prevOff = str.find_first_not_of(delims, 0);
	size_t off = str.find_first_of(delims, prevOff);
	while(off != std::string::npos || prevOff != std::string::npos)
	{
		ret.push_back(str.substr(prevOff, off - prevOff));

		prevOff = str.find_first_not_of(delims, off);
		off = str.find_first_of(delims, prevOff);
	}

	return ret;
}

void SystemData::createGroupedSystems()
{
	std::map<std::string, std::vector<SystemData*>> map;

	for (auto sys : sSystemVector)
	{
		if (sys->isCollection() || sys->getSystemEnvData()->mGroup.empty())
			continue;
		
		if (Settings::getInstance()->getBool(sys->getSystemEnvData()->mGroup + ".ungroup"))
			continue;

		if (sys->getName() == sys->getSystemEnvData()->mGroup)
		{
			sys->getSystemEnvData()->mGroup = "";
			continue;
		}

		map[sys->getSystemEnvData()->mGroup].push_back(sys);		
	}

	for (auto item : map)
	{	
		SystemData* system = nullptr;
		bool existingSystem = false;

		for (auto sys : sSystemVector)
		{
			if (sys->getName() == item.first)
			{
				existingSystem = true;
				system = sys;
				system->mIsGroupSystem = true;
				break;
			}
		}

		if (system == nullptr)
		{
			SystemEnvironmentData* envData = new SystemEnvironmentData;
			envData->mStartPath = "";
			envData->mLaunchCommand = "";

			SystemMetadata md;
			md.name = item.first;
			md.fullName = item.first;
			md.themeFolder = item.first;
			
			if (item.second.size() > 0)
			{
				SystemData* syss = *item.second.cbegin();
				md.manufacturer = syss->getSystemMetadata().manufacturer;
				md.releaseYear = syss->getSystemMetadata().releaseYear;
				md.hardwareType = "system";
			}

			system = new SystemData(md, envData, nullptr, false, true);
			system->mIsGroupSystem = true;
			system->mIsGameSystem = false;
		}

		FolderData* root = system->getRootFolder();
		
		for (auto childSystem : item.second)
		{			
			auto children = childSystem->getRootFolder()->getChildren();
			if (children.size() > 0)
			{
				auto folder = new FolderData(childSystem->getRootFolder()->getPath(), childSystem, false);
				folder->setMetadata(childSystem->getRootFolder()->getMetadata());
				root->addChild(folder);

				if (folder->getMetadata(MetaDataId::Image).empty())
				{
					auto theme = childSystem->getTheme();
					if (theme)
					{
						const ThemeData::ThemeElement* logoElem = theme->getElement("system", "logo", "image");
						if (logoElem && logoElem->has("path"))
						{
							std::string path = logoElem->get<std::string>("path");
							folder->setMetadata("image", path);
							folder->setMetadata("thumbnail", path);
							folder->enableVirtualFolderDisplay(true);
						}
					}
				}

				for (auto child : children)
					folder->addChild(child, false);

				folder->getMetadata().resetChangedFlag();
			}
		}

		if (root->getChildren().size() > 0 && !existingSystem)
		{
			system->loadTheme();
			sSystemVector.push_back(system);
		}
		
		root->getMetadata().resetChangedFlag();
	}
}

inline EmulatorFeatures::Features operator|(EmulatorFeatures::Features a, EmulatorFeatures::Features b)
{
	return static_cast<EmulatorFeatures::Features>(static_cast<int>(a) | static_cast<int>(b));
}

inline EmulatorFeatures::Features operator&(EmulatorFeatures::Features a, EmulatorFeatures::Features b)
{
	return static_cast<EmulatorFeatures::Features>(static_cast<int>(a) & static_cast<int>(b));
}

EmulatorFeatures::Features EmulatorFeatures::parseFeatures(const std::string features)
{
	EmulatorFeatures::Features ret = EmulatorFeatures::Features::none;

	for (auto name : Utils::String::split(features, ','))
	{
		std::string trim = Utils::String::trim(name);

		if (trim == "ratio") ret = ret | EmulatorFeatures::Features::ratio;
		if (trim == "rewind") ret = ret | EmulatorFeatures::Features::rewind;
		if (trim == "smooth") ret = ret | EmulatorFeatures::Features::smooth;
		if (trim == "shaders") ret = ret | EmulatorFeatures::Features::shaders;
		if (trim == "pixel_perfect") ret = ret | EmulatorFeatures::Features::pixel_perfect;
		if (trim == "decoration") ret = ret | EmulatorFeatures::Features::decoration;
		if (trim == "latency_reduction") ret = ret | EmulatorFeatures::Features::latency_reduction;
		if (trim == "game_translation") ret = ret | EmulatorFeatures::Features::game_translation;
		if (trim == "autosave") ret = ret | EmulatorFeatures::Features::autosave;
		if (trim == "netplay") ret = ret | EmulatorFeatures::Features::netplay;
		if (trim == "fullboot") ret = ret | EmulatorFeatures::Features::fullboot;
		if (trim == "emulated_wiimotes") ret = ret | EmulatorFeatures::Features::emulated_wiimotes;
		if (trim == "screen_layout") ret = ret | EmulatorFeatures::Features::screen_layout;
		if (trim == "internal_resolution") ret = ret | EmulatorFeatures::Features::internal_resolution;
		if (trim == "videomode") ret = ret | EmulatorFeatures::Features::videomode;
		if (trim == "colorization") ret = ret | EmulatorFeatures::Features::colorization;		
		if (trim == "vertical") ret = ret | EmulatorFeatures::Features::vertical;		
	}

	return ret;
}

bool SystemData::es_features_loaded = false;

std::vector<CustomFeature>  SystemData::loadCustomFeatures(pugi::xml_node node)
{
	std::vector<CustomFeature> ret;

	pugi::xml_node customFeatures = node.child("features");
	if (customFeatures == nullptr)
		customFeatures = node;

	for (pugi::xml_node featureNode = customFeatures.child("feature"); featureNode; featureNode = featureNode.next_sibling("feature"))
	{
		if (!featureNode.attribute("name"))
			continue;

		CustomFeature feat;
		feat.name = featureNode.attribute("name").value();

		if (featureNode.attribute("value"))
			feat.value = featureNode.attribute("value").value();
		else 
			feat.value = Utils::String::replace(feat.name, " ", "_");

		for (pugi::xml_node value = featureNode.child("choice"); value; value = value.next_sibling("choice"))
		{
			if (!value.attribute("name"))
				continue;

			CustomFeatureChoice cv;
			cv.name = value.attribute("name").value();

			if (value.attribute("value"))
				cv.value = value.attribute("value").value();
			else
				cv.value = Utils::String::replace(cv.name, " ", "_");

			feat.choices.push_back(cv);
		}

		if (feat.choices.size() > 0)
			ret.push_back(feat);
	}

	return ret;
}

bool SystemData::loadFeatures()
{
	std::string path = Utils::FileSystem::getEsConfigPath() + "/es_features.cfg";
	if (!Utils::FileSystem::exists(path))
		path = Utils::FileSystem::getSharedConfigPath() + "/es_features.cfg";

	if (!Utils::FileSystem::exists(path))
		return false;

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(path.c_str());

	if (!res)
	{
		LOG(LogError) << "Could not parse es_features.cfg file!";
		LOG(LogError) << res.description();
		return false;
	}

	//actually read the file
	pugi::xml_node systemList = doc.child("features");
	if (!systemList)
	{
		LOG(LogError) << "es_features.cfg is missing the <features> tag!";
		return false;
	}

	es_features_loaded = true;

	for (auto sys : SystemData::sSystemVector)
	{
		for (auto& emul : sys->mEmulators)
		{
			emul.features = EmulatorFeatures::Features::none;
			for (auto& core : emul.cores)
				core.features = EmulatorFeatures::Features::none;
		}
	}

	for (pugi::xml_node emulator = systemList.child("emulator"); emulator; emulator = emulator.next_sibling("emulator"))
	{		
		if (!emulator.attribute("name"))
			continue;

		std::string emulatorNames = emulator.attribute("name").value();
		for (auto tmpName : Utils::String::split(emulatorNames, ','))
		{
			std::string emulatorName = Utils::String::trim(tmpName);
		
			EmulatorFeatures::Features emulatorFeatures = EmulatorFeatures::Features::none;

			auto customEmulatorFeatures = loadCustomFeatures(emulator);

			if (emulator.attribute("features") || customEmulatorFeatures.size() > 0)
			{
				if (emulator.attribute("features"))
					emulatorFeatures = EmulatorFeatures::parseFeatures(emulator.attribute("features").value());

				for (auto sys : SystemData::sSystemVector)
				{
					for (auto& emul : sys->mEmulators)
					{
						if (emul.name == emulatorName || (emulatorName == "libretro" && Utils::String::startsWith(emul.name, "lr-")))
						{
							emul.features = emul.features | emulatorFeatures;
							for(auto feat : customEmulatorFeatures)
								emul.customFeatures.push_back(feat);
						}
					}
				}
			}

			pugi::xml_node coresNode = emulator.child("cores");
			if (coresNode == nullptr)
				coresNode = emulator;

			for (pugi::xml_node coreNode = coresNode.child("core"); coreNode; coreNode = coreNode.next_sibling("core"))
			{
				if (!coreNode.attribute("name") || !coreNode.attribute("features"))
					continue;

				std::string coreNames = coreNode.attribute("name").value();
				for (auto tmpCoreName : Utils::String::split(coreNames, ','))
				{
					std::string coreName = Utils::String::trim(tmpCoreName);

					EmulatorFeatures::Features coreFeatures = EmulatorFeatures::parseFeatures(coreNode.attribute("features").value());

					auto customCoreFeatures = loadCustomFeatures(coreNode);

					bool coreFound = false;

					for (auto sys : SystemData::sSystemVector)
					{
						if (sys->mEmulators.size() == 0)
						{
							// Try to handle systems without emulators defined
							std::string command = sys->getLaunchCommand(emulatorName, coreName);
							if (command.find(" " + coreName + " ") != std::string::npos || command.find(coreName + "_libretro") != std::string::npos)
							{
								EmulatorData emul;
								emul.name = emulatorName;
								emul.features = emulatorFeatures;

								for (auto feat : customEmulatorFeatures)
									emul.customFeatures.push_back(feat);							

								CoreData core;
								core.name = coreName;
								core.features = coreFeatures;
								core.customFeatures = customCoreFeatures;
								emul.cores.push_back(core);

								sys->mEmulators.push_back(emul);
							}
						}
						else
						{
							for (auto& emul : sys->mEmulators)
							{
								if (emul.name == emulatorName || (emulatorName == "libretro" && Utils::String::startsWith(emul.name, "lr-")))
								{
									for (auto& core : emul.cores)
									{
										if (core.name == coreName)
										{
											coreFound = true;
											core.features = core.features | coreFeatures;

											for (auto feat : customCoreFeatures)
												core.customFeatures.push_back(feat);

											core.netplay = coreFeatures & EmulatorFeatures::Features::netplay;
										}
									}
								}
							}
						}
					}

					pugi::xml_node systemsCoresNode = coreNode.child("systems");
					if (systemsCoresNode == nullptr)
						systemsCoresNode = coreNode;

					for (pugi::xml_node systemNode = systemsCoresNode.child("system"); systemNode; systemNode = systemNode.next_sibling("system"))
					{
						if (!systemNode.attribute("name") || !systemNode.attribute("features"))
							continue;

						std::string systemName = systemNode.attribute("name").value();
						EmulatorFeatures::Features systemFeatures = EmulatorFeatures::parseFeatures(systemNode.attribute("features").value());
						auto customSystemFeatures = loadCustomFeatures(systemNode);

						for (auto sys : SystemData::sSystemVector)
							if (sys->getName() == systemName)
								for (auto& emul : sys->mEmulators)
									for (auto& core : emul.cores)
										if (core.name == coreName) {
											core.features = core.features | systemFeatures;
											for (auto ft : customSystemFeatures) core.customFeatures.push_back(ft);
										}
					}
				}
			}

			if (emulatorFeatures != EmulatorFeatures::Features::none || customEmulatorFeatures.size() > 0)
			{
				for (auto sys : SystemData::sSystemVector)
				{
					if (sys->mEmulators.size() == 0 && sys->getName() == emulatorName)
					{
						EmulatorData emul;
						emul.name = emulatorName;
						emul.features = emulatorFeatures;

						for (auto feat : customEmulatorFeatures)
							emul.customFeatures.push_back(feat);

						sys->mEmulators.push_back(emul);
					}
				}
			}
		}
		
		pugi::xml_node systemsNode = emulator.child("systems");
		if (systemsNode == nullptr)
			systemsNode = emulator;
		
		for (pugi::xml_node systemNode = systemsNode.child("system"); systemNode; systemNode = systemNode.next_sibling("system"))
		{
			if (!systemNode.attribute("name") || !systemNode.attribute("features"))
				continue;

			std::string systemName = systemNode.attribute("name").value();
			EmulatorFeatures::Features systemFeatures = EmulatorFeatures::parseFeatures(systemNode.attribute("features").value());

			auto customSystemFeatures = loadCustomFeatures(systemNode);

			for (auto sys : SystemData::sSystemVector)
				if (sys->getName() == systemName)
					for (auto& emul : sys->mEmulators) {
						emul.features = emul.features | systemFeatures;
						for (auto ft : customSystemFeatures)							
							emul.customFeatures.push_back(ft);
					}
		}		
	}

	return true;
}

bool SystemData::isCurrentFeatureSupported(EmulatorFeatures::Features feature)
{
	std::string emul = getDefaultEmulator();
	std::string core = getDefaultCore(emul);

	return isFeatureSupported(emul, core, feature);
}

bool SystemData::hasFeatures()
{
	if (isGroupSystem() || isCollection() || hasPlatformId(PlatformIds::PLATFORM_IGNORE))
		return false;

	for (auto emulator : mEmulators)
	{
		for (auto& core : emulator.cores)
			if (core.features != EmulatorFeatures::Features::none || core.customFeatures.size() > 0)
				return true;

		if (emulator.features != EmulatorFeatures::Features::none || emulator.customFeatures.size() > 0)
			return true;
	}

	return !es_features_loaded;
}

std::vector<CustomFeature> SystemData::getCustomFeatures(std::string emulatorName, std::string coreName)
{
	std::vector<CustomFeature> ret;

	if (emulatorName.empty() || emulatorName == "auto")
		emulatorName = getDefaultEmulator();

	if (coreName.empty() || coreName == "auto")
		coreName = getDefaultCore(emulatorName);

	for (auto emulator : mEmulators)
	{
		if (emulator.name == emulatorName)
		{
			for (auto ft : emulator.customFeatures)
				ret.push_back(ft);

			for (auto& core : emulator.cores)
				if (coreName == core.name)
					for (auto ft : core.customFeatures)
						ret.push_back(ft);

			break;
		}
	}

	return ret;
}


bool SystemData::isFeatureSupported(std::string emulatorName, std::string coreName, EmulatorFeatures::Features feature)
{
	if (emulatorName.empty() || emulatorName == "auto")
		emulatorName = getDefaultEmulator();

	if (coreName.empty() || coreName == "auto")
		coreName = getDefaultCore(emulatorName);

	for (auto emulator : mEmulators)
	{
		if (emulator.name == emulatorName)
		{
			for (auto& core : emulator.cores)
				if (coreName == core.name)
					if ((core.features & feature) == feature)
						return true;
			
			return (emulator.features & feature) == feature;
		}
	}

	return !es_features_loaded;
}

//creates systems from information located in a config file
bool SystemData::loadConfig(Window* window)
{
	deleteSystems();
	ThemeData::setDefaultTheme(nullptr);

	std::string path = getConfigPath(false);

	LOG(LogInfo) << "Loading system config file " << path << "...";

	if (!Utils::FileSystem::exists(path))
	{
		LOG(LogError) << "es_systems.cfg file does not exist!";
		writeExampleConfig(getConfigPath(true));
		return false;
	}

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(path.c_str());

	if (!res)
	{
		LOG(LogError) << "Could not parse es_systems.cfg file!";
		LOG(LogError) << res.description();
		return false;
	}

	//actually read the file
	pugi::xml_node systemList = doc.child("systemList");
	if (!systemList)
	{
		LOG(LogError) << "es_systems.cfg is missing the <systemList> tag!";
		return false;
	}

	std::vector<std::string> systemsNames;

	int systemCount = 0;
	for (pugi::xml_node system = systemList.child("system"); system; system = system.next_sibling("system"))
	{
		systemsNames.push_back(system.child("fullname").text().get());
		systemCount++;
	}

	if (systemCount == 0)
	{
		LOG(LogError) << "no system found in es_systems.cfg";
		return false;
	}

	Utils::FileSystem::FileSystemCacheActivator fsc;

	int currentSystem = 0;

	typedef SystemData* SystemDataPtr;

	ThreadPool* pThreadPool = NULL;
	SystemDataPtr* systems = NULL;

	// Allow threaded loading only if processor threads > 2 so it does not apply on machines like Pi0.
	if (std::thread::hardware_concurrency() > 2 && Settings::getInstance()->getBool("ThreadedLoading"))
	{
		pThreadPool = new ThreadPool();

		systems = new SystemDataPtr[systemCount];
		for (int i = 0; i < systemCount; i++)
			systems[i] = nullptr;

		pThreadPool->queueWorkItem([] { CollectionSystemManager::get()->loadCollectionSystems(true); });
	}

	int processedSystem = 0;

	for (pugi::xml_node system = systemList.child("system"); system; system = system.next_sibling("system"))
	{
		if (pThreadPool != NULL)
		{
			pThreadPool->queueWorkItem([system, currentSystem, systems, &processedSystem]
			{
				systems[currentSystem] = loadSystem(system);
				processedSystem++;
			});
		}
		else
		{
			std::string fullname = system.child("fullname").text().get();

			if (window != NULL)
				window->renderSplashScreen(fullname, systemCount == 0 ? 0 : (float)currentSystem / (float)(systemCount + 1));

			std::string nm = system.child("name").text().get();

			SystemData* pSystem = loadSystem(system);
			if (pSystem != nullptr)
				sSystemVector.push_back(pSystem);
		}

		currentSystem++;
	}

	if (pThreadPool != NULL)
	{
		if (window != NULL)
		{
			pThreadPool->wait([window, &processedSystem, systemCount, &systemsNames]
			{
				int px = processedSystem - 1;
				if (px >= 0 && px < systemsNames.size())
					window->renderSplashScreen(systemsNames.at(px), (float)px / (float)(systemCount + 1));
			}, 50);
		}
		else
			pThreadPool->wait();

		for (int i = 0; i < systemCount; i++)
		{
			SystemData* pSystem = systems[i];
			if (pSystem != nullptr)
				sSystemVector.push_back(pSystem);
		}

		delete[] systems;
		delete pThreadPool;

		

		if (window != NULL)
			window->renderSplashScreen(_("Collections"), systemCount == 0 ? 0 : currentSystem / systemCount);

		// updateSystemsList can't be run async, systems have to be created before
		createGroupedSystems();
		CollectionSystemManager::get()->updateSystemsList();
	}
	else
	{
		if (window != NULL)
			window->renderSplashScreen(_("Collections"), systemCount == 0 ? 0 : currentSystem / systemCount);

		createGroupedSystems();
		CollectionSystemManager::get()->loadCollectionSystems();
	}

	if (SystemData::sSystemVector.size() > 0)
	{
		auto theme = SystemData::sSystemVector.at(0)->getTheme();
		ViewController::get()->onThemeChanged(theme);		
	}

	loadFeatures();

	if (window != nullptr && SystemConf::getInstance()->getBool("global.netplay") && !ThreadedHasher::isRunning())
	{
		if (Settings::getInstance()->getBool("NetPlayCheckIndexesAtStart"))
			ThreadedHasher::start(window, false, true);
	}
	
	return true;
}

SystemData* SystemData::loadSystem(std::string systemName, bool fullMode)
{
	std::string path = getConfigPath(false);
	if (!Utils::FileSystem::exists(path))
		return nullptr;

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(path.c_str());
	if (!res)
		return nullptr;

	//actually read the file
	pugi::xml_node systemList = doc.child("systemList");
	if (!systemList)
		return nullptr;

	for (pugi::xml_node system = systemList.child("system"); system; system = system.next_sibling("system"))
	{
		std::string name = system.child("name").text().get();
		if (name == systemName)
			return loadSystem(system, fullMode);
	}

	return nullptr;
}

SystemData* SystemData::loadSystem(pugi::xml_node system, bool fullMode)
{
	std::string path, cmd; // , name, fullname, themeFolder;

	path = system.child("path").text().get();

	SystemMetadata md;
	md.name = system.child("name").text().get();
	md.fullName = system.child("fullname").text().get();
	md.manufacturer = system.child("manufacturer").text().get();
	md.releaseYear = atoi(system.child("release").text().get());
	md.hardwareType = system.child("hardware").text().get();
	md.themeFolder = system.child("theme").text().as_string(md.name.c_str());

	// convert extensions list from a string into a vector of strings
	std::vector<std::string> extensions;
	for (auto ext : readList(system.child("extension").text().get()))
	{
		std::string extlow = Utils::String::toLower(ext);
		if (std::find(extensions.cbegin(), extensions.cend(), extlow) == extensions.cend())
			extensions.push_back(extlow);
	}

	cmd = system.child("command").text().get();

	// platform id list
	const char* platformList = system.child("platform").text().get();
	std::vector<std::string> platformStrs = readList(platformList);
	std::vector<PlatformIds::PlatformId> platformIds;
	for (auto it = platformStrs.cbegin(); it != platformStrs.cend(); it++)
	{
		const char* str = it->c_str();
		PlatformIds::PlatformId platformId = PlatformIds::getPlatformId(str);

		if (platformId == PlatformIds::PLATFORM_IGNORE)
		{
			// when platform is ignore, do not allow other platforms
			platformIds.clear();
			platformIds.push_back(platformId);
			break;
		}

		// if there appears to be an actual platform ID supplied but it didn't match the list, warn
		if (platformId != PlatformIds::PLATFORM_UNKNOWN)
			platformIds.push_back(platformId);
		else if (str != NULL && str[0] != '\0' && platformId == PlatformIds::PLATFORM_UNKNOWN)
			LOG(LogWarning) << "  Unknown platform for system \"" << md.name << "\" (platform \"" << str << "\" from list \"" << platformList << "\")";
	}


	//validate
	if (md.name.empty() || path.empty() || extensions.empty() || cmd.empty())
	{
		LOG(LogError) << "System \"" << md.name << "\" is missing name, path, extension, or command!";
		return nullptr;
	}

	//convert path to generic directory seperators
	path = Utils::FileSystem::getGenericPath(path);

	//expand home symbol if the startpath contains ~
	if (path[0] == '~')
	{
		path.erase(0, 1);
		path.insert(0, Utils::FileSystem::getHomePath());
		path = Utils::FileSystem::getCanonicalPath(path);
	}

	//create the system runtime environment data
	SystemEnvironmentData* envData = new SystemEnvironmentData;
	envData->mStartPath = path;
	envData->mSearchExtensions = extensions;
	envData->mLaunchCommand = cmd;
	envData->mPlatformIds = platformIds;
	envData->mGroup = system.child("group").text().get();
	
	// batocera
// emulators and cores

	std::vector<EmulatorData> systemEmulators;
	
	pugi::xml_node emulatorsNode = system.child("emulators");
	if (emulatorsNode != nullptr)
	{
		for (pugi::xml_node emuNode = emulatorsNode.child("emulator"); emuNode; emuNode = emuNode.next_sibling("emulator"))
		{
			EmulatorData emulatorData;
			emulatorData.name = emuNode.attribute("name").value();
			emulatorData.customCommandLine = emuNode.attribute("command").value();
			emulatorData.features = EmulatorFeatures::Features::all;

			pugi::xml_node coresNode = emuNode.child("cores");
			if (coresNode != nullptr)
			{
				for (pugi::xml_node coreNode = coresNode.child("core"); coreNode; coreNode = coreNode.next_sibling("core"))
				{
					CoreData core;
					core.name = coreNode.text().as_string();
					core.netplay = coreNode.attribute("netplay") && strcmp(coreNode.attribute("netplay").value(), "true") == 0;
					core.isDefault = coreNode.attribute("default") && strcmp(coreNode.attribute("default").value(), "true") == 0;
					core.features = EmulatorFeatures::Features::all;
					core.customCommandLine = coreNode.attribute("command").value();

					emulatorData.cores.push_back(core);
				}
			}

			systemEmulators.push_back(emulatorData);
		}
	}

	SystemData* newSys = new SystemData(md, envData, &systemEmulators, false, false, fullMode); // batocera

	if (!fullMode)
		return newSys;
	
	if (newSys->getRootFolder()->getChildren().size() == 0)
	{
		LOG(LogWarning) << "System \"" << md.name << "\" has no games! Ignoring it.";
		delete newSys;
		return nullptr;
	}	

	return newSys;
}

void SystemData::writeExampleConfig(const std::string& path)
{
	std::ofstream file(path.c_str());

	file << "<!-- This is the EmulationStation Systems configuration file.\n"
			"All systems must be contained within the <systemList> tag.-->\n"
			"\n"
			"<systemList>\n"
			"	<!-- Here's an example system to get you started. -->\n"
			"	<system>\n"
			"\n"
			"		<!-- A short name, used internally. Traditionally lower-case. -->\n"
			"		<name>nes</name>\n"
			"\n"
			"		<!-- A \"pretty\" name, displayed in menus and such. -->\n"
			"		<fullname>Nintendo Entertainment System</fullname>\n"
			"\n"
			"		<!-- The path to start searching for ROMs in. '~' will be expanded to $HOME on Linux or %HOMEPATH% on Windows. -->\n"
			"		<path>~/roms/nes</path>\n"
			"\n"
			"		<!-- A list of extensions to search for, delimited by any of the whitespace characters (\", \\r\\n\\t\").\n"
			"		You MUST include the period at the start of the extension! It's also case sensitive. -->\n"
			"		<extension>.nes .NES</extension>\n"
			"\n"
			"		<!-- The shell command executed when a game is selected. A few special tags are replaced if found in a command:\n"
			"		%ROM% is replaced by a bash-special-character-escaped absolute path to the ROM.\n"
			"		%BASENAME% is replaced by the \"base\" name of the ROM.  For example, \"/foo/bar.rom\" would have a basename of \"bar\". Useful for MAME.\n"
			"		%ROM_RAW% is the raw, unescaped path to the ROM. -->\n"
			"		<command>retroarch -L ~/cores/libretro-fceumm.so %ROM%</command>\n"
			"\n"
			"		<!-- The platform to use when scraping. You can see the full list of accepted platforms in src/PlatformIds.cpp.\n"
			"		It's case sensitive, but everything is lowercase. This tag is optional.\n"
			"		You can use multiple platforms too, delimited with any of the whitespace characters (\", \\r\\n\\t\"), eg: \"genesis, megadrive\" -->\n"
			"		<platform>nes</platform>\n"
			"\n"
			"		<!-- The theme to load from the current theme set.  See THEMES.md for more information.\n"
			"		This tag is optional. If not set, it will default to the value of <name>. -->\n"
			"		<theme>nes</theme>\n"
			"	</system>\n"
			"</systemList>\n";

	file.close();

	LOG(LogError) << "Example config written!  Go read it at \"" << path << "\"!";
}

bool SystemData::isManufacturerSupported()
{
	for (auto sys : sSystemVector)
	{
		if (!sys->isGameSystem() || sys->isGroupSystem() || sys->isCollection())
			continue;

		if (!sys->getSystemMetadata().manufacturer.empty())
			return true;
	}

	return false;
}

bool SystemData::hasDirtySystems()
{
	bool saveOnExit = !Settings::getInstance()->getBool("IgnoreGamelist") && Settings::getInstance()->getBool("SaveGamelistsOnExit");
	if (!saveOnExit)
		return false;

	for (unsigned int i = 0; i < sSystemVector.size(); i++)
	{
		SystemData* pData = sSystemVector.at(i);
		if (pData->mIsCollectionSystem)
			continue;
		
		if (hasDirtyFile(pData))
			return true;
	}

	return false;
}

void SystemData::deleteSystems()
{
	bool saveOnExit = !Settings::getInstance()->getBool("IgnoreGamelist") && Settings::getInstance()->getBool("SaveGamelistsOnExit");

	for (unsigned int i = 0; i < sSystemVector.size(); i++)
	{
		SystemData* pData = sSystemVector.at(i);
		pData->getRootFolder()->removeVirtualFolders();

		if (saveOnExit && !pData->mIsCollectionSystem)
			updateGamelist(pData);

		delete pData;
	}

	sSystemVector.clear();
}

std::string SystemData::getConfigPath(bool forWrite)
{
#if WIN32
	std::string customPath = Utils::FileSystem::getSharedConfigPath() + "/es_systems_custom.cfg"; // /usr/share/emulationstation/es_systems.cfg
	if (Utils::FileSystem::exists(customPath))
		return customPath;
#endif

	std::string userdataPath = Utils::FileSystem::getEsConfigPath() + "/es_systems.cfg"; // /userdata/system/configs/emulationstation/es_systems.cfg
	if(forWrite || Utils::FileSystem::exists(userdataPath))
		return userdataPath;

#if !WIN32
	std::string customPath = Utils::FileSystem::getSharedConfigPath() + "/es_systems.cfg"; // /usr/share/emulationstation/es_systems.cfg
	if (Utils::FileSystem::exists(customPath))
		return customPath;
#endif

	return "/etc/emulationstation/es_systems.cfg";
}

bool SystemData::isVisible()
{
	if (isGroupChildSystem())
		return false;

	if ((getDisplayedGameCount() > 0 ||
		(UIModeController::getInstance()->isUIModeFull() && mIsCollectionSystem) ||
		(mIsCollectionSystem && mMetadata.name == "favorites")))
	{
		if (!mIsCollectionSystem)
		{
			auto hiddenSystems = Utils::String::split(Settings::getInstance()->getString("HiddenSystems"), ';');
			return std::find(hiddenSystems.cbegin(), hiddenSystems.cend(), getName()) == hiddenSystems.cend();
		}

		return true;
	}

	return false;
}

SystemData* SystemData::getNext() const
{
	std::vector<SystemData*>::const_iterator it = getIterator();

	do {
		it++;
		if (it == sSystemVector.cend())
			it = sSystemVector.cbegin();
	} while (!(*it)->isVisible());
	// as we are starting in a valid gamelistview, this will always succeed, even if we have to come full circle.

	return *it;
}

SystemData* SystemData::getPrev() const
{
	std::vector<SystemData*>::const_reverse_iterator it = getRevIterator();

	do {
		it++;
		if (it == sSystemVector.crend())
			it = sSystemVector.crbegin();
	} while (!(*it)->isVisible());
	// as we are starting in a valid gamelistview, this will always succeed, even if we have to come full circle.

	return *it;
}

std::string SystemData::getGamelistPath(bool forWrite) const
{
	std::string filePath;

	filePath = mRootFolder->getPath() + "/gamelist.xml";
	if(Utils::FileSystem::exists(filePath))
		return filePath;

	std::string localPath = Utils::FileSystem::getEsConfigPath() + "/gamelists/" + mMetadata.name + "/gamelist.xml";
	if (Utils::FileSystem::exists(localPath))
		return localPath;

	if (forWrite)
		Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(filePath));
	
	return filePath;
}

std::string SystemData::getThemePath() const
{
	// where we check for themes, in order:
	// 1. [SYSTEM_PATH]/theme.xml
	// 2. system theme from currently selected theme set [CURRENT_THEME_PATH]/[SYSTEM]/theme.xml
	// 3. default system theme from currently selected theme set [CURRENT_THEME_PATH]/theme.xml

	// first, check game folder
	
	if (!mEnvData->mStartPath.empty())
	{
		std::string rootThemePath = mRootFolder->getPath() + "/theme.xml";
		if (Utils::FileSystem::exists(rootThemePath))
			return rootThemePath;
	}

	// not in game folder, try system theme in theme sets
	std::string localThemePath = ThemeData::getThemeFromCurrentSet(mMetadata.themeFolder);
	if (Utils::FileSystem::exists(localThemePath))
		return localThemePath;

	// not system theme, try default system theme in theme set
	localThemePath = Utils::FileSystem::getParent(Utils::FileSystem::getParent(localThemePath)) + "/theme.xml";

	return localThemePath;
}

bool SystemData::hasGamelist() const
{
	return (Utils::FileSystem::exists(getGamelistPath(false)));
}

unsigned int SystemData::getGameCount() const
{
	return (unsigned int)mRootFolder->getFilesRecursive(GAME).size();
}

SystemData* SystemData::getRandomSystem()
{
	//  this is a bit brute force. It might be more efficient to just to a while (!gameSystem) do random again...
	unsigned int total = 0;
	for(auto it = sSystemVector.cbegin(); it != sSystemVector.cend(); it++)
	{
		if ((*it)->isGameSystem())
			total ++;
	}

	// get random number in range
	int target = (int)Math::round((std::rand() / (float)RAND_MAX) * (total - 1));
	for (auto it = sSystemVector.cbegin(); it != sSystemVector.cend(); it++)
	{
		if ((*it)->isGameSystem())
		{
			if (target > 0)
			{
				target--;
			}
			else
			{
				return (*it);
			}
		}
	}

	// if we end up here, there is no valid system
	return NULL;
}

FileData* SystemData::getRandomGame()
{
	std::vector<FileData*> list = mRootFolder->getFilesRecursive(GAME, true);
	unsigned int total = (int)list.size();
	int target = 0;
	// get random number in range
	if (total == 0)
		return NULL;
	target = (int)Math::round((std::rand() / (float)RAND_MAX) * (total - 1));
	return list.at(target);
}

int SystemData::getDisplayedGameCount()
{
	if (mGameCount < 0)
	{
		if (this == CollectionSystemManager::get()->getCustomCollectionsBundle())
			mGameCount = mRootFolder->getChildren().size();
		else
			mGameCount = mRootFolder->getFilesRecursive(GAME, true).size();
	}

	return mGameCount;
}

void SystemData::updateDisplayedGameCount()
{
	mGameCount =-1;
}

void SystemData::loadTheme()
{
	mTheme = std::make_shared<ThemeData>();

	std::string path = getThemePath();

	if(!Utils::FileSystem::exists(path)) // no theme available for this platform
		return;

	try
	{
		// build map with system variables for theme to use,
		std::map<std::string, std::string> sysData;
		sysData.insert(std::pair<std::string, std::string>("system.name", getName()));
		sysData.insert(std::pair<std::string, std::string>("system.theme", getThemeFolder()));
		sysData.insert(std::pair<std::string, std::string>("system.fullName", Utils::String::proper(getFullName())));

		sysData.insert(std::pair<std::string, std::string>("system.manufacturer", getSystemMetadata().manufacturer));
		sysData.insert(std::pair<std::string, std::string>("system.hardwareType", Utils::String::proper(getSystemMetadata().hardwareType)));

		if (Settings::getInstance()->getString("SortSystems") == "hardware")
			sysData.insert(std::pair<std::string, std::string>("system.sortedBy", Utils::String::proper(getSystemMetadata().hardwareType)));
		else
			sysData.insert(std::pair<std::string, std::string>("system.sortedBy", getSystemMetadata().manufacturer));

		if (getSystemMetadata().releaseYear > 0)
		{
			sysData.insert(std::pair<std::string, std::string>("system.releaseYearOrNull", std::to_string(getSystemMetadata().releaseYear)));
			sysData.insert(std::pair<std::string, std::string>("system.releaseYear", std::to_string(getSystemMetadata().releaseYear)));
		}
		else
			sysData.insert(std::pair<std::string, std::string>("system.releaseYear", _("Unknown")));

		mTheme->loadFile(getThemeFolder(), sysData, path);
	} catch(ThemeException& e)
	{
		LOG(LogError) << e.what();
		mTheme = std::make_shared<ThemeData>(); // reset to empty
	}
}

void SystemData::setSortId(const unsigned int sortId)
{
	mSortId = sortId;
	Settings::getInstance()->setInt(getName() + ".sort", mSortId);
}

bool SystemData::setSystemViewMode(std::string newViewMode, Vector2f gridSizeOverride, bool setChanged)
{
	if (newViewMode == "automatic")
		newViewMode = "";

	if (mViewMode == newViewMode && gridSizeOverride == mGridSizeOverride)
		return false;

	mGridSizeOverride = gridSizeOverride;
	mViewMode = newViewMode;

	if (setChanged)
	{
		Settings::getInstance()->setString(getName() + ".defaultView", mViewMode == "automatic" ? "" : mViewMode);
		Settings::getInstance()->setString(getName() + ".gridSize", Utils::String::replace(Utils::String::replace(mGridSizeOverride.toString(), ".000000", ""), "0 0", ""));
	}

	return true;
}

Vector2f SystemData::getGridSizeOverride()
{
	return mGridSizeOverride;
}

bool SystemData::isNetplaySupported()
{
	if (isGroupSystem())
		return false;	

	for (auto emul : mEmulators)
		for (auto core : emul.cores)
			if (core.netplay)
				return true;

	if (!SystemData::es_features_loaded)
		return getSystemEnvData() != nullptr && getSystemEnvData()->mLaunchCommand.find("%NETPLAY%") != std::string::npos;

	return false;
}

bool SystemData::isNetplayActivated()
{
	for (auto sys : SystemData::sSystemVector)
		if (sys->isNetplaySupported())
			return true;

	return false;	
}

bool SystemData::isGroupChildSystem() 
{ 
	if (mEnvData != nullptr && !mEnvData->mGroup.empty())
		return !Settings::getInstance()->getBool(mEnvData->mGroup + ".ungroup");

	return false;
}

std::unordered_set<std::string> SystemData::getAllGroupNames()
{
	std::unordered_set<std::string> names;
	
	for (auto sys : SystemData::sSystemVector)
	{
		if (sys->isGroupSystem())
			names.insert(sys->getName());
		else if (sys->mEnvData != nullptr && !sys->mEnvData->mGroup.empty())
			names.insert(sys->mEnvData->mGroup);
	}

	return names;
}

std::unordered_set<std::string> SystemData::getGroupChildSystemNames(const std::string groupName)
{
	std::unordered_set<std::string> names;

	for (auto sys : SystemData::sSystemVector)
		if (sys->mEnvData != nullptr && sys->mEnvData->mGroup == groupName)
			names.insert(sys->getFullName());
		
	return names;
}

SystemData* SystemData::getParentGroupSystem()
{
	if (!isGroupChildSystem() || isGroupSystem())
		return this;

	for (auto sys : SystemData::sSystemVector)
		if (sys->isGroupSystem() && sys->getName() == mEnvData->mGroup)
			return sys;

	return this;
}

std::string SystemData::getDefaultEmulator()
{
#if WIN32 && !_DEBUG
	std::string emulator = Settings::getInstance()->getString(getName() + ".emulator");
#else
#ifndef _ENABLEEMUELEC
	std::string emulator = SystemConf::getInstance()->get(getName() + ".emulator");
#else
	std::string emulator = getShOutput(R"(emuelec-utils setemu get ')" + getName() + ".emulator' ");
#endif
#endif

	for (auto emul : mEmulators)
		if (emul.name == emulator)
			return emulator;

	if (emulator.empty() || emulator == "auto")
	{
		// Seeking default="true" attribute
		for (auto emul : mEmulators)
			for (auto core : emul.cores)
				if (core.isDefault)
					return emul.name;
		
		auto emulators = getEmulators();
		if (emulators.size() > 0)
			return emulators.begin()->name;
	}

	return "";
}

std::string SystemData::getDefaultCore(const std::string emulatorName)
{
#if WIN32 && !_DEBUG
	std::string core = Settings::getInstance()->getString(getName() + ".core");
#else
#ifndef _ENABLEEMUELEC
	std::string core = SystemConf::getInstance()->get(getName() + ".core");
#else
	std::string core = getShOutput(R"(emuelec-utils setemu get ')" + getName() + ".core' ");
#endif
#endif

	if (!core.empty() && core != "auto")
		return core;
	
	// Seeking default="true" attribute
	for (auto memul : mEmulators)
		for (auto mcore : memul.cores)
			if (mcore.isDefault)
				return mcore.name;

	std::string emul = emulatorName;

	if (emul.empty() || emul == "auto")
		emul = getDefaultEmulator();

	if (!emul.empty())
	{
		auto emulators = getEmulators();

		for (auto it : emulators)
			if (it.name == emul)
				if (it.cores.size() > 0)
					return it.cores.begin()->name;
	}

	return "";
}

std::string SystemData::getLaunchCommand(const std::string emulatorName, const std::string coreName)
{
	for (auto emulator : mEmulators)
	{
		if (emulator.name == emulatorName)
		{
			for (auto& core : emulator.cores)
				if (coreName == core.name && !core.customCommandLine.empty())
					return core.customCommandLine;

			if (!emulator.customCommandLine.empty())
				return emulator.customCommandLine;
		}
	}

	return getSystemEnvData()->mLaunchCommand;
}

std::vector<std::string> SystemData::getCoreNames(std::string emulatorName)
{
	std::vector<std::string> list;

	for (auto& emulator : mEmulators)
		if (emulatorName == emulator.name)
			for(auto& core : emulator.cores)
				list.push_back(core.name);

	return list;
}

bool SystemData::hasEmulatorSelection()
{
	if (isGroupSystem() || isCollection() || hasPlatformId(PlatformIds::PLATFORM_IGNORE))
		return false;

	int ec = 0;
	int cc = 0;

	for (auto& emulator : mEmulators)
	{
		ec++;
		for (auto& core : emulator.cores)
			cc++;
	}

	return ec > 1 || cc > 1;
}

SystemData* SystemData::getSystem(const std::string name)
{
	for (auto sys : SystemData::sSystemVector)
		if (sys->getName() == name)
			return sys;

	return nullptr;
}
