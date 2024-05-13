#include "guis/GuiMenu.h"

#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiCollectionSystemsOptions.h"
#include "guis/GuiDetectDevice.h"
#include "guis/GuiGeneralScreensaverOptions.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiScraperStart.h"
#include "guis/GuiHashStart.h"
#include "guis/GuiThemeInstaller.h"
#include "guis/GuiBezelInstaller.h"
#include "guis/GuiBatoceraStore.h"
#include "guis/GuiSettings.h"
#include "guis/GuiRetroAchievements.h"
#include "guis/GuiGamelistOptions.h"
#include "guis/GuiImageViewer.h"
#include "guis/GuiNetPlaySettings.h"
#include "guis/GuiRetroAchievementsSettings.h"
#include "guis/GuiSystemInformation.h"
#include "guis/GuiScraperSettings.h"
#include "guis/GuiControllersSettings.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "EmulationStation.h"
#include "Scripting.h"
#include "SystemData.h"
#include "VolumeControl.h"
#include <SDL_events.h>
#include <algorithm>
#include "utils/Platform.h"


#include "SystemConf.h"
#include "ApiSystem.h"
#include "InputManager.h"
#include "AudioManager.h"
#include <LibretroRatio.h>
#include "guis/GuiUpdate.h"
#include "guis/GuiInstallStart.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "guis/GuiBackupStart.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiWifi.h"
#include "guis/GuiBluetoothPair.h"
#include "guis/GuiBluetoothForget.h"
#include "scrapers/ThreadedScraper.h"
#include "FileSorts.h"
#include "ThreadedHasher.h"
#include "ThreadedBluetooth.h"
#include "views/gamelist/IGameListView.h"
#include "components/MultiLineMenuEntry.h"
#include "components/BatteryIndicatorComponent.h"
#include "GuiLoading.h"
#include "guis/GuiBios.h"
#include "guis/GuiKeyMappingEditor.h"
#include "Gamelist.h"
#include "TextToSpeech.h"
#include "Paths.h"

#if WIN32
#include "Win32ApiSystem.h"
#endif

#define fake_gettext_fade _("fade")
#define fake_gettext_slide _("slide")
#define fake_gettext_instant _("instant")
#define fake_gettext_fadeslide _("fade & slide")

#define fake_gettext_system       _("System")
#define fake_gettext_architecture _("Architecture")
#define fake_gettext_diskformat   _("Disk format")
#define fake_gettext_temperature  _("Temperature")
#define fake_gettext_avail_memory _("Available memory")
#define fake_gettext_battery      _("Battery")
#define fake_gettext_model        _("Model")
#define fake_gettext_cpu_model    _("Cpu model")
#define fake_gettext_cpu_number   _("Cpu number")
#define fake_gettext_cpu_frequency _("Cpu max frequency")
#define fake_gettext_cpu_feature  _("Cpu feature")

#define fake_gettext_simple_bilinear_simple	pgettext("game_options", "SHARP-BILINEAR-SIMPLE")
#define fake_gettext_scanlines				pgettext("game_options", "SCANLINES")
#define fake_gettext_retro					pgettext("game_options", "RETRO")
#define fake_gettext_enhanced				pgettext("game_options", "ENHANCED")
#define fake_gettext_curvature				pgettext("game_options", "CURVATURE")
#define fake_gettext_zfast					pgettext("game_options", "ZFAST")
#define fake_gettext_flatten_glow			pgettext("game_options", "FLATTEN-GLOW")
#define fake_gettext_rgascaling				pgettext("game_options", "RGA SCALING")

#define fake_gettext_glvendor		_("VENDOR")
#define fake_gettext_glvrenderer	_("RENDERER")
#define fake_gettext_glversion		_("VERSION")
#define fake_gettext_glslversion	_("SHADERS")

#define fake_gettext_disk_internal _("INTERNAL")
#define fake_gettext_disk_external _("ANY EXTERNAL")

#define fake_gettext_resolution_max_1K  _("maximum 1920x1080")
#define fake_gettext_resolution_max_640 _("maximum 640x480")

GuiMenu::GuiMenu(Window *window, bool animate) : GuiComponent(window), mMenu(window, _("MAIN MENU").c_str()), mVersion(window)
{
	// MAIN MENU
	bool isFullUI = !UIModeController::getInstance()->isUIModeKid() && !UIModeController::getInstance()->isUIModeKiosk();

	// KODI >
	// GAMES SETTINGS >
	// CONTROLLER & BLUETOOTH >
	// USER INTERFACE SETTINGS >
	// SOUND SETTINGS >
	// NETWORK >
	// SCRAPER >
	// SYSTEM SETTINGS >
	// QUIT >

	// KODI
#ifdef _ENABLE_KODI_
	if (SystemConf::getInstance()->getBool("kodi.enabled", true) && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::KODI))
		addEntry(_("KODI MEDIA CENTER").c_str(), false, [this] 
	{ 
		Window *window = mWindow;
		delete this;
		if (!ApiSystem::getInstance()->launchKodi(window))
			LOG(LogWarning) << "Shutdown terminated with non-zero result!";

	}, "iconKodi");	
#endif

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RETROACHIVEMENTS) &&
		SystemConf::getInstance()->getBool("global.retroachievements") &&
		Settings::getInstance()->getBool("RetroachievementsMenuitem") && 
		SystemConf::getInstance()->get("global.retroachievements.username") != "")
		addEntry(_("RETROACHIEVEMENTS").c_str(), true, [this] {
				if (!checkNetwork())
					return;
				GuiRetroAchievements::show(mWindow); }, "iconRetroachievements");
	
	if (isFullUI)
	{
#if BATOCERA
		addEntry(_("GAME SETTINGS").c_str(), true, [this] { openGamesSettings(); }, "iconGames");
		addEntry(GuiControllersSettings::getControllersSettingsLabel(), true, [window] { GuiControllersSettings::openControllersSettings(window); }, "iconControllers");
		addEntry(_("USER INTERFACE SETTINGS").c_str(), true, [this] { openUISettings(); }, "iconUI");
		addEntry(_("GAME COLLECTION SETTINGS").c_str(), true, [this] { openCollectionSystemSettings(); }, "iconAdvanced");
		addEntry(_("SOUND SETTINGS").c_str(), true, [this] { openSoundSettings(); }, "iconSound");

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::WIFI))
			addEntry(_("NETWORK SETTINGS").c_str(), true, [this] { openNetworkSettings(); }, "iconNetwork");
#else
		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
			addEntry(_("GAME SETTINGS").c_str(), true, [this] { openGamesSettings(); }, "iconGames");

		addEntry(_("USER INTERFACE SETTINGS").c_str(), true, [this] { openUISettings(); }, "iconUI");

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))		
			addEntry(GuiControllersSettings::getControllersSettingsLabel(), true, [window] { GuiControllersSettings::openControllersSettings(window); }, "iconControllers");
		else
			addEntry(_("CONFIGURE INPUT"), true, [this] { openConfigInput(); }, "iconControllers");

		addEntry(_("SOUND SETTINGS").c_str(), true, [this] { openSoundSettings(); }, "iconSound");
		addEntry(_("GAME COLLECTION SETTINGS").c_str(), true, [this] { openCollectionSystemSettings(); }, "iconAdvanced");

		if (!ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
		{
			for (auto system : SystemData::sSystemVector)
			{
				if (system->isCollection() || system->getEmulators().size() == 0 || (system->getEmulators().size() == 1 && system->getEmulators().begin()->cores.size() <= 1))
					continue;

				addEntry(_("EMULATOR SETTINGS"), true, [this] { openEmulatorSettings(); }, "iconGames");
				break;
			}
		}
#endif

		addEntry(_("SCRAPER").c_str(), true, [this] { openScraperSettings(); }, "iconScraper");		

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BATOCERASTORE) || ApiSystem::getInstance()->isScriptingSupported(ApiSystem::THEMESDOWNLOADER) ||
			(ApiSystem::getInstance()->isScriptingSupported(ApiSystem::THEBEZELPROJECT) && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DECORATIONS)) ||
			ApiSystem::getInstance()->isScriptingSupported(ApiSystem::UPGRADE))
			addEntry(_("UPDATES & DOWNLOADS"), true, [this] { openUpdatesSettings(); }, "iconUpdates");

		addEntry(_("SYSTEM SETTINGS").c_str(), true, [this] { openSystemSettings(); }, "iconSystem");
	}
	else
	{
		addEntry(_("INFORMATION").c_str(), true, [this] { openSystemInformations(); }, "iconSystem");
		addEntry(_("UNLOCK USER INTERFACE MODE").c_str(), true, [this] { exitKidMode(); }, "iconAdvanced");
	}

#ifdef WIN32
	addEntry(_("QUIT"), !Settings::getInstance()->getBool("ShowOnlyExit") || !Settings::getInstance()->getBool("ShowExit"), [this] { openQuitMenu(); }, "iconQuit");
#else
	addEntry(_("QUIT").c_str(), true, [this] { openQuitMenu(); }, "iconQuit");
#endif
	
	addChild(&mMenu);
	addVersionInfo();
	setSize(mMenu.getSize());

	if (animate)
	{
		if (Renderer::ScreenSettings::fullScreenMenus())
			animateTo(Vector2f((Renderer::getScreenWidth() - getSize().x()) / 2, (Renderer::getScreenHeight() - getSize().y()) / 2));
		else
			animateTo(Vector2f((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.15f));
	}
	else
	{
		if (Renderer::ScreenSettings::fullScreenMenus())
			setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);
		else
			setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.15f);
	}
}

void GuiMenu::openScraperSettings()
{		
	mWindow->pushGui(new GuiScraperStart(mWindow));
}

void GuiMenu::openConfigInput()
{
	Window* window = mWindow;
	window->pushGui(new GuiMsgBox(window, _("ARE YOU SURE YOU WANT TO CONFIGURE THE INPUT?"), 
		_("YES"), [window] { window->pushGui(new GuiDetectDevice(window, false, nullptr)); }, 
		_("NO"), nullptr)
	);
}

void GuiMenu::addVersionInfo()
{
	std::string  buildDate = (Settings::getInstance()->getBool("Debug") ? std::string( "   (" + Utils::String::toUpper(PROGRAM_BUILT_STRING) + ")") : (""));

	auto theme = ThemeData::getMenuTheme();

	mVersion.setFont(theme->Footer.font);
	mVersion.setColor(theme->Footer.color);

	mVersion.setLineSpacing(0);

	std::string label;

	if (!ApiSystem::getInstance()->getVersion().empty())
	{
		if (ApiSystem::getInstance()->getApplicationName() == "BATOCERA")
			label = "BATOCERA.LINUX ES V" + ApiSystem::getInstance()->getVersion() + buildDate;
		else
		{
			std::string aboutInfo = ApiSystem::getInstance()->getApplicationName() + " V" + ApiSystem::getInstance()->getVersion();
			label = aboutInfo + buildDate;
		}		
	}
		
	if (!label.empty())
	{
		if (Renderer::ScreenSettings::fullScreenMenus())
		{
			mMenu.setSubTitle(label);
			mMenu.addButton(_("BACK"), _("go back"), [&] { delete this; });
		}
		else
		{
			mVersion.setText(label);
		}
	}

	mVersion.setHorizontalAlignment(ALIGN_CENTER);
	mVersion.setVerticalAlignment(ALIGN_CENTER);
	addChild(&mVersion);
}

void GuiMenu::openScreensaverOptions() 
{
	mWindow->pushGui(new GuiGeneralScreensaverOptions(mWindow));
}

void GuiMenu::openCollectionSystemSettings() 
{
	if (ThreadedScraper::isRunning() || ThreadedHasher::isRunning())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("THIS FUNCTION IS DISABLED WHILE THE SCRAPER IS RUNNING")));
		return;
	}

	mWindow->pushGui(new GuiCollectionSystemsOptions(mWindow));
}

void GuiMenu::onSizeChanged()
{
	GuiComponent::onSizeChanged();

	float h = mMenu.getButtonGridHeight();

	mVersion.setSize(mSize.x(), h);
	mVersion.setPosition(0, mSize.y() - h);
}

void GuiMenu::addEntry(const std::string& name, bool add_arrow, const std::function<void()>& func, const std::string iconName)
{
	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	ComponentListRow row;

	MenuComponent::addMenuIcon(mWindow, row, iconName);

	auto text = std::make_shared<TextComponent>(mWindow, name, font, color);
	row.addElement(text, true);

	if (EsLocale::isRTL())
		text->setHorizontalAlignment(Alignment::ALIGN_RIGHT);

	if (add_arrow)
	{
		std::shared_ptr<ImageComponent> bracket = makeArrow(mWindow);

		if (EsLocale::isRTL())
			bracket->setFlipX(true);

		row.addElement(bracket, false);
	}

	row.makeAcceptInputHandler(func);
	mMenu.addRow(row);
}

bool GuiMenu::input(InputConfig* config, Input input)
{
	if(GuiComponent::input(config, input))
		return true;

	if((config->isMappedTo(BUTTON_BACK, input) || config->isMappedTo("start", input)) && input.value != 0)
	{
		delete this;
		return true;
	}

	return false;
}

std::vector<HelpPrompt> GuiMenu::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt("up/down", _("CHOOSE"))); 
	prompts.push_back(HelpPrompt(BUTTON_OK, _("SELECT"))); 
	prompts.push_back(HelpPrompt("start", _("CLOSE"), [&] { delete this; }));
	return prompts;
}

class ExitKidModeMsgBox : public GuiSettings
{
	public: ExitKidModeMsgBox(Window* window, const std::string& title, const std::string& text) : GuiSettings(window, title) { addEntry(text); }

	bool input(InputConfig* config, Input input) override
	{
		if (UIModeController::getInstance()->listen(config, input))
		{
			mWindow->pushGui(new GuiMsgBox(mWindow, _("THE USER INTERFACE MODE IS NOW UNLOCKED"),
				_("OK"), [this] 
				{
					Window* window = mWindow;
					while (window->peekGui() && window->peekGui() != ViewController::get())
						delete window->peekGui();
				}));


			return true;
		}

		return GuiComponent::input(config, input);
	}
};

void GuiMenu::exitKidMode()
{
	if (Settings::getInstance()->getString("UIMode") == "Basic")
	{
		Settings::getInstance()->setString("UIMode", "Full");

		Window* window = mWindow;
		while (window->peekGui() && window->peekGui() != ViewController::get())
			delete window->peekGui();
	}
	else
		mWindow->pushGui(new ExitKidModeMsgBox(mWindow, _("UNLOCK USER INTERFACE MODE"), _("ENTER THE CODE NOW TO UNLOCK")));
}

void GuiMenu::openSystemInformations()
{
	mWindow->pushGui(new GuiSystemInformation(mWindow));
}

void GuiMenu::openServicesSettings()
{
	auto s = new GuiSettings(mWindow, _("SERVICES").c_str());

	auto services = ApiSystem::getInstance()->getServices();
	for(unsigned int i = 0; i < services.size(); i++) {
	  auto service_enabled = std::make_shared<SwitchComponent>(mWindow);
	  service_enabled->setState(services[i].enabled);
	  s->addWithLabel(services[i].name, service_enabled);
	  service_enabled->setOnChangedCallback([services, i, service_enabled]()
	  {
	    ApiSystem::getInstance()->enableService(services[i].name, service_enabled->getState());
	  });
	}

	mWindow->pushGui(s);
}

void GuiMenu::openDmdSettings()
{
	auto s = new GuiSettings(mWindow, _("DMD").c_str());
	Window* window = mWindow;

	// server
	auto services = ApiSystem::getInstance()->getServices();
	std::string current_server = "";
	for(unsigned int i = 0; i < services.size(); i++) {
	  if(services[i].enabled) {
	    if(services[i].name == "dmd_real")      current_server = "dmd_real";
	    if(services[i].name == "dmd_simulator") current_server = "dmd_simulator";
	  }
	}
	auto server = std::make_shared< OptionListComponent<std::string> >(window, _("SERVER"), false);
	server->addRange({ { _("DISABLED"), "" }, { _("DMDSERVER (for real dmd)"), "dmd_real" }, { _("SIMULATOR (for web dmd)"), "dmd_simulator" } }, current_server);
	s->addWithDescription(_("SERVER"), _("dmd server"), server);

	// format
	auto format = std::make_shared< OptionListComponent<std::string> >(window, _("FORMAT"), false);
	std::string current_format = SystemConf::getInstance()->get("dmd.format");
	format->addRange({ { _("AUTO"), "" }, { "SD", "sd" }, { "HD", "hd" } }, current_format);
	s->addWithDescription(_("FORMAT"), _("dmd matrix size"), format);

	s->addGroup("PIXELCADE");

	// pixelcade.matrix
	auto pixelcade_matrix = std::make_shared< OptionListComponent<std::string> >(window, _("MATRIX"), false);
	std::string current_pixelcade_matrix = SystemConf::getInstance()->get("dmd.pixelcade.matrix");
	pixelcade_matrix->addRange({ { _("AUTO"), "" }, { "RGB", "rgb" }, { "RBG", "rbg" } }, current_pixelcade_matrix);
	s->addWithDescription(_("MATRIX"), _("rgb dmd order"), pixelcade_matrix);

	s->addGroup("ZEDMD");

	// zedmd.matrix
	auto zedmd_matrix = std::make_shared< OptionListComponent<std::string> >(window, _("MATRIX"), false);
	std::string current_zedmd_matrix = SystemConf::getInstance()->get("dmd.zedmd.matrix");
	zedmd_matrix->addRange({ { _("AUTO"), "" }, { "RGB", "rgb" }, { "RBG", "rbg" }, { "BRG", "brg" }, { "BGR", "bgr" }, { "GRB", "grb" }, { "GBR", "gbr" } }, current_zedmd_matrix);
	s->addWithDescription(_("MATRIX"), _("rgb dmd order"), zedmd_matrix);

	// zedmd.brightness
	auto zedmd_brightness = std::make_shared< OptionListComponent<std::string> >(window, _("BRIGHTNESS"), false);
	std::string current_zedmd_brightness = SystemConf::getInstance()->get("dmd.zedmd.brightness");
	zedmd_brightness->addRange({ { _("AUTO"), "" }, { "0", "0" }, { "1", "1" }, { "2", "2" }, { "3", "3" }, { "4", "4" }, { "5", "5" }, { "6", "6" }, { "7", "7" }, { "8", "8" }, { "9", "9" }, { "10", "10" }, { "11", "11" }, { "12", "12" }, { "13", "13" }, { "14", "14" }, { "15", "15" } }, current_zedmd_brightness);
	s->addWithLabel(_("BRIGHTNESS"), zedmd_brightness);

	s->addSaveFunc([window, server, format, pixelcade_matrix, zedmd_matrix, zedmd_brightness, current_server, current_format, current_pixelcade_matrix, current_zedmd_matrix, current_zedmd_brightness] {
	  bool needRestart = false;
	  bool needSave    = false;

	  if(current_format != format->getSelected()) {
	    SystemConf::getInstance()->set("dmd.format", format->getSelected());
	    needSave = true;
	  }
	  if(current_pixelcade_matrix != pixelcade_matrix->getSelected()) {
	    SystemConf::getInstance()->set("dmd.pixelcade.matrix", pixelcade_matrix->getSelected());
	    needRestart = true;
	    needSave = true;
	  }
	  if(current_zedmd_matrix != zedmd_matrix->getSelected()) {
	    SystemConf::getInstance()->set("dmd.zedmd.matrix", zedmd_matrix->getSelected());
	    needRestart = true;
	    needSave = true;
	  }
	  if(current_zedmd_brightness != zedmd_brightness->getSelected()) {
	    SystemConf::getInstance()->set("dmd.zedmd.brightness", zedmd_brightness->getSelected());
	    needRestart = true;
	    needSave = true;
	  }

	  if(server->getSelected() != current_server) {
	    needRestart = true;
	  }

	  if(needSave) {
	    SystemConf::getInstance()->saveSystemConf();
	  }

	  if(needRestart) {
	    bool stopped = false;
	    bool started = false;

	    // stop the existing server
	    if(current_server != "") {
	      ApiSystem::getInstance()->enableService(current_server, false);
	      stopped = true;
	    }
	    // start the new server
	    if(server->getSelected() != "") {
	      ApiSystem::getInstance()->enableService(server->getSelected(), true);
	      started = true;
	    }

	    if(stopped && !started) {
	      window->displayNotificationMessage(_U("\uF011  ") + _("DMDSERVER stopped"));
	    } else if(stopped && started) {
	      window->displayNotificationMessage(_U("\uF011  ") + _("DMDSERVER restarted"));
	    } else if(!stopped && started) {
	      window->displayNotificationMessage(_U("\uF011  ") + _("DMDSERVER started"));
	    }
	  }
	});

	window->pushGui(s);
}

void GuiMenu::openMultiScreensSettings()
{
	auto s = new GuiSettings(mWindow, _("MULTISCREENS").c_str());
	Window* window = mWindow;

#ifdef BATOCERA
	s->addGroup(_("BACKGLASS / INFORMATION SCREEN"));

	// video device2
	std::vector<std::string> availableVideo2 = ApiSystem::getInstance()->getAvailableVideoOutputDevices();
	if (availableVideo2.size())
	{
		auto optionsVideo2 = std::make_shared<OptionListComponent<std::string> >(mWindow, _("VIDEO OUTPUT"), false);
		std::string currentDevice2 = SystemConf::getInstance()->get("global.videooutput2");
		std::string currentDevice = SystemConf::getInstance()->get("global.videooutput");
		if (currentDevice2.empty()) currentDevice2 = "auto";

		bool vfound = false;
		for (auto it = availableVideo2.begin(); it != availableVideo2.end(); it++)
		{
		        if(currentDevice == (*it)) continue; // ignore the device of the first screen
			optionsVideo2->add((*it), (*it), currentDevice2 == (*it));
			if (currentDevice2 == (*it))
				vfound = true;
		}

		if (!vfound)
			optionsVideo2->add(currentDevice2, currentDevice2, true);

		s->addWithLabel(_("VIDEO OUTPUT"), optionsVideo2);
		s->addSaveFunc([this, optionsVideo2, currentDevice2, s] 
		{
			if (optionsVideo2->changed()) 
			{
				SystemConf::getInstance()->set("global.videooutput2", optionsVideo2->getSelected());
				SystemConf::getInstance()->saveSystemConf();
				s->setVariable("exitreboot", true);
			}
		});
	}

	// video resolution2
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RESOLUTION)) {
	  auto videoModeOptionList2 = createVideoResolutionModeOptionList(mWindow, "es", "resolution2");
	  s->addWithDescription(_("VIDEO MODE"), _("Sets the display's resolution."), videoModeOptionList2);
	  s->addSaveFunc([this, videoModeOptionList2, s] {
	    if(videoModeOptionList2->changed()) {
	      SystemConf::getInstance()->set("es.resolution2", videoModeOptionList2->getSelected());
	      SystemConf::getInstance()->saveSystemConf();
	      s->setVariable("exitreboot", true);
	    }
	  });
	}

	// video rotation2
	auto optionsRotation2 = std::make_shared<OptionListComponent<std::string> >(mWindow, _("ROTATION"), false);

	std::string selectedRotation2 = SystemConf::getInstance()->get("display.rotate2");
	if (selectedRotation2.empty())
		selectedRotation2 = "auto";

	optionsRotation2->add(_("AUTO"),          "auto", selectedRotation2 == "auto");
	optionsRotation2->add(_("0 DEGREES"),        "0", selectedRotation2 == "0");
	optionsRotation2->add(_("90 DEGREES"),       "1", selectedRotation2 == "1");
	optionsRotation2->add(_("180 DEGREES"),      "2", selectedRotation2 == "2");
	optionsRotation2->add(_("270 DEGREES"),      "3", selectedRotation2 == "3");

	s->addWithLabel(_("SCREEN ROTATION"), optionsRotation2);

	s->addSaveFunc([this, optionsRotation2, selectedRotation2, s]
	{
	  if (optionsRotation2->changed()) {
	    SystemConf::getInstance()->set("display.rotate2", optionsRotation2->getSelected());
	    SystemConf::getInstance()->saveSystemConf();
	    s->setVariable("exitreboot", true);
	  }
	});

	s->addGroup(_("DMD SCREEN"));

	// video device3
	std::vector<std::string> availableVideo3 = ApiSystem::getInstance()->getAvailableVideoOutputDevices();
	if (availableVideo3.size())
	{
		auto optionsVideo3 = std::make_shared<OptionListComponent<std::string> >(mWindow, _("VIDEO OUTPUT"), false);
		std::string currentDevice3 = SystemConf::getInstance()->get("global.videooutput3");
		std::string currentDevice = SystemConf::getInstance()->get("global.videooutput");
		if (currentDevice3.empty()) currentDevice3 = "auto";

		bool vfound = false;
		for (auto it = availableVideo3.begin(); it != availableVideo3.end(); it++)
		{
		        if(currentDevice == (*it)) continue; // ignore the device of the first screen
			optionsVideo3->add((*it), (*it), currentDevice3 == (*it));
			if (currentDevice3 == (*it))
				vfound = true;
		}

		if (!vfound)
			optionsVideo3->add(currentDevice3, currentDevice3, true);

		s->addWithLabel(_("VIDEO OUTPUT"), optionsVideo3);
		s->addSaveFunc([this, optionsVideo3, currentDevice3, s] 
		{
			if (optionsVideo3->changed()) 
			{
				SystemConf::getInstance()->set("global.videooutput3", optionsVideo3->getSelected());
				SystemConf::getInstance()->saveSystemConf();
				s->setVariable("exitreboot", true);
			}
		});
	}

	// video resolution3
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RESOLUTION)) {
	  auto videoModeOptionList3 = createVideoResolutionModeOptionList(mWindow, "es", "resolution3");
	  s->addWithDescription(_("VIDEO MODE"), _("Sets the display's resolution."), videoModeOptionList3);
	  s->addSaveFunc([this, videoModeOptionList3, s] {
	    if(videoModeOptionList3->changed()) {
	      SystemConf::getInstance()->set("es.resolution3", videoModeOptionList3->getSelected());
	      SystemConf::getInstance()->saveSystemConf();
	      s->setVariable("exitreboot", true);
	    }
	  });
	}

	// video rotation3
	auto optionsRotation3 = std::make_shared<OptionListComponent<std::string> >(mWindow, _("ROTATION"), false);

	std::string selectedRotation3 = SystemConf::getInstance()->get("display.rotate3");
	if (selectedRotation3.empty())
		selectedRotation3 = "auto";

	optionsRotation3->add(_("AUTO"),          "auto", selectedRotation3 == "auto");
	optionsRotation3->add(_("0 DEGREES"),        "0", selectedRotation3 == "0");
	optionsRotation3->add(_("90 DEGREES"),       "1", selectedRotation3 == "1");
	optionsRotation3->add(_("180 DEGREES"),      "2", selectedRotation3 == "2");
	optionsRotation3->add(_("270 DEGREES"),      "3", selectedRotation3 == "3");

	s->addWithLabel(_("SCREEN ROTATION"), optionsRotation3);

	s->addSaveFunc([this, optionsRotation3, selectedRotation3, s]
	{
	  if (optionsRotation3->changed()) 
{
	    SystemConf::getInstance()->set("display.rotate3", optionsRotation3->getSelected());
	    SystemConf::getInstance()->saveSystemConf();
	    s->setVariable("exitreboot", true);
	  }
	});

