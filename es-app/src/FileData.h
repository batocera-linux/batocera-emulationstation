#pragma once
#ifndef ES_APP_FILE_DATA_H
#define ES_APP_FILE_DATA_H

#include "utils/FileSystemUtil.h"
#include "MetaData.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <stack>
#include "KeyboardMapping.h"
#include "SystemData.h"
#include "SaveState.h"

class Window;
struct SystemEnvironmentData;


enum FileType
{
	GAME = 1,   // Cannot have children.
	FOLDER = 2,
	PLACEHOLDER = 3
};

enum FileChangeType
{
	FILE_ADDED,
	FILE_METADATA_CHANGED,
	FILE_REMOVED,
	FILE_SORTED
};

enum NetPlayMode
{
	DISABLED,
	CLIENT,
	SERVER,	
	SPECTATOR
};

struct LaunchGameOptions
{
	LaunchGameOptions() { netPlayMode = NetPlayMode::DISABLED; }

	int netPlayMode;
	std::string ip;
	int port;

	std::string core;
	std::string netplayClientPassword;

	SaveState	saveStateInfo;
};

class FolderData;

// A tree node that holds information for a file.
class FileData : public IKeyboardMapContainer
{
public:
	FileData(FileType type, const std::string& path, SystemData* system);
	virtual ~FileData();

	virtual const std::string& getName();

	inline FileType getType() const { return mType; }
	
	inline FolderData* getParent() const { return mParent; }
	inline void setParent(FolderData* parent) { mParent = parent; }

	inline SystemData* getSystem() const { return mSystem; }

	virtual const std::string getPath() const;
	const std::string getBreadCrumbPath();

	virtual SystemEnvironmentData* getSystemEnvData() const;

	virtual const std::string getThumbnailPath();
	virtual const std::string getVideoPath();
	virtual const std::string getMarqueePath();
	virtual const std::string getImagePath();

	virtual const std::string getCore(bool resolveDefault = true);
	virtual const std::string getEmulator(bool resolveDefault = true);

	virtual bool isNetplaySupported();

	void setCore(const std::string value);
	void setEmulator(const std::string value);

	virtual const bool getHidden();
	virtual const bool getFavorite();
	virtual const bool getKidGame();
	virtual const bool hasCheevos();

	const std::string getConfigurationName();

	inline bool isPlaceHolder() { return mType == PLACEHOLDER; };	

	virtual std::string getKey();
	const bool isArcadeAsset();
	const bool isVerticalArcadeGame();
	const bool isLightGunGame();
	inline std::string getFullPath() { return getPath(); };
	inline std::string getFileName() { return Utils::FileSystem::getFileName(getPath()); };
	virtual FileData* getSourceFileData();
	virtual std::string getSystemName() const;

	// Returns our best guess at the "real" name for this file (will attempt to perform MAME name translation)
	virtual std::string& getDisplayName();

	// As above, but also remove parenthesis
	std::string getCleanName();

	bool launchGame(Window* window, LaunchGameOptions options = LaunchGameOptions());

	static void resetSettings();
	
	virtual const MetaDataList& getMetadata() const { return mMetadata; }
	virtual MetaDataList& getMetadata() { return mMetadata; }

	void setMetadata(MetaDataList value) { getMetadata() = value; } 
	
	std::string getMetadata(MetaDataId key) { return getMetadata().get(key); }
	void setMetadata(MetaDataId key, const std::string& value) { return getMetadata().set(key, value); }

	void detectLanguageAndRegion(bool overWrite);

	void deleteGameFiles();

	void checkCrc32(bool force = false);
	void checkMd5(bool force = false);
	void checkCheevosHash(bool force = false);

	void importP2k(const std::string& p2k);
	std::string convertP2kFile();
	bool hasP2kFile();

	bool hasKeyboardMapping();
	KeyMappingFile getKeyboardMapping();
	bool isFeatureSupported(EmulatorFeatures::Features feature);
	bool isExtensionCompatible();

	std::string getCurrentGameSetting(const std::string& settingName);

	bool hasContentFiles();
	std::set<std::string> getContentFiles();

private:
	std::string getKeyboardMappingFilePath();
	MetaDataList mMetadata;

protected:	
	FolderData* mParent;
	std::string mPath;
	FileType mType;
	SystemData* mSystem;
	std::string* mDisplayName;
};

class CollectionFileData : public FileData
{
public:
	CollectionFileData(FileData* file, SystemData* system);
	~CollectionFileData();
	const std::string& getName();	
	FileData* getSourceFileData();
	std::string getKey();
	virtual const std::string getPath() const;

	virtual std::string getSystemName() const;
	virtual SystemEnvironmentData* getSystemEnvData() const;

	virtual const MetaDataList& getMetadata() const { return mSourceFileData->getMetadata(); }
	virtual MetaDataList& getMetadata() { return mSourceFileData->getMetadata(); }
	virtual std::string& getDisplayName() { return mSourceFileData->getDisplayName(); }

private:
	// needs to be updated when metadata changes
	FileData* mSourceFileData;
};

class FolderData : public FileData
{
	friend class FileData;
	friend class SystemData;

public:
	FolderData(const std::string& startpath, SystemData* system, bool ownsChildrens = true);
	~FolderData();

	inline bool isVirtualStorage() { return !mOwnsChildrens; }
	inline bool isVirtualFolderDisplay() { return mIsDisplayableAsVirtualFolder && !mOwnsChildrens; }
	
	void enableVirtualFolderDisplay(bool value) { mIsDisplayableAsVirtualFolder = value; };
	bool isVirtualFolderDisplayEnabled() { return mIsDisplayableAsVirtualFolder; };

	FileData* FindByPath(const std::string& path);

	inline const std::vector<FileData*>& getChildren() const { return mChildren; }
	const std::vector<FileData*> getChildrenListToDisplay();
	std::shared_ptr<std::vector<FileData*>> findChildrenListToDisplayAtCursor(FileData* toFind, std::stack<FileData*>& stack);

	std::vector<FileData*> getFilesRecursive(unsigned int typeMask, bool displayedOnly = false, SystemData* system = nullptr, bool includeVirtualStorage = true) const;
	std::vector<FileData*> getFlatGameList(bool displayedOnly, SystemData* system) const;

	void addChild(FileData* file, bool assignParent = true); // Error if mType != FOLDER
	void removeChild(FileData* file); //Error if mType != FOLDER

	void createChildrenByFilenameMap(std::unordered_map<std::string, FileData*>& map);

	FileData* findUniqueGameForFolder();

	void clear();
	void removeVirtualFolders();
	void removeFromVirtualFolders(FileData* game);

private:
	std::vector<FileData*> mChildren;
	bool	mOwnsChildrens;
	bool	mIsDisplayableAsVirtualFolder;
};

#endif // ES_APP_FILE_DATA_H
