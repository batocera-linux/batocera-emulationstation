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
#include "resources/ResourceManager.h"
#include "RetroAchievements.h"
#include "SaveStateRepository.h"

FileData::FileData(FileType type, const std::string& path, SystemData* system)
	: mPath(path), mType(type), mSystem(system), mParent(nullptr), mDisplayName(nullptr), mMetadata(type == GAME ? GAME_METADATA : FOLDER_METADATA) // metadata is REALLY set in the constructor!
{
	// metadata needs at least a name field (since that's what getName() will return)
	if (mMetadata.get(MetaDataId::Name).empty())
		mMetadata.set(MetaDataId::Name, getDisplayName());
	
	mMetadata.resetChangedFlag();
}

const std::string FileData::getPath() const
{
	if (mPath.empty())
		return getSystemEnvData()->mStartPath;

	return mPath;
}

const std::string FileData::getBreadCrumbPath()
{
	std::vector<std::string> paths;

	FileData* root = getSystem()->getParentGroupSystem() != nullptr ? getSystem()->getParentGroupSystem()->getRootFolder() : getSystem()->getRootFolder();

	FileData* parent = (getType() == GAME ? getParent() : this);
	parent = (getType() == GAME ? getParent() : this);
	while (parent != nullptr)
	{
		if (parent == root->getSystem()->getRootFolder() && !parent->getSystem()->isCollection())
			break;
		
		if (parent->getSystem()->getName() == CollectionSystemManager::get()->getCustomCollectionsBundle()->getName())
			break;

		if (parent->getSystem()->isGroupChildSystem() && 
			parent->getSystem()->getParentGroupSystem() != nullptr && 
			parent->getParent() == parent->getSystem()->getParentGroupSystem()->getRootFolder() && 			
			parent->getSystem()->getName() != "windows_installers")
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
	if (mDisplayName)
		delete mDisplayName;

	if(mParent)
		mParent->removeChild(this);

	if(mType == GAME)
		mSystem->removeFromIndex(this);	
}

std::string& FileData::getDisplayName()
{
	if (mDisplayName == nullptr)
	{
		std::string stem = Utils::FileSystem::getStem(getPath());
		if (mSystem && mSystem->hasPlatformId(PlatformIds::ARCADE) || mSystem->hasPlatformId(PlatformIds::NEOGEO))
			stem = MameNames::getInstance()->getRealName(stem);

		mDisplayName = new std::string(stem);
	}

	return *mDisplayName;
}

std::string FileData::getCleanName()
{
	return Utils::String::removeParenthesis(getDisplayName());
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
						setMetadata(MetaDataId::Thumbnail, path);
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

		if (thumbnail.empty() && getType() == GAME && getSourceFileData()->getSystem()->hasPlatformId(PlatformIds::IMAGEVIEWER))
		{
			if (getType() == FOLDER && ((FolderData*)this)->mChildren.size())
				return ((FolderData*)this)->mChildren[0]->getThumbnailPath();
			else if (getType() == GAME)
			{
				thumbnail = getPath();

				auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(thumbnail));
				if (ext == ".pdf" && ResourceManager::getInstance()->fileExists(":/pdf.jpg"))
					return ":/pdf.jpg";
				else if ((ext == ".mp4" || ext == ".avi" || ext == ".mkv") && ResourceManager::getInstance()->fileExists(":/vid.jpg"))
					return ":/vid.jpg";
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

const bool FileData::hasCheevos()
{
	if (Utils::String::toInteger(getMetadata(MetaDataId::CheevosId)) > 0)
		return getSourceFileData()->getSystem()->isCheevosSupported();

	return false;
}

void FileData::resetSettings() 
{
	
}

const std::string& FileData::getName()
{
	if (mSystem != nullptr && mSystem->getShowFilenames())
		return getDisplayName();

	return mMetadata.getName();
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
			setMetadata(MetaDataId::Video, path);
			video = path;
		}
	}

	if (video.empty() && getSourceFileData()->getSystem()->hasPlatformId(PlatformIds::IMAGEVIEWER))
	{
		if (getType() == FOLDER && ((FolderData*)this)->mChildren.size())
			return ((FolderData*)this)->mChildren[0]->getVideoPath();
		else if (getType() == GAME)
		{
			auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(getPath()));
			if (ext == ".mp4" || ext == ".avi" || ext == ".mkv")
				return getPath();
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
					setMetadata(MetaDataId::Marquee, path);
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
						setMetadata(MetaDataId::Image, path);
						image = path;
					}
				}
			}
		}

		if (image.empty() && getSourceFileData()->getSystem()->hasPlatformId(PlatformIds::IMAGEVIEWER))
		{
			if (getType() == FOLDER && ((FolderData*)this)->mChildren.size())
				return ((FolderData*)this)->mChildren[0]->getImagePath();
			else if (getType() == GAME)
			{
				image = getPath();

				auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(image));
				if (ext == ".pdf" && ResourceManager::getInstance()->fileExists(":/pdf.jpg"))
					return ":/pdf.jpg";
				else if ((ext == ".mp4" || ext == ".avi" || ext == ".mkv") && ResourceManager::getInstance()->fileExists(":/vid.jpg"))
					return ":/vid.jpg";
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

const bool FileData::isVerticalArcadeGame()
{
	if (mSystem && mSystem->hasPlatformId(PlatformIds::ARCADE))
	{
		const std::string stem = Utils::FileSystem::getStem(getPath());
		return MameNames::getInstance()->isVertical(stem);
	}

	return false;
}

FileData* FileData::getSourceFileData()
{
	return this;
}


bool FileData::launchGame(Window* window, LaunchGameOptions options)
{
	LOG(LogInfo) << "Attempting to launch game...";

	FileData* gameToUpdate = getSourceFileData();
	if (gameToUpdate == nullptr)
		return false;

	SystemData* system = gameToUpdate->getSystem();
	if (system == nullptr)
		return false;

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

	if (options.netPlayMode == CLIENT || options.netPlayMode == SPECTATOR)
	{
		std::string mode = (options.netPlayMode == SPECTATOR ? "spectator" : "client");
		std::string pass;
		
		if (!options.netplayClientPassword.empty())
			pass = " -netplaypass " + options.netplayClientPassword;

#if WIN32
		if (Utils::String::toLower(command).find("retroarch.exe") != std::string::npos)
			command = Utils::String::replace(command, "%NETPLAY%", "--connect " + options.ip + " --port " + std::to_string(options.port) + " --nick " + SystemConf::getInstance()->get("global.netplay.nickname"));
		else
#endif
#ifdef _ENABLEEMUELEC
		command = Utils::String::replace(command, "%NETPLAY%", "--connect " + options.ip + " --port " + std::to_string(options.port) + " --nick " + SystemConf::getInstance()->get("global.netplay.nickname"));
#else
		command = Utils::String::replace(command, "%NETPLAY%", "-netplaymode " + mode + " -netplayport " + std::to_string(options.port) + " -netplayip " + options.ip + pass);
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
	
	if (SaveStateRepository::isEnabled(this))
		command = options.saveStateInfo.setupSaveState(this, command);

	Scripting::fireEvent("game-start", rom, basename);

	time_t tstart = time(NULL);

	LOG(LogInfo) << "	" << command;

	auto p2kConv = convertP2kFile();

	int exitCode = runSystemCommand(command, getDisplayName(), hideWindow ? NULL : window);
	if (exitCode != 0)
		LOG(LogWarning) << "...launch terminated with nonzero exit code " << exitCode << "!";

	if (SaveStateRepository::isEnabled(this))
	{
		options.saveStateInfo.onGameEnded();
		getSourceFileData()->getSystem()->getSaveStateRepository()->refresh();
	}

	if (!p2kConv.empty()) // delete .keys file if it has been converted from p2k
		Utils::FileSystem::removeFile(p2kConv);

	Scripting::fireEvent("game-end");
	
	if (!hideWindow && Settings::getInstance()->getBool("HideWindowFullReinit"))
	{
		ResourceManager::getInstance()->reloadAll();
		window->deinit();
		window->init();
		window->setCustomSplashScreen(gameToUpdate->getImagePath(), gameToUpdate->getName());
	}
	else
		window->init(hideWindow);

	VolumeControl::getInstance()->init();
	AudioManager::getInstance()->init();

	window->normalizeNextUpdate();

	//update number of times the game has been launched
	if (exitCode == 0)
	{
		int timesPlayed = gameToUpdate->getMetadata().getInt(MetaDataId::PlayCount) + 1;
		gameToUpdate->setMetadata(MetaDataId::PlayCount, std::to_string(static_cast<long long>(timesPlayed)));

		// Batocera 5.25: how long have you played that game? (more than 10 seconds, otherwise
		// you might have experienced a loading problem)
		time_t tend = time(NULL);
		long elapsedSeconds = difftime(tend, tstart);
		long gameTime = gameToUpdate->getMetadata().getInt(MetaDataId::GameTime) + elapsedSeconds;
		if (elapsedSeconds >= 10)
			gameToUpdate->setMetadata(MetaDataId::GameTime, std::to_string(static_cast<long>(gameTime)));

		//update last played time
		gameToUpdate->setMetadata(MetaDataId::LastPlayed, Utils::Time::DateTime(Utils::Time::now()));
		CollectionSystemManager::get()->refreshCollectionSystems(gameToUpdate);
		saveToGamelistRecovery(gameToUpdate);
	} else {
		// show emuelec error message
		LOG(LogWarning) << "...Show Error message! exit code " << exitCode << "!";
        // ApiSystem::getInstance()->launchErrorWindow(window);
	}

	window->reactivateGui();

	if (system != nullptr && system->getTheme() != nullptr)
		AudioManager::getInstance()->changePlaylist(system->getTheme(), true);
	else
		AudioManager::getInstance()->playRandomMusic();

	return exitCode == 0;
}


bool FileData::hasContentFiles()
{
	if (mPath.empty())
		return false;

	std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(mPath));
	if (ext == ".m3u" || ext == ".cue" || ext == ".ccd" || ext == ".gdi")
		return getSourceFileData()->getSystemEnvData()->isValidExtension(ext) && getSourceFileData()->getSystemEnvData()->mSearchExtensions.size() > 1;

	return false;
}

static std::vector<std::string> getTokens(const std::string& string)
{
	std::vector<std::string> tokens;

	bool inString = false;
	int startPos = 0;
	int i = 0;
	for (;;)
	{
		char c = string[i];

		switch (c)
		{
		case '\"':
			inString = !inString;
			if (inString)
				startPos = i + 1;

		case '\0':
		case ' ':
		case '\t':
		case '\r':
		case '\n':
			if (!inString)
			{
				std::string value = string.substr(startPos, i - startPos);
				if (!value.empty())
					tokens.push_back(value);

				startPos = i + 1;
			}
			break;
		}

		if (c == '\0')
			break;

		i++;
	}

	return tokens;
}

std::set<std::string> FileData::getContentFiles()
{
	std::set<std::string> files;

	if (mPath.empty())
		return files;

	if (Utils::FileSystem::isDirectory(mPath))
	{
		for (auto file : Utils::FileSystem::getDirContent(mPath, true, true))
			files.insert(file);
	}
	else if (hasContentFiles())
	{
		auto path = Utils::FileSystem::getParent(mPath);
		auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(mPath));

		if (ext == ".cue")
		{
			std::string start = "FILE";

			std::ifstream cue(WINSTRINGW(mPath));
			if (cue && cue.is_open())
			{
				std::string line;
				while (std::getline(cue, line))
				{
					if (!Utils::String::startsWith(line, start))
						continue;

					auto tokens = getTokens(line);
					if (tokens.size() > 1)
						files.insert(path + "/" + tokens[1]);
				}

				cue.close();
			}
		}
		else if (ext == ".ccd")
		{
			std::string stem = Utils::FileSystem::getStem(mPath);
			files.insert(path + "/" + stem + ".cue");
			files.insert(path + "/" + stem + ".img");
			files.insert(path + "/" + stem + ".bin");
			files.insert(path + "/" + stem + ".sub");
		}
		else if (ext == ".m3u")
		{
			std::ifstream m3u(WINSTRINGW(mPath));
			if (m3u && m3u.is_open())
			{
				std::string line;
				while (std::getline(m3u, line))
				{
					auto trim = Utils::String::trim(line);
					if (trim[0] == '#' || trim[0] == '\\' || trim[0] == '/')
						continue;

					files.insert(path + "/" + trim);
				}

				m3u.close();
			}
		}
		else if (ext == ".gdi")
		{
			std::ifstream gdi(WINSTRINGW(mPath));
			if (gdi && gdi.is_open())
			{
				std::string line;
				while (std::getline(gdi, line))
				{
					auto tokens = getTokens(line);
					if (tokens.size() > 5 && tokens[4].find(".") != std::string::npos)
						files.insert(path + "/" + tokens[4]);
				}

				gdi.close();
			}			
		}
	}

	return files;
}

