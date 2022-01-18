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
#include "utils/Randomizer.h"
#include "views/ViewController.h"
#include "ThreadedHasher.h"
#include <unordered_set>
#include <algorithm>
#include "SaveStateRepository.h"
#include "Paths.h"

#if WIN32
#include "Win32ApiSystem.h"
#endif

using namespace Utils;

VectorEx<SystemData*> SystemData::sSystemVector;
bool SystemData::IsManufacturerSupported = false;

SystemData::SystemData(const SystemMetadata& meta, SystemEnvironmentData* envData, std::vector<EmulatorData>* pEmulators, bool CollectionSystem, bool groupedSystem, bool withTheme, bool loadThemeOnlyIfElements) :
	mMetadata(meta), mEnvData(envData), mIsCollectionSystem(CollectionSystem), mIsGameSystem(true)
{
	mSaveRepository = nullptr;
	mIsCheevosSupported = -1;
	mIsGroupSystem = groupedSystem;
	mGameListHash = 0;
	mGameCountInfo = nullptr;
	mSortId = Settings::getInstance()->getInt(getName() + ".sort");
	mGridSizeOverride = Vector2f(0, 0);

	mFilterIndex = nullptr;

	if (pEmulators != nullptr)
		mEmulators = *pEmulators;

	auto hiddenSystems = Utils::String::split(Settings::HiddenSystems(), ';');
	mHidden = (mIsCollectionSystem ? withTheme : (std::find(hiddenSystems.cbegin(), hiddenSystems.cend(), getName()) != hiddenSystems.cend()));

	loadFeatures();

	// if it's an actual system, initialize it, if not, just create the data structure
	if (!mIsCollectionSystem && mIsGameSystem)
	{
		mRootFolder = new FolderData(mEnvData->mStartPath, this);
		mRootFolder->getMetadata().set(MetaDataId::Name, mMetadata.fullName);

		std::unordered_map<std::string, FileData*> fileMap;
		fileMap[mEnvData->mStartPath] = mRootFolder;

		if (!Settings::ParseGamelistOnly())
		{
			populateFolder(mRootFolder, fileMap);
			if (mRootFolder->getChildren().size() == 0)
				return;

			if (mHidden && !Settings::HiddenSystemsShowGames())
				return;
		}

		if (!Settings::IgnoreGamelist())
			parseGamelist(this, fileMap);		
		
		if (Settings::RemoveMultiDiskContent())
			removeMultiDiskContent(fileMap);
	}
	else
	{
		// virtual systems are updated afterwards, we're just creating the data structure
		mRootFolder = new FolderData(mMetadata.fullName, this);
		mRootFolder->getMetadata().set(MetaDataId::Name, mMetadata.fullName);
	}

	mRootFolder->getMetadata().resetChangedFlag();

	if (withTheme && (!loadThemeOnlyIfElements || mRootFolder->mChildren.size() > 0))
	{
		loadTheme();

		auto defaultView = Settings::getInstance()->getString(getName() + ".defaultView");
		auto gridSizeOverride = Vector2f::parseString(Settings::getInstance()->getString(getName() + ".gridSize"));
		setSystemViewMode(defaultView, gridSizeOverride, false);

		setIsGameSystemStatus();

		if (Settings::PreloadMedias())
			getSaveStateRepository();
	}
}

SystemData::~SystemData()
{
	if (mRootFolder)
		delete mRootFolder;

	if (!mIsCollectionSystem && mEnvData != nullptr)
		delete mEnvData;

	if (mSaveRepository != nullptr)
		delete mSaveRepository;

	if (mGameCountInfo != nullptr)
		delete mGameCountInfo;

	if (mFilterIndex != nullptr)
		delete mFilterIndex;
}

void SystemData::removeMultiDiskContent(std::unordered_map<std::string, FileData*>& fileMap)
{	
	if (mEnvData == nullptr ||!(mEnvData->isValidExtension(".cue") || mEnvData->isValidExtension(".ccd") || mEnvData->isValidExtension(".gdi") || mEnvData->isValidExtension(".m3u")))
		return;

	StopWatch stopWatch("RemoveMultiDiskContent - "+ getName() +" :", LogDebug);

	std::vector<std::string> files;

	std::vector<FolderData*> folders;

	std::stack<FolderData*> stack;
	stack.push(mRootFolder);

	while (stack.size())
	{
		FolderData* current = stack.top();
		stack.pop();

		for (auto it : current->getChildren())
		{
			if (it->getType() == GAME && it->hasContentFiles())
			{
				for (auto ct : it->getContentFiles())
					files.push_back(ct);
			}
			else if (it->getType() == FOLDER)
			{
				folders.push_back((FolderData*)it);
				stack.push((FolderData*)it);
			}
		}
	}

	for (auto file : files)
	{
		auto it = fileMap.find(file);
		if (it != fileMap.cend())
		{
			delete it->second;
			fileMap.erase(it);
		}
	}

	// Remove empty folders
	for (auto folder = folders.crbegin(); folder != folders.crend(); ++folder)
	{
		if ((*folder)->getChildren().size())
			continue;
		
		auto it = fileMap.find((*folder)->getPath());
		if (it != fileMap.cend())
		{
			fileMap.erase(it);
			delete (*folder);
		}		
	}
}

