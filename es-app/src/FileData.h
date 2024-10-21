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
#include "BindingManager.h"

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
	SPECTATOR,
	OFFLINE,
};

struct GetFileContext
{
	bool showHiddenFiles;
	bool filterKidGame;
	std::set<std::string> hiddenExtensions;
};

struct LaunchGameOptions
{
	LaunchGameOptions() 
	{ 
		netPlayMode = NetPlayMode::DISABLED; 
		port = 0;
		saveStateInfo = nullptr; 
		isSaveStateInfoTemporary = false; 		  
	}

	int netPlayMode;
	std::string ip;
	int port;
	std::string session;

	std::string core;
	std::string netplayClientPassword;

	SaveState*	saveStateInfo;
	bool isSaveStateInfoTemporary;
};

class FolderData;

// A tree node that holds information for a file.
class FileData : public IKeyboardMapContainer, public IBindable
{
public:
	FileData(FileType type, const std::string& path, SystemData* system);
	virtual ~FileData();

	static FileData* GetRunningGame() { return mRunningGame; }

	virtual const std::string& getName();

	inline FileType getType() const { return mType; }
	
	inline FolderData* getParent() const { return mParent; }
	inline void setParent(FolderData* parent) { mParent = parent; }

	inline SystemData* getSystem() const { return mSystem; }

	virtual const std::string getPath() const;
	const std::string getBreadCrumbPath();

	virtual SystemEnvironmentData* getSystemEnvData() const;

	virtual const std::string getThumbnailPath(bool fallbackWithImage = true);
	virtual const std::string getVideoPath();
	virtual const std::string getMarqueePath();
	virtual const std::string getImagePath();

	virtual const std::string getCore(bool resolveDefault = true);
	virtual const std::string getEmulator(bool resolveDefault = true);

	virtual bool isNetplaySupported();

	void setCore(const std::string value);
	void setEmulator(const std::string value);

	virtual const bool getHidden() const;
	virtual const bool getFavorite() const;
	virtual const bool getKidGame() const;
	virtual const bool hasCheevos();

	bool hasAnyMedia();
	std::vector<std::string> getFileMedias();

	const std::string getConfigurationName();

	inline bool isPlaceHolder() { return mType == PLACEHOLDER; };	

	virtual std::string getKey();
	const bool isArcadeAsset();
	const bool isVerticalArcadeGame();
	const bool isLightGunGame();
  	const bool isWheelGame();
    	const bool isTrackballGame();
      	const bool isSpinnerGame();
	inline std::string getFullPath() { return getPath(); };
	inline std::string getFileName() { return Utils::FileSystem::getFileName(getPath()); };
	virtual FileData* getSourceFileData();
	virtual std::string getSystemName() const;

	// Returns our best guess at the "real" name for this file (will attempt to perform MAME name translation)
	virtual std::string& getDisplayName();

	// As above, but also remove parenthesis
	std::string getCleanName();

	std::string getlaunchCommand(bool includeControllers = true) { LaunchGameOptions options; return getlaunchCommand(options, includeControllers); };
	std::string getlaunchCommand(LaunchGameOptions& options, bool includeControllers = true);

	bool		launchGame(Window* window, LaunchGameOptions options = LaunchGameOptions());

	static void resetSettings();
	
	virtual const MetaDataList& getMetadata() const { return mMetadata; }
	virtual MetaDataList& getMetadata() { return mMetadata; }

	void setMetadata(MetaDataList value) { getMetadata() = value; } 
	
	std::string getMetadata(MetaDataId key) const { return getMetadata().get(key); }
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

	void setSelectedGame();

	std::pair<int, int> parsePlayersRange();

	// IBindable
	BindableProperty getProperty(const std::string& name) override;
	std::string getBindableTypeName()  override { return "game"; }
	IBindable*  getBindableParent() override;

	std::string getGenre();

private:
	std::string getKeyboardMappingFilePath();
	std::string getMessageFromExitCode(int exitCode);
	MetaDataList mMetadata;

protected:	
	std::string  findLocalArt(const std::string& type = "", std::vector<std::string> exts = { ".png", ".jpg" });

	static FileData* mRunningGame;

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
	void getFilesRecursiveWithContext(std::vector<FileData*>& out, unsigned int typeMask, GetFileContext* filter, bool displayedOnly, SystemData* system, bool includeVirtualStorage) const;


	std::vector<FileData*> mChildren;
	bool	mOwnsChildrens;
	bool	mIsDisplayableAsVirtualFolder;
};

#endif // ES_APP_FILE_DATA_H
