#pragma once
#ifndef ES_CORE_INPUT_CONFIG_H
#define ES_CORE_INPUT_CONFIG_H

#include <CECInput.h>
#include <SDL_joystick.h>
#include <SDL_keyboard.h>
#include <map>
#include <sstream>
#include <vector>

#ifdef WIN32
#define INVERTEDINPUTCONFIG
#endif

namespace pugi { class xml_node; }

#define DEVICE_KEYBOARD -1
#define DEVICE_CEC      -2
#define DEVICE_MOUSE	-3
#define DEVICE_GUN      -4

#define MAX_PLAYERS 8

extern char* BUTTON_OK;
extern char* BUTTON_BACK;

enum InputType
{
	TYPE_AXIS,
	TYPE_BUTTON,
	TYPE_HAT,
	TYPE_KEY,
	TYPE_CEC_BUTTON,
	TYPE_COUNT
};

struct Input
{
public:
	int device;
	InputType type;
	int id;
	int value;
	bool configured;

	Input()
	{
		device = DEVICE_KEYBOARD;
		configured = false;
		id = -1;
		value = -999;
		type = TYPE_COUNT;
	}

	Input(int dev, InputType t, int i, int val, bool conf) : device(dev), type(t), id(i), value(val), configured(conf)
	{
	}

	std::string getHatDir(int val)
	{
		if(val & SDL_HAT_UP)
			return "up";
		else if(val & SDL_HAT_DOWN)
			return "down";
		else if(val & SDL_HAT_LEFT)
			return "left";
		else if(val & SDL_HAT_RIGHT)
			return "right";
		return "neutral?";
	}

	std::string getCECButtonName(int keycode)
	{
		return CECInput::getKeyCodeString(keycode);
	}

	std::string string()
	{
		std::stringstream stream;
		switch(type)
		{
			case TYPE_BUTTON:
				stream << "Button " << id;
				break;
			case TYPE_AXIS:
				stream << "Axis " << id << (value > 0 ? "+" : "-");
				break;
			case TYPE_HAT:
				stream << "Hat " << id << " " << getHatDir(value);
				break;
			case TYPE_KEY:
				stream << "Key " << SDL_GetKeyName((SDL_Keycode)id);
				break;
			case TYPE_CEC_BUTTON:
				stream << "CEC-Button " << getCECButtonName(id);
				break;
			default:
				stream << "Input to string error";
				break;
		}

		return stream.str();
	}
};

class InputConfig
{
public:
	InputConfig(int deviceId, int deviceIndex, const std::string& deviceName, const std::string& deviceGUID, int deviceNbButtons, int deviceNbHats, int deviceNbAxes, const std::string& devicePath = ""); 

	void clear();
	void mapInput(const std::string& name, Input input);
	void unmapInput(const std::string& name); // unmap all Inputs mapped to this name

	inline int getDeviceId() const { return mDeviceId; };
        
	inline int getDeviceIndex() const { return mDeviceIndex; }; 
	inline const std::string& getDeviceName() { return mDeviceName; }
	inline const std::string& getDeviceGUIDString() { return mDeviceGUID; }
	inline int getDeviceNbButtons() const { return mDeviceNbButtons; }; 
	inline int getDeviceNbHats() const { return mDeviceNbHats; }; 
	inline int getDeviceNbAxes() const { return mDeviceNbAxes; }; 
	inline int getBatteryLevel() const { return mBatteryLevel; };
  	inline int isWheel()         const { return mIsWheel;      };
	inline const std::string& getDevicePath() { return mDevicePath; };

	std::string getSortDevicePath();

	//Returns true if Input is mapped to this name, false otherwise.
	bool isMappedTo(const std::string& name, Input input, bool reversedAxis = false); 
	bool isMappedLike(const std::string& name, Input input);

	//Returns a list of names this input is mapped to.
	std::vector<std::string> getMappedTo(Input input);

	// Returns true if there is an Input mapped to this name, false otherwise.
	// Writes Input mapped to this name to result if true.
	bool getInputByName(const std::string& name, Input* result);

	void loadFromXML(pugi::xml_node& root);
	void writeToXML(pugi::xml_node& parent);

	bool isConfigured();

	static std::string buttonLabel(const std::string& button);
	static std::string buttonDisplayName(const std::string& button);	
	static std::string buttonImage(const std::string& button);

	void updateBatteryLevel(int level) { mBatteryLevel = level; }; 

private:
	std::map<std::string, Input> mNameMap;
	const int mDeviceId;
	const int mDeviceIndex; 
	const std::string mDeviceName;
	const std::string mDeviceGUID;
	const int mDeviceNbButtons; // number of buttons of the device 
	const int mDeviceNbHats;    // number of hats    of the device 
	const int mDeviceNbAxes;    // number of axes    of the device 
	std::string mDevicePath;

	int mBatteryLevel;
	bool mIsWheel;

#ifdef HAVE_UDEV
	static bool isWheel(const std::string path);
#endif

public:
	static void AssignActionButtons();
};

#endif // ES_CORE_INPUT_CONFIG_H
