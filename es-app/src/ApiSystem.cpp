#include "ApiSystem.h"
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
#include <chrono>
#include <thread>

#include "AudioManager.h"
#include "VolumeControl.h"
#include "InputManager.h"
#include <SystemConf.h>

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#if !defined(WIN32)
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#endif

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <fstream>
#include <SDL.h>
#include <Sound.h>

#if defined(WIN32)
#include <Windows.h>
#include "EmulationStation.h"
#define popen _popen
#define pclose _pclose
#include <thread>
#endif

#include "platform.h"
#include <pugixml/src/pugixml.hpp>

ApiSystem::ApiSystem() 
{
}

ApiSystem* ApiSystem::instance = NULL;

ApiSystem *ApiSystem::getInstance() 
{
	if (ApiSystem::instance == NULL)
		ApiSystem::instance = new ApiSystem();

	return ApiSystem::instance;
}

unsigned long ApiSystem::getFreeSpaceGB(std::string mountpoint) 
{
	LOG(LogDebug) << "ApiSystem::getFreeSpaceGB";

#if !defined(WIN32)
	struct statvfs fiData;
	const char *fnPath = mountpoint.c_str();
	int free = 0;
	if ((statvfs(fnPath, &fiData)) >= 0) {
		free = (fiData.f_bfree * fiData.f_bsize) / (1024 * 1024 * 1024);
	}
	return free;
#else

	unsigned __int64 i64FreeBytesToCaller, i64TotalBytes, i64FreeBytes;

	std::string drive = Utils::FileSystem::getHomePath()[0] + std::string(":");

	BOOL  fResult = GetDiskFreeSpaceExA(drive.c_str(),
		(PULARGE_INTEGER)&i64FreeBytesToCaller,
		(PULARGE_INTEGER)&i64TotalBytes,
		(PULARGE_INTEGER)&i64FreeBytes);

	if (fResult)
		return i64FreeBytes / (1024 * 1024 * 1024);

	return 0;
#endif
}

std::string ApiSystem::getFreeSpaceInfo() 
{
	LOG(LogDebug) << "ApiSystem::getFreeSpaceInfo";

	std::string sharePart = "/userdata/";
	if (sharePart.size() > 0) {
		const char *fnPath = sharePart.c_str();
#if !defined(WIN32)
		struct statvfs fiData;
		if ((statvfs(fnPath, &fiData)) < 0) {
			return "";
		}
		else {
			unsigned long total = (fiData.f_blocks * (fiData.f_bsize / 1024)) / (1024L * 1024L);
			unsigned long free = (fiData.f_bfree * (fiData.f_bsize / 1024)) / (1024L * 1024L);
			unsigned long used = total - free;
			unsigned long percent = 0;
			std::ostringstream oss;
			if (total != 0) {  //for small SD card ;) with share < 1GB
				percent = used * 100 / total;
				oss << used << "GB/" << total << "GB (" << percent << "%)";
			}
			else
				oss << "N/A";
			return oss.str();
		}
#else
		unsigned __int64 i64FreeBytesToCaller, i64TotalBytes, i64FreeBytes;

		std::string drive = Utils::FileSystem::getHomePath()[0] + std::string(":");

		BOOL  fResult = GetDiskFreeSpaceExA(drive.c_str(),
			(PULARGE_INTEGER)&i64FreeBytesToCaller,
			(PULARGE_INTEGER)&i64TotalBytes,
			(PULARGE_INTEGER)&i64FreeBytes);

		if (fResult)
		{
			unsigned long total = i64TotalBytes / (1024L * 1024L);
			unsigned long free = i64FreeBytes / (1024L * 1024L);
			unsigned long used = total - free;
			unsigned long percent = 0;
			std::ostringstream oss;
			if (total != 0) {  //for small SD card ;) with share < 1GB
				percent = used * 100 / total;
				oss << used << "GB/" << total << "GB (" << percent << "%)";
			}
			else
				oss << "N/A";

			return oss.str();
		}

		return "N/A";
#endif
	}
	else
		return "ERROR";	
}

bool ApiSystem::isFreeSpaceLimit() 
{
	LOG(LogDebug) << "ApiSystem::isFreeSpaceLimit";

	std::string sharePart = "/userdata/";
	if (sharePart.size() > 0) {
		return getFreeSpaceGB(sharePart) < 2;
	}
	else {
		return "ERROR";
	}

}