#endif

	s->onFinalize([s, window]
	{
	  if (s->getVariable("exitreboot") && Settings::getInstance()->getBool("ExitOnRebootRequired"))
	    {
	      Utils::Platform::quitES(Utils::Platform::QuitMode::QUIT);
	      return;
	    }
	});

	window->pushGui(s);
}

void GuiMenu::openDeveloperSettings()
{
	Window *window = mWindow;

	auto s = new GuiSettings(mWindow, _("FRONTEND DEVELOPER OPTIONS").c_str());
	
	s->addGroup(_("VIDEO OPTIONS"));

	// maximum vram
	auto max_vram = std::make_shared<SliderComponent>(mWindow, 40.f, 1000.f, 10.f, "Mb");
	max_vram->setValue((float)(Settings::getInstance()->getInt("MaxVRAM")));
	s->addWithLabel(_("VRAM LIMIT"), max_vram);
	s->addSaveFunc([max_vram] { Settings::getInstance()->setInt("MaxVRAM", (int)round(max_vram->getValue())); });
	
	s->addSwitch(_("SHOW FRAMERATE"), _("Also turns on the emulator's native FPS counter, if available."), "DrawFramerate", true, nullptr);
	s->addSwitch(_("VSYNC"), "VSync", true, [] { Renderer::setSwapInterval(); });

#ifdef BATOCERA
	// overscan
	auto overscan_enabled = std::make_shared<SwitchComponent>(mWindow);
	overscan_enabled->setState(Settings::getInstance()->getBool("Overscan"));
	s->addWithLabel(_("OVERSCAN"), overscan_enabled);
	s->addSaveFunc([overscan_enabled] 
	{
		if (Settings::getInstance()->getBool("Overscan") != overscan_enabled->getState()) 
		{
			Settings::getInstance()->setBool("Overscan", overscan_enabled->getState());
			ApiSystem::getInstance()->setOverscan(overscan_enabled->getState());
		}
	});

	// es resolution
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RESOLUTION))
	  {
	    auto videoModeOptionList = createVideoResolutionModeOptionList(mWindow, "es", "resolution");
	    s->addWithDescription(_("VIDEO MODE"), _("Sets the display's resolution for emulationstation."), videoModeOptionList);
	    s->addSaveFunc([this, videoModeOptionList] { SystemConf::getInstance()->set("es.resolution", videoModeOptionList->getSelected()); });
	  }
#endif

#ifdef _RPI_
	// Video Player - VideoOmxPlayer
	auto omx_player = std::make_shared<SwitchComponent>(mWindow);
	omx_player->setState(Settings::getInstance()->getBool("VideoOmxPlayer"));
	s->addWithLabel(_("USE OMX PLAYER (HARDWARE ACCELERATED)"), omx_player);
	s->addSaveFunc([omx_player, window]
	{
		// need to reload all views to re-create the right video components
		bool needReload = false;
		if (Settings::getInstance()->getBool("VideoOmxPlayer") != omx_player->getState())
			needReload = true;

		Settings::getInstance()->setBool("VideoOmxPlayer", omx_player->getState());

		if (needReload)
		{
			ViewController::get()->reloadAll(window);
			window->closeSplashScreen();
		}
	});
#endif

	s->addGroup(_("TOOLS"));	

#ifndef WIN32
	// GAME AT STARTUP
	if (!SystemConf::getInstance()->get("global.bootgame.path").empty())
	{		
		std::string gamelabel = SystemConf::getInstance()->get("global.bootgame.path");			
		gamelabel = Utils::FileSystem::getStem(gamelabel) + " [" + Utils::FileSystem::getStem(Utils::FileSystem::getParent(gamelabel)) + "]";

		s->addWithDescription(_("STOP LAUNCHING THIS GAME AT STARTUP"), gamelabel, nullptr, [s]
		{
			SystemConf::getInstance()->set("global.bootgame.path", "");
			SystemConf::getInstance()->set("global.bootgame.cmd", "");

			s->close();
		});
	}
#endif

	// WEB ACCESS
	auto hostName = Utils::String::toLower(ApiSystem::getInstance()->getHostsName());

	auto webAccess = std::make_shared<SwitchComponent>(mWindow);
	webAccess->setState(Settings::getInstance()->getBool("PublicWebAccess"));
	s->addWithDescription(_("ENABLE PUBLIC WEB API ACCESS"), Utils::String::format(_("Allow public web access API using %s").c_str(), std::string("http://" + hostName + ":1234").c_str()), webAccess);
	s->addSaveFunc([webAccess, window, s]
	{ 
	  if (Settings::getInstance()->setBool("PublicWebAccess", webAccess->getState())) 
	  {
		  s->setVariable("reboot", true);
		  s->setVariable("exitreboot", true);
	  }
	});

	// log level
	auto logLevel = std::make_shared< OptionListComponent<std::string> >(mWindow, _("LOG LEVEL"), false);
	std::vector<std::string> modes;
	modes.push_back("default");
	modes.push_back("disabled");
	modes.push_back("warning");
	modes.push_back("error");
	modes.push_back("debug");

	auto level = Settings::getInstance()->getString("LogLevel");
	if (level.empty())
		level = "default";

	for (auto it = modes.cbegin(); it != modes.cend(); it++)
		logLevel->add(_(it->c_str()), *it, level == *it);

	s->addWithLabel(_("LOG LEVEL"), logLevel);
	s->addSaveFunc([this, logLevel]
	{
		if (Settings::getInstance()->setString("LogLevel", logLevel->getSelected() == "default" ? "" : logLevel->getSelected()))
		{
			Log::init();
		}
	});

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SUPPORTFILE))
	{
		// support
		s->addEntry(_("CREATE A SUPPORT FILE"), true, [window] 
		{
			window->pushGui(new GuiMsgBox(window, _("CREATE A SUPPORT FILE? THIS INCLUDES ALL DATA IN YOUR SYSTEM FOLDER."), _("YES"),
				[window] 
				{
					if (ApiSystem::getInstance()->generateSupportFile())
						window->pushGui(new GuiMsgBox(window, _("SUPPORT FILE CREATED IN SAVES FOLDER"), _("OK")));
					else
						window->pushGui(new GuiMsgBox(window, _("SUPPORT FILE CREATION FAILED"), _("OK")));				
				}, 
				_("NO"), nullptr));
		});
	}

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DISKFORMAT))
		s->addEntry(_("FORMAT A DISK"), true, [this] { openFormatDriveSettings(); });
	
	s->addWithDescription(_("CLEAN GAMELISTS & REMOVE UNUSED MEDIA"), _("Remove unused entries, and clean references to missing medias."), nullptr, [this, s]
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("ARE YOU SURE?"), _("YES"), [&]
		{
			int idx = 0;
			for (auto system : SystemData::sSystemVector)
			{
				mWindow->renderSplashScreen(_("Cleaning") + ": " + system->getFullName(), (float)idx / (float)SystemData::sSystemVector.size());
				cleanupGamelist(system);
				idx++;
			}

			mWindow->closeSplashScreen();
		}, _("NO"), nullptr));
	});

	s->addWithDescription(_("RESET GAMELISTS USAGE DATA"), _("Reset values of GameTime, PlayCount and LastPlayed metadatas."), nullptr, [this, s]
		{
			mWindow->pushGui(new GuiMsgBox(mWindow, _("ARE YOU SURE?"), _("YES"), [&]
				{
					int idx = 0;
					for (auto system : SystemData::sSystemVector)
					{
						mWindow->renderSplashScreen(_("Cleaning") + ": " + system->getFullName(), (float)idx / (float)SystemData::sSystemVector.size());
						resetGamelistUsageData(system);
						idx++;
					}

					mWindow->closeSplashScreen();

					ViewController::reloadAllGames(mWindow, false);
				}, _("NO"), nullptr));
		});

	s->addWithDescription(_("RESET FILE EXTENSIONS"), _("Reset customized file extensions filters to default."), nullptr, [this, s]
	//s->addEntry(_("RESET FILE EXTENSIONS"), false, [this, s]
	{
		for (auto system : SystemData::sSystemVector)
			Settings::getInstance()->setString(system->getName() + ".HiddenExt", "");

		Settings::getInstance()->saveFile();
		ViewController::reloadAllGames(mWindow, false);
	});

	s->addEntry(_("REDETECT ALL GAMES' LANG/REGION"), false, [this]
	{
		Window* window = mWindow;
		window->pushGui(new GuiLoading<int>(window, _("PLEASE WAIT"), [](auto gui)
		{
			for (auto system : SystemData::sSystemVector)
			{
				if (system->isCollection() || !system->isGameSystem())
					continue;

				for (auto game : system->getRootFolder()->getFilesRecursive(GAME))
					game->detectLanguageAndRegion(true);
			}

			return 0;
		}));
	});

	s->addEntry(_("FIND ALL GAMES WITH NETPLAY/ACHIEVEMENTS"), false, [this] { ThreadedHasher::start(mWindow, ThreadedHasher::HASH_ALL, true); });

	s->addEntry(_("CLEAR CACHES"), true, [this, s]
		{
			ImageIO::clearImageCache();

			auto rootPath = Utils::FileSystem::getGenericPath(Paths::getUserEmulationStationPath());

			Utils::FileSystem::deleteDirectoryFiles(rootPath + "/tmp/");
			Utils::FileSystem::deleteDirectoryFiles(Utils::FileSystem::getTempPath());
			Utils::FileSystem::deleteDirectoryFiles(Utils::FileSystem::getPdfTempPath());

			ViewController::reloadAllGames(mWindow, false);
		});

	s->addEntry(_("BUILD IMAGE CACHE"), true, [this, s]
		{
			unsigned int x;
			unsigned int y;

			int idx = 0;
			for (auto sys : SystemData::sSystemVector)
			{
				if (sys->isCollection())
				{
					idx++;
					continue;
				}

				mWindow->renderSplashScreen(_("Building image cache") + ": " + sys->getFullName(), (float)idx / (float)SystemData::sSystemVector.size());

				for (auto file : sys->getRootFolder()->getFilesRecursive(GAME))
				{
					for (auto mdd : MetaDataList::getMDD())
					{
						if (mdd.id != MetaDataId::Image && mdd.id != MetaDataId::Thumbnail)
							continue;

						auto value = file->getMetadata(mdd.id);
						if (value.empty())
							continue;

						auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(value));
						if (ext == ".jpg" || ext == ".png")
							ImageIO::loadImageSize(value.c_str(), &x, &y);
					}
				}

				idx++;
			}

			mWindow->closeSplashScreen();
		});

	s->addGroup(_("DISPLAY SETTINGS"));

	auto menuFontScale = std::make_shared< OptionListComponent<std::string> >(mWindow, _("MENU FONT SCALE"), false);
	menuFontScale->addRange({ { _("AUTO"), "" },{ "100%", "1.0" },{ "110%", "1.1" },{ "125%", "1.25" },{ "133%", "1.31" },{ "150%", "1.5" },{ "175%", "1.75" },{ "200%", "2" },{ "75%", "0.75" } ,{ "50%", "0.5" } },
		Settings::getInstance()->getString("MenuFontScale"));
	s->addWithLabel(_("MENU FONT SCALE"), menuFontScale);
	s->addSaveFunc([s, menuFontScale] { if (Settings::getInstance()->setString("MenuFontScale", menuFontScale->getSelected())) s->setVariable("reboot", true); });

	auto fontScale = std::make_shared< OptionListComponent<std::string> >(mWindow, _("FONT SCALE"), false);
	fontScale->addRange({ { _("AUTO"), "" },{ "100%", "1.0" },{ "110%", "1.1" },{ "125%", "1.25" },{ "133%", "1.31" },{ "150%", "1.5" },{ "175%", "1.75" },{ "200%", "2" },{ "75%", "0.75" } ,{ "50%", "0.5" } },
		Settings::getInstance()->getString("FontScale"));
	s->addWithLabel(_("THEME FONT SCALE"), fontScale);
	s->addSaveFunc([s, fontScale] { if (Settings::getInstance()->setString("FontScale", fontScale->getSelected())) s->setVariable("reboot", true); });

	auto fullScreenMenus = std::make_shared< OptionListComponent<std::string> >(mWindow, _("FULL SCREEN MENUS"), false);
	fullScreenMenus->addRange({ { _("AUTO"), "" },{ "YES", "true" },{ "NO", "false" } }, Settings::getInstance()->getString("FullScreenMenu"));
	s->addWithLabel(_("FULL SCREEN MENUS"), fullScreenMenus);
	s->addSaveFunc([s, fullScreenMenus] { if (Settings::getInstance()->setString("FullScreenMenu", fullScreenMenus->getSelected())) s->setVariable("reboot", true); });

	auto isSmallScreen = std::make_shared< OptionListComponent<std::string> >(mWindow, _("FORCE SMALL SCREEN THEMING"), false);
	isSmallScreen->addRange({ { _("AUTO"), "" },{ "YES", "true" },{ "NO", "false" } }, Settings::getInstance()->getString("ForceSmallScreen"));
	s->addWithLabel(_("FORCE SMALL SCREEN THEMING"), isSmallScreen);
	s->addSaveFunc([s, isSmallScreen] { if (Settings::getInstance()->setString("ForceSmallScreen", isSmallScreen->getSelected())) s->setVariable("reboot", true); });



	s->addGroup(_("DATA MANAGEMENT"));

	// ExcludeMultiDiskContent
	auto excludeMultiDiskContent = std::make_shared<SwitchComponent>(mWindow);
	excludeMultiDiskContent->setState(Settings::getInstance()->getBool("RemoveMultiDiskContent"));
	s->addWithLabel(_("IGNORE MULTI-FILE DISK CONTENT (CUE/GDI/CCD/M3U)"), excludeMultiDiskContent);
	s->addSaveFunc([excludeMultiDiskContent] { Settings::getInstance()->setBool("RemoveMultiDiskContent", excludeMultiDiskContent->getState()); });

	// enable filters (ForceDisableFilters)
	auto enable_filter = std::make_shared<SwitchComponent>(mWindow);
	enable_filter->setState(!Settings::getInstance()->getBool("ForceDisableFilters"));
	s->addWithDescription(_("ENABLE GAME FILTERING"), _("Whether to show or hide game filtering related settings in the view options."), enable_filter);
	s->addSaveFunc([this, enable_filter]
	{
		Settings::getInstance()->setBool("ForceDisableFilters", !enable_filter->getState());
	});

	// gamelist saving
	auto save_gamelists = std::make_shared<SwitchComponent>(mWindow);
	save_gamelists->setState(Settings::getInstance()->getBool("SaveGamelistsOnExit"));
	s->addWithLabel(_("SAVE METADATA ON EXIT"), save_gamelists);
	s->addSaveFunc([save_gamelists] { Settings::getInstance()->setBool("SaveGamelistsOnExit", save_gamelists->getState()); });

	// gamelist
	auto parse_gamelists = std::make_shared<SwitchComponent>(mWindow);
	parse_gamelists->setState(Settings::getInstance()->getBool("ParseGamelistOnly"));
	s->addWithDescription(_("PARSE GAMELISTS ONLY"), _("Debug tool: Don't check if the ROMs actually exist. Can cause problems!"), parse_gamelists);
	s->addSaveFunc([parse_gamelists] { Settings::getInstance()->setBool("ParseGamelistOnly", parse_gamelists->getState()); });

	// Local Art
	auto local_art = std::make_shared<SwitchComponent>(mWindow);
	local_art->setState(Settings::getInstance()->getBool("LocalArt"));
	s->addWithDescription(_("SEARCH FOR LOCAL ART"), _("If no image is specified in the gamelist, try to find media with the same filename to use."), local_art);
	s->addSaveFunc([local_art] { Settings::getInstance()->setBool("LocalArt", local_art->getState()); });

	s->addGroup(_("USER INTERFACE"));

	// carousel transition option
	auto move_carousel = std::make_shared<SwitchComponent>(mWindow);
	move_carousel->setState(Settings::getInstance()->getBool("MoveCarousel"));
	s->addWithLabel(_("CAROUSEL TRANSITIONS"), move_carousel);
	s->addSaveFunc([move_carousel] { Settings::getInstance()->setBool("MoveCarousel", move_carousel->getState()); });

	// quick system select (left/right in game list view)
	auto quick_sys_select = std::make_shared<SwitchComponent>(mWindow);
	quick_sys_select->setState(Settings::getInstance()->getBool("QuickSystemSelect"));
	s->addWithLabel(_("QUICK SYSTEM SELECT"), quick_sys_select);
	s->addSaveFunc([quick_sys_select] { Settings::getInstance()->setBool("QuickSystemSelect", quick_sys_select->getState()); });

	// Enable OSK (On-Screen-Keyboard)
	auto osk_enable = std::make_shared<SwitchComponent>(mWindow);
	osk_enable->setState(Settings::getInstance()->getBool("UseOSK"));
	s->addWithLabel(_("ON-SCREEN KEYBOARD"), osk_enable);
	s->addSaveFunc([osk_enable] { Settings::getInstance()->setBool("UseOSK", osk_enable->getState()); });
	
#if defined(_WIN32) || defined(X86) || defined(X86_64)
	// Hide EmulationStation Window when running a game ( windows only )
	auto hideWindowScreen = std::make_shared<SwitchComponent>(mWindow);
	hideWindowScreen->setState(Settings::getInstance()->getBool("HideWindow"));
	s->addWithLabel(_("HIDE EMULATIONSTATION WHEN RUNNING A GAME"), hideWindowScreen);
	s->addSaveFunc([hideWindowScreen] { Settings::getInstance()->setBool("HideWindow", hideWindowScreen->getState()); });
#endif
	
#if defined(WIN32) && !defined(_DEBUG)
	// full exit
	auto fullExitMenu = std::make_shared<SwitchComponent>(mWindow);
	fullExitMenu->setState(!Settings::getInstance()->getBool("ShowOnlyExit"));
	s->addWithLabel(_("COMPLETE QUIT MENU"), fullExitMenu);
	s->addSaveFunc([fullExitMenu] { Settings::getInstance()->setBool("ShowOnlyExit", !fullExitMenu->getState()); });
#endif

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
	{
		auto retroarchRgui = std::make_shared< OptionListComponent<std::string> >(mWindow, _("RETROARCH MENU DRIVER"), false);
		retroarchRgui->addRange({ { _("AUTO"), "" },{ "rgui", "rgui" },{ "xmb", "xmb" },{ "ozone", "ozone" },{ "glui", "glui" } }, SystemConf::getInstance()->get("global.retroarch.menu_driver"));
		s->addWithLabel(_("RETROARCH MENU DRIVER"), retroarchRgui);
		s->addSaveFunc([retroarchRgui] { SystemConf::getInstance()->set("global.retroarch.menu_driver", retroarchRgui->getSelected()); });
	}

	auto invertJoy = std::make_shared<SwitchComponent>(mWindow);
	invertJoy->setState(Settings::getInstance()->getBool("InvertButtons"));
	s->addWithDescription(_("SWITCH CONFIRM & CANCEL BUTTONS IN EMULATIONSTATION"), _("Switches the South and East buttons' functionality"), invertJoy);
	s->addSaveFunc([this, s, invertJoy]
	{
		if (Settings::getInstance()->setBool("InvertButtons", invertJoy->getState()))
		{
			InputConfig::AssignActionButtons();
			s->setVariable("reloadAll", true);
		}
	});

	auto invertLongPress = std::make_shared<SwitchComponent>(mWindow);
	invertLongPress->setState(Settings::getInstance()->getBool("GameOptionsAtNorth"));
	s->addWithDescription(_("ACCESS GAME OPTIONS WITH NORTH BUTTON"), _("Switches to short-press North for Savestates & long-press South button for Game Options"), invertLongPress);
	s->addSaveFunc([this, s, invertLongPress]
	{
		if (Settings::getInstance()->setBool("GameOptionsAtNorth", invertLongPress->getState()))
			s->setVariable("reloadAll", true);
	});

	auto firstJoystickOnly = std::make_shared<SwitchComponent>(mWindow);
	firstJoystickOnly->setState(Settings::getInstance()->getBool("FirstJoystickOnly"));
	s->addWithLabel(_("CONTROL EMULATIONSTATION WITH FIRST JOYSTICK ONLY"), firstJoystickOnly);
	s->addSaveFunc([this, firstJoystickOnly] { Settings::getInstance()->setBool("FirstJoystickOnly", firstJoystickOnly->getState()); });

//#if !defined(WIN32)
	{
	  auto gun_mt = std::make_shared<SliderComponent>(mWindow, 0.f, 10.f, 0.1f, "%");
	  gun_mt->setValue(Settings::getInstance()->getFloat("GunMoveTolerence"));
	  s->addWithLabel(_("GUN MOVE TOLERENCE"), gun_mt);
	  s->addSaveFunc([gun_mt] {
	    Settings::getInstance()->setFloat("GunMoveTolerence", gun_mt->getValue());
	  });
	}
//#endif

#if defined(BATOCERA)
	// PS3 controller enable
	auto enable_ps3 = std::make_shared<SwitchComponent>(mWindow);
	enable_ps3->setState(SystemConf::getInstance()->getBool("controllers.ps3.enabled"));
	s->addWithDescription(_("ENABLE PS3 CONTROLLER SUPPORT"), _("Might have negative impact on security."), enable_ps3);
	s->addSaveFunc([enable_ps3] {
		bool ps3Enabled = enable_ps3->getState();
		if (ps3Enabled != SystemConf::getInstance()->getBool("controllers.ps3.enabled"))
		{
			SystemConf::getInstance()->setBool("controllers.ps3.enabled", ps3Enabled);
			SystemConf::getInstance()->saveSystemConf();
			if(SystemConf::getInstance()->getBool("controllers.bluetooth.enabled"))
				ApiSystem::getInstance()->enableBluetooth();
		}
	});
#endif

#if defined(WIN32)

	auto hidJoysticks = std::make_shared<SwitchComponent>(mWindow);
	hidJoysticks->setState(Settings::getInstance()->getBool("HidJoysticks"));
	s->addWithLabel(_("ENABLE HID JOYSTICK DRIVERS"), hidJoysticks);
	s->addSaveFunc([this, hidJoysticks] { Settings::getInstance()->setBool("HidJoysticks", hidJoysticks->getState()); });
	
	// Network Indicator
	auto networkIndicator = std::make_shared<SwitchComponent>(mWindow);
	networkIndicator->setState(Settings::getInstance()->getBool("ShowNetworkIndicator"));
	s->addWithLabel(_("SHOW NETWORK INDICATOR"), networkIndicator);
	s->addSaveFunc([networkIndicator] { Settings::getInstance()->setBool("ShowNetworkIndicator", networkIndicator->getState()); });
#endif

	s->addGroup(_("OPTIMIZATIONS"));

	// preload UI
	auto preloadUI = std::make_shared<SwitchComponent>(mWindow);
	preloadUI->setState(Settings::getInstance()->getBool("PreloadUI"));
	s->addWithDescription(_("PRELOAD UI ELEMENTS ON BOOT"), _("Reduces lag when entering gamelists from the system menu, increases boot time"), preloadUI);
	s->addSaveFunc([preloadUI] { Settings::getInstance()->setBool("PreloadUI", preloadUI->getState()); });

	// preload Medias
	auto preloadMedias = std::make_shared<SwitchComponent>(mWindow);
	preloadMedias->setState(Settings::getInstance()->getBool("PreloadMedias"));
	s->addWithDescription(_("PRELOAD METADATA MEDIA ON BOOT"), _("Reduces lag when scrolling through a fully scraped gamelist, increases boot time"), preloadMedias);
	s->addSaveFunc([preloadMedias] { Settings::getInstance()->setBool("PreloadMedias", preloadMedias->getState()); });
	
	// threaded loading
	auto threadedLoading = std::make_shared<SwitchComponent>(mWindow);
	threadedLoading->setState(Settings::getInstance()->getBool("ThreadedLoading"));
	s->addWithLabel(_("THREADED LOADING"), threadedLoading);
	s->addSaveFunc([threadedLoading] { Settings::getInstance()->setBool("ThreadedLoading", threadedLoading->getState()); });

	// threaded loading
	auto asyncImages = std::make_shared<SwitchComponent>(mWindow);
	asyncImages->setState(Settings::getInstance()->getBool("AsyncImages"));
	s->addWithLabel(_("ASYNC IMAGE LOADING"), asyncImages);
	s->addSaveFunc([asyncImages] { Settings::getInstance()->setBool("AsyncImages", asyncImages->getState()); });

	// optimizeVram
	auto optimizeVram = std::make_shared<SwitchComponent>(mWindow);
	optimizeVram->setState(Settings::getInstance()->getBool("OptimizeVRAM"));
	s->addWithLabel(_("OPTIMIZE IMAGES VRAM USE"), optimizeVram);
	s->addSaveFunc([optimizeVram] { Settings::getInstance()->setBool("OptimizeVRAM", optimizeVram->getState()); });

	// optimizeVideo
	auto optimizeVideo = std::make_shared<SwitchComponent>(mWindow);
	optimizeVideo->setState(Settings::getInstance()->getBool("OptimizeVideo"));
	s->addWithLabel(_("OPTIMIZE VIDEO VRAM USAGE"), optimizeVideo);
	s->addSaveFunc([optimizeVideo] { Settings::getInstance()->setBool("OptimizeVideo", optimizeVideo->getState()); });
	
	s->onFinalize([s, window]
	{					
		if (s->getVariable("reboot"))
			window->displayNotificationMessage(_U("\uF011  ") + _("REBOOT REQUIRED TO APPLY THE NEW CONFIGURATION"));

		if (s->getVariable("reloadAll"))
		{
			ViewController::get()->reloadAll(window);
			window->closeSplashScreen();
		}
	});

	mWindow->pushGui(s);
}

