#include "ApiSystem.h"
#include "Settings.h"
#include "Log.h"
#include "HttpReq.h"
#include "AudioManager.h"
#include "VolumeControl.h"
#include "InputManager.h"
#include "EmulationStation.h"
#include "SystemConf.h"
#include "Sound.h"
#include "utils/Platform.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "utils/ThreadPool.h"
#include "RetroAchievements.h"
#include "utils/ZipFile.h"
#include "Paths.h"
#include "utils/VectorEx.h"
#include "LocaleES.h"

#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <fstream>
#include <SDL.h>
#include <pugixml/src/pugixml.hpp>
#include <rapidjson/rapidjson.h>
#include <rapidjson/pointer.h>

#if WIN32
#include <Windows.h>
#define popen _popen
#define pclose _pclose
#define WIFEXITED(x) x
#define WEXITSTATUS(x) x
#include "Win32ApiSystem.h"
#else
#include <sys/statvfs.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#endif

/*
#define script_config "batocera-config"; // canupdate, overscan enable, overscan disable, storage 'X', storage current, storage list, forgetBT, getRootPassword, lsoutputs
#define script_overclock "batocera-overclock"; // list, set X
#define script_upgrade "batocera-upgrade";
#define script_sync "batocera-sync"; // sync
#define script_install "batocera-install"; // listDisks, listArchs, install X Y
#define script_scraper "batocera-scraper";
#define script_kodi "batocera-kodi";
#define script_wifi "batocera-wifi"; // scanlist, list, enable X Y, disable
#define script_bluetooth "batocera-bluetooth"; // trust, list, remove 
#define script_resolution "batocera-resolution"; // listModes
#define script_sync "batocera-sync";   // list
#define script_info "batocera-info"; // --full
#define script_systems "batocera-systems"; // --filter
#define script_suport "batocera-support";
#define script_gameforce "batocera-gameforce"; // buttonColorLed X, powerLed X
#define script_audio "batocera-audio"; // list, list-profiles, get-profile, set-profile 'X', get, set 'X'
#define script_bezelproject "batocera-es-thebezelproject"; // list, install X, remove X
#define script_format "batocera-format"; // listDisks, listFstypes
#define script_store "batocera-store"; // list, update, refresh, clean-all, install "X", remove "X"
#define script_preupdategamelists "batocera-preupdate-gamelists-hook";
#define script_timezones "batocera-timezone"; // get, detect, set "X"
#define script_padsinfos "batocera-padsinfo";
#define script_swissknife "batocera-es-swissknife"; // --emukill"
*/

ApiSystem::ApiSystem() { }

ApiSystem* ApiSystem::instance = nullptr;

ApiSystem *ApiSystem::getInstance() 
{
	if (ApiSystem::instance == nullptr)
	{
#if WIN32
		ApiSystem::instance = new Win32ApiSystem();
#else
		ApiSystem::instance = new ApiSystem();
#endif
		
		IExternalActivity::Instance = ApiSystem::instance;
	}

	return ApiSystem::instance;
}

unsigned long ApiSystem::getFreeSpaceGB(std::string mountpoint) 
{
	LOG(LogDebug) << "ApiSystem::getFreeSpaceGB";

	int free = 0;

#if !WIN32
	struct statvfs fiData;
	if ((statvfs(mountpoint.c_str(), &fiData)) >= 0)
		free = (fiData.f_bfree * fiData.f_bsize) / (1024 * 1024 * 1024);
#endif

	return free;
}

std::string ApiSystem::getFreeSpaceUserInfo()
{
	return getFreeSpaceInfo(Paths::getRootPath());
}

std::string ApiSystem::getFreeSpaceSystemInfo()
{
	return getFreeSpaceInfo("/boot");
}

std::string ApiSystem::getFreeSpaceInfo(const std::string mountpoint)
{
	LOG(LogDebug) << "ApiSystem::getFreeSpaceInfo";

	std::ostringstream oss;

#if !WIN32
	struct statvfs fiData;
	if ((statvfs(mountpoint.c_str(), &fiData)) < 0)
		return "";
		
	unsigned long long total = (unsigned long long) fiData.f_blocks * (unsigned long long) (fiData.f_bsize);
	unsigned long long free = (unsigned long long) fiData.f_bfree * (unsigned long long) (fiData.f_bsize);
	unsigned long long used = total - free;
	unsigned long percent = 0;
	
	if (total != 0) 
	{  //for small SD card ;) with share < 1GB
		percent = used * 100 / total;
		oss << Utils::FileSystem::megaBytesToString(used / (1024L * 1024L)) << "/" << Utils::FileSystem::megaBytesToString(total / (1024L * 1024L)) << " (" << percent << "%)";
	}
	else
		oss << "N/A";	
#endif

	return oss.str();
}

bool ApiSystem::isFreeSpaceLimit() 
{
	return getFreeSpaceGB(Paths::getRootPath()) < 2;
}

std::string ApiSystem::getVersion(bool extra)
{
	LOG(LogDebug) << "ApiSystem::getVersion";

	if (isScriptingSupported(VERSIONINFO)) 
	{
		std::string command = "batocera-version";
		if (extra) 
			command += " --extra";

		auto res = executeEnumerationScript(command);
		if (res.size() > 0 && !res[0].empty())
			return res[0];
	}

	if (extra)
		return "none";

	std::string localVersionFile = Paths::getVersionInfoPath();
	if (!Utils::FileSystem::exists(localVersionFile))
		localVersionFile = Paths::findEmulationStationFile("version.info");

	if (Utils::FileSystem::exists(localVersionFile))
	{
		std::string localVersion = Utils::FileSystem::readAllText(localVersionFile);
		localVersion = Utils::String::replace(Utils::String::replace(localVersion, "\r", ""), "\n", "");
		return localVersion;
	}

	return PROGRAM_VERSION_STRING;	
}

std::string ApiSystem::getApplicationName()
{
	std::string localVersionFile = Paths::findEmulationStationFile("about.info");
	if (Utils::FileSystem::exists(localVersionFile))
	{
		std::string aboutInfo = Utils::FileSystem::readAllText(localVersionFile);
		aboutInfo = Utils::String::replace(Utils::String::replace(aboutInfo, "\r", ""), "\n", "");

		auto ver = ApiSystem::getInstance()->getVersion();
		auto cut = aboutInfo.find(" V" + ver);

		if (cut == std::string::npos)
			cut = aboutInfo.find(" " + ver);

		if (cut == std::string::npos)
			cut = aboutInfo.find(ver);

		if (cut != std::string::npos)
			aboutInfo = aboutInfo.substr(0, cut);

		return aboutInfo;
	}

#if BATOCERA
	return "BATOCERA";
#else
	return "EMULATIONSTATION";
#endif
}

bool ApiSystem::setOverscan(bool enable) 
{
	return executeScript("batocera-config overscan " + std::string(enable ? "enable" : "disable"));
}

bool ApiSystem::setOverclock(std::string mode) 
{
	if (mode.empty())
		return false;

	return executeScript("batocera-overclock set " + mode);
}

// BusyComponent* ui
std::pair<std::string, int> ApiSystem::updateSystem(const std::function<void(const std::string)>& func)
{
	LOG(LogDebug) << "ApiSystem::updateSystem";

	std::string updatecommand = "batocera-upgrade";

	FILE *pipe = popen(updatecommand.c_str(), "r");
	if (pipe == nullptr)
		return std::pair<std::string, int>(std::string("Cannot call update command"), -1);
	
	char line[1024] = "";
	FILE *flog = fopen(Utils::FileSystem::combine(Paths::getLogPath(), "batocera-upgrade.log").c_str(), "w");
	while (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");
		if (flog != nullptr) 
			fprintf(flog, "%s\n", line);

		if (func != nullptr)
			func(std::string(line));		
	}

	int exitCode = WEXITSTATUS(pclose(pipe));

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

	std::string updatecommand = "batocera-sync sync " + device;
	FILE* pipe = popen(updatecommand.c_str(), "r");
	if (pipe == NULL)
		return std::pair<std::string, int>(std::string("Cannot call sync command"), -1);

	char line[1024] = "";

	FILE* flog = fopen(Utils::FileSystem::combine(Paths::getLogPath(), "batocera-sync.log").c_str(), "w");
	while (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");

		if (flog != NULL) 
			fprintf(flog, "%s\n", line);

		ui->setText(std::string(line));
	}

	if (flog != NULL) 
		fclose(flog);

	int exitCode = WEXITSTATUS(pclose(pipe));
	return std::pair<std::string, int>(std::string(line), exitCode);
}