void SystemData::setIsGameSystemStatus()
{
	// we exclude non-game systems from specific operations
	// if/when there are more in the future, maybe this can be a more complex method, with a proper list
	// but for now a simple string comparison is more performant
	mIsGameSystem = (mMetadata.name != "retropie" && mMetadata.name != "retrobat");
}

void SystemData::populateFolder(FolderData* folder, std::unordered_map<std::string, FileData*>& fileMap)
{
	const std::string& folderPath = folder->getPath();

	if(!Utils::FileSystem::isDirectory(folderPath))
		return;
	/*
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
	bool showHidden = Settings::ShowHiddenFiles();
	bool preloadMedias = Settings::PreloadMedias();

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

			// Never look in "artwork", reserved for mame roms artwork
			if (fn == "artwork")
				continue;

			if (preloadMedias && (!mHidden || Settings::HiddenSystemsShowGames()))
			{
				// Recurse list files in medias folder, just to let OS build filesystem cache 
				if (fn == "media" || fn == "medias")
				{
					Utils::FileSystem::getDirContent(filePath, true);
					continue;
				}

				// List files in folder, just to get OS build filesystem cache 
				if (fn == "manuals" || fn == "images" || fn == "videos" || Utils::String::startsWith(fn, "downloaded_"))
				{
					Utils::FileSystem::getDirectoryFiles(filePath);
					continue;
				}
			}

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

void SystemData::createGroupedSystems()
{
	auto hiddenSystems = Utils::String::split(Settings::HiddenSystems(), ';');

	std::map<std::string, std::vector<SystemData*>> map;

	for (auto sys : sSystemVector)
	{
		if (sys->isCollection() || sys->getSystemEnvData()->mGroup.empty())
			continue;
		
		if (Settings::getInstance()->getBool(sys->getSystemEnvData()->mGroup + ".ungroup") || Settings::getInstance()->getBool(sys->getName() + ".ungroup"))
			continue;

		if (sys->getName() == sys->getSystemEnvData()->mGroup)
		{
			sys->getSystemEnvData()->mGroup = "";
			continue;
		}		
		else if (std::find(hiddenSystems.cbegin(), hiddenSystems.cend(), sys->getName()) != hiddenSystems.cend())
			continue;
		
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

			// Check if the system is described in es_systems but empty, to import metadatas )
			auto sourceSystem = SystemData::loadSystem(item.first, false);
			if (sourceSystem != nullptr)
			{
				md.fullName = sourceSystem->getSystemMetadata().fullName;
				md.themeFolder = sourceSystem->getSystemMetadata().themeFolder;
				md.manufacturer = sourceSystem->getSystemMetadata().manufacturer;
				md.releaseYear = sourceSystem->getSystemMetadata().releaseYear;
				md.hardwareType = sourceSystem->getSystemMetadata().hardwareType;

				delete sourceSystem;
			}
			else if (item.second.size() > 0)
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

		if (std::find(hiddenSystems.cbegin(), hiddenSystems.cend(), system->getName()) != hiddenSystems.cend())
		{
			system->mHidden = true;

			if (!existingSystem)
				sSystemVector.push_back(system);
						
			for (auto childSystem : item.second)
				childSystem->getSystemEnvData()->mGroup = "";

			continue;
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
							folder->setMetadata(MetaDataId::Image, path);
							folder->setMetadata(MetaDataId::Thumbnail, path);
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

bool SystemData::loadFeatures()
{
	if (mEmulators.size() == 0)
		return false;

	if (mIsCollectionSystem || hasPlatformId(PlatformIds::IMAGEVIEWER) || hasPlatformId(PlatformIds::PLATFORM_IGNORE))
		return false;

	std::string systemName = this->getName();

	for (auto& emul : mEmulators)
	{
		emul.features = EmulatorFeatures::Features::none;
		emul.customFeatures.clear();

		for (auto& core : emul.cores)
		{
			core.netplay = false;
			core.features = EmulatorFeatures::Features::none;
			core.customFeatures.clear();
		}
	}

	if (!CustomFeatures::FeaturesLoaded)
		return false;

	for (auto& emul : mEmulators)
	{
		std::string name = emul.name;

		if (Utils::String::startsWith(emul.name, "lr-"))
			name = "libretro";

		auto it = CustomFeatures::EmulatorFeatures.find(name);
		if (it != CustomFeatures::EmulatorFeatures.cend())
		{
			emul.features = it->second.features;
			emul.customFeatures = it->second.customFeatures;			

			for (auto essystem : it->second.systemFeatures)
			{
				if (essystem.name != systemName)
					continue;

				emul.features = emul.features | essystem.features;
				for (auto feat : essystem.customFeatures)
					emul.customFeatures.push_back(feat);
			}

			for (auto& core : emul.cores)
			{
				for (auto escore : it->second.cores)
				{
					if (core.name != escore.name)
						continue;
					
					core.features = core.features | escore.features;

					for (auto feat : escore.customFeatures)
						core.customFeatures.push_back(feat);

					for (auto essystem : escore.systemFeatures)
					{
						if (essystem.name != systemName)
							continue;

						core.features = core.features | essystem.features;
						for (auto feat : essystem.customFeatures)
							core.customFeatures.push_back(feat);
					}

					core.netplay = (core.features & EmulatorFeatures::Features::netplay) == EmulatorFeatures::Features::netplay;
				}
			}
		}
	}

	return true;
}

bool SystemData::isCurrentFeatureSupported(EmulatorFeatures::Features feature)
{
	return isFeatureSupported(getEmulator(), getCore(), feature);
}

bool SystemData::hasFeatures()
{
	if (isCollection() || hasPlatformId(PlatformIds::PLATFORM_IGNORE))
		return false;

	for (auto emulator : mEmulators)
	{
		for (auto& core : emulator.cores)
			if (core.features != EmulatorFeatures::Features::none || core.customFeatures.size() > 0)
				return true;

		if (emulator.features != EmulatorFeatures::Features::none || emulator.customFeatures.size() > 0)
			return true;
	}

	return !CustomFeatures::FeaturesLoaded;
}

CustomFeatures SystemData::getCustomFeatures(std::string emulatorName, std::string coreName)
{
	CustomFeatures ret;

	if (emulatorName.empty() || emulatorName == "auto")
		emulatorName = getEmulator();

	if (coreName.empty() || coreName == "auto")
		coreName = getCore();

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

	ret.sort();
	return ret;
}

bool SystemData::isFeatureSupported(std::string emulatorName, std::string coreName, EmulatorFeatures::Features feature)
{
	if (emulatorName.empty() || emulatorName == "auto")
		emulatorName = getEmulator();

	if (coreName.empty() || coreName == "auto")
		coreName = getCore();

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

	return !CustomFeatures::FeaturesLoaded;
}

// Load custom additionnal config from es_systems_*.cfg files
void SystemData::loadAdditionnalConfig(pugi::xml_node& srcSystems)
{
	for (auto customPath : Utils::FileSystem::getDirContent(Paths::getUserEmulationStationPath(), false, false))
	{
		if (Utils::FileSystem::getExtension(customPath) != ".cfg")
			continue;

		if (!Utils::String::startsWith(Utils::FileSystem::getFileName(customPath), "es_systems_"))
			continue;

		pugi::xml_document doc;
		pugi::xml_parse_result res = doc.load_file(customPath.c_str());
		if (!res)
		{
			LOG(LogError) << "Could not parse " << Utils::FileSystem::getFileName(customPath) << " file!";
			return;
		}

		pugi::xml_node systemList = doc.child("systemList");
		if (!systemList)
		{
			LOG(LogError) << Utils::FileSystem::getFileName(customPath) << " is missing the <systemList> tag !";
			return;
		}

		for (pugi::xml_node system = systemList.child("system"); system; system = system.next_sibling("system"))
		{
			if (!system.child("name"))
				continue;

			std::string name = system.child("name").text().get();
			if (name.empty())
				continue;

			bool found = false;

			// Remove existing one
			for (pugi::xml_node& srcSystem : srcSystems.children())
			{
				if (std::string(srcSystem.name()) != "system")
					continue;

				std::string srcName = srcSystem.child("name").text().get();
				if (srcName != name)
					continue;
				
				found = true;
					
				for (pugi::xml_node& child : system.children())
				{
					std::string tag = child.name();
					if (tag == "name")
						continue;
						
					srcSystem.remove_child(tag.c_str());

					if (tag == "emulators" || !std::string(child.text().get()).empty())				
						srcSystem.append_copy(child);												
				}
					
				break;				
			}

			if (!found)
				srcSystems.append_copy(system);
		}
	}
}

//creates systems from information located in a config file
bool SystemData::loadConfig(Window* window)
{
	deleteSystems();
	ThemeData::setDefaultTheme(nullptr);

	std::string path = getConfigPath();

	LOG(LogInfo) << "Loading system config file " << path << "...";

	if (!Utils::FileSystem::exists(path))
	{
		LOG(LogError) << "es_systems.cfg file does not exist!";
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

	loadAdditionnalConfig(systemList);

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

	CustomFeatures::loadEsFeaturesFile();

	int currentSystem = 0;

	typedef SystemData* SystemDataPtr;

	ThreadPool* pThreadPool = NULL;
	SystemDataPtr* systems = NULL;

	// Allow threaded loading only if processor threads > 1 so it does not apply on machines like Pi0.
	if (std::thread::hardware_concurrency() > 1 && Settings::ThreadedLoading())
	{
		pThreadPool = new ThreadPool();

		systems = new SystemDataPtr[systemCount];
		for (int i = 0; i < systemCount; i++)
			systems[i] = nullptr;

		pThreadPool->queueWorkItem([] { CollectionSystemManager::get()->loadCollectionSystems(); });
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
	}
	else
	{
		if (window != NULL)
			window->renderSplashScreen(_("Collections"), systemCount == 0 ? 0 : currentSystem / systemCount);

		CollectionSystemManager::get()->loadCollectionSystems();
	}

	if (SystemData::sSystemVector.size() > 0)
	{
		createGroupedSystems();

		// Load features before creating collections
		//loadFeatures();

		CollectionSystemManager::get()->updateSystemsList();

		for (auto sys : SystemData::sSystemVector)
		{
			auto theme = sys->getTheme();
			if (theme != nullptr)
			{
				ViewController::get()->onThemeChanged(theme);
				break;
			}
		}
	}

	if (window != nullptr && !ThreadedHasher::isRunning())
	{
		int checkIndex = 0;

		if (Settings::CheevosCheckIndexesAtStart())
			checkIndex |= (int) ThreadedHasher::HASH_CHEEVOS_MD5;

		if (SystemConf::getInstance()->getBool("global.netplay") && Settings::NetPlayCheckIndexesAtStart())
			checkIndex |= (int) ThreadedHasher::HASH_NETPLAY_CRC;

		if (checkIndex != 0)
			ThreadedHasher::start(window, (ThreadedHasher::HasherType)checkIndex, false, true);
	}

	return true;
}

SystemData* SystemData::loadSystem(std::string systemName, bool fullMode)
{
	std::string path = getConfigPath();
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

	loadAdditionnalConfig(systemList);

	for (pugi::xml_node system = systemList.child("system"); system; system = system.next_sibling("system"))
	{
		std::string name = system.child("name").text().get();
		if (name == systemName)
			return loadSystem(system, fullMode);
	}

	return nullptr;
}

std::map<std::string, std::string> SystemData::getKnownSystemNames()
{
	std::map<std::string, std::string> ret;

	std::string path = getConfigPath();
	if (!Utils::FileSystem::exists(path))
		return ret;

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(path.c_str());
	if (!res)
		return ret;

	//actually read the file
	pugi::xml_node systemList = doc.child("systemList");
	if (!systemList)
		return ret;

	loadAdditionnalConfig(systemList);

	for (pugi::xml_node system = systemList.child("system"); system; system = system.next_sibling("system"))
	{
		std::string name = system.child("name").text().get();
		if (name.empty())
			continue;

		std::string fullName = system.child("fullname").text().get();
		if (fullName.empty())
			continue;
		
		ret[name] = fullName;
	}

	return ret;
}

#define readList(x) Utils::String::splitAny(x, " \t\r\n,", true)

SystemData* SystemData::loadSystem(pugi::xml_node system, bool fullMode)
{
	std::string path, cmd; // , name, fullname, themeFolder;

	path = system.child("path").text().get();

	SystemMetadata md;
	md.name = system.child("name").text().get();
	md.fullName = system.child("fullname").text().get();
	md.manufacturer = system.child("manufacturer").text().get();
	md.releaseYear = Utils::String::toInteger(system.child("release").text().get());
	md.hardwareType = system.child("hardware").text().get();
	md.themeFolder = system.child("theme").text().as_string(md.name.c_str());

	// convert extensions list from a string into a vector of strings
	std::set<std::string> extensions;
	for (auto ext : readList(system.child("extension").text().get()))
	{
		std::string extlow = Utils::String::toLower(ext);
		if (extensions.find(extlow) == extensions.cend())
			extensions.insert(extlow);
	}

	cmd = system.child("command").text().get();

	// platform id list
	std::string platformList = system.child("platform").text().get();
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

			if (md.name == "imageviewer")
				platformIds.push_back(PlatformIds::IMAGEVIEWER);
			else
				platformIds.push_back(platformId);

			break;
		}

		// if there appears to be an actual platform ID supplied but it didn't match the list, warn
		if (platformId != PlatformIds::PLATFORM_UNKNOWN)
			platformIds.push_back(platformId);
		else if (str != NULL && str[0] != '\0' && platformId == PlatformIds::PLATFORM_UNKNOWN)
			LOG(LogWarning) << "  Unknown platform for system \"" << md.name << "\" (platform \"" << str << "\" from list \"" << platformList << "\")";
	}

	//convert path to generic directory seperators
	path = Utils::FileSystem::getGenericPath(path);

	//expand home symbol if the startpath contains ~
	if (path[0] == '~')
	{
		path.erase(0, 1);
		path.insert(0, Paths::getHomePath());
		path = Utils::FileSystem::getCanonicalPath(path);
	}

	//validate
	if (fullMode && (md.name.empty() || path.empty() || extensions.empty() || cmd.empty() || !Utils::FileSystem::exists(path)))
	{
		LOG(LogError) << "System \"" << md.name << "\" is missing name, path, extension, or command!";
		return nullptr;
	}

	//create the system runtime environment data
	SystemEnvironmentData* envData = new SystemEnvironmentData;
	envData->mStartPath = path;
	envData->mSearchExtensions = extensions;
	envData->mLaunchCommand = cmd;
	envData->mPlatformIds = platformIds;
	envData->mGroup = system.child("group").text().get();
	
	// Emulators and cores
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

			if (emuNode.attribute("incompatible_extensions"))
			{
				for (auto ext : readList(emuNode.attribute("incompatible_extensions").value()))
				{
					std::string extlow = Utils::String::toLower(ext);
					if (std::find(emulatorData.incompatibleExtensions.cbegin(), emulatorData.incompatibleExtensions.cend(), extlow) == emulatorData.incompatibleExtensions.cend())
						emulatorData.incompatibleExtensions.push_back(extlow);
				}
			}

			pugi::xml_node coresNode = emuNode.child("cores");
			if (coresNode != nullptr)
			{
				for (pugi::xml_node coreNode = coresNode.child("core"); coreNode; coreNode = coreNode.next_sibling("core"))
				{
					CoreData core;
					core.name = coreNode.text().as_string();
					core.netplay = coreNode.attribute("netplay") && strcmp(coreNode.attribute("netplay").value(), "true") == 0;
					core.isDefault = coreNode.attribute("default") && strcmp(coreNode.attribute("default").value(), "true") == 0;
					
					if (coreNode.attribute("incompatible_extensions"))
					{
						for (auto ext : readList(coreNode.attribute("incompatible_extensions").value()))
						{
							std::string extlow = Utils::String::toLower(ext);
							if (std::find(core.incompatibleExtensions.cbegin(), core.incompatibleExtensions.cend(), extlow) == core.incompatibleExtensions.cend())
								core.incompatibleExtensions.push_back(extlow);
						}
					}

					core.features = EmulatorFeatures::Features::all;
					core.customCommandLine = coreNode.attribute("command").value();

					emulatorData.cores.push_back(core);
				}
			}

			systemEmulators.push_back(emulatorData);
		}
	}

	SystemData* newSys = new SystemData(md, envData, &systemEmulators, false, false, fullMode, true);

	if (!fullMode)
		return newSys;
	
	if (newSys->getRootFolder()->getChildren().size() == 0)
	{
		LOG(LogWarning) << "System \"" << md.name << "\" has no games! Ignoring it.";
		delete newSys;
		return nullptr;
	}	

	if (!newSys->mIsCollectionSystem && newSys->mIsGameSystem && !md.manufacturer.empty() && !IsManufacturerSupported)
		IsManufacturerSupported = true;

	return newSys;
}

bool SystemData::hasDirtySystems()
{
	bool saveOnExit = !Settings::IgnoreGamelist() && Settings::SaveGamelistsOnExit();
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
	bool saveOnExit = !Settings::IgnoreGamelist() && Settings::SaveGamelistsOnExit;

	for (unsigned int i = 0; i < sSystemVector.size(); i++)
	{
		SystemData* pData = sSystemVector.at(i);
		pData->getRootFolder()->removeVirtualFolders();

		if (saveOnExit && !pData->mIsCollectionSystem)
			updateGamelist(pData);

		delete pData;
	}

	sSystemVector.clear();
	IsManufacturerSupported = false;
}

std::string SystemData::getConfigPath()
{
	std::string customPath = Paths::getUserEmulationStationPath() + "/es_systems_custom.cfg";
	if (Utils::FileSystem::exists(customPath))
		return customPath;

	std::string userdataPath = Paths::getUserEmulationStationPath() + "/es_systems.cfg";
	if(Utils::FileSystem::exists(userdataPath))
		return userdataPath;

	userdataPath = Paths::getEmulationStationPath() + "/es_systems.cfg";
	if (Utils::FileSystem::exists(userdataPath))
		return userdataPath;

	return "/etc/emulationstation/es_systems.cfg"; // Backward compatibility with Retropie
}

bool SystemData::isVisible()
{
	if (mIsCollectionSystem)
	{
		if (mMetadata.name != "favorites" && !UIModeController::getInstance()->isUIModeFull() && getGameCountInfo()->totalGames == 0)
			return false;

		return true;
	}

	if (isGroupChildSystem())
		return false;

	if (!mHidden && !mIsCollectionSystem && getGameCountInfo()->totalGames > 0)
		return true;

	return false;
}

SystemData* SystemData::getNext() const
{
	auto it = getIterator();

	do 
	{
		it++;
		if (it == sSystemVector.cend())
			it = sSystemVector.cbegin();
	} 
	while (!(*it)->isVisible());
	
	// as we are starting in a valid gamelistview, this will always succeed, even if we have to come full circle.

	return *it;
}

SystemData* SystemData::getPrev() const
{
	auto it = getRevIterator();

	do 
	{
		it++;
		if (it == sSystemVector.crend())
			it = sSystemVector.crbegin();
	} 
	while (!(*it)->isVisible());
	// as we are starting in a valid gamelistview, this will always succeed, even if we have to come full circle.

	return *it;
}

std::string SystemData::getGamelistPath(bool forWrite) const
{
	std::string filePath;

	filePath = mRootFolder->getPath() + "/gamelist.xml";
	if(Utils::FileSystem::exists(filePath))
		return filePath;

	std::string localPath = Paths::getUserEmulationStationPath() + "/gamelists/" + mMetadata.name + "/gamelist.xml";
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
	unsigned int total = sSystemVector.count([](auto sys) { return sys->isGameSystem(); });

	// get random number in range
	int target = Randomizer::random(total);
	//int target = (int)Math::round((std::rand() / (float)RAND_MAX) * (total - 1));
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
	if (total == 0)
		return NULL;

	int target = Randomizer::random(total);
	//target = (int)Math::round((std::rand() / (float)RAND_MAX) * (total - 1));
	return list.at(target);
}

GameCountInfo* SystemData::getGameCountInfo()
{
	if (mGameCountInfo != nullptr)
		return mGameCountInfo;	

	std::vector<FileData*> games = mRootFolder->getFilesRecursive(GAME, true);

	int realTotal = games.size();
	if (mFilterIndex != nullptr)
	{
		auto savedFilter = mFilterIndex;
		mFilterIndex = nullptr;
		realTotal = mRootFolder->getFilesRecursive(GAME, true).size();
		mFilterIndex = savedFilter;
	}


	mGameCountInfo = new GameCountInfo();
	mGameCountInfo->visibleGames = games.size();
	mGameCountInfo->totalGames = realTotal;
	mGameCountInfo->favoriteCount = 0;
	mGameCountInfo->hiddenCount = 0;
	mGameCountInfo->playCount = 0;
	mGameCountInfo->gamesPlayed = 0;

	int mostPlayCount = 0;

	for (auto game : games)
	{
		if (game->getFavorite())
			mGameCountInfo->favoriteCount++;

		if (game->getHidden())
			mGameCountInfo->hiddenCount++;

		int playCount = Utils::String::toInteger(game->getMetadata(MetaDataId::PlayCount));
		if (playCount > 0)
		{
			mGameCountInfo->gamesPlayed++;
			mGameCountInfo->playCount += playCount;

			if (playCount > mostPlayCount)
			{
				mGameCountInfo->mostPlayed = game->getName();
				mostPlayCount = playCount;
			}
		}

		auto lastPlayed = game->getMetadata(MetaDataId::LastPlayed);
		if (!lastPlayed.empty() && lastPlayed > mGameCountInfo->lastPlayedDate)
			mGameCountInfo->lastPlayedDate = lastPlayed;
	}

	return mGameCountInfo;
	/*
	if (this == CollectionSystemManager::get()->getCustomCollectionsBundle())
		mGameCount = mRootFolder->getChildren().size();
	else
		mGameCount = mRootFolder->getFilesRecursive(GAME, true).size();

	return mGameCount;*/
}

