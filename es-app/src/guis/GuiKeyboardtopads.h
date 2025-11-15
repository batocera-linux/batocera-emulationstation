#pragma once
#ifndef ES_APP_GUIS_GUI_KEYBOARDTOPADS_H
#define ES_APP_GUIS_GUI_KEYBOARDTOPADS_H

#include "GuiSettings.h"

struct Keyboardtopad;

class GuiKeyboardtopads : public GuiSettings
{
public:
	GuiKeyboardtopads(Window* window, Keyboardtopad ktp);
	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:	
	void loadActivePage(const std::string& device_path);
	void loadPlayerPage(int n, const std::string& device_path);
	void loadHotkeysPage(const std::string& device_path);

  	KeyboardtopadDevice* getJoystickDevice(int n);
  	KeyboardtopadDevice* getHotkeyDevice();
  	int countJoystickDevice(const std::string& type);

  	std::string getNameForInput(KeyboardtopadDevice* device, std::string value);
  	void declareEvKey(Window* window, const std::string& device_path, const std::function<void(std::string)>& func);

  	void setDeviceValue(KeyboardtopadDevice* ktp, std::string key, std::string value);

	Keyboardtopad m_ktp;
	std::vector<KeyboardtopadDevice> m_devices;
	bool m_need_save;
};

#endif // ES_APP_GUIS_GUI_KEYBOARDTOPADS_H
