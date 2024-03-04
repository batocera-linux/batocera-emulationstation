#include "InputManager.h"

#include "utils/FileSystemUtil.h"
#include "CECInput.h"
#include "Log.h"
#include "utils/Platform.h"
#include "Scripting.h"
#include "Window.h"
#include <pugixml/src/pugixml.hpp>
#include <SDL.h>
#include <iostream>
#include <assert.h>
#include "Settings.h"
#include <algorithm>
#include <mutex>
#include "utils/StringUtil.h"
#include "LocaleES.h"
#include "Paths.h"
#include "GunManager.h"
#include "renderers/Renderer.h"

#ifdef HAVE_UDEV
#include <libudev.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <unistd.h>
#endif

#define KEYBOARD_GUID_STRING "-1"
#define CEC_GUID_STRING      "-2"
#define GUN_GUID_STRING      "-4"

// SO HEY POTENTIAL POOR SAP WHO IS TRYING TO MAKE SENSE OF ALL THIS (by which I mean my future self)
// There are like four distinct IDs used for joysticks (crazy, right?)
// 1. Device index - this is the "lowest level" identifier, and is just the Nth joystick plugged in to the system (like /dev/js#).
//    It can change even if the device is the same, and is only used to open joysticks (required to receive SDL events).
// 2. SDL_JoystickID - this is an ID for each joystick that is supposed to remain consistent between plugging and unplugging.
//    ES doesn't care if it does, though.
// 3. "Device ID" - this is something I made up and is what InputConfig's getDeviceID() returns.  
//    This is actually just an SDL_JoystickID (also called instance ID), but -1 means "keyboard" instead of "error."
// 4. Joystick GUID - this is some squashed version of joystick vendor, version, and a bunch of other device-specific things.
//    It should remain the same across runs of the program/system restarts/device reordering and is what I use to identify which joystick to load.

// hack for cec support
int SDL_USER_CECBUTTONDOWN = -1;
int SDL_USER_CECBUTTONUP   = -1;

static std::mutex mJoysticksLock;

InputManager* InputManager::mInstance = NULL;
Delegate<IJoystickChangedEvent> InputManager::joystickChanged;

InputManager::InputManager() : mKeyboardInputConfig(nullptr), mMouseButtonsInputConfig(nullptr), mCECInputConfig(nullptr), mGunInputConfig(nullptr), mGunManager(nullptr)
{

}

InputManager::~InputManager()
{
	deinit();
}

InputManager* InputManager::getInstance()
{
	if (!mInstance)
		mInstance = new InputManager();

	return mInstance;
}


void InputManager::init()
{
	if (initialized())
		deinit();

	mKeyboardInputConfig = new InputConfig(DEVICE_KEYBOARD, -1, "Keyboard", KEYBOARD_GUID_STRING, 0, 0, 0); 
	loadInputConfig(mKeyboardInputConfig);

  rebuildAllJoysticks(false);

#ifdef HAVE_LIBCEC
	SDL_USER_CECBUTTONDOWN = SDL_RegisterEvents(2);
	SDL_USER_CECBUTTONUP   = SDL_USER_CECBUTTONDOWN + 1;
	CECInput::init();
	mCECInputConfig = new InputConfig(DEVICE_CEC, -1, "CEC", CEC_GUID_STRING, 0, 0, 0); 
	loadInputConfig(mCECInputConfig);
#else
	mCECInputConfig = nullptr;
#endif

	mGunInputConfig = new InputConfig(DEVICE_GUN, -1, "Gun", GUN_GUID_STRING, 0, 0, 0);
	loadDefaultGunConfig();

	// Mouse input, hardcoded not configurable with es_input.cfg
	mMouseButtonsInputConfig = new InputConfig(DEVICE_MOUSE, -1, "Mouse", CEC_GUID_STRING, 0, 0, 0);
	mMouseButtonsInputConfig->mapInput(BUTTON_OK, Input(DEVICE_MOUSE, TYPE_BUTTON, 1, 1, true));
	mMouseButtonsInputConfig->mapInput(BUTTON_BACK, Input(DEVICE_MOUSE, TYPE_BUTTON, 3, 1, true));

	mGunManager = new GunManager();
}

std::vector<std::string> InputManager::getMice() {
  std::vector<std::string> mice;

  #ifdef HAVE_UDEV
  struct udev *udev;
  struct udev_list_entry *devs = NULL;
  struct udev_list_entry *item = NULL;
  char ident[256];

  udev = udev_new();
  if (udev != NULL)
    {
      struct udev_enumerate *enumerate = udev_enumerate_new(udev);
      udev_enumerate_add_match_property(enumerate, "ID_INPUT_MOUSE", "1");
      udev_enumerate_add_match_subsystem(enumerate, "input");
      udev_enumerate_scan_devices(enumerate);
      devs = udev_enumerate_get_list_entry(enumerate);

      for (item = devs; item; item = udev_list_entry_get_next(item))
	{
	  const char *name = udev_list_entry_get_name(item);
	  struct udev_device *dev = udev_device_new_from_syspath(udev, name);

	  const char* devnode;
	  devnode = udev_device_get_devnode(dev);
	  if (devnode != NULL) {
	    int fd = open(devnode, O_RDONLY | O_NONBLOCK);
	    if (fd >= 0) {
	      if (ioctl(fd, EVIOCGNAME(sizeof(ident)), ident) > 0) {
		mice.push_back(std::string(ident));
	      }
	      close(fd);
	    }
	  }
	  udev_device_unref(dev);
	}

      udev_unref(udev);
    }
  #endif
  return mice;
}


