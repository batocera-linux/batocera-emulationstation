#ifndef RECALBOX_SYSTEM
#define    RECALBOX_SYSTEM

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

class RecalboxSystem {
public:

    static RecalboxSystem *getInstance();

    const static Uint32 SDL_FAST_QUIT = 0x800F;
    const static Uint32 SDL_RB_SHUTDOWN = 0X4000;
    const static Uint32 SDL_RB_REBOOT = 0x2000;

    unsigned long getFreeSpaceGB(std::string mountpoint);

    std::string getFreeSpaceInfo();

    bool isFreeSpaceLimit();

    std::string getVersion();
    std::string getRootPassword();

    bool setOverscan(bool enable);

    bool setOverclock(std::string mode);

    bool createLastVersionFileIfNotExisting();

    bool updateLastVersionFile();

    bool needToShowVersionMessage();

    std::string getVersionMessage();

    std::pair<std::string, int> updateSystem(BusyComponent* ui);
    std::pair<std::string, int> backupSystem(BusyComponent* ui, std::string device);

    bool ping();

    bool canUpdate();

    bool launchKodi(Window *window);
    bool launchFileManager(Window *window);

    bool enableWifi(std::string ssid, std::string key);

    bool disableWifi();

    bool reboot();

    bool shutdown();

    bool fastReboot();

    bool fastShutdown();

    std::string getIpAdress();


    std::vector<std::string> *scanBluetooth();

    bool pairBluetooth(std::string &basic_string);

    std::vector<std::string> getAvailableStorageDevices();
    std::vector<std::string> getAvailableBackupDevices();
    std::vector<std::string> getSystemInformations();
    std::vector<BiosSystem> getBiosInformations();
    bool generateSupportFile();

    std::string getCurrentStorage();

    bool setStorage(std::string basic_string);

    bool forgetBluetoothControllers();

    /* audio card */
    bool setAudioOutputDevice(std::string device);
    std::vector<std::string> getAvailableAudioOutputDevices();
    std::string getCurrentAudioOutputDevice();

private:
    static RecalboxSystem *instance;

    RecalboxSystem();

    bool halt(bool reboot, bool fast);

};

#endif