void GuiMenu::openUpdatesSettings()
{
	GuiSettings *updateGui = new GuiSettings(mWindow, _("UPDATES & DOWNLOADS").c_str());

	updateGui->addGroup(_("DOWNLOADS"));

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BATOCERASTORE))
	{
		updateGui->addEntry(_("CONTENT DOWNLOADER"), true, [this]
		{
			if (!checkNetwork())
				return;

			mWindow->pushGui(new GuiBatoceraStore(mWindow));
		});
	}

	// Themes installer/browser
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::THEMESDOWNLOADER))
	{
		updateGui->addEntry(_("THEMES"), true, [this]
		{
			if (!checkNetwork())
				return;

			mWindow->pushGui(new GuiThemeInstaller(mWindow));
		});
	}

	// integration with theBezelProject
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DECORATIONS) && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::THEBEZELPROJECT))
	{
		updateGui->addEntry(_("THE BEZEL PROJECT"), true, [this]
		{
			if (!checkNetwork())
				return;

			mWindow->pushGui(new GuiBezelInstaller(mWindow));
		});
	}

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::UPGRADE))
	{
		updateGui->addGroup(_("SOFTWARE UPDATES"));

		// Enable updates
		updateGui->addSwitch(_("CHECK FOR UPDATES"), "updates.enabled", false);

		auto updatesTypeList = std::make_shared<OptionListComponent<std::string> >(mWindow, _("UPDATE TYPE"), false);

#if BATOCERA
#define BETA_NAME "butterfly"
#else
#define BETA_NAME "beta"
#endif

		std::string updatesType = SystemConf::getInstance()->get("updates.type");

#if WIN32
		if (updatesType == "unstable")
			updatesTypeList->add("unstable", "unstable", updatesType == "unstable");
		else
#endif
			if (updatesType.empty() || updatesType != BETA_NAME)
				updatesType = "stable";

		updatesTypeList->add("stable", "stable", updatesType == "stable");
		updatesTypeList->add(BETA_NAME, BETA_NAME, updatesType == BETA_NAME);

		updateGui->addWithLabel(_("UPDATE TYPE"), updatesTypeList);
		updatesTypeList->setSelectedChangedCallback([](std::string name)
		{
			if (SystemConf::getInstance()->set("updates.type", name))
				SystemConf::getInstance()->saveSystemConf();
		});

		// Start update
		updateGui->addEntry(GuiUpdate::state == GuiUpdateState::State::UPDATE_READY ? _("APPLY UPDATE") : _("START UPDATE"), true, [this]
		{
			if (GuiUpdate::state == GuiUpdateState::State::UPDATE_READY)
				Utils::Platform::quitES(Utils::Platform::QuitMode::RESTART);
			else if (GuiUpdate::state == GuiUpdateState::State::UPDATER_RUNNING)
				mWindow->pushGui(new GuiMsgBox(mWindow, _("UPDATER IS ALREADY RUNNING")));
			else
			{
				if (!checkNetwork())
					return;

				mWindow->pushGui(new GuiUpdate(mWindow));
			}
		});
	}

	mWindow->pushGui(updateGui);
}

bool GuiMenu::checkNetwork()
{
	if (ApiSystem::getInstance()->getIpAdress() == "NOT CONNECTED")
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("YOU ARE NOT CONNECTED TO A NETWORK"), _("OK"), nullptr));
		return false;
	}

	return true;
}

void GuiMenu::openSystemSettings() 
{
	Window *window = mWindow;

	auto s = new GuiSettings(mWindow, _("SYSTEM SETTINGS").c_str());
	bool isFullUI = UIModeController::getInstance()->isUIModeFull();

	s->addGroup(_("SYSTEM"));

	// System informations
	s->addEntry(_("INFORMATION"), true, [this] { openSystemInformations(); });

	// language choice
	auto language_choice = std::make_shared<OptionListComponent<std::string> >(window, _("LANGUAGE"), false);

	std::string language = SystemConf::getInstance()->get("system.language");
	if (language.empty()) 
		language = "en_US";

	language_choice->add("ARABIC",               "ar_YE", language == "ar_YE");
	language_choice->add("CATALÀ",               "ca_ES", language == "ca_ES");
	language_choice->add("ČEŠTINA",                "cs_CZ", language == "cs_CZ");
	language_choice->add("CYMRAEG",              "cy_GB", language == "cy_GB");
	language_choice->add("DEUTSCH", 	     "de_DE", language == "de_DE");
	language_choice->add("GREEK",                "el_GR", language == "el_GR");
	language_choice->add("ENGLISH (US)", 	     "en_US", language == "en_US" || language == "en");
	language_choice->add("ENGLISH (UK)", 	     "en_GB", language == "en_GB");
	language_choice->add("ESPAÑOL", 	     "es_ES", language == "es_ES" || language == "es");
	language_choice->add("ESPAÑOL MEXICANO",     "es_MX", language == "es_MX");
	language_choice->add("EUSKARA",               "eu_ES", language == "eu_ES");
	language_choice->add("SUOMI",                "fi_FI", language == "fi_FI");
	language_choice->add("FRANÇAIS",             "fr_FR", language == "fr_FR" || language == "fr");
	language_choice->add("עברית",                "he_IL", language == "he_IL");
	language_choice->add("HUNGARIAN",            "hu_HU", language == "hu_HU");
	language_choice->add("BAHASA INDONESIA",     "id_ID", language == "id_ID");
	language_choice->add("ITALIANO",             "it_IT", language == "it_IT");
	language_choice->add("JAPANESE", 	     "ja_JP", language == "ja_JP");
	language_choice->add("KOREAN",   	     "ko_KR", language == "ko_KR" || language == "ko");
	language_choice->add("NORWEGIAN BOKMAL",     "nb_NO", language == "nb_NO");
	language_choice->add("DUTCH",                "nl_NL", language == "nl_NL");
	language_choice->add("NORWEGIAN",            "nn_NO", language == "nn_NO");
	language_choice->add("OCCITAN",              "oc_FR", language == "oc_FR");
	language_choice->add("POLISH",               "pl_PL", language == "pl_PL");
	language_choice->add("PORTUGUÊS BRASILEIRO", "pt_BR", language == "pt_BR");
	language_choice->add("PORTUGUÊS PORTUGAL",   "pt_PT", language == "pt_PT");
	language_choice->add("РУССКИЙ",              "ru_RU", language == "ru_RU");
	language_choice->add("SLOVENČINA", 	     "sk_SK", language == "sk_SK");
	language_choice->add("SVENSKA", 	     "sv_SE", language == "sv_SE");
	language_choice->add("TÜRKÇE",  	     "tr_TR", language == "tr_TR");
	language_choice->add("Українська",           "uk_UA", language == "uk_UA");
	language_choice->add("TIẾNG VIỆT",           "vi_VN", language == "vi_VN");
	language_choice->add("简体中文", 	     "zh_CN", language == "zh_CN");
	language_choice->add("正體中文", 	     "zh_TW", language == "zh_TW");

	s->addWithLabel(_("LANGUAGE (REGION)"), language_choice);
	s->addSaveFunc([window, language_choice, language, s]
	{
		if (language_choice->changed() && SystemConf::getInstance()->set("system.language", language_choice->getSelected()))
		{
			FileSorts::reset();
			MetaDataList::initMetadata();

#ifdef HAVE_INTL
			s->setVariable("exitreboot", true);
#endif
			s->setVariable("reloadGuiMenu", true);
		}		
	});

	// Timezone
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::TIMEZONES))
	{
		VectorEx<std::string> availableTimezones = ApiSystem::getInstance()->getTimezones();
		if (availableTimezones.size() > 0)
		{
			std::string currentTZ = ApiSystem::getInstance()->getCurrentTimezone();
			if (currentTZ.empty() || !availableTimezones.any([currentTZ](const std::string& tz) { return tz == currentTZ; }))
				currentTZ = "Europe/Paris";

			auto tzChoices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SELECT YOUR TIME ZONE"), false);

			for (auto tz : availableTimezones)
				tzChoices->add(_(Utils::String::toUpper(tz).c_str()), tz, currentTZ == tz);

			s->addWithLabel(_("TIME ZONE"), tzChoices);
			s->addSaveFunc([tzChoices] 
			{
				if (SystemConf::getInstance()->set("system.timezone", tzChoices->getSelected()))
					ApiSystem::getInstance()->setTimezone(tzChoices->getSelected());
			});
		}
	}

	// Clock time format (14:42 or 2:42 pm)
	s->addSwitch(_("SHOW CLOCK IN 12-HOUR FORMAT"), "ClockMode12", true);

	// power saver
	auto power_saver = std::make_shared< OptionListComponent<std::string> >(mWindow, _("POWER SAVING MODE"), false);
	power_saver->addRange({ { _("DISABLED"), "disabled" }, { _("DEFAULT"), "default" }, { _("ENHANCED"), "enhanced" }, { _("INSTANT"), "instant" }, }, Settings::PowerSaverMode());
	s->addWithDescription(_("POWER SAVING MODE"), _("Reduces power consumption when idle (useful for handhelds)."), power_saver);
	s->addSaveFunc([this, power_saver] 
	{
		if (Settings::PowerSaverMode() != "instant" && power_saver->getSelected() == "instant")
			Settings::getInstance()->setBool("EnableSounds", false);

		Settings::setPowerSaverMode(power_saver->getSelected());
		PowerSaver::init();
	});

#if defined(_ENABLE_TTS_) || defined(WIN32)
	if (TextToSpeech::getInstance()->isAvailable())
	{
			// tts
		auto tts = std::make_shared<SwitchComponent>(mWindow);
		tts->setState(Settings::getInstance()->getBool("TTS"));
		s->addWithLabel(_("SCREEN READER (TEXT TO SPEECH)"), tts);
		s->addSaveFunc([tts] {
			 if(TextToSpeech::getInstance()->isEnabled() != tts->getState()) {
			   TextToSpeech::getInstance()->enable(tts->getState());
			   Settings::getInstance()->setBool("TTS", tts->getState());
			 }});
	}
#endif

	// UI RESTRICTIONS
	auto UImodeSelection = std::make_shared< OptionListComponent<std::string> >(mWindow, _("USER INTERFACE MODE"), false);
	std::vector<std::string> UImodes = UIModeController::getInstance()->getUIModes();
	for (auto it = UImodes.cbegin(); it != UImodes.cend(); it++)
		UImodeSelection->add(_(it->c_str()), *it, Settings::getInstance()->getString("UIMode") == *it);

	s->addWithDescription(_("USER INTERFACE MODE"), _("Lock down certain config menus for use with guest users/kids."), UImodeSelection);
	s->addSaveFunc([UImodeSelection, window]
	{
		if (UImodeSelection->changed())
		{
			std::string selectedMode = UImodeSelection->getSelected();
			if (selectedMode == "Basic" || selectedMode == "Full")
				Settings::getInstance()->setString("UIMode", selectedMode);
			else
			{
				std::string msg = _("You are changing the user interface to a restricted mode:\nThis will hide most menu options to prevent changes to the system.\nTo unlock and return to the full user interface, enter this code:") + "\n";
				msg += "\"" + UIModeController::getInstance()->getFormattedPassKeyStr() + "\"\n\n";
				msg += _("Do you want to proceed?");
				window->pushGui(new GuiMsgBox(window, msg,
					_("YES"), [selectedMode] {
					LOG(LogDebug) << "Setting user interface mode to " << selectedMode;
					Settings::getInstance()->setString("UIMode", selectedMode);
					Settings::getInstance()->saveFile();
				}, _("NO"), nullptr));
			}
		}
	});

	// KODI SETTINGS
#ifdef _ENABLE_KODI_
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::KODI))
	{
		s->addEntry(_("KODI SETTINGS"), true, [this] 
		{
			GuiSettings* kodiGui = new GuiSettings(mWindow, _("KODI SETTINGS").c_str());
			kodiGui->addSwitch(_("ENABLE KODI"), "kodi.enabled", false);
			kodiGui->addSwitch(_("LAUNCH KODI AT BOOT"), "kodi.atstartup", false);
			mWindow->pushGui(kodiGui);
		});
	}
#endif

#ifdef BATOCERA
	s->addGroup(_("HARDWARE"));
#endif

	// brighness
	int brighness;
	if (ApiSystem::getInstance()->getBrightness(brighness))
	{
		auto brightnessComponent = std::make_shared<SliderComponent>(mWindow, 1.f, 100.f, 1.f, "%");
		brightnessComponent->setValue(brighness);
		brightnessComponent->setOnValueChanged([](const float &newVal)
		{
			ApiSystem::getInstance()->setBrightness((int)Math::round(newVal));
#if !WIN32
			SystemConf::getInstance()->set("display.brightness", std::to_string((int)Math::round(newVal)));
#endif
		});

		s->addWithLabel(_("BRIGHTNESS"), brightnessComponent);
	}

#ifdef BATOCERA
	// video device
	std::vector<std::string> availableVideo = ApiSystem::getInstance()->getAvailableVideoOutputDevices();
	if (availableVideo.size())
	{
		auto optionsVideo = std::make_shared<OptionListComponent<std::string> >(mWindow, _("VIDEO OUTPUT"), false);
		std::string currentDevice = SystemConf::getInstance()->get("global.videooutput");
		if (currentDevice.empty()) currentDevice = "auto";

		bool vfound = false;
		for (auto it = availableVideo.begin(); it != availableVideo.end(); it++)
		{
			optionsVideo->add((*it), (*it), currentDevice == (*it));
			if (currentDevice == (*it))
				vfound = true;
		}

		if (!vfound)
			optionsVideo->add(currentDevice, currentDevice, true);

		s->addWithLabel(_("VIDEO OUTPUT"), optionsVideo);
		s->addSaveFunc([this, optionsVideo, currentDevice, s] 
		{
			if (optionsVideo->changed()) 
			{
				SystemConf::getInstance()->set("global.videooutput", optionsVideo->getSelected());
				SystemConf::getInstance()->saveSystemConf();				
				s->setVariable("exitreboot", true);
			}
		});
	}
#endif

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::AUDIODEVICE))
	{
		std::vector<std::string> availableAudio = ApiSystem::getInstance()->getAvailableAudioOutputDevices();
		if (availableAudio.size())
		{
			// audio device
			auto optionsAudio = std::make_shared<OptionListComponent<std::string> >(mWindow, _("AUDIO OUTPUT"), false);

			std::string selectedAudio = ApiSystem::getInstance()->getCurrentAudioOutputDevice();
			if (selectedAudio.empty())
				selectedAudio = "auto";

			bool afound = false;
			for (auto it = availableAudio.begin(); it != availableAudio.end(); it++)
			{
				std::vector<std::string> tokens = Utils::String::split(*it, ' ');

				if (selectedAudio == tokens.at(0))
					afound = true;

				if (tokens.size() >= 2)
				{
					// concatenat the ending words
					std::string vname = "";
					for (unsigned int i = 1; i < tokens.size(); i++)
					{
						if (i > 2) vname += " ";
						vname += tokens.at(i);
					}
					optionsAudio->add(vname, tokens.at(0), selectedAudio == tokens.at(0));
				}
				else
					optionsAudio->add((*it), (*it), selectedAudio == tokens.at(0));
			}

			if (!afound)
				optionsAudio->add(selectedAudio, selectedAudio, true);

			s->addWithLabel(_("AUDIO OUTPUT"), optionsAudio);

			s->addSaveFunc([this, optionsAudio, selectedAudio]
			{
				if (optionsAudio->changed())
				{
					SystemConf::getInstance()->set("audio.device", optionsAudio->getSelected());
					ApiSystem::getInstance()->setAudioOutputDevice(optionsAudio->getSelected());
				}
				SystemConf::getInstance()->saveSystemConf();
			});
		}

		// audio profile
		std::vector<std::string> availableAudioProfiles = ApiSystem::getInstance()->getAvailableAudioOutputProfiles();
		if (availableAudioProfiles.size())
		{
			auto optionsAudioProfile = std::make_shared<OptionListComponent<std::string> >(mWindow, _("AUDIO PROFILE"), false);

			std::string selectedAudioProfile = ApiSystem::getInstance()->getCurrentAudioOutputProfile();
			if (selectedAudioProfile.empty())
				selectedAudioProfile = "auto";

			bool afound = false;
			for (auto it = availableAudioProfiles.begin(); it != availableAudioProfiles.end(); it++)
			{
				std::vector<std::string> tokens = Utils::String::split(*it, ' ');

				if (selectedAudioProfile == tokens.at(0))
					afound = true;

				if (tokens.size() >= 2)
				{
					// concatenat the ending words
					std::string vname = "";
					for (unsigned int i = 1; i < tokens.size(); i++)
					{
						if (i > 2) vname += " ";
						vname += tokens.at(i);
					}
					optionsAudioProfile->add(vname, tokens.at(0), selectedAudioProfile == tokens.at(0));
				}
				else
					optionsAudioProfile->add((*it), (*it), selectedAudioProfile == tokens.at(0));
			}

			if (afound == false)
				optionsAudioProfile->add(selectedAudioProfile, selectedAudioProfile, true);

			s->addWithDescription(_("AUDIO PROFILE"), _("Available options can change depending on current audio output."), optionsAudioProfile);

			s->addSaveFunc([this, optionsAudioProfile, selectedAudioProfile]
			{
				if (optionsAudioProfile->changed()) {
					SystemConf::getInstance()->set("audio.profile", optionsAudioProfile->getSelected());
					ApiSystem::getInstance()->setAudioOutputProfile(optionsAudioProfile->getSelected());
				}
				SystemConf::getInstance()->saveSystemConf();
			});
		}
	}

#ifdef BATOCERA
	// video rotation
	auto optionsRotation = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SCREEN ROTATION"), false);

	std::string selectedRotation = SystemConf::getInstance()->get("display.rotate");
	if (selectedRotation.empty())
		selectedRotation = "auto";

	optionsRotation->add(_("AUTO"),              "auto", selectedRotation == "auto");
	optionsRotation->add(_("0 DEGREES"),        "0", selectedRotation == "0");
	optionsRotation->add(_("90 DEGREES"),       "1", selectedRotation == "1");
	optionsRotation->add(_("180 DEGREES"),    "2", selectedRotation == "2");
	optionsRotation->add(_("270 DEGREES"),        "3", selectedRotation == "3");

	s->addWithLabel(_("SCREEN ROTATION"), optionsRotation);

	s->addSaveFunc([this, optionsRotation, selectedRotation, s]
	{
	  if (optionsRotation->changed()) 
{
	    SystemConf::getInstance()->set("display.rotate", optionsRotation->getSelected());
	    SystemConf::getInstance()->saveSystemConf();
		s->setVariable("exitreboot", true);
	  }
	});

	// splash
	auto optionsSplash = std::make_shared<OptionListComponent<std::string> >(mWindow, _("BOOT SPLASH"), false);

	std::string enabledSplash = SystemConf::getInstance()->get("splash.screen.enabled");
	std::string soundSplash   = SystemConf::getInstance()->get("splash.screen.sound");

	std::string selectedSplash = "auto";
	if(enabledSplash == "0")      selectedSplash = "nosplash";
	else if(soundSplash   == "0") selectedSplash = "silentsplash";

	optionsSplash->add(_("DEFAULT VIDEO/USER SET SPLASH"),          "auto", selectedSplash == "auto");
	optionsSplash->add(_("SILENT VIDEO/USER SET SPLASH"), "silentsplash", selectedSplash == "silentsplash");
	optionsSplash->add(_("BATOCERA SPLASH IMAGE"),     "nosplash", selectedSplash == "nosplash");

	s->addWithLabel(_("SPLASH SETTING"), optionsSplash);

	s->addSaveFunc([this, optionsSplash, selectedSplash]
	{
	  if (optionsSplash->changed()) {
	    if(optionsSplash->getSelected() == "nosplash") {
	      SystemConf::getInstance()->set("splash.screen.enabled", "0");
	    } else {
	      SystemConf::getInstance()->set("splash.screen.enabled", "1");
	      if(optionsSplash->getSelected() == "silentsplash") {
		SystemConf::getInstance()->set("splash.screen.sound", "0");
	      } else {
		SystemConf::getInstance()->set("splash.screen.sound", "1");
	      }
	    }
	    SystemConf::getInstance()->saveSystemConf();
	  }
	});	
#else
	if (!ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
	{
		// Retroachievements
		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RETROACHIVEMENTS))
			s->addEntry(_("RETROACHIEVEMENT SETTINGS"), true, [this] { openRetroachievementsSettings(); });

		if (SystemData::isNetplayActivated() && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::NETPLAY))
			s->addEntry(_("NETPLAY SETTINGS"), true, [this] { openNetplaySettings(); }, "iconNetplay");

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BIOSINFORMATION))
		{
			s->addEntry(_("MISSING BIOS CHECK"), true, [this, s] { openMissingBiosSettings(); });
			s->addSwitch(_("CHECK BIOS FILES BEFORE RUNNING A GAME"), "CheckBiosesAtLaunch", true);
		}
	}
#endif

#if ODROIDGOA || GAMEFORCE || RK3326
	// multimedia keys
	auto multimediakeys_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("MULTIMEDIA KEYS"));
	multimediakeys_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get("system.multimediakeys.enabled") != "0" && SystemConf::getInstance()->get("system.multimediakeys.enabled") != "1");
	multimediakeys_enabled->add(_("ON"), "1", SystemConf::getInstance()->get("system.multimediakeys.enabled") == "1");
	multimediakeys_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get("system.multimediakeys.enabled") == "0");
	s->addWithLabel(_("MULTIMEDIA KEYS"), multimediakeys_enabled);
	s->addSaveFunc([this, multimediakeys_enabled, s]
	{
		if (multimediakeys_enabled->changed())
		{
			SystemConf::getInstance()->set("system.multimediakeys.enabled", multimediakeys_enabled->getSelected());
			s->setVariable("reboot", true);
		}
	});
#endif

#if GAMEFORCE || RK3326
	auto buttonColor_GameForce = std::make_shared< OptionListComponent<std::string> >(mWindow, _("BUTTON LED COLOR"));
	buttonColor_GameForce->add(_("off"), "off", SystemConf::getInstance()->get("color_rgb") == "off" || SystemConf::getInstance()->get("color_rgb") == "");
	buttonColor_GameForce->add(_("red"), "red", SystemConf::getInstance()->get("color_rgb") == "red");
	buttonColor_GameForce->add(_("green"), "green", SystemConf::getInstance()->get("color_rgb") == "green");
	buttonColor_GameForce->add(_("blue"), "blue", SystemConf::getInstance()->get("color_rgb") == "blue");
	buttonColor_GameForce->add(_("white"), "white", SystemConf::getInstance()->get("color_rgb") == "white");
	buttonColor_GameForce->add(_("purple"), "purple", SystemConf::getInstance()->get("color_rgb") == "purple");
	buttonColor_GameForce->add(_("yellow"), "yellow", SystemConf::getInstance()->get("color_rgb") == "yellow");
	buttonColor_GameForce->add(_("cyan"), "cyan", SystemConf::getInstance()->get("color_rgb") == "cyan");		
	s->addWithLabel(_("BUTTON LED COLOR"), buttonColor_GameForce);
	s->addSaveFunc([buttonColor_GameForce] 
	{
		if (buttonColor_GameForce->changed()) {
			ApiSystem::getInstance()->setButtonColorGameForce(buttonColor_GameForce->getSelected());
			SystemConf::getInstance()->set("color_rgb", buttonColor_GameForce->getSelected());
		}
	});

	auto powerled_GameForce = std::make_shared< OptionListComponent<std::string> >(mWindow, _("POWER LED COLOR"));
	powerled_GameForce->add(_("heartbeat"), "heartbeat", SystemConf::getInstance()->get("option_powerled") == "heartbeat" || SystemConf::getInstance()->get("option_powerled") == "");
	powerled_GameForce->add(_("off"), "off", SystemConf::getInstance()->get("option_powerled") == "off");
	powerled_GameForce->add(_("on"), "on", SystemConf::getInstance()->get("option_powerled") == "on");	
	s->addWithLabel(_("POWER LED COLOR"), powerled_GameForce);
	s->addSaveFunc([powerled_GameForce] 
	{
		if (powerled_GameForce->changed()) {
			ApiSystem::getInstance()->setPowerLedGameForce(powerled_GameForce->getSelected());
			SystemConf::getInstance()->set("option_powerled", powerled_GameForce->getSelected());
		}
	});
#endif

	// Overclock choice
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::OVERCLOCK))
	{
		auto overclock_choice = std::make_shared<OptionListComponent<std::string>>(window, _("OVERCLOCK"), false);

		std::string currentOverclock = Settings::getInstance()->getString("Overclock");
		if (currentOverclock == "")
			currentOverclock = "none";

		std::vector<std::string> availableOverclocking = ApiSystem::getInstance()->getAvailableOverclocking();

		// Overclocking device
		bool isOneSet = false;
		for (auto it = availableOverclocking.begin(); it != availableOverclocking.end(); it++)
		{
			std::vector<std::string> tokens = Utils::String::split(*it, ' ');
			if (tokens.size() >= 2)
			{
				// concatenat the ending words
				std::string vname;
				for (unsigned int i = 1; i < tokens.size(); i++)
				{
					if (i > 1) vname += " ";
					vname += tokens.at(i);
				}
				bool isSet = currentOverclock == std::string(tokens.at(0));
				if (isSet)
					isOneSet = true;

				if (vname == "NONE" || vname == "none")
					vname = _("NONE");

				overclock_choice->add(vname, tokens.at(0), isSet);
			}
		}

		if (isOneSet == false)
		{
			if (currentOverclock == "none")
				overclock_choice->add(_("NONE"), currentOverclock, true);
			else
				overclock_choice->add(currentOverclock, currentOverclock, true);
		}

		// overclocking
		s->addWithLabel(_("OVERCLOCK"), overclock_choice);

		s->addSaveFunc([overclock_choice, window, s]
		{
			if (overclock_choice->changed() && Settings::getInstance()->setString("Overclock", overclock_choice->getSelected()))
			{
				ApiSystem::getInstance()->setOverclock(overclock_choice->getSelected());
				s->setVariable("reboot", true);
			}
		});
	}

#ifdef BATOCERA
	s->addEntry(_("DMD"), true, [this] { openDmdSettings(); });
#endif

#ifdef BATOCERA
#ifdef X86_64
        s->addEntry(_("MULTISCREENS"), true, [this] { openMultiScreensSettings(); });
#endif
#endif

