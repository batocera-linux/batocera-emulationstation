#include "FileData.h"

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "utils/TimeUtil.h"
#include "AudioManager.h"
#include "CollectionSystemManager.h"
#include "FileFilterIndex.h"
#include "FileSorts.h"
#include "Log.h"
#include "MameNames.h"
#include "platform.h"
#include "Scripting.h"
#include "SystemData.h"
#include "VolumeControl.h"
#include "Window.h"
#include "views/UIModeController.h"
#include <assert.h>
#include "SystemConf.h"
#include "InputManager.h"
#include "scrapers/ThreadedScraper.h"
#include "Gamelist.h" 
#include "ApiSystem.h"
#include <time.h>
#include <algorithm>
#include "LangParser.h"

FileData::FileData(FileType type, const std::string& path, SystemData* system)
	: mType(type), mSystem(system), mParent(NULL), mMetadata(type == GAME ? GAME_METADATA : FOLDER_METADATA) // metadata is REALLY set in the constructor!
{
	mPath = Utils::FileSystem::createRelativePath(path, getSystemEnvData()->mStartPath, false);
	
	// metadata needs at least a name field (since that's what getName() will return)
	if (mMetadata.get("name").empty())
		mMetadata.set("name", getDisplayName());
	
	mMetadata.resetChangedFlag();

}

const std::string FileData::getPath() const
{ 	
	if (mPath.empty())
		return getSystemEnvData()->mStartPath;

	return Utils::FileSystem::resolveRelativePath(mPath, getSystemEnvData()->mStartPath, true);	
}

const std::string FileData::getBreadCrumbPath()
{
	std::vector<std::string> paths;

	FileData* parent = (getType() == GAME ? getParent() : this);
	while (parent != nullptr)
	{
		if (parent == parent->getSystem()->getRootFolder() && !parent->getSystem()->isCollection())
			break;
		
		if (parent->getSystem()->getName() == CollectionSystemManager::get()->getCustomCollectionsBundle()->getName())
			break;

		if (parent->getSystem()->isGroupChildSystem() && parent->getSystem()->getParentGroupSystem() != nullptr && parent->getParent() == parent->getSystem()->getParentGroupSystem()->getRootFolder())
			break;

		paths.push_back(parent->getName());
		parent = parent->getParent();
	}

	std::reverse(paths.begin(), paths.end());
	return Utils::String::join(paths, " > ");
}


const std::string FileData::getConfigurationName()
{
	std::string gameConf = Utils::FileSystem::getFileName(getPath());
	gameConf = Utils::String::replace(gameConf, "=", "");
	gameConf = Utils::String::replace(gameConf, "#", "");
#ifdef _ENABLEEMUELEC
	gameConf = Utils::String::replace(gameConf, "'", "'\\''");
#endif
	gameConf = getSourceFileData()->getSystem()->getName() + std::string("[\"") + gameConf + std::string("\"]");
	return gameConf;
}

inline SystemEnvironmentData* FileData::getSystemEnvData() const
{ 
	return mSystem->getSystemEnvData(); 
}

std::string FileData::getSystemName() const
{
	return mSystem->getName();
}

FileData::~FileData()
{
	if(mParent)
		mParent->removeChild(this);

	if(mType == GAME)
		mSystem->removeFromIndex(this);	
}

std::string FileData::getDisplayName() const
{
	std::string stem = Utils::FileSystem::getStem(getPath());
	if(mSystem && mSystem->hasPlatformId(PlatformIds::ARCADE) || mSystem->hasPlatformId(PlatformIds::NEOGEO))
		stem = MameNames::getInstance()->getRealName(stem);

	return stem;
}

std::string FileData::getCleanName() const
{
	return Utils::String::removeParenthesis(this->getDisplayName());
}

