#include "ApiKeys.h"
#include "system/SystemServices.h"
#include <atomic>
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
#include <SDL3/SDL.h>
#include <pugixml/src/pugixml.hpp>
#include <rapidjson/rapidjson.h>
#include <rapidjson/pointer.h>

#include <sys/statvfs.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>

/*
#define script_config "batocera-config"; // canupdate, overscan enable, overscan disable, storage 'X', storage current, storage list, forgetBT, getRootPassword, lsoutputs
#define script_overclock "batocera-overclock"; // list, set X
#define script_upgrade "batocera-upgrade";
#define script_sync "batocera-sync"; // sync
#define script_install "batocera-install"; // listDisks, listArchs, install X Y
#define script_scraper "batocera-scraper";
#define script_kodi "batocera-kodi";
#define script_resolution "batocera-resolution"; // listModes
#define script_sync "batocera-sync";   // list
#define script_info "batocera-info"; // --full
#define script_systems "batocera-systems"; // --filter
#define script_suport "batocera-support";
#define script_gameforce "batocera-gameforce"; // buttonColorLed X, powerLed X
#define script_format "batocera-format"; // listDisks, listFstypes
#define script_store "batocera-store"; // list, update, refresh, clean-all, install "X", remove "X"
#define script_preupdategamelists "batocera-preupdate-gamelists-hook";
#define script_padsinfos "batocera-padsinfo";
#define script_swissknife "batocera-es-swissknife"; // --emukill"
*/

ApiSystem::ApiSystem() { }

ApiSystem* ApiSystem::instance = nullptr;
ApiSystem::LED_TYPE ApiSystem::mSystemLedType = ApiSystem::LED_TYPE_NONE;

ApiSystem *ApiSystem::getInstance() 
{
	if (ApiSystem::instance == nullptr)
	{
		ApiSystem::instance = new ApiSystem();
		
		IExternalActivity::Instance = ApiSystem::instance;
	}

	return ApiSystem::instance;
}

unsigned long ApiSystem::getFreeSpaceGB(std::string mountpoint) 
{
	LOG(LogDebug) << "ApiSystem::getFreeSpaceGB";

	int free = 0;

	struct statvfs fiData;
	if ((statvfs(mountpoint.c_str(), &fiData)) >= 0)
		free = (fiData.f_bfree * fiData.f_bsize) / (1024 * 1024 * 1024);

	return free;
}

std::string ApiSystem::getFreeSpaceUserInfo()
{
	return getFreeSpaceInfo(Paths::getRootPath());
}

std::string ApiSystem::getFreeSpaceSystemInfo()
{
	return getFreeSpaceInfo("/");
}

std::string ApiSystem::getFreeSpaceInfo(const std::string mountpoint)
{
	LOG(LogDebug) << "ApiSystem::getFreeSpaceInfo";

	std::ostringstream oss;

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

	return "EMULATIONSTATION";
}

// BusyComponent* ui
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
    // Google DNS
    if (!executeScript("ping -c 1 -W 2 -t 255 8.8.8.8"))
    {
        // Cloudflare DNS
        if (!executeScript("ping -c 1 -W 2 -t 255 1.1.1.1"))
        {
            // Quad9 DNS
            return executeScript("ping -c 1 -W 2 -t 255 9.9.9.9");
        }
    }

    return true;
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

	Utils::FileSystem::FileSystemCache::reset();

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

bool ApiSystem::enableWifi(std::string ssid, std::string key, std::string /*country*/)
{
	// The regulatory domain is the distribution's business (wireless-regdb, cfg80211)
	if (!SystemServices::Wifi::setEnabled(true))
		return false;

	return ssid.empty() || SystemServices::Wifi::connect(ssid, key);
}

bool ApiSystem::configureWifi()
{
	// NetworkManager keeps its own profiles: make sure the primary network is the one in use
	const std::string ssid = SystemConf::getInstance()->get("wifi.ssid");
	if (ssid.empty() || ssid == SystemServices::Wifi::connectedSsid())
		return true;

	return SystemServices::Wifi::connect(ssid, SystemConf::getInstance()->get("wifi.key"));
}

bool ApiSystem::connectWifi(const std::string& ssid)
{
	// The key saved with that network in the menu, if any
	std::string key;
	for (int i = 1; i <= 10; i++)
	{
		std::string slot = i == 1 ? "wifi" : "wifi" + std::to_string(i);
		if (SystemConf::getInstance()->get(slot + ".ssid") == ssid)
		{
			key = SystemConf::getInstance()->get(slot + ".key");
			break;
		}
	}

	return SystemServices::Wifi::connect(ssid, key);
}

bool ApiSystem::disableWifi()
{
	return SystemServices::Wifi::setEnabled(false);
}

std::string ApiSystem::getIpAddress()
{
	LOG(LogDebug) << "ApiSystem::getIpAddress";
	
	std::string result = Utils::Platform::queryIPAddress(); // platform.h
	if (result.empty())
		return "NOT CONNECTED";

	return result;
}

bool ApiSystem::enableBluetooth()
{
	return SystemServices::Bluetooth::setPowered(true);
}

bool ApiSystem::disableBluetooth()
{
	return SystemServices::Bluetooth::setPowered(false);
}

static std::atomic<bool> sStopBluetoothScan { false };

void ApiSystem::startBluetoothLiveDevices(const std::function<void(const std::string)>& func)
{
	sStopBluetoothScan = false;
	SystemServices::Bluetooth::scan([func](const std::string& xml) { if (func) func(xml); }, sStopBluetoothScan);
}

void ApiSystem::stopBluetoothLiveDevices()
{
	sStopBluetoothScan = true;
}

bool ApiSystem::pairBluetoothDevice(const std::string& deviceName)
{
	return SystemServices::Bluetooth::pair(deviceName);
}

bool ApiSystem::connectBluetoothDevice(const std::string& deviceName)
{
	return SystemServices::Bluetooth::connect(deviceName);
}

bool ApiSystem::disconnectBluetoothDevice(const std::string& deviceName)
{
	return SystemServices::Bluetooth::disconnect(deviceName);
}

bool ApiSystem::removeBluetoothDevice(const std::string& deviceName)
{
	return SystemServices::Bluetooth::remove(deviceName);
}

bool ApiSystem::scanNewBluetooth(const std::function<void(const std::string)>& func)
{
	return SystemServices::Bluetooth::pairInputDevices([func](const std::string& info) { if (func) func(info); });
}

