#pragma once
#ifndef ES_APP_SYSTEM_DATA_H
#define ES_APP_SYSTEM_DATA_H

#include "PlatformId.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <pugixml/src/pugixml.hpp>
#include <unordered_map>
#include <unordered_set>
#include "FileFilterIndex.h"
#include "math/Vector2f.h"

class FileData;
class FolderData;
class ThemeData;
class Window;

class EmulatorFeatures
{
public:
	enum Features
	{
		none = 0,
		ratio = 1,
		rewind = 2,
		smooth = 4,
		shaders = 8,
		pixel_perfect = 16,
		decoration = 32,
		latency_reduction = 64,
		game_translation = 128,
		autosave = 256,
		netplay = 512,
		fullboot = 1024,
		emulated_wiimotes = 2048,
		screen_layout = 4096,
		internal_resolution = 8192,
		videomode = 16384,
		colorization = 32768,

		all = 0x0FFFFFFF
	};

	static Features parseFeatures(const std::string features);
};

struct CoreData
{
	CoreData() 
	{ 
		netplay = false; 
		isDefault = false;
		features = EmulatorFeatures::Features::none;
	}

	std::string name;
	bool netplay;
	bool isDefault;

	std::string customCommandLine;

	EmulatorFeatures::Features features;
};

struct EmulatorData
{
	EmulatorData()
	{
		features = EmulatorFeatures::Features::none;
	}

	std::string name;	
	std::vector<CoreData> cores;

	std::string customCommandLine;

	EmulatorFeatures::Features features;
};

struct SystemEnvironmentData
{
	std::string mStartPath;
	std::vector<std::string> mSearchExtensions;
	std::string mLaunchCommand;
	std::vector<PlatformIds::PlatformId> mPlatformIds;
	std::string mGroup;

	bool isValidExtension(const std::string extension)
	{
		return std::find(mSearchExtensions.cbegin(), mSearchExtensions.cend(), extension) != mSearchExtensions.cend();
	}
};

class SystemData
{
public:
    SystemData(const std::string& name, const std::string& fullName, SystemEnvironmentData* envData, const std::string& themeFolder, std::vector<EmulatorData>* pEmulators, bool CollectionSystem = false, bool groupedSystem = false); // batocera
	~SystemData();

	static bool es_features_loaded;

	inline FolderData* getRootFolder() const { return mRootFolder; };
	inline const std::string& getName() const { return mName; }
	inline const std::string& getFullName() const { return mFullName; }
	inline const std::string& getStartPath() const { return mEnvData->mStartPath; }
	inline const std::vector<std::string>& getExtensions() const { return mEnvData->mSearchExtensions; }
	inline const std::string& getThemeFolder() const { return mThemeFolder; }
	inline SystemEnvironmentData* getSystemEnvData() const { return mEnvData; }
	inline const std::vector<PlatformIds::PlatformId>& getPlatformIds() const { return mEnvData->mPlatformIds; }
	inline bool hasPlatformId(PlatformIds::PlatformId id) { if (!mEnvData) return false; return std::find(mEnvData->mPlatformIds.cbegin(), mEnvData->mPlatformIds.cend(), id) != mEnvData->mPlatformIds.cend(); }

	inline const std::shared_ptr<ThemeData>& getTheme() const { return mTheme; }

	std::string getGamelistPath(bool forWrite) const;
	bool hasGamelist() const;
	std::string getThemePath() const;

	unsigned int getGameCount() const;

	int getDisplayedGameCount();
	void updateDisplayedGameCount();

	static bool hasDirtySystems();
	static void deleteSystems();
	static bool loadConfig(Window* window = nullptr); //Load the system config file at getConfigPath(). Returns true if no errors were encountered. An example will be written if the file doesn't exist.
	static void writeExampleConfig(const std::string& path);
	static std::string getConfigPath(bool forWrite); // if forWrite, will only return ~/.emulationstation/es_systems.cfg, never /etc/emulationstation/es_systems.cfg
	static bool loadFeatures();

	static std::vector<SystemData*> sSystemVector;

	inline std::vector<SystemData*>::const_iterator getIterator() const { return std::find(sSystemVector.cbegin(), sSystemVector.cend(), this); };
	inline std::vector<SystemData*>::const_reverse_iterator getRevIterator() const { return std::find(sSystemVector.crbegin(), sSystemVector.crend(), this); };
	inline bool isCollection() { return mIsCollectionSystem; };
	inline bool isGameSystem() { return mIsGameSystem; };

	inline bool isGroupSystem() { return mIsGroupSystem; };	
	bool isGroupChildSystem();

	bool isVisible();
	
	SystemData* getNext() const;
	SystemData* getPrev() const;
	static SystemData* getRandomSystem();
	FileData* getRandomGame();

	// Load or re-load theme.
	void loadTheme();

	FileFilterIndex* getIndex(bool createIndex);
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
	static bool isNetplayActivated();

	SystemData* getParentGroupSystem();

	static std::unordered_set<std::string> getAllGroupNames();
	static std::unordered_set<std::string> getGroupChildSystemNames(const std::string groupName);

	std::string getDefaultEmulator();
	std::string getDefaultCore(const std::string emulatorName);
	std::string getLaunchCommand(const std::string emulatorName, const std::string coreName);
	std::vector<std::string> getCoreNames(std::string emulatorName);

	bool isCurrentFeatureSupported(EmulatorFeatures::Features feature);
	bool isFeatureSupported(std::string emulatorName, std::string coreName, EmulatorFeatures::Features feature);
	
	bool hasFeatures();
	bool hasEmulatorSelection();

private:
	static void createGroupedSystems();

	size_t mGameListHash;

	bool mIsCollectionSystem;
	bool mIsGameSystem;
	bool mIsGroupSystem;

	std::string mName;
	std::string mFullName;
	SystemEnvironmentData* mEnvData;
	std::string mThemeFolder;
	std::shared_ptr<ThemeData> mTheme;

	void populateFolder(FolderData* folder, std::unordered_map<std::string, FileData*>& fileMap);
	void indexAllGameFilters(const FolderData* folder);
	void setIsGameSystemStatus();
	
	static SystemData* loadSystem(pugi::xml_node system);

	FileFilterIndex* mFilterIndex;

	FolderData* mRootFolder;

	std::vector<EmulatorData> mEmulators;
	
	unsigned int mSortId;
	std::string mViewMode;
	Vector2f    mGridSizeOverride;	

	int			mGameCount;
};

#endif // ES_APP_SYSTEM_DATA_H