void InputManager::deinit()
{
	if(!initialized())
		return;

	clearJoysticks();

	if(mKeyboardInputConfig != NULL)
	{
		delete mKeyboardInputConfig;
		mKeyboardInputConfig = NULL;
	}

	if(mCECInputConfig != NULL)
	{
		delete mCECInputConfig;
		mCECInputConfig = NULL;
	}

	if (mMouseButtonsInputConfig != NULL)
	{
		delete mMouseButtonsInputConfig;
		mMouseButtonsInputConfig = NULL;
	}

	if(mGunInputConfig != NULL)
	{
		delete mGunInputConfig;
		mGunInputConfig = NULL;
	}

#ifdef HAVE_LIBCEC
	CECInput::deinit();
#endif

	SDL_JoystickEventState(SDL_DISABLE);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);

	if (mGunManager != nullptr)
	{
		delete mGunManager;
		mGunManager = nullptr;
	}
}

int InputManager::getNumJoysticks() 
{ 
	std::unique_lock<std::mutex> lock(mJoysticksLock);
	return (int)mJoysticks.size(); 
}

InputConfig* InputManager::getInputConfigByDevice(int device)
{
	if(device == DEVICE_KEYBOARD)
		return mKeyboardInputConfig;

	if(device == DEVICE_CEC)
		return mCECInputConfig;

	if(device == DEVICE_MOUSE)
		return mMouseButtonsInputConfig;

	if(device == DEVICE_GUN)
		return mGunInputConfig;
	
	return mInputConfigs[device];
}

void InputManager::clearJoysticks()
{
	mJoysticksLock.lock();

	for (auto iter = mJoysticks.begin(); iter != mJoysticks.end(); iter++)
		SDL_JoystickClose(iter->second);

	mJoysticks.clear();

	for (auto iter = mInputConfigs.begin(); iter != mInputConfigs.end(); iter++)
		if (iter->second)
			delete iter->second;

	mInputConfigs.clear();

	for (auto iter = mPrevAxisValues.begin(); iter != mPrevAxisValues.end(); iter++)
		if (iter->second)
			delete[] iter->second;

	mPrevAxisValues.clear();

	mJoysticksLock.unlock();
}

#if defined(WIN32)
#include <cfgmgr32.h>

class Win32RawInputApi
{
public:
	Win32RawInputApi()
	{		
		m_hSDL2 = ::LoadLibrary("SDL2.dll");
		if (m_hSDL2 != NULL)
		{
			m_JoystickPathForIndex = (SDL_JoystickPathForIndexPtr) ::GetProcAddress(m_hSDL2, "SDL_JoystickPathForIndex");
		}

		m_hSetupapi = ::LoadLibrary("setupapi.dll");
		if (m_hSetupapi != NULL)
		{
			m_CM_Locate_DevNodeA = (CM_Locate_DevNodeAPtr) ::GetProcAddress(m_hSetupapi, "CM_Locate_DevNodeA");
			m_CM_Get_Parent = (CM_Get_ParentPtr) ::GetProcAddress(m_hSetupapi, "CM_Get_Parent");
			m_CM_Get_Device_IDA = (CM_Get_Device_IDAPtr) ::GetProcAddress(m_hSetupapi, "CM_Get_Device_IDA");
		}
	}

	~Win32RawInputApi()
	{
		if (m_hSDL2 != NULL)
			::FreeLibrary(m_hSDL2);

		m_hSDL2 = NULL;

		if (m_hSetupapi != NULL)
			::FreeLibrary(m_hSetupapi);

		m_hSetupapi = NULL;
	}
	
	std::string SDL_JoystickPathForIndex(int device_index)
	{
		if (m_JoystickPathForIndex != NULL)
			return m_JoystickPathForIndex(device_index);

		return "";
	}

	std::string getInputDeviceParent(const std::string& devicePath)
	{
		if (m_CM_Locate_DevNodeA == NULL || m_CM_Get_Parent == NULL || m_CM_Get_Device_IDA == NULL)
			return devicePath;

		std::string path = devicePath;

		auto vidindex = path.find("VID_");
		if (vidindex == std::string::npos)
			vidindex = path.find("vid_");

		if (vidindex != std::string::npos)
		{
			auto cut = path.find("#{", vidindex);
			if (cut != std::string::npos)
				path = path.substr(0, cut);
		}

		path = Utils::String::replace(path, "\\\\?\\", "");
		path = Utils::String::replace(path, "#", "\\");

		DEVINST nDevInst;
		int apiResult = m_CM_Locate_DevNodeA(&nDevInst, (DEVINSTID_A) path.c_str(), CM_LOCATE_DEVNODE_NORMAL);
		if (apiResult == CR_SUCCESS)
		{
			if (m_CM_Get_Parent(&nDevInst, nDevInst, 0) == CR_SUCCESS)
			{
				char buf[255];
				if (m_CM_Get_Device_IDA(nDevInst, buf, 255, 0) == CR_SUCCESS)
					return std::string(buf);
			}
		}

		return devicePath;
	}

private:	
	HMODULE m_hSDL2;
	HMODULE m_hSetupapi;

	typedef const char *(SDLCALL *SDL_JoystickPathForIndexPtr)(int);
	SDL_JoystickPathForIndexPtr m_JoystickPathForIndex;
	
	typedef CONFIGRET(WINAPI* CM_Locate_DevNodeAPtr)(PDEVINST pdnDevInst, DEVINSTID_A pDeviceID, ULONG ulFlags);
	CM_Locate_DevNodeAPtr m_CM_Locate_DevNodeA;

	typedef CONFIGRET(WINAPI* CM_Get_ParentPtr)(PDEVINST pdnDevInst, DEVINST dnDevInst, ULONG ulFlags);
	CM_Get_ParentPtr m_CM_Get_Parent;

	typedef CONFIGRET(WINAPI* CM_Get_Device_IDAPtr)(DEVINST dnDevInst, PSTR Buffer, ULONG BufferLen, ULONG ulFlags);
	CM_Get_Device_IDAPtr m_CM_Get_Device_IDA;
};

