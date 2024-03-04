#include "GuiControllersSettings.h"

#include "components/SwitchComponent.h"
#include "components/OptionListComponent.h"

#include "guis/GuiDetectDevice.h"
#include "guis/GuiBluetoothPair.h"
#include "ThreadedBluetooth.h"
#include "guis/GuiBluetoothForget.h"

#include "guis/GuiMsgBox.h"
#include "InputManager.h"
#include "SystemConf.h"

#define gettext_controllers_settings				_("CONTROLLER SETTINGS")
#define gettext_controllers_and_bluetooth_settings  _("CONTROLLER & BLUETOOTH SETTINGS")

#define gettext_controllers_priority _("CONTROLLERS PRIORITY")
#define gettext_controllers_player_assigments _("PLAYER ASSIGNMENTS")

#define gettext_controllerid _("CONTROLLER #%i")
#define gettext_playerid _("P%i'S CONTROLLER")

// Windows build does not have bluetooth support, so affect the label for Windows
#if WIN32
#define controllers_settings_label		gettext_controllers_settings
#define controllers_group_label		gettext_controllers_priority
#else
#define controllers_settings_label		gettext_controllers_and_bluetooth_settings
#define controllers_group_label		gettext_controllers_player_assigments
#endif

std::string GuiControllersSettings::getControllersSettingsLabel()
{
	return controllers_settings_label;
}

void GuiControllersSettings::openControllersSettings(Window* wnd, int autoSel)
{
	wnd->pushGui(new GuiControllersSettings(wnd, autoSel));
}

GuiControllersSettings::GuiControllersSettings(Window* wnd, int autoSel) : GuiSettings(wnd, controllers_settings_label.c_str())
{
	Window* window = mWindow;

	addGroup(_("SETTINGS"));

	// CONTROLLER CONFIGURATION
	addEntry(_("CONTROLLER MAPPING"), false, [window, this]
	{
		window->pushGui(new GuiMsgBox(window,
			_("YOU ARE GOING TO MAP A CONTROLLER. MAP BASED ON THE BUTTON'S POSITION, "
				"NOT ITS PHYSICAL LABEL. IF YOU DO NOT HAVE A SPECIAL BUTTON FOR HOTKEY, "
				"USE THE SELECT BUTTON. SKIP ALL BUTTONS/STICKS YOU DO NOT HAVE BY "
				"HOLDING ANY BUTTON. PRESS THE SOUTH BUTTON TO CONFIRM WHEN DONE."),
			_("OK"), [window, this] { window->pushGui(new GuiDetectDevice(window, false, [window, this] { Window* parent = window; setSave(false); delete this; openControllersSettings(parent); })); },
			_("CANCEL"), nullptr,
			GuiMsgBoxIcon::ICON_INFORMATION));
	});

	bool sindenguns_menu = false;
	bool wiiguns_menu = false;
	bool steamdeckguns_menu = false;

	for (auto gun : InputManager::getInstance()->getGuns())
	{
		sindenguns_menu |= gun->needBorders();
		wiiguns_menu |= gun->name() == "Wii Gun calibrated";
		steamdeckguns_menu |= gun->name() == "Steam Gun";
	}

	for (auto joy : InputManager::getInstance()->getInputConfigs())
		wiiguns_menu |= joy->getDeviceName() == "Nintendo Wii Remote";

	for (auto mouse : InputManager::getInstance()->getMice()) // the Steam Mouse can be converted to a Steam Gun via options
		steamdeckguns_menu |= mouse == "Steam Mouse";

	if (sindenguns_menu)
		addEntry(_("SINDEN GUN SETTINGS"), true, [this] { openControllersSpecificSettings_sindengun(); });

	if (wiiguns_menu)
		addEntry(_("WIIMOTE GUN SETTINGS"), true, [this] { openControllersSpecificSettings_wiigun(); });

	if (steamdeckguns_menu)
		addEntry(_("STEAMDECK MOUSE/GUN SETTINGS"), true, [this] { openControllersSpecificSettings_steamdeckgun(); });

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BLUETOOTH))
	{
		addGroup(_("BLUETOOTH"));

		// PAIR A BLUETOOTH CONTROLLER
		addEntry(_("PAIR BLUETOOTH PADS AUTOMATICALLY"), false, [window] { ThreadedBluetooth::start(window); });

#if defined(BATOCERA) || defined(WIN32)
		// PAIR A BLUETOOTH CONTROLLER OR BT AUDIO DEVICE
		addEntry(_("PAIR A BLUETOOTH DEVICE MANUALLY"), false, [window]
		{
			if (ThreadedBluetooth::isRunning())
				window->pushGui(new GuiMsgBox(window, _("BLUETOOTH SCAN IS ALREADY RUNNING.")));
			else
				window->pushGui(new GuiBluetoothPair(window));
		});
#endif
		// FORGET BLUETOOTH CONTROLLERS OR BT AUDIO DEVICES
		addEntry(_("FORGET A BLUETOOTH DEVICE"), false, [window] { window->pushGui(new GuiBluetoothForget(window)); });
	}

	addGroup(_("DISPLAY OPTIONS"));

	// CONTROLLER NOTIFICATION
	auto notification = std::make_shared<SwitchComponent>(mWindow);
	notification->setState(Settings::getInstance()->getBool("ShowControllerNotifications"));
	addWithLabel(_("SHOW CONTROLLER NOTIFICATIONS"), notification, autoSel == 1);
	notification->setOnChangedCallback([this, window, notification]
									   {
		if (Settings::getInstance()->setBool("ShowControllerNotifications", notification->getState()))
		{
			Window* parent = window;
			delete this;
			openControllersSettings(parent, 1);
		} });
		
	// CONTROLLER ACTIVITY
	auto activity = std::make_shared<SwitchComponent>(mWindow);
	activity->setState(Settings::getInstance()->getBool("ShowControllerActivity"));
	addWithLabel(_("SHOW CONTROLLER ACTIVITY"), activity, autoSel == 1);
	activity->setOnChangedCallback([this, window, activity]
	{
		if (Settings::getInstance()->setBool("ShowControllerActivity", activity->getState()))
		{
			Window* parent = window;
			delete this;
			openControllersSettings(parent, 1);
		}
	});

	if (Settings::getInstance()->getBool("ShowControllerActivity"))
		addSwitch(_("SHOW CONTROLLER BATTERY LEVEL"), "ShowControllerBattery", true);

	addGroup(controllers_group_label);

	// Here we go; for each player
	std::list<int> alreadyTaken = std::list<int>();

	// clear the current loaded inputs
	clearLoadedInput();

	std::vector<std::shared_ptr<OptionListComponent<InputConfigInfo*>>> options;

	auto configList = InputManager::getInstance()->getInputConfigs();

