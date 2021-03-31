#include "CollectionSystemManager.h"

#include "guis/GuiInfoPopup.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "views/gamelist/IGameListView.h"
#include "views/ViewController.h"
#include "FileData.h"
#include "FileSorts.h"
#include "FileFilterIndex.h"
#include "Log.h"
#include "Settings.h"
#include "SystemData.h"
#include "ThemeData.h"
#include "LocaleES.h"
#include <pugixml/src/pugixml.hpp>
#include <fstream>
#include "Gamelist.h"
#include "FileSorts.h"
#include "views/gamelist/ISimpleGameListView.h"
#include "PlatformId.h"
#include "utils/ThreadPool.h"

#if WIN32
#include "Win32ApiSystem.h"
#endif

std::string myCollectionsName = "collections";

#define LAST_PLAYED_MAX	50

/* Handling the getting, initialization, deinitialization, saving and deletion of
 * a CollectionSystemManager Instance */
CollectionSystemManager* CollectionSystemManager::sInstance = NULL;

std::vector<CollectionSystemDecl> CollectionSystemManager::getSystemDecls()
{
	CollectionSystemDecl systemDecls[] = {
		//type                name            long name                 default sort					  theme folder               isCustom     displayIfEmpty
		{ AUTO_ALL_GAMES,       "all",          _("all games"),         FileSorts::FILENAME_ASCENDING,    "auto-allgames",           false,       true },
		{ AUTO_LAST_PLAYED,     "recent",       _("last played"),       FileSorts::LASTPLAYED_ASCENDING,  "auto-lastplayed",         false,       true },
		{ AUTO_FAVORITES,       "favorites",    _("favorites"),         FileSorts::FILENAME_ASCENDING,    "auto-favorites",          false,       true },
		{ AUTO_AT2PLAYERS,      "2players",	    _("2 players"),         FileSorts::FILENAME_ASCENDING,    "auto-at2players",         false,       true }, // batocera
		{ AUTO_AT4PLAYERS,      "4players",     _("4 players"),         FileSorts::FILENAME_ASCENDING,    "auto-at4players",         false,       true }, // batocera
		{ AUTO_NEVER_PLAYED,    "neverplayed",  _("never played"),      FileSorts::FILENAME_ASCENDING,    "auto-neverplayed",        false,       true }, // batocera
		{ AUTO_RETROACHIEVEMENTS,"retroachievements",  _("retroachievements"),  FileSorts::FILENAME_ASCENDING,    "auto-retroachievements",        false,       true }, // batocera

		// Arcade meta 
		{ AUTO_ARCADE,           "arcade",      _("arcade"),            FileSorts::FILENAME_ASCENDING,    "arcade",				     false,       true }, // batocera
		{ AUTO_VERTICALARCADE,  "vertical",     _("vertical arcade"),   FileSorts::FILENAME_ASCENDING,    "auto-verticalarcade",     false,       true }, // batocera

		// Custom collection
		{ CUSTOM_COLLECTION,    myCollectionsName,  _("collections"),   FileSorts::FILENAME_ASCENDING,    "custom-collections",      true,        true }
	};

	auto ret = std::vector<CollectionSystemDecl>(systemDecls, systemDecls + sizeof(systemDecls) / sizeof(systemDecls[0]));

	// Arcade systems
	for (auto arcade : PlatformIds::ArcadeSystems)
	{
		CollectionSystemDecl decl;
		decl.type = (CollectionSystemType) (1000 + arcade.first);
		decl.name = "z" + arcade.second.first;
		decl.longName = arcade.second.second;
		decl.defaultSortId = FileSorts::FILENAME_ASCENDING;
		decl.themeFolder = arcade.second.first;
		decl.isCustom = false;
		decl.displayIfEmpty = false;
		ret.push_back(decl);
	}

	return ret;
}

CollectionSystemManager::CollectionSystemManager(Window* window) : mWindow(window)
{
	// create a map
	std::vector<CollectionSystemDecl> tempSystemDecl = getSystemDecls();

	for (auto it = tempSystemDecl.cbegin(); it != tempSystemDecl.cend(); ++it )
		mCollectionSystemDeclsIndex[(*it).name] = (*it);

	// creating standard environment data
	mCollectionEnvData = new SystemEnvironmentData;
	mCollectionEnvData->mStartPath = "";
	mCollectionEnvData->mLaunchCommand = "";
	mCollectionEnvData->mPlatformIds.push_back(PlatformIds::PLATFORM_IGNORE);

	std::string path = getCollectionsFolder();
	if(!Utils::FileSystem::exists(path))
		Utils::FileSystem::createDirectory(path);
		
	mCustomCollectionsBundle = NULL;
}

CollectionSystemManager::~CollectionSystemManager()
{
	removeCollectionsFromDisplayedSystems();

	// iterate the map
	for(auto it = mCustomCollectionSystemsData.cbegin() ; it != mCustomCollectionSystemsData.cend() ; it++ )
	{
		if (it->second.filteredIndex != nullptr)
			delete it->second.filteredIndex;
		else if (it->second.isPopulated)
			saveCustomCollection(it->second.system);

		delete it->second.system;
	}

	for (auto it = mAutoCollectionSystemsData.cbegin(); it != mAutoCollectionSystemsData.cend(); it++)
		delete it->second.system;

	if (mCustomCollectionsBundle != nullptr)
	{
		delete mCustomCollectionsBundle;
		mCustomCollectionsBundle = nullptr;
	}

	if (mCollectionEnvData != nullptr)
	{
		delete mCollectionEnvData;
		mCollectionEnvData = nullptr;
	}

	sInstance = NULL;
}

bool systemByAlphaSort(SystemData* sys1, SystemData* sys2)
{
	std::string name1 = Utils::String::toUpper(sys1->getFullName());
	std::string name2 = Utils::String::toUpper(sys2->getFullName());
	return name1.compare(name2) < 0;
}

bool systemByManufacurerSort(SystemData* sys1, SystemData* sys2)
{	
	// Move collection at End
	if (sys1->isCollection() != sys2->isCollection())
		return sys2->isCollection();

	// Move custom collections before auto collections
	if (sys1->isCollection() && sys2->isCollection())
	{
		std::string hw1 = Utils::String::toUpper(sys1->getSystemMetadata().hardwareType);
		std::string hw2 = Utils::String::toUpper(sys2->getSystemMetadata().hardwareType);

		if (hw1 != hw2)
			return hw1.compare(hw2) >= 0;
	}
	
	// Order by manufacturer
	std::string mf1 = Utils::String::toUpper(sys1->getSystemMetadata().manufacturer);
	std::string mf2 = Utils::String::toUpper(sys2->getSystemMetadata().manufacturer);

	if (mf1 != mf2)
		return mf1.compare(mf2) < 0;

	// Then by release date 
	if (sys1->getSystemMetadata().releaseYear < sys2->getSystemMetadata().releaseYear)
		return true;
	else if (sys1->getSystemMetadata().releaseYear > sys2->getSystemMetadata().releaseYear)
		return false;

	// Then by name
	std::string name1 = Utils::String::toUpper(sys1->getName());
	std::string name2 = Utils::String::toUpper(sys2->getName());
	return name1.compare(name2) < 0;
}