#ifdef BATOCERA
#ifdef X86_64
	int red, green, blue;
	if (ApiSystem::getInstance()->getLED(red, green, blue)) {
		s->addGroup(_("LED HARDWARE"));

		auto redLEDComponent = std::make_shared<SliderComponent>(mWindow, 0.f, 255.f, 1.f);
		redLEDComponent->setValue(red);
		redLEDComponent->setOnValueChanged([](const float &newVal) {
			int red, green, blue;
			ApiSystem::getInstance()->getLEDColours(red, green, blue);
			int redInt = static_cast<int>(newVal);
			ApiSystem::getInstance()->setLEDColours(redInt, green, blue);
			std::string colourString = std::to_string(redInt) + " " + std::to_string(green) + " " + std::to_string(blue);
			SystemConf::getInstance()->set("led.colour", colourString);
		});

		s->addWithLabel(_("RED"), redLEDComponent);

		auto greenLEDComponent = std::make_shared<SliderComponent>(mWindow, 0.f, 255.f, 1.f);
		greenLEDComponent->setValue(green);
		greenLEDComponent->setOnValueChanged([](const float &newVal) {
			int red, green, blue;
			ApiSystem::getInstance()->getLEDColours(red, green, blue);
			int greenInt = static_cast<int>(newVal);
			ApiSystem::getInstance()->setLEDColours(red, greenInt, blue);
			std::string colourString = std::to_string(red) + " " + std::to_string(greenInt) + " " + std::to_string(blue);
			SystemConf::getInstance()->set("led.colour", colourString);
		});

		s->addWithLabel(_("GREEN"), greenLEDComponent);

		auto blueLEDComponent = std::make_shared<SliderComponent>(mWindow, 0.f, 255.f, 1.f);
		blueLEDComponent->setValue(blue);
		blueLEDComponent->setOnValueChanged([](const float &newVal) {
			int red, green, blue;
			ApiSystem::getInstance()->getLEDColours(red, green, blue);
			int blueInt = static_cast<int>(newVal);
			ApiSystem::getInstance()->setLEDColours(red, green, blueInt);
			std::string colourString = std::to_string(red) + " " + std::to_string(green) + " " + std::to_string(blueInt);
			SystemConf::getInstance()->set("led.colour", colourString);
		});

		s->addWithLabel(_("BLUE"), blueLEDComponent);
	}
	
	// LED brightness
	int ledBrightness;
	if (ApiSystem::getInstance()->getLEDBrightness(ledBrightness)) {
		auto ledBrightnessComponent = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
		ledBrightnessComponent->setValue(ledBrightness);
		ledBrightnessComponent->setOnValueChanged([](const float &newVal)
		{
			ApiSystem::getInstance()->setLEDBrightness((int)Math::round(newVal));
			SystemConf::getInstance()->set("led.brightness", std::to_string((int)Math::round(newVal)));
		});

		s->addWithLabel(_("LED BRIGHTNESS"), ledBrightnessComponent);
	}
#endif
#endif

#ifdef BATOCERA
	s->addGroup(_("STORAGE"));

	// Storage device
	std::vector<std::string> availableStorage = ApiSystem::getInstance()->getAvailableStorageDevices();
	if (availableStorage.size())
	{		
		std::string selectedStorage = ApiSystem::getInstance()->getCurrentStorage();

		auto optionsStorage = std::make_shared<OptionListComponent<std::string> >(window, _("STORAGE DEVICE"), false);
		for (auto it = availableStorage.begin(); it != availableStorage.end(); it++)
		{
				if (Utils::String::startsWith(*it, "DEV"))
				{
					std::vector<std::string> tokens = Utils::String::split(*it, ' ');

					if (tokens.size() >= 3) {
						// concatenat the ending words
						std::string vname = "";
						for (unsigned int i = 2; i < tokens.size(); i++) {
							if (i > 2) vname += " ";
							vname += tokens.at(i);
						}
						optionsStorage->add(vname, (*it), selectedStorage == std::string("DEV " + tokens.at(1)));
					}
				} else {
				  std::vector<std::string> tokens = Utils::String::split(*it, ' ');
				  if (tokens.size() == 1) {
					optionsStorage->add((*it), (*it), selectedStorage == (*it));
				  } else {
				    // concatenat the ending words
				    std::string vname = "";
				    for (unsigned int i = 1; i < tokens.size(); i++) {
				      if (i > 1) vname += " ";
				      vname += tokens.at(i);
				    }
				    optionsStorage->add(_(vname.c_str()), tokens.at(0), selectedStorage == tokens.at(0));
				  }
				}
		}

		s->addWithLabel(_("STORAGE DEVICE"), optionsStorage);
		s->addSaveFunc([optionsStorage, selectedStorage, s]
		{
			if (optionsStorage->changed())
			{
				ApiSystem::getInstance()->setStorage(optionsStorage->getSelected());
				s->setVariable("reboot", true);
			}
		});
	}

	// backup
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BACKUP))
		s->addEntry(_("BACKUP USER DATA"), true, [this] { mWindow->pushGui(new GuiBackupStart(mWindow)); });

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::INSTALL))
		s->addEntry(_("INSTALL ON A NEW DISK"), true, [this] { mWindow->pushGui(new GuiInstallStart(mWindow)); });
	
	s->addGroup(_("ADVANCED"));

	if(ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SERVICES)) {
	  // Services
	  if (isFullUI)
	    s->addEntry(_("SERVICES"), true, [this] { openServicesSettings(); });
	}

	// Security
	s->addEntry(_("SECURITY"), true, [this, s] 
	{
		GuiSettings *securityGui = new GuiSettings(mWindow, _("SECURITY").c_str());
		auto securityEnabled = std::make_shared<SwitchComponent>(mWindow);
		securityEnabled->setState(SystemConf::getInstance()->get("system.security.enabled") == "1");
		securityGui->addWithDescription(_("ENFORCE SECURITY"), _("Require a password for accessing the network share."), securityEnabled);

		auto rootpassword = std::make_shared<TextComponent>(mWindow, ApiSystem::getInstance()->getRootPassword(), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);
		securityGui->addWithLabel(_("ROOT PASSWORD"), rootpassword);

		securityGui->addSaveFunc([this, securityEnabled, s] 
		{
			Window* window = this->mWindow;

			if (securityEnabled->changed()) 
			{
				SystemConf::getInstance()->set("system.security.enabled", securityEnabled->getState() ? "1" : "0");
				SystemConf::getInstance()->saveSystemConf();
				s->setVariable("reboot", true);				
			}
		});
		mWindow->pushGui(securityGui);
	});
#else
	if (isFullUI)
	{
		s->addGroup(_("ADVANCED"));

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SERVICES) && ApiSystem::getInstance()->getServices().size())
			s->addEntry(_("SERVICES"), true, [this] { openServicesSettings(); });
	}
#endif
	
	// Developer options
	if (isFullUI)
		s->addEntry(_("FRONTEND DEVELOPER OPTIONS"), true, [this] { openDeveloperSettings(); });

	auto pthis = this;
	s->onFinalize([s, pthis, window]
	{
		if (s->getVariable("exitreboot") && Settings::getInstance()->getBool("ExitOnRebootRequired"))
		{
			Utils::Platform::quitES(Utils::Platform::QuitMode::QUIT);
			return;
		}
		
		if (s->getVariable("reboot") || s->getVariable("exitreboot"))
			window->displayNotificationMessage(_U("\uF011  ") + _("REBOOT REQUIRED TO APPLY THE NEW CONFIGURATION"));

		if (s->getVariable("reloadGuiMenu"))
		{
			delete pthis;
			window->pushGui(new GuiMenu(window, false));
		}
	});

	mWindow->pushGui(s);
}

void GuiMenu::openLatencyReductionConfiguration(Window* mWindow, std::string configName)
{
	GuiSettings* guiLatency = new GuiSettings(mWindow, _("LATENCY REDUCTION").c_str());

	// run-ahead
	auto runahead_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("RUN-AHEAD FRAMES"));
	runahead_enabled->addRange({ { _("AUTO"), "" }, { _("NONE"), "0" }, { "1", "1" }, { "2", "2" }, { "3", "3" }, { "4", "4" }, { "5", "5" }, { "6", "6" } }, SystemConf::getInstance()->get(configName + ".runahead"));
	guiLatency->addWithDescription(_("RUN-AHEAD FRAMES"), _("High numbers can result in visible jitter."), runahead_enabled);
	guiLatency->addSaveFunc([configName, runahead_enabled] { SystemConf::getInstance()->set(configName + ".runahead", runahead_enabled->getSelected()); });

	// second instance
	auto secondinstance = std::make_shared<OptionListComponent<std::string>>(mWindow, _("USE SECOND INSTANCE FOR RUN-AHEAD"));
	secondinstance->addRange({ { _("AUTO"), "" }, { _("ON"), "1" }, { _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".secondinstance"));
	guiLatency->addWithDescription(_("USE SECOND INSTANCE FOR RUN-AHEAD"), _("Can prevent audio skips on button presses."), secondinstance);
	guiLatency->addSaveFunc([configName, secondinstance] { SystemConf::getInstance()->set(configName + ".secondinstance", secondinstance->getSelected()); });

	// auto frame delay
	auto video_frame_delay_auto = std::make_shared<OptionListComponent<std::string>>(mWindow, _("AUTOMATIC FRAME DELAY"));
	video_frame_delay_auto->addRange({ { _("AUTO"), "" }, { _("ON"), "1" }, { _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".video_frame_delay_auto"));
	guiLatency->addWithDescription(_("AUTO FRAME DELAY"), _("Automatically decrease frame delay temporarily to prevent frame drops. Can introduce stuttering."), video_frame_delay_auto);
	guiLatency->addSaveFunc([configName, video_frame_delay_auto] { SystemConf::getInstance()->set(configName + ".video_frame_delay_auto", video_frame_delay_auto->getSelected()); });

	// variable refresh rate (freesync, gsync, etc.)
	auto vrr_runloop_enable = std::make_shared<OptionListComponent<std::string>>(mWindow, _("VARIABLE REFRESH RATE (G-SYNC, FREESYNC)"));
	vrr_runloop_enable->addRange({ { _("AUTO"), "" }, { _("ON"), "1" }, { _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".vrr_runloop_enable"));
	guiLatency->addWithDescription(_("VARIABLE REFRESH RATE"), _("Don't deviate from core requested timing. G-Sync, FreeSync, HDMI 2.1 VRR."), vrr_runloop_enable);
	guiLatency->addSaveFunc([configName, vrr_runloop_enable] { SystemConf::getInstance()->set(configName + ".vrr_runloop_enable", vrr_runloop_enable->getSelected()); });

	mWindow->pushGui(guiLatency);
}

void GuiMenu::openRetroachievementsSettings()
{
	mWindow->pushGui(new GuiRetroAchievementsSettings(mWindow));
}

void GuiMenu::openNetplaySettings()
{
	mWindow->pushGui(new GuiNetPlaySettings(mWindow));	
}

void GuiMenu::addDecorationSetOptionListComponent(Window* window, GuiSettings* parentWindow, const std::vector<DecorationSetInfo>& sets, const std::string& configName)
{
	auto decorations = std::make_shared<OptionListComponent<std::string> >(window, _("DECORATION SET"), false);
	decorations->setRowTemplate([window, sets](std::string data, ComponentListRow& row) { createDecorationItemTemplate(window, sets, data, row); });

	std::vector<std::string> items = { _("AUTO"), _("NONE") };
	for (auto set : sets)
		items.push_back(set.name);

	std::string bezel = SystemConf::getInstance()->get(configName + ".bezel");

	for (auto item : items)
		decorations->add(item, item, (bezel == item) || (bezel == "none" && item == _("NONE")) || (bezel == "" && item == _("AUTO")));

	if (!decorations->hasSelection())
		decorations->selectFirstItem();

	parentWindow->addWithLabel(_("DECORATION SET"), decorations);
	parentWindow->addSaveFunc([decorations, configName]
	{
		SystemConf::getInstance()->set(configName + ".bezel", decorations->getSelected() == _("NONE") ? "none" : decorations->getSelected() == _("AUTO") ? "" : decorations->getSelected());
	});
}



void GuiMenu::addFeatureItem(Window* window, GuiSettings* settings, const CustomFeature& feat, const std::string& configName, const std::string& system, const std::string& emulator, const std::string& core)
{	
	if (feat.preset == "hidden")
		return;

	std::string storageName = configName + "." + feat.value;
	
	if (configName == "global" && Utils::String::startsWith(feat.value, configName + "."))
		storageName = feat.value;
			
	if (feat.preset == "input")
	{
		settings->addInputTextRow(pgettext("game_options", feat.name.c_str()), storageName, false);
		return;
	}
	
	if (feat.preset == "password")
	{
		settings->addInputTextRow(pgettext("game_options", feat.name.c_str()), storageName, true);
		return;
	}
	
	if (feat.preset == "image")
	{
		settings->addFileBrowser(pgettext("game_options", feat.name.c_str()), storageName, GuiFileBrowser::IMAGES);
		return;
	}

	if (feat.preset == "video")
	{
		settings->addFileBrowser(pgettext("game_options", feat.name.c_str()), storageName, GuiFileBrowser::VIDEO);
		return;
	}

	if (feat.preset == "folder")
	{
		settings->addFileBrowser(pgettext("game_options", feat.name.c_str()), storageName, GuiFileBrowser::DIRECTORY);
		return;
	}

	if (feat.preset == "document")
	{
		settings->addFileBrowser(pgettext("game_options", feat.name.c_str()), storageName, GuiFileBrowser::MANUALS);
		return;
	}

	std::string storedValue = SystemConf::getInstance()->get(storageName);
	
	std::string inheritedValue;
	if (!Utils::String::startsWith(storageName, "global."))
	{
		std::string systemSetting = storageName;

		bool querySystemSetting = false;

		// Look if we are using a "per-game" setting, then compute the system setting name
		auto gameInfoStart = storageName.find("[\"");
		if (gameInfoStart != std::string::npos)
		{
			auto gameInfoEnd = storageName.find("\"]");
			if (gameInfoEnd != std::string::npos)
			{
				systemSetting = storageName.substr(0, gameInfoStart) + storageName.substr(gameInfoEnd + 2);
				querySystemSetting = true;
			}
		}
	
		// First find the global option
		auto dotPos = systemSetting.find(".");
		if (dotPos != std::string::npos)
		{
			std::string globalSetting = "global." + systemSetting.substr(dotPos + 1);
			std::string globalStoredValue = SystemConf::getInstance()->get(globalSetting);
			if (!globalStoredValue.empty() && globalStoredValue != "auto" && globalStoredValue != storedValue)
				inheritedValue = globalStoredValue;
		}

		// Then take the system option
		if (querySystemSetting)
		{
			std::string systemStoredValue = SystemConf::getInstance()->get(systemSetting);
			if (!systemStoredValue.empty() && systemStoredValue != "auto" && systemStoredValue != storedValue)
				inheritedValue = systemStoredValue;
		}
	}

	if (feat.preset == "switch")
	{
		auto switchComponent = std::make_shared<SwitchComponent>(window);
		switchComponent->setState(storedValue == "1");

		if (!feat.description.empty())
			settings->addWithDescription(pgettext("game_options", feat.name.c_str()), pgettext("game_options", feat.description.c_str()), switchComponent);
		else
			settings->addWithLabel(pgettext("game_options", feat.name.c_str()), switchComponent);

		settings->addSaveFunc([storageName, switchComponent] { SystemConf::getInstance()->set(storageName, switchComponent->getState() ? "1" : ""); });
		return;
	}

	if (feat.preset == "switchon")
	{
		auto switchComponent = std::make_shared<SwitchComponent>(window);
		switchComponent->setState(storedValue != "0");

		if (!feat.description.empty())
			settings->addWithDescription(pgettext("game_options", feat.name.c_str()), pgettext("game_options", feat.description.c_str()), switchComponent);
		else
			settings->addWithLabel(pgettext("game_options", feat.name.c_str()), switchComponent);

		settings->addSaveFunc([storageName, switchComponent] { SystemConf::getInstance()->set(storageName, switchComponent->getState() ? "" : "0"); });
		return;
	}

	if (feat.preset == "switchoff")
	{
		auto switchComponent = std::make_shared<SwitchComponent>(window);
		switchComponent->setState(storedValue != "1");

		if (!feat.description.empty())
			settings->addWithDescription(pgettext("game_options", feat.name.c_str()), pgettext("game_options", feat.description.c_str()), switchComponent);
		else
			settings->addWithLabel(pgettext("game_options", feat.name.c_str()), switchComponent);

		settings->addSaveFunc([storageName, switchComponent] { SystemConf::getInstance()->set(storageName, switchComponent->getState() ? "" : "1"); });
		return;
	}

	if (feat.preset == "slider")
	{
		std::vector<std::string> tokens = Utils::String::split(feat.preset_parameters, ' ');

		float slider_from    =   0.0f;
		float slider_to      = 100.0f;
		float slider_step    =   1.0f;
		float slider_default =   0.0f;
		std::string slider_suffix = "";

		if (tokens.size() >= 1) slider_from    = Utils::String::toFloat(tokens.at(0));
		if (tokens.size() >= 2) slider_to      = Utils::String::toFloat(tokens.at(1));
		if (tokens.size() >= 3) slider_step    = Utils::String::toFloat(tokens.at(2));
		if (tokens.size() >= 4) slider_default = Utils::String::toFloat(tokens.at(3));
		if (tokens.size() >= 5) slider_suffix  = tokens.at(4);

		auto sliderComponent = std::make_shared<SliderComponent>(window, slider_from, slider_to, slider_step, slider_suffix);
		if(storedValue == "") {
		  sliderComponent->setValue(slider_default);
		} else {
		  sliderComponent->setValue(Utils::String::toFloat(storedValue));
		}

		if (!feat.description.empty())
			settings->addWithDescription(pgettext("game_options", feat.name.c_str()), pgettext("game_options", feat.description.c_str()), sliderComponent);
		else
			settings->addWithLabel(pgettext("game_options", feat.name.c_str()), sliderComponent);

		settings->addSaveFunc([storageName, sliderComponent] { SystemConf::getInstance()->set(storageName, std::to_string(sliderComponent->getValue())); });
		return;
	}

	auto item = std::make_shared<OptionListComponent<std::string>>(window, pgettext("game_options", feat.name.c_str()));

	if (feat.preset == "shaders" || feat.preset == "shaderset")
	{
		item->add(_("AUTO"), "auto", storedValue.empty() || storedValue == "auto");

		auto shaders = ApiSystem::getInstance()->getShaderList(configName != "global" ? system : "", configName != "global" ? emulator : "", configName != "global" ? core : "");
		if (shaders.size() > 0)
		{
			item->add(_("NONE"), "none", storedValue == "none");

			for (auto shader : shaders)
			  item->add(pgettext("game_options", Utils::String::toUpper(shader).c_str()), shader, storedValue == shader);
		}
	}
	else if (feat.preset == "videofilters")
	{
		item->add(_("AUTO"), "auto", storedValue.empty() || storedValue == "auto");

		auto videofilters = ApiSystem::getInstance()->getVideoFilterList(configName != "global" ? system : "", configName != "global" ? emulator : "", configName != "global" ? core : "");
		if (videofilters.size() > 0)
		{
			item->add(_("NONE"), "none", storedValue == "none");

			for (auto videofilter : videofilters)
				item->add(pgettext("game_options", Utils::String::toUpper(videofilter).c_str()), videofilter, storedValue == videofilter);
		}
	}
	else if (feat.preset == "decorations" || feat.preset == "bezel")
	{
		item->add(_("AUTO"), "auto", storedValue.empty() || storedValue == "auto");

		auto sets = GuiMenu::getDecorationsSets(ViewController::get()->getState().getSystem());
		if (sets.size() > 0)
		{
			item->setRowTemplate([window, sets](std::string data, ComponentListRow& row) { createDecorationItemTemplate(window, sets, data, row); });
			item->add(_("NONE"), "none", storedValue == "none");

			for (auto set : sets)
				item->add(set.name, set.name, storedValue == set.name);
		}
	}
	else if (feat.preset == "videomodes" || feat.preset == "videomode")
	{
		item->add(_("AUTO"), "auto", storedValue.empty() || storedValue == "auto");

		auto modes = ApiSystem::getInstance()->getVideoModes();
		for (auto videoMode : modes)
		{
			std::vector<std::string> tokens = Utils::String::split(videoMode, ':');
			if (tokens.size() == 0)
				continue;

			std::string vname;
			for (unsigned int i = 1; i < tokens.size(); i++)
			{
				if (i > 1)
					vname += ":";

				vname += tokens.at(i);
			}

			item->add(_(vname.c_str()), tokens.at(0), storedValue == tokens.at(0));
		}
	}
	else if (feat.preset == "runners")
	{
		item->add(_("AUTO"), "auto", storedValue.empty() || storedValue == "auto");

		auto runners = ApiSystem::getInstance()->getCustomRunners();
		for (auto customRunner : runners)
		{
			item->add(_(customRunner.c_str()), customRunner, storedValue == customRunner);
		}
	}
	else
	{
		item->add(_("AUTO"), "", storedValue.empty() || storedValue == "auto");

		for (auto fval : feat.choices)
			item->add(pgettext("game_options", fval.name.c_str()), fval.value, storedValue == fval.value);
	}

	if (!item->hasSelection())
		item->selectFirstItem();

	std::string desc = pgettext("game_options", feat.description.c_str());

	if (!inheritedValue.empty())
	{
		auto displayName = item->getItemDisplayName(inheritedValue);
		if (!displayName.empty())
		{
			if (desc.empty())
				desc = _("Current setting") + " : " + displayName;
			else
				desc = desc + "\r\n" + _("Current setting") + " : " + displayName;
		}
	}

	if (!desc.empty())
		settings->addWithDescription(pgettext("game_options", feat.name.c_str()), desc, item);
	else
		settings->addWithLabel(pgettext("game_options", feat.name.c_str()), item);

	settings->addSaveFunc([item, storageName] { SystemConf::getInstance()->set(storageName, item->getSelected()); });
}

static bool hasGlobalFeature(const std::string& name)
{
	return CustomFeatures::GlobalFeatures.hasGlobalFeature(name);
}

static std::string getFeatureMenuDescription(const std::string& configName, const VectorEx<CustomFeature>& items)
{
	std::string description;

	for (auto item : items)
	{
		std::string storageName = configName + "." + item.value;
		std::string storedValue = SystemConf::getInstance()->get(storageName);
		if (!storedValue.empty())
		{
			std::string text = pgettext("game_options", item.name.c_str());

			for (auto ch : item.choices)
			{
				if (ch.value == storedValue)
				{
					storedValue = ch.name;
					break;
				}
			}

			text += " : " + Utils::String::toUpper(storedValue);

			if (description.empty())
				description = text;
			else
				description = description + "\r\n" + text;
		}
	}

	return description;
}


void GuiMenu::addFeatures(const VectorEx<CustomFeature>& features, Window* window, GuiSettings* settings, const std::string& configName, const std::string& system, const std::string& emulator, const std::string& core, const std::string& defaultGroupName, bool addDefaultGroupOnlyIfNotFirst)
{
	bool firstGroup = true;
	
	auto groups = features.groupBy([](auto x) { return x.group; });
	for (auto group : groups)
	{
		settings->removeLastRowIfGroup();

		if (!group.first.empty())
			settings->addGroup(pgettext("game_options", group.first.c_str()));
		else if (!defaultGroupName.empty())
		{
			if (!addDefaultGroupOnlyIfNotFirst || !firstGroup)
				settings->addGroup(defaultGroupName); // _("DEFAULT GLOBAL SETTINGS")
		}

		firstGroup = false;

		std::set<std::string> processed;

		for (auto feat : group.second)
		{
			if (feat.submenu.empty())
			{
				addFeatureItem(window, settings, feat, configName, system, emulator, core);
				continue;
			}

			if (processed.find(feat.submenu) != processed.cend())
				continue;

			processed.insert(feat.submenu);

			auto items = features.where([feat](auto x) { return x.preset != "hidden" && x.submenu == feat.submenu; });
			if (items.size() > 0)
			{
				std::string label = Utils::String::toUpper(pgettext("game_options", feat.submenu.c_str()));
				std::string description = getFeatureMenuDescription(configName, items);

				std::shared_ptr<MultiLineMenuEntry> entry = std::make_shared<MultiLineMenuEntry>(window, label, description, true);

				ComponentListRow row;
				row.addElement(entry, true);

				auto arrow = makeArrow(window);
				if (EsLocale::isRTL()) arrow->setFlipX(true);
				row.addElement(arrow, false);

				row.makeAcceptInputHandler([window, configName, feat, items, system, emulator, core, settings, entry]
				{
					GuiSettings* groupSettings = new GuiSettings(window, pgettext("game_options", feat.submenu.c_str()));

					for (auto feat : items)
						addFeatureItem(window, groupSettings, feat, configName, system, emulator, core);

					groupSettings->addSaveFunc([settings, entry, configName, items]
					{
						if (entry != nullptr)
						{
							std::string newDesc = getFeatureMenuDescription(configName, items);
							if (newDesc != entry->getDescription())
							{
								entry->setDescription(newDesc);
								settings->updateSize();
							}
						}
					});

					window->pushGui(groupSettings);
				});

				settings->addRow(row);

				/*
				settings->addEntry(pgettext("game_options", feat.submenu.c_str()), true, [window, configName, feat, items, system, emulator, core]
				{
					GuiSettings* groupSettings = new GuiSettings(window, pgettext("game_options", feat.submenu.c_str()));

					for (auto feat : items)
						addFeatureItem(window, groupSettings, feat, configName, system, emulator, core);

					window->pushGui(groupSettings);
				});*/
			}
		}
	}
}

void GuiMenu::openGamesSettings() 
{
	Window* window = mWindow;

	auto s = new GuiSettings(mWindow, _("GAME SETTINGS").c_str());

	s->addGroup(_("TOOLS"));

	// Game List Update
	s->addEntry(_("UPDATE GAMELISTS"), false, [this, window] { updateGameLists(window); });

	if (SystemConf::getInstance()->getBool("global.retroachievements") && !Settings::getInstance()->getBool("RetroachievementsMenuitem") && SystemConf::getInstance()->get("global.retroachievements.username") != "")
	{
		s->addEntry(_("RETROACHIEVEMENTS").c_str(), true, [this] 
		{ 
			if (!checkNetwork())
				return;

			GuiRetroAchievements::show(mWindow); 
		});
	}
	

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RETROACHIVEMENTS) || (SystemData::isNetplayActivated() && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::NETPLAY)))
		s->addGroup(_("ACCOUNTS"));

	// Retroachievements
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RETROACHIVEMENTS))
		s->addEntry(_("RETROACHIEVEMENT SETTINGS"), true, [this] { openRetroachievementsSettings(); });

	// Netplay
	if (SystemData::isNetplayActivated() && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::NETPLAY))
		s->addEntry(_("NETPLAY SETTINGS"), true, [this] { openNetplaySettings(); }, "iconNetplay");

	// Missing Bios
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BIOSINFORMATION))
	{
		s->addGroup(_("BIOS SETTINGS"));
		s->addEntry(_("MISSING BIOS CHECK"), true, [this, s] { openMissingBiosSettings(); });

		auto checkBiosesAtLaunch = std::make_shared<SwitchComponent>(mWindow);
		checkBiosesAtLaunch->setState(Settings::getInstance()->getBool("CheckBiosesAtLaunch"));
		s->addWithLabel(_("CHECK BIOS FILES BEFORE RUNNING A GAME"), checkBiosesAtLaunch);
		s->addSaveFunc([checkBiosesAtLaunch] { Settings::getInstance()->setBool("CheckBiosesAtLaunch", checkBiosesAtLaunch->getState()); });
	}

	// Custom config for systems
	s->addGroup(_("SAVESTATES"));

	// AUTO SAVE/LOAD
	auto autosave_enabled = std::make_shared<SwitchComponent>(mWindow);
	autosave_enabled->setState(SystemConf::getInstance()->get("global.autosave") == "1");
	s->addWithDescription(_("AUTO SAVE/LOAD"), _("Load latest savestate on game launch and savestate when exiting game."), autosave_enabled);
	s->addSaveFunc([autosave_enabled] { SystemConf::getInstance()->set("global.autosave", autosave_enabled->getState() ? "1" : ""); });

	// INCREMENTAL SAVESTATES
	auto incrementalSaveStates = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INCREMENTAL SAVESTATES"));
	incrementalSaveStates->addRange({
		{ _("INCREMENT PER SAVE"), _("Never overwrite old savestates, always make new ones."), "" }, // Don't use 1 -> 1 is YES, auto too
		{ _("INCREMENT SLOT"), _("Increment slot on a new game."), "0" },
		{ _("DO NOT INCREMENT"), _("Use current slot on a new game."), "2" } },
		SystemConf::getInstance()->get("global.incrementalsavestates"));

	s->addWithLabel(_("INCREMENTAL SAVESTATES"), incrementalSaveStates);
	s->addSaveFunc([incrementalSaveStates] { SystemConf::getInstance()->set("global.incrementalsavestates", incrementalSaveStates->getSelected()); });

	// SHOW SAVE STATES
	auto showSaveStates = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW SAVESTATE MANAGER"));
	showSaveStates->addRange({ { _("NO"), "auto" },{ _("ALWAYS") , "1" },{ _("IF NOT EMPTY") , "2" } }, SystemConf::getInstance()->get("global.savestates"));
	s->addWithDescription(_("SHOW SAVESTATE MANAGER"), _("Display savestate manager before launching a game."), showSaveStates);
	s->addSaveFunc([showSaveStates] { SystemConf::getInstance()->set("global.savestates", showSaveStates->getSelected()); });

	s->addGroup(_("DEFAULT GLOBAL SETTINGS"));

	// Screen ratio choice
	if (!hasGlobalFeature("ratio"))
	{
		auto ratio_choice = createRatioOptionList(mWindow, "global");
		s->addWithDescription(_("GAME ASPECT RATIO"), _("Force the game to render in this aspect ratio."), ratio_choice);
		s->addSaveFunc([ratio_choice] { SystemConf::getInstance()->set("global.ratio", ratio_choice->getSelected()); });
	}

	// video resolution mode
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RESOLUTION) && !hasGlobalFeature("videomode"))
	{
		auto videoModeOptionList = createVideoResolutionModeOptionList(mWindow, "global");
		s->addWithDescription(_("VIDEO MODE"), _("Sets the display's resolution. Does not affect the rendering resolution."), videoModeOptionList);
		s->addSaveFunc([this, videoModeOptionList] { SystemConf::getInstance()->set("global.videomode", videoModeOptionList->getSelected()); });
	}

	// smoothing	
	if (!hasGlobalFeature("smooth"))
	{
		auto smoothing_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SMOOTH GAMES (BILINEAR FILTERING)"));
		smoothing_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF") , "0" } }, SystemConf::getInstance()->get("global.smooth"));
		s->addWithLabel(_("SMOOTH GAMES (BILINEAR FILTERING)"), smoothing_enabled);
		s->addSaveFunc([smoothing_enabled] { SystemConf::getInstance()->set("global.smooth", smoothing_enabled->getSelected()); });
	}

	// rewind
	if (!hasGlobalFeature("rewind"))
	{
		auto rewind_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("REWIND"));
		rewind_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF") , "0" } }, SystemConf::getInstance()->get("global.rewind"));
		s->addWithDescription(_("REWIND"), _("Store past states to rewind to in realtime, if the core supports it."), rewind_enabled);
		s->addSaveFunc([rewind_enabled] { SystemConf::getInstance()->set("global.rewind", rewind_enabled->getSelected()); });
	}
	
	// Integer scale
	if (!hasGlobalFeature("integerscale"))
	{
		auto integerscale_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INTEGER SCALING (PIXEL PERFECT)"));
		integerscale_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF") , "0" } }, SystemConf::getInstance()->get("global.integerscale"));
		s->addWithLabel(_("INTEGER SCALING (PIXEL PERFECT)"), integerscale_enabled);
		s->addSaveFunc([integerscale_enabled] { SystemConf::getInstance()->set("global.integerscale", integerscale_enabled->getSelected()); });
	}

	// Shaders preset
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SHADERS) && !hasGlobalFeature("shaderset"))
	{
		auto installedShaders = ApiSystem::getInstance()->getShaderList("", "", "");
		if (installedShaders.size() > 0)
		{
			std::string currentShader = SystemConf::getInstance()->get("global.shaderset");

			auto shaders_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SHADER SET"), false);			
			shaders_choices->add(_("AUTO"), "auto", currentShader.empty() || currentShader == "auto");
			shaders_choices->add(_("NONE"), "none", currentShader == "none");

			for (auto shader : installedShaders)
				shaders_choices->add(_(Utils::String::toUpper(shader).c_str()), shader, currentShader == shader);
			
			if (!shaders_choices->hasSelection())
				shaders_choices->selectFirstItem();

			s->addWithLabel(_("SHADER SET"), shaders_choices);
			s->addSaveFunc([shaders_choices] { SystemConf::getInstance()->set("global.shaderset", shaders_choices->getSelected()); });
		}
	}

	// Video Filters
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::VIDEOFILTERS) && !hasGlobalFeature("videofilters"))
	{
		auto installedVideofilters = ApiSystem::getInstance()->getVideoFilterList("", "", "");
		if (installedVideofilters.size() > 0)
		{
			std::string currentVideofilter = SystemConf::getInstance()->get("global.videofilters");

			auto videofilters_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("VIDEO FILTER"), false);
			videofilters_choices->add(_("AUTO"), "auto", currentVideofilter.empty() || currentVideofilter == "auto");
			videofilters_choices->add(_("NONE"), "none", currentVideofilter == "none");

			for (auto videofilter : installedVideofilters)
				videofilters_choices->add(_(Utils::String::toUpper(videofilter).c_str()), videofilter, currentVideofilter == videofilter);

			if (!videofilters_choices->hasSelection())
				videofilters_choices->selectFirstItem();

			s->addWithLabel(_("VIDEO FILTER"), videofilters_choices);
			s->addSaveFunc([videofilters_choices] { SystemConf::getInstance()->set("global.videofilters", videofilters_choices->getSelected()); });
		}
	}

	// decorations
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DECORATIONS) && !hasGlobalFeature("bezel"))
	{		
		auto sets = GuiMenu::getDecorationsSets(ViewController::get()->getState().getSystem());
		if (sets.size() > 0)
		{
#ifdef BATOCERA
			s->addEntry(_("DECORATIONS"), true, [this, sets]
			{
				GuiSettings *decorations_window = new GuiSettings(mWindow, _("DECORATIONS").c_str());

				addDecorationSetOptionListComponent(mWindow, decorations_window, sets);

				// stretch bezels
				auto bezel_stretch_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("STRETCH BEZELS (4K & ULTRAWIDE)"));
				bezel_stretch_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.bezel_stretch") != "0" && SystemConf::getInstance()->get("global.bezel_stretch") != "1");
				bezel_stretch_enabled->add(_("ON"), "1", SystemConf::getInstance()->get("global.bezel_stretch") == "1");
				bezel_stretch_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get("global.bezel_stretch") == "0");
				decorations_window->addWithLabel(_("STRETCH BEZELS (4K & ULTRAWIDE)"), bezel_stretch_enabled);
				decorations_window->addSaveFunc([bezel_stretch_enabled] {
						if (bezel_stretch_enabled->changed()) {
						SystemConf::getInstance()->set("global.bezel_stretch", bezel_stretch_enabled->getSelected());
						SystemConf::getInstance()->saveSystemConf();
						}
						});

				// tattoo and controller overlays
				auto bezel_tattoo = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW TATTOO OVER BEZEL"));
				bezel_tattoo->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.bezel.tattoo") != "0"
						&& SystemConf::getInstance()->get("global.bezel.tattoo") != "system"
						&& SystemConf::getInstance()->get("global.bezel.tattoo") != "custom");
				bezel_tattoo->add(_("NO"), "0", SystemConf::getInstance()->get("global.bezel.tattoo") == "0");
				bezel_tattoo->add(_("SYSTEM CONTROLLERS"), "system", SystemConf::getInstance()->get("global.bezel.tattoo") == "system");
				bezel_tattoo->add(_("CUSTOM IMAGE"), "custom", SystemConf::getInstance()->get("global.bezel.tattoo") == "custom");
				decorations_window->addWithDescription(_("SHOW TATTOO OVER BEZEL"), _("Show an image overlaid on top of the bezel."), bezel_tattoo);
				decorations_window->addSaveFunc([bezel_tattoo] {
						if (bezel_tattoo->changed()) {
						SystemConf::getInstance()->set("global.bezel.tattoo", bezel_tattoo->getSelected());
						SystemConf::getInstance()->saveSystemConf();
						}
						});

				auto bezel_tattoo_corner = std::make_shared<OptionListComponent<std::string>>(mWindow, _("TATTOO CORNER"));
				bezel_tattoo_corner->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.bezel.tattoo_corner") != "NW"
						&& SystemConf::getInstance()->get("global.bezel.tattoo_corner") != "NE"
						&& SystemConf::getInstance()->get("global.bezel.tattoo_corner") != "SE"
						&& SystemConf::getInstance()->get("global.bezel.tattoo_corner") != "SW");
				bezel_tattoo_corner->add(_("NORTH WEST"), "NW", SystemConf::getInstance()->get("global.bezel.tattoo_corner") == "NW");
				bezel_tattoo_corner->add(_("NORTH EAST"), "NE", SystemConf::getInstance()->get("global.bezel.tattoo_corner") == "NE");
				bezel_tattoo_corner->add(_("SOUTH EAST"), "SE", SystemConf::getInstance()->get("global.bezel.tattoo_corner") == "SE");
				bezel_tattoo_corner->add(_("SOUTH WEST"), "SW", SystemConf::getInstance()->get("global.bezel.tattoo_corner") == "SW");
				decorations_window->addWithLabel(_("TATTOO CORNER"), bezel_tattoo_corner);
				decorations_window->addSaveFunc([bezel_tattoo_corner] {
						if (bezel_tattoo_corner->changed()) {
						SystemConf::getInstance()->set("global.bezel.tattoo_corner", bezel_tattoo_corner->getSelected());
						SystemConf::getInstance()->saveSystemConf();
						}
						});
				decorations_window->addInputTextRow(_("CUSTOM .PNG IMAGE PATH"), "global.bezel.tattoo_file", false);

				auto bezel_resize_tattoo = std::make_shared<SwitchComponent>(mWindow);
				bezel_resize_tattoo->setState(SystemConf::getInstance()->getBool("global.bezel.resize_tattoo"));
				decorations_window->addWithDescription(_("RESIZE TATTOO"), _("Shrink/expand tattoo to fit within the bezel's border."), bezel_resize_tattoo);
				decorations_window->addSaveFunc([bezel_resize_tattoo]
				{
					if (SystemConf::getInstance()->getBool("global.bezel.resize_tattoo") != bezel_resize_tattoo->getState()) {
						SystemConf::getInstance()->setBool("global.bezel.resize_tattoo", bezel_resize_tattoo->getState());
					}
				});

				mWindow->pushGui(decorations_window);
			});			
