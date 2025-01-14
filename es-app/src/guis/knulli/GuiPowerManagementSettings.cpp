#include "guis/knulli/GuiPowerManagementSettings.h"
#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiSettings.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "SystemConf.h"
#include "ApiSystem.h"
#include "Scripting.h"
#include "InputManager.h"
#include "AudioManager.h"
#include <SDL_events.h>
#include <algorithm>
#include "utils/Platform.h"
#include "BoardCheck.h"

const std::vector<std::string> SUPPORTED_LID_BOARDS = {"rg40xx-sp"};

GuiPowerManagementSettings::GuiPowerManagementSettings(Window* window) : GuiSettings(window, _("POWER MANAGEMENT").c_str())
{

	addGroup(_("BATTERY SAVING"));

	// Battery save mode
	auto optionsBatterySaveMode = std::make_shared<OptionListComponent<std::string> >(mWindow, _("MODE"), false);

	std::string selectedBatteryMode = SystemConf::getInstance()->get("system.batterysaver.mode");
	if (selectedBatteryMode.empty())
		selectedBatteryMode = "dim";

	optionsBatterySaveMode->add(_("DIM"),            "dim", selectedBatteryMode == "dim");
	optionsBatterySaveMode->add(_("DISPLAY OFF"),    "dispoff", selectedBatteryMode == "dispoff");
	optionsBatterySaveMode->add(_("SUSPEND"),        "suspend", selectedBatteryMode == "suspend");
	optionsBatterySaveMode->add(_("SHUTDOWN"),       "shutdown", selectedBatteryMode == "shutdown");
	optionsBatterySaveMode->add(_("NONE"),           "none", selectedBatteryMode == "none");

	addWithLabel(_("MODE"), optionsBatterySaveMode);

	// Battery save mode timer (1-120 minutes in 1 minute steps)
	auto sliderBatterySaverTime = std::make_shared<SliderComponent>(mWindow, 1.f, 60.f, 1.f, "m");

	float selectedBatterySaverTimeSeconds = 120.f;
	std::string configuredBatterySaverTime = SystemConf::getInstance()->get("system.batterysaver.timer");
	if (!configuredBatterySaverTime.empty()) {
		selectedBatterySaverTimeSeconds = Utils::String::toFloat(configuredBatterySaverTime);
	}

	int selectedBatterySaverTimeMinutes = (int)Math::round(selectedBatterySaverTimeSeconds/60.f);

	sliderBatterySaverTime->setValue((float)(selectedBatterySaverTimeMinutes));
	addWithDescription(_("IDLE TIME"),_("Battery save mode is activated after idle time has passed without any input."), sliderBatterySaverTime);

	// Battery save extended mode
	auto optionsBatterySaveExtendedMode = std::make_shared<OptionListComponent<std::string> >(mWindow, _("EXTENDED MODE"), false);

	std::string selectedBatteryExtendedMode = SystemConf::getInstance()->get("system.batterysaver.extendedmode");
	if (selectedBatteryExtendedMode.empty())
		selectedBatteryExtendedMode = "suspend";

	optionsBatterySaveExtendedMode->add(_("SUSPEND"),        "suspend", selectedBatteryExtendedMode == "suspend");
	optionsBatterySaveExtendedMode->add(_("SHUTDOWN"),       "shutdown", selectedBatteryExtendedMode == "shutdown");
	optionsBatterySaveExtendedMode->add(_("NONE"),           "none", selectedBatteryExtendedMode == "none");

	addWithLabel(_("EXTENDED MODE"), optionsBatterySaveExtendedMode);

	// Battery save extended mode timer (1-120 minutes in 1 minute steps)
	auto sliderBatterySaverExtendedTime = std::make_shared<SliderComponent>(mWindow, 1.f, 60.f, 1.f, "m");

	float selectedBatterySaverExtendedTimeSeconds = 120.f;
	std::string configuredBatterySaverExtendedTime = SystemConf::getInstance()->get("system.batterysaver.extendedtimer");
	if (!configuredBatterySaverExtendedTime.empty()) {
		selectedBatterySaverExtendedTimeSeconds = Utils::String::toFloat(configuredBatterySaverExtendedTime);
	}

	int selectedBatterySaverExtendedTimeMinutes = (int)Math::round(selectedBatterySaverExtendedTimeSeconds/60.f);

	sliderBatterySaverExtendedTime->setValue((float)(selectedBatterySaverExtendedTimeMinutes));
	addWithDescription(_("EXTENDED IDLE TIME"),_("Secondary timer which starts after initial idle time has passed without any input."), sliderBatterySaverExtendedTime);

	// Aggressive battery save mode toggle
	auto aggressiveBatterySaveMode = std::make_shared<SwitchComponent>(mWindow);

	aggressiveBatterySaveMode->setState(SystemConf::getInstance()->getBool("system.batterysaver.aggressive"));
	addWithLabel(_("ENABLE AGGRESSIVE MODE"), aggressiveBatterySaveMode);

	// Lid close mode
	auto optionsLidCloseMode = std::make_shared<OptionListComponent<std::string> >(mWindow, _("LID CLOSE MODE"), false);
	// TODO: do not even instantiate if lid is not supported
	if (BoardCheck::isBoard(SUPPORTED_LID_BOARDS)) {
		std::string selectedLidCloseMode = SystemConf::getInstance()->get("system.lid");
		if (selectedLidCloseMode.empty())
			selectedLidCloseMode = "suspend";

		optionsLidCloseMode->add(_("DISPLAY OFF"),    "dispoff", selectedLidCloseMode == "dispoff");
		optionsLidCloseMode->add(_("SUSPEND"),        "suspend", selectedLidCloseMode == "suspend");
		optionsLidCloseMode->add(_("SHUTDOWN"),       "shutdown", selectedLidCloseMode == "shutdown");

		addWithLabel(_("LID CLOSE MODE"), optionsLidCloseMode);
	}

	addSaveFunc([this, optionsBatterySaveMode, sliderBatterySaverTime, optionsBatterySaveExtendedMode, sliderBatterySaverExtendedTime, aggressiveBatterySaveMode, optionsLidCloseMode, this]
	{
		int newBatterySaverTimeSeconds = (int)Math::round(sliderBatterySaverTime->getValue()*60.f);
		int newBatterySaverExtendedTimeSeconds = (int)Math::round(sliderBatterySaverExtendedTime->getValue()*60.f);
		SystemConf::getInstance()->set("system.batterysaver.timer", std::to_string(newBatterySaverTimeSeconds));
		SystemConf::getInstance()->set("system.batterysaver.mode", optionsBatterySaveMode->getSelected());
		SystemConf::getInstance()->set("system.batterysaver.extendedtimer", std::to_string(newBatterySaverExtendedTimeSeconds));
		SystemConf::getInstance()->set("system.batterysaver.extendedmode", optionsBatterySaveExtendedMode->getSelected());
		SystemConf::getInstance()->setBool("system.batterysaver.aggressive", aggressiveBatterySaveMode->getState());
		if (BoardCheck::isBoard(SUPPORTED_LID_BOARDS)) {
			SystemConf::getInstance()->set("system.lid", optionsLidCloseMode->getSelected());
		}
		SystemConf::getInstance()->saveSystemConf();
		Scripting::fireEvent("powermanagement-changed");
		setVariable("exitreboot", true);
	});

}