Win32RawInputApi Win32RawInput;
#endif

void InputManager::rebuildAllJoysticks(bool deinit)
{
	if (deinit)
		SDL_JoystickEventState(SDL_DISABLE);

	clearJoysticks();

	if (deinit)
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK);

#if WIN32
	if (!Settings::getInstance()->getBool("HidJoysticks"))
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");

	SDL_SetHint("SDL_JOYSTICK_HIDAPI_WII", "0");
#endif
			
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, Settings::getInstance()->getBool("BackgroundJoystickInput") ? "1" : "0");
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);	

	mJoysticksLock.lock();

	int numJoysticks = SDL_NumJoysticks();

	for (int idx = 0; idx < numJoysticks; idx++)
	{
		// open joystick & add to our list
		SDL_Joystick* joy = SDL_JoystickOpen(idx);
		if (joy == nullptr)
			continue;

		// add it to our list so we can close it again later
		SDL_JoystickID joyId = SDL_JoystickInstanceID(joy);

		mJoysticks.erase(joyId);
		mJoysticks[joyId] = joy;

		char guid[40];
		SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joy), guid, 40);

#if WIN32
		// SDL 2.26 + -> Remove new CRC-16 name hash encoding
		for (int i = 4; i < 8; i++) guid[i] = '0';
#endif

		// create the InputConfig
		auto cfg = mInputConfigs.find(joyId);
		if (cfg != mInputConfigs.cend())
		{
			if (cfg->second != nullptr)
				delete cfg->second;

			mInputConfigs.erase(joyId);
		}

		// if SDL_JoystickPathForIndex does not exist, store a value containing index + guid
		std::string devicePath = Utils::String::padLeft(std::to_string(idx), 4, '0') + "@" + std::string(guid);

#if WIN32
		SDL_version ver;
		SDL_GetVersion(&ver);
		if (ver.major >= 2 && ver.minor >= 24)
			devicePath = Win32RawInput.getInputDeviceParent(Win32RawInput.SDL_JoystickPathForIndex(idx));
#elif SDL_VERSION_ATLEAST(2, 24, 0)
		devicePath = SDL_JoystickPathForIndex(idx);
#endif

		mInputConfigs[joyId] = new InputConfig(joyId, idx, SDL_JoystickName(joy), guid, SDL_JoystickNumButtons(joy), SDL_JoystickNumHats(joy), SDL_JoystickNumAxes(joy), devicePath);

		if (!loadInputConfig(mInputConfigs[joyId]))
		{
#if !BATOCERA
			std::string mappingString;
			
			if (SDL_IsGameController(idx))
				mappingString = SDL_GameControllerMappingForDeviceIndex(idx);
			
			if (!mappingString.empty() && loadFromSdlMapping(mInputConfigs[joyId], mappingString))
			{
				InputManager::getInstance()->writeDeviceConfig(mInputConfigs[joyId]); // save
				LOG(LogInfo) << "Creating joystick from SDL Game Controller mapping " << SDL_JoystickName(joy) << " (GUID: " << guid << ", instance ID: " << joyId << ", device index: " << idx << ", device path : " << devicePath << ").";
			}
			else
#endif
				LOG(LogInfo) << "Added unconfigured joystick " << SDL_JoystickName(joy) << " (GUID: " << guid << ", instance ID: " << joyId << ", device index: " << idx << ", device path : " << devicePath << ").";
		}
		else
			LOG(LogInfo) << "Added known joystick " << SDL_JoystickName(joy) << " (GUID: " << guid << ", instance ID: " << joyId << ", device index: " << idx << ", device path : " << devicePath << ").";

		// set up the prevAxisValues
		int numAxes = SDL_JoystickNumAxes(joy);
		
		mPrevAxisValues.erase(joyId);
		mPrevAxisValues[joyId] = new int[numAxes];
		std::fill(mPrevAxisValues[joyId], mPrevAxisValues[joyId] + numAxes, 0); //initialize array to 0
	}	

	mJoysticksLock.unlock();

	computeLastKnownPlayersDeviceIndexes();

	joystickChanged.invoke([](IJoystickChangedEvent* c) { c->onJoystickChanged(); });

	SDL_JoystickEventState(SDL_ENABLE);
}

#if WIN32
// Retrocompatible declaration for SDL_JoyBatteryEvent
typedef struct SDL_JoyBatteryEventX
{
	Uint32 type;        
	Uint32 timestamp;   
	SDL_JoystickID which; 
	SDL_JoystickPowerLevel level;
} SDL_JoyBatteryEventX;
#endif