#else
			addDecorationSetOptionListComponent(mWindow, s, sets);
#endif
		}
	}
	
	// latency reduction
	if (!hasGlobalFeature("runahead"))
		s->addEntry(_("LATENCY REDUCTION"), true, [this] { openLatencyReductionConfiguration(mWindow, "global"); });

	//AI-enabled translations
	if (!hasGlobalFeature("ai_service_enabled"))
	{
		s->addEntry(_("AI GAME TRANSLATION"), true, [this]
		{
			GuiSettings *ai_service = new GuiSettings(mWindow, _("AI GAME TRANSLATION").c_str());

			// AI service enabled?
			auto ai_service_enabled = std::make_shared<SwitchComponent>(mWindow);
			ai_service_enabled->setState(
				SystemConf::getInstance()->get("global.ai_service_enabled") == "1");
			ai_service->addWithLabel(_("ENABLE AI TRANSLATION SERVICE"), ai_service_enabled);

			// Target language - order is: popular languages in the Batocera community first
			// then alphabetical order of the 2-char lang code (because the strings are localized)
			auto lang_choices = std::make_shared<OptionListComponent<std::string> >(mWindow,
				_("TARGET LANGUAGE"), false);
			std::string currentLang = SystemConf::getInstance()->get("global.ai_target_lang");
			if (currentLang.empty())
				currentLang = std::string("En");
			lang_choices->add("ENGLISH", "En", currentLang == "En");
			lang_choices->add("FRANÇAIS", "Fr", currentLang == "Fr");
			lang_choices->add("PORTUGUÊS", "Pt", currentLang == "Pt");
			lang_choices->add("DEUTSCH", "De", currentLang == "De");
			lang_choices->add("GREEK", "El", currentLang == "El");
			lang_choices->add("ESPAÑOL", "Es", currentLang == "Es");
			lang_choices->add("CZECH", "Cs", currentLang == "Cs");
			lang_choices->add("DANISH", "Da", currentLang == "Da");
			lang_choices->add("CROATIAN", "Hr", currentLang == "Hr");
			lang_choices->add("HUNGARIAN", "Hu", currentLang == "Hu");
			lang_choices->add("ITALIANO", "It", currentLang == "It");
			lang_choices->add("JAPANESE", "Ja", currentLang == "Ja");
			lang_choices->add("KOREAN", "Ko", currentLang == "Ko");
			lang_choices->add("DUTCH", "Nl", currentLang == "Nl");
			lang_choices->add("NORWEGIAN", "Nn", currentLang == "Nn");
			lang_choices->add("POLISH", "Po", currentLang == "Po");
			lang_choices->add("ROMANIAN", "Ro", currentLang == "Ro");
			lang_choices->add("РУССКИЙ", "Ru", currentLang == "Ru");
			lang_choices->add("SVENSKA", "Sv", currentLang == "Sv");
			lang_choices->add("TÜRKÇE", "Tr", currentLang == "Tr");
			lang_choices->add("简体中文", "Zh", currentLang == "Zh");
			ai_service->addWithLabel(_("TARGET LANGUAGE"), lang_choices);

			// Service  URL
			ai_service->addInputTextRow(_("AI TRANSLATION SERVICE URL"), "global.ai_service_url", false);

			// Pause game for translation?
			auto ai_service_pause = std::make_shared<SwitchComponent>(mWindow);
			ai_service_pause->setState(
				SystemConf::getInstance()->get("global.ai_service_pause") == "1");
			ai_service->addWithLabel(_("PAUSE ON TRANSLATED SCREEN"), ai_service_pause);

			ai_service->addSaveFunc([ai_service_enabled, lang_choices, ai_service_pause] {
				if (ai_service_enabled->changed())
					SystemConf::getInstance()->set("global.ai_service_enabled",
						ai_service_enabled->getState() ? "1" : "0");
				if (lang_choices->changed())
					SystemConf::getInstance()->set("global.ai_target_lang",
						lang_choices->getSelected());
				if (ai_service_pause->changed())
					SystemConf::getInstance()->set("global.ai_service_pause",
						ai_service_pause->getState() ? "1" : "0");
				SystemConf::getInstance()->saveSystemConf();
			});

			mWindow->pushGui(ai_service);
		});
	}
	
	// Load global custom features
	addFeatures(CustomFeatures::GlobalFeatures, window, s, "global", "", "", "", _("DEFAULT GLOBAL SETTINGS"));
	
	if (!hasGlobalFeature("disableautocontrollers") && SystemData::sSystemVector.any([](auto sys) { return !sys->getCompatibleCoreNames(EmulatorFeatures::autocontrollers).empty(); }))
	{
		auto autoControllers = std::make_shared<SwitchComponent>(mWindow);
		autoControllers->setState(SystemConf::getInstance()->get("global.disableautocontrollers") != "1");
		s->addWithLabel(_("AUTOCONFIGURE CONTROLLERS"), autoControllers);
		s->addSaveFunc([autoControllers] { SystemConf::getInstance()->set("global.disableautocontrollers", autoControllers->getState() ? "" : "1"); });
	}

	s->addGroup(_("SYSTEM SETTINGS"));

	// Custom config for systems
	s->addEntry(_("PER SYSTEM ADVANCED CONFIGURATION"), true, [this, s, window]
		{
			s->save();
			GuiSettings* configuration = new GuiSettings(window, _("PER SYSTEM ADVANCED CONFIGURATION").c_str());

			// For each activated system
			std::vector<SystemData*> systems = SystemData::sSystemVector;
			for (auto system : systems)
			{
				if (system->isCollection() || !system->isGameSystem())
					continue;

				if (system->hasPlatformId(PlatformIds::PLATFORM_IGNORE))
					continue;

				if (!system->hasFeatures() && !system->hasEmulatorSelection())
					continue;

				configuration->addEntry(system->getFullName(), true, [this, system, window] {
					popSystemConfigurationGui(window, system);
					});
			}

			window->pushGui(configuration);
		});

	mWindow->pushGui(s);
}

void GuiMenu::openMissingBiosSettings()
{
	GuiBios::show(mWindow);
}

void GuiMenu::updateGameLists(Window* window, bool confirm)
{
	if (ThreadedScraper::isRunning())
	{
		window->pushGui(new GuiMsgBox(window, _("SCRAPER IS RUNNING. DO YOU WANT TO STOP IT?"),
			_("YES"), [] { ThreadedScraper::stop(); }, 
			_("NO"), nullptr));

		return;
	}

	if (ThreadedHasher::isRunning())
	{
		window->pushGui(new GuiMsgBox(window, _("GAME HASHING IS RUNNING. DO YOU WANT TO STOP IT?"),
			_("YES"), [] { ThreadedHasher::stop(); },
			_("NO"), nullptr));

		return;
	}
	
	if (!confirm)
	{
		ViewController::reloadAllGames(window, true, true);
		return;
	}

	window->pushGui(new GuiMsgBox(window, _("REALLY UPDATE GAMELISTS?"), _("YES"), [window]
		{
		ViewController::reloadAllGames(window, true, true);
		}, 
		_("NO"), nullptr));
}

void GuiMenu::openSystemEmulatorSettings(SystemData* system)
{
	auto theme = ThemeData::getMenuTheme();

	GuiSettings* s = new GuiSettings(mWindow, system->getFullName().c_str());

	auto emul_choice = std::make_shared<OptionListComponent<std::string>>(mWindow, _("Emulator"), false);
	auto core_choice = std::make_shared<OptionListComponent<std::string>>(mWindow, _("Core"), false);

	std::string currentEmul = system->getEmulator(false);
	std::string defaultEmul = system->getDefaultEmulator();

	emul_choice->add(_("AUTO"), "", false);

	bool found = false;
	for (auto emul : system->getEmulators())
	{
		if (emul.name == currentEmul)
			found = true;

		emul_choice->add(emul.name, emul.name, emul.name == currentEmul);
	}

	if (!found)
		emul_choice->selectFirstItem();

	ComponentListRow row;
	row.addElement(std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(_("Emulator")), theme->Text.font, theme->Text.color), true);
	row.addElement(emul_choice, false);

	s->addRow(row);

	emul_choice->setSelectedChangedCallback([this, system, core_choice](std::string emulatorName)
	{
		std::string currentCore = system->getCore(false);
		std::string defaultCore = system->getDefaultCore(emulatorName);

		core_choice->clear();	
		core_choice->add(_("AUTO"), "", false);

		bool found = false;

		for (auto& emulator : system->getEmulators())
		{
			if (emulatorName != emulator.name)
				continue;
			
			for (auto core : emulator.cores)
			{
				core_choice->add(core.name, core.name, currentCore == core.name);
				if (currentCore == core.name)
					found = true;
			}			
		}
	
		if (!found)
			core_choice->selectFirstItem();
		else
			core_choice->invalidate();
	});

	row.elements.clear();
	row.addElement(std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(_("Core")), theme->Text.font, theme->Text.color), true);
	row.addElement(core_choice, false);
	s->addRow(row);

	// force change event to load core list
	emul_choice->invalidate();


	s->addSaveFunc([system, emul_choice, core_choice]
	{
		Settings::getInstance()->setString(system->getName() + ".emulator", emul_choice->getSelected());
		Settings::getInstance()->setString(system->getName() + ".core", core_choice->getSelected());
	});

	mWindow->pushGui(s);
}

void GuiMenu::openEmulatorSettings()
{
	GuiSettings* configuration = new GuiSettings(mWindow, _("EMULATOR SETTINGS").c_str());

	Window* window = mWindow;

	// For each activated system
	for (auto system : SystemData::sSystemVector)
	{
		if (system->isCollection())
			continue;

		if (system->getEmulators().size() == 0)
			continue;

		if (system->getEmulators().size() == 1 && system->getEmulators().cbegin()->cores.size() <= 1)
			continue;

		configuration->addEntry(system->getFullName(), true, [this, system] { openSystemEmulatorSettings(system); });
	}

	window->pushGui(configuration);
}

struct ThemeConfigOption
{
	std::string defaultSettingName;
	std::string subset;
	std::shared_ptr<OptionListComponent<std::string>> component;
};