void SystemData::updateDisplayedGameCount()
{
	if (mGameCountInfo != nullptr)
		delete mGameCountInfo;

	mGameCountInfo = nullptr;
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

		if (SystemConf::getInstance()->getBool("global.retroachievements") && (isCheevosSupported() || isCollection() || isGroupSystem()))
			sysData.insert(std::pair<std::string, std::string>("system.cheevos", "true"));

		if (SystemConf::getInstance()->getBool("global.retroachievements"))
			sysData.insert(std::pair<std::string, std::string>("cheevos.username", SystemConf::getInstance()->get("global.retroachievements.username")));

		mTheme->loadFile(getThemeFolder(), sysData, path);
	}
	catch(ThemeException& e)
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
	for (auto emul : mEmulators)
		for (auto core : emul.cores)
			if (core.netplay)
				return true;

	if (!isGameSystem())
		return false;

	if (!CustomFeatures::FeaturesLoaded)
		return getSystemEnvData() != nullptr && getSystemEnvData()->mLaunchCommand.find("%NETPLAY%") != std::string::npos;

	return false;
}

std::string SystemData::getCompatibleCoreNames(EmulatorFeatures::Features feature)
{
	std::string ret;

	for (auto emul : mEmulators)
	{
		if ((emul.features & feature) == feature)
			ret += ret.empty() ? emul.name : ", " + emul.name;
		else
		{
			for (auto core : emul.cores)
			{
				if ((core.features & feature) == feature)
				{
					std::string name = emul.name == core.name ? core.name : emul.name + "/" + core.name;
					ret += ret.empty() ? name : ", " + name;
				}
			}
		}
	}

	return ret;
}