bool InputManager::parseEvent(const SDL_Event& ev, Window* window)
{
	bool causedEvent = false;

	switch (ev.type)
	{
#if WIN32
	case 1543: // SDL_JOYBATTERYUPDATED, new event with SDL 2.24+
	{
		SDL_JoyBatteryEventX* jbattery = (SDL_JoyBatteryEventX*)&ev;

		auto inputConfig = mInputConfigs.find(jbattery->which);
		if (inputConfig != mInputConfigs.cend() && inputConfig->second->isConfigured() && jbattery->level != SDL_JoystickPowerLevel::SDL_JOYSTICK_POWER_UNKNOWN)
		{
			int level = 0;

			switch (jbattery->level)
			{
			case SDL_JoystickPowerLevel::SDL_JOYSTICK_POWER_LOW:
				level = 25;
				break;
			case SDL_JoystickPowerLevel::SDL_JOYSTICK_POWER_MEDIUM:
				level = 50;
				break;
			case SDL_JoystickPowerLevel::SDL_JOYSTICK_POWER_MAX:
				level = 75;
				break;
			case SDL_JoystickPowerLevel::SDL_JOYSTICK_POWER_FULL:
				level = 100;
				break;
			case SDL_JoystickPowerLevel::SDL_JOYSTICK_POWER_WIRED:
				level = -1;
				break;
			}

			inputConfig->second->updateBatteryLevel(level);
			computeLastKnownPlayersDeviceIndexes();
		}
	}
	break;
#endif

	case SDL_JOYAXISMOTION:
	{		
	// some axes are "full" : from -32000 to +32000
	// in this case, their unpressed state is not 0
	// SDL provides a function to get this value
	// in es, the trick is to minus this value to the value to do as if it started at 0
		int initialValue = 0;
		Sint16 x;

#if SDL_VERSION_ATLEAST(2, 0, 9)
		// SDL_JoystickGetAxisInitialState doesn't work with 8bitdo start+b
		// required for several pads like xbox and 8bitdo

		auto inputConfig = mInputConfigs.find(ev.jaxis.which);
		if (inputConfig != mInputConfigs.cend())
		{
			std::string guid = std::to_string(ev.jaxis.axis) + "@" + inputConfig->second->getDeviceGUIDString();

			auto it = mJoysticksInitialValues.find(guid);
			if (it != mJoysticksInitialValues.cend())
				initialValue = it->second;
			else if (SDL_JoystickGetAxisInitialState(mJoysticks[ev.jaxis.which], ev.jaxis.axis, &x))
			{
				mJoysticksInitialValues[guid] = x;
				initialValue = x;
			}
		}
#endif

		if (mPrevAxisValues.find(ev.jaxis.which) != mPrevAxisValues.cend())
		{			
			//if it switched boundaries
			if ((abs(ev.jaxis.value - initialValue) > DEADZONE) != (abs(mPrevAxisValues[ev.jaxis.which][ev.jaxis.axis]) > DEADZONE))
			{
				int normValue;
				if (abs(ev.jaxis.value - initialValue) <= DEADZONE) 
					normValue = 0;
				else
					if (ev.jaxis.value - initialValue > 0) 
						normValue = 1;
					else
						normValue = -1;

				window->input(getInputConfigByDevice(ev.jaxis.which), Input(ev.jaxis.which, TYPE_AXIS, ev.jaxis.axis, normValue, false));
				causedEvent = true;
			}

			mPrevAxisValues[ev.jaxis.which][ev.jaxis.axis] = ev.jaxis.value - initialValue; 
		}

		return causedEvent;
	}
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		window->input(getInputConfigByDevice(ev.jbutton.which), Input(ev.jbutton.which, TYPE_BUTTON, ev.jbutton.button, ev.jbutton.state == SDL_PRESSED, false));
		return true;
	
	case SDL_MOUSEBUTTONDOWN:        
	case SDL_MOUSEBUTTONUP:
		if (!getGunManager()->isReplacingMouse())
			if (!window->processMouseButton(ev.button.button, ev.type == SDL_MOUSEBUTTONDOWN, ev.button.x, ev.button.y))
				window->input(getInputConfigByDevice(DEVICE_MOUSE), Input(DEVICE_MOUSE, TYPE_BUTTON, ev.button.button, ev.type == SDL_MOUSEBUTTONDOWN, false));

		return true;

	case SDL_MOUSEMOTION:
#if !WIN32
	  if (ev.motion.which == SDL_TOUCH_MOUSEID)
#endif
		if (!getGunManager()->isReplacingMouse())
			window->processMouseMove(ev.motion.x, ev.motion.y, ev.motion.which == SDL_TOUCH_MOUSEID);

		return true;

	case SDL_MOUSEWHEEL:
		if (ev.wheel.which != SDL_TOUCH_MOUSEID)
			window->processMouseWheel(ev.wheel.y);

		return true;

	case SDL_JOYHATMOTION:
		window->input(getInputConfigByDevice(ev.jhat.which), Input(ev.jhat.which, TYPE_HAT, ev.jhat.hat, ev.jhat.value, false));
		return true;

	case SDL_KEYDOWN:
		if (ev.key.keysym.sym == SDLK_BACKSPACE && SDL_IsTextInputActive())
			window->textInput("\b");

		if (ev.key.repeat)
			return false;

#if !WIN32
		if (ev.key.keysym.sym == SDLK_F4)
		{
			SDL_Event* quit = new SDL_Event();
			quit->type = SDL_QUIT;
			SDL_PushEvent(quit);
			return false;
		}
#endif

		window->input(getInputConfigByDevice(DEVICE_KEYBOARD), Input(DEVICE_KEYBOARD, TYPE_KEY, ev.key.keysym.sym, 1, false));
		return true;

	case SDL_KEYUP:
		window->input(getInputConfigByDevice(DEVICE_KEYBOARD), Input(DEVICE_KEYBOARD, TYPE_KEY, ev.key.keysym.sym, 0, false));
		return true;

	case SDL_TEXTINPUT:
		window->textInput(ev.text.text);
		break;

	case SDL_JOYDEVICEADDED:
		{
			std::string addedDeviceName;
			bool isWheel = false;
			auto id = SDL_JoystickGetDeviceInstanceID(ev.jdevice.which);
			auto it = std::find_if(mInputConfigs.cbegin(), mInputConfigs.cend(), [id](const std::pair<SDL_JoystickID, InputConfig*> & t) { return t.second != nullptr && t.second->getDeviceId() == id; });
			if (it == mInputConfigs.cend())
				addedDeviceName = SDL_JoystickNameForIndex(ev.jdevice.which);

#ifdef HAVE_UDEV
#ifdef SDL_JoystickDevicePathById
                        SDL_Joystick* joy = SDL_JoystickOpen(ev.jdevice.which);
		        if (joy != nullptr) {
                          SDL_JoystickID joyId = SDL_JoystickInstanceID(joy);
                          isWheel = InputConfig::isWheel(SDL_JoystickDevicePathById(joyId));
                          SDL_JoystickClose(joy);
			}
#endif
#endif
			rebuildAllJoysticks();

			if (Settings::getInstance()->getBool("ShowControllerNotifications") && !addedDeviceName.empty()) {
			  if(isWheel) {
			    window->displayNotificationMessage(_U("\uF1B9 ") + Utils::String::format(_("%s connected").c_str(), Utils::String::trim(addedDeviceName).c_str()));
			  } else {
			    window->displayNotificationMessage(_U("\uF11B ") + Utils::String::format(_("%s connected").c_str(), Utils::String::trim(addedDeviceName).c_str()));
			  }
			}
		}
		return true;

	case SDL_JOYDEVICEREMOVED:
		{
			auto it = mInputConfigs.find(ev.jdevice.which);
			if (Settings::getInstance()->getBool("ShowControllerNotifications") && it != mInputConfigs.cend() && it->second != nullptr) {
			  if(it->second->isWheel()) {
			    window->displayNotificationMessage(_U("\uF1B9 ") + Utils::String::format(_("%s disconnected").c_str(), Utils::String::trim(it->second->getDeviceName()).c_str()));
			  } else {
			    window->displayNotificationMessage(_U("\uF11B ") + Utils::String::format(_("%s disconnected").c_str(), Utils::String::trim(it->second->getDeviceName()).c_str()));
			  }
			}
	
			rebuildAllJoysticks();
		}
		return false;
	}

	if (mCECInputConfig && (ev.type == (unsigned int)SDL_USER_CECBUTTONDOWN || ev.type == (unsigned int)SDL_USER_CECBUTTONUP))
	{
		window->input(getInputConfigByDevice(DEVICE_CEC), Input(DEVICE_CEC, TYPE_CEC_BUTTON, ev.user.code, ev.type == (unsigned int)SDL_USER_CECBUTTONDOWN, false));
		return true;
	}

	return false;
}

