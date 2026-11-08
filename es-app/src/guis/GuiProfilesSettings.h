#pragma once

#include "GuiSettings.h"

class Window;

class GuiProfilesSettings : public GuiSettings
{
public:
	GuiProfilesSettings(Window* window);

	// Is the hotkey double tap (and its help bar entry) active?  Setting "ProfilesHotkey": auto (the default: on as soon as a profile
	// has been created), on, or off.
	static bool isHotkeyEnabled();
};