std::vector<std::string> ApiSystem::getPairedBluetoothDeviceList()
{
	std::vector<std::string> result;
	for (const auto& device : SystemServices::Bluetooth::pairedDevices())
		result.push_back(SystemServices::Bluetooth::toXml(device));
	return result;
}

std::vector<std::string> ApiSystem::getVideoModes(const std::string output)
{
  if(output == "") {
    return executeEnumerationScript("batocera-resolution listModes");
  } else {
    return executeEnumerationScript("batocera-resolution --screen " + Utils::String::shellQuote(output) + " listModes");
  }
}


std::vector<std::string> ApiSystem::getCustomRunners() 
{
	return executeEnumerationScript("batocera-wine-runners");
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
		cmd += " --filter " + Utils::String::shellQuote(system);

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

bool ApiSystem::forgetBluetoothControllers()
{
	bool ok = true;
	for (const auto& device : SystemServices::Bluetooth::pairedDevices())
		ok &= SystemServices::Bluetooth::remove(device.address);
	return ok;
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
	return SystemServices::Audio::outputs();
}

std::string ApiSystem::getCurrentAudioOutputDevice()
{
	return SystemServices::Audio::currentOutput();
}

bool ApiSystem::setAudioOutputDevice(std::string selected)
{
	bool ok = SystemServices::Audio::setOutput(selected);
	Sound::get(":/checksound.ogg")->play();
	return ok;
}

std::vector<std::string> ApiSystem::getAvailableAudioOutputProfiles()
{
	return SystemServices::Audio::profiles();
}

std::string ApiSystem::getCurrentAudioOutputProfile()
{
	return SystemServices::Audio::currentProfile();
}

bool ApiSystem::setAudioOutputProfile(std::string selected)
{
	bool ok = SystemServices::Audio::setProfile(selected);
	Sound::get(":/checksound.ogg")->play();
	return ok;
}

std::string ApiSystem::getThemesUrl()
{
	auto systemsetting = SystemConf::getInstance()->get("global.themes.url");
	if (!systemsetting.empty())
		return systemsetting;

	// Published by the es-re-catalog repository, next to the emulator catalog; override with global.themes.url
	return "https://raw.githubusercontent.com/JyuHo/es-re-catalog/main/themes.json";
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

	std::vector<std::string> paths = { Paths::getUserThemesPath(), Paths::getThemesPath() };
	for (const auto& dir : Paths::getSystemDataPaths())
		paths.push_back(dir + "/themes");

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

			// A path relative to the list, or a full URL
			auto screenShot = jsonString(item, "screenshot");
			if (Utils::String::startsWith(screenShot, "https://") || Utils::String::startsWith(screenShot, "http://"))
				bt.image = screenShot;
			else if (!screenShot.empty())
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
			installFolder = Paths::getDataPath() + "/themes";

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
			installFolder = Paths::getDataPath() + "/themes";

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

	if (fromZipContents && ext == ".7z")
	{
		auto cmd = getSevenZipCommand() + " x -so " + Utils::String::shellQuote(fileName) + " | md5sum";
		auto ret = executeEnumerationScript(cmd);
		if (ret.size() == 1 && ret.cbegin()->length() >= 32)
			return ret.cbegin()->substr(0, 32);
	}

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
		auto cmd = getSevenZipCommand() + " l -slt " + Utils::String::shellQuote(fileName);
		auto lines = executeEnumerationScript(cmd);
		for (std::string all : lines)
		{
			size_t idx = all.find("CRC = ");
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

	std::string cmd = getSevenZipCommand() + " x " + Utils::String::shellQuote(Utils::FileSystem::getPreferredPath(fileName)) + " -y -o" + Utils::String::shellQuote(Utils::FileSystem::getPreferredPath(destFolder));
	bool ret = executeScript(cmd);
	LOG(LogDebug) << "unzipFile <<";
	return ret;
}

bool ApiSystem::getBrightness(std::vector<BrightnessDevice>& values)
{	

	auto files = Utils::FileSystem::getDirContent("/sys/class/backlight");

	// sort to have a chance to keep always the same values
	files.sort();

	for (auto file : files)
	  {				
	    std::string brightnessPath = file + "/brightness";
	    std::string maxBrightnessPath = file + "/max_brightness";

	    if (Utils::FileSystem::exists(brightnessPath) && Utils::FileSystem::exists(maxBrightnessPath))
	      {
		LOG(LogInfo) << "ApiSystem::getBrightness > brightness path resolved to " << file;
		
		BrightnessDevice b;
		b.path = brightnessPath;
		b.pathmax = maxBrightnessPath;

		int max = Utils::String::toInteger(Utils::FileSystem::readAllText(maxBrightnessPath));
		if (max != 0) {
		  int value = Utils::String::toInteger(Utils::FileSystem::readAllText(brightnessPath));
		  b.value = (uint32_t) ((value / (float)max * 100.0f) + 0.5f);
		  values.push_back(b);
		}
	      }
	  }
	return values.size() > 0;
}

void ApiSystem::setBrightness(BrightnessDevice bd)
{
	if (bd.value < 1)
		bd.value = 1;

	if (bd.value > 100)
		bd.value = 100;

	int max = Utils::String::toInteger(Utils::FileSystem::readAllText(bd.pathmax));
	if (max == 0)
	  return;

	float percent = (bd.value / 100.0f * (float)max) + 0.5f;
		
	std::string content = std::to_string((uint32_t) percent) + "\n";
	Utils::FileSystem::writeAllText(bd.path, content);
}

static std::string LED_COLOUR_NAME;
static std::string LED_BRIGHTNESS_VALUE;
static std::string LED_MAX_BRIGHTNESS_VALUE;

bool ApiSystem::getLED(int& red, int& green, int& blue)
{	

	if (mSystemLedType != LED_TYPE_NONE)
		return true;

	// Anbernic RG 55G1: the stick LEDs sit behind the joypad MCU driver, not the LED class
	if (Utils::FileSystem::exists("/sys/devices/platform/singleadc-joypad/led_set"))
	{
		LED_COLOUR_NAME = "rg55g1";
		mSystemLedType = LED_TYPE_UNIFIED;
		LOG(LogInfo) << "ApiSystem::getLED > Found Anbernic RG 55G1 joystick LEDs";
	}

	auto entries = Utils::FileSystem::getDirContent("/sys/class/leds");
	bool found_addressable = false;

	for (const auto& entry : entries)
	{
		if (mSystemLedType != LED_TYPE_NONE)
			break;

		if (entry.find("joystick-left") != std::string::npos)
		{
			LED_COLOUR_NAME = "rg_vita_pro";
			mSystemLedType = LED_TYPE_UNIFIED;
			LOG(LogInfo) << "ApiSystem::getLED > Found Anbernic RG Vita Pro LED at " << entry;
			break;
		}

		if (entry.find("rgb:kbd_backlight") != std::string::npos)
		{
			LED_COLOUR_NAME = "cubexx";
			mSystemLedType = LED_TYPE_UNIFIED;
			LOG(LogInfo) << "ApiSystem::getLED > Found Anbernic CubeXX LED at " << entry;
			break;
		}

		if (entry.find("go_s:rgb:joystick_rings") != std::string::npos)
		{
			LED_COLOUR_NAME = "legiongos";
			mSystemLedType = LED_TYPE_UNIFIED;
			LOG(LogInfo) << "ApiSystem::getLED > Found Lenovo Legion Go S LED at " << entry;
			break;
		}

		if (entry.find("go:rgb:joystick_rings") != std::string::npos)
		{
			LED_COLOUR_NAME = "legiongo";
			mSystemLedType = LED_TYPE_UNIFIED;
			LOG(LogInfo) << "ApiSystem::getLED > Found Lenovo Legion Go/Go 2 LED at " << entry;
			break;
		}

		if (entry.find("multicolor") != std::string::npos || 
			entry.find(":rgb:joystick_rings") != std::string::npos || 
			entry.find("rgb:l1") != std::string::npos)
		{
			std::string ledColourPath = entry + "/multi_intensity";				
			if (Utils::FileSystem::exists(ledColourPath))
			{
				LED_COLOUR_NAME = ledColourPath;
				mSystemLedType = LED_TYPE_UNIFIED;
				LOG(LogInfo) << "ApiSystem::getLED > Found UNIFIED LED at " << entry;
				break;
			}
		}

		// Fallback check for the Ayn Odin monochrome LEDs
		if (entry.find("left_joystick") != std::string::npos)
		{
			LED_COLOUR_NAME = "monochrome";
			mSystemLedType = LED_TYPE_UNIFIED;
			LOG(LogInfo) << "ApiSystem::getLED > Found MONOCHROME LED at " << entry;

			std::vector<std::string> mono_nodes = { "left_joystick", "right_joystick", "left_side", "right_side" };
			for (const auto& node : mono_nodes) {
				std::string trigger_path = "/sys/class/leds/" + node + "/trigger";
				if (Utils::FileSystem::exists(trigger_path)) {
					Utils::FileSystem::writeAllText(trigger_path, "none\n");
				}
			}
			break;
		}

		if (entry.find("l:b1") != std::string::npos)
		{
			found_addressable = true;
		}
	}

	if (mSystemLedType == LED_TYPE_NONE && found_addressable) {
		mSystemLedType = LED_TYPE_ADDRESSABLE;
		LOG(LogInfo) << "ApiSystem::getLED > Found ADDRESSABLE LED type";
	}

	if (mSystemLedType == LED_TYPE_NONE) {
		LED_COLOUR_NAME = "notfound";
		return false;
	}

	if (mSystemLedType == LED_TYPE_UNIFIED) 
	{
		if (LED_COLOUR_NAME == "monochrome") 
		{
			red = 255;
			green = 255;
			blue = 255;
			executeScript("batocera-led-handheld block_color_changes");
			return true;
		}
		else if (LED_COLOUR_NAME == "cubexx" || LED_COLOUR_NAME == "rg_vita_pro" || LED_COLOUR_NAME == "legiongos" || LED_COLOUR_NAME == "legiongo" || LED_COLOUR_NAME == "rg55g1") 
		{
			getLEDColours(red, green, blue);
			executeScript("batocera-led-handheld block_color_changes");
			return true;
		}
		else if (Utils::FileSystem::exists(LED_COLOUR_NAME)) 
		{
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

			executeScript("batocera-led-handheld block_color_changes"); // temporarily prevent changes from external daemon
			LOG(LogInfo) << "ApiSystem::getLED > LED colours are:" << red << " " << green << " " << blue;

			return true;
		}
	}
	else if (mSystemLedType == LED_TYPE_ADDRESSABLE) 
	{
		getLEDColours(red, green, blue);
		executeScript("batocera-led-handheld block_color_changes");
		return true;
	}

	return false;
}

void ApiSystem::getLEDColours(int& red, int& green, int& blue)
{
	std::string colourValue = SystemConf::getInstance()->get("led.colour");
	if (colourValue.empty())
		colourValue = "255 0 165";

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

// multi_intensity follows the channel order of multi_index, which is not always "red green blue"
// (the Retroid Pocket 5 lists "blue green red")
static std::string ledMultiIntensity(const std::string& ledPath, int red, int green, int blue)
{
	std::string content;
	for (const auto& channel : Utils::String::split(Utils::String::trim(Utils::FileSystem::readAllText(ledPath + "/multi_index")), ' ', true))
	{
		int value = channel == "red" ? red : channel == "green" ? green : channel == "blue" ? blue : 0;
		content += (content.empty() ? "" : " ") + std::to_string(value);
	}

	if (content.empty())
		content = std::to_string(red) + " " + std::to_string(green) + " " + std::to_string(blue);

	return content;
}

void ApiSystem::setLEDColours(int red, int green, int blue)
{

	if (mSystemLedType == LED_TYPE_NONE)
		return;

	// We allow this check to be bypassed ONLY if we are turning the LEDs OFF (red=0, green=0, blue=0).
	if (red != 0 || green != 0 || blue != 0)
	{
		std::string currentMode = SystemConf::getInstance()->get("led.mode");
		if (currentMode == "rainbow" || currentMode == "chroma" || currentMode == "pulse")
		{
			return; 
		}
	}

    // Ensure RGB values are within valid range
	if (red < 0) red = 0;
    if (red > 255) red = 255;
    if (green < 0) green = 0;
    if (green > 255) green = 255;
    if (blue < 0) blue = 0;
    if (blue > 255) blue = 255;

	if (mSystemLedType == LED_TYPE_UNIFIED)
	{
		if (LED_COLOUR_NAME.empty() || LED_COLOUR_NAME == "notfound") return;

		if (LED_COLOUR_NAME == "cubexx" || LED_COLOUR_NAME == "rg_vita_pro" || LED_COLOUR_NAME == "legiongos" || LED_COLOUR_NAME == "legiongo" || LED_COLOUR_NAME == "rg55g1") {
			executeScript("batocera-led-handheld set_color_force_dec " + 
                          std::to_string(red) + " " + 
                          std::to_string(green) + " " + 
                          std::to_string(blue));
		}
		// Handle monochrome power state mappings (1 = ON, 0 = OFF)
		else if (LED_COLOUR_NAME == "monochrome") {
			int brightnessValue = (red > 0 || green > 0 || blue > 0) ? 1 : 0;
			std::vector<std::string> mono_nodes = { "left_joystick", "right_joystick", "left_side", "right_side" };
			for (const auto& node : mono_nodes) {
				std::string path = "/sys/class/leds/" + node + "/brightness";
				if (Utils::FileSystem::exists(path)) {
					Utils::FileSystem::writeAllText(path, std::to_string(brightnessValue) + "\n");
				}
			}
		}
		else if (LED_COLOUR_NAME.find("rgb:l") != std::string::npos) {
			auto allLeds = Utils::FileSystem::getDirContent("/sys/class/leds");
			for (const auto& ledPath : allLeds) {
				if (ledPath.find("/rgb:l") != std::string::npos || 
					ledPath.find("/rgb:r") != std::string::npos) {
					std::string intensityPath = ledPath + "/multi_intensity";
					if (Utils::FileSystem::exists(intensityPath)) {
						Utils::FileSystem::writeAllText(intensityPath, ledMultiIntensity(ledPath, red, green, blue));
					}
				}
			}
		}
		else {
			std::string content = std::to_string(red) + " " + std::to_string(green) + " " + std::to_string(blue);
			Utils::FileSystem::writeAllText(LED_COLOUR_NAME, content);
		}
	}
	else if (mSystemLedType == LED_TYPE_ADDRESSABLE)
	{
		static std::vector<std::string> r_files, g_files, b_files;
		if (r_files.empty()) {
			auto all_files = Utils::FileSystem::getDirContent("/sys/class/leds");
			for(const auto& file : all_files) {
				if (file.find(":r") != std::string::npos) r_files.push_back(file + "/brightness");
				if (file.find(":g") != std::string::npos) g_files.push_back(file + "/brightness");
				if (file.find(":b") != std::string::npos) b_files.push_back(file + "/brightness");
			}
		}

		// the raw channels have no group brightness, so apply it to the colour
		std::string brightnessStr = SystemConf::getInstance()->get("led.brightness");
		int brightness = brightnessStr.empty() ? 100 : Utils::String::toInteger(brightnessStr);
		if (brightness < 0) brightness = 0;
		if (brightness > 100) brightness = 100;

		float factor = brightness / 100.0f;
		red = static_cast<int>(red * factor + 0.5f);
		green = static_cast<int>(green * factor + 0.5f);
		blue = static_cast<int>(blue * factor + 0.5f);

		for(const auto& file : r_files) Utils::FileSystem::writeAllText(file, std::to_string(red));
		for(const auto& file : g_files) Utils::FileSystem::writeAllText(file, std::to_string(green));
		for(const auto& file : b_files) Utils::FileSystem::writeAllText(file, std::to_string(blue));
	}
}

bool ApiSystem::getLEDBrightness(int& value)
{   

    // Handle software-controlled brightness platforms directly from configuration
    if (LED_COLOUR_NAME == "cubexx" || LED_COLOUR_NAME == "rg_vita_pro" || LED_COLOUR_NAME == "legiongos" || LED_COLOUR_NAME == "legiongo" || LED_COLOUR_NAME == "rg55g1")
    {
        mSystemLedType = LED_TYPE_UNIFIED;
        std::string valStr = SystemConf::getInstance()->get("led.brightness");
        if (valStr.empty()) {
            value = 100; // Default to 100% if not yet configured
        } else {
            value = Utils::String::toInteger(valStr);
        }
        return true;
    }

    if (LED_BRIGHTNESS_VALUE.empty() || LED_MAX_BRIGHTNESS_VALUE.empty())
    {
        auto directories = Utils::FileSystem::getDirContent("/sys/class/leds");

        for (const auto& directory : directories)
        {
            if (directory.find("multicolor") != std::string::npos || 
                directory.find(":rgb:joystick_rings") != std::string::npos ||
                directory.find("rgb:l1") != std::string::npos ||
                // raw l:r1 channels are busy when they belong to an rgb:l* group (Retroid Pocket 5)
                (directory.find("l:r1") != std::string::npos && !Utils::FileSystem::exists("/sys/class/leds/rgb:l1")) ||
                directory.find("left_joystick") != std::string::npos ||
                directory.find("rgb:kbd_backlight") != std::string::npos ||
                directory.find("joystick-left") != std::string::npos)
            {
                std::string ledBrightnessPath = directory + "/brightness";
                std::string ledMaxBrightnessPath = directory + "/max_brightness";

                if (Utils::FileSystem::exists(ledBrightnessPath) && Utils::FileSystem::exists(ledMaxBrightnessPath))
                {
                    LED_BRIGHTNESS_VALUE = ledBrightnessPath;
                    LED_MAX_BRIGHTNESS_VALUE = ledMaxBrightnessPath;

                    // FORCE ENABLE: a valid path means Unified mode, unless the LEDs are raw addressable channels
                    if (mSystemLedType != LED_TYPE_ADDRESSABLE)
                        mSystemLedType = LED_TYPE_UNIFIED;

                    LOG(LogInfo) << "ApiSystem::getLEDBrightness > LED brightness path resolved to " << directory;
                    break;
                }
            }
        }
    }

    if (mSystemLedType != LED_TYPE_UNIFIED && mSystemLedType != LED_TYPE_ADDRESSABLE)
        return false;

    if (LED_BRIGHTNESS_VALUE == "notfound" || LED_BRIGHTNESS_VALUE.empty())
        return false;

    // a raw channel holds one colour component, not the brightness
    if (mSystemLedType == LED_TYPE_ADDRESSABLE)
    {
        std::string valStr = SystemConf::getInstance()->get("led.brightness");
        value = valStr.empty() ? 100 : Utils::String::toInteger(valStr);
        return true;
    }

    value = 0;

    int max = Utils::String::toInteger(Utils::FileSystem::readAllText(LED_MAX_BRIGHTNESS_VALUE));
    if (max <= 0) return false;

    if (Utils::FileSystem::exists(LED_BRIGHTNESS_VALUE))
        value = Utils::String::toInteger(Utils::FileSystem::readAllText(LED_BRIGHTNESS_VALUE));

    // Convert raw hardware value (0-255) to percentage (0-100)
    value = (uint32_t)((value / (float)max * 100.0f) + 0.5f);
    return true;
}

void ApiSystem::setLEDBrightness(int value) 
{
    // If we haven't resolved the path yet, try to do it now
    int dummy;
    if (LED_BRIGHTNESS_VALUE.empty()) getLEDBrightness(dummy);

    if ((mSystemLedType != LED_TYPE_UNIFIED && mSystemLedType != LED_TYPE_ADDRESSABLE) || LED_BRIGHTNESS_VALUE == "notfound")
        return;

    if (value < 0) value = 0;
    if (value > 100) value = 100;

	if (LED_COLOUR_NAME == "rg55g1") {
		SystemConf::getInstance()->set("led.brightness", std::to_string(value));
		SystemConf::getInstance()->saveSystemConf();
		executeScript("batocera-led-handheld set_brightness " + std::to_string(value));
		return;
	}

	if (LED_COLOUR_NAME == "cubexx" || LED_COLOUR_NAME == "rg_vita_pro" || LED_COLOUR_NAME == "legiongos" || LED_COLOUR_NAME == "legiongo") {
		SystemConf::getInstance()->set("led.brightness", std::to_string(value));
		SystemConf::getInstance()->saveSystemConf();
		int r, g, b;
		getLEDColours(r, g, b);
		setLEDColours(r, g, b);
		return;
	}

    std::string colorStr = SystemConf::getInstance()->get("led.colour");
    if (colorStr.empty()) colorStr = "255 255 255"; // Default to white if not set

    int rBase = 255, gBase = 255, bBase = 255;
    
    // Simple parser for "R G B" or "RRGGBB" formats
    if (colorStr.find(" ") != std::string::npos) {
        std::vector<std::string> parts = Utils::String::split(colorStr, ' ');
        if (parts.size() >= 3) {
            rBase = Utils::String::toInteger(parts[0]);
            gBase = Utils::String::toInteger(parts[1]);
            bBase = Utils::String::toInteger(parts[2]);
        }
    } else if (colorStr.length() == 6) {
        rBase = std::stoul(colorStr.substr(0, 2), nullptr, 16);
        gBase = std::stoul(colorStr.substr(2, 2), nullptr, 16);
        bBase = std::stoul(colorStr.substr(4, 2), nullptr, 16);
    }

    float factor = static_cast<float>(value) / 100.0f;
    int rOut = static_cast<int>(rBase * factor + 0.5f);
    int gOut = static_cast<int>(gBase * factor + 0.5f);
    int bOut = static_cast<int>(bBase * factor + 0.5f);

    if (LED_BRIGHTNESS_VALUE.find("rgb:l") != std::string::npos)
    {
        int max = Utils::String::toInteger(Utils::FileSystem::readAllText(LED_MAX_BRIGHTNESS_VALUE));
        int brightnessValue = static_cast<int>(factor * max + 0.5f);
        for (const auto& ledPath : Utils::FileSystem::getDirContent("/sys/class/leds")) {
            if (ledPath.find("/rgb:l") == std::string::npos && ledPath.find("/rgb:r") == std::string::npos)
                continue;

            std::string brightPath = ledPath + "/brightness";
            if (Utils::FileSystem::exists(brightPath))
                Utils::FileSystem::writeAllText(brightPath, std::to_string(brightnessValue) + "\n");
        }
    }
    // Check if we are on an addressable device (Retroid/Ayn style)
    // These paths typically look like /sys/class/leds/l:r1
    else if (LED_BRIGHTNESS_VALUE.find("/l:") != std::string::npos || 
        LED_BRIGHTNESS_VALUE.find("/r:") != std::string::npos) 
    {
        // Batch update all addressable LED channels
        auto directories = Utils::FileSystem::getDirContent("/sys/class/leds");
        for (const auto& directory : directories)
        {
            // Identify if this is an addressable LED folder
            if (directory.find("/l:") != std::string::npos || directory.find("/r:") != std::string::npos)
            {
                std::string path = directory + "/brightness";
                if (!Utils::FileSystem::exists(path)) continue;

                // Check the channel suffix to apply the correct scaled color
                if (directory.find(":r") != std::string::npos)
                    Utils::FileSystem::writeAllText(path, std::to_string(rOut) + "\n");
                else if (directory.find(":g") != std::string::npos)
                    Utils::FileSystem::writeAllText(path, std::to_string(gOut) + "\n");
                else if (directory.find(":b") != std::string::npos)
                    Utils::FileSystem::writeAllText(path, std::to_string(bOut) + "\n");
            }
        }
    }
    else 
    {
        // Fallback for standard devices (multicolor/joystick_rings handles scaling internally)
        int max = Utils::String::toInteger(Utils::FileSystem::readAllText(LED_MAX_BRIGHTNESS_VALUE));
        int brightnessValue = static_cast<int>(factor * max + 0.5f);
        Utils::FileSystem::writeAllText(LED_BRIGHTNESS_VALUE, std::to_string(brightnessValue) + "\n");
    }
}

bool ApiSystem::isLEDEnabled()
{
	// Check batocera.conf for "led.enabled" setting, default to "1" (true) if not found
	return SystemConf::getInstance()->get("led.enabled") != "0";
}

void ApiSystem::setLEDMode(const std::string& mode)
{
    SystemConf::getInstance()->set("led.mode", mode);
    SystemConf::getInstance()->saveSystemConf();

    executeScript("batocera-led-handheld unblock_color_changes");

    // the effects run for seconds, don't hold the UI while they play
    if (mode == "rainbow") {
        executeScript("batocera-led-handheld rainbow &");
    } else if (mode == "chroma") {
        executeScript("batocera-led-handheld chroma &");
    } else if (mode == "pulse") {
        executeScript("batocera-led-handheld pulse &");
    } else {
        // Fall back to the user's customized static colors
        int r, g, b;
        getLEDColours(r, g, b);
        setLEDColours(r, g, b);
    }
}

void ApiSystem::setLEDEnabled(bool enabled)
{
	SystemConf::getInstance()->set("led.enabled", enabled ? "1" : "0");

	if (!enabled)
	{
		setLEDColours(0, 0, 0);
	}
	else
	{
		std::string mode = SystemConf::getInstance()->get("led.mode");
		if (mode == "rainbow" || mode == "chroma" || mode == "pulse") {
			setLEDMode(mode);
		} else {
			std::string lastColorStr = SystemConf::getInstance()->get("led.colour");
			if (lastColorStr.empty())
				lastColorStr = "255 0 165";

			std::stringstream ss(lastColorStr);
			int r, g, b;
			ss >> r >> g >> b;

			setLEDColours(r, g, b);
		}
	}

	SystemConf::getInstance()->saveSystemConf();
}

bool ApiSystem::isLEDMonochrome()
{
    return (LED_COLOUR_NAME == "monochrome");
}

std::vector<std::string> ApiSystem::getWifiNetworks(bool scan)
{
	return SystemServices::Wifi::networks(scan);
}

std::string ApiSystem::getConnectedWifiSsid()
{
	return SystemServices::Wifi::connectedSsid();
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
		return SystemServices::Wifi::isAvailable();
	case ApiSystem::BLUETOOTH:
		return SystemServices::Bluetooth::isAvailable();
	case ApiSystem::RESOLUTION:
		executables.push_back("batocera-resolution");
		break;
	case ApiSystem::BIOSINFORMATION:
		executables.push_back("batocera-systems");
		break;
	case ApiSystem::NETPLAY:
		executables.push_back("7zr");
		break;
	case ApiSystem::PDFEXTRACTION:
		executables.push_back("pdftoppm");
		executables.push_back("pdfinfo");
		break;
	case ApiSystem::PADSINFO:
		executables.push_back("batocera-padsinfo");
		break;
	case ApiSystem::EVMAPY:
		executables.push_back("evmapy");
		break;
	case ApiSystem::TIMEZONES:
		return SystemServices::TimeZone::isAvailable();
	case ApiSystem::AUDIODEVICE:
		return SystemServices::Audio::isAvailable();
	case ApiSystem::SUSPEND:
		return (Utils::FileSystem::exists("/usr/sbin/pm-suspend") && Utils::FileSystem::exists("/usr/bin/pm-is-supported") && executeScript("/usr/bin/pm-is-supported --suspend"));
	case ApiSystem::VERSIONINFO:
		executables.push_back("batocera-version");
		break;
	case ApiSystem::READPLANEMODE:
		executables.push_back("batocera-planemode");
		break;
	case ApiSystem::NFC:
		executables.push_back("batocera-nfc");
		break;
	default:
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
		Utils::FileSystem::removeFile(Paths::getRuntimePath() + "/emulationstation.ready");
		return;
	}

	FILE* fd = fopen((Paths::getRuntimePath() + "/emulationstation.ready").c_str(), "w");
	if (fd != NULL) 
		fclose(fd);
}

bool ApiSystem::isReadyFlagSet()
{
	return Utils::FileSystem::exists(Paths::getRuntimePath() + "/emulationstation.ready");
}

int ApiSystem::formatDisk(const std::string disk, const std::string format, const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-format format " + Utils::String::shellQuote(disk) + " " + Utils::String::shellQuote(format), func).second;
}

int ApiSystem::getPdfPageCount(const std::string& fileName)
{
	auto lines = executeEnumerationScript("pdfinfo " + Utils::String::shellQuote(fileName));
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
				Utils::ThreadPool pool("extractPdfImages", 1);

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

	executeEnumerationScript("pdftoppm -jpeg -r "+ squality +" -cropbox" + page + " " + Utils::String::shellQuote(fileName) + " " + Utils::String::shellQuote(pdfFolder + "/" + prefix));

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


std::pair<std::string, int> ApiSystem::installBatoceraStorePackage(std::string name, const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-store install " + Utils::String::shellQuote(name), func);
}

std::pair<std::string, int> ApiSystem::uninstallBatoceraStorePackage(std::string name, const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-store remove " + Utils::String::shellQuote(name), func);
}

// RetroArch shader presets, as paths relative to their folder without the extension ("crt/crt-royale")
std::vector<std::string> ApiSystem::getShaderList(const std::string& systemName, const std::string& emulator, const std::string& core)
{
	std::vector<std::string> ret;

	for (auto folder : { Paths::getUserShadersPath(), Paths::getShadersPath() })
	{
		if (folder.empty())
			continue;

		for (auto file : Utils::FileSystem::getDirContent(folder, true))
		{
			auto extension = Utils::FileSystem::getExtension(file);
			if (extension != ".slangp" && extension != ".glslp")
				continue;

			auto name = Utils::FileSystem::createRelativePath(file, folder, false);
			if (Utils::String::startsWith(name, "./"))
				name = name.substr(2);
			name = name.substr(0, name.size() - extension.size());

			if (std::find(ret.cbegin(), ret.cend(), name) == ret.cend())
				ret.push_back(name);
		}
	}

	std::sort(ret.begin(), ret.end());
	return ret;
}

std::vector<std::string> ApiSystem::getVideoFilterList(const std::string& systemName, const std::string& emulator, const std::string& core)
{
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
	return SystemServices::TimeZone::list();
}

std::string ApiSystem::getCurrentTimezone()
{
	return SystemServices::TimeZone::current();
}

bool ApiSystem::setTimezone(std::string tz)
{
	return SystemServices::TimeZone::set(tz);
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
	executeScript("/usr/bin/batocera-shutdown gui");
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

std::vector<Hotkey> ApiSystem::getJoysticksHotkeys() {
  std::vector<Hotkey> hotkeys;

  LOG(LogDebug) << "ApiSystem::getJoysticksHotkeys";

  auto res = executeEnumerationScript("batocera-joysticks-hotkeys");

  std::string data = Utils::String::join(res, "\n");
  if (data.empty())
    {
      LOG(LogError) << "List is empty";
      return hotkeys;
    }

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(data.c_str());
  if (!result)
    {
      LOG(LogError) << "Unable to parse hotkeys";
      return hotkeys;
    }

  pugi::xml_node root = doc.child("hotkeys");
  if (!root)
    {
      LOG(LogError) << "Could not find <hotkeys> node";
      return hotkeys;
    }

  for (pugi::xml_node hotkey = root.child("hotkey"); hotkey; hotkey = hotkey.next_sibling("hotkey"))
    {
      Hotkey hk;

      if (hotkey.attribute("button"))
	hk.button = hotkey.attribute("button").as_string();
      
      if (hotkey.attribute("action"))
	hk.action = hotkey.attribute("action").as_string();

      if (hotkey.attribute("default"))
	hk.default_action = hotkey.attribute("default").as_string();

      hotkeys.push_back(hk);
    }
  return hotkeys;
}

std::vector<std::string> ApiSystem::getJoysticksHotkeysValues() {
  return executeEnumerationScript("batocera-joysticks-hotkeys --values");
}

std::vector<std::string> ApiSystem::getGlobalHotkeysValues() {
  std::vector<std::string> hotkeys;

  LOG(LogDebug) << "ApiSystem::getGlobalHotkeysValues";

  auto res = executeEnumerationScript("batocera-hotkeys --values");

  std::string data = Utils::String::join(res, "\n");
  if (data.empty())
    {
      LOG(LogError) << "List is empty";
      return hotkeys;
    }

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(data.c_str());
  if (!result)
    {
      LOG(LogError) << "Unable to parse hotkeys values";
      return hotkeys;
    }

  pugi::xml_node root = doc.child("mapping");
  if (!root)
    {
      LOG(LogError) << "Could not find <mapping> node";
      return hotkeys;
    }

  for (pugi::xml_node key = root.child("key"); key; key = key.next_sibling("key"))
    {

      if (key.attribute("name")) {
	std::string key_name = key.attribute("name").as_string();
	hotkeys.push_back(key_name);
      }
    }

  return hotkeys;
}

void ApiSystem::setJoysticksHotkeys(const std::vector<Hotkey>& hotkeys) {
  LOG(LogDebug) << "ApiSystem::setJoysticksHotkeys";

  std::string params;
  for(unsigned int h = 0; h < hotkeys.size(); h++) {
    params = params + " --" + hotkeys[h].button + " " + hotkeys[h].action;
  }
  executeScript("batocera-joysticks-hotkeys " + params);
}

std::vector<GlobalHotkey> ApiSystem::detectGlobalHotkeys() {
  std::vector<GlobalHotkey> hotkeys;

  LOG(LogDebug) << "ApiSystem::detectGlobalHotkey";

  auto res = executeEnumerationScript("batocera-hotkeys --detect");

  std::string data = Utils::String::join(res, "\n");
  if (data.empty())
    {
      LOG(LogError) << "List is empty";
      return hotkeys;
    }

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(data.c_str());
  if (!result)
    {
      LOG(LogError) << "Unable to parse hotkeys";
      return hotkeys;
    }

  pugi::xml_node root = doc.child("keys");
  if (!root)
    {
      LOG(LogError) << "Could not find <keys> node";
      return hotkeys;
    }

  for (pugi::xml_node key = root.child("key"); key; key = key.next_sibling("key"))
    {
      GlobalHotkey hk;

      if (key.attribute("key"))
	hk.key = key.attribute("key").as_string();
      
      if (key.attribute("config"))
	hk.device_config = key.attribute("config").as_string();

      hotkeys.push_back(hk);
    }
  return hotkeys;
}

std::vector<GlobalHotkey> ApiSystem::getGlobalHotkeys() {
  std::vector<GlobalHotkey> hotkeys;

  LOG(LogDebug) << "ApiSystem::getGlobalHotkeys";

  auto res = executeEnumerationScript("batocera-hotkeys");

  std::string data = Utils::String::join(res, "\n");
  if (data.empty())
    {
      LOG(LogError) << "List is empty";
      return hotkeys;
    }

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(data.c_str());
  if (!result)
    {
      LOG(LogError) << "Unable to parse hotkeys";
      return hotkeys;
    }

  pugi::xml_node root = doc.child("hotkeys");
  if (!root)
    {
      LOG(LogError) << "Could not find <hotkeys> node";
      return hotkeys;
    }

  for (pugi::xml_node device = root.child("device"); device; device = device.next_sibling("device"))
    {

      for (pugi::xml_node hotkey = device.child("hotkey"); hotkey; hotkey = hotkey.next_sibling("hotkey"))
	{
      
	  GlobalHotkey hk;

	  if (device.attribute("fancy_name"))
	  	hk.device_fancy_name = device.attribute("fancy_name").as_string();

	  if (device.attribute("config"))
	  	hk.device_config = device.attribute("config").as_string();

	  if (hotkey.attribute("key"))
	  	hk.key = hotkey.attribute("key").as_string();

	  if (hotkey.attribute("action"))
	  	hk.action = hotkey.attribute("action").as_string();

	  hotkeys.push_back(hk);
	}
    }

  return hotkeys;
}

std::vector<Keyboardtopad> ApiSystem::getKeyboardtopads() {
  std::vector<Keyboardtopad> keyboardtopads;

  LOG(LogDebug) << "ApiSystem::getKeyboardtopads";

  auto res = executeEnumerationScript("keyboardToPads --search");

  std::string data = Utils::String::join(res, "\n");
  if (data.empty())
    {
      LOG(LogError) << "List is empty";
      return keyboardtopads;
    }

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(data.c_str());
  if (!result)
    {
      LOG(LogError) << "Unable to parse keyboardtopads";
      return keyboardtopads;
    }

  pugi::xml_node root = doc.child("keyboardtopads");
  if (!root)
    {
      LOG(LogError) << "Could not find <keyboardtopads> node";
      return keyboardtopads;
    }

  for (pugi::xml_node device = root.child("device"); device; device = device.next_sibling("device"))
    {
      Keyboardtopad ktp;

      if (device.attribute("name"))
	ktp.name = device.attribute("name").as_string();

      if (device.attribute("config"))
	ktp.config = device.attribute("config").as_string();

      if (device.attribute("device"))
	ktp.device_path = device.attribute("device").as_string();

      keyboardtopads.push_back(ktp);
    }

  return keyboardtopads;
}

std::vector<KeyboardtopadDevice> ApiSystem::getKeyboardtopadDevices(std::string config) {
  std::vector<KeyboardtopadDevice> devices;

  LOG(LogDebug) << "ApiSystem::getKeyboardtopadDevices";

  auto res = executeEnumerationScript("keyboardToPads --config \""+config+"\" --get-config");

  std::string data = Utils::String::join(res, "\n");
  if (data.empty())
    {
      LOG(LogError) << "List is empty";
      return devices;
    }

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(data.c_str());
  if (!result)
    {
      LOG(LogError) << "Unable to parse getKeyboardtopadDevices";
      return devices;
    }

  pugi::xml_node root = doc.child("keyboardtopad");
  if (!root)
    {
      LOG(LogError) << "Could not find <keyboardtopad> node";
      return devices;
    }

  for (pugi::xml_node device = root.child("device"); device; device = device.next_sibling("device"))
    {
      KeyboardtopadDevice dev;

      if (device.attribute("name"))
	dev.name = device.attribute("name").as_string();

      if (device.attribute("type"))
	dev.type = device.attribute("type").as_string();

      for (pugi::xml_node key = device.child("key"); key; key = key.next_sibling("key"))
	{
	  KeyboardtopadKey k;

	  if (key.attribute("name"))
	    k.name = key.attribute("name").as_string();

	  if (key.attribute("value"))
	    k.value = key.attribute("value").as_string();

	  dev.keys.push_back(k);
	}

      devices.push_back(dev);
    }

  return devices;
}

void ApiSystem::setGlobalHotkey(const std::string& config, const std::string& key, const std::string& action) {
  LOG(LogDebug) << "ApiSystem::setGlobalHotkey";
  executeScript("batocera-hotkeys --set --config " + config + " --key " + key + " --action " + action);
}

void ApiSystem::removeGlobalHotkey(const std::string& config, const std::string& key) {
  LOG(LogDebug) << "ApiSystem::removeGlobalHotkey";
  executeScript("batocera-hotkeys --remove --config " + config + " --key " + key);
}

std::vector<KeyboardtopadKey> ApiSystem::getKeyboardtopadKeyValues() {
  std::vector<KeyboardtopadKey> hotkeys;

  LOG(LogDebug) << "ApiSystem::getKeyboardtopadValues";

  auto res = executeEnumerationScript("batocera-hotkeys --values");

  std::string data = Utils::String::join(res, "\n");
  if (data.empty())
    {
      LOG(LogError) << "List is empty";
      return hotkeys;
    }

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(data.c_str());
  if (!result)
    {
      LOG(LogError) << "Unable to parse hotkeys values";
      return hotkeys;
    }

  pugi::xml_node root = doc.child("mapping");
  if (!root)
    {
      LOG(LogError) << "Could not find <mapping> node";
      return hotkeys;
    }

  for (pugi::xml_node key = root.child("key"); key; key = key.next_sibling("key"))
    {
      KeyboardtopadKey vkey;

      if (key.attribute("name")) {
	vkey.name = key.attribute("name").as_string();
      }

      if (key.attribute("evcode")) {
	vkey.value = key.attribute("evcode").as_string();
      }

      hotkeys.push_back(vkey);
    }

  return hotkeys;
}

void ApiSystem::saveKeyboardtopads(Keyboardtopad ktp, const std::vector<KeyboardtopadDevice>& ktp_devices) {
  LOG(LogDebug) << "ApiSystem::saveKeyboardtopads";

  std::string cmd = "keyboardToPads --set --config \"" + ktp.config + "\"";
  std::string cmd_dev;

  int njoys = 0;
  int nhotkeys = 0;
  bool stopjoy = false;

  for(int d=0; d<(int)ktp_devices.size(); d++) {
    bool enabled = (ktp_devices[d].name != "" || ktp_devices[d].type != "joystick") && ktp_devices[d].keys.size() > 0;

    // don't continue on joysticks if one is disabled
    if(ktp_devices[d].type == "joystick" && enabled == false && !stopjoy) {
      stopjoy = true;
      cmd_dev  = " --device-type "   + ktp_devices[d].type;
      cmd_dev += " --device-keep " + std::to_string(d);
      //printf("cmd: %s\n", (cmd + cmd_dev).c_str());
      executeScript(cmd + cmd_dev);
    }
    if(ktp_devices[d].type == "joystick" && enabled && stopjoy) {
      enabled = false;
    }

    if(enabled) {
      int ndev = 0;
      if(ktp_devices[d].type == "joystick") ndev = njoys;
      if(ktp_devices[d].type == "hotkeys")  ndev = nhotkeys;

      cmd_dev = "";
      cmd_dev += " --device-number " +  std::to_string(ndev);

      if(ktp_devices[d].type == "hotkeys" && ktp_devices[d].name == "") {
	// force a name for hotkeys
	cmd_dev += " --device-name \"ktp hotkeys\"";
      } else {
	cmd_dev += " --device-name \"" + ktp_devices[d].name + "\"";
      }
      cmd_dev += " --device-type "   + ktp_devices[d].type;

      cmd_dev += " --set-values \"";
      for(unsigned int k=0; k<ktp_devices[d].keys.size(); k++) {
	if(k != 0) cmd_dev += ",";
	cmd_dev += ktp_devices[d].keys[k].value + "=" + ktp_devices[d].keys[k].name;
      }
      cmd_dev += "\"";

      //printf("cmd: %s\n", (cmd + cmd_dev).c_str());
      executeScript(cmd + cmd_dev);
    }
    if(ktp_devices[d].type == "joystick") njoys++;
    if(ktp_devices[d].type == "hotkeys")  nhotkeys++;
  }
}

std::string ApiSystem::detectEvKey(const std::string& device_path) {
  std::string vkey = "";

  LOG(LogDebug) << "ApiSystem::detectEvKey";

  auto res = executeEnumerationScript("batocera-hotkeys --detect --count 1 --nowait --evformat --device " + device_path);

  std::string data = Utils::String::join(res, "\n");
  if (data.empty())
    {
      LOG(LogError) << "List is empty";
      return vkey;
    }

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(data.c_str());
  if (!result)
    {
      LOG(LogError) << "Unable to parse hotkeys";
      return vkey;
    }

  pugi::xml_node root = doc.child("keys");
  if (!root)
    {
      LOG(LogError) << "Could not find <keys> node";
      return vkey;
    }

  for (pugi::xml_node key = root.child("key"); key; key = key.next_sibling("key"))
    {
      if (key.attribute("key"))
	vkey = key.attribute("key").as_string();
    }

  return vkey;
}

std::vector<std::string> ApiSystem::getEjectableDrives()
{
    return executeEnumerationScript("batocera-storage-manager list_ejectable");
}

bool ApiSystem::ejectDrive(const std::string& mountPath)
{
    std::string cmd = "batocera-storage-manager eject \"" + mountPath + "\"";
    return executeScript(cmd);
}

bool ApiSystem::mergeDrive(const std::string& mountPath)
{
    std::string cmd = "batocera-storage-manager merge \"" + mountPath + "\"";
    return executeScript(cmd);
}

bool ApiSystem::prepareDrive(const std::string& device, const std::string& fsType)
{
    std::string cmd = "batocera-storage-manager format \"" + device + "\" \"" + fsType + "\"";
    return executeScript(cmd);
}

bool ApiSystem::ignoreDevicePermanently(const std::string& deviceId)
{
    std::string cmd = "batocera-storage-manager ignore \"" + deviceId + "\"";
    return executeScript(cmd);
}

bool ApiSystem::nfc_is_available() {
  return Utils::FileSystem::exists("/var/run/batocera-nfc.running");
}

bool ApiSystem::nfc_write(const std::string& game) {
  std::string cmd = "batocera-nfc --write \"" + game + "\"";
  return executeScript(cmd);
}
