#include "SystemConf.h"
#include <iostream>
#include <fstream>
#include "Log.h"
#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"
#include "Settings.h"
#include "Paths.h"

#include <set>
#include <regex>
#include <string>
#include <iostream>
#include <SDL_timer.h>

static std::string mapSettingsName(const std::string& name)
{
	if (name == "system.language")
		return "Language";

	return name;
}

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
	{ "system.hostname", "BATOCERA" }, // batocera
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

SystemConf::SystemConf() 
{
	mSystemConfFile = Paths::getSystemConfFilePath();
	if (mSystemConfFile.empty())
		return;
	
	mSystemConfFileTmp = mSystemConfFile + ".tmp";
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
	if (mSystemConfFile.empty())
		return true;

	changedConf.clear();

	std::string line;
	std::ifstream systemConf(mSystemConfFile);
	if (systemConf && systemConf.is_open()) 
	{
		while (std::getline(systemConf, line)) 
		{

			int idx = line.find("=");
			if (idx == std::string::npos || line.find("#") == 0 || line.find(";") == 0)
				continue;

			std::string key = line.substr(0, idx);
			std::string value = line.substr(idx + 1);
			if (!key.empty() && !value.empty())
				confMap[key] = value;

		}
		systemConf.close();
	}
	else
	{
		LOG(LogError) << "Unable to open " << mSystemConfFile;
		return false;
	}

	return true;
}

bool SystemConf::saveSystemConf()
{
	if (mSystemConfFile.empty())
		return Settings::getInstance()->saveFile();	

	if (changedConf.empty())
		return false;

	std::ifstream filein(mSystemConfFile); //File to read from

#ifndef WIN32
	if (!filein)
	{
		LOG(LogError) << "Unable to open for saving :  " << mSystemConfFile << "\n";
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

	static std::string removeID = "$^é(p$^mpv$êrpver$^vper$vper$^vper$vper$vper$^vperv^pervncvizn";

	int lastTime = SDL_GetTicks();

	/* Save new value if exists */
	for (auto& it : changedConf)
	{
		std::string key = it + "=";
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
				std::string val = confMap[it];
				if ((!val.empty() && val != "auto") || dontRemoveValue.find(it) != dontRemoveValue.cend())
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
			std::string val = confMap[it];
			if (!val.empty() && val != "auto")
				fileLines.push_back(key + val);
		}
	}

	lastTime = SDL_GetTicks() - lastTime;

	LOG(LogDebug) << "saveSystemConf :  " << lastTime;

	std::ofstream fileout(mSystemConfFileTmp); //Temporary file
	if (!fileout)
	{
		LOG(LogError) << "Unable to open for saving :  " << mSystemConfFileTmp << "\n";
		return false;
	}
	for (int i = 0; i < fileLines.size(); i++) 
	{
		if (fileLines[i] != removeID)
			fileout << fileLines[i] << "\n";
	}

	fileout.close();
	
	std::ifstream  src(mSystemConfFileTmp, std::ios::binary);
	std::ofstream  dst(mSystemConfFile, std::ios::binary);
	dst << src.rdbuf();

	remove(mSystemConfFileTmp.c_str());
	changedConf.clear();

	return true;
}

std::string SystemConf::get(const std::string &name) 
{
	if (mSystemConfFile.empty())
		return Settings::getInstance()->getString(mapSettingsName(name));
	
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
	if (mSystemConfFile.empty())
		return Settings::getInstance()->setString(mapSettingsName(name), value == "auto" ? "" : value);

	if (confMap.count(name) == 0 || confMap[name] != value)
	{
		confMap[name] = value;
		changedConf.insert(name);
		return true;
	}

	return false;
}

bool SystemConf::getBool(const std::string &name, bool defaultValue)
{
	if (mSystemConfFile.empty())
		return Settings::getInstance()->getBool(mapSettingsName(name));

	if (defaultValue)
		return get(name) != "0";

	return get(name) == "1";
}

bool SystemConf::setBool(const std::string &name, bool value)
{
	if (mSystemConfFile.empty())
		return Settings::getInstance()->setBool(mapSettingsName(name), value);

	return set(name, value  ? "1" : "0");
}

bool SystemConf::getIncrementalSaveStates()
{
	auto valGSS = SystemConf::getInstance()->get("global.incrementalsavestates");
	return valGSS != "0" && valGSS != "2";
}

bool SystemConf::getIncrementalSaveStatesUseCurrentSlot()
{
	return SystemConf::getInstance()->get("global.incrementalsavestates") == "2";
}