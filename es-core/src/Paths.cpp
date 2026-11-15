#include "Paths.h"
#include "Log.h"
#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

Paths* Paths::_instance = nullptr;

#define SETTINGS_FILENAME "emulationstation.ini"
#define APP_DIR "emulationstation"

static bool homePathOverridden = false;

// $VAR/emulationstation if VAR holds an absolute path, else ~/fallback/emulationstation
static std::string getUserDirectory(const char* var, const char* fallback)
{
	if (!homePathOverridden)
	{
		const char* value = getenv(var);
		if (value != nullptr && value[0] == '/')
			return Utils::FileSystem::getGenericPath(value) + "/" APP_DIR;
	}

	return Paths::getHomePath() + "/" + fallback + "/" APP_DIR;
}

// Each absolute entry of the colon separated list $VAR (or fallback), with /emulationstation appended
static void addSystemDirectories(std::vector<std::string>& list, const char* var, const char* fallback)
{
	const char* value = getenv(var);
	std::string dirs = (value != nullptr && value[0] != 0) ? value : fallback;

	for (const auto& dir : Utils::String::split(dirs, ':', true))
	{
		if (dir.empty() || dir[0] != '/')
			continue;

		std::string path = Utils::FileSystem::getGenericPath(dir) + "/" APP_DIR;
		if (std::find(list.cbegin(), list.cend(), path) == list.cend())
			list.push_back(path);
	}
}

static std::string getRuntimeDirectory()
{
	const char* value = getenv("XDG_RUNTIME_DIR");
	if (value != nullptr && value[0] == '/')
		return Utils::FileSystem::getGenericPath(value) + "/" APP_DIR;

	// No session runtime dir (started outside a login session): use a private folder in /tmp
	return "/tmp/" APP_DIR "-" + std::to_string(getuid());
}

static std::string firstExistingDirectory(const std::vector<std::string>& roots, const std::string& name)
{
	for (const auto& root : roots)
	{
		std::string path = root + "/" + name;
		if (Utils::FileSystem::isDirectory(path))
			return path;
	}

	return std::string();
}

Paths::Paths()
{
	mConfigPath = getUserDirectory("XDG_CONFIG_HOME", ".config");
	mDataPath = getUserDirectory("XDG_DATA_HOME", ".local/share");
	mCachePath = getUserDirectory("XDG_CACHE_HOME", ".cache");
	mStatePath = getUserDirectory("XDG_STATE_HOME", ".local/state");
	mRuntimePath = getRuntimeDirectory();

	addSystemDirectories(mSystemConfigPaths, "XDG_CONFIG_DIRS", "/etc/xdg");
	addSystemDirectories(mSystemDataPaths, "XDG_DATA_DIRS", "/usr/local/share:/usr/share");
#ifdef ES_INSTALL_DATADIR
	addSystemDirectories(mSystemDataPaths, "", ES_INSTALL_DATADIR);
#endif

	mEmulationStationPath = getExePath();
	mRootPath = getHomePath();

	mLogPath = mStatePath;
	mUserThemesPath = mDataPath + "/themes";
	mThemesPath = firstExistingDirectory(mSystemDataPaths, "themes");
	mUserMusicPath = mDataPath + "/music";
	mMusicPath = firstExistingDirectory(mSystemDataPaths, "music");
	mKeyboardMappingsPath = mConfigPath + "/padtokey";
	mUserKeyboardMappingsPath = mConfigPath + "/padtokey";
	mBiosPath = mDataPath + "/bios";
	mScreenShotsPath = mDataPath + "/screenshots";
	mSaveStatesPath = mDataPath + "/saves";
	mUserRetroachivementSounds = mDataPath + "/sounds/retroachievements";
	mUserManualPath = mDataPath + "/notice.pdf";

	// RetroArch shader presets of the player (system ones: "system.shaders" in the paths file)
	mUserShadersPath = mDataPath + "/shaders";
	// Decoration sets (RetroArch overlays, The Bezel Project...), one folder per set
	mUserDecorationsPath = mDataPath + "/decorations";

	// RetroArch's video filters: the player's own, then the ones the distribution's RetroArch ships
	mUserVideoFiltersPath = mDataPath + "/videofilters";
	for (const char* dir : { "/usr/lib/retroarch/filters/video", "/usr/lib/x86_64-linux-gnu/retroarch/filters/video", "/usr/lib/aarch64-linux-gnu/retroarch/filters/video" })
	{
		if (Utils::FileSystem::isDirectory(dir))
		{
			mVideoFiltersPath = dir;
			break;
		}
	}

	loadCustomConfiguration(false); // Try to detect alternate paths ( Decorations, Shaders... ) without loading overrides
	loadCustomConfiguration(true); // Load paths overrides from emulationstation.ini file
}

