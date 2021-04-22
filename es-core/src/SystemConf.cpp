#include "SystemConf.h"
#include <iostream>
#include <fstream>
#include "Log.h"
#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"

#include <set>
#include <regex>
#include <string>
#include <iostream>
#include <SDL_timer.h>


#if WIN32 & !_DEBUG
	// NOBATOCERACONF routes all SystemConf to es_settings for Windows Release version

	#include "Settings.h"
	#define NOBATOCERACONF

	std::string mapSettingsName(const std::string name)
	{
		if (name == "system.language")
			return "Language";

		return name;
	}
#endif

SystemConf *SystemConf::sInstance = NULL;

static std::set<std::string> dontRemoveValue
{
	{ "audio.device" }
};

static std::map<std::string, std::string> defaults =
{
	{ "kodi.enabled", "1" },
	{ "kodi.atstartup", "0" },
	{ "audio.bgmusic", "1" },
	{ "wifi.enabled", "0" },
	{ "system.hostname", "BATOCERA" },
	{ "global.retroachievements", "0" },
	{ "global.retroachievements.hardcore", "0" },
	{ "global.retroachievements.leaderboards", "0" },
	{ "global.retroachievements.verbose", "0" },
	{ "global.retroachievements.screenshot", "0" },
	{ "global.retroachievements.username", "" },
	{ "global.retroachievements.password", "" },
	{ "global.netplay_public_announce", "1" },
	{ "global.ai_service_enabled", "0" },
};

#ifdef _ENABLEEMUELEC
std::string systemConfFile = "/storage/.config/emuelec/configs/emuelec.conf";
std::string systemConfFileTmp = "/storage/.config/emuelec/configs/emuelec.conf.tmp";
#else
std::string systemConfFile = "/userdata/system/batocera.conf";
std::string systemConfFileTmp = "/userdata/system/batocera.conf.tmp";
#endif

SystemConf::SystemConf() 
{
#if WIN32
	systemConfFile = Utils::FileSystem::getEsConfigPath() + "/batocera.conf";
	systemConfFileTmp = Utils::FileSystem::getEsConfigPath() + "/batocera.conf.tmp";
#endif

    loadSystemConf();
}

SystemConf *SystemConf::getInstance() 
{
    if (sInstance == NULL)
        sInstance = new SystemConf();

    return sInstance;
}

bool SystemConf::loadSystemConf() 
{
#ifdef NOBATOCERACONF
	return true;
#endif

	mWasChanged = false;

    std::string line;
    std::ifstream systemConf(systemConfFile);
    if (systemConf && systemConf.is_open()) {
        while (std::getline(systemConf, line)) {

			int idx = line.find("=");
			if (idx == std::string::npos || line.find("#") == 0 || line.find(";") == 0)
				continue;
			
			std::string key = line.substr(0, idx);
			std::string value = line.substr(idx+1);
			if (!key.empty() && !value.empty())
				confMap[key] = line.substr(idx + 1);

        }
        systemConf.close();
    } else {
        LOG(LogError) << "Unable to open " << systemConfFile;
        return false;
    }
    return true;
}

bool SystemConf::saveSystemConf()
{
#ifdef NOBATOCERACONF
	return Settings::getInstance()->saveFile();	
#endif

	if (!mWasChanged)
		return false;

	std::ifstream filein(systemConfFile); //File to read from

#ifndef WIN32
	if (!filein)
	{
		LOG(LogError) << "Unable to open for saving :  " << systemConfFile << "\n";
		return false;
	}
#endif

	/* Read all lines in a vector */
	std::vector<std::string> fileLines;
	std::string line;

	if (filein)
	{
		while (std::getline(filein, line))
			fileLines.push_back(line);

		filein.close();
	}

	static std::string removeID = "$^�(p$^mpv$�rpver$^vper$vper$^vper$vper$vper$^vperv^pervncvizn";

	int lastTime = SDL_GetTicks();

	/* Save new value if exists */
	for (auto& it : confMap)
	{
		std::string key = it.first + "=";		
		char key0 = key[0];

		bool lineFound = false;

		for (auto& currentLine : fileLines)
		{
			if (currentLine.size() < 3)
				continue;

			char fc = currentLine[0];
			if (fc != key0 && currentLine[1] != key0)
				continue;

			int idx = currentLine.find(key);
			if (idx == std::string::npos)
				continue;

			if (idx == 0 || (idx == 1 && (fc == ';' || fc == '#')))
			{
				std::string val = it.second;
				if ((!val.empty() && val != "auto") || dontRemoveValue.find(it.first) != dontRemoveValue.cend())
				{
					auto defaultValue = defaults.find(key);
					if (defaultValue != defaults.cend() && defaultValue->second == val)
						currentLine = removeID;
					else
						currentLine = key + val;
				}
				else 
					currentLine = removeID;

				lineFound = true;
			}
		}

		if (!lineFound)
		{
			std::string val = it.second;
			if (!val.empty() && val != "auto")
				fileLines.push_back(key + val);
		}
	}

	lastTime = SDL_GetTicks() - lastTime;

	LOG(LogDebug) << "saveSystemConf :  " << lastTime;

	std::ofstream fileout(systemConfFileTmp); //Temporary file
	if (!fileout)
	{
		LOG(LogError) << "Unable to open for saving :  " << systemConfFileTmp << "\n";
		return false;
	}
	for (int i = 0; i < fileLines.size(); i++) 
	{
		if (fileLines[i] != removeID)
			fileout << fileLines[i] << "\n";
	}

	fileout.close();

	/* Copy back the tmp to batocera.conf */
	std::ifstream  src(systemConfFileTmp, std::ios::binary);
	std::ofstream  dst(systemConfFile, std::ios::binary);
	dst << src.rdbuf();

	remove(systemConfFileTmp.c_str());
	mWasChanged = false;

	return true;
}

std::string SystemConf::get(const std::string &name) 
{
#ifdef NOBATOCERACONF
	return Settings::getInstance()->getString(mapSettingsName(name));
#endif
	
	auto it = confMap.find(name);
	if (it != confMap.cend())
		return it->second;

	auto dit = defaults.find(name);
	if (dit != defaults.cend())
		return dit->second;

    return "";
}

bool SystemConf::set(const std::string &name, const std::string &value) 
{
#ifdef NOBATOCERACONF
	return Settings::getInstance()->setString(mapSettingsName(name), value == "auto" ? "" : value);
#endif

	if (confMap.count(name) == 0 || confMap[name] != value)
	{
		confMap[name] = value;
		mWasChanged = true;
		return true;
	}

	return false;
}

bool SystemConf::getBool(const std::string &name, bool defaultValue)
{
#ifdef NOBATOCERACONF
	return Settings::getInstance()->getBool(mapSettingsName(name));
#endif

	if (defaultValue)
		return get(name) != "0";

	return get(name) == "1";
}

bool SystemConf::setBool(const std::string &name, bool value)
{
#ifdef NOBATOCERACONF	
	return Settings::getInstance()->setBool(mapSettingsName(name), value);
#endif

	return set(name, value  ? "1" : "0");
}