bool InputManager::tryLoadInputConfig(std::string path, InputConfig* config, bool allowApproximate)
{
	pugi::xml_node configNode(NULL);

	if (!Utils::FileSystem::exists(path))
		return false;

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(WINSTRINGW(path).c_str());

	if (!res)
	{
		LOG(LogError) << "Error parsing input config: " << res.description();
		return false;
	}

	pugi::xml_node root = doc.child("inputList");
	if (!root)
		return false;
	
	// Search for exact match guid + name
	for (pugi::xml_node item = root.child("inputConfig"); item; item = item.next_sibling("inputConfig"))
	{
		if (strcmp(config->getDeviceGUIDString().c_str(), item.attribute("deviceGUID").value()) == 0 && strcmp(config->getDeviceName().c_str(), item.attribute("deviceName").value()) == 0)
		{
			configNode = item;
			break;
		}
	}

	// Search for guid + name match but test guid without the new crc16 bytes (SDL2 2.26)
	// Cf. https://github.com/libsdl-org/SDL/commit/c1e087394020a8cb9d2a04a1eabbcc23a6a5b20d
	if (!configNode)
	{
		for (pugi::xml_node item = root.child("inputConfig"); item; item = item.next_sibling("inputConfig"))
		{
			// Remove CRC-16 encoding from SDL2 2.26+ guids
			std::string guid = config->getDeviceGUIDString();
			for (int i = 4; i < 8 && i < guid.length(); i++)
				guid[i] = '0';

			if (guid != config->getDeviceGUIDString())
			if (strcmp(guid.c_str(), item.attribute("deviceGUID").value()) == 0 && strcmp(config->getDeviceName().c_str(), item.attribute("deviceName").value()) == 0)
			{
				configNode = item;
				break;
			}
		}
	}

	// Search for exact match guid
	if (!configNode && allowApproximate)
	{
		for (pugi::xml_node item = root.child("inputConfig"); item; item = item.next_sibling("inputConfig"))
		{
			if (strcmp(config->getDeviceGUIDString().c_str(), item.attribute("deviceGUID").value()) == 0)
			{
				LOG(LogInfo) << "Approximative device found using guid=" << configNode.attribute("deviceGUID").value() << " name=" << configNode.attribute("deviceName").value() << ")";
				configNode = item;
				break;
			}
		}
	}

	// Search for name match
	if (!configNode && allowApproximate)
	{
		for (pugi::xml_node item = root.child("inputConfig"); item; item = item.next_sibling("inputConfig"))
		{
			if (strcmp(config->getDeviceName().c_str(), item.attribute("deviceName").value()) == 0)
			{
				LOG(LogInfo) << "Approximative device found using guid=" << configNode.attribute("deviceGUID").value() << " name=" << configNode.attribute("deviceName").value() << ")";
				configNode = item;
				break;
			}
		}
	}

	if (!configNode)
		return false;

	config->loadFromXML(configNode);
	return true;
}

static std::map<std::string, std::string> _sdlToEsMapping =
{
#ifdef INVERTEDINPUTCONFIG
   { "a",             "a" },
   { "b",             "b" },
#else
   { "a",             "b" },
   { "b",             "a" },
#endif
   { "x",             "y" },
   { "y",             "x" },
   { "back",          "select" },
   { "start",         "start" },
   { "leftshoulder",  "pageup" },
   { "rightshoulder", "pagedown" },
   { "dpup",          "up" },
   { "dpright",       "right" },
   { "dpdown",        "down" },
   { "dpleft",        "left" },
   { "leftx",         "joystick1left" },
   { "lefty",         "joystick1up" },
   { "rightx",        "joystick2left" },
   { "righty",        "joystick2up" },
   { "lefttrigger",   "l2" },
   { "righttrigger",  "r2" },
   { "leftstick",     "l3" },
   { "rightstick",    "r3" }
   // { "guide",         "hotkey" } // Better take select for hotkey ?
};

