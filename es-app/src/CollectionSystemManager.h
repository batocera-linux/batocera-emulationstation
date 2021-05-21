#pragma once
#ifndef ES_APP_COLLECTION_SYSTEM_MANAGER_H
#define ES_APP_COLLECTION_SYSTEM_MANAGER_H

#include <map>
#include <string>
#include <vector>
#include <unordered_map>

class FileData;
class FolderData;
class SystemData;
class Window;
class CollectionFilter;
struct SystemEnvironmentData;

enum CollectionSystemType
{
	AUTO_ALL_GAMES,
	AUTO_LAST_PLAYED,	
	AUTO_FAVORITES,
	AUTO_AT2PLAYERS,
	AUTO_AT4PLAYERS,
	AUTO_NEVER_PLAYED,
	AUTO_ARCADE,
	AUTO_RETROACHIEVEMENTS,
	AUTO_VERTICALARCADE,
	AUTO_LIGHTGUN,
	CUSTOM_COLLECTION,	
};

struct CollectionSystemDecl
{
	CollectionSystemType type; // type of system
	std::string name;
	std::string longName;
	int			defaultSortId;
	std::string themeFolder;
	bool isCustom;	
    bool displayIfEmpty;

	bool isArcadeSubSystem() { return (int)type >= 1000 && (int)type < 10000; }
	bool isGenreCollection() { return (int)type >= 10000 && (int)type < 20000; }
};

struct CollectionSystemData
{
	SystemData* system;
	CollectionSystemDecl decl;

	CollectionFilter* filteredIndex;
	bool isEnabled;
	bool isPopulated;
	bool needsSave;
};

class CollectionSystemManager
{
public:
	static std::vector<CollectionSystemDecl> getSystemDecls();


	CollectionSystemManager(Window* window);
	~CollectionSystemManager();

	static CollectionSystemManager* get();
	static void init(Window* window);
	static void deinit();
	void saveCustomCollection(SystemData* sys);

	void loadCollectionSystems();
	void loadEnabledListFromSettings();
	void updateSystemsList();

	void refreshCollectionSystems(FileData* file);
	void updateCollectionSystem(FileData* file, CollectionSystemData sysData);
	void deleteCollectionFiles(FileData* file);

	inline std::map<std::string, CollectionSystemData>& getAutoCollectionSystems() { return mAutoCollectionSystemsData; };
	inline std::map<std::string, CollectionSystemData> getCustomCollectionSystems() { return mCustomCollectionSystemsData; };
	inline SystemData* getCustomCollectionsBundle() { return mCustomCollectionsBundle; };
	std::vector<std::string> getUnusedSystemsFromTheme();
	SystemData* addNewCustomCollection(std::string name, bool needSave = true);

	bool isThemeGenericCollectionCompatible(bool genericCustomCollections);
	bool isThemeCustomCollectionCompatible(std::vector<std::string> stringVector);
	std::string getValidNewCollectionName(std::string name, int index = 0);
			
	bool toggleGameInCollection(FileData* file, const std::string collectionName = "");

	SystemData* getSystemToView(SystemData* sys);
	void updateCollectionFolderMetadata(SystemData* sys);

	void reloadCollection(const std::string collectionName, bool repopulateGamelist = true);
    void populateAutoCollection(CollectionSystemData* sysData);
	bool deleteCustomCollection(CollectionSystemData* data);

	bool isCustomCollection(const std::string collectionName);
	bool isDynamicCollection(const std::string collectionName);
	
	bool inInCustomCollection(FileData* file, const std::string collectionName);

	SystemData* getArcadeCollection();

private:
	static CollectionSystemManager* sInstance;
	SystemEnvironmentData* mCollectionEnvData;
	std::map<std::string, CollectionSystemDecl> mCollectionSystemDeclsIndex;
	std::map<std::string, CollectionSystemData> mAutoCollectionSystemsData;
	std::map<std::string, CollectionSystemData> mCustomCollectionSystemsData;
	Window* mWindow;
	
	void initAutoCollectionSystems();
	void initCustomCollectionSystems();
		
	SystemData* getAllGamesCollection();
	SystemData* createNewCollectionEntry(std::string name, CollectionSystemDecl sysDecl, bool index = true, bool needSave = true);

	void populateCustomCollection(CollectionSystemData* sysData, std::unordered_map<std::string, FileData*>* pMap = nullptr);

	void removeCollectionsFromDisplayedSystems();
	void addEnabledCollectionsToDisplayedSystems(std::map<std::string, CollectionSystemData>* colSystemData, std::unordered_map<std::string, FileData*>* pMap);

	std::vector<std::string> getSystemsFromConfig();
	std::vector<std::string> getSystemsFromTheme();
	std::vector<std::string> getCollectionsFromConfigFolder();
	std::vector<std::string> getCollectionThemeFolders(bool custom);
	std::vector<std::string> getUserCollectionThemeFolders();

	void trimCollectionCount(FolderData* rootFolder, int limit);
	void sortLastPlayed(SystemData* system);

	bool themeFolderExists(std::string folder);

	bool includeFileInAutoCollections(FileData* file);

	SystemData* mCustomCollectionsBundle;
};

std::string getCustomCollectionConfigPath(std::string collectionName);
std::string getFilteredCollectionPath(std::string collectionName);
std::string getCollectionsFolder();
bool systemSort(SystemData* sys1, SystemData* sys2);

#endif // ES_APP_COLLECTION_SYSTEM_MANAGER_H
