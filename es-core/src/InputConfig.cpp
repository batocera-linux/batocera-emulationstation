#include "InputConfig.h"

#include "Log.h"
#include <pugixml/src/pugixml.hpp>
#include "Settings.h"
#include "utils/StringUtil.h"

#ifdef HAVE_UDEV
#include <libudev.h>
#endif


//some util functions
std::string inputTypeToString(InputType type)
{
	switch(type)
	{
	case TYPE_AXIS:
		return "axis";
	case TYPE_BUTTON:
		return "button";
	case TYPE_HAT:
		return "hat";
	case TYPE_KEY:
		return "key";
	case TYPE_CEC_BUTTON:
		return "cec-button";
	default:
		return "error";
	}
}

InputType stringToInputType(const std::string& type)
{
	if(type == "axis")
		return TYPE_AXIS;
	if(type == "button")
		return TYPE_BUTTON;
	if(type == "hat")
		return TYPE_HAT;
	if(type == "key")
		return TYPE_KEY;
	if(type == "cec-button")
		return TYPE_CEC_BUTTON;
	return TYPE_COUNT;
}


std::string toLower(std::string str)
{
	for(unsigned int i = 0; i < str.length(); i++)
	{
		str[i] = (char)tolower(str[i]);
	}

	return str;
}
//end util functions

InputConfig::InputConfig(int deviceId, int deviceIndex, const std::string& deviceName, const std::string& deviceGUID, int deviceNbButtons, int deviceNbHats, int deviceNbAxes, const std::string& devicePath) 
	: mDeviceId(deviceId), mDeviceIndex(deviceIndex), mDeviceName(deviceName), mDeviceGUID(deviceGUID), mDeviceNbButtons(deviceNbButtons), mDeviceNbHats(deviceNbHats), mDeviceNbAxes(deviceNbAxes), mDevicePath(devicePath)
{
	mBatteryLevel = -1;
	mIsWheel = false;
#ifdef HAVE_UDEV
	mIsWheel = isWheel(devicePath);
#endif
}

#ifdef HAVE_UDEV
bool InputConfig::isWheel(const std::string path) {
	struct udev *udev;
	struct udev_list_entry *devs = NULL;
	struct udev_list_entry *item = NULL;
	bool res = false;

	udev = udev_new();
	if (udev != NULL)
	  {
	    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
	    udev_enumerate_add_match_property(enumerate, "ID_INPUT_WHEEL", "1");
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
		  std::string strdevnode = devnode;
		  if(strdevnode == path) {
		    res = true;
		  }
	    	}
	    	udev_device_unref(dev);
	      }
	    udev_unref(udev);
	  }
	return res;
}
#endif

void InputConfig::clear()
{
	mNameMap.clear();
}

bool InputConfig::isConfigured()
{
	return mNameMap.size() > 0;
}

void InputConfig::mapInput(const std::string& name, Input input)
{
	mNameMap[toLower(name)] = input;
}

void InputConfig::unmapInput(const std::string& name)
{
	auto it = mNameMap.find(toLower(name));
	if(it != mNameMap.cend())
		mNameMap.erase(it);
}

bool InputConfig::getInputByName(const std::string& name, Input* result)
{
	auto it = mNameMap.find(toLower(name));
	if(it != mNameMap.cend())
	{
		*result = it->second;
		return true;
	}

	return false;
}

bool InputConfig::isMappedTo(const std::string& name, Input input, bool reversedAxis)
{
	Input comp;
	if (!getInputByName(name, &comp))
		return false;

	if (reversedAxis)
		comp.value *= -1;

	if (comp.configured && comp.type == input.type && comp.id == input.id)
	{
		if (comp.type == TYPE_HAT)
			return (input.value == 0 || input.value & comp.value);

		if (comp.type == TYPE_AXIS)
			return input.value == 0 || comp.value == input.value;

		return true;		
	}

	return false;
}

bool InputConfig::isMappedLike(const std::string& name, Input input)
{
	if (name == "left")
		return isMappedTo("left", input) || isMappedTo("joystick1left", input);

	if (name == "right")
		return isMappedTo("right", input) || isMappedTo("joystick1left", input, true);

	if (name == "up")
		return isMappedTo("up", input) || isMappedTo("joystick1up", input);

	if (name == "down")
		return isMappedTo("down", input) || isMappedTo("joystick1up", input, true);

	return isMappedTo(name, input);
}

std::vector<std::string> InputConfig::getMappedTo(Input input)
{
	std::vector<std::string> maps;

	typedef std::map<std::string, Input>::const_iterator it_type;
	for (it_type iterator = mNameMap.cbegin(); iterator != mNameMap.cend(); iterator++)
	{
		Input chk = iterator->second;

		if (!chk.configured)
			continue;

		if (chk.device == input.device && chk.type == input.type && chk.id == input.id)
		{
			if (chk.type == TYPE_HAT)
			{
				if (input.value == 0 || input.value & chk.value)
					maps.push_back(iterator->first);

				continue;
			}

			if (input.type == TYPE_AXIS)
			{
				if (input.value == 0 || chk.value == input.value)
					maps.push_back(iterator->first);
			}
			else
				maps.push_back(iterator->first);
		}
	}

	return maps;
}