const std::string FileData::getThumbnailPath()
{
	std::string thumbnail = getMetadata(MetaDataId::Thumbnail);

	// no thumbnail, try image
	if(thumbnail.empty())
	{			
		// no image, try to use local image
		if(thumbnail.empty() && Settings::getInstance()->getBool("LocalArt"))
		{
			const char* extList[2] = { ".png", ".jpg" };
			for(int i = 0; i < 2; i++)
			{
				if(thumbnail.empty())
				{
					std::string path = getSystemEnvData()->mStartPath + "/images/" + getDisplayName() + "-thumb" + extList[i];
					if (Utils::FileSystem::exists(path))
					{
						setMetadata("thumbnail", path);
						thumbnail = path;
					}
				}
			}
		}

		if (thumbnail.empty())
			thumbnail = getMetadata(MetaDataId::Image);

		// no image, try to use local image
		if (thumbnail.empty() && Settings::getInstance()->getBool("LocalArt"))
		{
			const char* extList[2] = { ".png", ".jpg" };
			for (int i = 0; i < 2; i++)
			{
				if (thumbnail.empty())
				{
					std::string path = getSystemEnvData()->mStartPath + "/images/" + getDisplayName() + "-image" + extList[i];					
					if (!Utils::FileSystem::exists(path))
						path = getSystemEnvData()->mStartPath + "/images/" + getDisplayName() + extList[i];

					if (Utils::FileSystem::exists(path))
						thumbnail = path;
				}
			}
		}
	}

	return thumbnail;
}

const bool FileData::getFavorite()
{
	return getMetadata(MetaDataId::Favorite) == "true";
}

const bool FileData::getHidden()
{
	return getMetadata(MetaDataId::Hidden) == "true";
}

const bool FileData::getKidGame()
{
	auto data = getMetadata(MetaDataId::KidGame);
	return data != "false" && !data.empty();
}

static std::shared_ptr<bool> showFilenames;

void FileData::resetSettings() 
{
	showFilenames = nullptr;
}

const std::string FileData::getName()
{
	if (showFilenames == nullptr)
		showFilenames = std::make_shared<bool>(Settings::getInstance()->getBool("ShowFilenames"));

	// Faster than accessing map each time
	if (*showFilenames)
	{
		if (mSystem != nullptr && !mSystem->hasPlatformId(PlatformIds::ARCADE) && !mSystem->hasPlatformId(PlatformIds::NEOGEO))
			return Utils::FileSystem::getStem(getPath());
		else
			return getDisplayName();
	}

	return getMetadata().getName();
}

const std::string FileData::getVideoPath()
{
	std::string video = getMetadata(MetaDataId::Video);
	
	// no video, try to use local video
	if(video.empty() && Settings::getInstance()->getBool("LocalArt"))
	{
		std::string path = getSystemEnvData()->mStartPath + "/images/" + getDisplayName() + "-video.mp4";
		if (Utils::FileSystem::exists(path))
		{
			setMetadata("video", path);
			video = path;
		}
	}
	
	return video;
}

const std::string FileData::getMarqueePath()
{
	std::string marquee = getMetadata(MetaDataId::Marquee);

	// no marquee, try to use local marquee
	if (marquee.empty() && Settings::getInstance()->getBool("LocalArt"))
	{
		const char* extList[2] = { ".png", ".jpg" };
		for(int i = 0; i < 2; i++)
		{
			if(marquee.empty())
			{
				std::string path = getSystemEnvData()->mStartPath + "/images/" + getDisplayName() + "-marquee" + extList[i];
				if (Utils::FileSystem::exists(path))
				{
					setMetadata("marquee", path);
					marquee = path;
				}
			}
		}
	}

	return marquee;
}

const std::string FileData::getImagePath()
{
	std::string image = getMetadata(MetaDataId::Image);

	// no image, try to use local image
	if(image.empty())
	{
		if (getSystemName() == "imageviewer")
			image = getPath();

		if (Settings::getInstance()->getBool("LocalArt"))
		{
			const char* extList[2] = { ".png", ".jpg" };
			for (int i = 0; i < 2; i++)
			{
				if (image.empty())
				{
					std::string path = getSystemEnvData()->mStartPath + "/images/" + getDisplayName() + "-image" + extList[i];
					if (!Utils::FileSystem::exists(path))
						path = getSystemEnvData()->mStartPath + "/images/" + getDisplayName() + extList[i];

					if (Utils::FileSystem::exists(path))
					{
						setMetadata("image", path);
						image = path;
					}
				}
			}
		}
	}

	return image;
}

