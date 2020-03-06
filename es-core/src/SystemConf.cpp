#include "SystemConf.h"
#include <iostream>
#include <fstream>
#include "Log.h"
#include "utils/StringUtil.h"
#include  "utils/FileSystemUtil.h"
#include <regex>

SystemConf *SystemConf::sInstance = NULL;


std::string systemConfFile = "/userdata/system/batocera.conf";
std::string systemConfFileTmp = "/userdata/system/batocera.conf.tmp";

SystemConf::SystemConf() 
{
#if WIN32
	systemConfFile = Utils::FileSystem::getEsConfigPath() + "/batocera.conf";
	systemConfFileTmp = Utils::FileSystem::getEsConfigPath() + "/batocera.conf.tmp";
#endif

    loadSystemConf();
}

SystemConf *SystemConf::getInstance() {
    if (sInstance == NULL)
        sInstance = new SystemConf();

    return sInstance;
}

#include <regex>
#include <string>
#include <iostream>

bool SystemConf::loadSystemConf() 
{
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

#include <SDL_timer.h>

bool SystemConf::saveSystemConf()
{
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
				if (!val.empty() && val != "auto")
					currentLine = key + val;
				else
					currentLine = "#" + key + it.second;

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
	for (int i = 0; i < fileLines.size(); i++) {
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
    if (confMap.count(name))
        return confMap[name];
    
    return "";
}

std::string SystemConf::get(const std::string &name, const std::string &defaut) 
{
    if (confMap.count(name))
        return confMap[name];
    
    return defaut;
}

bool SystemConf::set(const std::string &name, const std::string &value) 
{
	if (confMap.count(name) == 0 || confMap[name] != value)
	{
		confMap[name] = value;
		mWasChanged = true;
		return true;
	}

	return false;
}
