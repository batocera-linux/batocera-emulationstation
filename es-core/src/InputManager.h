#pragma once
#ifndef ES_CORE_INPUT_MANAGER_H
#define ES_CORE_INPUT_MANAGER_H

#include <SDL_joystick.h>
#include <map>
#include <pugixml/src/pugixml.hpp>
#include <utils/Delegate.h>

#ifdef HAVE_UDEV
#include <libudev.h>
#endif

class InputConfig;
class Window;
union SDL_Event;

struct PlayerDeviceInfo
{
	int index;
	int batteryLevel;
};

class IJoystickChangedEvent
{
public:
	virtual void onJoystickChanged() = 0;
};

struct Gun {
  std::string name;
#ifdef HAVE_UDEV
  std::string devpath;
  udev_device* dev;
  int fd;
#endif
};

//you should only ever instantiate one of these, by the way
class InputManager
{
public:
	static InputManager* getInstance();

	virtual ~InputManager();

	void writeDeviceConfig(InputConfig* config);
	void doOnFinish();
	static std::string getConfigPath();

	void init();
	void deinit();

	int getNumJoysticks();
	int getNumConfiguredDevices();

	std::vector<InputConfig*> getInputConfigs();

	bool parseEvent(const SDL_Event& ev, Window* window);

	void parseGuns(Window* window);
	bool getGunPosition(Gun* gun, float& perx, float& pery);
	std::map<int, Gun*>& getGuns() { return mGuns; }

	std::string configureEmulators();

	// information about last association players/pads 
	std::map<int, PlayerDeviceInfo>& lastKnownPlayersDeviceIndexes() { return m_lastKnownPlayersDeviceIndexes; }
	void computeLastKnownPlayersDeviceIndexes();

	void updateBatteryLevel(int id, std::string device, int level);

	static Delegate<IJoystickChangedEvent> joystickChanged;

private:
	InputManager();

	static InputManager* mInstance;
	static const int DEADZONE = 23000;
	static std::string getTemporaryConfigPath();

	void loadDefaultKBConfig();

	std::map<std::string, int> mJoysticksInitialValues;
	std::map<SDL_JoystickID, SDL_Joystick*> mJoysticks;
	std::map<SDL_JoystickID, InputConfig*> mInputConfigs;

	std::map<int, Gun*> mGuns;

	InputConfig* mMouseButtonsInputConfig;
	InputConfig* mKeyboardInputConfig;
	InputConfig* mCECInputConfig;

	std::map<SDL_JoystickID, int*> mPrevAxisValues;
	std::map<int, PlayerDeviceInfo> m_lastKnownPlayersDeviceIndexes;
	std::map<int, InputConfig*> computePlayersConfigs();

	bool initialized() const;
	bool loadInputConfig(InputConfig* config); // returns true if successfully loaded, false if not (or didn't exist)

	bool tryLoadInputConfig(std::string path, InputConfig* config, bool allowApproximate = true);

	InputConfig* getInputConfigByDevice(int deviceId);

	void clearJoysticks();
	void rebuildAllJoysticks(bool deinit = true);

#ifdef HAVE_UDEV
  struct udev *udev;
  struct udev_monitor *udev_monitor;

  static bool udev_input_poll_hotplug_available(struct udev_monitor *dev);
  void udev_initial_gunsList();
  bool udev_addGun(struct udev_device *dev, Window* window);
  bool udev_removeGun(struct udev_device *dev, Window* window);
#endif
};

#endif // ES_CORE_INPUT_MANAGER_H