bool InputManager::loadFromSdlMapping(InputConfig* config, const std::string& mapping)
{	
	bool isConfigured = false;

	auto mapArray = Utils::String::split(mapping, ',', true);
	for (auto tt : mapArray)
	{
		size_t pos = tt.find(':');
		if (pos == std::string::npos) 
			continue;

		std::string key = tt.substr(0, pos);
		std::string value = tt.substr(pos + 1);

		auto inputName = _sdlToEsMapping.find(key);
		if (inputName == _sdlToEsMapping.cend())
		{
			LOG(LogError) << "[InputDevice] Unknown mapping: " << key;
			continue;
		}

		int valueSign = 1;
		int idx = 0;
				
		switch (value[idx])
		{
		case '-': 
			valueSign = -1;
			idx++;
			break;
		case '+': 
			idx++;
			break;
		}

		Input input;
		input.device = config->getDeviceId();
		input.type = InputType::TYPE_COUNT;
		input.configured = false;		

		switch (value[idx])
		{
		case 'a':
			{
				input.type = InputType::TYPE_AXIS;
				input.id = Utils::String::toInteger(value.substr(idx + 1));				
				input.value = 1 * valueSign;
				input.configured = true;

				// Invert directions for these mappings
				if (inputName->second == "joystick1up" || inputName->second == "joystick1left" || inputName->second == "joystick2up" || inputName->second == "joystick2left")
					input.value *= -1;
			}
			break;
		case 'b':
			{
				input.type = InputType::TYPE_BUTTON;
				input.id = Utils::String::toInteger(value.substr(idx + 1));
				input.value = 1;
				input.configured = true;
			}
			break;
		case 'h':
			{
				auto hatIds = Utils::String::split(value.substr(idx + 1), '.', true);
				if (hatIds.size() > 1)
				{
					input.type = InputType::TYPE_HAT;
					input.id = Utils::String::toInteger(hatIds[0]);
					input.value = Utils::String::toInteger(hatIds[1]);
					input.configured = true;
				}
			}
			break;
		}

		if (input.configured)
		{
			// BATOCERA : Uncomment the next line using a patch to compute the code
			// input.computeCode();
			
			config->mapInput(inputName->second, input);
			isConfigured = true;
		}
	}

	if (isConfigured)
	{
		// Use select button if hotkey is undefined
		Input tmp;
		if (!config->getInputByName("hotkey", &tmp))
			if (config->getInputByName("select", &tmp))
				config->mapInput("hotkey", tmp);
	}

	return isConfigured;
}

bool InputManager::loadInputConfig(InputConfig* config)
{
	std::string path = getConfigPath();

#if WIN32
	// Find exact device
	if (tryLoadInputConfig(path, config, false))
		return true;
#else
	// Find exact device
	if (tryLoadInputConfig(path, config, false))
		return true;

	// Find system exact device
	std::string sharedPath = Paths::getEmulationStationPath() + "/es_input.cfg";
	if (tryLoadInputConfig(sharedPath, config, false))
		return true;

	// Find user Approximative device
	if (tryLoadInputConfig(path, config, true))
		return true;

	// Find system Approximative device
	if (tryLoadInputConfig(sharedPath, config, true))
		return true;
#endif

	return false;
}

//used in an "emergency" where no keyboard config could be loaded from the inputmanager config file
//allows the user to select to reconfigure in menus if this happens without having to delete es_input.cfg manually
void InputManager::loadDefaultKBConfig()
{
	InputConfig* cfg = getInputConfigByDevice(DEVICE_KEYBOARD);

	cfg->clear();
	cfg->mapInput("up", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_UP, 1, true));
	cfg->mapInput("down", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_DOWN, 1, true));
	cfg->mapInput("left", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_LEFT, 1, true));
	cfg->mapInput("right", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RIGHT, 1, true));

	cfg->mapInput(BUTTON_OK, Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RETURN, 1, true));
	cfg->mapInput(BUTTON_BACK, Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_ESCAPE, 1, true));
	cfg->mapInput("start", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_F1, 1, true));
	cfg->mapInput("select", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_F2, 1, true));

	cfg->mapInput("pageup", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RIGHTBRACKET, 1, true));
	cfg->mapInput("pagedown", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_LEFTBRACKET, 1, true));
}

void InputManager::loadDefaultGunConfig()
{
#ifdef HAVE_UDEV
	InputConfig* cfg = getInputConfigByDevice(DEVICE_GUN);

	cfg->clear();
	cfg->mapInput("up",    Input(DEVICE_GUN, TYPE_BUTTON, BTN_5, 1, true));
	cfg->mapInput("down",  Input(DEVICE_GUN, TYPE_BUTTON, BTN_6, 1, true));
	cfg->mapInput("left",  Input(DEVICE_GUN, TYPE_BUTTON, BTN_7, 1, true));
	cfg->mapInput("right", Input(DEVICE_GUN, TYPE_BUTTON, BTN_8, 1, true));
	cfg->mapInput("start",  Input(DEVICE_GUN, TYPE_BUTTON, BTN_MIDDLE, 1, true));
	cfg->mapInput("select", Input(DEVICE_GUN, TYPE_BUTTON, BTN_1, 1, true));
#endif
}