void GuiMenu::openThemeConfiguration(Window* mWindow, GuiComponent* s, std::shared_ptr<OptionListComponent<std::string>> theme_set, const std::string systemTheme)
{
	if (theme_set != nullptr && Settings::getInstance()->getString("ThemeSet") != theme_set->getSelected())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("YOU MUST APPLY THE THEME BEFORE EDITING ITS CONFIGURATION"), _("OK")));
		return;
	}

	Window* window = mWindow;

	auto system = ViewController::get()->getState().getSystem();
	auto theme = system->getTheme();

	auto themeconfig = new GuiSettings(mWindow, (systemTheme.empty() ? _("THEME CONFIGURATION") : _("VIEW CUSTOMIZATION")).c_str());

	auto themeSubSets = theme->getSubSets();

	std::string viewName;
	bool showGridFeatures = true;
	if (!systemTheme.empty())
	{
		auto glv = ViewController::get()->getGameListView(system);
		viewName = glv->getName();
		std::string baseType = theme->getCustomViewBaseType(viewName);

		showGridFeatures = (viewName == "grid" || baseType == "grid");
	}

	// gamelist_style
	std::shared_ptr<OptionListComponent<std::string>> gamelist_style = nullptr;

	if (systemTheme.empty() || showGridFeatures && system != NULL && theme->hasView("grid"))
		themeconfig->addGroup(_("GAMELIST STYLE"));

	if (systemTheme.empty())
	{
		gamelist_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("GAMELIST VIEW STYLE"), false);

		std::vector<std::pair<std::string, std::string>> styles;
		styles.push_back(std::pair<std::string, std::string>("automatic", _("automatic")));

		bool showViewStyle = true;

		if (system != NULL)
		{
			auto mViews = theme->getViewsOfTheme();

			showViewStyle = mViews.size() > 1;

			for (auto it = mViews.cbegin(); it != mViews.cend(); ++it)
			{
				if (it->first == "basic" || it->first == "detailed" || it->first == "grid" || it->first == "video" || it->first == "gamecarousel")
					styles.push_back(std::pair<std::string, std::string>(it->first, _(it->first.c_str())));
				else
					styles.push_back(*it);
			}
		}
		else
		{
			styles.push_back(std::pair<std::string, std::string>("basic", _("basic")));
			styles.push_back(std::pair<std::string, std::string>("detailed", _("detailed")));
		}

		auto viewPreference = systemTheme.empty() ? Settings::getInstance()->getString("GamelistViewStyle") : system->getSystemViewMode();
		if (!theme->hasView(viewPreference))
			viewPreference = "automatic";

		for (auto it = styles.cbegin(); it != styles.cend(); it++)
			gamelist_style->add(it->second, it->first, viewPreference == it->first);

		if (!gamelist_style->hasSelection())
			gamelist_style->selectFirstItem();

		if (showViewStyle)
			themeconfig->addWithLabel(_("GAMELIST VIEW STYLE"), gamelist_style);
	}

	// Default grid size
	std::shared_ptr<OptionListComponent<std::string>> mGridSize = nullptr;
	if (showGridFeatures && system != NULL && theme->hasView("grid"))
	{
		Vector2f gridOverride =
			systemTheme.empty() ? Vector2f::parseString(Settings::getInstance()->getString("DefaultGridSize")) :
			system->getGridSizeOverride();

		auto ovv = std::to_string((int)gridOverride.x()) + "x" + std::to_string((int)gridOverride.y());

		mGridSize = std::make_shared<OptionListComponent<std::string>>(mWindow, _("DEFAULT GRID SIZE"), false);

		bool found = false;
		for (auto it = GuiGamelistOptions::gridSizes.cbegin(); it != GuiGamelistOptions::gridSizes.cend(); it++)
		{
			bool sel = (gridOverride == Vector2f(0, 0) && *it == "automatic") || ovv == *it;
			if (sel)
				found = true;

			mGridSize->add(_(it->c_str()), *it, sel);
		}

		if (!found)
			mGridSize->selectFirstItem();

		themeconfig->addWithLabel(_("DEFAULT GRID SIZE"), mGridSize);
	}



	std::map<std::string, ThemeConfigOption> options;

	auto subsetNames = theme->getSubSetNames(viewName);
	
	// push appliesTo at end of list
	std::stable_sort(subsetNames.begin(), subsetNames.end(), [themeSubSets](const std::string& a, const std::string& b) -> bool
	{ 
		auto sa = ThemeData::getSubSet(themeSubSets, a);
		auto sb = ThemeData::getSubSet(themeSubSets, b);

		bool aHasApplies = sa.size() > 0 && !sa.cbegin()->appliesTo.empty();
		bool bHasApplies = sb.size() > 0 && !sb.cbegin()->appliesTo.empty();

		return aHasApplies < bHasApplies;
	});

	bool hasThemeOptionGroup = false;
	bool hasApplyToGroup = false;
	for (std::string subset : subsetNames) // theme->getSubSetNames(viewName)
	{
		std::string settingName = "subset." + subset;
		std::string perSystemSettingName = systemTheme.empty() ? "" : "subset." + systemTheme + "." + subset;

		if (subset == "colorset") settingName = "ThemeColorSet";
		else if (subset == "iconset") settingName = "ThemeIconSet";
		else if (subset == "menu") settingName = "ThemeMenu";
		else if (subset == "systemview") settingName = "ThemeSystemView";
		else if (subset == "gamelistview") settingName = "ThemeGamelistView";
		else if (subset == "region") settingName = "ThemeRegionName";

		auto themeColorSets = ThemeData::getSubSet(themeSubSets, subset);

		if (themeColorSets.size() > 0)
		{
			auto selectedColorSet = themeColorSets.end();
			auto selectedName = !perSystemSettingName.empty() ? Settings::getInstance()->getString(perSystemSettingName) : Settings::getInstance()->getString(settingName);

			if (!perSystemSettingName.empty() && selectedName.empty())
				selectedName = Settings::getInstance()->getString(settingName);

			for (auto it = themeColorSets.begin(); it != themeColorSets.end() && selectedColorSet == themeColorSets.end(); it++)
				if (it->name == selectedName)
					selectedColorSet = it;

			std::string displayName;
			if (!themeColorSets.empty())
				displayName = themeColorSets.cbegin()->subSetDisplayName;

			std::shared_ptr<OptionListComponent<std::string>> item = std::make_shared<OptionListComponent<std::string> >(mWindow, displayName.empty() ? _(("THEME " + Utils::String::toUpper(subset)).c_str()) : displayName, false);
			item->setTag(!perSystemSettingName.empty() ? perSystemSettingName : settingName);

			std::string defaultName;
			for (auto it = themeColorSets.begin(); it != themeColorSets.end(); it++)
			{
				std::string displayName = it->displayName;

				if (!systemTheme.empty())
				{
					std::string defaultValue = Settings::getInstance()->getString(settingName);
					if (defaultValue.empty())
						defaultValue = system->getTheme()->getDefaultSubSetValue(subset);

					if (it->name == defaultValue)
					{
						defaultName = Utils::String::toUpper(displayName);
						// displayName = displayName + " (" + _("DEFAULT") + ")";
					}
				}

				item->add(displayName, it->name, it == selectedColorSet);
			}

			if (selectedColorSet == themeColorSets.end())
				item->selectFirstItem();

			if (!themeColorSets.empty())
			{				
				if (!displayName.empty())
				{
					bool hasApplyToSubset = themeColorSets.cbegin()->appliesTo.size() > 0;

					std::string prefix;

					if (systemTheme.empty())
					{
						for (auto subsetName : themeColorSets.cbegin()->appliesTo)
						{
							std::string pfx = theme->getViewDisplayName(subsetName);
							if (!pfx.empty())
							{
								if (prefix.empty())
									prefix = pfx;
								else
									prefix = prefix + ", " + pfx;
							}
						}

						prefix = Utils::String::toUpper(prefix);
					}

					if (hasApplyToSubset && !hasApplyToGroup)
					{
						hasApplyToGroup = true;
						themeconfig->addGroup(_("GAMELIST THEME OPTIONS"));
					}
					else if (!hasApplyToSubset && !hasThemeOptionGroup)
					{
						hasThemeOptionGroup = true;
						themeconfig->addGroup(_("THEME OPTIONS"));
					}

					if (displayName == "-" && item->size() <= 1)
					{
						ComponentListRow row;
						row.selectable = false;

						auto font = ThemeData::getMenuTheme()->TextSmall.font;
						auto text = std::make_shared<TextComponent>(mWindow, "", font, 0); 						
						text->setLineSpacing(1.0f);
						row.addElement(text, true);						

						themeconfig->addRow(row);
					}
					else if (!prefix.empty())
						themeconfig->addWithDescription(displayName, prefix, item);
					else if (!defaultName.empty())
						themeconfig->addWithDescription(displayName, _("DEFAULT VALUE") + " : " + defaultName, item);
					else 
						themeconfig->addWithLabel(displayName + prefix, item);
				}
				else
				{
					if (!hasThemeOptionGroup)
					{
						hasThemeOptionGroup = true;
						themeconfig->addGroup(_("THEME OPTIONS"));
					}

					themeconfig->addWithLabel(_(("THEME " + Utils::String::toUpper(subset)).c_str()), item);
				}
			}

			ThemeConfigOption opt;
			opt.component = item;
			opt.subset = subset;
			opt.defaultSettingName = settingName;
			options[!perSystemSettingName.empty() ? perSystemSettingName : settingName] = opt;
		}
		else
		{
			ThemeConfigOption opt;
			opt.component = nullptr;
			options[!perSystemSettingName.empty() ? perSystemSettingName : settingName] = opt;
		}
	}
	

	if (!systemTheme.empty())
	{
		themeconfig->addGroup(_("GAMELIST OPTIONS"));

		// Show favorites first in gamelists
		auto fav = Settings::getInstance()->getString(system->getName() + ".FavoritesFirst");
		auto favoritesFirst = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW FAVORITES ON TOP"), false);
		std::string defFav = Settings::getInstance()->getBool("FavoritesFirst") ? _("YES") : _("NO");
		favoritesFirst->add(_("AUTO"), "", fav == "" || fav == "auto");
		favoritesFirst->add(_("YES"), "1", fav == "1");
		favoritesFirst->add(_("NO"), "0", fav == "0");
		themeconfig->addWithDescription(_("SHOW FAVORITES ON TOP"), _("DEFAULT VALUE") + " : " + defFav, favoritesFirst);
		themeconfig->addSaveFunc([themeconfig, favoritesFirst, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".FavoritesFirst", favoritesFirst->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});

		// Show favorites first in gamelists
		auto defHid = Settings::ShowHiddenFiles() ? _("YES") : _("NO");
		auto curhid = Settings::getInstance()->getString(system->getName() + ".ShowHiddenFiles");
		auto hiddenFiles = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW HIDDEN FILES"), false);
		hiddenFiles->add(_("AUTO"), "", curhid == "" || curhid == "auto");
		hiddenFiles->add(_("YES"), "1", curhid == "1");
		hiddenFiles->add(_("NO"), "0", curhid == "0");
		themeconfig->addWithDescription(_("SHOW HIDDEN FILES"), _("DEFAULT VALUE") + " : " + defHid, hiddenFiles);
		themeconfig->addSaveFunc([themeconfig, hiddenFiles, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".ShowHiddenFiles", hiddenFiles->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});

		// Folder View Mode
		auto folderView = Settings::getInstance()->getString("FolderViewMode");
		auto defFol = folderView.empty() ? "" : Utils::String::toUpper(_(folderView.c_str()));
		auto curFol = Settings::getInstance()->getString(system->getName() + ".FolderViewMode");

		auto foldersBehavior = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW FOLDERS"), false);
		foldersBehavior->add(_("AUTO"), "", curFol == "" || curFol == "auto"); //  + " (" + defFol + ")"
		foldersBehavior->add(_("always"), "always", curFol == "always");
		foldersBehavior->add(_("never"), "never", curFol == "never");
		foldersBehavior->add(_("having multiple games"), "having multiple games", curFol == "having multiple games");

		themeconfig->addWithDescription(_("SHOW FOLDERS"), _("DEFAULT VALUE") + " : " + defFol, foldersBehavior);
		themeconfig->addSaveFunc([themeconfig, foldersBehavior, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".FolderViewMode", foldersBehavior->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});
		
		// Show parent folder in gamelists
		auto defPf = Settings::getInstance()->getBool("ShowParentFolder") ? _("YES") : _("NO");
		auto curPf = Settings::getInstance()->getString(system->getName() + ".ShowParentFolder");
		auto parentFolder = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW '..' PARENT FOLDER"), false);
		parentFolder->add(_("AUTO"), "", curPf == "" || curPf == "auto");
		parentFolder->add(_("YES"), "1", curPf == "1");
		parentFolder->add(_("NO"), "0", curPf == "0");
		themeconfig->addWithDescription(_("SHOW '..' PARENT FOLDER"), _("DEFAULT VALUE") + " : " + defPf, parentFolder);
		themeconfig->addSaveFunc([themeconfig, parentFolder, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".ShowParentFolder", parentFolder->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});

		// Show flags

		auto defSF = Settings::getInstance()->getString("ShowFlags");
		if (defSF == "1")
			defSF = _("BEFORE NAME");
		else if (defSF == "2")
			defSF = _("AFTER NAME");
		else 
			defSF = _("NO");
		
		auto curSF = Settings::getInstance()->getString(system->getName() + ".ShowFlags");
		auto showRegionFlags = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW REGION FLAG"), false);

		showRegionFlags->addRange({ 
			{ _("AUTO"), "auto" },
			{ _("NO"), "0" },
			{ _("BEFORE NAME") , "1" },
			{ _("AFTER NAME"), "2" } }, 
			curSF);

		themeconfig->addWithDescription(_("SHOW REGION FLAG"), _("DEFAULT VALUE") + " : " + defSF, showRegionFlags);
		themeconfig->addSaveFunc([themeconfig, showRegionFlags, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".ShowFlags", showRegionFlags->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});
		
		// Show SaveStates
		auto defSS = Settings::getInstance()->getBool("ShowSaveStates") ? _("YES") : _("NO");
		auto curSS = Settings::getInstance()->getString(system->getName() + ".ShowSaveStates");
		auto showSaveStates = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW SAVESTATE ICON"), false);
		showSaveStates->add(_("AUTO"), "", curSS == "" || curSS == "auto");
		showSaveStates->add(_("YES"), "1", curSS == "1");
		showSaveStates->add(_("NO"), "0", curSS == "0");
		themeconfig->addWithDescription(_("SHOW SAVESTATE ICON"), _("DEFAULT VALUE") + " : " + defSS, showSaveStates);
		themeconfig->addSaveFunc([themeconfig, showSaveStates, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".ShowSaveStates", showSaveStates->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});

		// Show Manual
		auto defMM = Settings::getInstance()->getBool("ShowManualIcon") ? _("YES") : _("NO");
		auto curMM = Settings::getInstance()->getString(system->getName() + ".ShowManualIcon");
		auto showManual = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW MANUAL ICON"), false);
		showManual->add(_("AUTO"), "", curMM == "" || curMM == "auto");
		showManual->add(_("YES"), "1", curMM == "1");
		showManual->add(_("NO"), "0", curMM == "0");
		themeconfig->addWithDescription(_("SHOW MANUAL ICON"), _("DEFAULT VALUE") + " : " + defMM, showManual);
		themeconfig->addSaveFunc([themeconfig, showManual, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".ShowManualIcon", showManual->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});

		// Show Cheevos
		auto defCI = Settings::getInstance()->getBool("ShowCheevosIcon") ? _("YES") : _("NO");
		auto curCI = Settings::getInstance()->getString(system->getName() + ".ShowCheevosIcon");
		auto showCheevos = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW RETROACHIEVEMENTS ICON"), false);
		showCheevos->add(_("AUTO"), "", curCI == "" || curCI == "auto");
		showCheevos->add(_("YES"), "1", curCI == "1");
		showCheevos->add(_("NO"), "0", curCI == "0");
		themeconfig->addWithDescription(_("SHOW RETROACHIEVEMENTS ICON"), _("DEFAULT VALUE") + " : " + defCI, showCheevos);
		themeconfig->addSaveFunc([themeconfig, showCheevos, system]
			{
				if (Settings::getInstance()->setString(system->getName() + ".ShowCheevosIcon", showCheevos->getSelected()))
					themeconfig->setVariable("reloadAll", true);
			});

		// Show gun icons
		auto defGI = Settings::getInstance()->getBool("ShowGunIconOnGames") ? _("YES") : _("NO");
		auto curGI = Settings::getInstance()->getString(system->getName() + ".ShowGunIconOnGames");
		auto showGun = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW GUN ICON"), false);
		showGun->add(_("AUTO"), "", curGI == "" || curGI == "auto");
		showGun->add(_("YES"), "1", curGI == "1");
		showGun->add(_("NO"), "0", curGI == "0");
		themeconfig->addWithDescription(_("SHOW GUN ICON"), _("DEFAULT VALUE") + " : " + defGI, showGun);
		themeconfig->addSaveFunc([themeconfig, showGun, system]
			{
				if (Settings::getInstance()->setString(system->getName() + ".ShowGunIconOnGames", showGun->getSelected()))
					themeconfig->setVariable("reloadAll", true);
			});

		// Show wheel icons
		auto defWI = Settings::getInstance()->getBool("ShowWheelIconOnGames") ? _("YES") : _("NO");
		auto curWI = Settings::getInstance()->getString(system->getName() + ".ShowWheelIconOnGames");
		auto showWheel = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW WHEEL ICON"), false);
		showWheel->add(_("AUTO"), "", curWI == "" || curWI == "auto");
		showWheel->add(_("YES"), "1", curWI == "1");
		showWheel->add(_("NO"), "0", curWI == "0");
		themeconfig->addWithDescription(_("SHOW WHEEL ICON"), _("DEFAULT VALUE") + " : " + defWI, showWheel);
		themeconfig->addSaveFunc([themeconfig, showWheel, system]
			{
				if (Settings::getInstance()->setString(system->getName() + ".ShowWheelIconOnGames", showWheel->getSelected()))
					themeconfig->setVariable("reloadAll", true);
			});

		// Show filenames
		auto defFn = Settings::getInstance()->getBool("ShowFilenames") ? _("YES") : _("NO");
		auto curFn = Settings::getInstance()->getString(system->getName() + ".ShowFilenames");

		auto showFilenames = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW FILENAMES INSTEAD"), false);
		showFilenames->add(_("AUTO"), "", curFn == "");
		showFilenames->add(_("YES"), "1", curFn == "1");
		showFilenames->add(_("NO"), "0", curFn == "0");
		themeconfig->addWithDescription(_("SHOW FILENAMES INSTEAD"), _("DEFAULT VALUE") + " : " + defFn, showFilenames);
		themeconfig->addSaveFunc([themeconfig, showFilenames, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".ShowFilenames", showFilenames->getSelected()))
			{
				SystemData::resetSettings();
				FileData::resetSettings();

		//		themeconfig->setVariable("reloadCollections", true);
				themeconfig->setVariable("reloadAll", true);				
			}
		});
		

		// File extensions
		if (!system->isCollection() && system->isGameSystem())
		{
			auto hiddenExts = Utils::String::split(Settings::getInstance()->getString(system->getName() + ".HiddenExt"), ';');

			auto hiddenCtrl = std::make_shared<OptionListComponent<std::string>>(mWindow, _("FILE EXTENSIONS"), true);

			for (auto ext : system->getExtensions())
			{
				std::string extid = Utils::String::toLower(Utils::String::replace(ext, ".", ""));
				hiddenCtrl->add(ext, extid, std::find(hiddenExts.cbegin(), hiddenExts.cend(), extid) == hiddenExts.cend());
			}

			themeconfig->addWithLabel(_("FILE EXTENSIONS"), hiddenCtrl);
			themeconfig->addSaveFunc([themeconfig, system, hiddenCtrl]
			{
				std::string hiddenSystems;

				std::vector<std::string> sel = hiddenCtrl->getSelectedObjects();

				for (auto ext : system->getExtensions())
				{
					std::string extid = Utils::String::toLower(Utils::String::replace(ext, ".", ""));
					if (std::find(sel.cbegin(), sel.cend(), extid) == sel.cend())
					{
						if (hiddenSystems.empty())
							hiddenSystems = extid;
						else
							hiddenSystems = hiddenSystems + ";" + extid;
					}
				}

				if (Settings::getInstance()->setString(system->getName() + ".HiddenExt", hiddenSystems))
				{
					Settings::getInstance()->saveFile();

					themeconfig->setVariable("reloadAll", true);
					themeconfig->setVariable("forceReloadGames", true);
				}
			});
		}
	}

	if (systemTheme.empty())
	{
		themeconfig->addGroup(_("TOOLS"));

		themeconfig->addEntry(_("RESET CUSTOMIZATIONS"), false, [s, themeconfig, window]
		{
			themeconfig->setVariable("resetTheme", true);
			themeconfig->setVariable("reloadAll", true);
			themeconfig->close();
		});
	}

	//  theme_colorset, theme_iconset, theme_menu, theme_systemview, theme_gamelistview, theme_region,
	themeconfig->addSaveFunc([systemTheme, system, themeconfig, options, gamelist_style, mGridSize, window]
	{
		bool reloadAll = false;

		for (auto option : options)
		{
			ThemeConfigOption& opt = option.second;

			std::string value;

			if (opt.component != nullptr)
			{
				value = opt.component->getSelected();

				if (!systemTheme.empty() && !value.empty())
				{
					std::string defaultValue = Settings::getInstance()->getString(opt.defaultSettingName);
					if (defaultValue.empty())
						defaultValue = system->getTheme()->getDefaultSubSetValue(opt.subset);

					if (value == defaultValue)
						value = "";
				}
				else if (systemTheme.empty() && value == system->getTheme()->getDefaultSubSetValue(opt.subset))
					value = "";
			}

			if (value != Settings::getInstance()->getString(option.first))
				reloadAll |= Settings::getInstance()->setString(option.first, value);
		}

		Vector2f gridSizeOverride(0, 0);

		if (mGridSize != nullptr)
		{
			std::string str = mGridSize->getSelected();
			std::string value = "";

			size_t divider = str.find('x');
			if (divider != std::string::npos)
			{
				std::string first = str.substr(0, divider);
				std::string second = str.substr(divider + 1, std::string::npos);

				gridSizeOverride = Vector2f((float)atof(first.c_str()), (float)atof(second.c_str()));
				value = Utils::String::replace(Utils::String::replace(gridSizeOverride.toString(), ".000000", ""), "0 0", "");
			}

			if (systemTheme.empty())
				reloadAll |= Settings::getInstance()->setString("DefaultGridSize", value);
		}
		else if (systemTheme.empty())
			reloadAll |= Settings::getInstance()->setString("DefaultGridSize", "");

		if (systemTheme.empty())
			reloadAll |= Settings::getInstance()->setString("GamelistViewStyle", gamelist_style == nullptr ? "" : gamelist_style->getSelected());
		else
		{
			std::string viewMode = gamelist_style == nullptr ? system->getSystemViewMode() : gamelist_style->getSelected();
			reloadAll |= system->setSystemViewMode(viewMode, gridSizeOverride);
		}

		if (themeconfig->getVariable("resetTheme"))
		{
			Settings::getInstance()->setString("GamelistViewStyle", "");
			Settings::getInstance()->setString("DefaultGridSize", "");
			Settings::getInstance()->setString("ThemeRegionName", "");
			Settings::getInstance()->setString("ThemeColorSet", "");
			Settings::getInstance()->setString("ThemeIconSet", "");
			Settings::getInstance()->setString("ThemeMenu", "");
			Settings::getInstance()->setString("ThemeSystemView", "");
			Settings::getInstance()->setString("ThemeGamelistView", "");
			Settings::getInstance()->setString("GamelistViewStyle", "");
			Settings::getInstance()->setString("DefaultGridSize", "");

			for (auto sm : Settings::getInstance()->getStringMap())
				if (Utils::String::startsWith(sm.first, "subset."))
					Settings::getInstance()->setString(sm.first, "");

			for (auto system : SystemData::sSystemVector)
			{
				system->setSystemViewMode("automatic", Vector2f(0, 0));

				Settings::getInstance()->setString(system->getName() + ".FavoritesFirst", "");
				Settings::getInstance()->setString(system->getName() + ".ShowHiddenFiles", "");
				Settings::getInstance()->setString(system->getName() + ".FolderViewMode", "");
				Settings::getInstance()->setString(system->getName() + ".ShowFilenames", "");
				Settings::getInstance()->setString(system->getName() + ".ShowParentFolder", "");
			}

			Settings::getInstance()->saveFile();
			std::string path = Paths::getUserEmulationStationPath() + "/themesettings/" + Settings::getInstance()->getString("ThemeSet") + ".cfg";
			if (Utils::FileSystem::exists(path))
				Utils::FileSystem::removeFile(path);
		}

		if (reloadAll || themeconfig->getVariable("reloadAll"))
		{
			if (themeconfig->getVariable("forceReloadGames"))
			{
				ViewController::reloadAllGames(window, false);
			}
			else if (systemTheme.empty())
			{				
				CollectionSystemManager::get()->updateSystemsList();
				ViewController::get()->reloadAll(window);
				window->closeSplashScreen();
			}
			else
			{
				system->loadTheme();
				system->resetFilters();

				ViewController::get()->reloadSystemListViewTheme(system);
				ViewController::get()->reloadGameListView(system);
			}
		}
	});

	mWindow->pushGui(themeconfig);
}

void GuiMenu::openUISettings() 
{
	auto pthis = this;
	Window* window = mWindow;

	auto s = new GuiSettings(mWindow, _("USER INTERFACE SETTINGS").c_str());

	// theme set
	auto theme = ThemeData::getMenuTheme();
	auto themeSets = ThemeData::getThemeSets();
	auto system = ViewController::get()->getState().getSystem();

	s->addGroup(_("APPEARANCE"));

	if (system != nullptr && !themeSets.empty())
	{		
		auto selectedSet = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
		if (selectedSet == themeSets.end())
			selectedSet = themeSets.begin();

		auto theme_set = std::make_shared<OptionListComponent<std::string> >(mWindow, _("THEME SET"), false);

		std::vector<std::string> themeList;
		for (auto it = themeSets.begin(); it != themeSets.end(); it++)
			themeList.push_back(it->first);

		std::sort(themeList.begin(), themeList.end(), [](const std::string& a, const std::string& b) -> bool { return Utils::String::toLower(a).compare(Utils::String::toLower(b)) < 0; });

		for (auto themeName : themeList)
			theme_set->add(themeName, themeName, themeName == selectedSet->first);

		//for (auto it = themeSets.begin(); it != themeSets.end(); it++)
		//	theme_set->add(it->first, it->first, it == selectedSet);

		s->addWithLabel(_("THEME SET"), theme_set);
		s->addSaveFunc([s, theme_set, pthis, window, system]
		{
			std::string oldTheme = Settings::getInstance()->getString("ThemeSet");
			if (oldTheme != theme_set->getSelected())
			{			
				saveSubsetSettings();

				Settings::getInstance()->setString("ThemeSet", theme_set->getSelected());

				// theme changed without setting options, forcing options to avoid crash/blank theme
				Settings::getInstance()->setString("ThemeRegionName", "");
				Settings::getInstance()->setString("ThemeColorSet", "");
				Settings::getInstance()->setString("ThemeIconSet", "");
				Settings::getInstance()->setString("ThemeMenu", "");
				Settings::getInstance()->setString("ThemeSystemView", "");
				Settings::getInstance()->setString("ThemeGamelistView", "");
				Settings::getInstance()->setString("GamelistViewStyle", "");
				Settings::getInstance()->setString("DefaultGridSize", "");

				for(auto sm : Settings::getInstance()->getStringMap())
					if (Utils::String::startsWith(sm.first, "subset."))
						Settings::getInstance()->setString(sm.first, "");

				for (auto sysIt = SystemData::sSystemVector.cbegin(); sysIt != SystemData::sSystemVector.cend(); sysIt++)
					(*sysIt)->setSystemViewMode("automatic", Vector2f(0,0));

				loadSubsetSettings(theme_set->getSelected());

				s->setVariable("reloadCollections", true);
				s->setVariable("reloadAll", true);
				s->setVariable("reloadGuiMenu", true);

				Scripting::fireEvent("theme-changed", theme_set->getSelected(), oldTheme);				
			}
		});

		bool showThemeConfiguration = system->getTheme()->hasSubsets() || system->getTheme()->hasView("grid");
		if (showThemeConfiguration)
		{
			s->addSubMenu(_("THEME CONFIGURATION"), [this, s, theme_set]() { openThemeConfiguration(mWindow, s, theme_set); });
		}
		else // GameList view style only, acts like Retropie for simple themes
		{
			auto gamelist_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("GAMELIST VIEW STYLE"), false);
			std::vector<std::pair<std::string, std::string>> styles;
			styles.push_back(std::pair<std::string, std::string>("automatic", _("automatic")));

			auto system = ViewController::get()->getState().getSystem();
			if (system != NULL)
			{
				auto mViews = system->getTheme()->getViewsOfTheme();
				for (auto it = mViews.cbegin(); it != mViews.cend(); ++it)
					styles.push_back(*it);
			}
			else
			{
				styles.push_back(std::pair<std::string, std::string>("basic", _("basic")));
				styles.push_back(std::pair<std::string, std::string>("detailed", _("detailed")));
				styles.push_back(std::pair<std::string, std::string>("video", _("video")));
				styles.push_back(std::pair<std::string, std::string>("grid", _("grid")));				
			}

			auto viewPreference = Settings::getInstance()->getString("GamelistViewStyle");
			if (!system->getTheme()->hasView(viewPreference))
				viewPreference = "automatic";

			for (auto it = styles.cbegin(); it != styles.cend(); it++)
				gamelist_style->add(it->second, it->first, viewPreference == it->first);

			s->addWithLabel(_("GAMELIST VIEW STYLE"), gamelist_style);
			s->addSaveFunc([s, gamelist_style, window] {
				if (Settings::getInstance()->setString("GamelistViewStyle", gamelist_style->getSelected()))
				{
					s->setVariable("reloadAll", true);
					s->setVariable("reloadGuiMenu", true);
				}
			});
		}		
	}

	s->addGroup(_("DISPLAY OPTIONS"));
	s->addEntry(_("SCREENSAVER SETTINGS"), true, std::bind(&GuiMenu::openScreensaverOptions, this));
	s->addOptionList(_("LIST TRANSITION"), { { _("auto"), "auto" },{ _("fade") , "fade" },{ _("slide"), "slide" },{ _("fade & slide"), "fade & slide" },{ _("instant"), "instant" } }, "TransitionStyle", true);
	s->addOptionList(_("GAME LAUNCH TRANSITION"), { { _("auto"), "auto" },{ _("fade") , "fade" },{ _("slide"), "slide" },{ _("instant"), "instant" } }, "GameTransitionStyle", true);

	s->addSwitch(_("GAME MEDIAS DURING FAST SCROLLING"), "ScrollLoadMedias", false); 

	s->addSwitch(_("SHOW CLOCK"), "DrawClock", true);
	s->addSwitch(_("ON-SCREEN HELP"), "ShowHelpPrompts", true, [s] { s->setVariable("reloadAll", true); });

	if (Utils::Platform::queryBatteryInformation().hasBattery)
		s->addOptionList(_("SHOW BATTERY STATUS"), { { _("NO"), "" },{ _("ICON"), "icon" },{ _("ICON AND TEXT"), "text" } }, "ShowBattery", true);

	s->addGroup(_("GAMELIST OPTIONS"));
	s->addSwitch(_("SHOW FAVORITES ON TOP"), "FavoritesFirst", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW HIDDEN FILES"), "ShowHiddenFiles", true, [s] { s->setVariable("reloadAll", true); });
	s->addOptionList(_("SHOW FOLDERS"), { { _("always"), "always" },{ _("never") , "never" },{ _("having multiple games"), "having multiple games" } }, "FolderViewMode", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW FOLDERS FIRST"), "ShowFoldersFirst", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW '..' PARENT FOLDER"), "ShowParentFolder", true, [s] { s->setVariable("reloadAll", true); });
	s->addOptionList(_("SHOW REGION FLAG"), { { _("NO"), "auto" },{ _("BEFORE NAME") , "1" },{ _("AFTER NAME"), "2" } }, "ShowFlags", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW SAVESTATE ICON"), "ShowSaveStates", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW MANUAL ICON"), "ShowManualIcon", true, [s] { s->setVariable("reloadAll", true); });	
	s->addSwitch(_("SHOW RETROACHIEVEMENTS ICON"), "ShowCheevosIcon", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW GUN ICON"), "ShowGunIconOnGames", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW WHEEL ICON"), "ShowWheelIconOnGames", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW FILENAMES INSTEAD"), "ShowFilenames", true, [s] 
		{
			SystemData::resetSettings();
			FileData::resetSettings();

			s->setVariable("reloadCollections", true);
			s->setVariable("reloadAll", true); 
		});
	s->addSwitch(_("IGNORE LEADING ARTICLES WHEN SORTING"), _("Ignore 'The' and 'A' if at the start."), "IgnoreLeadingArticles", true, [s] { s->setVariable("reloadAll", true); });
	
	s->onFinalize([s, pthis, window]
	{
		if (s->getVariable("reloadCollections"))
			CollectionSystemManager::get()->updateSystemsList();

		if (s->getVariable("reloadAll"))
		{
			ViewController::get()->reloadAll(window);
			window->closeSplashScreen();
		}

		if (s->getVariable("reloadGuiMenu"))
		{
			delete pthis;
			window->pushGui(new GuiMenu(window));
		}
	});

	mWindow->pushGui(s);
}