std::pair<std::string, int> ApiSystem::installSystem(BusyComponent* ui, std::string device, std::string architecture) 
{
	LOG(LogDebug) << "ApiSystem::installSystem";

	std::string updatecommand = "batocera-install install " + device + " " + architecture;
	FILE *pipe = popen(updatecommand.c_str(), "r");
	if (pipe == NULL)
		return std::pair<std::string, int>(std::string("Cannot call install command"), -1);

	char line[1024] = "";

	FILE *flog = fopen(Utils::FileSystem::combine(Paths::getLogPath(), "batocera-install.log").c_str(), "w");
	while (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");
		if (flog != NULL) fprintf(flog, "%s\n", line);
		ui->setText(std::string(line));
	}

	int exitCode = WEXITSTATUS(pclose(pipe));

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

	FILE* pipe = popen("batocera-scraper", "r");
	if (pipe == nullptr)
		return std::pair<std::string, int>(std::string("Cannot call scrape command"), -1);

	char line[1024] = "";

	FILE* flog = fopen(Utils::FileSystem::combine(Paths::getLogPath(), "batocera-scraper.log").c_str(), "w");
	while (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");

		if (flog != NULL) 
			fprintf(flog, "%s\n", line);

		if (ui != nullptr && Utils::String::startsWith(line, "GAME: "))
			ui->setText(std::string(line));	
	}

	if (flog != nullptr)
		fclose(flog);

	int exitCode = WEXITSTATUS(pclose(pipe));
	return std::pair<std::string, int>(std::string(line), exitCode);
}

bool ApiSystem::ping() 
{
	// ping Google, if it fails then move on, if succeeds exit loop and return "true"
	if (!executeScript("timeout 1 ping -c 1 -t 255 google.com"))
	{
		// ping Google DNS
		if (!executeScript("timeout 1 ping -c 1 -t 255 8.8.8.8"))
		{
			// ping Google secondary DNS & give 2 seconds, return this one's status
			return executeScript("timeout 2 ping -c 1 -t 255 8.8.4.4");
		}
	}

	return true;
}

bool ApiSystem::canUpdate(std::vector<std::string>& output) 
{
	LOG(LogDebug) << "ApiSystem::canUpdate";

	FILE *pipe = popen("batocera-config canupdate", "r");
	if (pipe == NULL)
		return false;

	char line[1024];
	while (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");
		output.push_back(std::string(line));
	}

	int res = WEXITSTATUS(pclose(pipe));
	if (res == 0) 
	{
		LOG(LogInfo) << "Can update ";
		return true;
	}

	LOG(LogInfo) << "Cannot update ";
	return false;
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
	std::string command = "batocera-kodi " + commandline;

	ApiSystem::launchExternalWindow_before(window);

	int exitCode = system(command.c_str());

	// WIFEXITED returns a nonzero value if the child process terminated normally with exit or _exit.
	// https://www.gnu.org/software/libc/manual/html_node/Process-Completion-Status.html
	if (WIFEXITED(exitCode))
		exitCode = WEXITSTATUS(exitCode);

	ApiSystem::launchExternalWindow_after(window);

	// handle end of kodi
	switch (exitCode) 
	{
	case 10: // reboot code
		Utils::Platform::quitES(Utils::Platform::QuitMode::REBOOT);
		return true;
		
	case 11: // shutdown code
		Utils::Platform::quitES(Utils::Platform::QuitMode::SHUTDOWN);
		return true;
	}

	return exitCode == 0;
}

bool ApiSystem::launchFileManager(Window *window) 
{
	LOG(LogDebug) << "ApiSystem::launchFileManager";

	std::string command = "filemanagerlauncher";

	ApiSystem::launchExternalWindow_before(window);

	int exitCode = system(command.c_str());
	if (WIFEXITED(exitCode))
		exitCode = WEXITSTATUS(exitCode);

	ApiSystem::launchExternalWindow_after(window);

	return exitCode == 0;
}

bool ApiSystem::enableWifi(std::string ssid, std::string key) 
{
	return executeScript("batocera-wifi enable \"" + ssid + "\" \"" + key + "\"");
}

bool ApiSystem::disableWifi() 
{
	return executeScript("batocera-wifi disable");
}

std::string ApiSystem::getIpAdress() 
{
	LOG(LogDebug) << "ApiSystem::getIpAdress";
	
	std::string result = Utils::Platform::queryIPAdress(); // platform.h
	if (result.empty())
		return "NOT CONNECTED";

	return result;
}

bool ApiSystem::enableBluetooth()
{
	return executeScript("batocera-bluetooth enable 2>&1 >/dev/null");
}

bool ApiSystem::disableBluetooth()
{
	return executeScript("batocera-bluetooth disable");
}

void ApiSystem::startBluetoothLiveDevices(const std::function<void(const std::string)>& func)
{
	executeScript("batocera-bluetooth live_devices", func);
}

void ApiSystem::stopBluetoothLiveDevices()
{
	executeScript("batocera-bluetooth stop_live_devices");
}

bool ApiSystem::pairBluetoothDevice(const std::string& deviceName)
{
	return executeScript("batocera-bluetooth trust " + deviceName);
}

bool ApiSystem::removeBluetoothDevice(const std::string& deviceName)
{
	return executeScript("batocera-bluetooth remove " + deviceName);
}

bool ApiSystem::scanNewBluetooth(const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-bluetooth trust input", func).second == 0;
}

std::vector<std::string> ApiSystem::getPairedBluetoothDeviceList()
{
	return executeEnumerationScript("batocera-bluetooth list");
}


std::vector<std::string> ApiSystem::getAvailableStorageDevices() 
{
	return executeEnumerationScript("batocera-config storage list");
}

std::vector<std::string> ApiSystem::getVideoModes(const std::string output)
{
  if(output == "") {
    return executeEnumerationScript("batocera-resolution listModes");
  } else {
    return executeEnumerationScript("batocera-resolution --screen \"" + output + "\" listModes");
  }
}

std::vector<std::string> ApiSystem::getCustomRunners() 
{
	return executeEnumerationScript("batocera-wine-runners");
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
	return executeEnumerationScript("batocera-info --full");
}

std::vector<BiosSystem> ApiSystem::getBiosInformations(const std::string system) 
{
	std::vector<BiosSystem> res;
	BiosSystem current;
	bool isCurrent = false;

	std::string cmd = "batocera-systems";
	if (!system.empty())
		cmd += " --filter " + system;

	auto systems = executeEnumerationScript(cmd);
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
	return executeScript("batocera-support");
}

std::string ApiSystem::getCurrentStorage() 
{
	LOG(LogDebug) << "ApiSystem::getCurrentStorage";

#if WIN32
	return "DEFAULT";
#endif

	std::ostringstream oss;
	oss << "batocera-config storage current";
	FILE *pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
		return "";	

	if (fgets(line, 1024, pipe)) {
		strtok(line, "\n");
		pclose(pipe);
		return std::string(line);
	}
	return "INTERNAL";
}

bool ApiSystem::setStorage(std::string selected) 
{
	return executeScript("batocera-config storage " + selected);
}

bool ApiSystem::setButtonColorGameForce(std::string selected)
{
	return executeScript("batocera-gameforce buttonColorLed " + selected);
}

bool ApiSystem::setPowerLedGameForce(std::string selected)
{
	return executeScript("batocera-gameforce powerLed " + selected);
}

