/*
 * File:   RecalboxSystem.cpp
 * Author: digitallumberjack
 * 
 * Created on 29 novembre 2014, 03:15
 */

#include "RecalboxSystem.h"
#include <stdlib.h>
#if !defined(WIN32)
#include <sys/statvfs.h>
#endif
#include <sstream>
#include "Settings.h"
#include <iostream>
#include <fstream>
#include "Log.h"
#include "HttpReq.h"

#include "AudioManager.h"
#include "VolumeControl.h"
#include "InputManager.h"

#include <stdio.h>
#include <sys/types.h>
#if !defined(WIN32)
#include <ifaddrs.h>
#include <netinet/in.h>
#endif
#include <string.h>
#if !defined(WIN32)
#include <arpa/inet.h>
#endif
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include <fstream>

#if defined(WIN32)
#define popen _popen
#define pclose _pclose
#endif

RecalboxSystem::RecalboxSystem() {
}

RecalboxSystem *RecalboxSystem::instance = NULL;


RecalboxSystem *RecalboxSystem::getInstance() {
    if (RecalboxSystem::instance == NULL) {
        RecalboxSystem::instance = new RecalboxSystem();
    }
    return RecalboxSystem::instance;
}

unsigned long RecalboxSystem::getFreeSpaceGB(std::string mountpoint) {
#if !defined(WIN32)
    struct statvfs fiData;
    const char *fnPath = mountpoint.c_str();
    int free = 0;
    if ((statvfs(fnPath, &fiData)) >= 0) {
        free = (fiData.f_bfree * fiData.f_bsize) / (1024 * 1024 * 1024);
    }
    return free;
#else
    return 0;
#endif
}

std::string RecalboxSystem::getFreeSpaceInfo() {
    std::string sharePart = Settings::getInstance()->getString("SharePartition");
    if (sharePart.size() > 0) {
        const char *fnPath = sharePart.c_str();
#if !defined(WIN32)
        struct statvfs fiData;
        if ((statvfs(fnPath, &fiData)) < 0) {
            return "";
        } else {
            unsigned long total = (fiData.f_blocks * (fiData.f_bsize / 1024)) / (1024L * 1024L);
            unsigned long free = (fiData.f_bfree * (fiData.f_bsize / 1024)) / (1024L * 1024L);
            unsigned long used = total - free;
            unsigned long percent = 0;
            std::ostringstream oss;
            if (total != 0){  //for small SD card ;) with share < 1GB
				percent = used * 100 / total;
                oss << used << "GB/" << total << "GB (" << percent << "%)";
			}
			else
			    oss << "N/A";
            return oss.str();
        }
#else
        (void)(fnPath);
        return "TODO";
#endif
    } else {
        return "ERROR";
    }
}

bool RecalboxSystem::isFreeSpaceLimit() {
    std::string sharePart = Settings::getInstance()->getString("SharePartition");
    if (sharePart.size() > 0) {
        return getFreeSpaceGB(sharePart) < 2;
    } else {
        return "ERROR";
    }

}

std::string RecalboxSystem::getVersion() {
    std::string version = Settings::getInstance()->getString("VersionFile");
    if (version.size() > 0) {
        std::ifstream ifs(version);

        if (ifs.good()) {
            std::string contents;
            std::getline(ifs, contents);
            return contents;
        }
    }
    return "";
}

bool RecalboxSystem::needToShowVersionMessage() {
    createLastVersionFileIfNotExisting();
    std::string versionFile = Settings::getInstance()->getString("LastVersionFile");
    if (versionFile.size() > 0) {
        std::ifstream lvifs(versionFile);
        if (lvifs.good()) {
            std::string lastVersion;
            std::getline(lvifs, lastVersion);
            std::string currentVersion = getVersion();
            if (lastVersion == currentVersion) {
                return false;
            }
        }
    }
    return true;
}

bool RecalboxSystem::createLastVersionFileIfNotExisting() {
    std::string versionFile = Settings::getInstance()->getString("LastVersionFile");

    FILE *file;
    if (file = fopen(versionFile.c_str(), "r")) {
        fclose(file);
        return true;
    }
    return updateLastVersionFile();
}

bool RecalboxSystem::updateLastVersionFile() {
    std::string versionFile = Settings::getInstance()->getString("LastVersionFile");
    std::string currentVersion = getVersion();
    std::ostringstream oss;
    oss << "echo " << currentVersion << " > " << versionFile;
    if (system(oss.str().c_str())) {
        LOG(LogWarning) << "Error executing " << oss.str().c_str();
        return false;
    } else {
        LOG(LogError) << "Last version file updated";
        return true;
    }
}

