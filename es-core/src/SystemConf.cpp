#include "SystemConf.h"
#include <iostream>
#include <fstream>
#include <boost/regex.hpp>
#include "Log.h"
#include <boost/algorithm/string/predicate.hpp>

SystemConf *SystemConf::sInstance = NULL;
boost::regex validLine("^(?<key>[^;|#].*?)=(?<val>.*?)$");
boost::regex commentLine("^;(?<key>.*?)=(?<val>.*?)$");

std::string systemConfFile = "/userdata/system/batocera.conf";
std::string systemConfFileTmp = "/userdata/system/batocera.conf.tmp";

SystemConf::SystemConf() {
    loadSystemConf();
}

SystemConf *SystemConf::getInstance() {
    if (sInstance == NULL)
        sInstance = new SystemConf();

    return sInstance;
}

bool SystemConf::loadSystemConf() {
    std::string line;
    std::ifstream systemConf(systemConfFile);
    if (systemConf && systemConf.is_open()) {
        while (std::getline(systemConf, line)) {
            boost::smatch lineInfo;
            if (boost::regex_match(line, lineInfo, validLine)) {
                confMap[std::string(lineInfo["key"])] = std::string(lineInfo["val"]);
            }
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

            if (boost::starts_with(currentLine, key+"=") || boost::starts_with(currentLine, ";"+key+"=") || boost::starts_with(currentLine, "#"+key+"=") ){
	      if(val != "" && val != "auto") {
                fileLines[i] = key + "=" + val;
	      } else {
                fileLines[i] = "#" + key + "=" + val;
	      }
	      lineFound = true;
            }
        }
        if(!lineFound) {
	  if(val != "" && val != "auto") {
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
    std::ofstream  dst(systemConfFile,   std::ios::binary);
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
