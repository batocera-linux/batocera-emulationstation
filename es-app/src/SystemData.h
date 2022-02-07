#pragma once
#ifndef ES_APP_SYSTEM_DATA_H
#define ES_APP_SYSTEM_DATA_H

#include "PlatformId.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <pugixml/src/pugixml.hpp>
#include <unordered_map>
#include <unordered_set>
#include "FileFilterIndex.h"
#include "KeyboardMapping.h"
#include "math/Vector2f.h"
#include "CustomFeatures.h"
#include "utils/VectorEx.h"

class FileData;
class FolderData;
class ThemeData;
class Window;
class SaveStateRepository;

struct GameCountInfo
{
	int visibleGames;
	int totalGames;
	int playCount;
	int favoriteCount;
	int hiddenCount;
	int gamesPlayed;
	std::string mostPlayed;
	std::string lastPlayedDate;
};

struct SystemMetadata
{
	std::string name;
	std::string fullName;
	std::string themeFolder;
	std::string manufacturer;
	int releaseYear;
	std::string hardwareType;
};

struct SystemEnvironmentData
{
	std::string mStartPath;
	std::set<std::string> mSearchExtensions;
	std::string mLaunchCommand;
	std::vector<PlatformIds::PlatformId> mPlatformIds;
	std::string mGroup;

	inline bool isValidExtension(const std::string& extension)
	{
		return mSearchExtensions.find(extension) != mSearchExtensions.cend();
	}
};

class SystemData : public IKeyboardMapContainer
{
public:
    SystemData(const SystemMetadata& type, SystemEnvironmentData* envData, std::vector<EmulatorData>* pEmulators, bool CollectionSystem = false, bool groupedSystem = false, bool withTheme = true, bool loadThemeOnlyIfElements = false);
	~SystemData();

	static SystemData* getSystem(const std::string name);
	static SystemData* getFirstVisibleSystem();

	inline FolderData* getRootFolder() const { return mRootFolder; };
	inline const std::string& getName() const { return mMetadata.name; }
	inline const std::string& getFullName() const { return mMetadata.fullName; }
	inline const std::string& getStartPath() const { return mEnvData->mStartPath; }
	inline const std::set<std::string>& getExtensions() const { return mEnvData->mSearchExtensions; }
	inline const std::string& getThemeFolder() const { return mMetadata.themeFolder; }
	inline SystemEnvironmentData* getSystemEnvData() const { return mEnvData; }
	inline const std::vector<PlatformIds::PlatformId>& getPlatformIds() const { return mEnvData->mPlatformIds; }
	inline bool hasPlatformId(PlatformIds::PlatformId id) { if (!mEnvData) return false; return std::find(mEnvData->mPlatformIds.cbegin(), mEnvData->mPlatformIds.cend(), id) != mEnvData->mPlatformIds.cend(); }
	inline const SystemMetadata& getSystemMetadata() const { return mMetadata; }

	inline const std::shared_ptr<ThemeData>& getTheme() const { return mTheme; }

	std::string getGamelistPath(bool forWrite) const;
	bool hasGamelist() const;
	std::string getThemePath() const;

	unsigned int getGameCount() const;

	GameCountInfo* getGameCountInfo();
	void updateDisplayedGameCount();

	static bool IsManufacturerSupported;
	static bool hasDirtySystems();
	static void deleteSystems();
	static bool loadConfig(Window* window = nullptr); //Load the system config file at getConfigPath(). Returns true if no errors were encountered. An example will be written if the file doesn't exist.	
	static std::string getConfigPath();
	
	bool loadFeatures();

	static VectorEx<SystemData*> sSystemVector;

	inline std::vector<SystemData*>::const_iterator getIterator() const { return std::find(sSystemVector.cbegin(), sSystemVector.cend(), this); };
	inline std::vector<SystemData*>::const_reverse_iterator getRevIterator() const { return std::find(sSystemVector.crbegin(), sSystemVector.crend(), this); };
	inline bool isCollection() { return mIsCollectionSystem; };
	inline bool isGameSystem() { return mIsGameSystem; };

	inline bool isGroupSystem() { return mIsGroupSystem; };	
	bool isGroupChildSystem();

	bool isVisible();
	bool isHidden() { return mHidden; } // This flag is different from !isVisible because it only returns the user setting

	SystemData* getNext() const;
	SystemData* getPrev() const;
	static SystemData* getRandomSystem();
	FileData* getRandomGame();

	// Load or re-load theme.
	void loadTheme();

