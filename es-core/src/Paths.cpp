#include "Paths.h"

#include <iostream>
#include <fstream>
#include <map>

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

Paths* Paths::_instance = nullptr;

#ifdef WIN32
#define SETTINGS_FILENAME "emulatorLauncher.cfg"
#else
#define SETTINGS_FILENAME "emulationstation.ini"
#endif

Paths::Paths()
{	
	mEmulationStationPath = getExePath();
	mUserEmulationStationPath = Utils::FileSystem::getCanonicalPath(getHomePath() + "/.emulationstation");
	mRootPath = Utils::FileSystem::getParent(getHomePath());

	mLogPath = mUserEmulationStationPath;
	mThemesPath = mUserEmulationStationPath + "/themes";
	mMusicPath = mUserEmulationStationPath + "/music";
	mKeyboardMappingsPath = mUserEmulationStationPath + "/padtokey";
	mUserManualPath = mUserEmulationStationPath + "/notice.pdf";

#if defined(WIN32) && defined(_DEBUG)
	mSystemConfFilePath = mUserEmulationStationPath + "/batocera.conf";
#endif

	loadCustomConfiguration(false); // Try to detect alternate paths ( Decorations, Shaders... ) without loading overrides

	// Here, BATOCERA & Forks can define their own paths

#if BATOCERA
	mRootPath = "/userdata";
	mEmulationStationPath = "/usr/share/emulationstation";
	mUserEmulationStationPath = "/userdata/system/configs/emulationstation";

	mConfigPath = "/userdata/system/";
	mLogPath = "/userdata/system/logs";
	mScreenShotsPath = "/userdata/screenshots";
	mSaveStatesPath = "/userdata/saves";
	mMusicPath = "/usr/share/batocera/music";
	mUserMusicPath = "/userdata/music";
	mThemesPath = "/usr/share/emulationstation/themes";
	mUserThemesPath = "/userdata/themes";
	mKeyboardMappingsPath = "/usr/share/evmapy";
	mUserKeyboardMappingsPath = "/userdata/system/configs/evmapy";
	mDecorationsPath = "/usr/share/batocera/datainit/decorations";
	mUserDecorationsPath = "/userdata/decorations";
	mShadersPath = "/usr/share/batocera/shaders/configs";
	mUserShadersPath = "/userdata/shaders/configs";
	mTimeZonesPath = "/usr/share/zoneinfo/";
	mRetroachivementSounds = "/usr/share/libretro/assets/sounds";
	mUserRetroachivementSounds = "/userdata/sounds/retroachievements"
	
	mSystemConfFilePath = "/userdata/system/batocera.conf";
	mUserManualPath = "/usr/share/batocera/doc/notice.pdf";
	mVersionInfoPath = "/usr/share/batocera/batocera.version";
#endif

/* EmuElec sample locations.
#ifdef _ENABLEEMUELEC
	mRootPath = "/storage/roms"; // ?
	mEmulationStationPath = Utils::FileSystem::getExePath();
	mUserEmulationStationPath = Utils::FileSystem::getCanonicalPath(Utils::FileSystem::getHomePath() + "/.emulationstation");
	mLogPath = "/storage/.config/emuelec/logs";
	mThemesPath = mEmulationStationPath + "/themes";
	mUserThemesPath = "/emuelec/themes";
	mMusicPath = "/storage/roms/BGM";
	mUserMusicPath = "/storage/.config/emuelec/BGM";
	mDecorationsPath = "/storage/roms/bezels";
	mUserDecorationsPath = "/tmp/overlays/bezels";
	mVersionInfoPath = "/usr/config/EE_VERSION";
	mSystemConfFilePath = "/storage/.config/emuelec/configs/emuelec.conf";
#endif
*/
	loadCustomConfiguration(true); // Load paths overrides from emulationstation.ini file
}