void GuiMenu::openSoundSettings()
{
	auto s = new GuiSettings(mWindow, _("SOUND SETTINGS").c_str());

	if (VolumeControl::getInstance()->isAvailable())
	{
		s->addGroup(_("VOLUME"));

		// volume
		auto volume = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
		volume->setValue((float)VolumeControl::getInstance()->getVolume());
		volume->setOnValueChanged([](const float &newVal) { VolumeControl::getInstance()->setVolume((int)Math::round(newVal)); });
		s->addWithLabel(_("SYSTEM VOLUME"), volume);
		s->addSaveFunc([this, volume]
		{
			VolumeControl::getInstance()->setVolume((int)Math::round(volume->getValue()));
#if !WIN32
			SystemConf::getInstance()->set("audio.volume", std::to_string((int)round(volume->getValue())));
#endif
		});


		// Music Volume
		auto musicVolume = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
		musicVolume->setValue(Settings::getInstance()->getInt("MusicVolume"));
		musicVolume->setOnValueChanged([](const float &newVal) { Settings::getInstance()->setInt("MusicVolume", (int)round(newVal)); });
		s->addWithLabel(_("MUSIC VOLUME"), musicVolume);

		s->addSwitch(_("SHOW OVERLAY WHEN VOLUME CHANGES"), "VolumePopup", true);
	}

	s->addGroup(_("MUSIC"));

	s->addSwitch(_("FRONTEND MUSIC"), "audio.bgmusic", true, []
	{
		if (Settings::getInstance()->getBool("audio.bgmusic"))
			AudioManager::getInstance()->playRandomMusic();
		else
			AudioManager::getInstance()->stopMusic();
	});
	
	s->addSwitch(_("DISPLAY SONG TITLES"), "audio.display_titles", true);

	// how long to display the song titles?
	auto titles_time = std::make_shared<SliderComponent>(mWindow, 2.f, 120.f, 2.f, "s");
	titles_time->setValue(Settings::getInstance()->getInt("audio.display_titles_time"));
	s->addWithLabel(_("SONG TITLE DISPLAY DURATION"), titles_time);
	s->addSaveFunc([titles_time] {
		Settings::getInstance()->setInt("audio.display_titles_time", (int)Math::round(titles_time->getValue()));
	});

	s->addSwitch(_("ONLY PLAY SYSTEM-SPECIFIC MUSIC FOLDER"), "audio.persystem", true, [] { AudioManager::getInstance()->changePlaylist(ViewController::get()->getState().getSystem()->getTheme(), true); } );
	s->addSwitch(_("PLAY SYSTEM-SPECIFIC MUSIC"), "audio.thememusics", true, [] { AudioManager::getInstance()->changePlaylist(ViewController::get()->getState().getSystem()->getTheme(), true); });	
	s->addSwitch(_("LOWER MUSIC WHEN PLAYING VIDEO"), "VideoLowersMusic", true);

	s->addGroup(_("SOUNDS"));

	s->addSwitch(_("ENABLE NAVIGATION SOUNDS"), "EnableSounds", true, []
	{
		if (Settings::getInstance()->getBool("EnableSounds") && PowerSaver::getMode() == PowerSaver::INSTANT)
		{
			Settings::getInstance()->setPowerSaverMode("default");
			PowerSaver::init();
		}
	});

	s->addSwitch(_("ENABLE VIDEO PREVIEW AUDIO"), "VideoAudio", true);
	
	mWindow->pushGui(s);
}

void GuiMenu::openWifiSettings(Window* win, std::string title, std::string data, const std::function<void(std::string)>& onsave)
{
	win->pushGui(new GuiWifi(win, title, data, onsave));
}

void GuiMenu::openNetworkSettings(bool selectWifiEnable)
{
	bool baseWifiEnabled = SystemConf::getInstance()->getBool("wifi.enabled");

	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	Window *window = mWindow;

	auto s = new GuiSettings(mWindow, _("NETWORK SETTINGS").c_str());
	s->addGroup(_("INFORMATION"));

	auto ip = std::make_shared<TextComponent>(mWindow, ApiSystem::getInstance()->getIpAdress(), font, color);
	s->addWithLabel(_("IP ADDRESS"), ip);

	auto status = std::make_shared<TextComponent>(mWindow, ApiSystem::getInstance()->ping() ? _("CONNECTED") : _("NOT CONNECTED"), font, color);
	s->addWithLabel(_("INTERNET STATUS"), status);

	// Network Indicator
	auto networkIndicator = std::make_shared<SwitchComponent>(mWindow);
	networkIndicator->setState(Settings::getInstance()->getBool("ShowNetworkIndicator"));
	s->addWithLabel(_("SHOW NETWORK INDICATOR"), networkIndicator);
	s->addSaveFunc([networkIndicator] { Settings::getInstance()->setBool("ShowNetworkIndicator", networkIndicator->getState()); });

	s->addGroup(_("SETTINGS"));

#if !WIN32
	// Hostname
	s->addInputTextRow(_("HOSTNAME"), "system.hostname", false);
#endif

	// Wifi enable
	auto enable_wifi = std::make_shared<SwitchComponent>(mWindow);	
	enable_wifi->setState(baseWifiEnabled);
	s->addWithLabel(_("ENABLE WIFI"), enable_wifi, selectWifiEnable);

	// window, title, settingstring,
	const std::string baseSSID = SystemConf::getInstance()->get("wifi.ssid");
	const std::string baseKEY = SystemConf::getInstance()->get("wifi.key");

	if (baseWifiEnabled)
	{
		s->addInputTextRow(_("WIFI SSID"), "wifi.ssid", false, false, &openWifiSettings);
		s->addInputTextRow(_("WIFI KEY"), "wifi.key", true);
	}
	
	s->addSaveFunc([baseWifiEnabled, baseSSID, baseKEY, enable_wifi, window]
	{
		bool wifienabled = enable_wifi->getState();

		SystemConf::getInstance()->setBool("wifi.enabled", wifienabled);

		if (wifienabled) 
		{
			std::string newSSID = SystemConf::getInstance()->get("wifi.ssid");
			std::string newKey = SystemConf::getInstance()->get("wifi.key");

			if (baseSSID != newSSID || baseKEY != newKey || !baseWifiEnabled)
			{
				if (ApiSystem::getInstance()->enableWifi(newSSID, newKey)) 
					window->pushGui(new GuiMsgBox(window, _("WIFI ENABLED")));
				else 
					window->pushGui(new GuiMsgBox(window, _("WIFI CONFIGURATION ERROR")));
			}
		}
		else if (baseWifiEnabled)
			ApiSystem::getInstance()->disableWifi();
	});
	

	enable_wifi->setOnChangedCallback([this, s, baseWifiEnabled, enable_wifi]()
	{
		bool wifienabled = enable_wifi->getState();
		if (baseWifiEnabled != wifienabled)
		{
			SystemConf::getInstance()->setBool("wifi.enabled", wifienabled);

			if (wifienabled)
				ApiSystem::getInstance()->enableWifi(SystemConf::getInstance()->get("wifi.ssid"), SystemConf::getInstance()->get("wifi.key"));
			else
				ApiSystem::getInstance()->disableWifi();

			delete s;
			openNetworkSettings(true);
		}
	});

	mWindow->pushGui(s);
}

void GuiMenu::openQuitMenu()
{
  GuiMenu::openQuitMenu_static(mWindow);
}

void GuiMenu::openQuitMenu_static(Window *window, bool quickAccessMenu, bool animate)
{
#ifdef WIN32
	if (!quickAccessMenu && Settings::getInstance()->getBool("ShowOnlyExit") && Settings::getInstance()->getBool("ShowExit"))
	{
		Utils::Platform::quitES(Utils::Platform::QuitMode::QUIT);
		return;
	}
#endif

	auto s = new GuiSettings(window, (quickAccessMenu ? _("QUICK ACCESS") : _("QUIT")).c_str());
	s->setCloseButton("select");

	if (quickAccessMenu)
	{
		s->addGroup(_("QUICK ACCESS"));

		// Don't like one of the songs? Press next
		if (AudioManager::getInstance()->isSongPlaying())
		{
			auto sname = AudioManager::getInstance()->getSongName();
			if (!sname.empty())
			{
				s->addWithDescription(_("SKIP TO THE NEXT SONG"), _("NOW PLAYING") + ": " + sname, nullptr, [s, window]
					{
						Window* w = window;
						AudioManager::getInstance()->playRandomMusic(false);
						delete s;
						openQuitMenu_static(w, true, false);
					}, "iconSound");
			}
		}

		s->addEntry(_("LAUNCH SCREENSAVER"), false, [s, window]
			{
				Window* w = window;
				window->postToUiThread([w]()
					{
						w->startScreenSaver();
						w->renderScreenSaver();
					});
				delete s;

			}, "iconScraper", true);

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::PDFEXTRACTION) && Utils::FileSystem::exists(Paths::getUserManualPath()))
		{
			s->addEntry(_("VIEW USER MANUAL"), false, [s, window]
				{
					GuiImageViewer::showPdf(window, Paths::getUserManualPath());
					delete s;
				}, "iconManual");
		}

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::WRITEPLANEMODE))
		{
			if (ApiSystem::getInstance()->isPlaneMode())
			{
				s->addEntry(_("DISABLE PLANE MODE"), false, [window, s]
					{
						ApiSystem::getInstance()->setPlaneMode(false);
						delete s;
					}, "iconPlanemode");
			}
			else 
			{
				s->addEntry(_("ENABLE PLANE MODE"), false, [window, s]
					{
						ApiSystem::getInstance()->setPlaneMode(true);
						delete s;
					}, "iconPlanemode");
			}
		}
	}

	if (quickAccessMenu)
		s->addGroup(_("QUIT"));

	s->addEntry(_("RESTART SYSTEM"), false, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY RESTART?"), 
			_("YES"), [] { Utils::Platform::quitES(Utils::Platform::QuitMode::REBOOT); },
			_("NO"), nullptr));
	}, "iconRestart");

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SUSPEND))
	{
		s->addEntry(_("SUSPEND SYSTEM"), false, [window, s] {
			window->pushGui(new GuiMsgBox(window, _("REALLY SUSPEND ?"),
				_("YES"), [s] { s->close(); ApiSystem::getInstance()->suspend(); },
				_("NO"), nullptr));
		}, "iconFastShutdown");
	}

	s->addEntry(_("SHUTDOWN SYSTEM"), false, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY SHUTDOWN?"), 
			_("YES"), [] { Utils::Platform::quitES(Utils::Platform::QuitMode::SHUTDOWN); },
			_("NO"), nullptr));
	}, "iconShutdown");

	s->addWithDescription(_("FAST SHUTDOWN SYSTEM"), _("Shutdown without saving metadata."), nullptr, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY SHUTDOWN WITHOUT SAVING METADATA?"), 
			_("YES"), [] { Utils::Platform::quitES(Utils::Platform::QuitMode::FAST_SHUTDOWN); },
			_("NO"), nullptr));
	}, "iconFastShutdown");

#ifdef WIN32
	if (Settings::getInstance()->getBool("ShowExit"))
	{
		s->addEntry(_("QUIT EMULATIONSTATION"), false, [window] {
			window->pushGui(new GuiMsgBox(window, _("REALLY QUIT?"), 
				_("YES"), [] { Utils::Platform::quitES(Utils::Platform::QuitMode::QUIT); },
				_("NO"), nullptr));
		}, "iconQuit");
	}
#endif

	if (quickAccessMenu && animate)
		s->getMenu().animateTo(Vector2f((Renderer::getScreenWidth() - s->getMenu().getSize().x()) / 2, (Renderer::getScreenHeight() - s->getMenu().getSize().y()) / 2));
	else if (quickAccessMenu)
		s->getMenu().setPosition((Renderer::getScreenWidth() - s->getMenu().getSize().x()) / 2, (Renderer::getScreenHeight() - s->getMenu().getSize().y()) / 2);

	window->pushGui(s);
}

void GuiMenu::createDecorationItemTemplate(Window* window, std::vector<DecorationSetInfo> sets, std::string data, ComponentListRow& row)
{
	Vector2f maxSize(Renderer::getScreenWidth() * 0.14, Renderer::getScreenHeight() * 0.14);

	int IMGPADDING = Renderer::getScreenHeight()*0.01f;

	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	// spacer between icon and text
	auto spacer = std::make_shared<GuiComponent>(window);
	spacer->setSize(IMGPADDING, 0);
	row.addElement(spacer, false);

	std::string label = data;
	if (data.empty())
		label = _("AUTO");
	else if (data == "none")
		label = _("NONE");
	else
		label = Utils::String::toUpper(Utils::String::replace(data, "_", " "));
		
	row.addElement(std::make_shared<TextComponent>(window, label, font, color, ALIGN_LEFT), true);

	std::string imageUrl;

	for (auto set : sets)
		if (set.name == data)
			imageUrl = set.imageUrl;

	// image
	if (!imageUrl.empty())
	{
		auto icon = std::make_shared<ImageComponent>(window);
		icon->setImage(imageUrl, false, maxSize);
		icon->setMaxSize(maxSize);
		icon->setColorShift(theme->Text.color);
		icon->setPadding(IMGPADDING);
		row.addElement(icon, false);
	}
}

void GuiMenu::popSystemConfigurationGui(Window* mWindow, SystemData* systemData) 
{  
	popSpecificConfigurationGui(mWindow, 
		systemData->getFullName(), 
		systemData->getName(), 
		systemData, 
		nullptr);
}

void GuiMenu::popGameConfigurationGui(Window* mWindow, FileData* fileData)
{
	popSpecificConfigurationGui(mWindow,
		fileData->getName(),
		fileData->getConfigurationName(),
		fileData->getSourceFileData()->getSystem(),
		fileData);
}