std::string RecalboxSystem::getVersionMessage() {
    std::string versionMessageFile = Settings::getInstance()->getString("VersionMessage");
    if (versionMessageFile.size() > 0) {
        std::ifstream ifs(versionMessageFile);

        if (ifs.good()) {
            std::string contents((std::istreambuf_iterator<char>(ifs)),
                                 std::istreambuf_iterator<char>());
            return contents;
        }
    }
    return "";

}

bool RecalboxSystem::setOverscan(bool enable) {

    std::ostringstream oss;
    oss << Settings::getInstance()->getString("RecalboxSettingScript") << " " << "overscan";
    if (enable) {
        oss << " " << "enable";
    } else {
        oss << " " << "disable";
    }
    std::string command = oss.str();
    LOG(LogInfo) << "Launching " << command;
    if (system(command.c_str())) {
        LOG(LogWarning) << "Error executing " << command;
        return false;
    } else {
        LOG(LogInfo) << "Overscan set to : " << enable;
        return true;
    }

}

bool RecalboxSystem::setOverclock(std::string mode) {
    if (mode != "") {
        std::ostringstream oss;
        oss << Settings::getInstance()->getString("RecalboxSettingScript") << " "
        << "overclock" << " " << mode;
        std::string command = oss.str();
        LOG(LogInfo) << "Launching " << command;
        if (system(command.c_str())) {
            LOG(LogWarning) << "Error executing " << command;
            return false;
        } else {
            LOG(LogInfo) << "Overclocking set to " << mode;
            return true;
        }
    }

    return false;
}


std::pair<std::string,int> RecalboxSystem::updateSystem(BusyComponent* ui) {
    std::string updatecommand = Settings::getInstance()->getString("UpdateCommand");
    FILE *pipe = popen(updatecommand.c_str(), "r");
    char line[1024] = "";
    if (pipe == NULL) {
        return std::pair<std::string,int>(std::string("Cannot call update command"),-1);
    }

    FILE *flog = fopen("/recalbox/share/system/logs/recalbox-upgrade.log", "w");
    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
	if(flog != NULL) fprintf(flog, "%s\n", line);
        ui->setText(std::string(line));
    }
    if(flog != NULL) fclose(flog);

    int exitCode = pclose(pipe);
    return std::pair<std::string,int>(std::string(line), exitCode);
}

std::pair<std::string,int> RecalboxSystem::backupSystem(BusyComponent* ui, std::string device) {
    std::string updatecommand = std::string("/recalbox/scripts/recalbox-sync.sh sync ") + device;
    FILE *pipe = popen(updatecommand.c_str(), "r");
    char line[1024] = "";
    if (pipe == NULL) {
        return std::pair<std::string,int>(std::string("Cannot call sync command"),-1);
    }

    FILE *flog = fopen("/recalbox/share/system/logs/recalbox-sync.log", "w");
    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
	if(flog != NULL) fprintf(flog, "%s\n", line);
        ui->setText(std::string(line));
    }
    if(flog != NULL) fclose(flog);

    int exitCode = pclose(pipe);
    return std::pair<std::string,int>(std::string(line), exitCode);
}

std::pair<std::string,int> RecalboxSystem::installSystem(BusyComponent* ui, std::string device, std::string architecture) {
    std::string updatecommand = std::string("/recalbox/scripts/recalbox-install.sh install ") + device + " " + architecture;
    FILE *pipe = popen(updatecommand.c_str(), "r");
    char line[1024] = "";
    if (pipe == NULL) {
        return std::pair<std::string,int>(std::string("Cannot call install command"),-1);
    }

    FILE *flog = fopen("/recalbox/share/system/logs/recalbox-install.log", "w");
    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
	if(flog != NULL) fprintf(flog, "%s\n", line);
        ui->setText(std::string(line));
    }
    if(flog != NULL) fclose(flog);

    int exitCode = pclose(pipe);
    return std::pair<std::string,int>(std::string(line), exitCode);
}