#if WIN32
	for (int player = 0; player < MAX_PLAYERS; player++)
	{
		std::string label = Utils::String::format(gettext_controllerid.c_str(), player + 1);

		auto inputOptionList = std::make_shared<OptionListComponent<InputConfigInfo*> >(mWindow, label, false);
		inputOptionList->add(_("default"), nullptr, false);
		options.push_back(inputOptionList);

		// Checking if a setting has been saved, else setting to default
		std::string configuratedName = Settings::getInstance()->getString(Utils::String::format("INPUT P%iNAME", player + 1));
		std::string configuratedGuid = Settings::getInstance()->getString(Utils::String::format("INPUT P%iGUID", player + 1));
		std::string configuratedPath = Settings::getInstance()->getString(Utils::String::format("INPUT P%iPATH", player + 1));

		bool found = false;

		// Add configurated controller even if it's disconnected
		InputConfigInfo* defaultInputConfig = nullptr;

		if (!configuratedPath.empty())
		{
			InputConfigInfo* newInputConfig = new InputConfigInfo(configuratedName, configuratedGuid, configuratedPath);
			mLoadedInput.push_back(newInputConfig);

			auto it = std::find_if(configList.cbegin(), configList.cend(), [configuratedPath](InputConfig* x) { return x->getSortDevicePath() == configuratedPath; });
			if (it != configList.cend())
			{
				if (std::find(alreadyTaken.begin(), alreadyTaken.end(), (*it)->getDeviceId()) == alreadyTaken.end())
				{
					inputOptionList->addEx(configuratedName, configuratedPath, newInputConfig, true, false, false);
					alreadyTaken.push_back((*it)->getDeviceId());
					defaultInputConfig = newInputConfig;
				}
			}
			else
				inputOptionList->addEx(configuratedName + " (" + _("NOT CONNECTED") + ")", configuratedPath, newInputConfig, true, false, false);

			found = true;
		}

		// for each available and configured input
		for (auto config : configList)
		{
			if (defaultInputConfig != nullptr && defaultInputConfig->name == config->getDeviceName() && defaultInputConfig->guid == config->getDeviceGUIDString() && defaultInputConfig->path == config->getSortDevicePath())
				continue;

			std::string displayName = config->getDeviceName();

			bool foundFromConfig = !configuratedPath.empty() ? config->getSortDevicePath() == configuratedPath : configuratedName == config->getDeviceName() && configuratedGuid == config->getDeviceGUIDString();
			int deviceID = config->getDeviceId();

			InputConfigInfo* newInputConfig = new InputConfigInfo(config->getDeviceName(), config->getDeviceGUIDString(), config->getSortDevicePath());
			mLoadedInput.push_back(newInputConfig);

			if (foundFromConfig && std::find(alreadyTaken.begin(), alreadyTaken.end(), deviceID) == alreadyTaken.end() && !found)
			{
				found = true;
				alreadyTaken.push_back(deviceID);

				LOG(LogWarning) << "adding entry for player" << player << " (selected): " << config->getDeviceName() << "  " << config->getDeviceGUIDString() << "  " << config->getDevicePath();
				inputOptionList->addEx(displayName, config->getDevicePath(), newInputConfig, true, false, false);
			}
			else
			{
				LOG(LogInfo) << "adding entry for player" << player << " (not selected): " << config->getDeviceName() << "  " << config->getDeviceGUIDString() << "  " << config->getDevicePath();
				inputOptionList->addEx(displayName, config->getDevicePath(), newInputConfig, false, false, false);
			}
		}

		if (!inputOptionList->hasSelection())
			inputOptionList->selectFirstItem();

		// Populate controllers list
		addWithLabel(label, inputOptionList);
	}