std::string FileData::getKey() {
	return getFileName();
}

const bool FileData::isArcadeAsset()
{
	if (mSystem && (mSystem->hasPlatformId(PlatformIds::ARCADE) || mSystem->hasPlatformId(PlatformIds::NEOGEO)))
	{	
		const std::string stem = Utils::FileSystem::getStem(getPath());
		return MameNames::getInstance()->isBios(stem) || MameNames::getInstance()->isDevice(stem);		
	}

	return false;
}

FileData* FileData::getSourceFileData()
{
	return this;
}


void FileData::launchGame(Window* window, LaunchGameOptions options)
{
	LOG(LogInfo) << "Attempting to launch game...";

	FileData* gameToUpdate = getSourceFileData();
	if (gameToUpdate == nullptr)
		return;

	SystemData* system = gameToUpdate->getSystem();
	if (system == nullptr)
		return;

	AudioManager::getInstance()->deinit(); // batocera
	VolumeControl::getInstance()->deinit();

	// batocera / must really;-) be done before window->deinit while it closes joysticks
	const std::string controllersConfig = InputManager::getInstance()->configureEmulators();

	bool hideWindow = Settings::getInstance()->getBool("HideWindow");
#if !WIN32
	hideWindow = false;
#endif
	window->deinit(hideWindow);
	
	std::string systemName = system->getName();
	std::string emulator = getEmulator();
	std::string core = getCore();

	bool forceCore = false;

	if (options.netPlayMode == CLIENT && !options.core.empty() && core != options.core)
	{
		for (auto& em : system->getEmulators())
		{
			for (auto& cr : em.cores)
			{
				if (cr.name == options.core)
				{
					emulator = em.name;
					core = cr.name;
					forceCore = true;
					break;
				}
			}

			if (forceCore)
				break;
		}
	}

	std::string command = system->getLaunchCommand(emulator, core);

	if (forceCore)
	{
		if (command.find("%EMULATOR%") == std::string::npos && command.find("-emulator") == std::string::npos)
			command = command + " -emulator %EMULATOR%";

		if (command.find("%CORE%") == std::string::npos && command.find("-core") == std::string::npos)
			command = command + " -core %CORE%";
	}

	const std::string rom = Utils::FileSystem::getEscapedPath(getPath());
	const std::string basename = Utils::FileSystem::getStem(getPath());
	const std::string rom_raw = Utils::FileSystem::getPreferredPath(getPath());

	command = Utils::String::replace(command, "%SYSTEM%", systemName); // batocera
	command = Utils::String::replace(command, "%ROM%", rom);
	command = Utils::String::replace(command, "%BASENAME%", basename);
	command = Utils::String::replace(command, "%ROM_RAW%", rom_raw);
	command = Utils::String::replace(command, "%CONTROLLERSCONFIG%", controllersConfig); // batocera
	command = Utils::String::replace(command, "%EMULATOR%", emulator);
	command = Utils::String::replace(command, "%CORE%", core);
	command = Utils::String::replace(command, "%HOME%", Utils::FileSystem::getHomePath());

	if (options.netPlayMode != DISABLED && (forceCore || gameToUpdate->isNetplaySupported()) && command.find("%NETPLAY%") == std::string::npos)
		command = command + " %NETPLAY%"; // Add command line parameter if the netplay option is defined at <core netplay="true"> level

	if (options.netPlayMode == CLIENT)
	{
#if WIN32
		if (Utils::String::toLower(command).find("retroarch.exe") != std::string::npos)
			command = Utils::String::replace(command, "%NETPLAY%", "--connect " + options.ip + " --port " + std::to_string(options.port) + " --nick " + SystemConf::getInstance()->get("global.netplay.nickname"));
		else
#endif
#ifdef _ENABLEEMUELEC
		command = Utils::String::replace(command, "%NETPLAY%", "--connect " + options.ip + " --port " + std::to_string(options.port) + " --nick " + SystemConf::getInstance()->get("global.netplay.nickname"));
#else
		command = Utils::String::replace(command, "%NETPLAY%", "-netplaymode client -netplayport " + std::to_string(options.port) + " -netplayip " + options.ip);
#endif
	}
	else if (options.netPlayMode == SERVER)
	{
#if WIN32
		if (Utils::String::toLower(command).find("retroarch.exe") != std::string::npos)
			command = Utils::String::replace(command, "%NETPLAY%", "--host --port " + SystemConf::getInstance()->get("global.netplay.port") + " --nick " + SystemConf::getInstance()->get("global.netplay.nickname"));
		else
#endif
#ifdef _ENABLEEMUELEC
		command = Utils::String::replace(command, "%NETPLAY%", "--host --port " + SystemConf::getInstance()->get("global.netplay.port") + " --nick " + SystemConf::getInstance()->get("global.netplay.nickname"));
#else
		command = Utils::String::replace(command, "%NETPLAY%", "-netplaymode host");
#endif
	}
	else
		command = Utils::String::replace(command, "%NETPLAY%", "");

	int monitorId = Settings::getInstance()->getInt("MonitorID");
	if (monitorId >= 0 && command.find(" -system ") != std::string::npos)
		command = command + " -monitor " + std::to_string(monitorId);

	Scripting::fireEvent("game-start", rom, basename);

	time_t tstart = time(NULL);

	LOG(LogInfo) << "	" << command;

	int exitCode = runSystemCommand(command, getDisplayName(), hideWindow ? NULL : window);
	if (exitCode != 0)
		LOG(LogWarning) << "...launch terminated with nonzero exit code " << exitCode << "!";

	Scripting::fireEvent("game-end");

	window->init(hideWindow);
	VolumeControl::getInstance()->init();

	// mSystem can be NULL
	//AudioManager::getInstance()->setName(mSystem->getName()); // batocera system-specific music
	AudioManager::getInstance()->init(); // batocera
	window->normalizeNextUpdate();

	//update number of times the game has been launched
	if (exitCode == 0)
	{
		int timesPlayed = gameToUpdate->getMetadata().getInt("playcount") + 1;
		gameToUpdate->setMetadata("playcount", std::to_string(static_cast<long long>(timesPlayed)));

		// Batocera 5.25: how long have you played that game? (more than 10 seconds, otherwise
		// you might have experienced a loading problem)
		time_t tend = time(NULL);
		long elapsedSeconds = difftime(tend, tstart);
		long gameTime = gameToUpdate->getMetadata().getInt("gametime") + elapsedSeconds;
		if (elapsedSeconds >= 10)
			gameToUpdate->setMetadata("gametime", std::to_string(static_cast<long>(gameTime)));

		//update last played time
		gameToUpdate->setMetadata("lastplayed", Utils::Time::DateTime(Utils::Time::now()));
		CollectionSystemManager::get()->refreshCollectionSystems(gameToUpdate);
		saveToGamelistRecovery(gameToUpdate);
	} else {
		// show error message
		LOG(LogWarning) << "...Show Error message! exit code " << exitCode << "!";
		ApiSystem::getInstance()->launchErrorWindow(window);
	}

	window->reactivateGui();

	if (system != nullptr && system->getTheme() != nullptr)
		AudioManager::getInstance()->changePlaylist(system->getTheme(), true);
	else
		AudioManager::getInstance()->playRandomMusic();
}

