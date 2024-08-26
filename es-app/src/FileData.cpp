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
#include "utils/Platform.h"
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
#include "Genres.h"
#include "TextToSpeech.h"
#include "LocaleES.h"
#include "guis/GuiMsgBox.h"
#include "Paths.h"
#include "resources/TextureData.h"

using namespace Utils::Platform;

static std::map<std::string, std::function<BindableProperty(FileData*)>> properties =
{
	{ "name",				[](FileData* file) { return file->getName(); } },
	{ "rom",				[](FileData* file) { return BindableProperty(Utils::FileSystem::getFileName(file->getPath()), BindablePropertyType::String); } },
	{ "stem",				[](FileData* file) { return BindableProperty(Utils::FileSystem::getStem(file->getPath()), BindablePropertyType::String); } },
	{ "path",				[](FileData* file) { return BindableProperty(file->getPath(), BindablePropertyType::Path); } },
	{ "image",				[](FileData* file) { return BindableProperty(file->getImagePath(), BindablePropertyType::Path); } },
	{ "thumbnail",			[](FileData* file) { return BindableProperty(file->getThumbnailPath(false), BindablePropertyType::Path); } },
	{ "video",				[](FileData* file) { return BindableProperty(file->getVideoPath(), BindablePropertyType::Path); } },
	{ "marquee",			[](FileData* file) { return BindableProperty(file->getMarqueePath(), BindablePropertyType::Path); } },
	{ "favorite",			[](FileData* file) { return file->getFavorite(); } },
	{ "hidden",				[](FileData* file) { return file->getHidden(); } },
	{ "kidGame",			[](FileData* file) { return file->getKidGame(); } },
	{ "gunGame",			[](FileData* file) { return file->isLightGunGame(); } },
	{ "wheelGame",			[](FileData* file) { return file->isWheelGame(); } },
	{ "cheevos",			[](FileData* file) { return file->hasCheevos(); } },
	{ "genre",			    [](FileData* file) { return file->getGenre(); } },
	{ "hasKeyboardMapping", [](FileData* file) { return file->hasKeyboardMapping(); } },	
	{ "systemName",			[](FileData* file) { return file->getSourceFileData()->getSystem()->getFullName(); } },
};

FileData* FileData::mRunningGame = nullptr;

FileData::FileData(FileType type, const std::string& path, SystemData* system)
	: mPath(path), mType(type), mSystem(system), mParent(nullptr), mDisplayName(nullptr), mMetadata(type == GAME ? GAME_METADATA : FOLDER_METADATA) // metadata is REALLY set in the constructor!
{
	// metadata needs at least a name field (since that's what getName() will return)
	if (mMetadata.get(MetaDataId::Name).empty() && !mPath.empty())
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

	if (mParent)
		mParent->removeChild(this);

	if (mType == GAME)
		mSystem->removeFromIndex(this);
}

std::string& FileData::getDisplayName()
{
	if (mDisplayName == nullptr)
	{
		std::string stem = Utils::FileSystem::getStem(getPath());
		if (mSystem && (mSystem->hasPlatformId(PlatformIds::ARCADE) || mSystem->hasPlatformId(PlatformIds::NEOGEO)))
			stem = MameNames::getInstance()->getRealName(stem);

		mDisplayName = new std::string(stem);
	}

	return *mDisplayName;
}

std::string FileData::getCleanName()
{
	return Utils::String::removeParenthesis(getDisplayName());
}

std::string FileData::findLocalArt(const std::string& type, std::vector<std::string> exts)
{
	if (Settings::getInstance()->getBool("LocalArt"))
	{
		for (auto ext : exts)
		{
			std::string path = getSystemEnvData()->mStartPath + "/images/" + getDisplayName() + (type.empty() ? "" :  "-" + type) + ext;
			if (Utils::FileSystem::exists(path))
				return path;

			if (type == "video")
			{
				path = getSystemEnvData()->mStartPath + "/videos/" + getDisplayName() + "-" + type + ext;
				if (Utils::FileSystem::exists(path))
					return path;

				path = getSystemEnvData()->mStartPath + "/videos/" + getDisplayName() + ext;
				if (Utils::FileSystem::exists(path))
					return path;
			}
		}
	}

	return "";
}