#else
	for (int player = 0; player < MAX_PLAYERS; player++)
	{
		std::string confName = Utils::String::format("INPUT P%iNAME", player + 1);
		std::string confGuid = Utils::String::format("INPUT P%iGUID", player + 1);
		std::string confPath = Utils::String::format("INPUT P%iPATH", player + 1);

		std::string label = Utils::String::format(gettext_playerid.c_str(), player + 1);

		auto inputOptionList = std::make_shared<OptionListComponent<InputConfigInfo*> >(mWindow, label, false);
		inputOptionList->add(_("default"), nullptr, false);
		options.push_back(inputOptionList);

		// Checking if a setting has been saved, else setting to default
		std::string configuratedName = Settings::getInstance()->getString(confName);
		std::string configuratedGuid = Settings::getInstance()->getString(confGuid);
		std::string configuratedPath = Settings::getInstance()->getString(confPath);

		bool found = false;

		// For each available and configured input
		for (auto config : configList)
		{
			std::string displayName = "#" + std::to_string(config->getDeviceIndex()) + " " + config->getDeviceName();
			bool foundFromConfig = !configuratedPath.empty() ? config->getSortDevicePath() == configuratedPath : configuratedName == config->getDeviceName() && configuratedGuid == config->getDeviceGUIDString();

			int deviceID = config->getDeviceId();

			InputConfigInfo* newInputConfig = new InputConfigInfo(config->getDeviceName(), config->getDeviceGUIDString(), config->getSortDevicePath());
			mLoadedInput.push_back(newInputConfig);

			if (foundFromConfig && std::find(alreadyTaken.begin(), alreadyTaken.end(), deviceID) == alreadyTaken.end() && !found)
			{
				found = true;
				alreadyTaken.push_back(deviceID);

				LOG(LogWarning) << "adding entry for player" << player << " (selected): " << config->getDeviceName() << "  " << config->getDeviceGUIDString() << "  " << config->getDevicePath();
				inputOptionList->add(displayName, newInputConfig, true);
			}
			else
			{
				LOG(LogInfo) << "adding entry for player" << player << " (not selected): " << config->getDeviceName() << "  " << config->getDeviceGUIDString() << "  " << config->getDevicePath();
				inputOptionList->add(displayName, newInputConfig, false);
			}
		}

		if (!inputOptionList->hasSelection())
			inputOptionList->selectFirstItem();

		// Populate controllers list
		addWithLabel(label, inputOptionList);
	}