CollectionFileData::CollectionFileData(FileData* file, SystemData* system)
	: FileData(file->getSourceFileData()->getType(), "", system)
{
	mSourceFileData = file->getSourceFileData();
	mParent = NULL;
	mDirty = true;
}

SystemEnvironmentData* CollectionFileData::getSystemEnvData() const
{ 
	return mSourceFileData->getSystemEnvData();
}

const std::string CollectionFileData::getPath() const
{
	return mSourceFileData->getPath();
}

std::string CollectionFileData::getSystemName() const
{
	return mSourceFileData->getSystem()->getName();
}

CollectionFileData::~CollectionFileData()
{
	// need to remove collection file data at the collection object destructor
	if(mParent)
		mParent->removeChild(this);
	mParent = NULL;
}

std::string CollectionFileData::getKey() {
	return getFullPath();
}

FileData* CollectionFileData::getSourceFileData()
{
	return mSourceFileData;
}

void CollectionFileData::refreshMetadata()
{
	// metadata = mSourceFileData->metadata;
	mDirty = true;
}

const std::string CollectionFileData::getName()
{
	if (mDirty) {
		mCollectionFileName = Utils::String::removeParenthesis(mSourceFileData->getMetadata(MetaDataId::Name));
		mCollectionFileName += " [" + Utils::String::toUpper(mSourceFileData->getSystem()->getName()) + "]";
		mDirty = false;
	}

	if (Settings::getInstance()->getBool("CollectionShowSystemInfo"))
		return mCollectionFileName;
		
	return Utils::String::removeParenthesis(mSourceFileData->getMetadata(MetaDataId::Name));
}