void InputManager::writeDeviceConfig(InputConfig* config)
{
	assert(initialized());

	std::string path = getConfigPath();

	pugi::xml_document doc;

	if (Utils::FileSystem::exists(path))
	{
		// merge files
		pugi::xml_parse_result result = doc.load_file(WINSTRINGW(path).c_str());
		if (!result)
		{
			LOG(LogError) << "Error parsing input config: " << result.description();
		}
		else
		{
			// successfully loaded, delete the old entry if it exists
			pugi::xml_node root = doc.child("inputList");
			if (root)
			{				
				pugi::xml_node oldEntry(NULL);
				for (pugi::xml_node item = root.child("inputConfig"); item; item = item.next_sibling("inputConfig")) 
				{
					if (strcmp(config->getDeviceGUIDString().c_str(), item.attribute("deviceGUID").value()) == 0
#if !WIN32
						&& strcmp(config->getDeviceName().c_str(), item.attribute("deviceName").value()) == 0
#endif
						) 
					{
						oldEntry = item;
						break;
					}
				}
				if (oldEntry)
					root.remove_child(oldEntry);
			}
		}
	}

	pugi::xml_node root = doc.child("inputList");
	if(!root)
		root = doc.append_child("inputList");

	config->writeToXML(root);
	doc.save_file(WINSTRINGW(path).c_str());
        
	/* create a es_last_input.cfg so that people can easily share their config */
	pugi::xml_document lastdoc;
	pugi::xml_node lastroot = lastdoc.append_child("inputList");
	config->writeToXML(lastroot);
	std::string lastpath = getTemporaryConfigPath();
	lastdoc.save_file(WINSTRINGW(lastpath).c_str());

	Scripting::fireEvent("config-changed");
	Scripting::fireEvent("controls-changed");
	
	// execute any onFinish commands and re-load the config for changes
	doOnFinish();
	loadInputConfig(config);
}

void InputManager::doOnFinish()
{
	if (!initialized())
		return;

	std::string path = getConfigPath();
	pugi::xml_document doc;

	if(Utils::FileSystem::exists(path))
	{
		pugi::xml_parse_result result = doc.load_file(WINSTRINGW(path).c_str());
		if(!result)
		{
			LOG(LogError) << "Error parsing input config: " << result.description();
		}
		else
		{
			pugi::xml_node root = doc.child("inputList");
			if(root)
			{
				root = root.find_child_by_attribute("inputAction", "type", "onfinish");
				if(root)
				{
					for(pugi::xml_node command = root.child("command"); command;
							command = command.next_sibling("command"))
					{
						std::string tocall = command.text().get();

						LOG(LogInfo) << "	" << tocall;
						std::cout << "==============================================\ninput config finish command:\n";						
						int exitCode = Utils::Platform::ProcessStartInfo(tocall).run();
						std::cout << "==============================================\n";

						if(exitCode != 0)
						{
							LOG(LogWarning) << "...launch terminated with nonzero exit code " << exitCode << "!";
						}
					}
				}
			}
		}
	}
}

std::string InputManager::getConfigPath()
{
	return Paths::getUserEmulationStationPath() + "/es_input.cfg";
}

std::string InputManager::getTemporaryConfigPath()
{
	return Paths::getUserEmulationStationPath() + "/es_last_input.cfg";
}

bool InputManager::initialized() const
{
	return mKeyboardInputConfig != NULL;
}

int InputManager::getNumConfiguredDevices()
{
	int num = 0;

	for(auto it = mInputConfigs.cbegin(); it != mInputConfigs.cend(); it++)
		if(it->second && it->second->isConfigured())
			num++;

	if (mKeyboardInputConfig && mKeyboardInputConfig->isConfigured())
		num++;

	if (mCECInputConfig && mCECInputConfig->isConfigured())
		num++;

	// Mouse input is hardcoded & not configurable with es_input.cfg
	// Gun input is hardcoded & not configurable with es_input.cfg

	return num;
}

void InputManager::computeLastKnownPlayersDeviceIndexes() 
{
	std::map<int, InputConfig*> playerJoysticks = computePlayersConfigs();

	m_lastKnownPlayersDeviceIndexes.clear();
	for (int player = 0; player < MAX_PLAYERS; player++)
	{
		auto playerJoystick = playerJoysticks[player];
		if (playerJoystick != nullptr)
		{
			PlayerDeviceInfo dev;
			dev.index = playerJoystick->getDeviceIndex();
			dev.batteryLevel = playerJoystick->getBatteryLevel();
			dev.isWheel = playerJoystick->isWheel();

			m_lastKnownPlayersDeviceIndexes[player] = dev;
		}
	}
}