std::string ApiSystem::getVersion() 
{
	LOG(LogDebug) << "ApiSystem::getVersion";

#if WIN32
	return PROGRAM_VERSION_STRING;
#endif

	std::string version = "/usr/share/batocera/batocera.version";
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

bool ApiSystem::needToShowVersionMessage() 
{
	LOG(LogDebug) << "ApiSystem::needToShowVersionMessage";

	createLastVersionFileIfNotExisting();
	std::string versionFile = "/userdata/system/update.done";
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

bool ApiSystem::createLastVersionFileIfNotExisting() 
{
	LOG(LogDebug) << "ApiSystem::createLastVersionFileIfNotExisting";

	std::string versionFile = "/userdata/system/update.done";

	FILE *file;
	if (file = fopen(versionFile.c_str(), "r")) {
		fclose(file);
		return true;
	}
	return updateLastVersionFile();
}

bool ApiSystem::updateLastVersionFile() 
{
	LOG(LogDebug) << "ApiSystem::updateLastVersionFile";

	std::string versionFile = "/userdata/system/update.done";
	std::string currentVersion = getVersion();
	std::ostringstream oss;
	oss << "echo " << currentVersion << " > " << versionFile;
	if (system(oss.str().c_str())) 
	{
		LOG(LogError) << "Error executing " << oss.str().c_str();
		return false;
	}
	else 
	{
		LOG(LogInfo) << "Last version file updated";
		return true;
	}
}

bool ApiSystem::setOverscan(bool enable) 
{
	LOG(LogDebug) << "ApiSystem::setOverscan";

	std::ostringstream oss;
	oss << "batocera-config" << " " << "overscan";
	if (enable) {
		oss << " " << "enable";
	}
	else {
		oss << " " << "disable";
	}
	std::string command = oss.str();
	LOG(LogInfo) << "Launching " << command;
	if (system(command.c_str())) {
		LOG(LogError) << "Error executing " << command;
		return false;
	}
	else {
		LOG(LogInfo) << "Overscan set to : " << enable;
		return true;
	}
}

bool ApiSystem::setOverclock(std::string mode) 
{
	LOG(LogDebug) << "ApiSystem::setOverclock";

	if (mode != "") {
		std::ostringstream oss;
		oss << "batocera-overclock set " << mode;
		std::string command = oss.str();
		LOG(LogInfo) << "Launching " << command;
		if (system(command.c_str())) {
			LOG(LogError) << "Error executing " << command;
			return false;
		}
		else {
			LOG(LogInfo) << "Overclocking set to " << mode;
			return true;
		}
	}

	return false;
}

// BusyComponent* ui
std::pair<std::string, int> ApiSystem::updateSystem(const std::function<void(const std::string)>& func)
{
#if defined(WIN32) && defined(_DEBUG)
	for (int i = 0; i < 100; i += 2)
	{
		if (func != nullptr)
			func(std::string("Downloading >>> "+ std::to_string(i)+" %"));

		::Sleep(200);
	}

	if (func != nullptr)
		func(std::string("Extracting files"));

	::Sleep(750);

	return std::pair<std::string, int>("done.", 0);
#endif

	LOG(LogDebug) << "ApiSystem::updateSystem";

	std::string updatecommand = "batocera-upgrade";

	FILE *pipe = popen(updatecommand.c_str(), "r");
	if (pipe == nullptr)
		return std::pair<std::string, int>(std::string("Cannot call update command"), -1);

	char line[1024] = "";
	FILE *flog = fopen("/userdata/system/logs/batocera-upgrade.log", "w");
	while (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");
		if (flog != nullptr) 
			fprintf(flog, "%s\n", line);

		if (func != nullptr)
			func(std::string(line));		
	}

	int exitCode = pclose(pipe);

	if (flog != NULL)
	{
		fprintf(flog, "Exit code : %d\n", exitCode);
		fclose(flog);
	}

	return std::pair<std::string, int>(std::string(line), exitCode);
}

std::pair<std::string, int> ApiSystem::backupSystem(BusyComponent* ui, std::string device) 
{
	LOG(LogDebug) << "ApiSystem::backupSystem";

	std::string updatecommand = std::string("batocera-sync sync ") + device;
	FILE *pipe = popen(updatecommand.c_str(), "r");
	char line[1024] = "";
	if (pipe == NULL) {
		return std::pair<std::string, int>(std::string("Cannot call sync command"), -1);
	}

	FILE *flog = fopen("/userdata/system/logs/batocera-sync.log", "w");
	while (fgets(line, 1024, pipe)) {
		strtok(line, "\n");
		if (flog != NULL) fprintf(flog, "%s\n", line);
		ui->setText(std::string(line));
	}
	if (flog != NULL) fclose(flog);

	int exitCode = pclose(pipe);
	return std::pair<std::string, int>(std::string(line), exitCode);
}

std::pair<std::string, int> ApiSystem::installSystem(BusyComponent* ui, std::string device, std::string architecture) 
{
	LOG(LogDebug) << "ApiSystem::installSystem";

	std::string updatecommand = std::string("batocera-install install ") + device + " " + architecture;
	FILE *pipe = popen(updatecommand.c_str(), "r");
	char line[1024] = "";
	if (pipe == NULL) {
		return std::pair<std::string, int>(std::string("Cannot call install command"), -1);
	}

	FILE *flog = fopen("/userdata/system/logs/batocera-install.log", "w");
	while (fgets(line, 1024, pipe)) {
		strtok(line, "\n");
		if (flog != NULL) fprintf(flog, "%s\n", line);
		ui->setText(std::string(line));
	}

	int exitCode = pclose(pipe);

	if (flog != NULL)
	{
		fprintf(flog, "Exit code : %d\n", exitCode);
		fclose(flog);
	}

	return std::pair<std::string, int>(std::string(line), exitCode);
}

std::pair<std::string, int> ApiSystem::scrape(BusyComponent* ui) 
{
	LOG(LogDebug) << "ApiSystem::scrape";

	std::string scrapecommand = std::string("batocera-scraper");
	FILE *pipe = popen(scrapecommand.c_str(), "r");
	char line[1024] = "";
	if (pipe == NULL) {
		return std::pair<std::string, int>(std::string("Cannot call scrape command"), -1);
	}

	FILE *flog = fopen("/userdata/system/logs/batocera-scraper.log", "w");
	while (fgets(line, 1024, pipe)) {
		strtok(line, "\n");
		if (flog != NULL) fprintf(flog, "%s\n", line);


		if (Utils::String::startsWith(line, "GAME: ")) {
			ui->setText(std::string(line));
		}
	}
	if (flog != NULL) fclose(flog);

	int exitCode = pclose(pipe);
	return std::pair<std::string, int>(std::string(line), exitCode);
}

bool ApiSystem::ping() 
{
	LOG(LogDebug) << "ApiSystem::ping";

#if WIN32
	bool connected = false;

	HMODULE hWinInet = LoadLibrary("WININET.DLL");
	if (hWinInet != NULL)
	{
		typedef BOOL(WINAPI *PF_INETGETCONNECTEDSTATE)(LPDWORD, DWORD);
		PF_INETGETCONNECTEDSTATE pfInternetGetConnectedState;

		pfInternetGetConnectedState = (PF_INETGETCONNECTEDSTATE) ::GetProcAddress(hWinInet, "InternetGetConnectedState");
		// affectation du pointeur sur la fonction
		if (pfInternetGetConnectedState != NULL)
		{
			DWORD TypeCon;
			if (pfInternetGetConnectedState(&TypeCon, 0))
				connected = true;			
		}

		FreeLibrary(hWinInet);
	}

	return connected;
#endif

	std::string updateserver = "github.com"; // a pingable web url
	std::string s("timeout 1 fping -c 1 -t 1000 " + updateserver);
	int exitcode = system(s.c_str());
	return exitcode == 0;
}

bool ApiSystem::canUpdate(std::vector<std::string>& output) 
{
#if defined(WIN32) && defined(_DEBUG)
	output.push_back("super");
	return true;
#endif

	LOG(LogDebug) << "ApiSystem::canUpdate";

	int res;
	int exitCode;
	std::ostringstream oss;
	oss << "batocera-config" << " " << "canupdate";
	std::string command = oss.str();
	LOG(LogInfo) << "Launching " << command;

	FILE *pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL) {
		return false;
	}

	while (fgets(line, 1024, pipe)) {
		strtok(line, "\n");
		output.push_back(std::string(line));
	}
	exitCode = pclose(pipe);

#ifdef WIN32
	res = exitCode;
#else
	res = WEXITSTATUS(exitCode);
#endif

	if (res == 0) {
		LOG(LogInfo) << "Can update ";
		return true;
	}
	else {
		LOG(LogInfo) << "Cannot update ";
		return false;
	}
}

void ApiSystem::launchExternalWindow_before(Window *window) 
{
	LOG(LogDebug) << "ApiSystem::launchExternalWindow_before";

	AudioManager::getInstance()->deinit();
	VolumeControl::getInstance()->deinit();
	window->deinit();

	LOG(LogDebug) << "ApiSystem::launchExternalWindow_before OK";
}

void ApiSystem::launchExternalWindow_after(Window *window) 
{
	LOG(LogDebug) << "ApiSystem::launchExternalWindow_after";

	window->init();
	VolumeControl::getInstance()->init();
	AudioManager::getInstance()->init();
	window->normalizeNextUpdate();
	window->reactivateGui();

	AudioManager::getInstance()->playRandomMusic();

	LOG(LogDebug) << "ApiSystem::launchExternalWindow_after OK";
}

bool ApiSystem::launchKodi(Window *window) 
{
	LOG(LogDebug) << "ApiSystem::launchKodi";

	std::string commandline = InputManager::getInstance()->configureEmulators();
	std::string command = "python /usr/lib/python2.7/site-packages/configgen/emulatorlauncher.py -system kodi -rom '' " + commandline;

	ApiSystem::launchExternalWindow_before(window);

	int exitCode = system(command.c_str());
#if !defined(WIN32)
	// WIFEXITED returns a nonzero value if the child process terminated normally with exit or _exit.
	// https://www.gnu.org/software/libc/manual/html_node/Process-Completion-Status.html
	if (WIFEXITED(exitCode)) {
		exitCode = WEXITSTATUS(exitCode);
	}
#endif

	ApiSystem::launchExternalWindow_after(window);

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

bool ApiSystem::launchFileManager(Window *window) 
{
	LOG(LogDebug) << "ApiSystem::launchFileManager";

	std::string command = "filemanagerlauncher";

	ApiSystem::launchExternalWindow_before(window);

	int exitCode = system(command.c_str());
#if !defined(WIN32)
	if (WIFEXITED(exitCode)) {
		exitCode = WEXITSTATUS(exitCode);
	}
#endif

	ApiSystem::launchExternalWindow_after(window);

	return exitCode == 0;
}

bool ApiSystem::enableWifi(std::string ssid, std::string key) 
{
	LOG(LogDebug) << "ApiSystem::enableWifi";

	std::ostringstream oss;

	Utils::String::replace(ssid, "\"", "\\\"");
	Utils::String::replace(key, "\"", "\\\"");

	oss << "batocera-config" << " "
		<< "wifi" << " "
		<< "enable" << " \""
		<< ssid << "\" \"" << key << "\"";
	std::string command = oss.str();
	LOG(LogInfo) << "Launching " << command;
	if (system(command.c_str()) == 0) {
		LOG(LogInfo) << "Wifi enabled ";
		return true;
	}
	else {
		LOG(LogError) << "Cannot enable wifi ";
		return false;
	}
}

bool ApiSystem::disableWifi() 
{
	LOG(LogDebug) << "ApiSystem::disableWifi";

	std::ostringstream oss;
	oss << "batocera-config" << " "
		<< "wifi" << " "
		<< "disable";
	std::string command = oss.str();
	LOG(LogInfo) << "Launching " << command;
	if (system(command.c_str()) == 0) {
		LOG(LogInfo) << "Wifi disabled ";
		return true;
	}
	else {
		LOG(LogError) << "Cannot disable wifi ";
		return false;
	}
}

bool ApiSystem::halt(bool reboot, bool fast) 
{
	LOG(LogDebug) << "ApiSystem::halt";

	SDL_Event *quit = new SDL_Event();
	if (fast)
		if (reboot)
			quit->type = SDL_FAST_QUIT | SDL_SYS_REBOOT;
		else
			quit->type = SDL_FAST_QUIT | SDL_SYS_SHUTDOWN;
	else
		if (reboot)
			quit->type = SDL_QUIT | SDL_SYS_REBOOT;
		else
			quit->type = SDL_QUIT | SDL_SYS_SHUTDOWN;

	SDL_PushEvent(quit);
	return 0;
}

bool ApiSystem::reboot() {
	return halt(true, false);
}

bool ApiSystem::fastReboot() {
	return halt(true, true);
}

bool ApiSystem::shutdown() {
	return halt(false, false);
}

bool ApiSystem::fastShutdown() {
	return halt(false, true);
}

std::string ApiSystem::getIpAdress() 
{
	LOG(LogDebug) << "ApiSystem::getIpAdress";

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
	return std::string("127.0.0.1");
#endif
}

bool ApiSystem::scanNewBluetooth(const std::function<void(const std::string)>& func)
{
	if (func == nullptr)
		return false;

	LOG(LogDebug) << "ApiSystem::scanNewBluetooth";

	std::ostringstream oss;
	oss << "batocera-bluetooth" << " " << "trust";
	int exitcode = system(oss.str().c_str());
	return exitcode == 0;
}

std::vector<std::string> ApiSystem::getAvailableStorageDevices() 
{
#if WIN32
	std::vector<std::string> res;
	res.push_back("DEFAULT");
	return res;
#endif

	return executeEnumerationScript("batocera-config storage list");
}

std::vector<std::string> ApiSystem::getVideoModes() 
{
	return executeEnumerationScript("batocera-resolution listModes");
}

std::vector<std::string> ApiSystem::getAvailableBackupDevices() 
{
	return executeEnumerationScript("batocera-sync list");
}

std::vector<std::string> ApiSystem::getAvailableInstallDevices() 
{
	return executeEnumerationScript("batocera-install listDisks");
}

std::vector<std::string> ApiSystem::getAvailableInstallArchitectures() 
{
	return executeEnumerationScript("batocera-install listArchs");
}

std::vector<std::string> ApiSystem::getAvailableOverclocking() 
{
	return executeEnumerationScript("batocera-overclock list");
}

std::vector<std::string> ApiSystem::getSystemInformations() 
{
#if WIN32
	LOG(LogDebug) << "ApiSystem::getSystemInformations";

	std::vector<std::string> res;

	int CPUInfo[4] = { -1 };
	unsigned   nExIds, i = 0;
	char CPUBrandString[0x40];
	// Get the information associated with each extended ID.
	__cpuid(CPUInfo, 0x80000000);
	nExIds = CPUInfo[0];
	for (i = 0x80000000; i <= nExIds; ++i)
	{
		__cpuid(CPUInfo, i);
		// Interpret CPU brand string
		if (i == 0x80000002)
			memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
		else if (i == 0x80000003)
			memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
		else if (i == 0x80000004)
			memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
	}

	//string includes manufacturer, model and clockspeed
	res.push_back("CPU:" + std::string(CPUBrandString));

	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);	
	res.push_back("CORES:" + std::to_string(sysInfo.dwNumberOfProcessors));

	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof(statex);
	GlobalMemoryStatusEx(&statex);
	res.push_back("MEMORY:" + std::to_string((statex.ullTotalPhys / 1024) / 1024) + "MB");

	return res;
#endif

	return executeEnumerationScript("batocera-info");
}

std::vector<BiosSystem> ApiSystem::getBiosInformations() 
{
	std::vector<BiosSystem> res;
	BiosSystem current;
	bool isCurrent = false;

#if WIN32 && _DEBUG
	LOG(LogDebug) << "ApiSystem::getBiosInformations";

	current.name = "atari5200";

	BiosFile biosFile;
	biosFile.md5 = "281f20ea4320404ec820fb7ec0693b38";
	biosFile.path = "bios/5200.rom";
	biosFile.status = "missing";
	current.bios.push_back(biosFile);

	res.push_back(current);
	return res;
#endif

	auto systems = executeEnumerationScript("batocera-systems");
	for (auto line : systems)
	{
		if (Utils::String::startsWith(line, "> ")) 
		{
			if (isCurrent)
				res.push_back(current);

			isCurrent = true;
			current.name = std::string(std::string(line).substr(2));
			current.bios.clear();
		}
		else 
		{
			BiosFile biosFile;
			std::vector<std::string> tokens = Utils::String::split(line, ' ');
			if (tokens.size() >= 3) 
			{
				biosFile.status = tokens.at(0);
				biosFile.md5 = tokens.at(1);

				// concatenat the ending words
				std::string vname = "";
				for (unsigned int i = 2; i < tokens.size(); i++) 
				{
					if (i > 2) vname += " ";
					vname += tokens.at(i);
				}
				biosFile.path = vname;

				current.bios.push_back(biosFile);
			}
		}
	}

	if (isCurrent)
		res.push_back(current);

	return res;
}

bool ApiSystem::generateSupportFile() 
{
	LOG(LogDebug) << "ApiSystem::generateSupportFile";

#if !defined(WIN32)
	std::string cmd = "batocera-support";
	int exitcode = system(cmd.c_str());
	if (WIFEXITED(exitcode)) {
		exitcode = WEXITSTATUS(exitcode);
	}
	return exitcode == 0;
#else
	return false;
#endif
}

std::string ApiSystem::getCurrentStorage() 
{
	LOG(LogDebug) << "ApiSystem::getCurrentStorage";

#if WIN32
	return "DEFAULT";
#endif

	std::ostringstream oss;
	oss << "batocera-config" << " " << "storage current";
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

bool ApiSystem::setStorage(std::string selected) 
{
	LOG(LogDebug) << "ApiSystem::setStorage";

	std::ostringstream oss;
	oss << "batocera-config" << " " << "storage" << " " << selected;
	int exitcode = system(oss.str().c_str());
	return exitcode == 0;
}

bool ApiSystem::forgetBluetoothControllers() 
{
	LOG(LogDebug) << "ApiSystem::forgetBluetoothControllers";

	std::ostringstream oss;
	oss << "batocera-config" << " " << "forgetBT";
	int exitcode = system(oss.str().c_str());
	return exitcode == 0;
}

std::string ApiSystem::getRootPassword() 
{
	LOG(LogDebug) << "ApiSystem::getRootPassword";

	std::ostringstream oss;
	oss << "batocera-config" << " " << "getRootPassword";
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
	return oss.str().c_str();
}

std::vector<std::string> ApiSystem::getAvailableAudioOutputDevices() 
{
#if WIN32
	std::vector<std::string> res;
	res.push_back("auto");
	return res;
#endif

	return executeEnumerationScript("batocera-config lsaudio");
}

std::vector<std::string> ApiSystem::getAvailableVideoOutputDevices() 
{
	return executeEnumerationScript("batocera-config lsoutputs");
}

std::string ApiSystem::getCurrentAudioOutputDevice() 
{
#if WIN32
	return "auto";
#endif

	LOG(LogDebug) << "ApiSystem::getCurrentAudioOutputDevice";

	std::ostringstream oss;
	oss << "batocera-config" << " " << "getaudio";
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
	return "";
}

bool ApiSystem::setAudioOutputDevice(std::string selected) 
{
	LOG(LogDebug) << "ApiSystem::setAudioOutputDevice";

	std::ostringstream oss;

	AudioManager::getInstance()->deinit();
	VolumeControl::getInstance()->deinit();

	oss << "batocera-config" << " " << "audio" << " '" << selected << "'";
	int exitcode = system(oss.str().c_str());

	VolumeControl::getInstance()->init();
	AudioManager::getInstance()->init();
	Sound::get("/usr/share/emulationstation/resources/checksound.ogg")->play();

	return exitcode == 0;
}



// Batocera
RetroAchievementInfo ApiSystem::getRetroAchievements()
{
	RetroAchievementInfo info;

	LOG(LogDebug) << "ApiSystem::getRetroAchievements";

	std::ostringstream oss;
	oss << "batocera-retroachievements-info";
	FILE *pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
	{
		info.error = "Error accessing 'batocera-retroachievements-info' script";
		return info;
	}

	std::string data;

	while (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");		
		data = data + std::string(line) + "\n";
	}

	pclose(pipe);

#if defined(WIN32) && defined(_DEBUG)
	// Xml Test 
	data = 
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<retroachievements>"
		"<username>lbrpdx</username>"
		"<totalpoints>4743</totalpoints>"
		"<rank>4184 / 61121 ranked users (Top 8%)</rank>"
		"<userpic>https://retroachievements.org/UserPic/lbrpdx.png</userpic>"
		"<registered>29 Aug 2018, 03:00</registered>"
		"<lastactivity>16 Nov 2019, 02:35</lastactivity>"
		"<game>"
		"  <name>Captain Commando (Arcade)</name>"
		"  <achievements>4 of 54</achievements>"
		"  <points>25/830</points>"
		"  <lastplayed>2020-01-23 04:51:33</lastplayed>"
		"  <badge>https://s3-eu-west-1.amazonaws.com/i.retroachievements.org/Badge/97969.png</badge>"
		"</game>"
		"<game>"
		"  <name>Pac-in-Time (Game Boy)</name>"
		"  <achievements>0 of 14</achievements>"
		"  <points>0/140</points>"
		"  <lastplayed>2020-01-23 04:41:26</lastplayed>"
		"  <badge>https://s3-eu-west-1.amazonaws.com/i.retroachievements.org/Badge/00136_lock.png</badge>"
		"</game>"
		"<game>"
		"  <name>1942 (NES)</name>"
		"  <achievements>6 of 19</achievements>"
		"  <points>90/400</points>"
		"  <lastplayed>2020-01-19 23:15:40</lastplayed>"
		"  <badge>https://s3-eu-west-1.amazonaws.com/i.retroachievements.org/Badge/04200.png</badge>"
		"</game>"
		"<game>"
		"  <name>Tecmo Bowl (NES)</name>"
		"  <achievements>3 of 6</achievements>"
		"  <points>15/35</points>"
		"  <lastplayed>2020-01-19 15:46:33</lastplayed>"
		"  <badge>https://s3-eu-west-1.amazonaws.com/i.retroachievements.org/Badge/09025.png</badge>"
		"</game>"
		"<game>"
		"  <name>Bank Heist (Atari 2600)</name>"
		"  <achievements>5 of 17</achievements>"
		"  <points>23/100</points>"
		"  <lastplayed>2020-01-19 15:38:57</lastplayed>"
		"  <badge>https://s3-eu-west-1.amazonaws.com/i.retroachievements.org/Badge/00083.png</badge>"
		"</game>"
		"</retroachievements>";

	/*
	// Retrocompatibility Test 
	data = "Player TOTO (51 points) is 42287 / 61014 ranked users (Top 70%)\n"
		   "Last RetroAchievements games played:\n"
			"Tetris (Game Boy)@0 of 13 achievements@0/177 points@Last played 2019-11-05 23:47:44\n"
			"F-Zero - GP Legend (Game Boy Advance)@0 of 41 achievements@0/400 points@Last played 2019-11-04 23:43:02\n"
			"Ice Climber (NES)@0 of 34 achievements@0/400 points@Last played 2019-10-30 22:06:15\n"
			"Solstice: The Quest for the Staff of Demnos (NES)@0 of 41 achievements@0/400 points@Last played 2019-10-30 13:38:01\n";*/
#endif

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load(data.c_str()); // doc.load_buffer(data.c_str(), data.size);

	if (!result)
	{
		// Temporary retrocompatibility mode
		auto lines = Utils::String::split(data, '\n');
		if (lines.size() == 1)
		{
			info.error = lines[0];
			return info;
		}

		for (auto line : lines)
		{
			std::vector<std::string> tokens = Utils::String::split(line, '@');
			if (tokens.size() == 0)
				continue;

			if (tokens.size() == 1)
			{					
				if (info.username.empty())
				{
					auto userParse = Utils::String::split(line, ' ');
					if (userParse.size() > 2)
						info.username = userParse[1];

					auto infoParsePoints = Utils::String::splitAny(line, "()");
					if (infoParsePoints.size() > 3)
					{
						info.totalpoints = infoParsePoints[1];
						info.rank = Utils::String::replace(infoParsePoints[2], " is ", "") + " (" + infoParsePoints[3] + ")";
					}
				}

				continue;
			}

			RetroAchievementGame rg;
			rg.name = tokens[0];
			rg.achievements = Utils::String::replace(tokens[1], " achievements", "");

			if (tokens.size() >= 4)
			{
				rg.points = Utils::String::replace(tokens[2], " points", ""); 
				rg.lastplayed = Utils::String::replace(tokens[3], "Last played ", "");
			}

			info.games.push_back(rg);
		}

		return info;
	}

	pugi::xml_node root = doc.child("retroachievements");
	if (!root)
	{
		LOG(LogError) << "Could not find <retroachievements> node";
		return info;
	}

	for (pugi::xml_node node : root.children())
	{
		std::string tag = node.name();

		if (tag == "error")
		{
			info.error = node.text().get();
			break;
		}

		if (tag == "username")
			info.username = node.text().get();
		else if (tag == "totalpoints")
			info.totalpoints = node.text().get();
		else if (tag == "rank")
			info.rank = node.text().get();
		else if (tag == "userpic")
		{
			std::string userpic = node.text().get();
			if (!userpic.empty())
			{
				std::string localPath = Utils::FileSystem::getGenericPath(Utils::FileSystem::getEsConfigPath() + "/tmp");
				if (!Utils::FileSystem::exists(localPath))
					Utils::FileSystem::createDirectory(localPath);

				std::string localFile = localPath + "/" + Utils::FileSystem::getFileName(userpic);
				if (!Utils::FileSystem::exists(localFile))
				{
					std::shared_ptr<HttpReq> httpreq = std::make_shared<HttpReq>(userpic);

					while (httpreq->status() == HttpReq::REQ_IN_PROGRESS)
						std::this_thread::sleep_for(std::chrono::milliseconds(20));

					if (httpreq->status() == HttpReq::REQ_SUCCESS)
						httpreq->saveContent(localFile);
				}

				if (Utils::FileSystem::exists(localFile))
					info.userpic = localFile;
			}
		}
		else if (tag == "registered")
			info.registered = node.text().get();
		else if (tag == "game")
		{
			RetroAchievementGame rg;

			for (pugi::xml_node game : node.children())
			{
				tag = game.name();

				if (tag == "name")
					rg.name = game.text().get();
				else if (tag == "achievements")
					rg.achievements = game.text().get();
				else if (tag == "points")
					rg.points = game.text().get();
				else if (tag == "lastplayed")
					rg.lastplayed = game.text().get();
				else if (tag == "badge")
				{
					std::string badge = game.text().get();

					if (!badge.empty())
					{
						std::string localPath = Utils::FileSystem::getGenericPath(Utils::FileSystem::getEsConfigPath() + "/tmp");
						if (!Utils::FileSystem::exists(localPath))
							Utils::FileSystem::createDirectory(localPath);

						std::string localFile = localPath + "/" + Utils::FileSystem::getFileName(badge);
						if (!Utils::FileSystem::exists(localFile))
						{
							std::shared_ptr<HttpReq> httpreq = std::make_shared<HttpReq>(badge);

							while (httpreq->status() == HttpReq::REQ_IN_PROGRESS)
								std::this_thread::sleep_for(std::chrono::milliseconds(20));

							if (httpreq->status() == HttpReq::REQ_SUCCESS)
								httpreq->saveContent(localFile);
						}
						if (Utils::FileSystem::exists(localFile))
							rg.badge = localFile;
					}
				}
			}

			if (!rg.name.empty())
				info.games.push_back(rg);
		}
	}

	return info;
}

std::vector<std::string> ApiSystem::getBatoceraThemesList() 
{
	LOG(LogDebug) << "ApiSystem::getBatoceraThemesList";

	std::vector<std::string> res;

#if WIN32
	std::shared_ptr<HttpReq> httpreq = std::make_shared<HttpReq>("https://batocera.org/upgrades/themes.txt");

	while (httpreq->status() == HttpReq::REQ_IN_PROGRESS)
		std::this_thread::sleep_for(std::chrono::milliseconds(20));

	if (httpreq->status() == HttpReq::REQ_SUCCESS)
	{		
		auto lines = Utils::String::split(httpreq->getContent(), '\n');
		for (auto line : lines)
		{
			auto parts = Utils::String::splitAny(line, " \t");
			if (parts.size() > 1)
			{
				auto themeName = parts[0];
				std::string themeUrl = Utils::FileSystem::getFileName(parts[1]);

				bool themeExists = false;

				std::vector<std::string> paths { 
					"/etc/emulationstation/themes",
					Utils::FileSystem::getEsConfigPath() + "/themes",
					"/userdata/themes" // batocera
				};

				for (auto path : paths)
				{
					if (Utils::FileSystem::isDirectory(path + "/" + themeUrl+"-master"))
					{
						themeExists = true;
						break;
					}
					else if (Utils::FileSystem::isDirectory(path + "/" + themeUrl))
					{
						themeExists = true;
						break;
					}
					else if (Utils::FileSystem::isDirectory(path + "/"+ themeName))
					{
						themeExists = true;
						break;
					}
				}

				if (themeExists)
					res.push_back("[I]\t" + line);
				else
					res.push_back("[A]\t" + line);
			}
		}
	}

	return res;
#endif

	std::ostringstream oss;
	oss << "batocera-es-theme" << " " << "list";
	FILE *pipe = popen(oss.str().c_str(), "r");
	char line[1024];
	char *pch;

	if (pipe == NULL) {
		return res;
	}
	while (fgets(line, 1024, pipe)) {
		strtok(line, "\n");
		// provide only themes that are [A]vailable or [I]nstalled as a result
		// (Eliminate [?] and other non-installable lines of text)
		if ((strncmp(line, "[A]", 3) == 0) || (strncmp(line, "[I]", 3) == 0))
			res.push_back(std::string(line));
	}
	pclose(pipe);
	return res;
}

#if WIN32
#include <ShlDisp.h>
#include <comutil.h> // #include for _bstr_t
#pragma comment(lib, "shell32.lib")
#pragma comment (lib, "comsuppw.lib" ) // link with "comsuppw.lib" (or debug version: "comsuppwd.lib")

bool unzipFile(const std::string fileName, const std::string dest)
{
	bool	ret = false;

	HRESULT          hResult;
	IShellDispatch*	 pISD;
	Folder*			 pFromZip = nullptr;
	VARIANT          vDir, vFile, vOpt;

	OleInitialize(NULL);
	CoInitialize(NULL);

	hResult = CoCreateInstance(CLSID_Shell, NULL, CLSCTX_INPROC_SERVER, IID_IShellDispatch, (void **)&pISD);

	if (SUCCEEDED(hResult))
	{
		VariantInit(&vDir);
		vDir.vt = VT_BSTR;

		int zipDirLen = (lstrlen(fileName.c_str()) + 1) * sizeof(WCHAR);
		BSTR bstrZip = SysAllocStringByteLen(NULL, zipDirLen);
		MultiByteToWideChar(CP_ACP, 0, fileName.c_str(), -1, bstrZip, zipDirLen);
		vDir.bstrVal = bstrZip;
		
		hResult = pISD->NameSpace(vDir, &pFromZip);

		if (hResult == S_OK && pFromZip != nullptr)
		{
			if (!Utils::FileSystem::exists(dest))
				Utils::FileSystem::createDirectory(dest);

			Folder *pToFolder = NULL;

			VariantInit(&vFile);
			vFile.vt = VT_BSTR;

			int fnLen = (lstrlen(dest.c_str()) + 1) * sizeof(WCHAR);
			BSTR bstrFolder = SysAllocStringByteLen(NULL, fnLen);
			MultiByteToWideChar(CP_ACP, 0, dest.c_str(), -1, bstrFolder, fnLen);
			vFile.bstrVal = bstrFolder;

			hResult = pISD->NameSpace(vFile, &pToFolder);
			if (hResult == S_OK && pToFolder)
			{
				FolderItems *fi = NULL;
				pFromZip->Items(&fi);

				VariantInit(&vOpt);
				vOpt.vt = VT_I4;
				vOpt.lVal = FOF_NO_UI; //4; // Do not display a progress dialog box

				VARIANT newV;
				VariantInit(&newV);
				newV.vt = VT_DISPATCH;
				newV.pdispVal = fi;
				hResult = pToFolder->CopyHere(newV, vOpt);
				if (hResult == S_OK)
					ret = true;

				pFromZip->Release();
				pToFolder->Release();
			}
		}
		pISD->Release();
	}

	CoUninitialize();
	return ret;
}

std::shared_ptr<HttpReq> downloadGitRepository(const std::string url, const std::string label, const std::function<void(const std::string)>& func)
{
	if (func != nullptr)
		func("Downloading " + label);

	long downloadSize = 0;

	std::string statUrl = Utils::String::replace(url, "https://github.com/", "https://api.github.com/repos/");
	if (statUrl != url)
	{
		std::shared_ptr<HttpReq> statreq = std::make_shared<HttpReq>(statUrl);

		while (statreq->status() == HttpReq::REQ_IN_PROGRESS)
			std::this_thread::sleep_for(std::chrono::milliseconds(20));

		if (statreq->status() == HttpReq::REQ_SUCCESS)
		{
			std::string content = statreq->getContent();
			auto pos = content.find("\"size\": ");
			if (pos != std::string::npos)
			{
				auto end = content.find(",", pos);
				if (end != std::string::npos)
					downloadSize = atoi(content.substr(pos + 8, end - pos - 8).c_str()) * 1024;
			}
		}
	}

	std::shared_ptr<HttpReq> httpreq = std::make_shared<HttpReq>(url + "/archive/master.zip");

	int curPos = -1;
	while (httpreq->status() == HttpReq::REQ_IN_PROGRESS)
	{
		if (downloadSize > 0)
		{
			double pos = httpreq->getPosition();
			if (pos > 0 && curPos != pos)
			{
				if (func != nullptr)
				{
					std::string pc = std::to_string((int)(pos * 100.0 / downloadSize));
					func(std::string("Downloading " + label + " >>> " + pc + " %"));
				}

				curPos = pos;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	if (httpreq->status() != HttpReq::REQ_SUCCESS)
		return nullptr;

	return httpreq;
}
#endif

std::pair<std::string, int> ApiSystem::installBatoceraTheme(std::string thname, const std::function<void(const std::string)>& func)
{
#if WIN32
	for (auto theme : getBatoceraThemesList())
	{
		auto parts = Utils::String::splitAny(theme, " \t");
		if (parts.size() < 2)
			continue;

		if (parts[1] == thname)
		{
			std::string themeUrl = parts.size() < 3 ? "" : (parts[2] == "-" ? parts[3] : parts[2]);
			
			std::shared_ptr<HttpReq> httpreq = downloadGitRepository(themeUrl, thname, func);
			if (httpreq != nullptr && httpreq->status() == HttpReq::REQ_SUCCESS)
			{
				if (func != nullptr)
					func("Extracting " + thname);

				std::string themeFileName = Utils::FileSystem::getFileName(themeUrl);
				std::string zipFile = Utils::FileSystem::getEsConfigPath() + "/themes/"+ themeFileName +".zip";
				zipFile = Utils::String::replace(zipFile, "/", "\\");
				httpreq->saveContent(zipFile);

				unzipFile(zipFile, Utils::String::replace(Utils::FileSystem::getEsConfigPath() + "/themes", "/", "\\"));

				std::string folderName = Utils::FileSystem::getEsConfigPath() + "/themes/" + themeFileName + "-master";
				std::string finalfolderName = Utils::String::replace(folderName, "-master", "");

				rename(folderName.c_str(), finalfolderName.c_str());

				Utils::FileSystem::removeFile(zipFile);

				return std::pair<std::string, int>(std::string("OK"), 0);
			}

			break;
		}
	}

	return std::pair<std::string, int>(std::string(""), 1);
#endif

	LOG(LogDebug) << "ApiSystem::installBatoceraTheme";

	std::string updatecommand = std::string("batocera-es-theme install ") + thname;
	LOG(LogWarning) << "Installing theme " << thname;
	FILE *pipe = popen(updatecommand.c_str(), "r");
	char line[1024] = "";
	if (pipe == NULL) {
		return std::pair<std::string, int>(std::string("Error starting `batocera-es-theme` command."), -1);
	}

	while (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");
		LOG(LogWarning) << "Theme install: " << line;
		// Long theme names/URL can crash the GUI MsgBox
		// "48" found by trials and errors. Ideally should be fixed
		// in es-core MsgBox -- FIXME
		if (strlen(line) > 48)
			line[47] = '\0';

		if (func != nullptr)
			func(std::string(line));		
	}

	int exitCode = pclose(pipe);
	return std::pair<std::string, int>(std::string(line), exitCode);
}

std::vector<std::string> ApiSystem::getBatoceraBezelsList() 
{
	LOG(LogDebug) << "ApiSystem::getBatoceraBezelsList";

	std::vector<std::string> res;

#if WIN32
	std::shared_ptr<HttpReq> httpreq = std::make_shared<HttpReq>("https://batocera.org/upgrades/bezels.txt");
	
	while (httpreq->status() == HttpReq::REQ_IN_PROGRESS)
		std::this_thread::sleep_for(std::chrono::milliseconds(20));

	if (httpreq->status() == HttpReq::REQ_SUCCESS)
	{
		auto lines = Utils::String::split(httpreq->getContent(), '\n');
		for (auto line : lines)
			res.push_back("[A]\t" + line);
	}

	return res;
#endif

	std::ostringstream oss;
	oss << "batocera-es-thebezelproject list";
	FILE *pipe = popen(oss.str().c_str(), "r");
	char line[1024];
	char *pch;

	if (pipe == NULL) {
		return res;
	}
	while (fgets(line, 1024, pipe)) {
		strtok(line, "\n");
		// provide only themes that are [A]vailable or [I]nstalled as a result
		// (Eliminate [?] and other non-installable lines of text)
		if ((strncmp(line, "[A]", 3) == 0) || (strncmp(line, "[I]", 3) == 0))
			res.push_back(std::string(line));
	}
	pclose(pipe);
	return res;
}

std::pair<std::string, int> ApiSystem::installBatoceraBezel(std::string bezelsystem, const std::function<void(const std::string)>& func)
{
	LOG(LogDebug) << "ApiSystem::installBatoceraBezel";

#if WIN32
	for (auto bezel : getBatoceraBezelsList())
	{
		auto parts = Utils::String::splitAny(bezel, " \t");
		if (parts.size() < 2)
			continue;

		if (parts[1] == bezelsystem)
		{
			std::string themeUrl = parts.size() < 3 ? "" : (parts[2] == "-" ? parts[3] : parts[2]);

			std::shared_ptr<HttpReq> httpreq = downloadGitRepository(themeUrl, bezelsystem, func);
			if (httpreq != nullptr && httpreq->status() == HttpReq::REQ_SUCCESS)
			{
				/*
				if (func != nullptr)
					func("Extracting " + bezelsystem);
								
				std::string themeFileName = Utils::FileSystem::getFileName(themeUrl);
				std::string zipFile = Utils::FileSystem::getEsConfigPath() + "/themes/" + themeFileName + ".zip";
				zipFile = Utils::String::replace(zipFile, "/", "\\");
				httpreq->saveContent(zipFile);

				unzipFile(zipFile, Utils::String::replace(Utils::FileSystem::getEsConfigPath() + "/themes", "/", "\\"));

				Utils::FileSystem::removeFile(zipFile);
				*/
				return std::pair<std::string, int>(std::string("OK"), 0);
			}

			break;
		}
	}

	return std::pair<std::string, int>(std::string(""), 1);
#endif

	std::string updatecommand = std::string("batocera-es-thebezelproject install ") + bezelsystem;
	LOG(LogWarning) << "Installing bezels for " << bezelsystem;
	FILE *pipe = popen(updatecommand.c_str(), "r");
	char line[1024] = "";
	if (pipe == NULL) {
		return std::pair<std::string, int>(std::string("Error starting `batocera-es-thebezelproject install` command."), -1);
	}

	while (fgets(line, 1024, pipe)) {
		strtok(line, "\n");
		// Long theme names/URL can crash the GUI MsgBox
		// "48" found by trials and errors. Ideally should be fixed
		// in es-core MsgBox -- FIXME
		if (strlen(line) > 48)
			line[47] = '\0';

		if (func != nullptr)
			func(std::string(line));			
	}

	int exitCode = pclose(pipe);
	return std::pair<std::string, int>(std::string(line), exitCode);
}

std::pair<std::string, int> ApiSystem::uninstallBatoceraBezel(BusyComponent* ui, std::string bezelsystem) 
{
	LOG(LogDebug) << "ApiSystem::uninstallBatoceraBezel";

	std::string updatecommand = std::string("batocera-es-thebezelproject remove ") + bezelsystem;
	LOG(LogWarning) << "Removing bezels for " << bezelsystem;
	FILE *pipe = popen(updatecommand.c_str(), "r");
	char line[1024] = "";
	if (pipe == NULL) {
		return std::pair<std::string, int>(std::string("Error starting `batocera-es-thebezelproject remove` command."), -1);
	}

	while (fgets(line, 1024, pipe)) {
		strtok(line, "\n");
		// Long theme names/URL can crash the GUI MsgBox
		// "48" found by trials and errors. Ideally should be fixed
		// in es-core MsgBox -- FIXME
		if (strlen(line) > 48)
			line[47] = '\0';
		ui->setText(std::string(line));
	}

	int exitCode = pclose(pipe);
	return std::pair<std::string, int>(std::string(line), exitCode);
}

#if WIN32
std::string executeCMDInNewProcessAndReadOutput(LPSTR lpCommandLine)
{
	std::string ret;

	STARTUPINFO si;
	SECURITY_ATTRIBUTES sa;
	PROCESS_INFORMATION pi;
	HANDLE g_hChildStd_IN_Rd, g_hChildStd_OUT_Wr, g_hChildStd_OUT_Rd, g_hChildStd_IN_Wr;  //pipe handles
	char buf[1024];           //i/o buffer

	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	if (CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &sa, 0))   //create stdin pipe
	{
		if (CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &sa, 0))  //create stdout pipe
		{
			//set startupinfo for the spawned process
			/*The dwFlags member tells CreateProcess how to make the process.
			STARTF_USESTDHANDLES: validates the hStd* members.
			STARTF_USESHOWWINDOW: validates the wShowWindow member*/
			GetStartupInfo(&si);

			si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
			si.wShowWindow = SW_HIDE;
			//set the new handles for the child process
			si.hStdOutput = g_hChildStd_OUT_Wr;
			si.hStdError = g_hChildStd_OUT_Wr;
			si.hStdInput = g_hChildStd_IN_Rd;

			//spawn the child process
			if (CreateProcess(NULL, lpCommandLine, NULL, NULL, TRUE, CREATE_NEW_CONSOLE,
				NULL, NULL, &si, &pi))
			{
				unsigned long bread;   //bytes read
				unsigned long avail;   //bytes available
				memset(buf, 0, sizeof(buf));

				for (;;)
				{
					PeekNamedPipe(g_hChildStd_OUT_Rd, buf, 1023, &bread, &avail, NULL);
					//check to see if there is any data to read from stdout
					if (bread != 0)
					{
						while (ReadFile(g_hChildStd_OUT_Rd, buf, 1023, &bread, NULL))
						{						
							ret += std::string(buf);
							PeekNamedPipe(g_hChildStd_OUT_Rd, buf, 1023, &bread, &avail, NULL);
							if (bread == 0)
								break;
						}

						break;
					}
				}

				//clean up all handles
				CloseHandle(pi.hThread);
				CloseHandle(pi.hProcess);
				CloseHandle(g_hChildStd_IN_Rd);
				CloseHandle(g_hChildStd_OUT_Wr);
				CloseHandle(g_hChildStd_OUT_Rd);
				CloseHandle(g_hChildStd_IN_Wr);
			}
			else
			{
				CloseHandle(g_hChildStd_IN_Rd);
				CloseHandle(g_hChildStd_OUT_Wr);
				CloseHandle(g_hChildStd_OUT_Rd);
				CloseHandle(g_hChildStd_IN_Wr);
			}
		}
		else
		{
			CloseHandle(g_hChildStd_IN_Rd);
			CloseHandle(g_hChildStd_IN_Wr);
		}
	}

	return ret;
}
#endif

std::string ApiSystem::getCRC32(std::string fileName, bool fromZipContents)
{
	std::string cmd = "7zr h \"" + fileName + "\"";
	
	std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(fileName));
	if (fromZipContents && (ext == ".7z" || ext == ".zip"))
		cmd = "7zr l -slt \"" + fileName + "\"";

	std::string crc;
	std::string fn = Utils::FileSystem::getFileName(fileName);

#if WIN32
	// Windows : use x86 7za to test. x64 version fails ( cuz our process is x86 )
	cmd = Utils::String::replace(cmd, "7zr ", "C:\\src\\7za.exe ");

	std::string output = executeCMDInNewProcessAndReadOutput((char*)cmd.c_str());
	for (std::string all : Utils::String::splitAny(output, "\r\n"))
	{
		int idx = all.find("CRC = ");
		if (idx != std::string::npos)
			crc = all.substr(idx + 6);
		else if (all.find(fn) == (all.size() - fn.size()) && all.length() > 8 && all[9] == ' ')
			crc = all.substr(0, 8);
	}

	return crc;
#endif

	FILE *pipe = popen(cmd.c_str(), "r");
	if (pipe == NULL)
		return "";

	char line[1024];
	while (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");
		std::string all = line;

		int idx = all.find("CRC = ");
		if (idx != std::string::npos)
			crc = all.substr(idx + 6);
		else if (all.find(fn) == (all.size() - fn.size()) && all.length() > 8 && all[9] == ' ')
			crc = all.substr(0, 8);
	}
	
	pclose(pipe);

	return crc;
}

const char* BACKLIGHT_BRIGHTNESS_NAME = "/sys/class/backlight/backlight/brightness";
const char* BACKLIGHT_BRIGHTNESS_MAX_NAME = "/sys/class/backlight/backlight/max_brightness";
#define BACKLIGHT_BUFFER_SIZE 127

bool ApiSystem::getBrighness(int& value)
{
#if WIN32
	value = 100;
	return true;
#else
	value = 0;

	int fd;
	int max = 255;	
	char buffer[BACKLIGHT_BUFFER_SIZE + 1];
	ssize_t count;

	fd = open(BACKLIGHT_BRIGHTNESS_MAX_NAME, O_RDONLY);
	if (fd < 0)
		return false;

	memset(buffer, 0, BACKLIGHT_BUFFER_SIZE + 1);

	count = read(fd, buffer, BACKLIGHT_BUFFER_SIZE);
	if (count > 0)
		max = atoi(buffer);

	close(fd);

	if (max == 0) 
		return 0;

	fd = open(BACKLIGHT_BRIGHTNESS_NAME, O_RDONLY);
	if (fd < 0)
		return false;

	memset(buffer, 0, BACKLIGHT_BUFFER_SIZE + 1);

	count = read(fd, buffer, BACKLIGHT_BUFFER_SIZE);
	if (count > 0)
		value = atoi(buffer);

	close(fd);

	value = (uint32_t) (value / (float)max * 100.0f);
	return true;
#endif
}

void ApiSystem::setBrighness(int value)
{
#if !WIN32	
	if (value < 5)
		value = 5;

	if (value > 100)
		value = 100;

	int fd;
	int max = 255;
	char buffer[BACKLIGHT_BUFFER_SIZE + 1];
	ssize_t count;

	fd = open(BACKLIGHT_BRIGHTNESS_MAX_NAME, O_RDONLY);
	if (fd < 0)
		return;

	memset(buffer, 0, BACKLIGHT_BUFFER_SIZE + 1);

	count = read(fd, buffer, BACKLIGHT_BUFFER_SIZE);
	if (count > 0)
		max = atoi(buffer);

	close(fd);

	if (max == 0) 
		return;

	fd = open(BACKLIGHT_BRIGHTNESS_NAME, O_WRONLY);
	if (fd < 0)
		return;
	
	float percent = value / 100.0f * (float)max;
	sprintf(buffer, "%d\n", (uint32_t)percent);

	count = write(fd, buffer, strlen(buffer));
	if (count < 0)
		LOG(LogError) << "ApiSystem::setBrighness failed";

	close(fd);
#endif
}

std::vector<std::string> ApiSystem::getWifiNetworks(bool scan)
{
#if WIN32
	std::vector<std::string> res;
	if (scan)
	{
		::Sleep(1000);
		res.push_back("Toto");
		res.push_back("Tata");
		res.push_back("Titi");
	}
	res.push_back("Fake SSID");
	return res;
#endif

	return executeEnumerationScript(scan ? "batocera-wifi scanlist" : "batocera-wifi list");
}

std::vector<std::string> ApiSystem::executeEnumerationScript(const std::string command)
{
	LOG(LogDebug) << "ApiSystem::executeEnumerationScript -> " << command;

	std::vector<std::string> res;

#if WIN32
	return res;
#endif

	FILE *pipe = popen(command.c_str(), "r");
	if (pipe == NULL)
		return res;

	char line[1024];
	while (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");
		res.push_back(std::string(line));
	}

	pclose(pipe);
	return res;
}