std::pair<std::string,int> RecalboxSystem::scrape(BusyComponent* ui) {
  std::string scrapecommand = std::string("/recalbox/scripts/recalbox-scraper.sh");
  FILE *pipe = popen(scrapecommand.c_str(), "r");
  char line[1024] = "";
  if (pipe == NULL) {
    return std::pair<std::string,int>(std::string("Cannot call scrape command"),-1);
  }
  
  FILE *flog = fopen("/recalbox/share/system/logs/recalbox-scrape.log", "w");
  while (fgets(line, 1024, pipe)) {
    strtok(line, "\n");
    if(flog != NULL) fprintf(flog, "%s\n", line);

    if(boost::starts_with(line, "GAME: ")) {
      ui->setText(std::string(line));
    }
  }
  if(flog != NULL) fclose(flog);
  
  int exitCode = pclose(pipe);
  return std::pair<std::string,int>(std::string(line), exitCode);
}

bool RecalboxSystem::ping() {
    std::string updateserver = Settings::getInstance()->getString("UpdateServer");
    std::string s("ping -c 1 " + updateserver);
    int exitcode = system(s.c_str());
    return exitcode == 0;
}

bool RecalboxSystem::canUpdate() {
    std::ostringstream oss;
    oss << Settings::getInstance()->getString("RecalboxSettingScript") << " " << "canupdate";
    std::string command = oss.str();
    LOG(LogInfo) << "Launching " << command;
    if (system(command.c_str()) == 0) {
        LOG(LogInfo) << "Can update ";
        return true;
    } else {
        LOG(LogInfo) << "Cannot update ";
        return false;
    }
}

bool RecalboxSystem::launchKodi(Window *window) {

    LOG(LogInfo) << "Attempting to launch kodi...";


    AudioManager::getInstance()->deinit();
    VolumeControl::getInstance()->deinit();

    std::string commandline = InputManager::getInstance()->configureEmulators();
    std::string command = "configgen -system kodi -rom '' " + commandline;

    window->deinit();
    int exitCode = system(command.c_str());
#if !defined(WIN32)
    // WIFEXITED returns a nonzero value if the child process terminated normally with exit or _exit.
    // https://www.gnu.org/software/libc/manual/html_node/Process-Completion-Status.html
    if (WIFEXITED(exitCode)) {
        exitCode = WEXITSTATUS(exitCode);
    }
#endif

    window->init();
    VolumeControl::getInstance()->init();
    AudioManager::getInstance()->resumeMusic();
    window->normalizeNextUpdate();

    // handle end of kodi
    switch (exitCode) {
        case 10: // reboot code
            reboot();
            return true;
            break;
        case 11: // shutdown code
            shutdown();
            return true;
            break;
    }

    return exitCode == 0;

}

bool RecalboxSystem::launchFileManager(Window *window) {
    LOG(LogInfo) << "Attempting to launch filemanager...";

    AudioManager::getInstance()->deinit();
    VolumeControl::getInstance()->deinit();

    std::string command = "/recalbox/scripts/filemanagerlauncher.sh";

    window->deinit();

    int exitCode = system(command.c_str());
#if !defined(WIN32)
    if (WIFEXITED(exitCode)) {
        exitCode = WEXITSTATUS(exitCode);
    }
#endif

    window->init();
    VolumeControl::getInstance()->init();
    AudioManager::getInstance()->resumeMusic();
    window->normalizeNextUpdate();

    return exitCode == 0;
}

bool RecalboxSystem::enableWifi(std::string ssid, std::string key) {
    std::ostringstream oss;
    boost::replace_all(ssid, "\"", "\\\"");
    boost::replace_all(key, "\"", "\\\"");
    oss << Settings::getInstance()->getString("RecalboxSettingScript") << " "
    << "wifi" << " "
    << "enable" << " \""
    << ssid << "\" \"" << key << "\"";
    std::string command = oss.str();
    LOG(LogInfo) << "Launching " << command;
    if (system(command.c_str()) == 0) {
        LOG(LogInfo) << "Wifi enabled ";
        return true;
    } else {
        LOG(LogInfo) << "Cannot enable wifi ";
        return false;
    }
}

bool RecalboxSystem::disableWifi() {
    std::ostringstream oss;
    oss << Settings::getInstance()->getString("RecalboxSettingScript") << " "
    << "wifi" << " "
    << "disable";
    std::string command = oss.str();
    LOG(LogInfo) << "Launching " << command;
    if (system(command.c_str()) == 0) {
        LOG(LogInfo) << "Wifi disabled ";
        return true;
    } else {
        LOG(LogInfo) << "Cannot disable wifi ";
        return false;
    }
}