void GuiMenu::popSpecificConfigurationGui(Window* mWindow, std::string title, std::string configName, SystemData *systemData, FileData* fileData, bool selectCoreLine)
{
	// The system configuration
	GuiSettings* systemConfiguration = new GuiSettings(mWindow, title.c_str());

	if (fileData != nullptr)
		systemConfiguration->setSubTitle(systemData->getFullName());

	std::string currentEmulator = fileData != nullptr ? fileData->getEmulator(false) : systemData->getEmulator(false);
	std::string currentCore = fileData != nullptr ? fileData->getCore(false) : systemData->getCore(false);

	if (systemData->hasEmulatorSelection())
	{
		auto emulChoice = std::make_shared<OptionListComponent<std::string>>(mWindow, _("Emulator"), false);
		emulChoice->add(_("AUTO"), "", false);
		for (auto& emul : systemData->getEmulators())
		{
			if (emul.cores.size() == 0)
				emulChoice->add(emul.name, emul.name, emul.name == currentEmulator);
			else
			{
				for (auto& core : emul.cores)
				{
					bool selected = (emul.name == currentEmulator && core.name == currentCore);

					if (emul.name == core.name)
						emulChoice->add(emul.name, emul.name + "/" + core.name, selected);
					else
						emulChoice->add(emul.name + ": " + Utils::String::replace(core.name, "_", " "), emul.name + "/" + core.name, selected);
				}
			}
		}

		if (!emulChoice->hasSelection())
			emulChoice->selectFirstItem();

		emulChoice->setSelectedChangedCallback([mWindow, title, systemConfiguration, systemData, fileData, configName, emulChoice](std::string s)
		{
			std::string newEmul;
			std::string newCore;

			auto values = Utils::String::split(emulChoice->getSelected(), '/');
			if (values.size() > 0)
				newEmul = values[0];

			if (values.size() > 1)
				newCore = values[1];

			if (fileData != nullptr)
			{
				fileData->setEmulator(newEmul);
				fileData->setCore(newCore);
			}
			else
			{
				SystemConf::getInstance()->set(configName + ".emulator", newEmul);
				SystemConf::getInstance()->set(configName + ".core", newCore);
			}

			popSpecificConfigurationGui(mWindow, title, configName, systemData, fileData);
			delete systemConfiguration;

		});

		systemConfiguration->addWithLabel(_("Emulator"), emulChoice);
	}

	auto customFeatures = systemData->getCustomFeatures(currentEmulator, currentCore);

	// Screen ratio choice
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::ratio))
	{
		auto ratio_choice = createRatioOptionList(mWindow, configName);
		systemConfiguration->addWithDescription(_("GAME ASPECT RATIO"), _("Force the game to render in this aspect ratio."), ratio_choice);
		systemConfiguration->addSaveFunc([configName, ratio_choice] { SystemConf::getInstance()->set(configName + ".ratio", ratio_choice->getSelected()); });
	}

	// video resolution mode
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::videomode))
	{
		auto videoResolutionMode_choice = createVideoResolutionModeOptionList(mWindow, configName);
		systemConfiguration->addWithDescription(_("VIDEO MODE"), _("Sets the display's resolution. Does not affect the rendering resolution."), videoResolutionMode_choice);
		systemConfiguration->addSaveFunc([configName, videoResolutionMode_choice] { SystemConf::getInstance()->set(configName + ".videomode", videoResolutionMode_choice->getSelected()); });
	}

	// smoothing
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::smooth))
	{
		auto smoothing_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SMOOTH GAMES (BILINEAR FILTERING)"));
		smoothing_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".smooth"));
		systemConfiguration->addWithLabel(_("SMOOTH GAMES (BILINEAR FILTERING)"), smoothing_enabled);
		systemConfiguration->addSaveFunc([configName, smoothing_enabled] { SystemConf::getInstance()->set(configName + ".smooth", smoothing_enabled->getSelected()); });
	}

	// Rewind
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::rewind))
	{
		auto rewind_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("REWIND"));
		rewind_enabled->addRange({ { _("AUTO"), "auto" }, { _("ON") , "1" }, { _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".rewind"));
		systemConfiguration->addWithLabel(_("REWIND"), rewind_enabled);
		systemConfiguration->addSaveFunc([configName, rewind_enabled] { SystemConf::getInstance()->set(configName + ".rewind", rewind_enabled->getSelected()); });
	}

	// AUTO SAVE/LOAD
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::autosave) && !customFeatures.hasFeature("autosave"))
	{
		auto autosave_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("AUTO SAVE/LOAD ON GAME LAUNCH"));
		autosave_enabled->addRange({ { _("AUTO"), "auto" }, { _("ON") , "1" }, { _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".autosave"));
		systemConfiguration->addWithDescription(_("AUTO SAVE/LOAD ON GAME LAUNCH"), _("Load latest save state on game launch and save state when exiting game."), autosave_enabled);
		systemConfiguration->addSaveFunc([configName, autosave_enabled] { SystemConf::getInstance()->set(configName + ".autosave", autosave_enabled->getSelected()); });
	}
	
	// Shaders preset
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SHADERS) &&
		systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::shaders))
	{
		auto installedShaders = ApiSystem::getInstance()->getShaderList(systemData->getName(), currentEmulator, currentCore);
		if (installedShaders.size() > 0)
		{
			std::string currentShader = SystemConf::getInstance()->get(configName + ".shaderset");

			auto shaders_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SHADER SET"), false);
			shaders_choices->add(_("AUTO"), "auto", currentShader.empty() || currentShader == "auto");
			shaders_choices->add(_("NONE"), "none", currentShader == "none");

			for (auto shader : installedShaders)
				shaders_choices->add(_(Utils::String::toUpper(shader).c_str()), shader, currentShader == shader);

			if (!shaders_choices->hasSelection())
				shaders_choices->selectFirstItem();

			systemConfiguration->addWithLabel(_("SHADER SET"), shaders_choices);
			systemConfiguration->addSaveFunc([configName, shaders_choices] { SystemConf::getInstance()->set(configName + ".shaderset", shaders_choices->getSelected()); });
		}
	}

	// Video Filters preset
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::VIDEOFILTERS) &&
		systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::videofilters))
	{
		auto installedVideofilters = ApiSystem::getInstance()->getVideoFilterList(systemData->getName(), currentEmulator, currentCore);
		if (installedVideofilters.size() > 0)
		{
			std::string currentVideofilter = SystemConf::getInstance()->get(configName + ".videofilter");

			auto videofilters_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("VIDEO FILTER"), false);
			videofilters_choices->add(_("AUTO"), "auto", currentVideofilter.empty() || currentVideofilter == "auto");
			videofilters_choices->add(_("NONE"), "none", currentVideofilter == "none");

			for (auto videofilter : installedVideofilters)
				videofilters_choices->add(_(Utils::String::toUpper(videofilter).c_str()), videofilter, currentVideofilter == videofilter);

			if (!videofilters_choices->hasSelection())
				videofilters_choices->selectFirstItem();

			systemConfiguration->addWithLabel(_("VIDEO FILTER"), videofilters_choices);
			systemConfiguration->addSaveFunc([configName, videofilters_choices] { SystemConf::getInstance()->set(configName + ".videofilter", videofilters_choices->getSelected()); });
		}
	}

	// Integer scale
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::pixel_perfect))
	{
		auto integerscale_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INTEGER SCALING (PIXEL PERFECT)"));
		integerscale_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".integerscale"));
		systemConfiguration->addWithLabel(_("INTEGER SCALING (PIXEL PERFECT)"), integerscale_enabled);
		systemConfiguration->addSaveFunc([integerscale_enabled, configName] { SystemConf::getInstance()->set(configName + ".integerscale", integerscale_enabled->getSelected()); });
	}

	// decorations
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DECORATIONS) && systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::decoration))
	{
		auto sets = GuiMenu::getDecorationsSets(systemData);
		if (sets.size() > 0)
		{
#ifdef BATOCERA
			systemConfiguration->addEntry(_("DECORATIONS"), true, [mWindow, configName, systemData, sets]
			{
				GuiSettings* decorations_window = new GuiSettings(mWindow, _("DECORATIONS").c_str());

				addDecorationSetOptionListComponent(mWindow, decorations_window, sets, configName);
				
				// stretch bezels
				auto bezel_stretch_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("STRETCH BEZELS (4K & ULTRAWIDE)"));
				bezel_stretch_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".bezel_stretch") != "0" && SystemConf::getInstance()->get(configName + ".bezel_stretch") != "1");
				bezel_stretch_enabled->add(_("ON"), "1", SystemConf::getInstance()->get(configName + ".bezel_stretch") == "1");
				bezel_stretch_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get(configName + ".bezel_stretch") == "0");
				decorations_window->addWithLabel(_("STRETCH BEZELS (4K & ULTRAWIDE)"), bezel_stretch_enabled);
				decorations_window->addSaveFunc([bezel_stretch_enabled, configName] {
					if (bezel_stretch_enabled->changed()) {
						SystemConf::getInstance()->set(configName + ".bezel_stretch", bezel_stretch_enabled->getSelected());
						SystemConf::getInstance()->saveSystemConf();
					}
				});

				// tattoo and controller overlays
				auto bezel_tattoo = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW CONTROLLER OVERLAYS"));
				bezel_tattoo->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".bezel.tattoo") != "0"
					&& SystemConf::getInstance()->get(configName + ".bezel.tattoo") != "system"
					&& SystemConf::getInstance()->get(configName + ".bezel.tattoo") != "custom");
				bezel_tattoo->add(_("NO"), "0", SystemConf::getInstance()->get(configName + ".bezel.tattoo") == "0");
				bezel_tattoo->add(_("SYSTEM CONTROLLERS"), "system", SystemConf::getInstance()->get(configName + ".bezel.tattoo") == "system");
				bezel_tattoo->add(_("CUSTOM .PNG IMAGE"), "custom", SystemConf::getInstance()->get(configName + ".bezel.tattoo") == "custom");
				decorations_window->addWithLabel(_("SHOW CONTROLLER OVERLAYS"), bezel_tattoo);
				decorations_window->addSaveFunc([bezel_tattoo, configName] {
					if (bezel_tattoo->changed()) {
						SystemConf::getInstance()->set(configName + ".bezel.tattoo", bezel_tattoo->getSelected());
						SystemConf::getInstance()->saveSystemConf();
					}
				});

				auto bezel_tattoo_corner = std::make_shared<OptionListComponent<std::string>>(mWindow, _("OVERLAY CORNER"));
				bezel_tattoo_corner->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") != "NW"
					&& SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") != "NE"
					&& SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") != "SE"
					&& SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") != "SW");
				bezel_tattoo_corner->add(_("NORTH WEST"), "NW", SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") == "NW");
				bezel_tattoo_corner->add(_("NORTH EAST"), "NE", SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") == "NE");
				bezel_tattoo_corner->add(_("SOUTH EAST"), "SE", SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") == "SE");
				bezel_tattoo_corner->add(_("SOUTH WEST"), "SW", SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") == "SW");
				decorations_window->addWithLabel(_("OVERLAY CORNER"), bezel_tattoo_corner);
				decorations_window->addSaveFunc([bezel_tattoo_corner, configName] {
					if (bezel_tattoo_corner->changed()) {
						SystemConf::getInstance()->set(configName + ".bezel.tattoo_corner", bezel_tattoo_corner->getSelected());
						SystemConf::getInstance()->saveSystemConf();
					}
				});

				std::string tatpath = configName + ".bezel.tattoo_file";
				const char *bezelpath = const_cast<char*>(tatpath.data());
				decorations_window->addInputTextRow(_("CUSTOM .PNG IMAGE PATH"), bezelpath, false);

				mWindow->pushGui(decorations_window);
			});
#else
			addDecorationSetOptionListComponent(mWindow, systemConfiguration, sets, configName);
#endif		
		}
	}	

	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::latency_reduction))	
		systemConfiguration->addEntry(_("LATENCY REDUCTION"), true, [mWindow, configName] { openLatencyReductionConfiguration(mWindow, configName); });

	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::colorization))
	{
		// gameboy colorize
		auto colorizations_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("COLORIZATION"), false);
		std::string currentColorization = SystemConf::getInstance()->get(configName + "-renderer.colorization");
		if (currentColorization.empty())
			currentColorization = std::string("auto");
		
		colorizations_choices->add(_("AUTO"), "auto", currentColorization == "auto");
		colorizations_choices->add(_("NONE"), "none", currentColorization == "none");

		const char* all_gambate_gc_colors_modes[] = { "GB - DMG",
								 "GB - Light",
								 "GB - Pocket",
								 "GBC - Blue",
								 "GBC - Brown",
								 "GBC - Dark Blue",
								 "GBC - Dark Brown",
								 "GBC - Dark Green",
								 "GBC - Grayscale",
								 "GBC - Green",
								 "GBC - Inverted",
								 "GBC - Orange",
								 "GBC - Pastel Mix",
								 "GBC - Red",
								 "GBC - Yellow",
								 "SGB - 1A",
								 "SGB - 1B",
								 "SGB - 1C",
								 "SGB - 1D",
								 "SGB - 1E",
								 "SGB - 1F",
								 "SGB - 1G",
								 "SGB - 1H",
								 "SGB - 2A",
								 "SGB - 2B",
								 "SGB - 2C",
								 "SGB - 2D",
								 "SGB - 2E",
								 "SGB - 2F",
								 "SGB - 2G",
								 "SGB - 2H",
								 "SGB - 3A",
								 "SGB - 3B",
								 "SGB - 3C",
								 "SGB - 3D",
								 "SGB - 3E",
								 "SGB - 3F",
								 "SGB - 3G",
								 "SGB - 3H",
								 "SGB - 4A",
								 "SGB - 4B",
								 "SGB - 4C",
								 "SGB - 4D",
								 "SGB - 4E",
								 "SGB - 4F",
								 "SGB - 4G",
								 "SGB - 4H",
								 "Special 1",
								 "Special 2",
								 "Special 3",
								 "TWB01 - 756 Production",
								 "TWB02 - AKB48 Pink",
								 "TWB03 - Angry Volcano",
								 "TWB04 - Anime Expo",
								 "TWB05 - Aqours Blue",
								 "TWB06 - Aquatic Iro",
								 "TWB07 - Bandai Namco",
								 "TWB08 - Blossom Pink",
								 "TWB09 - Bubbles Blue",
								 "TWB10 - Builder Yellow",
								 "TWB11 - Buttercup Green",
								 "TWB12 - Camouflage",
								 "TWB13 - Cardcaptor Pink",
								 "TWB14 - Christmas",
								 "TWB15 - Crunchyroll Orange",
								 "TWB16 - Digivice",
								 "TWB17 - Do The Dew",
								 "TWB18 - Eevee Brown",
								 "TWB19 - Fruity Orange",
								 "TWB20 - Game.com",
								 "TWB21 - Game Grump Orange",
								 "TWB22 - GameKing",
								 "TWB23 - Game Master",
								 "TWB24 - Ghostly Aoi",
								 "TWB25 - Golden Wild",
								 "TWB26 - Green Banana",
								 "TWB27 - Greenscale",
								 "TWB28 - Halloween",
								 "TWB29 - Hero Yellow",
								 "TWB30 - Hokage Orange",
								 "TWB31 - Labo Fawn",
								 "TWB32 - Legendary Super Saiyan",
								 "TWB33 - Lemon Lime Green",
								 "TWB34 - Lime Midori",
								 "TWB35 - Mania Plus Green",
								 "TWB36 - Microvision",
								 "TWB37 - Million Live Gold",
								 "TWB38 - Miraitowa Blue",
								 "TWB39 - NASCAR",
								 "TWB40 - Neo Geo Pocket",
								 "TWB41 - Neon Blue",
								 "TWB42 - Neon Green",
								 "TWB43 - Neon Pink",
								 "TWB44 - Neon Red",
								 "TWB45 - Neon Yellow",
								 "TWB46 - Nick Orange",
								 "TWB47 - Nijigasaki Orange",
								 "TWB48 - Odyssey Gold",
								 "TWB49 - Patrick Star Pink",
								 "TWB50 - Pikachu Yellow",
								 "TWB51 - Pocket Tales",
								 "TWB52 - Pokemon mini",
								 "TWB53 - Pretty Guardian Gold",
								 "TWB54 - S.E.E.S. Blue",
								 "TWB55 - Saint Snow Red",
								 "TWB56 - Scooby-Doo Mystery",
								 "TWB57 - Shiny Sky Blue",
								 "TWB58 - Sidem Green",
								 "TWB59 - Slime Blue",
								 "TWB60 - Spongebob Yellow",
								 "TWB61 - Stone Orange",
								 "TWB62 - Straw Hat Red",
								 "TWB63 - Superball Ivory",
								 "TWB64 - Super Saiyan Blue",
								 "TWB65 - Super Saiyan Rose",
								 "TWB66 - Supervision",
								 "TWB67 - Survey Corps Brown",
								 "TWB68 - Tea Midori",
								 "TWB69 - TI-83",
								 "TWB70 - Tokyo Midtown",
								 "TWB71 - Travel Wood",
								 "TWB72 - Virtual Boy",
								 "TWB73 - VMU",
								 "TWB74 - Wisteria Murasaki",
								 "TWB75 - WonderSwan",
								 "TWB76 - Yellow Banana" };

		int n_all_gambate_gc_colors_modes = 126;
		for (int i = 0; i < n_all_gambate_gc_colors_modes; i++)
			colorizations_choices->add(all_gambate_gc_colors_modes[i], all_gambate_gc_colors_modes[i], currentColorization == std::string(all_gambate_gc_colors_modes[i]));
		
		if (CustomFeatures::FeaturesLoaded || (!CustomFeatures::FeaturesLoaded && (systemData->getName() == "gb" || systemData->getName() == "gbc" || systemData->getName() == "gb2players" || systemData->getName() == "gbc2players")))  // only for gb, gbc and gb2players
		{
			systemConfiguration->addWithLabel(_("COLORIZATION"), colorizations_choices);
			systemConfiguration->addSaveFunc([colorizations_choices, configName] { SystemConf::getInstance()->set(configName + "-renderer.colorization", colorizations_choices->getSelected()); });
		}		
	}

	// ps2 full boot
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::fullboot))
	{
		if (CustomFeatures::FeaturesLoaded || (!CustomFeatures::FeaturesLoaded && systemData->getName() == "ps2")) // only for ps2			
		{
			auto fullboot_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("FULL BOOT"));
			fullboot_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".fullboot"));
			systemConfiguration->addWithLabel(_("FULL BOOT"), fullboot_enabled);
			systemConfiguration->addSaveFunc([fullboot_enabled, configName] { SystemConf::getInstance()->set(configName + ".fullboot", fullboot_enabled->getSelected()); });
		}
	}

	// wii emulated wiimotes
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::emulated_wiimotes))
	{
		if (CustomFeatures::FeaturesLoaded || (!CustomFeatures::FeaturesLoaded && systemData->getName() == "wii"))  // only for wii
		{
			auto emulatedwiimotes_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("EMULATED WIIMOTES"));
			emulatedwiimotes_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".emulatedwiimotes"));
			systemConfiguration->addWithLabel(_("EMULATED WIIMOTES"), emulatedwiimotes_enabled);
			systemConfiguration->addSaveFunc([emulatedwiimotes_enabled, configName] { SystemConf::getInstance()->set(configName + ".emulatedwiimotes", emulatedwiimotes_enabled->getSelected()); });
		}
	}

	// citra change screen layout
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::screen_layout))
	{
		if (CustomFeatures::FeaturesLoaded || (!CustomFeatures::FeaturesLoaded && systemData->getName() == "3ds"))  // only for 3ds
		{
			auto changescreen_layout = std::make_shared<OptionListComponent<std::string>>(mWindow, _("CHANGE SCREEN LAYOUT"));
			changescreen_layout->addRange({ { _("AUTO"), "auto" },{ _("LARGE SCREEN") , "2" },{ _("SIDE BY SIDE"), "3" } }, SystemConf::getInstance()->get(configName + ".layout_option"));
			systemConfiguration->addWithLabel(_("CHANGE SCREEN LAYOUT"), changescreen_layout);
			systemConfiguration->addSaveFunc([changescreen_layout, configName] { SystemConf::getInstance()->set(configName + ".layout_option", changescreen_layout->getSelected()); });
		}
	}

	// psp internal resolution
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::internal_resolution))
	{
		std::string curResol = SystemConf::getInstance()->get(configName + ".internalresolution");

		auto internalresolution = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INTERNAL RESOLUTION"));
		internalresolution->add(_("AUTO"), "auto", curResol.empty() || curResol == "auto");
		internalresolution->add("1:1", "0", curResol == "0");
		internalresolution->add("x1", "1", curResol == "1");
		internalresolution->add("x2", "2", curResol == "2");
		internalresolution->add("x3", "3", curResol == "3");
		internalresolution->add("x4", "4", curResol == "4");
		internalresolution->add("x5", "5", curResol == "5");
		internalresolution->add("x8", "8", curResol == "8");
		internalresolution->add("x10", "10", curResol == "10");

		if (!internalresolution->hasSelection())
			internalresolution->selectFirstItem();

		if (CustomFeatures::FeaturesLoaded || (!CustomFeatures::FeaturesLoaded && (systemData->getName() == "psp" || systemData->getName() == "wii" || systemData->getName() == "gamecube"))) // only for psp, wii, gamecube
		{
			systemConfiguration->addWithLabel(_("INTERNAL RESOLUTION"), internalresolution);
			systemConfiguration->addSaveFunc([internalresolution, configName] { SystemConf::getInstance()->set(configName + ".internalresolution", internalresolution->getSelected()); });
		}
	}

	// Load per-game / per-emulator / per-system custom features
	addFeatures(customFeatures, mWindow, systemConfiguration, configName, systemData->getName(), currentEmulator.empty() ? systemData->getEmulator(true) : currentEmulator, currentCore.empty() ? systemData->getCore(true) : currentCore, _("SETTINGS"), true);

	// automatic controller configuration
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::autocontrollers))
	{
		auto autoControllers = std::make_shared<OptionListComponent<std::string>>(mWindow, _("AUTOCONFIGURE CONTROLLERS"));
		autoControllers->addRange({ { _("AUTO"), "" },{ _("ON"), "0" },{ _("OFF"), "1" } }, SystemConf::getInstance()->get(configName + ".disableautocontrollers"));
		systemConfiguration->addWithLabel(_("AUTOCONFIGURE CONTROLLERS"), autoControllers);
		systemConfiguration->addSaveFunc([configName, autoControllers] { SystemConf::getInstance()->set(configName + ".disableautocontrollers", autoControllers->getSelected()); });
	}

	if (fileData == nullptr && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::EVMAPY) && systemData->isCurrentFeatureSupported(EmulatorFeatures::Features::padTokeyboard))
	{
		if (systemData->hasKeyboardMapping())
			systemConfiguration->addEntry(_("EDIT PADTOKEY PROFILE"), true, [mWindow, systemData] { editKeyboardMappings(mWindow, systemData, true); });
		else
			systemConfiguration->addEntry(_("CREATE PADTOKEY PROFILE"), true, [mWindow, systemData] { editKeyboardMappings(mWindow, systemData, true); });
	}
	
#ifndef WIN32
	// Set as boot game 
	if (fileData != nullptr)
	{
		std::string gamePath = fileData->getFullPath();

		auto bootgame = std::make_shared<SwitchComponent>(mWindow);
		bootgame->setState(SystemConf::getInstance()->get("global.bootgame.path") == gamePath);
		systemConfiguration->addWithLabel(_("LAUNCH THIS GAME AT STARTUP"), bootgame);
		systemConfiguration->addSaveFunc([bootgame, fileData, gamePath]
		{ 
			if (bootgame->changed()) 
			{
				SystemConf::getInstance()->set("global.bootgame.path", bootgame->getState() ? gamePath : "");
				SystemConf::getInstance()->set("global.bootgame.cmd", bootgame->getState() ? fileData->getlaunchCommand(false) : "");
			}
		});
	}
#endif

	mWindow->pushGui(systemConfiguration);
}

std::shared_ptr<OptionListComponent<std::string>> GuiMenu::createRatioOptionList(Window *window, std::string configname)
{
	auto ratio_choice = std::make_shared<OptionListComponent<std::string> >(window, _("GAME ASPECT RATIO"), false);
	std::string currentRatio = SystemConf::getInstance()->get(configname + ".ratio");
	if (currentRatio.empty())
		currentRatio = std::string("auto");

	std::map<std::string, std::string> *ratioMap = LibretroRatio::getInstance()->getRatio();
	for (auto ratio = ratioMap->begin(); ratio != ratioMap->end(); ratio++)
		ratio_choice->add(_(ratio->first.c_str()), ratio->second, currentRatio == ratio->second);	

	if (!ratio_choice->hasSelection())
		ratio_choice->selectFirstItem();
	
	return ratio_choice;
}

std::shared_ptr<OptionListComponent<std::string>> GuiMenu::createVideoResolutionModeOptionList(Window *window, std::string configname, std::string configoptname) 
{
	auto videoResolutionMode_choice = std::make_shared<OptionListComponent<std::string> >(window, _("VIDEO MODE"), false);

	std::string currentVideoMode = SystemConf::getInstance()->get(configname + "." + configoptname);
	if (currentVideoMode.empty())
		currentVideoMode = std::string("auto");
	
	std::vector<std::string> videoResolutionModeMap = ApiSystem::getInstance()->getVideoModes();
	videoResolutionMode_choice->add(_("AUTO"), "auto", currentVideoMode == "auto");
	for (auto videoMode = videoResolutionModeMap.begin(); videoMode != videoResolutionModeMap.end(); videoMode++)
	{
		std::vector<std::string> tokens = Utils::String::split(*videoMode, ':');

		// concatenat the ending words
		std::string vname;
		for (unsigned int i = 1; i < tokens.size(); i++) 
		{
			if (i > 1) 
				vname += ":";

			vname += tokens.at(i);
		}

		videoResolutionMode_choice->add(_(vname.c_str()), tokens.at(0), currentVideoMode == tokens.at(0));
	}

	if (!videoResolutionMode_choice->hasSelection())
		videoResolutionMode_choice->selectFirstItem();

	return videoResolutionMode_choice;
}

std::vector<DecorationSetInfo> GuiMenu::getDecorationsSets(SystemData* system)
{
	std::vector<DecorationSetInfo> sets;
	if (system == nullptr)
		return sets;

	static const size_t pathCount = 3;

	std::vector<std::string> paths = 
	{
#if WIN32
		Paths::getUserEmulationStationPath() + "/decorations", // for win32 testings
#endif
		Paths::getUserDecorationsPath(),
		Paths::getDecorationsPath()
	};
	
	Utils::FileSystem::stringList dirContent;
	std::string folder;

	for (auto path : paths)
	{
		if (!Utils::FileSystem::isDirectory(path))
			continue;

		dirContent = Utils::FileSystem::getDirContent(path);
		for (Utils::FileSystem::stringList::const_iterator it = dirContent.cbegin(); it != dirContent.cend(); ++it)
		{
			if (Utils::FileSystem::isDirectory(*it))
			{
				folder = *it;

				DecorationSetInfo info;
				info.name = folder.substr(path.size() + 1);
				info.path = folder;

				if (system != nullptr && Utils::String::startsWith(info.name, "default"))
				{
					std::string systemImg = path + "/"+ info.name +"/systems/" + system->getName() + ".png";
					if (Utils::FileSystem::exists(systemImg))
						info.imageUrl = systemImg;
				}

				if (info.imageUrl.empty())
				{
					std::string img = folder + "/default.png";
					if (Utils::FileSystem::exists(img))
						info.imageUrl = img;
				}

				sets.push_back(info);
			}
		}
	}

	struct { bool operator()(DecorationSetInfo& a, DecorationSetInfo& b) const { return a.name < b.name; } } compareByName;
	struct { bool operator()(DecorationSetInfo& a, DecorationSetInfo& b) const { return a.name == b.name; } } nameEquals;

	// sort and remove duplicates
	std::sort(sets.begin(), sets.end(), compareByName);
	sets.erase(std::unique(sets.begin(), sets.end(), nameEquals), sets.end());

	return sets;
}


void GuiMenu::openFormatDriveSettings()
{
	Window *window = mWindow;

	auto s = new GuiSettings(mWindow, _("FORMAT DEVICE").c_str());

	// Drive
	auto optionsStorage = std::make_shared<OptionListComponent<std::string> >(window, _("DEVICE TO FORMAT"), false);

	std::vector<std::string> disks = ApiSystem::getInstance()->getFormatDiskList();
	if (disks.size() == 0)
		optionsStorage->add(_("NONE"), "", false);
	else 
	{
		for (auto disk : disks)
		{
			auto idx = disk.find(" ");
			if (idx != std::string::npos)
				optionsStorage->add(disk.substr(idx + 1), disk.substr(0, idx), false);
		}
	}

	optionsStorage->selectFirstItem();
	s->addWithLabel(_("DEVICE TO FORMAT"), optionsStorage);

	// File system
	auto fileSystem = std::make_shared<OptionListComponent<std::string> >(window, _("FILE SYSTEM"), false);

	std::vector<std::string> fileSystems = ApiSystem::getInstance()->getFormatFileSystems();
	if (fileSystems.size() == 0)
		fileSystem->add(_("NONE"), "", false);
	else
	{
		for (auto fs : fileSystems)
			fileSystem->add(fs, fs, false);
	}

	fileSystem->selectFirstItem();
	s->addWithLabel(_("FILE SYSTEM"), fileSystem);

	s->addEntry(_("FORMAT NOW"), false, [s, optionsStorage, fileSystem, window]
		{
			std::string disk = optionsStorage->getSelected();
			std::string fs = fileSystem->getSelected();

			if (disk.empty() || fs.empty())
			{
				window->pushGui(new GuiMsgBox(window, _("SELECTED OPTIONS ARE INVALID")));
				return;
			}

			window->pushGui(new GuiMsgBox(window, _("ARE YOU SURE YOU WANT TO FORMAT THIS DRIVE?"), _("YES"), [s, window, disk, fs]
			{
				ThreadedFormatter::start(window, disk, fs);
				s->close();
			}, _("NO"), nullptr));
			
		});

	mWindow->pushGui(s);
}



void GuiMenu::saveSubsetSettings()
{
	auto currentSystem = ViewController::get()->getState().getSystem();
	if (currentSystem == nullptr || currentSystem->getTheme() == nullptr)
		return;

	std::string fileData;

	auto subsets = currentSystem->getTheme()->getSubSetNames();
	for (auto subset : subsets)
	{
		std::string name = subset;
		std::string value;

		if (name == "colorset")
			value = Settings::getInstance()->getString("ThemeColorSet");
		else if (name == "iconset")
			value = Settings::getInstance()->getString("ThemeIconSet");
		else if (name == "menu")
			value = Settings::getInstance()->getString("ThemeMenu");
		else if (name == "systemview")
			value = Settings::getInstance()->getString("ThemeSystemView");
		else if (name == "gamelistview")
			value = Settings::getInstance()->getString("ThemeGamelistView");
		else if (name == "region")
			value = Settings::getInstance()->getString("ThemeRegionName");
		else
		{
			value = Settings::getInstance()->getString("subset." + name);
			name = "subset." + name;
		}

		if (!value.empty())
			fileData += name + "=" + value + "\r";

		for (auto system : SystemData::sSystemVector)
		{
			value = Settings::getInstance()->getString("subset." + system->getThemeFolder() + "." + subset);
			if (!value.empty())
				fileData += "subset." + system->getThemeFolder() + "." + subset + "=" + value + "\r";
		}
	}

	if (!Settings::getInstance()->getString("GamelistViewStyle").empty() && Settings::getInstance()->getString("GamelistViewStyle") != "automatic")
		fileData += "GamelistViewStyle=" + Settings::getInstance()->getString("GamelistViewStyle") + "\r";

	if (!Settings::getInstance()->getString("DefaultGridSize").empty())
		fileData += "DefaultGridSize=" + Settings::getInstance()->getString("DefaultGridSize") + "\r";

	for (auto system : SystemData::sSystemVector)
	{
		auto defaultView = Settings::getInstance()->getString(system->getName() + ".defaultView");
		if (!defaultView.empty())
			fileData += system->getName() + ".defaultView=" + defaultView + "\r";

		auto gridSizeOverride = Settings::getInstance()->getString(system->getName() + ".gridSize");
		if (!gridSizeOverride.empty())
			fileData += system->getName() + ".gridSize=" + gridSizeOverride + "\r";
	}

	std::string path = Paths::getUserEmulationStationPath() + "/themesettings";
	if (!Utils::FileSystem::exists(path))
		Utils::FileSystem::createDirectory(path);

	std::string themeSet = Settings::getInstance()->getString("ThemeSet");
	std::string fileName = path + "/" + themeSet + ".cfg";

	if (fileData.empty())
	{
		if (Utils::FileSystem::exists(fileName))
			Utils::FileSystem::removeFile(fileName);
	}
	else
		Utils::FileSystem::writeAllText(fileName, fileData);

}

void GuiMenu::loadSubsetSettings(const std::string themeName)
{
	std::string path = Paths::getUserEmulationStationPath() + "/themesettings";
	if (!Utils::FileSystem::exists(path))
		Utils::FileSystem::createDirectory(path);

	std::string fileName = path + "/" + themeName + ".cfg";
	if (!Utils::FileSystem::exists(fileName))
		return;

	std::string line;
	std::ifstream systemConf(fileName);
	if (systemConf && systemConf.is_open())
	{
		while (std::getline(systemConf, line, '\r'))
		{
			int idx = line.find("=");
			if (idx == std::string::npos || line.find("#") == 0 || line.find(";") == 0)
				continue;

			std::string name = line.substr(0, idx);
			std::string value = line.substr(idx + 1);
			if (!name.empty() && !value.empty())
			{
				if (name == "colorset")
					Settings::getInstance()->setString("ThemeColorSet", value);
				else if (name == "iconset")
					Settings::getInstance()->setString("ThemeIconSet", value);
				else if (name == "menu")
					Settings::getInstance()->setString("ThemeMenu", value);
				else if (name == "systemview")
					Settings::getInstance()->setString("ThemeSystemView", value);
				else if (name == "gamelistview")
					Settings::getInstance()->setString("ThemeGamelistView", value);
				else if (name == "region")
					Settings::getInstance()->setString("ThemeRegionName", value);
				else if (name == "GamelistViewStyle")
					Settings::getInstance()->setString("GamelistViewStyle", value);
				else if (name == "DefaultGridSize")
					Settings::getInstance()->setString("DefaultGridSize", value);
				else if (name.find(".defaultView") != std::string::npos)
					Settings::getInstance()->setString(name, value);
				else if (name.find(".gridSize") != std::string::npos)
					Settings::getInstance()->setString(name, value);
				else if (Utils::String::startsWith(name, "subset."))
					Settings::getInstance()->setString(name, value);
			}
		}
		systemConf.close();

		for (auto system : SystemData::sSystemVector)
		{
			auto defaultView = Settings::getInstance()->getString(system->getName() + ".defaultView");
			auto gridSizeOverride = Vector2f::parseString(Settings::getInstance()->getString(system->getName() + ".gridSize"));
			system->setSystemViewMode(defaultView, gridSizeOverride, false);
		}
	}
	else
		LOG(LogError) << "Unable to open " << fileName;
}

void GuiMenu::editKeyboardMappings(Window *window, IKeyboardMapContainer* mapping, bool editable)
{
	window->pushGui(new GuiKeyMappingEditor(window, mapping, editable));
}

bool GuiMenu::hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult)
{
	if (pResult) pResult->push_back(this); // Always return this as it's a fake fullscreen, so we always have click events
	GuiComponent::hitTest(x, y, parentTransform, pResult);
	return true;
}

bool GuiMenu::onMouseClick(int button, bool pressed, int x, int y)
{
	if (pressed && button == 1 && !mMenu.isMouseOver())
	{
		delete this;
		return true;
	}

	return (button == 1);
}