bool systemByReleaseDate(SystemData* sys1, SystemData* sys2)
{
	// Order by hardware
	int mf1 = sys1->getSystemMetadata().releaseYear;
	int mf2 = sys2->getSystemMetadata().releaseYear;
	if (mf1 != mf2)
		return mf1 < mf2;

	// Move collection at Begin
	if (sys1->isCollection() != sys2->isCollection())
		return !sys2->isCollection();

	// Then by name
	std::string name1 = Utils::String::toUpper(sys1->getName());
	std::string name2 = Utils::String::toUpper(sys2->getName());
	return name1.compare(name2) < 0;
}

bool systemByHardwareSort(SystemData* sys1, SystemData* sys2)
{
	// Move collection at End
	if (sys1->isCollection() != sys2->isCollection())
		return sys2->isCollection();

	// Order by hardware
	std::string mf1 = Utils::String::toUpper(sys1->getSystemMetadata().hardwareType);
	std::string mf2 = Utils::String::toUpper(sys2->getSystemMetadata().hardwareType);
	if (mf1 != mf2)
		return mf1.compare(mf2) < 0;

	// Then by name
	std::string name1 = Utils::String::toUpper(sys1->getName());
	std::string name2 = Utils::String::toUpper(sys2->getName());
	return name1.compare(name2) < 0;
}

CollectionSystemManager* CollectionSystemManager::get()
{
	assert(sInstance);
	return sInstance;
}

void CollectionSystemManager::init(Window* window)
{
	deinit();
	sInstance = new CollectionSystemManager(window);
}

void CollectionSystemManager::deinit()
{
	if (sInstance)
	{
		delete sInstance;
		sInstance = nullptr;
	}
}

static std::string getAbsolutePathRoot()
{
	std::string relativeTo = "-portnawak-";

#if WIN32
	std::string romPath = Win32ApiSystem::getEmulatorLauncherPath("roms");
	if (!romPath.empty())
		return Utils::FileSystem::getCanonicalPath(Utils::FileSystem::getParent(romPath));
#else
	relativeTo = "/userdata";
#endif

	return relativeTo;
}

void CollectionSystemManager::saveCustomCollection(SystemData* sys)
{
	std::string name = sys->getName();
	auto games = sys->getRootFolder()->getChildren();	
	bool found = mCustomCollectionSystemsData.find(name) != mCustomCollectionSystemsData.cend();
	if (found) 
	{
		CollectionSystemData sysData = mCustomCollectionSystemsData.at(name);
		if (sysData.needsSave)
		{
			std::string relativeTo = getAbsolutePathRoot();

			std::ofstream configFile;
			configFile.open(getCustomCollectionConfigPath(name));
			for (auto iter = games.cbegin(); iter != games.cend(); ++iter)
			{
				std::string path = (*iter)->getKey();

				// Allow collection items to be portable, relative to home
				path = Utils::FileSystem::createRelativePath(path, relativeTo, true);

				configFile << path << std::endl;
			}

			configFile.close();
		}
	}
	else
	{
		LOG(LogError) << "Couldn't find collection to save! " << name;
	}
}

/* Methods to load all Collections into memory, and handle enabling the active ones */
// loads all Collection Systems
void CollectionSystemManager::loadCollectionSystems()
{
	initAutoCollectionSystems();
	
	CollectionSystemDecl decl = mCollectionSystemDeclsIndex[myCollectionsName];
	mCustomCollectionsBundle = createNewCollectionEntry(decl.name, decl, false);

	initCustomCollectionSystems();
	loadEnabledListFromSettings();
}

// loads settings
void CollectionSystemManager::loadEnabledListFromSettings()
{
	// we parse the auto collection settings list
	std::vector<std::string> autoSelected = Utils::String::split(Settings::getInstance()->getString("CollectionSystemsAuto"), ',', true);

	// iterate the map
	for(auto& item : mAutoCollectionSystemsData)
		item.second.isEnabled = (std::find(autoSelected.cbegin(), autoSelected.cend(), item.first) != autoSelected.cend());

	// we parse the custom collection settings list
	std::vector<std::string> customSelected = Utils::String::split(Settings::getInstance()->getString("CollectionSystemsCustom"), ',', true);

	// iterate the map
	for (auto& item : mCustomCollectionSystemsData)
		item.second.isEnabled = (std::find(customSelected.cbegin(), customSelected.cend(), item.first) != customSelected.cend());
}

// updates enabled system list in System View
void CollectionSystemManager::updateSystemsList()
{
	auto sortMode = Settings::getInstance()->getString("SortSystems");
	bool sortByManufacturer = SystemData::isManufacturerSupported() && sortMode == "manufacturer";
	bool sortByHardware = SystemData::isManufacturerSupported() && sortMode == "hardware";
	bool sortByReleaseDate = SystemData::isManufacturerSupported() && sortMode == "releaseDate";

	// remove all Collection Systems
	removeCollectionsFromDisplayedSystems();

	std::unordered_map<std::string, FileData*> map;
	getAllGamesCollection()->getRootFolder()->createChildrenByFilenameMap(map);

	// add custom enabled ones
	addEnabledCollectionsToDisplayedSystems(&mCustomCollectionSystemsData, &map);

	if (!sortMode.empty() && !sortByManufacturer && !sortByHardware && !sortByReleaseDate)
		std::sort(SystemData::sSystemVector.begin(), SystemData::sSystemVector.end(), systemByAlphaSort);

	if (mCustomCollectionsBundle->getRootFolder()->getChildren().size() > 0)
		SystemData::sSystemVector.push_back(mCustomCollectionsBundle);

	// add auto enabled ones
	addEnabledCollectionsToDisplayedSystems(&mAutoCollectionSystemsData, &map);

	if (!sortMode.empty())
	{
		if (sortByManufacturer)
			std::sort(SystemData::sSystemVector.begin(), SystemData::sSystemVector.end(), systemByManufacurerSort);
		else if (sortByHardware)
			std::sort(SystemData::sSystemVector.begin(), SystemData::sSystemVector.end(), systemByHardwareSort);
		else if (sortByReleaseDate)
			std::sort(SystemData::sSystemVector.begin(), SystemData::sSystemVector.end(), systemByReleaseDate);

		// Move RetroPie / Retrobat system to end
		for (auto sysIt = SystemData::sSystemVector.cbegin(); sysIt != SystemData::sSystemVector.cend(); )
		{
			if ((*sysIt)->getName() == "retropie" || (*sysIt)->getName() == "retrobat")
			{
				SystemData* retroPieSystem = (*sysIt);
				sysIt = SystemData::sSystemVector.erase(sysIt);
				SystemData::sSystemVector.push_back(retroPieSystem);
				break;
			}
			else
				sysIt++;
		}
	}
}