bool RecalboxSystem::halt(bool reboot, bool fast) {
    SDL_Event *quit = new SDL_Event();
    if (fast)
        if (reboot)
            quit->type = SDL_FAST_QUIT | SDL_RB_REBOOT;
        else
            quit->type = SDL_FAST_QUIT | SDL_RB_SHUTDOWN;
    else
        if (reboot)
            quit->type = SDL_QUIT | SDL_RB_REBOOT;
        else
            quit->type = SDL_QUIT | SDL_RB_SHUTDOWN;
    SDL_PushEvent(quit);
    return 0;
}

bool RecalboxSystem::reboot() {
    return halt(true, false);
}

bool RecalboxSystem::fastReboot() {
    return halt(true, true);
}

bool RecalboxSystem::shutdown() {
    return halt(false, false);
}

bool RecalboxSystem::fastShutdown() {
    return halt(false, true);
}


std::string RecalboxSystem::getIpAdress() {
#if !defined(WIN32)
    struct ifaddrs *ifAddrStruct = NULL;
    struct ifaddrs *ifa = NULL;
    void *tmpAddrPtr = NULL;

    std::string result = "NOT CONNECTED";
    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr = &((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
            if (std::string(ifa->ifa_name).find("eth") != std::string::npos ||
                std::string(ifa->ifa_name).find("wlan") != std::string::npos) {
                result = std::string(addressBuffer);
            }
        }
    }
    // Seeking for ipv6 if no IPV4
    if (result.compare("NOT CONNECTED") == 0) {
        for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) {
                continue;
            }
            if (ifa->ifa_addr->sa_family == AF_INET6) { // check it is IP6
                // is a valid IP6 Address
                tmpAddrPtr = &((struct sockaddr_in6 *) ifa->ifa_addr)->sin6_addr;
                char addressBuffer[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
                printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
                if (std::string(ifa->ifa_name).find("eth") != std::string::npos ||
                    std::string(ifa->ifa_name).find("wlan") != std::string::npos) {
                    return std::string(addressBuffer);
                }
            }
        }
    }
    if (ifAddrStruct != NULL) freeifaddrs(ifAddrStruct);
    return result;
#else
    return std::string();
#endif
}

std::vector<std::string> *RecalboxSystem::scanBluetooth() {
    std::vector<std::string> *res = new std::vector<std::string>();
    std::ostringstream oss;
    oss << Settings::getInstance()->getString("RecalboxSettingScript") << " " << "hcitoolscan";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return NULL;
    }

    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        res->push_back(std::string(line));
    }
    pclose(pipe);

    return res;
}

bool RecalboxSystem::pairBluetooth(std::string &controller) {
    std::ostringstream oss;
    oss << Settings::getInstance()->getString("RecalboxSettingScript") << " " << "hiddpair" << " " << controller;
    int exitcode = system(oss.str().c_str());
    return exitcode == 0;
}

std::vector<std::string> RecalboxSystem::getAvailableStorageDevices() {

    std::vector<std::string> res;
    std::ostringstream oss;
    oss << Settings::getInstance()->getString("RecalboxSettingScript") << " " << "storage list";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return res;
    }

    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        res.push_back(std::string(line));
    }
    pclose(pipe);

    return res;
}

std::vector<std::string> RecalboxSystem::getAvailableBackupDevices() {

    std::vector<std::string> res;
    std::ostringstream oss;
    oss << "/recalbox/scripts/recalbox-sync.sh list";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return res;
    }

    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        res.push_back(std::string(line));
    }
    pclose(pipe);

    return res;
}

std::vector<std::string> RecalboxSystem::getAvailableInstallDevices() {

    std::vector<std::string> res;
    std::ostringstream oss;
    oss << "/recalbox/scripts/recalbox-install.sh listDisks";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return res;
    }

    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        res.push_back(std::string(line));
    }
    pclose(pipe);

    return res;
}

std::vector<std::string> RecalboxSystem::getAvailableInstallArchitectures() {

    std::vector<std::string> res;
    std::ostringstream oss;
    oss << "/recalbox/scripts/recalbox-install.sh listArchs";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return res;
    }

    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        res.push_back(std::string(line));
    }
    pclose(pipe);

    return res;
}

std::vector<std::string> RecalboxSystem::getSystemInformations() {
    std::vector<std::string> res;
    FILE *pipe = popen("/recalbox/scripts/recalbox-info.sh", "r");
    char line[1024];

    if (pipe == NULL) {
      return res;
    }

    while(fgets(line, 1024, pipe)) {
      strtok(line, "\n");
      res.push_back(std::string(line));
    }
    pclose(pipe);

    return res;
}