const std::vector<FileData*> FolderData::getChildrenListToDisplay() 
{
	std::vector<FileData*> ret;

	std::string showFoldersMode = Settings::getInstance()->getString("FolderViewMode");

	auto fvm = Settings::getInstance()->getString(getSystem()->getName() + ".FolderViewMode");
	if (!fvm.empty() && fvm != "auto") showFoldersMode = fvm;
	
	bool showHiddenFiles = Settings::getInstance()->getBool("ShowHiddenFiles");

	auto shv = Settings::getInstance()->getString(getSystem()->getName() + ".ShowHiddenFiles");
	if (shv == "1") showHiddenFiles = true;
	else if (shv == "0") showHiddenFiles = false;

	bool filterKidGame = false;

	if (!Settings::getInstance()->getBool("ForceDisableFilters"))
	{
		if (UIModeController::getInstance()->isUIModeKiosk())
			showHiddenFiles = false;

		if (UIModeController::getInstance()->isUIModeKid())
			filterKidGame = true;
	}

	auto sys = CollectionSystemManager::get()->getSystemToView(mSystem);

	std::vector<std::string> hiddenExts;
	if (!mSystem->isGroupSystem() && !mSystem->isCollection())
		for (auto ext : Utils::String::split(Settings::getInstance()->getString(mSystem->getName() + ".HiddenExt"), ';'))	
			hiddenExts.push_back("." + Utils::String::toLower(ext));
	
	FileFilterIndex* idx = sys->getIndex(false);
	if (idx != nullptr && !idx->isFiltered())
		idx = nullptr;

	std::vector<FileData*>* items = &mChildren;
	
	std::vector<FileData*> flatGameList;
	if (showFoldersMode == "never")
	{
		flatGameList = getFlatGameList(false, sys);
		items = &flatGameList;		
	}

	bool refactorUniqueGameFolders = (showFoldersMode == "having multiple games");

	for (auto it = items->cbegin(); it != items->cend(); it++)
	{
		if (idx != nullptr && !idx->showFile((*it)))
			continue;

		if (!showHiddenFiles && (*it)->getHidden())
			continue;

		if (filterKidGame && !(*it)->getKidGame())
			continue;
		
		if (hiddenExts.size() > 0 && (*it)->getType() == GAME)
		{
			std::string extlow = Utils::String::toLower(Utils::FileSystem::getExtension((*it)->getFileName()));
			if (std::find(hiddenExts.cbegin(), hiddenExts.cend(), extlow) != hiddenExts.cend())
				continue;
		}

		if ((*it)->getType() == FOLDER && refactorUniqueGameFolders)
		{
			FolderData* pFolder = (FolderData*)(*it);
			auto fd = pFolder->findUniqueGameForFolder();
			if (fd != nullptr)
			{
				if (idx != nullptr && !idx->showFile(fd))
					continue;

				if (!showHiddenFiles && fd->getHidden())
					continue;

				if (filterKidGame && !fd->getKidGame())
					continue;

				ret.push_back(fd);

				continue;
			}
		}

		ret.push_back(*it);
	}

	unsigned int currentSortId = sys->getSortId();
	if (currentSortId > FileSorts::getSortTypes().size())
		currentSortId = 0;

	const FileSorts::SortType& sort = FileSorts::getSortTypes().at(currentSortId);
	std::sort(ret.begin(), ret.end(), sort.comparisonFunction);

	if (!sort.ascending)
		std::reverse(ret.begin(), ret.end());

	return ret;
}

