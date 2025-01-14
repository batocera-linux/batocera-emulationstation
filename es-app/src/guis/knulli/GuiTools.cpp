#include "guis/knulli/GuiTools.h"
#include "guis/knulli/GuiPowerManagementSettings.h"
#include "guis/knulli/GuiRgbSettings.h"
#include "guis/knulli/Pico8Installer.h"
#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiSettings.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "SystemConf.h"
#include "ApiSystem.h"
#include "InputManager.h"
#include "AudioManager.h"
#include <SDL_events.h>
#include <algorithm>
#include "utils/Platform.h"
#include "BoardCheck.h"

const std::vector<std::string> SUPPORTED_RGB_BOARDS = {"rg40xx-h", "rg40xx-v", "rg-cubexx", "trimui-smart-pro", "trimui-brick"};

GuiTools::GuiTools(Window* window) : GuiSettings(window, _("TOOLS").c_str())
{
	addGroup(_("DEVICE SETTINGS"));
	addEntry(_("POWER MANAGEMENT"), true, [this] { openPowerManagementSettings(); });
	if(BoardCheck::isBoard(SUPPORTED_RGB_BOARDS)) {
		addEntry(_("RGB LED SETTINGS"), true, [this] { openRgbLedSettings(); });
	}
	addGroup(_("THIRD PARTY SOFTWARE"));
	addEntry(_("INSTALL PICO-8"), true, [this] { installPico8(); });
}


void GuiTools::openPowerManagementSettings()
{
	mWindow->pushGui(new GuiPowerManagementSettings(mWindow));
}

void GuiTools::openRgbLedSettings()
{
	mWindow->pushGui(new GuiRgbSettings(mWindow));
}

void GuiTools::installPico8()
{
	int result = Pico8Installer::install();
	if(result == 0) {
		mWindow->pushGui(new GuiMsgBox(mWindow, _("Native Pico-8 was successfully installed."), _("OK"), nullptr));
	} else if(result == 1) {
		mWindow->pushGui(new GuiMsgBox(mWindow, "Unable to install: An unknown error occurred. If the error persists, try installing Pico-8 manually.", "OK", nullptr));
	} else if(result == 2) {
		mWindow->pushGui(new GuiMsgBox(mWindow, "Unable to install: Pico-8 installer files missing. Please download the Raspberry Pi version of Pico-8 and place the ZIP file in the roms/pico8 folder and try again.", "OK", nullptr));
	}
}