void FileData::deleteGameFiles()
{
	for (auto mdd : mMetadata.getMDD())
	{
		if (mMetadata.getType(mdd.id) != MetaDataType::MD_PATH)
			continue;

		Utils::FileSystem::removeFile(mMetadata.get(mdd.id));
	}

	Utils::FileSystem::removeFile(getPath());

	for (auto contentFile : getContentFiles())
		Utils::FileSystem::removeFile(contentFile);
}

CollectionFileData::CollectionFileData(FileData* file, SystemData* system)
	: FileData(file->getSourceFileData()->getType(), "", system)
{
	mSourceFileData = file->getSourceFileData();
	mParent = NULL;	
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

std::string CollectionFileData::getKey() 
{
	return getFullPath();
}

FileData* CollectionFileData::getSourceFileData()
{
	return mSourceFileData;
}

const std::string& CollectionFileData::getName()
{
	return mSourceFileData->getName();
}

const std::vector<FileData*> FolderData::getChildrenListToDisplay() 
{
	std::vector<FileData*> ret;

	std::string showFoldersMode = getSystem()->getFolderViewMode();
	
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

	std::map<FileData*, int> scoringBoard;

	bool refactorUniqueGameFolders = (showFoldersMode == "having multiple games");

	for (auto it = items->cbegin(); it != items->cend(); it++)
	{
		if (!showHiddenFiles && (*it)->getHidden())
			continue;

		if (filterKidGame && (*it)->getType() == GAME && !(*it)->getKidGame())
			continue;

		if (hiddenExts.size() > 0 && (*it)->getType() == GAME)
		{
			std::string extlow = Utils::String::toLower(Utils::FileSystem::getExtension((*it)->getFileName()));
			if (std::find(hiddenExts.cbegin(), hiddenExts.cend(), extlow) != hiddenExts.cend())
				continue;
		}

		if (idx != nullptr)
		{
			int score = idx->showFile(*it);
			if (score == 0)
				continue;

			scoringBoard[*it] = score;
		}

		if ((*it)->getType() == FOLDER && refactorUniqueGameFolders)
		{
			FolderData* pFolder = (FolderData*)(*it);
			if (pFolder->getChildren().size() == 0)
				continue;

			if (pFolder->isVirtualStorage() && pFolder->getSourceFileData()->getSystem()->isGroupChildSystem() && pFolder->getSourceFileData()->getSystem()->getName() == "windows_installers")
			{
				ret.push_back(*it);
				continue;
			}

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

	if (idx != nullptr && idx->hasRelevency())
	{
		auto compf = sort.comparisonFunction;

		std::sort(ret.begin(), ret.end(), [scoringBoard, compf](const FileData* file1, const FileData* file2) -> bool
		{ 
			auto s1 = scoringBoard.find((FileData*) file1);
			auto s2 = scoringBoard.find((FileData*) file2);		

			if (s1 != scoringBoard.cend() && s2 != scoringBoard.cend() && s1->second != s2->second)
				return s1->second < s2->second;
			
			return compf(file1, file2);
		});
	}
	else
	{
		std::sort(ret.begin(), ret.end(), sort.comparisonFunction);

		if (!sort.ascending)
			std::reverse(ret.begin(), ret.end());
	}

	return ret;
}

std::shared_ptr<std::vector<FileData*>> FolderData::findChildrenListToDisplayAtCursor(FileData* toFind, std::stack<FileData*>& stack)
{
	auto items = getChildrenListToDisplay();

	for (auto item : items)
		if (toFind == item)
			return std::make_shared<std::vector<FileData*>>(items);

	for (auto item : items)
	{
		if (item->getType() != FOLDER)
			continue;
		
		stack.push(item);

		auto ret = ((FolderData*)item)->findChildrenListToDisplayAtCursor(toFind, stack);
		if (ret != nullptr)
			return ret;

		stack.pop();		
	}

	if (stack.empty())
		return std::make_shared<std::vector<FileData*>>(items);

	return nullptr;
}

FileData* FolderData::findUniqueGameForFolder()
{
	auto games = this->getFilesRecursive(GAME);

	FileData* found = nullptr;

	int count = 0;
	for (auto game : games)
	{
		if (game->getHidden())
		{
			bool showHiddenFiles = Settings::getInstance()->getBool("ShowHiddenFiles") && !UIModeController::getInstance()->isUIModeKiosk();

			auto shv = Settings::getInstance()->getString(getSystem()->getName() + ".ShowHiddenFiles");
			if (shv == "1") showHiddenFiles = true;
			else if (shv == "0") showHiddenFiles = false;

			if (!showHiddenFiles)
				continue;
		}

		found = game;
		count++;
		if (count > 1)
			break;
	}
	
	if (count == 1)
		return found;
	/*{
		auto it = games.cbegin();
		if ((*it)->getType() == GAME)
			return (*it);
	}
	*/
	return nullptr;
}

std::vector<FileData*> FolderData::getFlatGameList(bool displayedOnly, SystemData* system) const 
{
	return getFilesRecursive(GAME, displayedOnly, system);
}

std::vector<FileData*> FolderData::getFilesRecursive(unsigned int typeMask, bool displayedOnly, SystemData* system, bool includeVirtualStorage) const
{
	std::vector<FileData*> out;

	auto isVirtualFolder = [](FileData* file) 
	{ 
		if (file->getType() == GAME)
			return false;

		FolderData* fld = (FolderData*)file;
		return fld->isVirtualStorage();
	};

	bool showHiddenFiles = Settings::getInstance()->getBool("ShowHiddenFiles") && !UIModeController::getInstance()->isUIModeKiosk();

	auto shv = Settings::getInstance()->getString(getSystem()->getName() + ".ShowHiddenFiles");
	if (shv == "1") showHiddenFiles = true;
	else if (shv == "0") showHiddenFiles = false;

	SystemData* pSystem = (system != nullptr ? system : mSystem);

	std::vector<std::string> hiddenExts;
	if (!pSystem->isGroupSystem() && !pSystem->isCollection())
		for (auto ext : Utils::String::split(Settings::getInstance()->getString(pSystem->getName() + ".HiddenExt"), ';'))
			hiddenExts.push_back("." + Utils::String::toLower(ext));

	bool filterKidGame = UIModeController::getInstance()->isUIModeKid();

	FileFilterIndex* idx = pSystem->getIndex(false);

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

					if (hiddenExts.size() > 0 && it->getType() == GAME)
					{
						std::string extlow = Utils::String::toLower(Utils::FileSystem::getExtension(it->getFileName()));
						if (std::find(hiddenExts.cbegin(), hiddenExts.cend(), extlow) != hiddenExts.cend())
							continue;
					}
				}

				if (includeVirtualStorage || !isVirtualFolder(it))
					out.push_back(it);
			}
		}

		if (it->getType() != FOLDER)
			continue;
				
		FolderData* folder = (FolderData*)it;		
		if (folder->getChildren().size() > 0)
		{
			if (includeVirtualStorage || !isVirtualFolder(folder))
			{
				if (folder->isVirtualStorage() && folder->getSourceFileData()->getSystem()->isGroupChildSystem() && folder->getSourceFileData()->getSystem()->getName() == "windows_installers")
					out.push_back(it);
				else
				{
					std::vector<FileData*> subchildren = folder->getFilesRecursive(typeMask, displayedOnly, system, includeVirtualStorage);
					out.insert(out.cend(), subchildren.cbegin(), subchildren.cend());
				}
			}
		}
	}

	return out;
}