FileData* FolderData::findUniqueGameForFolder()
{
	auto games = this->getFilesRecursive(GAME);
	if (games.size() == 1)
	{
		auto it = games.cbegin();
		if ((*it)->getType() == GAME)
			return (*it);
	}

	return nullptr;
}

std::vector<FileData*> FolderData::getFlatGameList(bool displayedOnly, SystemData* system) const 
{
	return getFilesRecursive(GAME, displayedOnly, system);
}

std::vector<FileData*> FolderData::getFilesRecursive(unsigned int typeMask, bool displayedOnly, SystemData* system) const
{
	std::vector<FileData*> out;

	bool showHiddenFiles = Settings::getInstance()->getBool("ShowHiddenFiles") && !UIModeController::getInstance()->isUIModeKiosk();

	auto shv = Settings::getInstance()->getString(getSystem()->getName() + ".ShowHiddenFiles");
	if (shv == "1") showHiddenFiles = true;
	else if (shv == "0") showHiddenFiles = false;

	bool filterKidGame = UIModeController::getInstance()->isUIModeKid();

	FileFilterIndex* idx = (system != nullptr ? system : mSystem)->getIndex(false);

	for (auto it : mChildren)
	{
		if (it->getType() & typeMask)
		{
			if (!displayedOnly || idx == nullptr || !idx->isFiltered() || idx->showFile(it))
			{
				if (displayedOnly)
				{
					if (!showHiddenFiles && it->getHidden())
						continue;

					if (filterKidGame && it->getKidGame())
						continue;
				}

				out.push_back(it);
			}
		}

		if (it->getType() != FOLDER)
			continue;

		FolderData* folder = (FolderData*) it;
		if (folder->getChildren().size() > 0)
		{
			std::vector<FileData*> subchildren = folder->getFilesRecursive(typeMask, displayedOnly, system);
			out.insert(out.cend(), subchildren.cbegin(), subchildren.cend());
		}
	}

	return out;
}

void FolderData::addChild(FileData* file, bool assignParent)
{
	assert(file->getParent() == nullptr || !assignParent);

	mChildren.push_back(file);

	if (assignParent)
		file->setParent(this);	
}

void FolderData::removeChild(FileData* file)
{
	assert(mType == FOLDER);
	assert(file->getParent() == this);

	for (auto it = mChildren.cbegin(); it != mChildren.cend(); it++)
	{
		if (*it == file)
		{
			file->setParent(NULL);
			mChildren.erase(it);
			return;
		}
	}

	// File somehow wasn't in our children.
	assert(false);

}

FileData* FolderData::FindByPath(const std::string& path)
{
	std::vector<FileData*> children = getChildren();

	for (std::vector<FileData*>::const_iterator it = children.cbegin(); it != children.cend(); ++it)
	{
		if ((*it)->getPath() == path)
			return (*it);

		if ((*it)->getType() != FOLDER)
			continue;
		
		auto item = ((FolderData*)(*it))->FindByPath(path);
		if (item != nullptr)
			return item;
	}

	return nullptr;
}

void FolderData::createChildrenByFilenameMap(std::unordered_map<std::string, FileData*>& map)
{
	std::vector<FileData*> children = getChildren();

	for (std::vector<FileData*>::const_iterator it = children.cbegin(); it != children.cend(); ++it)
	{
		if ((*it)->getType() == FOLDER)
			((FolderData*)(*it))->createChildrenByFilenameMap(map);			
		else 
			map[(*it)->getKey()] = (*it);
	}	
}

const std::string FileData::getCore(bool resolveDefault)
{
#if WIN32 && !_DEBUG
	std::string core = getMetadata(MetaDataId::Core);
#else
#ifndef _ENABLEEMUELEC	
	std::string core = SystemConf::getInstance()->get(getConfigurationName() + ".core"); 
#else
	std::string core = getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + getConfigurationName() + ".core'");