#endif

	addSaveFunc([this, options, window]
	{
		bool changed = false;

		for (int player = 0; player < MAX_PLAYERS; player++)
		{
			std::string confName = Utils::String::format("INPUT P%iNAME", player + 1);
			std::string confGuid = Utils::String::format("INPUT P%iGUID", player + 1);
			std::string confPath = Utils::String::format("INPUT P%iPATH", player + 1);

			auto input = options.at(player);

			InputConfigInfo* selected = input->getSelected();
			if (selected == nullptr)
			{
				changed |= Settings::getInstance()->setString(confName, "DEFAULT");
				changed |= Settings::getInstance()->setString(confGuid, "");
				changed |= Settings::getInstance()->setString(confPath, "");
			}
			else if (input->changed())
			{
				LOG(LogInfo) << "Found the selected controller : " << input->getSelectedName() << ", " << selected->guid << ", " << selected->path;

				changed |= Settings::getInstance()->setString(confName, selected->name);
				changed |= Settings::getInstance()->setString(confGuid, selected->guid);
				changed |= Settings::getInstance()->setString(confPath, selected->path);
			}
		}

		if (changed)
			Settings::getInstance()->saveFile();

		// this is dependant of this configuration, thus update it
		InputManager::getInstance()->computeLastKnownPlayersDeviceIndexes();
	});
}