bool SystemData::isCheevosSupported()
{
	if (isCollection())
		return false;

	if (mIsCheevosSupported < 0)
	{
		mIsCheevosSupported = 0;

		if (!CustomFeatures::FeaturesLoaded)
		{
			const std::set<std::string> cheevosSystems = {
				"megadrive", "n64", "snes", "gb", "gba", "gbc", "nes", "fds", "pcengine", "segacd", "sega32x", "mastersystem",
				"atarilynx", "lynx", "ngp", "gamegear", "pokemini", "atari2600", "fbneo", "fbn", "virtualboy", "pcfx", "tg16", "famicom", "msx1",
				"psx", "sg-1000", "sg1000", "coleco", "colecovision", "atari7800", "wonderswan", "pc88", "saturn", "3do", "apple2", "neogeo", "arcade", "mame",
				"nds", "arcade", "atarilynx", "megadrive-japan", "pcenginecd", "supergrafx", "supervision", "snes-msu1" };

			if (cheevosSystems.find(getName()) != cheevosSystems.cend())
				mIsCheevosSupported = 1;

			return mIsCheevosSupported != 0;
		}

		for (auto emul : mEmulators)
		{
			for (auto core : emul.cores)
			{
				if ((core.features & EmulatorFeatures::cheevos) == EmulatorFeatures::cheevos)
				{
					mIsCheevosSupported = 1;
					return true;
				}
			}
		}
	}

	return mIsCheevosSupported != 0;
}