#endif
#endif

	if (core == "auto")
		core = "";

	if (!core.empty())
	{
		// Check core exists 
		std::string emulator = getEmulator();
		if (emulator.empty())
			core = "";
		else
		{
			bool exists = false;

			for (auto emul : getSourceFileData()->getSystem()->getEmulators())
			{
				if (emul.name == emulator)
				{
					for (auto cr : emul.cores)
					{
						if (cr.name == core)
						{
							exists = true;
							break;
						}
					}

					if (exists)
						break;
				}
			}

			if (!exists)
				core = "";
		}
	}

	if (resolveDefault && core.empty())
		core = getSourceFileData()->getSystem()->getDefaultCore(getEmulator());

	return core;
}

const std::string FileData::getEmulator(bool resolveDefault)
{
#if WIN32 && !_DEBUG
	std::string emulator = getMetadata(MetaDataId::Emulator);
#else
#ifndef _ENABLEEMUELEC	
	std::string emulator = SystemConf::getInstance()->get(getConfigurationName() + ".emulator");
#else
	std::string emulator = getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + getConfigurationName() + ".emulator'");
#endif
#endif

	if (emulator == "auto")
		emulator = "";

	if (!emulator.empty())
	{
		// Check emulator exists 
		bool exists = false;

		for (auto emul : getSourceFileData()->getSystem()->getEmulators())
			if (emul.name == emulator) { exists = true; break; }

		if (!exists)
			emulator = "";
	}

	if (resolveDefault && emulator.empty())
		emulator = getSourceFileData()->getSystem()->getDefaultEmulator();

	return emulator;
}

void FileData::setCore(const std::string value)
{
#if WIN32 && !_DEBUG
	setMetadata("core", value == "auto" ? "" : value);
#else
#ifndef _ENABLEEMUELEC	
	SystemConf::getInstance()->set(getConfigurationName() + ".core", value);
#else
	getShOutput(R"(/emuelec/scripts/setemu.sh set ')" + getConfigurationName() + ".core' " + value);
#endif
#endif
}

void FileData::setEmulator(const std::string value)
{
#if WIN32 && !_DEBUG
	setMetadata("emulator", value == "auto" ? "" : value);
#else
#ifndef _ENABLEEMUELEC
	SystemConf::getInstance()->set(getConfigurationName() + ".emulator", value);
#else
	getShOutput(R"(/emuelec/scripts/setemu.sh set ')" + getConfigurationName() + ".emulator' " + value);
#endif
#endif
}

bool FileData::isNetplaySupported()
{
	if (!SystemConf::getInstance()->getBool("global.netplay"))
		return false;

	auto file = getSourceFileData();
	if (file->getType() != GAME)
		return false;

	auto system = file->getSystem();
	if (system == nullptr)
		return false;
	
	std::string emulName = getEmulator();
	std::string coreName = getCore();

	if (!SystemData::es_features_loaded)
	{
		std::string command = system->getLaunchCommand(emulName, coreName);
		if (command.find("%NETPLAY%") != std::string::npos)
			return true;
	}
	
	for (auto emul : system->getEmulators())
		if (emulName == emul.name)
			for (auto core : emul.cores)
				if (coreName == core.name)
					return core.netplay;
					
	return false;
}

void FileData::detectLanguageAndRegion(bool overWrite)
{
	if (!overWrite && (!getMetadata(MetaDataId::Language).empty() || !getMetadata(MetaDataId::Region).empty()))
		return;

	if (getSystem()->isCollection() || getType() == FOLDER)
		return;

	auto info = LangInfo::parse(getSourceFileData()->getPath(), getSourceFileData()->getSystem());
	if (info.languages.size() > 0)
		mMetadata.set("lang", info.getLanguageString());
	if (!info.region.empty())
		mMetadata.set("region", info.region);
}

void FolderData::removeVirtualFolders()
{
	if (!mOwnsChildrens)
		return;

	for (int i = mChildren.size() - 1; i >= 0; i--)
	{
		auto file = mChildren.at(i);
		if (file->getType() != FOLDER)
			continue;

		if (((FolderData*)file)->mOwnsChildrens)
			continue;

		removeChild(file);
		delete file;
	}
}