// The distribution can ship default port scripts in <datadir>/emulationstation/ports.
// Copy them to ~/ROMs/ports on the first start, when the folder does not exist yet:
// a user who removes a script later does not get it back on every launch.
static void installDefaultPorts()
{
	const std::string portsPath = Paths::getHomePath() + "/ROMs/ports";
	if (Utils::FileSystem::isDirectory(portsPath))
		return;

	Utils::FileSystem::createDirectory(portsPath);
	if (!Utils::FileSystem::isDirectory(portsPath))
		return;

	std::set<std::string> copied;
	for (const auto& root : Paths::getSystemDataPaths())
	{
		const std::string defaults = root + "/ports";
		if (!Utils::FileSystem::isDirectory(defaults))
			continue;

		for (const auto& file : Utils::FileSystem::getDirContent(defaults, false, false))
		{
			if (Utils::FileSystem::isDirectory(file))
				continue;

			const std::string name = Utils::FileSystem::getFileName(file);
			if (!copied.insert(name).second)
				continue;

			const std::string destination = Utils::FileSystem::combine(portsPath, name);
			if (Utils::FileSystem::exists(destination))
				continue;

			if (!Utils::FileSystem::copyFile(file, destination))
			{
				std::cerr << "Could not install port \"" << destination << "\"\n";
				continue;
			}

			chmod(destination.c_str(), 0755);
		}
	}
}

bool Paths::createUserDirectories()
{
	bool ok = true;

	// The XDG roots are required: without them ES cannot store anything.
	for (const auto& dir : { getConfigPath(), getDataPath(), getCachePath(), getStatePath(), getRuntimePath() })
	{
		if (Utils::FileSystem::isDirectory(dir))
			continue;

		Utils::FileSystem::createDirectory(dir);
		if (!Utils::FileSystem::isDirectory(dir))
		{
			std::cerr << "Could not create directory \"" << dir << "\"\n";
			ok = false;
		}
	}

	// Everything a new install needs: the folders the menus and launchers write
	// to, and the default ROM folder of the shipped es_systems.cfg. A home
	// directory that does not allow one of them (a file named ROMs, a read-only
	// media folder...) must not stop ES from starting.
	const std::vector<std::string> optional = {
		getBiosPath(),
		getSavesPath(),
		getScreenShotPath(),
		getUserThemesPath(),
		getUserMusicPath(),
		getUserShadersPath(),
		getUserDecorationsPath(),
		getUserVideoFilters(),
		getUserKeyboardMappingsPath(),
		getUserRetroachivementSounds(),
		getDataPath() + "/gamelists",
		getDataPath() + "/collections",
		getDataPath() + "/emulators",
		getDataPath() + "/cores",
		getConfigPath() + "/emulators",
		getHomePath() + "/ROMs",
	};

	for (const auto& dir : optional)
	{
		if (dir.empty() || Utils::FileSystem::isDirectory(dir))
			continue;

		Utils::FileSystem::createDirectory(dir);
		if (!Utils::FileSystem::isDirectory(dir))
			std::cerr << "Could not create directory \"" << dir << "\"\n";
	}

	installDefaultPorts();

	// The runtime dir holds flag files and must stay private (XDG spec: mode 0700)
	chmod(getRuntimePath().c_str(), S_IRWXU);
	return ok;
}

std::string Paths::findSystemFile(const std::string& fileName)
{
	std::vector<std::string> roots = { getConfigPath(), getEmulationStationPath() };
	roots.insert(roots.end(), getSystemConfigPaths().cbegin(), getSystemConfigPaths().cend());
	roots.insert(roots.end(), getSystemDataPaths().cbegin(), getSystemDataPaths().cend());

	for (const auto& root : roots)
	{
		if (root.empty())
			continue;

		std::string path = root + "/" + fileName;
		if (Utils::FileSystem::exists(path))
			return path;
	}

	return std::string();
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
		{ "bios", &mBiosPath },
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
		{ "system.videofilters", &mVideoFiltersPath },
		{ "videofilters", &mUserVideoFiltersPath },
		{ "system.retroachievementsounds", &mRetroachivementSounds },
		{ "retroachievementsounds", &mUserRetroachivementSounds },
		{ "timezones", &mTimeZonesPath },
	};

	std::map<std::string, std::string> ret;

	std::string path = mEmulationStationPath + std::string("/") + SETTINGS_FILENAME;
	if (!Utils::FileSystem::exists(path))
		path = mConfigPath + std::string("/") + SETTINGS_FILENAME;

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
				ret[variable] = mStatePath;
				continue;
			}


			if (Utils::String::startsWith(variable, "system."))
			{
				auto name = Utils::FileSystem::getGenericPath(variable.substr(7));

				auto dir = Utils::FileSystem::getCanonicalPath(mConfigPath + "/" + name);
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
				else
				{
					auto dir = Utils::FileSystem::getCanonicalPath(relativeTo + "/../system/" + variable);
					if (Utils::FileSystem::isDirectory(dir))
						ret[variable] = dir;
				}
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
				size_t idx = line.find("=");
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


	char* envHome = getenv("HOME");
	if (envHome)
		homePath = Utils::FileSystem::getGenericPath(envHome);


	// no homepath found, fall back to current working directory
	if (!homePath.length())
		homePath = Utils::FileSystem::getCWDPath();

	homePath = Utils::FileSystem::getGenericPath(homePath);

	// return constructed homepath
	return homePath;

} // getHomePath


void Paths::setHomePath(const std::string& _path)
{
	homePathOverridden = true;
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
	// Install files (version.info, about.info...): executable dir first, then the system data dirs
	std::vector<std::string> roots = { getEmulationStationPath() };
	roots.insert(roots.end(), getSystemDataPaths().cbegin(), getSystemDataPaths().cend());
	roots.push_back(getConfigPath());

	for (const auto& root : roots)
	{
		if (root.empty())
			continue;

		std::string path = root + "/" + fileName;
		if (Utils::FileSystem::exists(path))
			return path;
	}

	return std::string();
}
