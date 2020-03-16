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
		KODI = 6
	};

	virtual bool isScriptingSupported(ScriptId script);

    static ApiSystem* getInstance();

    const static Uint32 SDL_FAST_QUIT = 0x800F;
    const static Uint32 SDL_SYS_SHUTDOWN = 0X4000;
    const static Uint32 SDL_SYS_REBOOT = 0x2000;

    virtual unsigned long getFreeSpaceGB(std::string mountpoint);

    virtual std::string getFreeSpaceInfo();

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

    virtual bool launchKodi(Window *window);
    bool launchFileManager(Window *window);

    bool enableWifi(std::string ssid, std::string key);
    bool disableWifi();

	bool reboot() { return halt(true, false); }
	bool fastReboot() { return halt(true, true); }
	bool shutdown() { return halt(false, false); }
	bool fastShutdown() { return halt(false, true); }

    virtual std::string getIpAdress();

    bool scanNewBluetooth(const std::function<void(const std::string)>& func = nullptr);

    std::vector<std::string> getAvailableBackupDevices();
    std::vector<std::string> getAvailableInstallDevices();
    std::vector<std::string> getAvailableInstallArchitectures();
    std::vector<std::string> getAvailableOverclocking();
    std::vector<BiosSystem> getBiosInformations();
    std::vector<std::string> getVideoModes();

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
	virtual std::vector<std::string> getBatoceraThemesList();
	virtual std::pair<std::string,int> installBatoceraTheme(std::string thname, const std::function<void(const std::string)>& func = nullptr);

    virtual std::vector<std::string> getBatoceraBezelsList();
	virtual std::pair<std::string,int> installBatoceraBezel(std::string bezelsystem, const std::function<void(const std::string)>& func = nullptr);
	virtual std::pair<std::string,int> uninstallBatoceraBezel(BusyComponent* ui, std::string bezelsystem);

	virtual std::string getCRC32(const std::string fileName, bool fromZipContents = true);

	bool	getBrighness(int& value);
	void	setBrighness(int value);

	std::vector<std::string> getWifiNetworks(bool scan = false);

	bool downloadFile(const std::string url, const std::string fileName, const std::string label = "", const std::function<void(const std::string)>& func = nullptr);

protected:
	ApiSystem();

	virtual bool executeScript(const std::string command);	
	virtual std::vector<std::string> executeEnumerationScript(const std::string command);

    static ApiSystem* instance;

    bool halt(bool reboot, bool fast);
    
    void launchExternalWindow_before(Window *window);
    void launchExternalWindow_after(Window *window);
};

#endif

