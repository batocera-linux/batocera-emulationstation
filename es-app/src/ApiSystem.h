#ifndef API_SYSTEM
#define API_SYSTEM

#include <string>
#include <map>
#include "Window.h"
#include "components/BusyComponent.h"
#include "resources/TextureData.h"
#include "components/IExternalActivity.h"

struct BiosFile 
{
  std::string status;
  std::string md5;
  std::string path;
};

struct BiosSystem 
{
  std::string name;
  std::vector<BiosFile> bios;
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
	std::string author;
	std::string lastUpdate;
	int upToDate;
	int size;
	std::string image;
	
	bool isInstalled;
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

	std::string preview_url;

	std::string group;
	std::vector<std::string> licenses;	

	std::string arch;

	bool isInstalled() { return status == "installed"; }
};

struct PadInfo
{
	int id;
	std::string name;
	std::string device;
	std::string status;
	std::string path;
	int battery;
};

class ApiSystem : public IPdfHandler, public IExternalActivity
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
		EVMAPY = 14,
		THEMESDOWNLOADER = 15,
		THEBEZELPROJECT = 16,
		PADSINFO = 17,
		BATOCERAPREGAMELISTSHOOK = 18,
		TIMEZONES = 19,
		AUDIODEVICE = 20,
		BACKUP = 21,
		INSTALL = 22,
		SUPPORTFILE = 23,
		UPGRADE = 24,
		SUSPEND = 25,
		VERSIONINFO = 26,
		PLANEMODE = 27,
	};

	virtual bool isScriptingSupported(ScriptId script);

    static ApiSystem* getInstance();
	virtual void deinit() { };

    virtual unsigned long getFreeSpaceGB(std::string mountpoint);

    virtual std::string getFreeSpaceUserInfo();
    virtual std::string getFreeSpaceSystemInfo();
    std::string getFreeSpaceInfo(const std::string mountpoint);

    bool isFreeSpaceLimit();

    virtual std::string getVersion(bool extra = false);
	virtual std::string getApplicationName();

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

    bool enableWifi(std::string ssid, std::string key);
    bool disableWifi();

	virtual std::string getIpAdress();

	void startBluetoothLiveDevices(const std::function<void(const std::string)>& func);
	void stopBluetoothLiveDevices();
	bool pairBluetoothDevice(const std::string& deviceName);
	bool removeBluetoothDevice(const std::string& deviceName);

	std::vector<std::string> getPairedBluetoothDeviceList();

	// Obsolete
    bool scanNewBluetooth(const std::function<void(const std::string)>& func = nullptr);

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

	bool setButtonColorGameForce(std::string basic_string);

	bool setPowerLedGameForce(std::string basic_string);

    bool forgetBluetoothControllers();

    /* audio card */
    bool setAudioOutputDevice(std::string device);
    std::vector<std::string> getAvailableAudioOutputDevices();
    std::string getCurrentAudioOutputDevice();
    bool setAudioOutputProfile(std::string profile);
    std::vector<std::string> getAvailableAudioOutputProfiles();
    std::string getCurrentAudioOutputProfile();

    /* video output */
    std::vector<std::string> getAvailableVideoOutputDevices();

	// Themes
	virtual std::vector<BatoceraTheme> getBatoceraThemesList();
	virtual bool isThemeInstalled(const std::string& themeName, const std::string& url);
	virtual std::pair<std::string,int> installBatoceraTheme(std::string thname, const std::function<void(const std::string)>& func = nullptr);
	virtual std::pair<std::string, int> uninstallBatoceraBezel(std::string bezelsystem, const std::function<void(const std::string)>& func = nullptr);

    virtual std::vector<BatoceraBezel> getBatoceraBezelsList();
	virtual std::pair<std::string,int> installBatoceraBezel(std::string bezelsystem, const std::function<void(const std::string)>& func = nullptr);
	virtual std::pair<std::string,int> uninstallBatoceraTheme(std::string bezelsystem, const std::function<void(const std::string)>& func = nullptr);

	virtual std::string getCRC32(const std::string fileName, bool fromZipContents = true);
	virtual std::string getMD5(const std::string fileName, bool fromZipContents = true);

	virtual bool unzipFile(const std::string fileName, const std::string destFolder = "", const std::function<bool(const std::string)>& shouldExtract = nullptr);

	virtual int getPdfPageCount(const std::string& fileName);
	virtual std::vector<std::string> extractPdfImages(const std::string& fileName, int pageIndex = -1, int pageCount = 1, int quality = 0);

	virtual std::string getRunningArchitecture();

	std::vector<PacmanPackage> getBatoceraStorePackages();
	std::pair<std::string, int> installBatoceraStorePackage(std::string name, const std::function<void(const std::string)>& func = nullptr);
	std::pair<std::string, int> uninstallBatoceraStorePackage(std::string name, const std::function<void(const std::string)>& func = nullptr);
	void updateBatoceraStorePackageList();
	void refreshBatoceraStorePackageList();

	void callBatoceraPreGameListsHook();

	bool	getBrightness(int& value);
	void	setBrightness(int value);

	std::vector<std::string> getWifiNetworks(bool scan = false);

	bool downloadFile(const std::string url, const std::string fileName, const std::string label = "", const std::function<void(const std::string)>& func = nullptr);
	
	// Formating
	std::vector<std::string> getFormatDiskList();
	std::vector<std::string> getFormatFileSystems();
	int formatDisk(const std::string disk, const std::string format, const std::function<void(const std::string)>& func = nullptr);


	virtual std::vector<std::string> getRetroachievementsSoundsList();
	virtual std::vector<std::string> getShaderList(const std::string& systemName, const std::string& emulator, const std::string& core);
	virtual std::string getSevenZipCommand() { return "7zr"; }

	virtual std::vector<std::string> getTimezones();
	virtual std::string getCurrentTimezone();
	virtual bool setTimezone(std::string tz);

	virtual std::vector<PadInfo> getPadsInfo();
	virtual std::string getHostsName();
	virtual bool emuKill();
	virtual void suspend();

  	virtual void replugControllers_sindenguns();
    	virtual void replugControllers_wiimotes();
    	virtual void replugControllers_steamdeckguns();

  	virtual bool isPlaneMode();
    virtual bool setPlaneMode(bool enable);

protected:
	ApiSystem();

	virtual bool executeScript(const std::string command);  
	virtual std::pair<std::string, int> executeScript(const std::string command, const std::function<void(const std::string)>& func);
	virtual std::vector<std::string> executeEnumerationScript(const std::string command);
	virtual bool downloadGitRepository(const std::string& url, const std::string& branch, const std::string& fileName, const std::string& label, const std::function<void(const std::string)>& func, long defaultDownloadSize = 0);
	virtual std::string getGitRepositoryDefaultBranch(const std::string& url);
		
	virtual std::string getUpdateUrl();
	virtual std::string getThemesUrl();

    static ApiSystem* instance;

    void launchExternalWindow_before(Window *window);
    void launchExternalWindow_after(Window *window);
};

#endif