void FolderData::addChild(FileData* file, bool assignParent)
{
#if DEBUG
	assert(file->getParent() == nullptr || !assignParent);
#endif

	mChildren.push_back(file);

	if (assignParent)
		file->setParent(this);	
}

void FolderData::removeChild(FileData* file)
{
#if DEBUG
	assert(mType == FOLDER);
	assert(file->getParent() == this);
#endif

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
#if DEBUG
	assert(false);
#endif
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
	std::string core = SystemConf::getInstance()->get(getConfigurationName() + ".core"); 
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
		core = getSourceFileData()->getSystem()->getCore();

	return core;
}

const std::string FileData::getEmulator(bool resolveDefault)
{
#if WIN32 && !_DEBUG
	std::string emulator = getMetadata(MetaDataId::Emulator);
#else
	std::string emulator = SystemConf::getInstance()->get(getConfigurationName() + ".emulator");
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
		emulator = getSourceFileData()->getSystem()->getEmulator();

	return emulator;
}

void FileData::setCore(const std::string value)
{
#if WIN32 && !_DEBUG
	setMetadata(MetaDataId::Core, value == "auto" ? "" : value);
#else
	SystemConf::getInstance()->set(getConfigurationName() + ".core", value);
#endif
}

void FileData::setEmulator(const std::string value)
{
#if WIN32 && !_DEBUG
	setMetadata(MetaDataId::Emulator, value == "auto" ? "" : value);
#else
	SystemConf::getInstance()->set(getConfigurationName() + ".emulator", value);
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
		mMetadata.set(MetaDataId::Language, info.getLanguageString());
	if (!info.region.empty())
		mMetadata.set(MetaDataId::Region, info.region);
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

void FileData::checkCrc32(bool force)
{
	if (getSourceFileData() != this && getSourceFileData() != nullptr)
	{
		getSourceFileData()->checkCrc32(force);
		return;
	}

	if (!force && !getMetadata(MetaDataId::Crc32).empty())
		return;

	SystemData* system = getSystem();
	if (system == nullptr)
		return;

	auto crc = ApiSystem::getInstance()->getCRC32(getPath(), system->shouldExtractHashesFromArchives());
	if (!crc.empty())
	{
		getMetadata().set(MetaDataId::Crc32, Utils::String::toUpper(crc));
		saveToGamelistRecovery(this);
	}
}

void FileData::checkMd5(bool force)
{
	if (getSourceFileData() != this && getSourceFileData() != nullptr)
	{
		getSourceFileData()->checkMd5(force);
		return;
	}

	if (!force && !getMetadata(MetaDataId::Md5).empty())
		return;

	SystemData* system = getSystem();
	if (system == nullptr)
		return;

	auto crc = ApiSystem::getInstance()->getMD5(getPath(), system->shouldExtractHashesFromArchives());
	if (!crc.empty())
	{
		getMetadata().set(MetaDataId::Md5, Utils::String::toUpper(crc));
		saveToGamelistRecovery(this);
	}
}


void FileData::checkCheevosHash(bool force)
{
	if (getSourceFileData() != this)
	{
		getSourceFileData()->checkCheevosHash(force);
		return;
	}

	if (!force && !getMetadata(MetaDataId::CheevosHash).empty())
		return;

	SystemData* system = getSystem();
	if (system == nullptr)
		return;

	auto crc = RetroAchievements::getCheevosHash(system, getPath());
	getMetadata().set(MetaDataId::CheevosHash, Utils::String::toUpper(crc));
	saveToGamelistRecovery(this);
}

std::string FileData::getKeyboardMappingFilePath()
{
	if (Utils::FileSystem::isDirectory(getSourceFileData()->getPath()))
		return getSourceFileData()->getPath() + "/padto.keys";

	return getSourceFileData()->getPath() + ".keys";
}

bool FileData::hasP2kFile()
{
	std::string p2kPath = getSourceFileData()->getPath() + ".p2k.cfg";
	if (Utils::FileSystem::isDirectory(getSourceFileData()->getPath()))
		p2kPath = getSourceFileData()->getPath() + "/.p2k.cfg";

	return Utils::FileSystem::exists(p2kPath);
}

void FileData::importP2k(const std::string& p2k)
{
	if (p2k.empty())
		return;

	std::string p2kPath = getSourceFileData()->getPath() + ".p2k.cfg";
	if (Utils::FileSystem::isDirectory(getSourceFileData()->getPath()))
		p2kPath = getSourceFileData()->getPath() + "/.p2k.cfg";

	Utils::FileSystem::writeAllText(p2kPath, p2k);

	std::string keysPath = getKeyboardMappingFilePath();
	if (Utils::FileSystem::exists(keysPath))
		Utils::FileSystem::removeFile(keysPath);
}

std::string FileData::convertP2kFile()
{
	std::string p2kPath = getSourceFileData()->getPath() + ".p2k.cfg";
	if (Utils::FileSystem::isDirectory(getSourceFileData()->getPath()))
		p2kPath = getSourceFileData()->getPath() + "/.p2k.cfg";

	if (!Utils::FileSystem::exists(p2kPath))
		return "";

	std::string keysPath = getKeyboardMappingFilePath();
	if (Utils::FileSystem::exists(keysPath))
		return "";

	auto map = KeyMappingFile::fromP2k(p2kPath);
	if (map.isValid())
	{
		map.save(keysPath);
		return keysPath;
	}

	return "";
}

bool FileData::hasKeyboardMapping()
{
	if (!Utils::FileSystem::exists(getKeyboardMappingFilePath()))
		return hasP2kFile();

	return true;
}

KeyMappingFile FileData::getKeyboardMapping()
{
	KeyMappingFile ret;
	auto path = getKeyboardMappingFilePath();

	// If pk2.cfg file but no .keys file, then convert & load
	if (!Utils::FileSystem::exists(path) && hasP2kFile())
	{
		convertP2kFile();

		ret = KeyMappingFile::load(path);
		Utils::FileSystem::removeFile(path);
		return ret;
	}
		
	if (Utils::FileSystem::exists(path))
		ret = KeyMappingFile::load(path);
	else
		ret = getSystem()->getKeyboardMapping(); // if .keys file does not exist, take system config as predefined mapping

	ret.path = path;
	return ret;
}

bool FileData::isFeatureSupported(EmulatorFeatures::Features feature)
{
	auto system = getSourceFileData()->getSystem();
	return system->isFeatureSupported(getEmulator(), getCore(), feature);
}

bool FileData::isExtensionCompatible()
{
	auto game = getSourceFileData();
	auto extension = Utils::String::toLower(Utils::FileSystem::getExtension(game->getPath()));

	auto system = game->getSystem();
	auto emulName = game->getEmulator();
	auto coreName = game->getCore();

	for (auto emul : system->getEmulators())
	{
		if (emulName == emul.name)
		{
			if (std::find(emul.incompatibleExtensions.cbegin(), emul.incompatibleExtensions.cend(), extension) != emul.incompatibleExtensions.cend())
				return false;

			for (auto core : emul.cores)
			{
				if (coreName == core.name)
					return std::find(core.incompatibleExtensions.cbegin(), core.incompatibleExtensions.cend(), extension) == core.incompatibleExtensions.cend();
			}

			break;
		}
	}

	return true;
}

FolderData::FolderData(const std::string& startpath, SystemData* system, bool ownsChildrens) : FileData(FOLDER, startpath, system)
{
	mIsDisplayableAsVirtualFolder = false;
	mOwnsChildrens = ownsChildrens;
}

FolderData::~FolderData()
{
	clear();
}

void FolderData::clear()
{
	if (mOwnsChildrens)
	{
		for (int i = mChildren.size() - 1; i >= 0; i--)
			delete mChildren.at(i);
	}

	mChildren.clear();
}

void FolderData::removeFromVirtualFolders(FileData* game)
{
	for (auto it = mChildren.begin(); it != mChildren.end(); ++it) 
	{		
		if ((*it)->getType() == FOLDER)
		{
			((FolderData*)(*it))->removeFromVirtualFolders(game);
			continue;
		}

		if ((*it) == game)
		{
			mChildren.erase(it);
			return;
		}
	}
}

std::string FileData::getCurrentGameSetting(const std::string& settingName)
{
	FileData* src = getSourceFileData();

	std::string value = SystemConf::getInstance()->get(getConfigurationName() + "." + settingName);
	if (!value.empty() && value != "auto")
		return value;

	value = SystemConf::getInstance()->get(src->getSystem()->getName() + "." + settingName);
	if (!value.empty() && value != "auto")
		return value;

	return SystemConf::getInstance()->get("global." + settingName);
}