const std::string FileData::getThumbnailPath(bool fallbackWithImage)
{
	std::string thumbnail = getMetadata(MetaDataId::Thumbnail);

	// no thumbnail, try image
	if (thumbnail.empty())
	{
		thumbnail = findLocalArt("thumb");
		if (!thumbnail.empty())
			setMetadata(MetaDataId::Thumbnail, thumbnail);

		// no image, try to use local image
		if (fallbackWithImage)
		{
			if (thumbnail.empty())
				thumbnail = getMetadata(MetaDataId::Image);

			if (thumbnail.empty())
				thumbnail = findLocalArt("image");

			if (thumbnail.empty())
				thumbnail = findLocalArt();
		}

		if (thumbnail.empty() && getType() == GAME && getSourceFileData()->getSystem()->hasPlatformId(PlatformIds::IMAGEVIEWER))
		{
			if (getType() == FOLDER && ((FolderData*)this)->mChildren.size())
				return ((FolderData*)this)->mChildren[0]->getThumbnailPath();
			else if (getType() == GAME)
			{
				thumbnail = getPath();

				auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(thumbnail));
				if (TextureData::PdfHandler == nullptr && ext == ".pdf" && ResourceManager::getInstance()->fileExists(":/pdf.jpg"))
					return ":/pdf.jpg";
			}
		}

	}

	return thumbnail;
}

const bool FileData::getFavorite() const
{
	return getMetadata(MetaDataId::Favorite) == "true";
}

const bool FileData::getHidden() const
{
	return getMetadata(MetaDataId::Hidden) == "true";
}

const bool FileData::getKidGame() const
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

bool FileData::hasAnyMedia()
{
	if (Utils::FileSystem::exists(getImagePath()) || Utils::FileSystem::exists(getThumbnailPath(false)) || Utils::FileSystem::exists(getVideoPath()))
		return true;

	for (auto mdd : mMetadata.getMDD())
	{
		if (mdd.type != MetaDataType::MD_PATH)
			continue;

		std::string path = mMetadata.get(mdd.key);
		if (path.empty())
			continue;

		if (mdd.id == MetaDataId::Manual || mdd.id == MetaDataId::Magazine)
		{
			if (Utils::FileSystem::exists(path))
				return true;
		}
		else if (mdd.id != MetaDataId::Image && mdd.id != MetaDataId::Thumbnail)
		{
			if (Utils::FileSystem::isImage(path))
				continue;

			if (Utils::FileSystem::exists(path))
				return true;
		}
	}

	return false;
}

std::vector<std::string> FileData::getFileMedias()
{
	std::vector<std::string> ret;

	for (auto mdd : mMetadata.getMDD())
	{
		if (mdd.type != MetaDataType::MD_PATH)
			continue;

		if (mdd.id == MetaDataId::Video || mdd.id == MetaDataId::Manual || mdd.id == MetaDataId::Magazine)
			continue;

		std::string path = mMetadata.get(mdd.key);
		if (path.empty())
			continue;

		if (!Utils::FileSystem::isImage(path))
			continue;
		
		if (Utils::FileSystem::exists(path))
			ret.push_back(path);
	}

	return ret;
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
	if (video.empty())
	{
		video = findLocalArt("video", { ".mp4" });
		if (!video.empty())
			setMetadata(MetaDataId::Video, video);
	}
	
	if (video.empty() && getSourceFileData()->getSystem()->hasPlatformId(PlatformIds::IMAGEVIEWER))
	{
		if (getType() == FOLDER && ((FolderData*)this)->mChildren.size())
			return ((FolderData*)this)->mChildren[0]->getVideoPath();
		else if (getType() == GAME)
		{
			if (Utils::FileSystem::isVideo(getPath()))
				return getPath();

			if (Utils::FileSystem::isAudio(getPath()))
				return getPath();
		}
	}

	return video;
}