	FileFilterIndex* getIndex(bool createIndex);
	void setIndex(FileFilterIndex* index) { mFilterIndex = index; }

	void deleteIndex();

	void removeFromIndex(FileData* game) {
		if (mFilterIndex != nullptr) mFilterIndex->removeFromIndex(game);
	};

	void addToIndex(FileData* game) {
		if (mFilterIndex != nullptr) mFilterIndex->addToIndex(game);
	};

	void resetFilters() {
		if (mFilterIndex != nullptr) mFilterIndex->resetFilters();
	};

	void resetIndex() {
		if (mFilterIndex != nullptr) mFilterIndex->resetIndex();
	};
	
	void setUIModeFilters() {
		if (mFilterIndex != nullptr) mFilterIndex->setUIModeFilters();
	}

	std::vector<EmulatorData> getEmulators() { return mEmulators; }

	unsigned int getSortId() const { return mSortId; };
	void setSortId(const unsigned int sortId = 0);

	std::string getSystemViewMode() const { if (mViewMode == "automatic") return ""; else return mViewMode; };
	bool setSystemViewMode(std::string newViewMode, Vector2f gridSizeOverride = Vector2f(0, 0), bool setChanged = true);

	Vector2f getGridSizeOverride();

	void setGamelistHash(size_t size) { mGameListHash = size; }
	size_t getGamelistHash() { return mGameListHash; }

	bool isNetplaySupported();
	bool isCheevosSupported();

	static bool isNetplayActivated();

	SystemData* getParentGroupSystem();

	static std::unordered_set<std::string> getAllGroupNames();
	static std::unordered_set<std::string> getGroupChildSystemNames(const std::string groupName);

	std::string getEmulator(bool resolveDefault = true);
	std::string getCore(bool resolveDefault = true);

	std::string getDefaultEmulator();
	std::string getDefaultCore(const std::string emulatorName = "");

	std::string getLaunchCommand(const std::string emulatorName, const std::string coreName);
	std::vector<std::string> getCoreNames(std::string emulatorName);

	bool isCurrentFeatureSupported(EmulatorFeatures::Features feature);
	bool isFeatureSupported(std::string emulatorName, std::string coreName, EmulatorFeatures::Features feature);
	CustomFeatures getCustomFeatures(std::string emulatorName, std::string coreName);
	std::string		getCompatibleCoreNames(EmulatorFeatures::Features feature);

	bool hasFeatures();
	bool hasEmulatorSelection();

	FileFilterIndex* getFilterIndex() { return mFilterIndex; }

	static SystemData* loadSystem(std::string systemName, bool fullMode = true);
	static std::map<std::string, std::string> getKnownSystemNames();

	bool hasKeyboardMapping();
	KeyMappingFile getKeyboardMapping();

	bool shouldExtractHashesFromArchives();

	bool getShowFilenames();
	bool getShowParentFolder();
	bool getShowFavoritesFirst();
	bool getShowFavoritesIcon();
	bool getShowCheevosIcon();
	int  getShowFlags();
	std::string getFolderViewMode();
	bool getBoolSetting(const std::string& settingName);

	static void resetSettings();

	SaveStateRepository* getSaveStateRepository();

private:
	std::string getKeyboardMappingFilePath();
	static void createGroupedSystems();

	size_t mGameListHash;

	bool mIsCollectionSystem;
	bool mIsGameSystem;
	bool mIsGroupSystem;

	int mIsCheevosSupported;

	SystemMetadata mMetadata;

	SystemEnvironmentData* mEnvData;
	std::shared_ptr<ThemeData> mTheme;

	void populateFolder(FolderData* folder, std::unordered_map<std::string, FileData*>& fileMap);
	void indexAllGameFilters(const FolderData* folder);
	void setIsGameSystemStatus();
	void removeMultiDiskContent(std::unordered_map<std::string, FileData*>& fileMap);

	static SystemData* loadSystem(pugi::xml_node system, bool fullMode = true);
	static void loadAdditionnalConfig(pugi::xml_node& srcSystems);

	FileFilterIndex* mFilterIndex;

	FolderData* mRootFolder;

	std::vector<EmulatorData> mEmulators;
	
	unsigned int mSortId;
	std::string mViewMode;
	Vector2f    mGridSizeOverride;	
	
	std::shared_ptr<bool> mShowFilenames;

	GameCountInfo* mGameCountInfo;
	SaveStateRepository* mSaveRepository;

	bool mHidden;
};

#endif // ES_APP_SYSTEM_DATA_H