void GuiControllersSettings::openControllersSpecificSettings_sindengun()
{
	GuiSettings* s = new GuiSettings(mWindow, controllers_settings_label.c_str());

	std::string selectedBordersSize = SystemConf::getInstance()->get("controllers.guns.borderssize");
	auto borderssize_set = std::make_shared<OptionListComponent<std::string> >(mWindow, _("BORDER SIZE"), false);
	borderssize_set->add(_("AUTO"), "", "" == selectedBordersSize);
	borderssize_set->add(_("THIN"), "THIN", "THIN" == selectedBordersSize);
	borderssize_set->add(_("MEDIUM"), "MEDIUM", "MEDIUM" == selectedBordersSize);
	borderssize_set->add(_("BIG"), "BIG", "BIG" == selectedBordersSize);
	s->addOptionList(_("BORDER SIZE"), { { _("AUTO"), "auto" },{ _("THIN") , "thin" },{ _("MEDIUM"), "medium" },{ _("BIG"), "big" } }, "controllers.guns.borderssize", false);

	std::string selectedBordersMode = SystemConf::getInstance()->get("controllers.guns.bordersmode");
	auto bordersmode_set = std::make_shared<OptionListComponent<std::string> >(mWindow, _("BORDER MODE"), false);
	bordersmode_set->add(_("AUTO"), "", "" == selectedBordersMode);
	bordersmode_set->add(_("NORMAL"), "NORMAL", "NORMAL" == selectedBordersMode);
	bordersmode_set->add(_("IN GAME ONLY"), "INGAMEONLY", "INGAMEONLY" == selectedBordersMode);
	bordersmode_set->add(_("HIDDEN"), "HIDDEN", "HIDDEN" == selectedBordersMode);
	s->addOptionList(_("BORDER MODE"), { { _("AUTO"), "auto" },{ _("NORMAL") , "normal" },{ _("IN GAME ONLY"), "gameonly" },{ _("HIDDEN"), "hidden" } }, "controllers.guns.bordersmode", false);

	std::string selectedBordersColor = SystemConf::getInstance()->get("controllers.guns.borderscolor");
	auto borderscolor_set = std::make_shared<OptionListComponent<std::string> >(mWindow, _("BORDER COLOR"), false);
	borderscolor_set->add(_("AUTO"), "", "" == selectedBordersColor);
	borderscolor_set->add(_("WHITE"), "WHITE", "white" == selectedBordersColor);
	borderscolor_set->add(_("RED"), "RED", "red" == selectedBordersColor);
	borderscolor_set->add(_("GREEN"), "GREEN", "green" == selectedBordersColor);
	borderscolor_set->add(_("BLUE"), "BLUE", "blue" == selectedBordersColor);
	s->addOptionList(_("BORDER COLOR"), { { _("AUTO"), "auto" },{ _("WHITE") , "white" },{ _("RED") , "red" },{ _("GREEN"), "green" },{ _("BLUE"), "blue" } }, "controllers.guns.borderscolor", false);

#if BATOCERA
	std::string selectedCameraContrast = SystemConf::getInstance()->get("controllers.guns.sinden.contrast");
	auto cameracontrast_set = std::make_shared<OptionListComponent<std::string> >(mWindow, _("CAMERA CONTRAST"), false);
	cameracontrast_set->add(_("AUTO"), "", "" == selectedCameraContrast);
	cameracontrast_set->add(_("Daytime/Bright Sunlight (40)"), "40", "40" == selectedCameraContrast);
	cameracontrast_set->add(_("Default (50)"), "50", "50" == selectedCameraContrast);
	cameracontrast_set->add(_("Dim Display/Evening (60)"), "60", "60" == selectedCameraContrast);
	s->addWithLabel(_("CAMERA CONTRAST"), cameracontrast_set);

	std::string selectedCameraBrightness = SystemConf::getInstance()->get("controllers.guns.sinden.brightness");
	auto camerabrightness_set = std::make_shared<OptionListComponent<std::string> >(mWindow, _("CAMERA BRIGHTNESS"), false);
	camerabrightness_set->add(_("AUTO"), "", "" == selectedCameraBrightness);
	camerabrightness_set->add(_("Daytime/Bright Sunlight (80)"), "80", "80" == selectedCameraBrightness);
	camerabrightness_set->add(_("Default (100)"), "100", "100" == selectedCameraBrightness);
	camerabrightness_set->add(_("Dim Display/Evening (120)"), "120", "120" == selectedCameraBrightness);
	s->addWithLabel(_("CAMERA BRIGHTNESS"), camerabrightness_set);

	std::string selectedCameraExposure = SystemConf::getInstance()->get("controllers.guns.sinden.exposure");
	auto cameraexposure_set = std::make_shared<OptionListComponent<std::string> >(mWindow, _("CAMERA EXPOSURE"), false);
	cameraexposure_set->add(_("AUTO"), "", "" == selectedCameraExposure);
	cameraexposure_set->add(_("Projector/CRT (-5)"), "-5", "-5" == selectedCameraExposure);
	cameraexposure_set->add(_("Projector/CRT (-6)"), "-6", "-6" == selectedCameraExposure);
	cameraexposure_set->add(_("Default (-7)"), "-7", "-7" == selectedCameraExposure);
	cameraexposure_set->add(_("Other (-8)"), "-8", "-8" == selectedCameraExposure);
	cameraexposure_set->add(_("Other (-9)"), "-9", "-9" == selectedCameraExposure);
	s->addWithLabel(_("CAMERA EXPOSURE"), cameraexposure_set);

	std::string baseMode = SystemConf::getInstance()->get("controllers.guns.recoil");
	auto sindenmode_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("RECOIL"), false);
	sindenmode_choices->add(_("AUTO"), "auto", baseMode.empty() || baseMode == "auto");
	sindenmode_choices->add(_("DISABLED"), "disabled", baseMode == "disabled");
	sindenmode_choices->add(_("GUN"), "gun", baseMode == "gun");
	sindenmode_choices->add(_("MACHINE GUN"), "machinegun", baseMode == "machinegun");
	sindenmode_choices->add(_("QUIET GUN"), "gun-quiet", baseMode == "gun-quiet");
	sindenmode_choices->add(_("QUIET MACHINE GUN"), "machinegun-quiet", baseMode == "machinegun-quiet");
	s->addWithLabel(_("RECOIL"), sindenmode_choices);

	s->addSaveFunc([sindenmode_choices, cameracontrast_set, camerabrightness_set, cameraexposure_set] {
		if (sindenmode_choices->getSelected() != SystemConf::getInstance()->get("controllers.guns.recoil") ||
			cameracontrast_set->getSelected() != SystemConf::getInstance()->get("controllers.guns.sinden.contrast") ||
			camerabrightness_set->getSelected() != SystemConf::getInstance()->get("controllers.guns.sinden.brightness") ||
			cameraexposure_set->getSelected() != SystemConf::getInstance()->get("controllers.guns.sinden.exposure")
			) {
			SystemConf::getInstance()->set("controllers.guns.recoil", sindenmode_choices->getSelected());
			SystemConf::getInstance()->set("controllers.guns.sinden.contrast", cameracontrast_set->getSelected());
			SystemConf::getInstance()->set("controllers.guns.sinden.brightness", camerabrightness_set->getSelected());
			SystemConf::getInstance()->set("controllers.guns.sinden.exposure", cameraexposure_set->getSelected());
			SystemConf::getInstance()->saveSystemConf();
			ApiSystem::getInstance()->replugControllers_sindenguns();
		}
	});