const std::string FileData::getMarqueePath()
{
	std::string marquee = getMetadata(MetaDataId::Marquee);

	// no marquee, try to use local marquee
	if (marquee.empty())
	{
		marquee = findLocalArt("marquee");
		if (!marquee.empty())
			setMetadata(MetaDataId::Marquee, marquee);
	}
	
	return marquee;
}

const std::string FileData::getImagePath()
{
	std::string image = getMetadata(MetaDataId::Image);

	// no image, try to use local image
	if (image.empty())
	{
		auto romExt = Utils::String::toLower(Utils::FileSystem::getExtension(getPath()));
		if (romExt == ".png" || (getSystemName() == "pico8" && romExt == ".p8"))
			return getPath();

		if (image.empty())
			image = findLocalArt("image");

		if (image.empty())
			image = findLocalArt();

		if (!image.empty())
			setMetadata(MetaDataId::Image, image);

		if (image.empty() && getSourceFileData()->getSystem()->hasPlatformId(PlatformIds::IMAGEVIEWER))
		{
			if (getType() == FOLDER && ((FolderData*)this)->mChildren.size())
				return ((FolderData*)this)->mChildren[0]->getImagePath();
			else if (getType() == GAME)
			{
				image = getPath();

				auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(image));
				if (TextureData::PdfHandler == nullptr && ext == ".pdf" && ResourceManager::getInstance()->fileExists(":/pdf.jpg"))
					return ":/pdf.jpg";

				if (Utils::FileSystem::isAudio(image) && ResourceManager::getInstance()->fileExists(":/mp3.jpg"))
					return ":/mp3.jpg";
			}
		}
	}

	return image;
}

std::string FileData::getKey()
{
	return getFileName();
}

const bool FileData::isArcadeAsset()
{
	if (mSystem && (mSystem->hasPlatformId(PlatformIds::ARCADE) || mSystem->hasPlatformId(PlatformIds::NEOGEO)))
	{	
		const std::string stem = Utils::FileSystem::getStem(getPath());
		return MameNames::getInstance()->isBiosOrDevice(stem);		
	}

	return false;
}

const bool FileData::isVerticalArcadeGame()
{
	if (mSystem && mSystem->hasPlatformId(PlatformIds::ARCADE))
		return MameNames::getInstance()->isVertical(Utils::FileSystem::getStem(getPath()));

	return false;
}

const bool FileData::isLightGunGame()
{
	return MameNames::getInstance()->isLightgun(Utils::FileSystem::getStem(getPath()), mSystem->getName(), mSystem && mSystem->hasPlatformId(PlatformIds::ARCADE));
	//return Genres::genreExists(&getMetadata(), GENRE_LIGHTGUN);
}

const bool FileData::isWheelGame()
{
	return MameNames::getInstance()->isWheel(Utils::FileSystem::getStem(getPath()), mSystem->getName(), mSystem && mSystem->hasPlatformId(PlatformIds::ARCADE));
	//return Genres::genreExists(&getMetadata(), GENRE_WHEEL);
}

FileData* FileData::getSourceFileData()
{
	return this;
}

static std::string formatCommandLineArgument(const std::string& name)
{
	if (name.find(" ") != std::string::npos)
		return "\"" + Utils::String::replace(name, "\"", "\\\"") + "\"";

	return Utils::String::replace(name, "\"", "\\\"");
};