std::vector<BiosSystem> RecalboxSystem::getBiosInformations() {
  std::vector<BiosSystem> res;
  BiosSystem current;
  bool isCurrent = false;

  FILE *pipe = popen("/recalbox/scripts/recalbox-systems.py", "r");
  char line[1024];

  if (pipe == NULL) {
    return res;
  }

  while(fgets(line, 1024, pipe)) {
    strtok(line, "\n");
    if(boost::starts_with(line, "> ")) {
      if(isCurrent) {
	res.push_back(current);
      }
      isCurrent = true;
      current.name = std::string(std::string(line).substr(2));
      current.bios.clear();
    } else {
      BiosFile biosFile;
      std::vector<std::string> tokens;
      boost::split( tokens, line, boost::is_any_of(" "));

      if(tokens.size() >= 3) {
	biosFile.status = tokens.at(0);
	biosFile.md5    = tokens.at(1);

	// concatenat the ending words
	std::string vname = "";
	for(unsigned int i=2; i<tokens.size(); i++) {
	  if(i > 2) vname += " ";
	  vname += tokens.at(i);
	}
	biosFile.path = vname;

	current.bios.push_back(biosFile);
      }
    }
  }
  if(isCurrent) {
    res.push_back(current);
  }
  pclose(pipe);
  return res;
}

bool RecalboxSystem::generateSupportFile() {
#if !defined(WIN32)
  std::string cmd = "/recalbox/scripts/recalbox-support.sh";
  int exitcode = system(cmd.c_str());
  if (WIFEXITED(exitcode)) {
    exitcode = WEXITSTATUS(exitcode);
  }
  return exitcode == 0;
#else
    return false;
#endif
}

std::string RecalboxSystem::getCurrentStorage() {

    std::ostringstream oss;
    oss << Settings::getInstance()->getString("RecalboxSettingScript") << " " << "storage current";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return "";
    }

    if (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        pclose(pipe);
        return std::string(line);
    }
    return "INTERNAL";
}

bool RecalboxSystem::setStorage(std::string selected) {
    std::ostringstream oss;
    oss << Settings::getInstance()->getString("RecalboxSettingScript") << " " << "storage" << " " << selected;
    int exitcode = system(oss.str().c_str());
    return exitcode == 0;
}

bool RecalboxSystem::forgetBluetoothControllers() {
    std::ostringstream oss;
    oss << Settings::getInstance()->getString("RecalboxSettingScript") << " " << "forgetBT";
    int exitcode = system(oss.str().c_str());
    return exitcode == 0;
}

std::string RecalboxSystem::getRootPassword() {

    std::ostringstream oss;
    oss << Settings::getInstance()->getString("RecalboxSettingScript") << " " << "getRootPassword";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return "";
    }

    if(fgets(line, 1024, pipe)){
        strtok(line, "\n");
        pclose(pipe);
        return std::string(line);
    }
    return oss.str().c_str();
}

std::vector<std::string> RecalboxSystem::getAvailableAudioOutputDevices() {

    std::vector<std::string> res;
    std::ostringstream oss;
    oss << Settings::getInstance()->getString("RecalboxSettingScript") << " " << "lsaudio";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return res;
    }

    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        res.push_back(std::string(line));
    }
    pclose(pipe);

    return res;
}

std::vector<std::string> RecalboxSystem::getAvailableVideoOutputDevices() {

    std::vector<std::string> res;
    std::ostringstream oss;
    oss << Settings::getInstance()->getString("RecalboxSettingScript") << " " << "lsoutputs";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return res;
    }

    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        res.push_back(std::string(line));
    }
    pclose(pipe);

    return res;
}

std::string RecalboxSystem::getCurrentAudioOutputDevice() {

    std::ostringstream oss;
    oss << Settings::getInstance()->getString("RecalboxSettingScript") << " " << "getaudio";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return "";
    }

    if (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        pclose(pipe);
        return std::string(line);
    }
    return "INTERNAL";
}

bool RecalboxSystem::setAudioOutputDevice(std::string selected) {
    std::ostringstream oss;

    AudioManager::getInstance()->deinit();
    VolumeControl::getInstance()->deinit();

    oss << Settings::getInstance()->getString("RecalboxSettingScript") << " " << "audio" << " '" << selected << "'";
    int exitcode = system(oss.str().c_str());

    VolumeControl::getInstance()->init();
    AudioManager::getInstance()->resumeMusic();
    AudioManager::getInstance()->playCheckSound();

    return exitcode == 0;
}
