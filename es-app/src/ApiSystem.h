#ifndef API_SYSTEM
#define API_SYSTEM

#include <string>
#include "Window.h"
#include "components/BusyComponent.h"

struct BiosFile {
  std::string status;
  std::string md5;
  std::string path;
};

struct BiosSystem {
  std::string name;
  std::vector<BiosFile> bios;
};

struct RetroAchievementGame
{
	std::string name;
	std::string achievements;
	std::string points;
	std::string lastplayed;
	std::string badge;
};

struct RetroAchievementInfo
{	
	std::string username;
	std::string totalpoints;
	std::string rank;
	std::string userpic;
	std::string registered;

	std::string error;

	std::vector<RetroAchievementGame> games;
};

struct BatoceraBezel
{
	std::string name;
	std::string url;
	std::string folderPath;
	bool isInstalled;
};

struct BatoceraTheme
{
	std::string name;
	std::string url;
	bool isInstalled;

	std::string image;
};

struct PacmanPackage
{
	PacmanPackage()
	{
		download_size = 0;
		installed_size = 0;
	}

	std::string name;
	std::string repository;
	std::string available_version;
	std::string description;
	std::string url;

	std::string packager;
	std::string status;

	size_t download_size;
	size_t installed_size;

	std::string group;
	//std::vector<std::string> groups;
	std::vector<std::string> licenses;	

	bool isInstalled()
	{
		return status == "installed";
	}
};


class ApiSystem 
{
public:
	enum ScriptId : unsigned int
	{
		WIFI = 0,
		RETROACHIVEMENTS = 1,
		BLUETOOTH = 2,
		RESOLUTION = 3,
		BIOSINFORMATION = 4,
		NETPLAY = 5,
		KODI = 6,
		GAMESETTINGS = 7,
		DECORATIONS = 8,
		SHADERS = 9,
		DISKFORMAT = 10,
		OVERCLOCK = 11,
		PDFEXTRACTION = 12,
		BATOCERASTORE = 13,
		EVMAPY = 14
	};

	virtual bool isScriptingSupported(ScriptId script);

    static ApiSystem* getInstance();

	/*
    const static Uint32 SDL_FAST_QUIT = 0x800F;
    const static Uint32 SDL_SYS_SHUTDOWN = 0X4000;
    const static Uint32 SDL_SYS_REBOOT = 0x2000;
	*/

    virtual unsigned long getFreeSpaceGB(std::string mountpoint);

    virtual std::string getFreeSpaceUserInfo();
    virtual std::string getFreeSpaceSystemInfo();
    std::string getFreeSpaceInfo(const std::string mountpoint);

    bool isFreeSpaceLimit();

    virtual std::string getVersion();
    std::string getRootPassword();

    bool setOverscan(bool enable);

    bool setOverclock(std::string mode);

    virtual std::pair<std::string, int> updateSystem(const std::function<void(const std::string)>& func = nullptr);

    std::pair<std::string, int> backupSystem(BusyComponent* ui, std::string device);
    std::pair<std::string, int> installSystem(BusyComponent* ui, std::string device, std::string architecture);
    std::pair<std::string, int> scrape(BusyComponent* ui);

    virtual bool ping();
    virtual bool canUpdate(std::vector<std::string>& output);
	virtual void setReadyFlag(bool ready = true);
	virtual bool isReadyFlagSet();

    virtual bool launchKodi(Window *window);
    bool launchFileManager(Window *window);
    bool launchErrorWindow(Window *window);

    bool enableWifi(std::string ssid, std::string key);
    bool disableWifi();

	virtual std::string getIpAdress();

    bool scanNewBluetooth(const std::function<void(const std::string)>& func = nullptr);
	std::vector<std::string> getBluetoothDeviceList();
	bool removeBluetoothDevice(const std::string deviceName);

    std::vector<std::string> getAvailableBackupDevices();
    std::vector<std::string> getAvailableInstallDevices();
    std::vector<std::string> getAvailableInstallArchitectures();
    std::vector<std::string> getAvailableOverclocking();
    std::vector<BiosSystem> getBiosInformations(const std::string system = "");
    virtual std::vector<std::string> getVideoModes();

	virtual std::vector<std::string> getAvailableStorageDevices();
	virtual std::vector<std::string> getSystemInformations();

    bool generateSupportFile();

    std::string getCurrentStorage();

    bool setStorage(std::string basic_string);

    bool forgetBluetoothControllers();

    /* audio card */
    bool setAudioOutputDevice(std::string device);
    std::vector<std::string> getAvailableAudioOutputDevices();
    std::string getCurrentAudioOutputDevice();

    /* video output */
    std::vector<std::string> getAvailableVideoOutputDevices();

    // Batocera
	RetroAchievementInfo getRetroAchievements();

	// Themes
	virtual std::vector<BatoceraTheme> getBatoceraThemesList();
	virtual std::pair<std::string,int> installBatoceraTheme(std::string thname, const std::function<void(const std::string)>& func = nullptr);
	virtual std::pair<std::string, int> uninstallBatoceraBezel(std::string bezelsystem, const std::function<void(const std::string)>& func = nullptr);

    virtual std::vector<BatoceraBezel> getBatoceraBezelsList();
	virtual std::pair<std::string,int> installBatoceraBezel(std::string bezelsystem, const std::function<void(const std::string)>& func = nullptr);
	virtual std::pair<std::string,int> uninstallBatoceraTheme(std::string bezelsystem, const std::function<void(const std::string)>& func = nullptr);

	virtual std::string getCRC32(const std::string fileName, bool fromZipContents = true);

	virtual int getPdfPageCount(const std::string fileName);
	virtual std::vector<std::string> extractPdfImages(const std::string fileName, int pageIndex = -1, int pageCount = 1);

	std::vector<PacmanPackage> getBatoceraStorePackages();
	std::pair<std::string, int> installBatoceraStorePackage(std::string name, const std::function<void(const std::string)>& func = nullptr);
	std::pair<std::string, int> uninstallBatoceraStorePackage(std::string name, const std::function<void(const std::string)>& func = nullptr);
	void updateBatoceraStorePackageList();
	void refreshBatoceraStorePackageList();

	bool	getBrighness(int& value);
	void	setBrighness(int value);

	std::vector<std::string> getWifiNetworks(bool scan = false);

	bool downloadFile(const std::string url, const std::string fileName, const std::string label = "", const std::function<void(const std::string)>& func = nullptr);
	std::string downloadToCache(const std::string url);

	// Formating
	std::vector<std::string> getFormatDiskList();
	std::vector<std::string> getFormatFileSystems();
	int formatDisk(const std::string disk, const std::string format, const std::function<void(const std::string)>& func = nullptr);

	virtual std::vector<std::string> getShaderList();

protected:
	ApiSystem();

	virtual bool executeScript(const std::string command);	
	virtual std::pair<std::string, int> executeScript(const std::string command, const std::function<void(const std::string)>& func);
	virtual std::vector<std::string> executeEnumerationScript(const std::string command);
	
	void getBatoceraThemesImages(std::vector<BatoceraTheme>& items);
	std::string getUpdateUrl();
    static ApiSystem* instance;

    void launchExternalWindow_before(Window *window);
    void launchExternalWindow_after(Window *window);
};

#endif