void Paths::loadCustomConfiguration(bool overridesOnly)
{
	// Files
	std::map<std::string, std::string*> files =
	{
		{ "config", &mSystemConfFilePath },
		{ "manual", &mUserManualPath },
		{ "versioninfo", &mVersionInfoPath },
		{ "kodi", &mKodiPath }
	};

	std::map<std::string, std::string*> folders = 
	{
		// Folders
		{ "root", &mRootPath },
		{ "log", &mLogPath },
		{ "screenshots", &mScreenShotsPath },
		{ "saves", &mSaveStatesPath },
		{ "system.music", &mMusicPath },
		{ "music", &mUserMusicPath },
		{ "system.themes", &mThemesPath },
		{ "themes", &mUserThemesPath },
		{ "system.padtokey", &mKeyboardMappingsPath },
		{ "padtokey", &mUserKeyboardMappingsPath },
		{ "system.decorations", &mDecorationsPath },
		{ "decorations", &mUserDecorationsPath },
		{ "system.shaders", &mShadersPath },
		{ "shaders", &mUserShadersPath },
		{ "system.retroachievementsounds", &mRetroachivementSounds },
		{ "retroachievementsounds", &mUserRetroachivementSounds },
		{ "timezones", &mTimeZonesPath },
	};

	std::map<std::string, std::string> ret;

	std::string path = mEmulationStationPath + std::string("/") + SETTINGS_FILENAME;
	if (!Utils::FileSystem::exists(path))
		path = mUserEmulationStationPath + std::string("/") + SETTINGS_FILENAME;
	if (!Utils::FileSystem::exists(path))
		path = Utils::FileSystem::getParent(mUserEmulationStationPath) + std::string("/") + SETTINGS_FILENAME;

	if (!Utils::FileSystem::exists(path))
		return;
		
	std::string relativeTo = Utils::FileSystem::getParent(path);

	if (!overridesOnly)
	{
		for (auto var : folders)
		{
			std::string variable = var.first;

			if (variable == "root")
			{
				ret[variable] = Utils::FileSystem::getParent(relativeTo);
				continue;
			}

			if (variable == "log")
			{
				ret[variable] = mUserEmulationStationPath;
				continue;
			}

			if (variable == "kodi")
			{
				if (Utils::FileSystem::exists("C:\\Program Files\\Kodi\\kodi.exe"))
					ret[variable] = "C:\\Program Files\\Kodi\\kodi.exe";
				else if (Utils::FileSystem::exists("C:\\Program Files (x86)\\Kodi\\kodi.exe"))
					ret[variable] = "C:\\Program Files (x86)\\Kodi\\kodi.exe";

				continue;
			}

			if (Utils::String::startsWith(variable, "system."))
			{
				auto name = Utils::FileSystem::getGenericPath(variable.substr(7));

				auto dir = Utils::FileSystem::getCanonicalPath(mUserEmulationStationPath + "/" + name);
				if (Utils::FileSystem::isDirectory(dir))
					ret[variable] = dir;
				else
				{
					auto dir = Utils::FileSystem::getCanonicalPath(relativeTo + "/../system/" + name);
					if (Utils::FileSystem::isDirectory(dir))
						ret[variable] = dir;
				}
			}
			else
			{
				auto dir = Utils::FileSystem::getCanonicalPath(relativeTo + "/../" + variable);
				if (Utils::FileSystem::isDirectory(dir))
					ret[variable] = dir;
			}
		}

		for (auto var : files)
		{
			auto file = Utils::FileSystem::resolveRelativePath(var.first, relativeTo, true);
			if (Utils::FileSystem::exists(file) && !Utils::FileSystem::isDirectory(file))
				ret[var.first] = file;
		}
	}
	else
	{
		std::string line;
		std::ifstream systemConf(path);
		if (systemConf && systemConf.is_open())
		{
			while (std::getline(systemConf, line))
			{
				int idx = line.find("=");
				if (idx == std::string::npos || line.find("#") == 0 || line.find(";") == 0)
					continue;

				std::string key = line.substr(0, idx);
				std::string value = Utils::String::replace(line.substr(idx + 1), "\\", "/");
				if (!key.empty() && !value.empty())
				{
					auto dir = Utils::FileSystem::resolveRelativePath(value, relativeTo, true);
					if (Utils::FileSystem::isDirectory(dir))
						ret[key] = dir;
				}
			}

			systemConf.close();
		}
	}

	for (auto vv : folders)
	{
		auto it = ret.find(vv.first);
		if (it == ret.cend())
			continue;

		(*vv.second) = it->second;
	}

	for (auto vv : files)
	{
		auto it = ret.find(vv.first);
		if (it == ret.cend())
			continue;

		(*vv.second) = it->second;
	}
}

static std::string homePath;

std::string& Paths::getHomePath()
{
	if (homePath.length())
		return homePath;

#ifdef WIN32
	// Is it a portable installation ? Check if ".emulationstation/es_systems.cfg" exists in the exe's path
	if (!homePath.length())
	{
		std::string portableCfg = getExePath() + "/.emulationstation/es_systems.cfg";
		if (Utils::FileSystem::exists(portableCfg))
			homePath = getExePath();
	}
#endif

	// HOME has different usages in Linux & Windows
	// On Windows,  "HOME" is not a system variable but a user's environment variable that can be defined by users in batch files. 
	// If defined : The environment variable has priority over all
	char* envHome = getenv("HOME");
	if (envHome)
		homePath = Utils::FileSystem::getGenericPath(envHome);

#ifdef WIN32
	// On Windows, getenv("HOME") is not the system's user path but a user environment variable.
	// Instead we get the home user's path using %HOMEDRIVE%/%HOMEPATH% which are system variables.
	if (!homePath.length())
	{
		char* envHomeDrive = getenv("HOMEDRIVE");
		char* envHomePath = getenv("HOMEPATH");
		if (envHomeDrive && envHomePath)
			homePath = Utils::FileSystem::getGenericPath(std::string(envHomeDrive) + "/" + envHomePath);
	}
#endif // _WIN32

	// no homepath found, fall back to current working directory
	if (!homePath.length())
		homePath = Utils::FileSystem::getCWDPath();

	homePath = Utils::FileSystem::getGenericPath(homePath);

	// return constructed homepath
	return homePath;

} // getHomePath


void Paths::setHomePath(const std::string& _path)
{
	homePath = Utils::FileSystem::getGenericPath(_path);
}

static std::string exePath;

std::string& Paths::getExePath()
{
	return exePath;
}

void Paths::setExePath(const std::string& _path)
{
	std::string path = Utils::FileSystem::getCanonicalPath(_path);
	if (Utils::FileSystem::isRegularFile(path))
		path = Utils::FileSystem::getParent(path);

	exePath = Utils::FileSystem::getGenericPath(path);
}

std::string Paths::findEmulationStationFile(const std::string& fileName)
{
	std::string localVersionFile = Paths::getEmulationStationPath() + "/" + fileName;
	if (Utils::FileSystem::exists(localVersionFile))
		return localVersionFile;

	localVersionFile = Paths::getUserEmulationStationPath() + "/" + fileName;
	if (Utils::FileSystem::exists(localVersionFile))
		return localVersionFile;

	localVersionFile = Utils::FileSystem::getParent(Paths::getUserEmulationStationPath()) + "/" + fileName;
	if (Utils::FileSystem::exists(localVersionFile))
		return localVersionFile;

	return std::string();
}