std::string FileData::getlaunchCommand(LaunchGameOptions& options, bool includeControllers)
{
	FileData* gameToUpdate = getSourceFileData();
	if (gameToUpdate == nullptr)
		return "";

	SystemData* system = gameToUpdate->getSystem();
	if (system == nullptr)
		return "";

	// must really;-) be done before window->deinit while it closes joysticks
	std::string controllersConfig = InputManager::getInstance()->configureEmulators();

#if WIN32
	if (gameToUpdate->isLightGunGame())
#else
	if (InputManager::getInstance()->getGuns().size() && gameToUpdate->isLightGunGame())
#endif
		controllersConfig = controllersConfig + "-lightgun ";

        if (gameToUpdate->isWheelGame())
		controllersConfig = controllersConfig + "-wheel ";

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
	/*else if (!isExtensionCompatible())
	{
		auto extension = Utils::String::toLower(Utils::FileSystem::getExtension(gameToUpdate->getPath()));

		for (auto emul : system->getEmulators())
		{
			if (std::find(emul.incompatibleExtensions.cbegin(), emul.incompatibleExtensions.cend(), extension) == emul.incompatibleExtensions.cend())
			{
				for (auto coreZ : emul.cores)
				{
					if (std::find(coreZ.incompatibleExtensions.cbegin(), coreZ.incompatibleExtensions.cend(), extension) == coreZ.incompatibleExtensions.cend())
					{
						emulator = emul.name;
						core = coreZ.name;
						break;
					}
				}
			}
		}
	}*/
	
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
	
	command = Utils::String::replace(command, "%SYSTEM%", systemName);
	command = Utils::String::replace(command, "%ROM%", rom);
	command = Utils::String::replace(command, "%BASENAME%", basename);
	command = Utils::String::replace(command, "%ROM_RAW%", rom_raw);
	command = Utils::String::replace(command, "%EMULATOR%", emulator);
	command = Utils::String::replace(command, "%CORE%", core);
	command = Utils::String::replace(command, "%HOME%", Paths::getHomePath());
	command = Utils::String::replace(command, "%GAMENAME%", formatCommandLineArgument(gameToUpdate->getName()));
	command = Utils::String::replace(command, "%SYSTEMNAME%", formatCommandLineArgument(system->getFullName()));

	// Export Game info XML is requested
#ifdef WIN32
	std::string fileInfo = Utils::FileSystem::combine(Utils::FileSystem::getTempPath(), "game.xml");
#else
	std::string fileInfo = "/tmp/game.xml";
#endif

	if (command.find("%GAMEINFOXML%") != std::string::npos && saveToXml(gameToUpdate, fileInfo, true))
		command = Utils::String::replace(command, "%GAMEINFOXML%", Utils::FileSystem::getEscapedPath(fileInfo));
	else
	{
		command = Utils::String::replace(command, "%GAMEINFOXML%", "");
		Utils::FileSystem::removeFile(fileInfo);
	}
	
	if (includeControllers)
		command = Utils::String::replace(command, "%CONTROLLERSCONFIG%", controllersConfig);

	if (options.netPlayMode != DISABLED && (forceCore || gameToUpdate->isNetplaySupported()) && command.find("%NETPLAY%") == std::string::npos)
		command = command + " %NETPLAY%"; // Add command line parameter if the netplay option is defined at <core netplay="true"> level

	if (options.netPlayMode == CLIENT || options.netPlayMode == SPECTATOR)
	{
		std::string mode = (options.netPlayMode == SPECTATOR ? "spectator" : "client");
		std::string pass;

		if (!options.netplayClientPassword.empty())
			pass = " -netplaypass " + options.netplayClientPassword;

		// mitm_session: Should inject this into retroarch -> --mitm-session=ID
		std::string session;
		if (!options.session.empty())
			session = " -netplaysession " + options.session;

#if WIN32
		if (Utils::String::toLower(command).find("retroarch.exe") != std::string::npos)
			command = Utils::String::replace(command, "%NETPLAY%", "--connect " + options.ip + " --port " + std::to_string(options.port) + " --nick " + SystemConf::getInstance()->get("global.netplay.nickname"));
		else
#endif
			command = Utils::String::replace(command, "%NETPLAY%", "-netplaymode " + mode + " -netplayport " + std::to_string(options.port) + " -netplayip " + options.ip + session + pass);
	}
	else if (options.netPlayMode == SERVER)
	{
#if WIN32
		if (Utils::String::toLower(command).find("retroarch.exe") != std::string::npos)
			command = Utils::String::replace(command, "%NETPLAY%", "--host --port " + SystemConf::getInstance()->get("global.netplay.port") + " --nick " + SystemConf::getInstance()->get("global.netplay.nickname"));
		else
#endif
			command = Utils::String::replace(command, "%NETPLAY%", "-netplaymode host");
	}
	else
		command = Utils::String::replace(command, "%NETPLAY%", "");

	int monitorId = Settings::getInstance()->getInt("MonitorID");
	if (monitorId >= 0 && command.find(" -system ") != std::string::npos)
		command = command + " -monitor " + std::to_string(monitorId);

	if (SaveStateRepository::isEnabled(this))
	{
		if (options.saveStateInfo == nullptr)
		{
			if (getCurrentGameSetting("autosave") == "1")
				options.saveStateInfo = system->getSaveStateRepository()->getGameAutoSave(this);
			else
				options.saveStateInfo = SaveStateRepository::getEmptySaveState();
		}

		command = options.saveStateInfo->setupSaveState(this, command);		
	}

	return command;
}

