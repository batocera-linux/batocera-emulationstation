#pragma once
#ifndef ES_CORE_INPUT_MANAGER_H
#define ES_CORE_INPUT_MANAGER_H

#include <SDL_joystick.h>
#include <map>
#include <pugixml/src/pugixml.hpp>
#include <utils/Delegate.h>

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
};

#endif // ES_CORE_INPUT_MANAGER_H
