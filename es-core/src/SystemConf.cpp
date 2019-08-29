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
	systemConfFile = Utils::FileSystem::getExePath() + "/batocera.conf";
	systemConfFileTmp = Utils::FileSystem::getExePath() + "/batocera.conf.tmp";
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

bool SystemConf::loadSystemConf() {

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


bool SystemConf::saveSystemConf() {
	std::ifstream filein(systemConfFile); //File to read from
	if (!filein) {
		LOG(LogError) << "Unable to open for saving :  " << systemConfFile << "\n";
		return false;
	}
	/* Read all lines in a vector */
	std::vector<std::string> fileLines;
	std::string line;
	while (std::getline(filein, line)) {
		fileLines.push_back(line);
	}
	filein.close();


	/* Save new value if exists */
	for (std::map<std::string, std::string>::iterator it = confMap.begin(); it != confMap.end(); ++it) {
		std::string key = it->first;
		std::string val = it->second;
		bool lineFound = false;
		for (int i = 0; i < fileLines.size(); i++) {
			std::string currentLine = fileLines[i];

			if (Utils::String::startsWith(currentLine, key + "=") || Utils::String::startsWith(currentLine, ";" + key + "=") || Utils::String::startsWith(currentLine, "#" + key + "=")) {
				if (val != "" && val != "auto") {
					fileLines[i] = key + "=" + val;
				}
				else {
					fileLines[i] = "#" + key + "=" + val;
				}
				lineFound = true;
			}
		}
		if (!lineFound) {
			if (val != "" && val != "auto") {
				fileLines.push_back(key + "=" + val);
			}
		}
	}
	std::ofstream fileout(systemConfFileTmp); //Temporary file
	if (!fileout) {
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

	return true;
}

std::string SystemConf::get(const std::string &name) {
    if (confMap.count(name)) {
        return confMap[name];
    }
    return "";
}
std::string SystemConf::get(const std::string &name, const std::string &defaut) {
    if (confMap.count(name)) {
        return confMap[name];
    }
    return defaut;
}

void SystemConf::set(const std::string &name, const std::string &value) {
    confMap[name] = value;
}