/* Methods to manage collection files related to a source FileData */
// updates all collection files related to the source file
void CollectionSystemManager::refreshCollectionSystems(FileData* file)
{
	if (!file->getSystem()->isGameSystem() || file->getType() != GAME)
		return;

	std::map<std::string, CollectionSystemData> allCollections;
	allCollections.insert(mAutoCollectionSystemsData.cbegin(), mAutoCollectionSystemsData.cend());
	allCollections.insert(mCustomCollectionSystemsData.cbegin(), mCustomCollectionSystemsData.cend());

	for (auto sysDataIt = allCollections.cbegin(); sysDataIt != allCollections.cend(); sysDataIt++)
		updateCollectionSystem(file, sysDataIt->second);
}

void CollectionSystemManager::updateCollectionSystem(FileData* file, CollectionSystemData sysData)
{
	if (!sysData.isPopulated)
		return;

	// collection files use the full path as key, to avoid clashes
	std::string key = file->getFullPath();

	SystemData* curSys = sysData.system;
	FileData* collectionEntry = curSys->getRootFolder()->FindByPath(key);
	FolderData* rootFolder = curSys->getRootFolder();

	std::string name = curSys->getName();

	if (collectionEntry != nullptr)
	{
		// remove from index, so we can re-index metadata after refreshing
		curSys->removeFromIndex(collectionEntry);

		// found and we are removing
		if (name == "favorites" && !file->getFavorite())
		{
			// need to check if still marked as favorite, if not remove
			auto view = ViewController::get()->getGameListView(curSys, false);
			if (view != nullptr)
				view.get()->remove(collectionEntry);
			else
				delete collectionEntry;

			// Send an event when removing from favorites
			ViewController::get()->onFileChanged(file, FILE_METADATA_CHANGED);

			if (view != nullptr)
				view->onFileChanged(collectionEntry, FILE_METADATA_CHANGED);
		}
		else
		{
			// re-index with new metadata
			curSys->addToIndex(collectionEntry);
			ViewController::get()->onFileChanged(collectionEntry, FILE_METADATA_CHANGED);
		}
	}
	else
	{
		// we didn't find it here - we need to check if we should add it
		if (name == "recent" && file->getMetadata(MetaDataId::PlayCount) > "0" && includeFileInAutoCollections(file) ||
			name == "favorites" && file->getFavorite())
		{
			CollectionFileData* newGame = new CollectionFileData(file, curSys);
			rootFolder->addChild(newGame);
			curSys->addToIndex(newGame);
			ViewController::get()->onFileChanged(file, FILE_METADATA_CHANGED);

			auto view = ViewController::get()->getGameListView(curSys, false);
			if (view != nullptr)
				view->onFileChanged(newGame, FILE_METADATA_CHANGED);
		}
	}

	curSys->updateDisplayedGameCount();

	if (name == "recent")
	{
		sortLastPlayed(curSys);
		trimCollectionCount(rootFolder, LAST_PLAYED_MAX);
		ViewController::get()->onFileChanged(rootFolder, FILE_METADATA_CHANGED);
	}
	else
		ViewController::get()->onFileChanged(rootFolder, FILE_SORTED);
}

void CollectionSystemManager::sortLastPlayed(SystemData* system)
{
	if (system->getName() != "recent")
		return;

	FolderData* rootFolder = system->getRootFolder();
	system->setSortId(FileSorts::LASTPLAYED_DESCENDING);

	const FileSorts::SortType& sort = FileSorts::getSortTypes().at(system->getSortId());

	std::vector<FileData*>& childs = (std::vector<FileData*>&) rootFolder->getChildren();
	std::sort(childs.begin(), childs.end(), sort.comparisonFunction);
	if (!sort.ascending)
		std::reverse(childs.begin(), childs.end());
}

void CollectionSystemManager::trimCollectionCount(FolderData* rootFolder, int limit)
{
	SystemData* curSys = rootFolder->getSystem();
	std::shared_ptr<IGameListView> listView = ViewController::get()->getGameListView(curSys, false);
	
	auto& childs = rootFolder->getChildren();
	while ((int)childs.size() > limit)
	{
		CollectionFileData* gameToRemove = (CollectionFileData*)childs.back();
		if (listView != nullptr)
			listView.get()->remove(gameToRemove);
		else
			delete gameToRemove;
	}
}

// deletes all collection files from collection systems related to the source file
void CollectionSystemManager::deleteCollectionFiles(FileData* file)
{
	// collection files use the full path as key, to avoid clashes
	std::string key = file->getFullPath();

	// find games in collection systems
	std::map<std::string, CollectionSystemData> allCollections;
	allCollections.insert(mAutoCollectionSystemsData.cbegin(), mAutoCollectionSystemsData.cend());
	allCollections.insert(mCustomCollectionSystemsData.cbegin(), mCustomCollectionSystemsData.cend());

	for (auto sysDataIt = allCollections.begin(); sysDataIt != allCollections.end(); sysDataIt++)
	{
		if (!sysDataIt->second.isPopulated)
			continue;

		FileData* collectionEntry = (sysDataIt->second.system)->getRootFolder()->FindByPath(key);
		if (collectionEntry == nullptr)
			continue;
		
		sysDataIt->second.needsSave = true;

		SystemData* systemViewToUpdate = getSystemToView(sysDataIt->second.system);
		if (systemViewToUpdate == nullptr)
			continue;

		auto view = ViewController::get()->getGameListView(systemViewToUpdate, false);
		if (view != nullptr)
			view.get()->remove(collectionEntry);		
		else
			delete collectionEntry;		
	}
}

// returns whether the current theme is compatible with Automatic or Custom Collections
bool CollectionSystemManager::isThemeGenericCollectionCompatible(bool genericCustomCollections)
{
	std::vector<std::string> cfgSys = getCollectionThemeFolders(genericCustomCollections);
	for(auto sysIt = cfgSys.cbegin(); sysIt != cfgSys.cend(); sysIt++)
	{
		if(!themeFolderExists(*sysIt))
			return false;
	}
	return true;
}

bool CollectionSystemManager::isThemeCustomCollectionCompatible(std::vector<std::string> stringVector)
{
	if (isThemeGenericCollectionCompatible(true))
		return true;

	// get theme path
	auto themeSets = ThemeData::getThemeSets();
	auto set = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
	if(set != themeSets.cend())
	{
		std::string defaultThemeFilePath = set->second.path + "/theme.xml";
		if (Utils::FileSystem::exists(defaultThemeFilePath))
		{
			return true;
		}
	}

	for(auto sysIt = stringVector.cbegin(); sysIt != stringVector.cend(); sysIt++)
	{
		if(!themeFolderExists(*sysIt))
			return false;
	}
	return true;
}