std::map<int, InputConfig*> InputManager::computePlayersConfigs()
{
	std::unique_lock<std::mutex> lock(mJoysticksLock);

	// 1. Recuperer les configurated
	std::vector<InputConfig *> availableConfigured;
	for (auto conf : mInputConfigs)
		if (conf.second != nullptr && conf.second->isConfigured())
			availableConfigured.push_back(conf.second);

	// sort available configs
	std::sort(availableConfigured.begin(), availableConfigured.end(), [](InputConfig * a, InputConfig * b) -> bool { return a->getSortDevicePath() < b->getSortDevicePath(); });

	// 2. Pour chaque joueur verifier si il y a un configurated
	// associer le input au joueur
	// enlever des disponibles
	std::map<int, InputConfig*> playerJoysticks;

	// First loop, search for PATH. Ultra High Priority
	for (int player = 0; player < MAX_PLAYERS; player++)
	{
		std::string playerConfigPath = Settings::getInstance()->getString(Utils::String::format("INPUT P%iPATH", player + 1));
		if (!playerConfigPath.empty())
		{
			for (auto it1 = availableConfigured.begin(); it1 != availableConfigured.end(); ++it1)
			{
				InputConfig* config = *it1;
				if (playerConfigPath == config->getSortDevicePath())
				{
					availableConfigured.erase(it1);
					playerJoysticks[player] = config;
					break;
				}
			}
		}
	}

	// First loop, search for GUID + NAME. High Priority
	for (int player = 0; player < MAX_PLAYERS; player++) 
	{
		if (playerJoysticks.find(player) != playerJoysticks.cend())
			continue;

		std::string playerConfigName = Settings::getInstance()->getString(Utils::String::format("INPUT P%iNAME", player + 1));
		std::string playerConfigGuid = Settings::getInstance()->getString(Utils::String::format("INPUT P%iGUID", player + 1));

		for (auto it1 = availableConfigured.begin(); it1 != availableConfigured.end(); ++it1)
		{
			InputConfig* config = *it1;
			if (playerConfigName == config->getDeviceName() && playerConfigGuid == config->getDeviceGUIDString())
			{
				availableConfigured.erase(it1);
				playerJoysticks[player] = config;
				break;
			}
		}
	}

	// Second loop, search for NAME. Low Priority
	for (int player = 0; player < MAX_PLAYERS; player++) 
	{
		if (playerJoysticks.find(player) != playerJoysticks.cend())
			continue;

		std::string playerConfigName = Settings::getInstance()->getString(Utils::String::format("INPUT P%dNAME", player + 1));

		for (auto it1 = availableConfigured.begin(); it1 != availableConfigured.end(); ++it1)
		{
			InputConfig * config = *it1;
			if (playerConfigName == config->getDeviceName())
			{
				availableConfigured.erase(it1);
				playerJoysticks[player] = config;
				break;
			}
		}
	}

	// Last loop, search for free controllers for remaining players.
	for (int player = 0; player < MAX_PLAYERS; player++) 
	{
		if (playerJoysticks.find(player) != playerJoysticks.cend())
			continue;

		// si aucune config a été trouvé pour le joueur, on essaie de lui filer un libre
		for (auto it1 = availableConfigured.begin(); it1 != availableConfigured.end(); ++it1)
		{
			playerJoysticks[player] = *it1;
			availableConfigured.erase(it1);
			break;
		}
	}

	// in case of hole (player 1 missing, but player 4 set, fill the holes with last players joysticks)
	for (int player = 0; player < MAX_PLAYERS; player++) 
	{
		if (playerJoysticks.find(player) != playerJoysticks.cend())
			continue;

		for (int repplayer = MAX_PLAYERS; repplayer > player; repplayer--) 
		{
			if (playerJoysticks[player] == NULL && playerJoysticks[repplayer] != NULL) 
			{
				playerJoysticks[player] = playerJoysticks[repplayer];
				playerJoysticks[repplayer] = NULL;
			}
		}		
	}

	for (int player = 0; player < MAX_PLAYERS; player++)
	{
		if (playerJoysticks.find(player) != playerJoysticks.cend())
			continue;

		LOG(LogInfo) << "computePlayersConfigs : Player " << player << " => " << playerJoysticks[player]->getDevicePath();
	}

	return playerJoysticks;
}

std::string InputManager::configureEmulators() {
  std::map<int, InputConfig*> playerJoysticks = computePlayersConfigs();
  std::stringstream command;

  for (int player = 0; player < MAX_PLAYERS; player++) {
    InputConfig * playerInputConfig = playerJoysticks[player];
    if(playerInputConfig != NULL){
      command <<  "-p" << player+1 << "index "      << playerInputConfig->getDeviceIndex();
      command << " -p" << player+1 << "guid "       << playerInputConfig->getDeviceGUIDString();
#if WIN32
	  command << " -p" << player+1 << "path \""     << playerInputConfig->getDevicePath() << "\"";
#endif
      command << " -p" << player+1 << "name \""     << playerInputConfig->getDeviceName() << "\"";
      command << " -p" << player+1 << "nbbuttons "  << playerInputConfig->getDeviceNbButtons();
      command << " -p" << player+1 << "nbhats "     << playerInputConfig->getDeviceNbHats();
      command << " -p" << player+1 << "nbaxes "     << playerInputConfig->getDeviceNbAxes();
      command << " ";
    }
  }
  LOG(LogInfo) << "Configure emulators command : " << command.str().c_str();
  return command.str();
}

void InputManager::updateBatteryLevel(int id, const std::string& device, const std::string& devicePath, int level)
{
	bool changed = false;

	mJoysticksLock.lock();

	for (auto joy : mJoysticks) 
	{
		InputConfig* config = getInputConfigByDevice(joy.first);
		if (config != NULL && config->isConfigured())
		{
			if (!devicePath.empty())
			{
				if (Utils::String::compareIgnoreCase(config->getDevicePath(), devicePath) == 0)
				{
					config->updateBatteryLevel(level);
					changed = true;
				}
			}
			else
			{
				if (Utils::String::compareIgnoreCase(config->getDeviceGUIDString(), device) == 0)
				{
					config->updateBatteryLevel(level);
					changed = true;
				}
			}
		}
	}

	mJoysticksLock.unlock();

	if (changed)
		computeLastKnownPlayersDeviceIndexes();
}

std::vector<InputConfig*> InputManager::getInputConfigs()
{
	std::vector<InputConfig*> ret;

	std::map<int, InputConfig*> playerJoysticks = computePlayersConfigs();

	for (int player = 0; player < MAX_PLAYERS; player++)
	{
		auto playerJoystick = playerJoysticks[player];
		if (playerJoystick != nullptr && playerJoystick->isConfigured())
			ret.push_back(playerJoystick);
	}

	return ret;
}

void InputManager::updateGuns(Window* window)
{
	if (mGunManager)
		mGunManager->updateGuns(window);
}

void InputManager::sendMouseClick(Window* window, int button)
{
	window->input(getInputConfigByDevice(DEVICE_MOUSE), Input(DEVICE_MOUSE, TYPE_BUTTON, button, true, false));
	window->input(getInputConfigByDevice(DEVICE_MOUSE), Input(DEVICE_MOUSE, TYPE_BUTTON, button, false, false));
}