void InputConfig::loadFromXML(pugi::xml_node& node)
{
	clear();

	for(pugi::xml_node input = node.child("input"); input; input = input.next_sibling("input"))
	{
		std::string name = input.attribute("name").as_string();
		std::string type = input.attribute("type").as_string();
		InputType typeEnum = stringToInputType(type);

		if(typeEnum == TYPE_COUNT)
		{
			LOG(LogError) << "InputConfig load error - input of type \"" << type << "\" is invalid! Skipping input \"" << name << "\".\n";
			continue;
		}

		int id = input.attribute("id").as_int();
		int value = input.attribute("value").as_int();

		if(value == 0)
			LOG(LogWarning) << "WARNING: InputConfig value is 0 for " << type << " " << id << "!\n";

		mNameMap[toLower(name)] = Input(mDeviceId, typeEnum, id, value, true);
	}
}

void InputConfig::writeToXML(pugi::xml_node& parent)
{
	pugi::xml_node cfg = parent.append_child("inputConfig");

	if(mDeviceId == DEVICE_KEYBOARD)
	{
		cfg.append_attribute("type") = "keyboard";
		cfg.append_attribute("deviceName") = "Keyboard";
	}
	else if(mDeviceId == DEVICE_CEC)
	{
		cfg.append_attribute("type") = "cec";
		cfg.append_attribute("deviceName") = "CEC";
	}
	else
	{
		cfg.append_attribute("type") = "joystick";
		cfg.append_attribute("deviceName") = mDeviceName.c_str();
	}

	cfg.append_attribute("deviceGUID") = mDeviceGUID.c_str();

	typedef std::map<std::string, Input>::const_iterator it_type;
	for(it_type iterator = mNameMap.cbegin(); iterator != mNameMap.cend(); iterator++)
	{
		if(!iterator->second.configured)
			continue;

		pugi::xml_node input = cfg.append_child("input");
		input.append_attribute("name") = iterator->first.c_str();
		input.append_attribute("type") = inputTypeToString(iterator->second.type).c_str();
		input.append_attribute("id").set_value(iterator->second.id);
		input.append_attribute("value").set_value(iterator->second.value);
	}
}

static char ABUTTON[2] = "a";
static char BBUTTON[2] = "b";

char* BUTTON_OK = ABUTTON;
char* BUTTON_BACK = BBUTTON;

std::string InputConfig::buttonLabel(const std::string& button)
{
#ifdef INVERTEDINPUTCONFIG
	if (!Settings::getInstance()->getBool("InvertButtons"))
	{
		if (button == "a")
			return "b";
		else if (button == "b")
			return "a";
	}
#endif

	return button;
}

std::string InputConfig::buttonDisplayName(const std::string& button)
{
#ifdef INVERTEDINPUTCONFIG
	if (!Settings::getInstance()->getBool("InvertButtons"))
	{
		return button == "a" ? "SOUTH" : button == "b" ? "EAST" : button;
	}
#endif

	return button == "a" ? "EAST" : button == "b" ? "SOUTH" : button;
}

std::string InputConfig::buttonImage(const std::string& button)
{
#ifdef INVERTEDINPUTCONFIG
	if (!Settings::getInstance()->getBool("InvertButtons"))
	{
		if (button == "a")
			return ":/help/buttons_south.svg";		
		if (button == "b")
			return ":/help/buttons_east.svg";
	}
#endif
	if (button == "a")
		return ":/help/buttons_east.svg";
	if (button == "b")
		return ":/help/buttons_south.svg";
	if (button == "x")
		return ":/help/buttons_north.svg";
	if (button == "y")
		return ":/help/buttons_west.svg";
	
	return button;
}

void InputConfig::AssignActionButtons()
{
	bool invertButtons = Settings::getInstance()->getBool("InvertButtons");
	
#ifdef INVERTEDINPUTCONFIG
	BUTTON_OK = invertButtons ? BBUTTON : ABUTTON;
	BUTTON_BACK = invertButtons ? ABUTTON : BBUTTON;
#else
	BUTTON_OK = invertButtons ? ABUTTON : BBUTTON;
	BUTTON_BACK = invertButtons ? BBUTTON : ABUTTON;
#endif
}

std::string InputConfig::getSortDevicePath()
{
	if (Utils::String::startsWith(Utils::String::toUpper(mDevicePath), "USB\\"))
	{
		auto lastSplit = mDevicePath.rfind("\\");
		if (lastSplit != std::string::npos)
		{
			auto lastAnd = mDevicePath.rfind("&");
			if (lastAnd != std::string::npos && lastAnd > lastSplit)
			{
				// Keep only last part which is probably some kind of MAC address
				// Ex : USB\VID_045E&PID_02FF&IG_00\01&00&0000B7234380ED7E -> USB\VID_045E&PID_02FF&IG_00\0000B7234380ED7E
				// 01 is some kind of a system device index, but can change upon reboot & we sometimes have 00&00&0000B7234380ED7E, and the order changes !
				// Sort only with 0000B7234380ED7E ignoring 01&00 part

				std::string ret = mDevicePath;
				ret = ret.substr(0, lastSplit + 1) + ret.substr(lastAnd + 1);				
				return ret;
			}
		}
	}

	return mDevicePath;
}