#endif

	mWindow->pushGui(s);
}

void GuiControllersSettings::openControllersSpecificSettings_wiigun()
{
	GuiSettings* s = new GuiSettings(mWindow, controllers_settings_label.c_str());

	std::string baseMode = SystemConf::getInstance()->get("controllers.wiimote.mode");
	auto wiimode_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("MODE"), false);
	wiimode_choices->add(_("AUTO"), "auto", baseMode.empty() || baseMode == "auto");
	wiimode_choices->add(_("GUN"), "gun", baseMode == "gun");
	wiimode_choices->add(_("JOYSTICK"), "joystick", baseMode == "joystick");
	s->addWithLabel(_("MODE"), wiimode_choices);
	s->addSaveFunc([wiimode_choices] {
		if (wiimode_choices->getSelected() != SystemConf::getInstance()->get("controllers.wiimote.mode")) {
			SystemConf::getInstance()->set("controllers.wiimote.mode", wiimode_choices->getSelected());
			SystemConf::getInstance()->saveSystemConf();
			ApiSystem::getInstance()->replugControllers_wiimotes();
		}
	});
	mWindow->pushGui(s);
}

void GuiControllersSettings::openControllersSpecificSettings_steamdeckgun()
{
	GuiSettings* s = new GuiSettings(mWindow, controllers_settings_label.c_str());

	std::string baseMode = SystemConf::getInstance()->get("controllers.steamdeckmouse.gun");
	auto mode_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("MODE"), false);
	mode_choices->add(_("AUTO"), "auto", baseMode.empty() || baseMode == "auto");
	mode_choices->add(_("MOUSE ONLY"), "0", baseMode == "0");
	mode_choices->add(_("GUN"), "1", baseMode == "1");
	s->addWithLabel(_("MODE"), mode_choices);
	s->addSaveFunc([mode_choices] {
		if (mode_choices->getSelected() != SystemConf::getInstance()->get("controllers.steamdeckmouse.gun")) {
			SystemConf::getInstance()->set("controllers.steamdeckmouse.gun", mode_choices->getSelected());
			SystemConf::getInstance()->saveSystemConf();
			ApiSystem::getInstance()->replugControllers_steamdeckguns();
		}
	});

	std::string baseHand = SystemConf::getInstance()->get("controllers.steamdeckmouse.hand");
	auto hand_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("HAND"), false);
	hand_choices->add(_("AUTO"), "auto", baseHand.empty() || baseHand == "auto");
	hand_choices->add(_("LEFT"), "left", baseHand == "left");
	hand_choices->add(_("RIGHT"), "right", baseHand == "right");
	s->addWithLabel(_("HAND"), hand_choices);
	s->addSaveFunc([hand_choices] {
		if (hand_choices->getSelected() != SystemConf::getInstance()->get("controllers.steamdeckmouse.hand")) {
			SystemConf::getInstance()->set("controllers.steamdeckmouse.hand", hand_choices->getSelected());
			SystemConf::getInstance()->saveSystemConf();
			ApiSystem::getInstance()->replugControllers_steamdeckguns();
		}
	});

	mWindow->pushGui(s);
}

void GuiControllersSettings::clearLoadedInput() 
{
	for (int i = 0; i < mLoadedInput.size(); i++) 
		delete mLoadedInput[i];

	mLoadedInput.clear();
}

GuiControllersSettings::~GuiControllersSettings() 
{
	clearLoadedInput();
}