bool ApiSystem::forgetBluetoothControllers() 
{
	return executeScript("batocera-config forgetBT");
}

std::string ApiSystem::getRootPassword() 
{
	LOG(LogDebug) << "ApiSystem::getRootPassword";

	std::ostringstream oss;
	oss << "batocera-config getRootPassword";
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

std::vector<std::string> ApiSystem::getAvailableVideoOutputDevices() 
{
	return executeEnumerationScript("batocera-config lsoutputs");
}

std::vector<std::string> ApiSystem::getAvailableAudioOutputDevices() 
{
#if WIN32
	std::vector<std::string> res;
	res.push_back("auto");
	return res;
#endif

	return executeEnumerationScript("batocera-audio list");
}

std::string ApiSystem::getCurrentAudioOutputDevice() 
{
#if WIN32
	return "auto";
#endif

	LOG(LogDebug) << "ApiSystem::getCurrentAudioOutputDevice";

	std::ostringstream oss;
	oss << "batocera-audio get";
	FILE *pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
		return "";	

	if (fgets(line, 1024, pipe)) 
	{
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

	oss << "batocera-audio set" << " '" << selected << "'";
	int exitcode = system(oss.str().c_str());

	Sound::get(":/checksound.ogg")->play();

	return exitcode == 0;
}

std::vector<std::string> ApiSystem::getAvailableAudioOutputProfiles()
{
#if WIN32
	std::vector<std::string> res;
	res.push_back("auto");
	return res;
#endif

	return executeEnumerationScript("batocera-audio list-profiles");
}

std::string ApiSystem::getCurrentAudioOutputProfile() 
{
#if WIN32
	return "auto";
#endif

	LOG(LogDebug) << "ApiSystem::getCurrentAudioOutputProfile";

	std::ostringstream oss;
	oss << "batocera-audio get-profile";
	FILE *pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
		return "";	

	if (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");
		pclose(pipe);
		return std::string(line);
	}

	return "";
}

bool ApiSystem::setAudioOutputProfile(std::string selected) 
{
	LOG(LogDebug) << "ApiSystem::setAudioOutputProfile";

	std::ostringstream oss;

	oss << "batocera-audio set-profile" << " '" << selected << "'";
	int exitcode = system(oss.str().c_str());
	
	Sound::get(":/checksound.ogg")->play();

	return exitcode == 0;
}

std::string ApiSystem::getUpdateUrl()
{
	auto systemsetting = SystemConf::getInstance()->get("global.updates.url");
	if (!systemsetting.empty())
		return systemsetting;

	return "https://updates.batocera.org";
}

std::string ApiSystem::getThemesUrl()
{
	auto systemsetting = SystemConf::getInstance()->get("global.themes.url");
	if (!systemsetting.empty())
		return systemsetting;

	return getUpdateUrl() + "/themes.json";
}

std::string ApiSystem::getGitRepositoryDefaultBranch(const std::string& url)
{
	std::string ret = "master";

	std::string statUrl = Utils::String::replace(url, "https://github.com/", "https://api.github.com/repos/");
	if (statUrl != url)
	{
		HttpReq statreq(statUrl);
		if (statreq.wait())
		{
			const std::string default_branch = "\"default_branch\": ";

			std::string content = statreq.getContent();
			auto pos = content.find(default_branch);
			if (pos != std::string::npos)
			{
				auto end = content.find(",", pos);
				if (end != std::string::npos)
				{
					ret = Utils::String::replace(content.substr(pos + default_branch.length(), end - pos - default_branch.length()), "\"", "");
				}
			}
		}
	}

	return ret;
}

bool ApiSystem::downloadGitRepository(const std::string& url, const std::string& branch, const std::string& fileName, const std::string& label, const std::function<void(const std::string)>& func, int64_t defaultDownloadSize)
{
	if (func != nullptr)
		func("Downloading " + label);

	int64_t downloadSize = defaultDownloadSize;
	if (downloadSize == 0)
	{
		std::string statUrl = Utils::String::replace(url, "https://github.com/", "https://api.github.com/repos/");
		if (statUrl != url)
		{
			HttpReq statreq(statUrl);
			if (statreq.wait())
			{
				std::string content = statreq.getContent();
				auto pos = content.find("\"size\": ");
				if (pos != std::string::npos)
				{
					auto end = content.find(",", pos);
					if (end != std::string::npos)
						downloadSize = atoi(content.substr(pos + 8, end - pos - 8).c_str()) * 1024LL;
				}
			}
		}
	}

	HttpReq httpreq(url + "/archive/"+ branch +".zip", fileName);

	int curPos = -1;
	while (httpreq.status() == HttpReq::REQ_IN_PROGRESS)
	{
		if (downloadSize > 0)
		{
			int64_t pos = httpreq.getPosition();
			if (pos > 0 && curPos != pos)
			{
				if (func != nullptr)
				{
					std::string pc = std::to_string((int)(pos * 100LL / downloadSize));
					func(std::string("Downloading " + label + " >>> " + pc + " %"));
				}

				curPos = pos;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	if (httpreq.status() != HttpReq::REQ_SUCCESS)
		return false;

	return true;
}

bool ApiSystem::isThemeInstalled(const std::string& themeName, const std::string& url)
{
	std::string themeUrl = Utils::FileSystem::getFileName(url);

	std::vector<std::string> paths =
	{
		Paths::getUserThemesPath(),
		Paths::getThemesPath(),
		Paths::getUserEmulationStationPath() + "/themes"
#if !WIN32
		,"/etc/emulationstation/themes" // Backward compatibility with Retropie
#endif
	};

	for (auto path : VectorHelper::distinct(paths, [](auto x) { return x; }))
	{
		if (path.empty())
			continue;

		if (Utils::FileSystem::isDirectory(path + "/" + themeUrl))
			return true;

		if (Utils::FileSystem::isDirectory(path + "/" + themeUrl + "-master"))
			return true;

		if (Utils::FileSystem::isDirectory(path + "/" + themeName))
			return true;
	}

	return false;
}

extern std::string jsonString(const rapidjson::Value& val, const std::string& name);
extern int jsonInt(const rapidjson::Value& val, const std::string& name);

std::vector<BatoceraTheme> ApiSystem::getBatoceraThemesList()
{
	LOG(LogDebug) << "ApiSystem::getBatoceraThemesList";

	std::vector<BatoceraTheme> res;

	auto url = getThemesUrl();

	HttpReq httpreq(url);
	if (httpreq.wait())
	{
		rapidjson::Document doc;
		doc.Parse(httpreq.getContent().c_str());
		if (doc.HasParseError())
			return res;

		if (!doc.HasMember("data"))
			return res;

		for (auto& item : doc["data"].GetArray())
		{
			BatoceraTheme bt;
			bt.name = jsonString(item, "theme");
			bt.url = jsonString(item, "theme_url");
			bt.author = jsonString(item, "author");
			bt.lastUpdate = jsonString(item, "last_update");
			bt.upToDate = jsonInt(item, "up_to_date");
			bt.size = jsonInt(item, "size");
			bt.isInstalled = isThemeInstalled(bt.name, bt.url);

			auto screenShot = jsonString(item, "screenshot");
			if (!screenShot.empty())
				bt.image = Utils::FileSystem::getParent(url) + "/" + screenShot;

			res.push_back(bt);
		}
	}

	return res;	
}

std::pair<std::string, int> ApiSystem::installBatoceraTheme(std::string thname, const std::function<void(const std::string)>& func)
{
	for (auto theme : getBatoceraThemesList())
	{
		if (theme.name != thname)
			continue;

		std::string installFolder = Paths::getUserThemesPath();
		if (installFolder.empty())
			installFolder = Paths::getThemesPath();
		if (installFolder.empty())
			installFolder = Paths::getUserEmulationStationPath() + "/themes";

		std::string themeFileName = Utils::FileSystem::getFileName(theme.url);
		std::string extractionDirectory = installFolder + "/.tmp";
		std::string zipFile = extractionDirectory +"/" + themeFileName + ".zip";

		Utils::FileSystem::createDirectory(extractionDirectory);
		Utils::FileSystem::removeFile(zipFile);
		
		std::string branch = getGitRepositoryDefaultBranch(theme.url);

		if (downloadGitRepository(theme.url, branch, zipFile, thname, func, theme.size * 1024LL * 1024))
		{
			if (func != nullptr)
				func(_("Extracting") + " " + thname);

			unzipFile(zipFile, extractionDirectory);
			
			std::string folderName = extractionDirectory + "/" + themeFileName + "-" + branch;
			if (!Utils::FileSystem::exists(folderName))
				folderName = extractionDirectory + "/" + themeFileName;

			if (Utils::FileSystem::exists(folderName))
			{
				std::string finalfolderName = installFolder  + "/" + themeFileName;
				if (Utils::FileSystem::exists(finalfolderName))
					Utils::FileSystem::deleteDirectoryFiles(finalfolderName, true);

				Utils::FileSystem::renameFile(folderName, finalfolderName);
			}

			Utils::FileSystem::removeFile(zipFile);
			Utils::FileSystem::deleteDirectoryFiles(extractionDirectory, true);

			return std::pair<std::string, int>(std::string("OK"), 0);
		}

		Utils::FileSystem::deleteDirectoryFiles(extractionDirectory, true);
		return std::pair<std::string, int>(std::string(""), 1);
	}

	return std::pair<std::string, int>(std::string(""), 1);
}

std::pair<std::string, int> ApiSystem::uninstallBatoceraTheme(std::string thname, const std::function<void(const std::string)>& func)
{
	for (auto theme : getBatoceraThemesList())
	{
		if (!theme.isInstalled || theme.name != thname)
			continue;

		std::string installFolder = Paths::getUserThemesPath();
		if (installFolder.empty())
			installFolder = Paths::getThemesPath();
		if (installFolder.empty())
			installFolder = Paths::getUserEmulationStationPath() + "/themes";

		std::string themeFileName = Utils::FileSystem::getFileName(theme.url);

		std::string folderName = installFolder + "/" + themeFileName;
		if (!Utils::FileSystem::exists(folderName))
			folderName = folderName + "-master";

		if (Utils::FileSystem::exists(folderName))
		{
			Utils::FileSystem::deleteDirectoryFiles(folderName, true);
			return std::pair<std::string, int>("OK", 0);
		}

		break;
	}

	return std::pair<std::string, int>(std::string(""), 1);
}

std::vector<BatoceraBezel> ApiSystem::getBatoceraBezelsList()
{
	LOG(LogInfo) << "ApiSystem::getBatoceraBezelsList";

	std::vector<BatoceraBezel> res;

	auto lines = executeEnumerationScript("batocera-es-thebezelproject list");
	for (auto line : lines)
	{
		auto parts = Utils::String::splitAny(line, " \t");
		if (parts.size() < 2)
			continue;

		if (!Utils::String::startsWith(parts[0], "[I]") && !Utils::String::startsWith(parts[0], "[A]"))
			continue;

		BatoceraBezel bz;
		bz.isInstalled = (Utils::String::startsWith(parts[0], "[I]"));
		bz.name = parts[1];
		bz.url = parts.size() < 3 ? "" : (parts[2] == "-" ? parts[3] : parts[2]);
		bz.folderPath = parts.size() < 4 ? "" : parts[3];

		if (bz.name != "?")
			res.push_back(bz);
	}

	return res;
}

std::pair<std::string, int> ApiSystem::installBatoceraBezel(std::string bezelsystem, const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-es-thebezelproject install " + bezelsystem, func);
}

std::pair<std::string, int> ApiSystem::uninstallBatoceraBezel(std::string bezelsystem, const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-es-thebezelproject remove " + bezelsystem, func);
}

std::string ApiSystem::getMD5(const std::string fileName, bool fromZipContents)
{
	LOG(LogDebug) << "getMD5 >> " << fileName;

	// 7za x -so test.7z | md5sum
	std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(fileName));
	if (ext == ".zip" && fromZipContents)
	{
		Utils::Zip::ZipFile file;
		if (file.load(fileName))
		{
			std::string romName;

			for (auto name : file.namelist())
			{
				if (Utils::FileSystem::getExtension(name) != ".txt" && !Utils::String::endsWith(name, "/"))
				{
					if (!romName.empty())
					{
						romName = "";
						break;
					}

					romName = name;
				}
			}

			if (!romName.empty())
				return file.getFileMd5(romName);
		}
	}

#if !WIN32
	if (fromZipContents && ext == ".7z")
	{
		auto cmd = getSevenZipCommand() + " x -so \"" + fileName + "\" | md5sum";
		auto ret = executeEnumerationScript(cmd);
		if (ret.size() == 1 && ret.cbegin()->length() >= 32)
			return ret.cbegin()->substr(0, 32);
	}
#endif

	std::string contentFile = fileName;
	std::string ret;
	std::string tmpZipDirectory;

	if (fromZipContents && ext == ".7z")
	{
		tmpZipDirectory = Utils::FileSystem::combine(Utils::FileSystem::getTempPath(), Utils::FileSystem::getStem(fileName));
		Utils::FileSystem::deleteDirectoryFiles(tmpZipDirectory);

		if (unzipFile(fileName, tmpZipDirectory))
		{
			auto fileList = Utils::FileSystem::getDirContent(tmpZipDirectory, true);

			std::vector<std::string> res;
			std::copy_if(fileList.cbegin(), fileList.cend(), std::back_inserter(res), [](const std::string file) { return Utils::FileSystem::getExtension(file) != ".txt";  });
		
			if (res.size() == 1)
				contentFile = *res.cbegin();
		}

		// if there's no file or many files ? get md5 of archive
	}

	ret = Utils::FileSystem::getFileMd5(contentFile);

	if (!tmpZipDirectory.empty())
		Utils::FileSystem::deleteDirectoryFiles(tmpZipDirectory, true);

	LOG(LogDebug) << "getMD5 << " << ret;

	return ret;
}

std::string ApiSystem::getCRC32(std::string fileName, bool fromZipContents)
{
	LOG(LogDebug) << "getCRC32 >> " << fileName;

	std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(fileName));

	if (ext == ".7z" && fromZipContents)
	{
		LOG(LogDebug) << "getCRC32 is using 7z";

		std::string fn = Utils::FileSystem::getFileName(fileName);
		auto cmd = getSevenZipCommand() + " l -slt \"" + fileName + "\"";
		auto lines = executeEnumerationScript(cmd);
		for (std::string all : lines)
		{
			int idx = all.find("CRC = ");
			if (idx != std::string::npos)
				return all.substr(idx + 6);
			else if (all.find(fn) == (all.size() - fn.size()) && all.length() > 8 && all[9] == ' ')
				return all.substr(0, 8);
		}
	}
	else if (ext == ".zip" && fromZipContents)
	{
		LOG(LogDebug) << "getCRC32 is using ZipFile";

		Utils::Zip::ZipFile file;
		if (file.load(fileName))
		{
			std::string romName;

			for (auto name : file.namelist())
			{
				if (Utils::FileSystem::getExtension(name) != ".txt" && !Utils::String::endsWith(name, "/"))
				{
					if (!romName.empty())
					{
						romName = "";
						break;
					}

					romName = name;
				}
			}

			if (!romName.empty())
				return file.getFileCrc(romName);
		}
	}

	LOG(LogDebug) << "getCRC32 is using fileBuffer";
	return Utils::FileSystem::getFileCrc32(fileName);
}

bool ApiSystem::unzipFile(const std::string fileName, const std::string destFolder, const std::function<bool(const std::string)>& shouldExtract)
{
	LOG(LogDebug) << "unzipFile >> " << fileName << " to " << destFolder;

	if (!Utils::FileSystem::exists(destFolder))
		Utils::FileSystem::createDirectory(destFolder);
		
	if (Utils::String::toLower(Utils::FileSystem::getExtension(fileName)) == ".zip")
	{
		LOG(LogDebug) << "unzipFile is using ZipFile";

		Utils::Zip::ZipFile file;
		if (file.load(fileName))
		{
			for (auto name : file.namelist())
			{
				if (Utils::String::endsWith(name, "/"))
				{
					Utils::FileSystem::createDirectory(Utils::FileSystem::combine(destFolder, name.substr(0, name.length() - 1)));
					continue;
				}

				if (shouldExtract != nullptr && !shouldExtract(Utils::FileSystem::combine(destFolder, name)))
					continue;

				file.extract(name, destFolder);
			}

			LOG(LogDebug) << "unzipFile << OK";
			return true;
		}

		LOG(LogDebug) << "unzipFile << KO Bad format ?" << fileName;
		return false;
	}
	
	LOG(LogDebug) << "unzipFile is using 7z";

	std::string cmd = getSevenZipCommand() + " x \"" + Utils::FileSystem::getPreferredPath(fileName) + "\" -y -o\"" + Utils::FileSystem::getPreferredPath(destFolder) + "\"";
	bool ret = executeScript(cmd);
	LOG(LogDebug) << "unzipFile <<";
	return ret;
}

static std::string BACKLIGHT_BRIGHTNESS_NAME;
static std::string BACKLIGHT_BRIGHTNESS_MAX_NAME;

bool ApiSystem::getBrightness(int& value)
{	
	#if WIN32
	return false;
	#endif

	if (BACKLIGHT_BRIGHTNESS_NAME == "notfound")
		return false;

	if (BACKLIGHT_BRIGHTNESS_NAME.empty() || BACKLIGHT_BRIGHTNESS_MAX_NAME.empty())
	{
		for (auto file : Utils::FileSystem::getDirContent("/sys/class/backlight"))
		{				
			std::string brightnessPath = file + "/brightness";
			std::string maxBrightnessPath = file + "/max_brightness";

			if (Utils::FileSystem::exists(brightnessPath) && Utils::FileSystem::exists(maxBrightnessPath))
			{
				BACKLIGHT_BRIGHTNESS_NAME = brightnessPath;
				BACKLIGHT_BRIGHTNESS_MAX_NAME = maxBrightnessPath;

				LOG(LogInfo) << "ApiSystem::getBrightness > brightness path resolved to " << file;
				break;
			}
		}
	}

	if (BACKLIGHT_BRIGHTNESS_NAME.empty() || BACKLIGHT_BRIGHTNESS_MAX_NAME.empty())
	{
		LOG(LogInfo) << "ApiSystem::getBrightness > brightness path is not resolved";

		BACKLIGHT_BRIGHTNESS_NAME = "notfound";
		return false;
	}

	value = 0;

	int max = Utils::String::toInteger(Utils::FileSystem::readAllText(BACKLIGHT_BRIGHTNESS_MAX_NAME));
	if (max == 0)
		return false;

	if (Utils::FileSystem::exists(BACKLIGHT_BRIGHTNESS_NAME))
		value = Utils::String::toInteger(Utils::FileSystem::readAllText(BACKLIGHT_BRIGHTNESS_NAME));

	value = (uint32_t) ((value / (float)max * 100.0f) + 0.5f);
	return true;
}

void ApiSystem::setBrightness(int value)
{
#if WIN32	
	return;
#endif 

	if (BACKLIGHT_BRIGHTNESS_NAME.empty() || BACKLIGHT_BRIGHTNESS_NAME == "notfound")
		return;

	if (value < 1)
		value = 1;

	if (value > 100)
		value = 100;

	int max = Utils::String::toInteger(Utils::FileSystem::readAllText(BACKLIGHT_BRIGHTNESS_MAX_NAME));
	if (max == 0)
		return;

	float percent = (value / 100.0f * (float)max) + 0.5f;
		
	std::string content = std::to_string((uint32_t) percent) + "\n";
	Utils::FileSystem::writeAllText(BACKLIGHT_BRIGHTNESS_NAME, content);
}

static std::string LED_COLOUR_NAME;
static std::string LED_BRIGHTNESS_VALUE;
static std::string LED_MAX_BRIGHTNESS_VALUE;

bool ApiSystem::getLED(int& red, int& green, int& blue)
{	
	#if WIN32
	return false;
	#endif

	if (LED_COLOUR_NAME == "notfound")
		return false;

	if (LED_COLOUR_NAME.empty())
	{
		auto directories = Utils::FileSystem::getDirContent("/sys/class/leds");

        for (const auto& directory : directories)
        {
            if (directory.find("multicolor") != std::string::npos)
            {
				std::string ledColourPath = directory + "/multi_intensity";
				
				if (Utils::FileSystem::exists(ledColourPath))
				{
					LED_COLOUR_NAME = ledColourPath;

					LOG(LogInfo) << "ApiSystem::getLED > LED path resolved to " << directory;
					break;
				}
			}
		}
	}

	if (LED_COLOUR_NAME.empty())
	{
		LOG(LogInfo) << "ApiSystem::getLED > LED path is not resolved";

		LED_COLOUR_NAME = "notfound";
		return false;
	}

    if (Utils::FileSystem::exists(LED_COLOUR_NAME)) {
        std::string colourValue = Utils::FileSystem::readAllText(LED_COLOUR_NAME);
        std::stringstream ss(colourValue);
        std::string token;

        // Extract red value
        std::getline(ss, token, ' ');
        red = std::stoi(token);

        // Extract green value
        std::getline(ss, token, ' ');
        green = std::stoi(token);

        // Extract blue value
        std::getline(ss, token);
        blue = std::stoi(token);

		LOG(LogInfo) << "ApiSystem::getLED > LED colours are:" << red << " " << green << " " << blue;

        return true;
    }
}

void ApiSystem::getLEDColours(int& red, int& green, int& blue)
{
	if (Utils::FileSystem::exists(LED_COLOUR_NAME)) {
        std::string colourValue = Utils::FileSystem::readAllText(LED_COLOUR_NAME);
        std::stringstream ss(colourValue);
        std::string token;

        // Extract red value
        std::getline(ss, token, ' ');
        red = std::stoi(token);

        // Extract green value
        std::getline(ss, token, ' ');
        green = std::stoi(token);

        // Extract blue value
        std::getline(ss, token);
        blue = std::stoi(token);

		LOG(LogInfo) << "ApiSystem::getLEDColours > LED colours are: " << red << " " << green << " " << blue;
    }
}

void ApiSystem::setLEDColours(int red, int green, int blue)
{
#if WIN32    
    return;
#endif 

    if (LED_COLOUR_NAME.empty() || LED_COLOUR_NAME == "notfound")
        return;

    // Ensure RGB values are within valid range
	if (red < 0) red = 0;
    if (red > 255) red = 255;
    if (green < 0) green = 0;
    if (green > 255) green = 255;
    if (blue < 0) blue = 0;
    if (blue > 255) blue = 255;

    std::string content = std::to_string(red) + " " + std::to_string(green) + " " + std::to_string(blue);

    // Write LED color values to file
    Utils::FileSystem::writeAllText(LED_COLOUR_NAME, content);
}

bool ApiSystem::getLEDBrightness(int& value)
{   
    #if WIN32
    return false;
    #endif

    if (LED_BRIGHTNESS_VALUE == "notfound")
        return false;

    if (LED_BRIGHTNESS_VALUE.empty() || LED_MAX_BRIGHTNESS_VALUE.empty())
    {
        auto directories = Utils::FileSystem::getDirContent("/sys/class/leds");

        for (const auto& directory : directories)
        {
            if (directory.find("multicolor") != std::string::npos)
            {
                std::string ledBrightnessPath = directory + "/brightness";
                std::string ledMaxBrightnessPath = directory + "/max_brightness";

                if (Utils::FileSystem::exists(ledBrightnessPath) && Utils::FileSystem::exists(ledMaxBrightnessPath))
                {
                    LED_BRIGHTNESS_VALUE = ledBrightnessPath;
                    LED_MAX_BRIGHTNESS_VALUE = ledMaxBrightnessPath;

                    LOG(LogInfo) << "ApiSystem::getLEDBrightness > LED brightness path resolved to " << directory;
                    break;
                }
            }
        }
    }

    if (LED_BRIGHTNESS_VALUE.empty() || LED_MAX_BRIGHTNESS_VALUE.empty())
    {
        LOG(LogInfo) << "ApiSystem::getLEDBrightness > LED brightness path is not resolved";

        LED_BRIGHTNESS_VALUE = "notfound";
        return false;
    }

    value = 0;

    int max = Utils::String::toInteger(Utils::FileSystem::readAllText(LED_MAX_BRIGHTNESS_VALUE));
    if (max == 0)
        return false;

    if (Utils::FileSystem::exists(LED_BRIGHTNESS_VALUE))
        value = Utils::String::toInteger(Utils::FileSystem::readAllText(LED_BRIGHTNESS_VALUE));

    value = (uint32_t) ((value / (float)max * 100.0f) + 0.5f);
    return true;
}

void ApiSystem::setLEDBrightness(int value) {
#if WIN32
    return;
#endif

    if (LED_BRIGHTNESS_VALUE.empty() || LED_BRIGHTNESS_VALUE == "notfound")
        return;

    if (value < 0) value = 0;
	if (value > 100) value = 100;

    int max = Utils::String::toInteger(Utils::FileSystem::readAllText(LED_MAX_BRIGHTNESS_VALUE));
    if (max == 0)
        return;

    float percent = static_cast<float>(value) / 100.0f;
	int brightnessValue = static_cast<int>(percent * max + 0.5f);
    std::string content = std::to_string(brightnessValue) + "\n";
    Utils::FileSystem::writeAllText(LED_BRIGHTNESS_VALUE, content);
}

std::vector<std::string> ApiSystem::getWifiNetworks(bool scan)
{
	return executeEnumerationScript(scan ? "batocera-wifi scanlist" : "batocera-wifi list");
}

std::vector<std::string> ApiSystem::executeEnumerationScript(const std::string command)
{
	LOG(LogDebug) << "ApiSystem::executeEnumerationScript -> " << command;

	std::vector<std::string> res;

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

std::pair<std::string, int> ApiSystem::executeScript(const std::string command, const std::function<void(const std::string)>& func)
{
	LOG(LogInfo) << "ApiSystem::executeScript -> " << command;

	FILE *pipe = popen(command.c_str(), "r");
	if (pipe == NULL)
	{
		LOG(LogError) << "Error executing " << command;
		return std::pair<std::string, int>("Error starting command : " + command, -1);
	}

	char line[1024];
	while (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");

		if (func != nullptr)
			func(std::string(line));
	}

	int exitCode = WEXITSTATUS(pclose(pipe));
	return std::pair<std::string, int>(line, exitCode);
}

bool ApiSystem::executeScript(const std::string command)
{	
	LOG(LogInfo) << "Running " << command;

	if (system(command.c_str()) == 0)
		return true;
	
	LOG(LogError) << "Error executing " << command;
	return false;
}

bool ApiSystem::isScriptingSupported(ScriptId script)
{
	std::vector<std::string> executables;

	switch (script)
	{
	case ApiSystem::THEMESDOWNLOADER:
		return true;
	case ApiSystem::RETROACHIVEMENTS:
#ifdef CHEEVOS_DEV_LOGIN
		return true;
#endif
		break;
	case ApiSystem::KODI:
		executables.push_back("kodi");
		break;
	case ApiSystem::WIFI:
		executables.push_back("batocera-wifi");
		break;
	case ApiSystem::BLUETOOTH:
		executables.push_back("batocera-bluetooth");
		break;
	case ApiSystem::RESOLUTION:
		executables.push_back("batocera-resolution");
		break;
	case ApiSystem::BIOSINFORMATION:
		executables.push_back("batocera-systems");
		break;
	case ApiSystem::DISKFORMAT:
		executables.push_back("batocera-format");
		break;
	case ApiSystem::OVERCLOCK:
		executables.push_back("batocera-overclock");
		break;
	case ApiSystem::NETPLAY:
		executables.push_back("7zr");
		break;
	case ApiSystem::PDFEXTRACTION:
		executables.push_back("pdftoppm");
		executables.push_back("pdfinfo");
		break;
	case ApiSystem::BATOCERASTORE:
		executables.push_back("batocera-store");
		break;
	case ApiSystem::THEBEZELPROJECT:
		executables.push_back("batocera-es-thebezelproject");
		break;		
	case ApiSystem::PADSINFO:
		executables.push_back("batocera-padsinfo");
		break;
	case ApiSystem::EVMAPY:
		executables.push_back("evmapy");
		break;
	case ApiSystem::BATOCERAPREGAMELISTSHOOK:
		executables.push_back("batocera-preupdate-gamelists-hook");
		break;
	case ApiSystem::TIMEZONES:
		executables.push_back("batocera-timezone");
		break;
	case ApiSystem::AUDIODEVICE:
		executables.push_back("batocera-audio");
		break;		
	case ApiSystem::BACKUP:
		executables.push_back("batocera-sync");
		break;
	case ApiSystem::INSTALL:
		executables.push_back("batocera-install");
		break;	
	case ApiSystem::SUPPORTFILE:
		executables.push_back("batocera-support");
		break;
	case ApiSystem::UPGRADE:
		executables.push_back("batocera-upgrade");
		break;
	case ApiSystem::SUSPEND:
		return (Utils::FileSystem::exists("/usr/sbin/pm-suspend") && Utils::FileSystem::exists("/usr/bin/pm-is-supported") && executeScript("/usr/bin/pm-is-supported --suspend"));
	case ApiSystem::VERSIONINFO:
		executables.push_back("batocera-version");
		break;
	case ApiSystem::READPLANEMODE:
	case ApiSystem::WRITEPLANEMODE:
		executables.push_back("batocera-planemode");
		break;
	case ApiSystem::SERVICES:
		executables.push_back("batocera-services");
		break;
	case ApiSystem::BACKGLASS:
		executables.push_back("batocera-backglass");
		break;
	}

	if (executables.size() == 0)
		return true;

	for (auto executable : executables)
		if (!Utils::FileSystem::exists("/usr/bin/" + executable))
			return false;

	return true;
}

bool ApiSystem::downloadFile(const std::string url, const std::string fileName, const std::string label, const std::function<void(const std::string)>& func)
{
	if (func != nullptr)
		func("Downloading " + label);

	HttpReq httpreq(url, fileName);
	while (httpreq.status() == HttpReq::REQ_IN_PROGRESS)
	{
		if (func != nullptr)
			func(std::string("Downloading " + label + " >>> " + std::to_string(httpreq.getPercent()) + " %"));

		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	if (httpreq.status() != HttpReq::REQ_SUCCESS)
		return false;

	return true;
}

void ApiSystem::setReadyFlag(bool ready)
{
	if (!ready)
	{
		Utils::FileSystem::removeFile("/tmp/emulationstation.ready");
		return;
	}

	FILE* fd = fopen("/tmp/emulationstation.ready", "w");
	if (fd != NULL) 
		fclose(fd);
}

bool ApiSystem::isReadyFlagSet()
{
	return Utils::FileSystem::exists("/tmp/emulationstation.ready");
}

std::vector<std::string> ApiSystem::getFormatDiskList()
{
#if WIN32 && _DEBUG
	std::vector<std::string> ret;
	ret.push_back("d:\\ DRIVE D:");
	ret.push_back("e:\\ DRIVE Z:");
	return ret;
#endif
	return executeEnumerationScript("batocera-format listDisks");
}

std::vector<std::string> ApiSystem::getFormatFileSystems()
{
#if WIN32 && _DEBUG
	std::vector<std::string> ret;
	ret.push_back("exfat");	
	ret.push_back("brfs");
	return ret;
#endif
	return executeEnumerationScript("batocera-format listFstypes");
}

int ApiSystem::formatDisk(const std::string disk, const std::string format, const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-format format " + disk + " " + format, func).second;
}

int ApiSystem::getPdfPageCount(const std::string& fileName)
{
	auto lines = executeEnumerationScript("pdfinfo \"" + fileName + "\"");
	for (auto line : lines)
	{
		auto splits = Utils::String::split(line, ':', true);
		if (splits.size() == 2 && splits[0] == "Pages")
			return atoi(Utils::String::trim(splits[1]).c_str());
	}

	return 0;
}

std::vector<std::string> ApiSystem::extractPdfImages(const std::string& fileName, int pageIndex, int pageCount, int quality)
{
	auto pdfFolder = Utils::FileSystem::getPdfTempPath();

	std::vector<std::string> ret;

	if (pageIndex < 0)
	{
		Utils::FileSystem::deleteDirectoryFiles(pdfFolder);

		int hardWareCoreCount = std::thread::hardware_concurrency();
		if (hardWareCoreCount > 1)
		{
			int lastTime = SDL_GetTicks();

			int numberOfPagesToProcess = 1;
			if (hardWareCoreCount < 8)
				numberOfPagesToProcess = 2;

			int pc = getPdfPageCount(fileName);
			if (pc > 0)
			{
				Utils::ThreadPool pool(1);

				for (int i = 0; i < pc; i += numberOfPagesToProcess)
					pool.queueWorkItem([this, fileName, i, numberOfPagesToProcess] { extractPdfImages(fileName, i + 1, numberOfPagesToProcess); });

				pool.wait();

				int time = SDL_GetTicks() - lastTime;
				std::string timeText = std::to_string(time) + "ms";

				for (auto file : Utils::FileSystem::getDirContent(pdfFolder, false))
				{
					auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(file));
					if (ext != ".jpg" && ext != ".png" && ext != ".ppm")
						continue;

					ret.push_back(file);
				}

				std::sort(ret.begin(), ret.end());
			}

			return ret;
		}
	}

	int lastTime = SDL_GetTicks();

	std::string page;

	std::string squality = Renderer::isSmallScreen() ? "96" : "125";
	if (quality > 0)
		squality = std::to_string(quality); // "300";

	std::string prefix = "extract";
	if (pageIndex >= 0)
	{
		char buffer[12];
		sprintf(buffer, "%08d", (uint32_t)pageIndex);
		
		if (pageIndex < 0)
			prefix = "page-" + squality + "-" + std::string(buffer) + "-pdf"; // page
		else
			prefix = Utils::FileSystem::getFileName(fileName) + "-" + squality + "-" + std::string(buffer) + "-pdf"; // page

		page = " -f " + std::to_string(pageIndex) + " -l " + std::to_string(pageIndex + pageCount - 1);
	}

#if WIN32
	executeEnumerationScript("pdftoppm -r "+ squality + page +" \"" + fileName + "\" \""+ pdfFolder +"/" + prefix +"\"");
#else
	executeEnumerationScript("pdftoppm -jpeg -r "+ squality +" -cropbox" + page + " \"" + fileName + "\" \"" + pdfFolder + "/" + prefix + "\"");
#endif

	int time = SDL_GetTicks() - lastTime;
	std::string text = std::to_string(time);
	
	for (auto file : Utils::FileSystem::getDirContent(pdfFolder, false))
	{
		auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(file));
		if (ext != ".jpg" && ext != ".png" && ext != ".ppm")
			continue;

		if (pageIndex >= 0 && !Utils::String::startsWith(Utils::FileSystem::getFileName(file), prefix))
			continue;

		ret.push_back(file);
	}

	std::sort(ret.begin(), ret.end());
	return ret;
}


std::vector<PacmanPackage> ApiSystem::getBatoceraStorePackages()
{
	std::vector<PacmanPackage> packages;

	LOG(LogDebug) << "ApiSystem::getBatoceraStorePackages";

	auto res = executeEnumerationScript("batocera-store list");
	std::string data = Utils::String::join(res, "\n");
	if (data.empty())
	{
		LOG(LogError) << "Package list is empty";
		return packages;
	}

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_string(data.c_str());
	if (!result)
	{
		LOG(LogError) << "Unable to parse packages";
		return packages;
	}

	pugi::xml_node root = doc.child("packages");
	if (!root)
	{
		LOG(LogError) << "Could not find <packages> node";
		return packages;
	}

	for (pugi::xml_node pkgNode = root.child("package"); pkgNode; pkgNode = pkgNode.next_sibling("package"))
	{
		PacmanPackage package;

		for (pugi::xml_node node = pkgNode.first_child(); node; node = node.next_sibling())
		{
			std::string tag = node.name();
			if (tag == "name")
				package.name = node.text().get();
			if (tag == "repository")
				package.repository = node.text().get();
			if (tag == "available_version")
				package.available_version = node.text().get();
			if (tag == "description")
				package.description = node.text().get();
			if (tag == "group")
				package.group = node.text().get(); // groups.push_back(
			if (tag == "license")
				package.licenses.push_back(node.text().get());
			if (tag == "packager")
				package.packager = node.text().get();
			if (tag == "status")
				package.status = node.text().get();
			if (tag == "repository")
				package.repository = node.text().get();
			if (tag == "url")
				package.url = node.text().get();			
			if (tag == "arch")
				package.arch = node.text().get();
			if (tag == "download_size")
				package.download_size = node.text().as_llong();
			if (tag == "installed_size")
				package.installed_size = node.text().as_llong();
			if (tag == "preview_url")
				package.preview_url = node.text().get();
		}

		if (!package.name.empty())
			packages.push_back(package);		
	}

	return packages;
}

std::pair<std::string, int> ApiSystem::installBatoceraStorePackage(std::string name, const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-store install \"" + name + "\"", func);
}

std::pair<std::string, int> ApiSystem::uninstallBatoceraStorePackage(std::string name, const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-store remove \"" + name + "\"", func);
}

void ApiSystem::refreshBatoceraStorePackageList()
{
	executeScript("batocera-store refresh");
	executeScript("batocera-store clean-all");
}

void ApiSystem::callBatoceraPreGameListsHook()
{
	executeScript("batocera-preupdate-gamelists-hook");
}

void ApiSystem::updateBatoceraStorePackageList()
{
	executeScript("batocera-store update");
}

std::vector<std::string> ApiSystem::getShaderList(const std::string& systemName, const std::string& emulator, const std::string& core)
{
	Utils::FileSystem::FileSystemCacheActivator fsc;

	std::vector<std::string> ret;

	for (auto folder : { Paths::getUserShadersPath(), Paths::getShadersPath() })
	{
		for (auto file : Utils::FileSystem::getDirContent(folder, true))
		{
			if (Utils::FileSystem::getFileName(file) == "rendering-defaults.yml")
			{
				auto parent = Utils::FileSystem::getFileName(Utils::FileSystem::getParent(file));
				if (parent == "configs")
					continue;

				if (std::find(ret.cbegin(), ret.cend(), parent) == ret.cend())
					ret.push_back(parent);
			}
		}
	}

	std::sort(ret.begin(), ret.end());
	return ret;
}

std::vector<std::string> ApiSystem::getVideoFilterList(const std::string& systemName, const std::string& emulator, const std::string& core)
{
	Utils::FileSystem::FileSystemCacheActivator fsc;

	std::vector<std::string> ret;

	LOG(LogDebug) << "ApiSystem::getVideoFilterList";

	for (auto folder : { Paths::getUserVideoFilters(), Paths::getVideoFilters() })
	{
		for (auto file : Utils::FileSystem::getDirContent(folder, false))
		{
			auto videofilter = Utils::FileSystem::getFileName(file);
			if (videofilter.substr(videofilter.find_last_of('.') + 1) == "filt")
			{
				if (std::find(ret.cbegin(), ret.cend(), videofilter) == ret.cend())
					ret.push_back(videofilter.substr(0, videofilter.find_last_of('.')));
			}
		}
	}

	std::sort(ret.begin(), ret.end());
	return ret;
}

std::vector<std::string> ApiSystem::getRetroachievementsSoundsList()
{
	Utils::FileSystem::FileSystemCacheActivator fsc;

	std::vector<std::string> ret;

	LOG(LogDebug) << "ApiSystem::getRetroAchievementsSoundsList";

	for (auto folder : { Paths::getUserRetroachivementSounds(), Paths::getRetroachivementSounds() })
	{
		for (auto file : Utils::FileSystem::getDirContent(folder, false))
		{
			auto sound = Utils::FileSystem::getFileName(file);
			if (sound.substr(sound.find_last_of('.') + 1) == "ogg")
			{
				if (std::find(ret.cbegin(), ret.cend(), sound) == ret.cend())
				  ret.push_back(sound.substr(0, sound.find_last_of('.')));
			}
		}
	}

	std::sort(ret.begin(), ret.end());
	return ret;
}

std::vector<std::string> ApiSystem::getTimezones()
{
	std::vector<std::string> ret;

	LOG(LogDebug) << "ApiSystem::getTimezones";

	auto folder = Paths::getTimeZonesPath();
	if (Utils::FileSystem::isDirectory(folder))
	{
		for (auto continent : Utils::FileSystem::getDirContent(folder, false))
		{
			for (auto file : Utils::FileSystem::getDirContent(continent, false))
			{
				std::string short_continent = continent.substr(continent.find_last_of('/') + 1, -1);
				if (short_continent == "Africa" || short_continent == "America"
					|| short_continent == "Antarctica" || short_continent == "Asia"
					|| short_continent == "Atlantic" || short_continent == "Australia"
					|| short_continent == "Etc" || short_continent == "Europe"
					|| short_continent == "Indian" || short_continent == "Pacific")
				{
					auto tz = Utils::FileSystem::getFileName(file);
					if (std::find(ret.cbegin(), ret.cend(), tz) == ret.cend())
						  ret.push_back(short_continent + "/" + tz);
				}
			}
		}
	}

	std::sort(ret.begin(), ret.end());
	return ret;
}

std::string ApiSystem::getCurrentTimezone()
{
	LOG(LogInfo) << "ApiSystem::getCurrentTimezone";
	auto cmd = executeEnumerationScript("batocera-timezone get");
	std::string tz = Utils::String::join(cmd, "");
	remove_if(tz.begin(), tz.end(), isspace);
	if (tz.empty()) {
		cmd = executeEnumerationScript("batocera-timezone detect");
		tz = Utils::String::join(cmd, "");
	}
	return tz;
}

bool ApiSystem::setTimezone(std::string tz)
{
	if (tz.empty())
		return false;
	return executeScript("batocera-timezone set \"" + tz + "\"");
}

std::vector<PadInfo> ApiSystem::getPadsInfo()
{
	LOG(LogInfo) << "ApiSystem::getPadsInfo";

	std::vector<PadInfo> ret;

	auto res = executeEnumerationScript("batocera-padsinfo");
	std::string data = Utils::String::join(res, "\n");
	if (data.empty())
	{
		LOG(LogError) << "Package list is empty";
		return ret;
	}

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_string(data.c_str());
	if (!result)
	{
		LOG(LogError) << "Unable to parse packages";
		return ret;
	}

	pugi::xml_node root = doc.child("pads");
	if (!root)
	{
		LOG(LogError) << "Could not find <pads> node";
		return ret;
	}

	for (pugi::xml_node pad = root.child("pad"); pad; pad = pad.next_sibling("pad"))
	{
		PadInfo pi;

		if (pad.attribute("device"))
			pi.device = pad.attribute("device").as_string();

		if (pad.attribute("id"))
			pi.id = Utils::String::toInteger(pad.attribute("id").as_string());

		if (pad.attribute("name"))
			pi.name = pad.attribute("name").as_string();

		if (pad.attribute("battery"))
			pi.battery = Utils::String::toInteger(pad.attribute("battery").as_string());

		if (pad.attribute("status"))
			pi.status = pad.attribute("status").as_string();

		if (pad.attribute("path"))
			pi.path = pad.attribute("path").as_string();

		ret.push_back(pi);
	}

	return ret;
}

std::string ApiSystem::getRunningArchitecture()
{
	auto res = executeEnumerationScript("uname -m");
	if (res.size() > 0)
		return res[0];

	return "";
}

std::string ApiSystem::getRunningBoard()
{
	auto res = executeEnumerationScript("cat /boot/boot/batocera.board");
	if (res.size() > 0)
		return res[0];

	return "";
}

std::string ApiSystem::getHostsName()
{
	auto hostName = SystemConf::getInstance()->get("system.hostname");
	if (!hostName.empty())
		return hostName;

	return "127.0.0.1";
}

bool ApiSystem::emuKill()
{
	LOG(LogDebug) << "ApiSystem::emuKill";
	return executeScript("batocera-es-swissknife --emukill");
}

void ApiSystem::suspend()
{
	LOG(LogDebug) << "ApiSystem::suspend";
	executeScript("/usr/sbin/pm-suspend");
}

void ApiSystem::replugControllers_sindenguns()
{
	LOG(LogDebug) << "ApiSystem::replugControllers_sindenguns";
	executeScript("/usr/bin/virtual-sindenlightgun-remap");
}

void ApiSystem::replugControllers_wiimotes()
{
	LOG(LogDebug) << "ApiSystem::replugControllers_wiimotes";
	executeScript("/usr/bin/virtual-wii-mouse-bar-remap");
}

void ApiSystem::replugControllers_steamdeckguns()
{
	LOG(LogDebug) << "ApiSystem::replugControllers_steamdeckguns";
	executeScript("/usr/bin/steamdeckgun-remap");
}

bool ApiSystem::isPlaneMode()
{
	auto res = executeEnumerationScript("batocera-planemode status");
	if (res.size() > 0)
		return res[0] == "on";

	return false;
}

bool ApiSystem::isReadPlaneModeSupported()
{
	return isScriptingSupported(READPLANEMODE);
}

bool ApiSystem::setPlaneMode(bool enable)
{
	LOG(LogDebug) << "ApiSystem::setPlaneMode";
	return executeScript("batocera-planemode " + std::string(enable ? "enable" : "disable"));
}

std::vector<Service> ApiSystem::getServices()
{
	std::vector<Service> services;

	LOG(LogDebug) << "ApiSystem::getServices";

	auto slines = executeEnumerationScript("batocera-services list");

	for (auto sline : slines) 
	{
		auto splits = Utils::String::split(sline, ';', true);
		if (splits.size() == 2) 
		{
			Service s;
			s.name = splits[0];
			s.enabled = (splits[1] == "*");
			services.push_back(s);
		}
	}
	return services;
}

std::vector<std::string> ApiSystem::backglassThemes() {
  std::vector<std::string> themes;

  LOG(LogDebug) << "ApiSystem::backglassThemes";

  auto slines = executeEnumerationScript("batocera-backglass list-themes");

  for (auto sline : slines) 
    {
      themes.push_back(sline);
    }
  return themes;
}

void ApiSystem::restartBackglass() {
  LOG(LogDebug) << "ApiSystem::restartBackglass";
  executeScript("/usr/bin/batocera-backglass restart");
}

bool ApiSystem::enableService(std::string name, bool enable) 
{
	std::string serviceName = name;
	if (serviceName.find(" ") != std::string::npos)
		serviceName = "\"" + serviceName + "\"";

	LOG(LogDebug) << "ApiSystem::enableService " << serviceName;

	bool res = executeScript("batocera-services " + std::string(enable ? "enable" : "disable") + " " + serviceName);
	if (res)
		res = executeScript("batocera-services " + std::string(enable ? "start" : "stop") + " " + serviceName);
	
	return res;
}