std::string FileData::getMessageFromExitCode(int exitCode)
{
	switch (exitCode)
	{
	case 200:
		return _("THE EMULATOR EXITED UNEXPECTEDLY");
	case 201:
		return _("BAD COMMAND LINE ARGUMENTS");
	case 202:
		return _("INVALID CONFIGURATION");
	case 203:
		return _("UNKNOWN EMULATOR");
	case 204:
		return _("EMULATOR IS MISSING");
	case 205:
		return _("CORE IS MISSING");
	case 299:
		{
	#if WIN32
			std::string messageFile = Utils::FileSystem::combine(Utils::FileSystem::getTempPath(), "launch_error.log");
	#else
			std::string messageFile = "/tmp/launch_error.log";
	#endif
			if (Utils::FileSystem::exists(messageFile))
			{
				auto message = Utils::FileSystem::readAllText(messageFile);
				Utils::FileSystem::removeFile(messageFile);

				if (!message.empty())
					return message;
			}
		}
	}

	return _("UKNOWN ERROR") + " : " + std::to_string(exitCode);
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

	std::string command = getlaunchCommand(options);
	if (command.empty())
		return false;

	AudioManager::getInstance()->deinit();
	VolumeControl::getInstance()->deinit();

	bool hideWindow = Settings::getInstance()->getBool("HideWindow");
	window->deinit(hideWindow);
	
	const std::string rom = Utils::FileSystem::getEscapedPath(getPath());
	const std::string basename = Utils::FileSystem::getStem(getPath());

	Scripting::fireEvent("game-start", rom, basename, getName());

	time_t tstart = time(NULL);

	LOG(LogInfo) << "	" << command;

	auto p2kConv = convertP2kFile();

	mRunningGame = gameToUpdate;

	ProcessStartInfo process(command);
	process.window = hideWindow ? NULL : window;
	
	int exitCode = process.run();
	if (exitCode != 0)
		LOG(LogWarning) << "...launch terminated with nonzero exit code " << exitCode << "!";

	mRunningGame = nullptr;

	if (SaveStateRepository::isEnabled(this))
	{
		if (options.saveStateInfo != nullptr)
			options.saveStateInfo->onGameEnded(this);

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

		// How long have you played that game? (more than 10 seconds, otherwise
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
	}

	window->reactivateGui();

	if (system != nullptr && system->getTheme() != nullptr)
		AudioManager::getInstance()->changePlaylist(system->getTheme(), true);
	else
		AudioManager::getInstance()->playRandomMusic();

	if (exitCode >= 200 && exitCode <= 300)
		window->pushGui(new GuiMsgBox(window, _("AN ERROR OCCURRED") + ":\r\n" + getMessageFromExitCode(exitCode), _("OK"), nullptr, GuiMsgBoxIcon::ICON_ERROR));

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
	
	bool showHiddenFiles = Settings::ShowHiddenFiles();

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
	if (mSystem->isGameSystem() && !mSystem->isCollection())
		hiddenExts = Utils::String::split(Utils::String::toLower(Settings::getInstance()->getString(mSystem->getName() + ".HiddenExt")), ';');

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
			std::string extlow = Utils::String::toLower(Utils::FileSystem::getExtension((*it)->getFileName(), false));
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
		bool foldersFirst = Settings::ShowFoldersFirst();
		bool favoritesFirst = getSystem()->getShowFavoritesFirst();

		std::stable_sort(ret.begin(), ret.end(), [sort, foldersFirst, favoritesFirst](const FileData* file1, const FileData* file2) -> bool
			{
				if (favoritesFirst && file1->getFavorite() != file2->getFavorite())
					return file1->getFavorite();

				if (foldersFirst && file1->getType() != file2->getType())
					return (file1->getType() == FOLDER);

				return sort.comparisonFunction(file1, file2) == sort.ascending;
			});
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
			bool showHiddenFiles = Settings::ShowHiddenFiles() && !UIModeController::getInstance()->isUIModeKiosk();

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



void FolderData::getFilesRecursiveWithContext(std::vector<FileData*>& out, unsigned int typeMask, GetFileContext* filter, bool displayedOnly, SystemData* system, bool includeVirtualStorage) const
{
	if (filter == nullptr)
		return;

	auto isVirtualFolder = [](FileData* file)
	{
		if (file->getType() == GAME)
			return false;

		FolderData* fld = (FolderData*)file;
		return fld->isVirtualStorage();
	};

	SystemData* pSystem = (system != nullptr ? system : mSystem);
	
	FileFilterIndex* idx = pSystem->getIndex(false);

	for (auto it : mChildren)
	{
		if (it->getType() & typeMask)
		{
			if (!displayedOnly || idx == nullptr || !idx->isFiltered() || idx->showFile(it))
			{
				if (displayedOnly)
				{
					if (!filter->showHiddenFiles && it->getHidden())
						continue;

					if (filter->filterKidGame && it->getKidGame())
						continue;

					if (typeMask == GAME && filter->hiddenExtensions.size() > 0)
					{
						std::string extlow = Utils::String::toLower(Utils::FileSystem::getExtension(it->getFileName(), false));
						if (filter->hiddenExtensions.find(extlow) != filter->hiddenExtensions.cend())
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
					folder->getFilesRecursiveWithContext(out, typeMask, filter, displayedOnly, system, includeVirtualStorage);
			}
		}
	}
}

std::vector<FileData*> FolderData::getFlatGameList(bool displayedOnly, SystemData* system) const
{
	return getFilesRecursive(GAME, displayedOnly, system);
}

std::vector<FileData*> FolderData::getFilesRecursive(unsigned int typeMask, bool displayedOnly, SystemData* system, bool includeVirtualStorage) const
{
	SystemData* pSystem = (system != nullptr ? system : mSystem);
	
	GetFileContext ctx;
	ctx.showHiddenFiles = Settings::ShowHiddenFiles() && !UIModeController::getInstance()->isUIModeKiosk();

	auto shv = Settings::getInstance()->getString(getSystem()->getName() + ".ShowHiddenFiles");
	if (shv == "1")
		ctx.showHiddenFiles = true;
	else if (shv == "0")
		ctx.showHiddenFiles = false;

	if (pSystem->isGameSystem() && !pSystem->isCollection())
	{
		for (auto ext : Utils::String::split(Utils::String::toLower(Settings::getInstance()->getString(pSystem->getName() + ".HiddenExt")), ';'))
			if (ctx.hiddenExtensions.find(ext) == ctx.hiddenExtensions.cend())
				ctx.hiddenExtensions.insert(ext);
	}

	ctx.filterKidGame = UIModeController::getInstance()->isUIModeKid();

	std::vector<FileData*> out;
	getFilesRecursiveWithContext(out, typeMask, &ctx, displayedOnly, system, includeVirtualStorage);
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

	if (!CustomFeatures::FeaturesLoaded)
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

void FileData::setSelectedGame()
{
	TextToSpeech::getInstance()->say(getName(), false);

	Scripting::fireEvent("game-selected", getSourceFileData()->getSystem()->getName(), getPath(), getName());

	std::string desc = getMetadata(MetaDataId::Desc);
	if (!desc.empty())
		TextToSpeech::getInstance()->say(desc, true);	
}

std::string FileData::getGenre()
{
	std::string genre = Genres::genreStringFromIds(Utils::String::split(getMetadata(MetaDataId::GenreIds), ',', true));
	if (genre.empty())
		genre = getMetadata(MetaDataId::Genre);

	return genre;
}

BindableProperty FileData::getProperty(const std::string& name)
{
	auto it = properties.find(name);
	if (it != properties.cend())
		return it->second(this);

	if (name == "nameShort")
	{
		auto name = getName();

		for (int i = 0; i < name.size(); i++)
			if (name[i] == '(' || name[i] == '[')
				return name.substr(0, i);

		return name;
	}

	if (name == "nameExtra")
	{
		auto name = getName();

		for (int i = 0; i < name.size(); i++)
			if (name[i] == '(' || name[i] == '[')
				return name.substr(i);

		return "";
	}	

	if (name == "collection")
	{
		if (getSystem()->isCollection() || getSystem()->isGroupChildSystem())
		{
			FolderData* parent = getParent();

			if (getType() == FOLDER)
			{				
				if (parent != nullptr && (parent->getSystem()->isCollection() || getSystem()->isGroupChildSystem() || getSystem()->isGroupSystem()))
					return BindableProperty(parent->getSystem());
			}

			if (getSystem()->isGroupChildSystem() && parent != nullptr)
			{
				auto group = getSystem()->getParentGroupSystem();
				if (group != nullptr)
				{
					std::string showFoldersMode = group->getFolderViewMode();
					if (showFoldersMode == "never")
						return BindableProperty(getBindableParent());
					else if (showFoldersMode == "having multiple games" && parent && parent->findUniqueGameForFolder() != nullptr)
						return BindableProperty(getBindableParent());
				}
			}

			if (getSystem()->isCollection() || getSystem()->isGroupChildSystem() || getSystem()->isGroupSystem())
				return BindableProperty(getSystem());
		}
			
		return BindableProperty::Null; // getProperty("system");
	}

	if (name == "system")
	{
		auto sys = getSourceFileData()->getSystem();
		if (mPath == ".." && sys->isGroupChildSystem())
		{
			SystemData* group = sys->getParentGroupSystem();
			if (group != nullptr)
				sys = group;
		}

		return BindableProperty(sys);
	}

	if (name == "directory")
	{				
		if (!getSystem()->isCollection() && getSystem()->isGroupChildSystem())
		{
			SystemData* group = getSystem()->getParentGroupSystem();
			if (group != nullptr)
				return BindableProperty::EmptyString;
		}

		std::string showFoldersMode = getSystem()->getFolderViewMode();
		if (showFoldersMode == "never")
			return BindableProperty::EmptyString;
		
		auto parent = getParent();
		if (parent != nullptr)
		{
			if (showFoldersMode == "having multiple games")
			{
				auto fd = parent->findUniqueGameForFolder();
				if (fd != nullptr) 
					return BindableProperty::EmptyString;
			}

			if (getSystem()->isCollection() && parent == getSystem()->getRootFolder())
				return BindableProperty::EmptyString;

			if (!parent->isVirtualFolderDisplay())
				return parent->getBreadCrumbPath();
		}

		return BindableProperty::EmptyString;
	}

	if (name == "type")
	{
		switch (getType())
		{
		case FOLDER: return BindableProperty("folder", BindablePropertyType::String);
		case PLACEHOLDER: return BindableProperty("placeholder", BindablePropertyType::String);
		default: return BindableProperty("game", BindablePropertyType::String);
		}
	}

	if (name == "stars")
	{
		#define RATINGSTAR _U("\uF005")

		int stars = (int)Math::round(Math::clamp(0.0f, 1.0f, Utils::String::toFloat(getMetadata(MetaDataId::Rating))) * 5.0);

		std::string str;
		for (int i = 0; i < stars; i++)
			str += RATINGSTAR;

		return str;
	}

	if (name == "folder" || name == "isFolder")
		return getType() == FOLDER; 

	if (name == "virtualfolder")
		return getType() == FOLDER && (getPath() == ".." || ((FolderData*)this)->isVirtualFolderDisplay());

	if (name == "placeHolder" || name == "isPlaceHolder" || name == "placeholder")
		return getType() == PLACEHOLDER;

	if (name == "playerCount" || name == "playercount")
	{
		std::string value = getMetadata().get("players");
		auto split = value.rfind("+");
		if (split != std::string::npos)
			return value.substr(0, split);

		split = value.rfind("-");
		if (split != std::string::npos)
			return value.substr(split + 1);

		return (int) Math::clamp(Utils::String::toInteger(value), 1, 9);
	}

	if (name == "hasManual" || name == "hasmanual")
	{
		if (Settings::getInstance()->getBool("PreloadMedias"))
			return !getMetadata(MetaDataId::Manual).empty() || !getMetadata(MetaDataId::Magazine).empty(); // ? _("YES") : _("NO");
		
		return Utils::FileSystem::exists(getMetadata(MetaDataId::Manual)) || Utils::FileSystem::exists(getMetadata(MetaDataId::Magazine)); // ? _("YES") : _("NO");
	}

	if (name == "hasSaveState" || name == "hassavestate" || name == "savestate")
	{
		bool hasSaveState = SaveStateRepository::isEnabled(this) && getSourceFileData()->getSystem()->getSaveStateRepository()->hasSaveStates(this);
		return hasSaveState;
	}

	if (name == "releaseyear" || name == "releaseYear")
	{
		std::string releaseDateMeta = getMetadata(MetaDataId::ReleaseDate);
		if (releaseDateMeta.empty())
			return releaseDateMeta;

		Utils::Time::DateTime date = releaseDateMeta;
		if (!date.isValid())
			return std::string();

		return Utils::Time::timeToString(date.getTime(), "%Y");
	}

	MetaDataList& md = getMetadata();

	if (!md.exists(name))
		return BindableProperty::Null;

	std::string finalValue = md.get(name);

	auto type = md.getType(name);

	switch (type)
	{
	case MetaDataType::MD_PATH:				
		return BindableProperty(finalValue, BindablePropertyType::Path);
	case MetaDataType::MD_INT:
		return Utils::String::toInteger(finalValue);
	case MetaDataType::MD_RATING:
		return Utils::String::toFloat(finalValue);
	case MetaDataType::MD_BOOL:
		return finalValue == "1" || finalValue == "true";
	case MetaDataType::MD_DATE:
	case MetaDataType::MD_TIME:
		return finalValue.empty() ? "" : Utils::Time::timeToString(Utils::Time::DateTime(finalValue).getTime(), Utils::Time::getSystemDateFormat());
	}

	return finalValue;
}

std::pair<int, int> FileData::parsePlayersRange()
{
	std::string players = getMetadata(MetaDataId::Players);
	if (players.empty())
		return std::pair<int, int>(-1,-1);

	auto key = players;

	int min = 1;

	auto split = key.rfind("+");
	if (split != std::string::npos)
		key = Utils::String::replace(key, "+", "-99999");

	split = key.rfind("-");
	if (split != std::string::npos)
	{
		min = Utils::String::toInteger(key.substr(0, split));
		key = key.substr(split + 1);
	}

	int max = Utils::String::toInteger(key);

	return std::pair<int, int>(min, max);
}

IBindable* FileData::getBindableParent()
{ 
	SystemData* sys = getSystem();

	SystemData* group = sys->getParentGroupSystem();

	if (group != nullptr && sys->isGroupChildSystem())
		return group;

	if (group != nullptr && group != sys)
	{
		std::string showFoldersMode = group->getFolderViewMode();
		if (showFoldersMode == "never")
			return group;

		if (showFoldersMode == "having multiple games")
		{
			for (auto child : group->getRootFolder()->getChildren())
			{
				if (child->getType() == FOLDER && ((FolderData*)child)->getChildren().size() == 1)
					if (((FolderData*)child)->getChildren()[0] == this)
						return group;
			}
		}
	}

	FolderData* parent = getParent();	
	
	if (mPath == "..")
	{
		if (sys->isCollection())
		{
			parent = sys->getRootFolder()->getParent();
			if (parent != nullptr)
				return parent->getSystem();
		}

		if (group != nullptr && sys->isGroupChildSystem())
			return group;		
	}
	
	while (parent != nullptr)
	{
		sys = parent->getSystem();

	//	if (sys->isCollection())
	//		return sys;

		parent = parent->getParent();
	}

	return sys;
}