std::string CollectionSystemManager::getValidNewCollectionName(std::string inName, int index)
{
	std::string name = inName;

	if(index == 0)
	{
		size_t remove = std::string::npos;

		// get valid name
		while((remove = name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-[]() ")) != std::string::npos)
		{
			name.erase(remove, 1);
		}
	}
	else
	{
		name += " (" + std::to_string(index) + ")";
	}

	if(name == "")
	{
		name = "New Collection";
	}

	if(name != inName)
	{
		LOG(LogInfo) << "Had to change name, from: " << inName << " to: " << name;
	}

	// get used systems in es_systems.cfg
	std::vector<std::string> systemsInUse = getSystemsFromConfig();
	// get folders assigned to custom collections
	std::vector<std::string> autoSys = getCollectionThemeFolders(false);
	// get folder assigned to custom collections
	std::vector<std::string> customSys = getCollectionThemeFolders(true);
	// get folders assigned to user collections
	std::vector<std::string> userSys = getUserCollectionThemeFolders();
	// add them all to the list of systems in use
	systemsInUse.insert(systemsInUse.cend(), autoSys.cbegin(), autoSys.cend());
	systemsInUse.insert(systemsInUse.cend(), customSys.cbegin(), customSys.cend());
	systemsInUse.insert(systemsInUse.cend(), userSys.cbegin(), userSys.cend());
	for(auto sysIt = systemsInUse.cbegin(); sysIt != systemsInUse.cend(); sysIt++)
	{
		if (*sysIt == name)
		{
			if(index > 0) {
				name = name.substr(0, name.size()-4);
			}
			return getValidNewCollectionName(name, index+1);
		}
	}
	// if it matches one of the custom collections reserved names
	if (mCollectionSystemDeclsIndex.find(name) != mCollectionSystemDeclsIndex.cend())
		return getValidNewCollectionName(name, index+1);
	return name;
}

bool CollectionSystemManager::inInCustomCollection(FileData* file, const std::string collectionName)
{
	auto data = mCustomCollectionSystemsData.find(collectionName);
	if (data == mCustomCollectionSystemsData.cend())
		return false;

	if (!data->second.isPopulated)
		populateCustomCollection(&data->second);
	
	std::string key = file->getFullPath();
	FolderData* rootFolder = data->second.system->getRootFolder();
	FileData* collectionEntry = rootFolder->FindByPath(key);
	return collectionEntry != nullptr;
}

// adds or removes a game from a specific collection
bool CollectionSystemManager::toggleGameInCollection(FileData* file, const std::string collectionName)
{
	if (file->getType() != GAME)
		return false;

	CollectionSystemData* collectionSystemData = nullptr;

	if (collectionName != "Favorites")
	{
		if (mCustomCollectionSystemsData.find(collectionName) == mCustomCollectionSystemsData.cend())
			return false;

		collectionSystemData = &(mCustomCollectionSystemsData.at(collectionName));

		if (collectionSystemData != nullptr && !collectionSystemData->isPopulated)
			populateCustomCollection(collectionSystemData);
	}

	bool adding = true;
	std::string name = file->getName();

	if (collectionSystemData != nullptr) // Custom collection
	{			
		SystemData* sysData = collectionSystemData->system;

		collectionSystemData->needsSave = true;

		std::string key = file->getFullPath();
		FolderData* rootFolder = sysData->getRootFolder();
		FileData* collectionEntry = rootFolder->FindByPath(key);

		std::string name = sysData->getName();

		SystemData* systemViewToUpdate = getSystemToView(sysData);
		if (collectionEntry != nullptr) 
		{
			adding = false;
			// remove from index
			sysData->removeFromIndex(collectionEntry);
			// remove from bundle index as well, if needed
			if (systemViewToUpdate != sysData)
				systemViewToUpdate->removeFromIndex(collectionEntry);

			auto view = ViewController::get()->getGameListView(systemViewToUpdate, false);
			if (view != nullptr)
				view.get()->remove(collectionEntry);
			else
				delete collectionEntry;
		}
		else
		{
			// we didn't find it here, we should add it
			CollectionFileData* newGame = new CollectionFileData(file, sysData);
			rootFolder->addChild(newGame);
			sysData->addToIndex(newGame);

			auto view = ViewController::get()->getGameListView(systemViewToUpdate, false);
			if (view != nullptr)
				view->onFileChanged(newGame, FILE_METADATA_CHANGED);

			ViewController::get()->onFileChanged(systemViewToUpdate->getRootFolder(), FILE_SORTED);

			// add to bundle index as well, if needed
			if (systemViewToUpdate != sysData)
				systemViewToUpdate->addToIndex(newGame);
		}
		updateCollectionFolderMetadata(sysData);
	}
	else
	{
		SystemData* sysData = file->getSourceFileData()->getSystem();
		sysData->removeFromIndex(file);
			
		MetaDataList* md = &file->getSourceFileData()->getMetadata();
			
		std::string value = md->get(MetaDataId::Favorite);
		if (value != "true")
			md->set(MetaDataId::Favorite, "true");
		else
		{
			adding = false;
			md->set(MetaDataId::Favorite, "false");
		}

		sysData->addToIndex(file);
		saveToGamelistRecovery(file);

		refreshCollectionSystems(file->getSourceFileData());

		SystemData* systemViewToUpdate = getSystemToView(sysData);
		if (systemViewToUpdate != NULL)
		{
			// Notify the current gamelist
			ViewController::get()->onFileChanged(file, FILE_METADATA_CHANGED);

			auto view = ViewController::get()->getGameListView(systemViewToUpdate, false);
			if (view != nullptr)
				view->onFileChanged(file, FILE_METADATA_CHANGED);
		}		
	}

	char trstring[1024];

	if (adding)
		snprintf(trstring, 1024, _("Added '%s' to '%s'").c_str(), Utils::String::removeParenthesis(name).c_str(), Utils::String::toUpper(collectionName).c_str()); // batocera
	else
		snprintf(trstring, 1024, _("Removed '%s' from '%s'").c_str(), Utils::String::removeParenthesis(name).c_str(), Utils::String::toUpper(collectionName).c_str()); // batocera		  

	mWindow->displayNotificationMessage(trstring, 4000);
	return true;
}

SystemData* CollectionSystemManager::getSystemToView(SystemData* sys)
{
	SystemData* systemToView = sys;
	FileData* rootFolder = sys->getRootFolder();

	FolderData* bundleRootFolder = mCustomCollectionsBundle->getRootFolder();

	bool sysFoundInBundle = bundleRootFolder->FindByPath(rootFolder->getKey()) != nullptr;
	if (sysFoundInBundle && sys->isCollection())
	{
		systemToView = mCustomCollectionsBundle;
	}
	return systemToView;
}

/* Handles loading a collection system, creating an empty one, and populating on demand */
// loads Automatic Collection systems (All, Favorites, Last Played)
void CollectionSystemManager::initAutoCollectionSystems()
{
	for(std::map<std::string, CollectionSystemDecl>::const_iterator it = mCollectionSystemDeclsIndex.cbegin() ; it != mCollectionSystemDeclsIndex.cend() ; it++ )
	{
		CollectionSystemDecl sysDecl = it->second;
		if (!sysDecl.isCustom)
		{
			createNewCollectionEntry(sysDecl.name, sysDecl);
		}
	}
}

// this may come in handy if at any point in time in the future we want to
// automatically generate metadata for a folder
void CollectionSystemManager::updateCollectionFolderMetadata(SystemData* sys)
{
	FolderData* rootFolder = sys->getRootFolder();

	std::string desc = _("This collection is empty.");
	std::string rating = "0";
	std::string players = "1";
	std::string releasedate = "N/A";
	std::string developer = _("None");
	std::string genre = _("None");
	std::string video = "";
	std::string thumbnail = "";
	std::string image = "";

	auto games = rootFolder->getChildren();
	char trstring[1024];

	if (games.size() > 0)
	{
		std::string games_list = "";
		int games_counter = 0;
		for (auto iter = games.cbegin(); iter != games.cend(); ++iter)
		{
			games_counter++;
			if (games_counter == 5)
				break;

			FileData* file = *iter;

			std::string new_rating = file->getMetadata(MetaDataId::Rating);
			std::string new_releasedate = file->getMetadata(MetaDataId::ReleaseDate);
			std::string new_developer = file->getMetadata(MetaDataId::Developer);
			std::string new_genre = file->getMetadata(MetaDataId::Genre);
			std::string new_players = file->getMetadata(MetaDataId::Players);

			rating = (new_rating > rating ? (new_rating != "" ? new_rating : rating) : rating);
			players = (new_players > players ? (new_players != "" ? new_players : players) : players);
			releasedate = (new_releasedate < releasedate ? (new_releasedate != "" ? new_releasedate : releasedate) : releasedate);
			// TRANSLATION: number of developpers / various developpers
			developer = (developer == _("None") ? new_developer : (new_developer != developer ? _("Various") : new_developer));
			genre = (genre == _("None") ? new_genre : (new_genre != genre ? _("Various") : new_genre));

			switch (games_counter)
			{
			case 2:
			case 3:
			case 4:
				games_list += "\n";
			case 1:
				games_list += "- " + file->getName();
				break;
			}			
		}

		games_list = "\n" + games_list;
		games_counter = games.size();

		snprintf(trstring, 1024, ngettext(
			"This collection contains %i game, including :%s",
			"This collection contains %i games, including :%s", games_counter), games_counter, games_list.c_str());

		desc = trstring;

		FileData* randomGame = sys->getRandomGame();
		if (randomGame != nullptr) { // batocera
			video = randomGame->getVideoPath();
			thumbnail = randomGame->getThumbnailPath();
			image = randomGame->getImagePath();
		}
	}
	
	rootFolder->setMetadata(MetaDataId::Desc, desc);
	rootFolder->setMetadata(MetaDataId::Rating, rating);
	rootFolder->setMetadata(MetaDataId::Players, players);
	rootFolder->setMetadata(MetaDataId::Genre, genre);
	rootFolder->setMetadata(MetaDataId::ReleaseDate, releasedate);
	rootFolder->setMetadata(MetaDataId::Developer, developer);
	rootFolder->setMetadata(MetaDataId::Video, video);
	rootFolder->setMetadata(MetaDataId::Thumbnail, thumbnail);
	rootFolder->setMetadata(MetaDataId::Image, image);
	rootFolder->setMetadata(MetaDataId::KidGame, "false");
	rootFolder->setMetadata(MetaDataId::Hidden, "false");
	rootFolder->setMetadata(MetaDataId::Favorite, "false");
}

void CollectionSystemManager::initCustomCollectionSystems()
{
	for (auto name : getCollectionsFromConfigFolder())
		addNewCustomCollection(name, false);
}

SystemData* CollectionSystemManager::getArcadeCollection()
{
	CollectionSystemData* allSysData = &mAutoCollectionSystemsData["arcade"];
	if (!allSysData->isPopulated)
		populateAutoCollection(allSysData);

	return allSysData->system;
}

SystemData* CollectionSystemManager::getAllGamesCollection()
{
	CollectionSystemData* allSysData = &mAutoCollectionSystemsData["all"];
	if (!allSysData->isPopulated)
		populateAutoCollection(allSysData);

	return allSysData->system;
}

SystemData* CollectionSystemManager::addNewCustomCollection(std::string name, bool needSave)
{
	CollectionSystemDecl decl = mCollectionSystemDeclsIndex[myCollectionsName];
	decl.themeFolder = name;
	decl.name = name;
	decl.longName = name;	
	return createNewCollectionEntry(name, decl, true, needSave);
}

// creates a new, empty Collection system, based on the name and declaration
SystemData* CollectionSystemManager::createNewCollectionEntry(std::string name, CollectionSystemDecl sysDecl, bool index, bool needSave)
{	
	SystemMetadata md;
	md.name = name;
	md.fullName = sysDecl.longName;
	md.themeFolder = sysDecl.themeFolder;
	md.manufacturer = "Collections";
	md.hardwareType = sysDecl.isCustom ? "custom collection" : "auto collection";
	md.releaseYear = 0;
	
	SystemData* newSys = new SystemData(md, mCollectionEnvData, NULL, true); // batocera

	CollectionSystemData newCollectionData;
	newCollectionData.system = newSys;
	newCollectionData.decl = sysDecl;
	newCollectionData.isEnabled = false;
	newCollectionData.isPopulated = false;
	newCollectionData.needsSave = false;
	newCollectionData.filteredIndex = nullptr;

	if (index)
	{
		if (!sysDecl.isCustom)
		{
			mAutoCollectionSystemsData[name] = newCollectionData;
		}
		else
		{
			std::string indexPath = getFilteredCollectionPath(name);
			if (Utils::FileSystem::exists(indexPath))
				newCollectionData.filteredIndex = new CollectionFilter();
			else
				newCollectionData.needsSave = needSave;

			mCustomCollectionSystemsData[name] = newCollectionData;
		}
	}

	return newSys;
}

// populates an Automatic Collection System
void CollectionSystemManager::populateAutoCollection(CollectionSystemData* sysData)
{
	SystemData* newSys = sysData->system;
	CollectionSystemDecl sysDecl = sysData->decl;
	FolderData* rootFolder = newSys->getRootFolder(); 

	bool hiddenSystemsShowGames = Settings::getInstance()->getBool("HiddenSystemsShowGames");
	auto hiddenSystems = Utils::String::split(Settings::getInstance()->getString("HiddenSystems"), ';');
	
	for(auto& system : SystemData::sSystemVector)
	{
		// we won't iterate all collections
		if (!system->isGameSystem() || system->isCollection())
			continue;
		
		if (!hiddenSystemsShowGames && std::find(hiddenSystems.cbegin(), hiddenSystems.cend(), system->getName()) != hiddenSystems.cend())
			continue;

		std::vector<PlatformIds::PlatformId> platforms = system->getPlatformIds();
		bool isArcade = std::find(platforms.begin(), platforms.end(), PlatformIds::ARCADE) != platforms.end();

		std::vector<std::string> hiddenExts;
		for (auto ext : Utils::String::split(Settings::getInstance()->getString(system->getName() + ".HiddenExt"), ';'))
			hiddenExts.push_back("." + Utils::String::toLower(ext));

		std::vector<FileData*> files = system->getRootFolder()->getFilesRecursive(GAME);
		for(auto& game : files)
		{
			if (system->isGroupSystem() && game->getSystem() != system)
				continue;

			bool include = includeFileInAutoCollections(game);
			if (!include)
				continue;

			if (hiddenExts.size() > 0 && game->getType() == GAME)
			{
				std::string extlow = Utils::String::toLower(Utils::FileSystem::getExtension(game->getFileName()));
				if (std::find(hiddenExts.cbegin(), hiddenExts.cend(), extlow) != hiddenExts.cend())
					continue;
			}

			switch(sysDecl.type) 
			{
				case AUTO_ALL_GAMES:
#ifdef _ENABLEEMUELEC
				include = !(game->getSystemName() == "setup") && !(game->getSystemName() == "imageviewer") && !(game->getSystemName() == "mediaplayer");
#endif
					break;
				case AUTO_VERTICALARCADE:
					include = game->isVerticalArcadeGame();
					break;
				case AUTO_RETROACHIEVEMENTS:
					include = game->hasCheevos();
					break;
				case AUTO_LAST_PLAYED:
					include = game->getMetadata(MetaDataId::PlayCount) > "0";
					break;
				case AUTO_NEVER_PLAYED:
					include = !(game->getMetadata(MetaDataId::PlayCount) > "0");
					break;					
				case AUTO_FAVORITES:
					// we may still want to add files we don't want in auto collections in "favorites"
					include = game->getFavorite();
					break;
				case AUTO_ARCADE:
					include = isArcade;
					break;
				case AUTO_AT2PLAYERS: // batocera
				case AUTO_AT4PLAYERS:
					{
						std::string players = game->getMetadata(MetaDataId::Players);
						if (players.empty())
							include = false;
						else
						{
							int min = -1;

							auto split = players.rfind("+");
							if (split != std::string::npos)
								players = Utils::String::replace(players, "+", "-999");

							split = players.rfind("-");
							if (split != std::string::npos)
							{
								min = atoi(players.substr(0, split).c_str());
								players = players.substr(split + 1);
							}

							int max = atoi(players.c_str());
							int val = (sysDecl.type == AUTO_AT2PLAYERS ? 2 : 4);
							include = min <= 0 ? (val == max) : (min <= val && val <= max);
						}
					}
					break;
				default:
					if (!sysDecl.isCustom && !sysDecl.displayIfEmpty)
						include = isArcade && game->getMetadata(MetaDataId::ArcadeSystemName) == sysDecl.themeFolder;

					break;				
			}
							    
			if (include) 
			{
				CollectionFileData* newGame = new CollectionFileData(game, newSys);
				rootFolder->addChild(newGame);
				newSys->addToIndex(newGame);
			}			
		}
	}

	if (sysDecl.type == AUTO_LAST_PLAYED)
	{
		sortLastPlayed(newSys);
		trimCollectionCount(rootFolder, LAST_PLAYED_MAX);
	}

	sysData->isPopulated = true;
}

// populates a Custom Collection System
void CollectionSystemManager::populateCustomCollection(CollectionSystemData* sysData, std::unordered_map<std::string, FileData*>* pMap)
{
	SystemData* newSys = sysData->system;
	sysData->isPopulated = true;
	CollectionSystemDecl sysDecl = sysData->decl;

	auto hiddenSystems = Utils::String::split(Settings::getInstance()->getString("HiddenSystems"), ';');

	if (sysData->filteredIndex != nullptr)
	{
		sysData->filteredIndex->resetIndex();

		FolderData* rootFolder = newSys->getRootFolder();
		rootFolder->clear();

		std::string indexPath = getFilteredCollectionPath(newSys->getName());
		if (sysData->filteredIndex->load(indexPath))
		{
			FolderData* folder = getAllGamesCollection()->getRootFolder();

			std::vector<FileData*> games = folder->getFilesRecursive(GAME);
			for (auto game : games)
			{
                if (game->getSystemName() != "mplayer") { //emuelec
                    if (sysData->filteredIndex->isSystemSelected(game->getSystemName()))
                        sysData->filteredIndex->addToIndex(game);

				if (sysData->filteredIndex->showFile(game))
				{
					if (std::find(hiddenSystems.cbegin(), hiddenSystems.cend(), game->getSystemName()) != hiddenSystems.cend())
						continue;

					CollectionFileData* newGame = new CollectionFileData(game, newSys);
					rootFolder->addChild(newGame);
				}
			}
		}
    }
		updateCollectionFolderMetadata(newSys);
		return;
	}

	std::string path = getCustomCollectionConfigPath(newSys->getName());

	if(!Utils::FileSystem::exists(path))
	{
		LOG(LogInfo) << "Couldn't find custom collection config file at " << path;
		return;
	}
	LOG(LogInfo) << "Loading custom collection config file at " << path;

	FolderData* rootFolder = newSys->getRootFolder();

	// get Configuration for this Custom System
	std::ifstream input(path);

	FolderData* folder = getAllGamesCollection()->getRootFolder();

	std::unordered_map<std::string, FileData*> map;

	if (pMap == nullptr && folder != nullptr)
	{
		folder->createChildrenByFilenameMap(map);
		pMap = &map;
	}

	std::string relativeTo = getAbsolutePathRoot();

	// iterate list of files in config file
	for(std::string gameKey; getline(input, gameKey); )
	{
		if (gameKey.empty() || gameKey[0] == '0' || gameKey[0] == '#')
			continue;

		// if item is portable relative to homepath
		gameKey = Utils::FileSystem::resolveRelativePath(Utils::String::trim(gameKey), relativeTo, true);

		std::unordered_map<std::string, FileData*>::const_iterator it = pMap->find(gameKey);
		if (it != pMap->cend())
		{
			if (std::find(hiddenSystems.cbegin(), hiddenSystems.cend(), it->second->getName()) != hiddenSystems.cend())
				continue;

			CollectionFileData* newGame = new CollectionFileData(it->second, newSys);
			rootFolder->addChild(newGame);
			newSys->addToIndex(newGame);
		}
		else
		{
			LOG(LogInfo) << "Couldn't find game referenced at '" << gameKey << "' for system config '" << path << "'";
		}
	}
	updateCollectionFolderMetadata(newSys);
}

/* Handle System View removal and insertion of Collections */
void CollectionSystemManager::removeCollectionsFromDisplayedSystems()
{
	// remove all Collection Systems
	for(auto sysIt = SystemData::sSystemVector.cbegin(); sysIt != SystemData::sSystemVector.cend(); )
	{
		if ((*sysIt)->isCollection())
			sysIt = SystemData::sSystemVector.erase(sysIt);
		else
			sysIt++;
	}

	if (mCustomCollectionsBundle == nullptr)
		return;

	// remove all custom collections in bundle
	// this should not delete the objects from memory!
	FolderData* customRoot = mCustomCollectionsBundle->getRootFolder();
	std::vector<FileData*> mChildren = customRoot->getChildren();
	for(auto it = mChildren.cbegin(); it != mChildren.cend(); it++)
		customRoot->removeChild(*it);

	// clear index
	mCustomCollectionsBundle->resetIndex();
	// remove view so it's re-created as needed
	ViewController::get()->removeGameListView(mCustomCollectionsBundle);
}

void CollectionSystemManager::addEnabledCollectionsToDisplayedSystems(std::map<std::string, CollectionSystemData>* colSystemData, std::unordered_map<std::string, FileData*>* pMap)
{
	if (Settings::getInstance()->getBool("ThreadedLoading"))
	{
		std::vector<CollectionSystemData*> collectionsToPopulate;
		for (auto it = colSystemData->begin(); it != colSystemData->end(); it++)
			if (it->second.isEnabled && !it->second.isPopulated)
				collectionsToPopulate.push_back(&(it->second));

		if (collectionsToPopulate.size() > 1)
		{
			getAllGamesCollection();

			Utils::ThreadPool pool;

			for (auto collection : collectionsToPopulate)
			{
				if (collection->decl.isCustom)
					pool.queueWorkItem([this, collection, pMap] { populateCustomCollection(collection, pMap); });
				else
					pool.queueWorkItem([this, collection, pMap] { populateAutoCollection(collection); });
			}

			pool.wait();
		}
	}

	// add auto enabled ones
	for (auto it = colSystemData->begin(); it != colSystemData->end(); it++)
	{
		if (!it->second.isEnabled)
			continue;

		// check if populated, otherwise populate
		if (!it->second.isPopulated)
		{
			if (it->second.decl.isCustom)
				populateCustomCollection(&(it->second), pMap);
			else
				populateAutoCollection(&(it->second));
		}

		// check if it has its own view
		if (!it->second.decl.isCustom || themeFolderExists(it->first) || !Settings::getInstance()->getBool("UseCustomCollectionsSystem")) // batocera
		{
			if (it->second.decl.displayIfEmpty || it->second.system->getRootFolder()->getChildren().size() > 0)
			{
				// exists theme folder, or we chose not to bundle it under the custom-collections system
				// so we need to create a view
				if (it->second.isEnabled)
					SystemData::sSystemVector.push_back(it->second.system);
			}
		}
		else
		{
			FileData* newSysRootFolder = it->second.system->getRootFolder();
			mCustomCollectionsBundle->getRootFolder()->addChild(newSysRootFolder);

			auto idx = it->second.system->getIndex(false);
			if (idx != nullptr)
				mCustomCollectionsBundle->getIndex(true)->importIndex(idx);
		}
	}
}

/* Auxiliary methods to get available custom collection possibilities */
std::vector<std::string> CollectionSystemManager::getSystemsFromConfig()
{
	std::vector<std::string> systems;
	std::string path = SystemData::getConfigPath(false);

	if(!Utils::FileSystem::exists(path))
	{
		return systems;
	}

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(path.c_str());

	if(!res)
	{
		return systems;
	}

	//actually read the file
	pugi::xml_node systemList = doc.child("systemList");

	if(!systemList)
	{
		return systems;
	}

	for(pugi::xml_node system = systemList.child("system"); system; system = system.next_sibling("system"))
	{
		// theme folder
		std::string themeFolder = system.child("theme").text().get();
		systems.push_back(themeFolder);
	}
	std::sort(systems.begin(), systems.end());
	return systems;
}

// gets all folders from the current theme path
std::vector<std::string> CollectionSystemManager::getSystemsFromTheme()
{
	std::vector<std::string> systems;

	auto themeSets = ThemeData::getThemeSets();
	if(themeSets.empty())
	{
		// no theme sets available
		return systems;
	}

	std::map<std::string, ThemeSet>::const_iterator set = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
	if(set == themeSets.cend())
	{
		// currently selected theme set is missing, so just pick the first available set
		set = themeSets.cbegin();
		Settings::getInstance()->setString("ThemeSet", set->first);
	}

	std::string themePath = set->second.path;

	if (Utils::FileSystem::exists(themePath))
	{
		Utils::FileSystem::stringList dirContent = Utils::FileSystem::getDirContent(themePath);

		for (Utils::FileSystem::stringList::const_iterator it = dirContent.cbegin(); it != dirContent.cend(); ++it)
		{
			if (Utils::FileSystem::isDirectory(*it))
			{
				//... here you have a directory
				std::string folder = *it;
				folder = folder.substr(themePath.size()+1);

				if(Utils::FileSystem::exists(set->second.getThemePath(folder)))
				{
					systems.push_back(folder);
				}
			}
		}
	}
	std::sort(systems.begin(), systems.end());
	return systems;
}

// returns the unused folders from current theme path
std::vector<std::string> CollectionSystemManager::getUnusedSystemsFromTheme()
{
	// get used systems in es_systems.cfg
	std::vector<std::string> systemsInUse = getSystemsFromConfig();
	// get available folders in theme
	std::vector<std::string> themeSys = getSystemsFromTheme();
	// get folders assigned to custom collections
	std::vector<std::string> autoSys = getCollectionThemeFolders(false);
	// get folder assigned to custom collections
	std::vector<std::string> customSys = getCollectionThemeFolders(true);
	// get folders assigned to user collections
	std::vector<std::string> userSys = getUserCollectionThemeFolders();
	// add them all to the list of systems in use
	systemsInUse.insert(systemsInUse.cend(), autoSys.cbegin(), autoSys.cend());
	systemsInUse.insert(systemsInUse.cend(), customSys.cbegin(), customSys.cend());
	systemsInUse.insert(systemsInUse.cend(), userSys.cbegin(), userSys.cend());

	for(auto sysIt = themeSys.cbegin(); sysIt != themeSys.cend(); )
	{
		if (std::find(systemsInUse.cbegin(), systemsInUse.cend(), *sysIt) != systemsInUse.cend())
		{
			sysIt = themeSys.erase(sysIt);
		}
		else
		{
			sysIt++;
		}
	}
	return themeSys;
}

// returns which collection config files exist in the user folder
std::vector<std::string> CollectionSystemManager::getCollectionsFromConfigFolder()
{
	std::vector<std::string> systems;
	std::string configPath = getCollectionsFolder();

	if (Utils::FileSystem::exists(configPath))
	{
		Utils::FileSystem::stringList dirContent = Utils::FileSystem::getDirContent(configPath);
		for (Utils::FileSystem::stringList::const_iterator it = dirContent.cbegin(); it != dirContent.cend(); ++it)
		{
			if (Utils::FileSystem::isRegularFile(*it))
			{
				// it's a file
				std::string filename = Utils::FileSystem::getFileName(*it);

				// need to confirm filename matches config format
				if (filename != "custom-.cfg" && Utils::String::startsWith(filename, "custom-") && Utils::String::endsWith(filename, ".cfg"))
				{
					filename = filename.substr(7, filename.size()-11);
					systems.push_back(filename);
				}
				else if (Utils::FileSystem::getExtension(filename) ==  ".xcc")
				{
					filename = Utils::FileSystem::getStem(filename);
					systems.push_back(filename);
				}
				else
					LOG(LogInfo) << "Found non-collection config file in collections folder: " << filename;				
			}
		}
	}
	return systems;
}

// returns the theme folders for Automatic Collections (All, Favorites, Last Played) or generic Custom Collections folder
std::vector<std::string> CollectionSystemManager::getCollectionThemeFolders(bool custom)
{
	std::vector<std::string> systems;
	for(std::map<std::string, CollectionSystemDecl>::const_iterator it = mCollectionSystemDeclsIndex.cbegin() ; it != mCollectionSystemDeclsIndex.cend() ; it++ )
	{
		CollectionSystemDecl sysDecl = it->second;
		if (sysDecl.isCustom == custom)
		{
			systems.push_back(sysDecl.themeFolder);
		}
	}
	return systems;
}

// returns the theme folders in use for the user-defined Custom Collections
std::vector<std::string> CollectionSystemManager::getUserCollectionThemeFolders()
{
	std::vector<std::string> systems;

	for(auto& item : mCustomCollectionSystemsData)
		systems.push_back(item.second.decl.themeFolder);

	return systems;
}

// returns whether a specific folder exists in the theme
bool CollectionSystemManager::themeFolderExists(std::string folder)
{
	std::vector<std::string> themeSys = getSystemsFromTheme();
	return std::find(themeSys.cbegin(), themeSys.cend(), folder) != themeSys.cend();
}

bool CollectionSystemManager::includeFileInAutoCollections(FileData* file)
{
	// we exclude non-game files from collections (i.e. "kodi", entries from non-game systems)
	// if/when there are more in the future, maybe this can be a more complex method, with a proper list
	// but for now a simple string comparison is more performant
	if (file->getName() != "kodi" && file->getSystem()->isGameSystem() && !file->getSystem()->hasPlatformId(PlatformIds::PLATFORM_IGNORE))
		return true;

	return false;
}

std::string getFilteredCollectionPath(std::string collectionName)
{
	return getCollectionsFolder() + "/" + collectionName + ".xcc";
}

std::string getCustomCollectionConfigPath(std::string collectionName)
{
	return getCollectionsFolder() + "/custom-" + collectionName + ".cfg";
}

std::string getCollectionsFolder()
{
	return Utils::FileSystem::getGenericPath(Utils::FileSystem::getEsConfigPath() + "/collections");
}

bool CollectionSystemManager::isCustomCollection(const std::string collectionName)
{
	auto data = mCustomCollectionSystemsData.find(collectionName);
	if (data == mCustomCollectionSystemsData.cend())
		return false;

	return data->second.decl.isCustom;
}

bool CollectionSystemManager::isDynamicCollection(const std::string collectionName)
{
	auto data = mCustomCollectionSystemsData.find(collectionName);
	if (data == mCustomCollectionSystemsData.cend())
		return false;

	return data->second.decl.isCustom && data->second.filteredIndex != nullptr;
}

void CollectionSystemManager::reloadCollection(const std::string collectionName, bool repopulateGamelist)
{
	auto autoc = mAutoCollectionSystemsData.find(collectionName);
	if (autoc != mAutoCollectionSystemsData.cend())
	{
		if (repopulateGamelist)
		{
			for (auto system : SystemData::sSystemVector)
			{
				if (system->isCollection() && system->getName() == collectionName)
				{
					system->updateDisplayedGameCount();

					auto view = ViewController::get()->getGameListView(system, false);
					if (view != nullptr)
						view->repopulate();
				}
			}
		}

		return;
	}
	
	auto data = mCustomCollectionSystemsData.find(collectionName);
	if (data == mCustomCollectionSystemsData.cend())
		return;

	if (data->second.filteredIndex == nullptr)
		return;

	data->second.isPopulated = false;
	populateCustomCollection(&data->second);

	if (!repopulateGamelist)
		return;

	auto bundle = CollectionSystemManager::get()->getCustomCollectionsBundle();
	for (auto ff : bundle->getRootFolder()->getChildren())
	{
		if (ff->getType() == FOLDER && ff->getName() == collectionName)
		{			
			auto view = ViewController::get()->getGameListView(bundle, false);
			if (view != nullptr)
			{
				ViewController::get()->reloadGameListView(bundle);

				ISimpleGameListView* sgview = dynamic_cast<ISimpleGameListView*>(view.get());
				if (sgview != nullptr)
					sgview->moveToFolder((FolderData*)ff);
			}

			return;
		}
	}

	for (auto system : SystemData::sSystemVector)
	{
		if (system->isCollection() && system->getName() == collectionName)
		{
			auto view = ViewController::get()->getGameListView(system, false);					
			if (view != nullptr)
				view->repopulate();

			return;
		}
	}
}

bool CollectionSystemManager::deleteCustomCollection(CollectionSystemData* data)
{
	std::string name = data->system->getName();

	std::string path = getCollectionsFolder() + "/" + name + ".xcc";
	if (!Utils::FileSystem::exists(path))
		path = getCollectionsFolder() + "/custom-" + name + ".cfg";

	if (!Utils::FileSystem::exists(path))
		return false;

	std::vector<std::string> customSelected = Utils::String::split(Settings::getInstance()->getString("CollectionSystemsCustom"), ',', true);

	auto idx = std::find(customSelected.begin(), customSelected.end(), name);
	if (idx != customSelected.cend())
	{
		customSelected.erase(idx);
		Settings::getInstance()->setString("CollectionSystemsCustom", Utils::String::vectorToCommaString(customSelected));
	}

	auto sys = mCustomCollectionSystemsData.find(name);
	if (sys != mCustomCollectionSystemsData.cend())
		mCustomCollectionSystemsData.erase(sys);

	Utils::FileSystem::removeFile(path);
	return true;
}