bool SystemData::isNetplayActivated()
{
	return sSystemVector.any([](auto sys) { return sys->isNetplaySupported(); });
}

bool SystemData::isGroupChildSystem() 
{ 
	if (mEnvData != nullptr && !mEnvData->mGroup.empty())
		return !Settings::getInstance()->getBool(mEnvData->mGroup + ".ungroup") && 
			   !Settings::getInstance()->getBool(getName() + ".ungroup");

	return false;
}

std::unordered_set<std::string> SystemData::getAllGroupNames()
{
	auto hiddenSystems = Utils::String::split(Settings::HiddenSystems(), ';');

	std::unordered_set<std::string> names;
	
	for (auto sys : SystemData::sSystemVector)
	{
		std::string name;
		if (sys->isGroupSystem())
			name = sys->getName();
		else if (sys->mEnvData != nullptr && !sys->mEnvData->mGroup.empty())
			name = sys->mEnvData->mGroup;

		if (!name.empty() && std::find(hiddenSystems.cbegin(), hiddenSystems.cend(), name) == hiddenSystems.cend())
			names.insert(name);
	}

	return names;
}

std::unordered_set<std::string> SystemData::getGroupChildSystemNames(const std::string groupName)
{
	std::unordered_set<std::string> names;

	for (auto sys : SystemData::sSystemVector)
		if (sys->mEnvData != nullptr && sys->mEnvData->mGroup == groupName)
			names.insert(sys->getName());
		
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


std::string SystemData::getEmulator(bool resolveDefault)
{
#if WIN32 && !_DEBUG
	std::string emulator = Settings::getInstance()->getString(getName() + ".emulator");
#else
	std::string emulator = SystemConf::getInstance()->get(getName() + ".emulator");
#endif

	for (auto emul : mEmulators)
		if (emul.name == emulator)
			return emulator;

	if (resolveDefault)
		return getDefaultEmulator();

	return "";
}

std::string SystemData::getCore(bool resolveDefault)
{
#if WIN32 && !_DEBUG
	std::string core = Settings::getInstance()->getString(getName() + ".core");
#else
	std::string core = SystemConf::getInstance()->get(getName() + ".core");
#endif

	if (!core.empty() && core != "auto")
	{
		auto emul = getEmulator(true);

		for (auto memul : mEmulators)
			if (memul.name == emul)
				for (auto mcore : memul.cores)
					if (mcore.name == core)
						return core;
	}

	if (!getEmulator(false).empty())
		return getDefaultCore(getEmulator(false));

	if (resolveDefault)
		return getDefaultCore(getEmulator(true));

	return "";
}


std::string SystemData::getDefaultEmulator()
{
	// Seeking default="true" attribute
	for (auto emul : mEmulators)
		for (auto core : emul.cores)
			if (core.isDefault)
				return emul.name;
		
	auto emulators = getEmulators();
	if (emulators.size() > 0)
		return emulators.begin()->name;

	return "";
}

std::string SystemData::getDefaultCore(const std::string emulatorName)
{
	std::string emul = emulatorName;
	if (emul.empty() || emul == "auto")
		emul = getDefaultEmulator();

	if (emul.empty())
		return "";
	
	for (auto it : mEmulators)
	{
		if (it.name == emul)
		{
			for (auto core : it.cores)
				if (core.isDefault)
					return core.name;

			if (it.cores.size() > 0)
				return it.cores.begin()->name;
		}
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
	if (isCollection() || hasPlatformId(PlatformIds::PLATFORM_IGNORE))
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

SystemData* SystemData::getFirstVisibleSystem()
{
	for (auto sys : SystemData::sSystemVector)
		if (sys->mTheme != nullptr && sys->isVisible())
			return sys;

	return nullptr;
}

std::string SystemData::getKeyboardMappingFilePath()
{		
	return Paths::getUserKeyboardMappingsPath() + "/" + getName() + ".keys";
}

bool SystemData::hasKeyboardMapping()
{
	if (isCollection())
		return false;

	return Utils::FileSystem::exists(getKeyboardMappingFilePath());
}

KeyMappingFile SystemData::getKeyboardMapping()
{
	KeyMappingFile ret;

	if (Utils::FileSystem::exists(getKeyboardMappingFilePath()))
		ret = KeyMappingFile::load(getKeyboardMappingFilePath());
	else
	{
		std::string path = Paths::getKeyboardMappingsPath();
		if (!path.empty())
			ret = KeyMappingFile::load(path + "/" + getName() + ".keys");
	}

	ret.path = getKeyboardMappingFilePath();
	return ret;
}

bool SystemData::shouldExtractHashesFromArchives()
{
	return
		!hasPlatformId(PlatformIds::ARCADE) &&
		!hasPlatformId(PlatformIds::NEOGEO) &&
		!hasPlatformId(PlatformIds::DAPHNE) &&
		!hasPlatformId(PlatformIds::LUTRO) &&
		!hasPlatformId(PlatformIds::SEGA_DREAMCAST) &&
		!hasPlatformId(PlatformIds::ATOMISWAVE) &&
		!hasPlatformId(PlatformIds::NAOMI);
}

void SystemData::resetSettings()
{
	for(auto sys : sSystemVector)
		sys->mShowFilenames.reset();
}

SaveStateRepository* SystemData::getSaveStateRepository()
{
	if (mSaveRepository == nullptr)
		mSaveRepository = new SaveStateRepository(this);

	return mSaveRepository;
}

bool SystemData::getShowFilenames()
{
	if (mShowFilenames == nullptr)
	{
		auto curFn = Settings::getInstance()->getString(getName() + ".ShowFilenames");
		if (curFn.empty())
			mShowFilenames = std::make_shared<bool>(Settings::getInstance()->getBool("ShowFilenames"));
		else
			mShowFilenames = std::make_shared<bool>(curFn == "1");
	}

	return *mShowFilenames;
}

bool SystemData::getBoolSetting(const std::string& settingName)
{
	bool show = Settings::getInstance()->getBool(settingName);

	auto spf = Settings::getInstance()->getString(getName() + "." + settingName);
	if (spf == "1")
		return true;
	else if (spf == "0")
		return false;

	return show;
}

bool SystemData::getShowParentFolder()
{
	return getBoolSetting("ShowParentFolder");
}

std::string SystemData::getFolderViewMode()
{
	std::string showFoldersMode = Settings::getInstance()->getString("FolderViewMode");

	auto fvm = Settings::getInstance()->getString(getName() + ".FolderViewMode");
	if (!fvm.empty() && fvm != "auto") 
		showFoldersMode = fvm;

	if ((fvm.empty() || fvm == "auto") && this == CollectionSystemManager::get()->getCustomCollectionsBundle())
		showFoldersMode = "always";
	else if (getName() == "windows_installers")
		showFoldersMode = "always";

	return showFoldersMode;
}

bool SystemData::getShowFavoritesFirst()
{
	if (!getShowFavoritesIcon())
		return false;

	return getBoolSetting("FavoritesFirst");
}

bool SystemData::getShowFavoritesIcon()
{
	return getName() != "favorites" && getName() != "recent";
}

bool SystemData::getShowCheevosIcon()
{
	if (getName() != "retroachievements" && SystemConf::getInstance()->getBool("global.retroachievements"))
		return isCollection() || isCheevosSupported();

	return false;
}

int SystemData::getShowFlags()
{
	if (!getShowFavoritesIcon())
		return false;

	int show = Utils::String::toInteger(Settings::getInstance()->getString("ShowFlags"));

	auto spf = Settings::getInstance()->getString(getName() + ".ShowFlags");
	if (spf == "" || spf == "auto")
		return show;
	
	return Utils::String::toInteger(spf);
}
