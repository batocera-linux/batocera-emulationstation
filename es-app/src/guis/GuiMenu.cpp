#include "guis/GuiMenu.h"

#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiCollectionSystemsOptions.h"
#include "guis/GuiDetectDevice.h"
#include "guis/GuiGeneralScreensaverOptions.h"
#include "guis/GuiSlideshowScreensaverOptions.h"
#include "guis/GuiVideoScreensaverOptions.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiScraperStart.h"
#include "guis/GuiThemeInstallStart.h" //batocera
#include "guis/GuiBezelInstallStart.h" //batocera
#include "guis/GuiSettings.h"
#include "guis/GuiSystemsHide.h" //batocera
#include "guis/GuiRetroAchievements.h" //batocera
#include "guis/GuiGamelistOptions.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "EmulationStation.h"
#include "Scripting.h"
#include "SystemData.h"
#include "VolumeControl.h"
#include <SDL_events.h>
#include <algorithm>
#include "platform.h"

#include "SystemConf.h"
#include "ApiSystem.h"
#include "InputManager.h"
#include "AudioManager.h"
#include <LibretroRatio.h>
#include "guis/GuiAutoScrape.h"
#include "guis/GuiUpdate.h"
#include "guis/GuiInstallStart.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "guis/GuiBackupStart.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiWifi.h"
#include "scrapers/ThreadedScraper.h"
#include "FileSorts.h"
#include "ThreadedHasher.h"
#include "ThreadedBluetooth.h"
#include "views/gamelist/IGameListView.h"

GuiMenu::GuiMenu(Window *window) : GuiComponent(window), mMenu(window, _("MAIN MENU").c_str()), mVersion(window)
{
	// MAIN MENU
	bool isFullUI = UIModeController::getInstance()->isUIModeFull();

	// KODI >
	// GAMES SETTINGS >
	// CONTROLLERS >
	// UI SETTINGS >
	// SOUND SETTINGS >
	// NETWORK >
	// SCRAPER >
	// SYSTEM SETTINGS >
	// QUIT >

	// KODI
#ifdef _ENABLE_KODI_
	if (SystemConf::getInstance()->get("kodi.enabled") != "0")
		addEntry(_("KODI MEDIA CENTER").c_str(), false, [this] { openKodiLauncher_batocera(); }, "iconKodi");	
#endif

	if (isFullUI &&
		SystemConf::getInstance()->get("global.retroachievements") == "1" &&
		SystemConf::getInstance()->get("global.retroachievements.username") != "")
		addEntry(_("RETROACHIEVEMENTS").c_str(), true, [this] { GuiRetroAchievements::show(mWindow); }, "iconRetroachievements");

	// GAMES SETTINGS
	if (isFullUI)
		addEntry(_("GAMES SETTINGS").c_str(), true, [this] { openGamesSettings_batocera(); }, "iconGames");

	// CONTROLLERS SETTINGS
	if (isFullUI)
		addEntry(_("CONTROLLERS SETTINGS").c_str(), true, [this] { openControllersSettings_batocera(); }, "iconControllers");

	if (isFullUI)
		addEntry(_("UI SETTINGS").c_str(), true, [this] { openUISettings(); }, "iconUI");

	// batocera
	if (isFullUI)
		addEntry(_("GAME COLLECTION SETTINGS").c_str(), true, [this] { openCollectionSystemSettings(); }, "iconAdvanced");

	if (isFullUI)
		addEntry(_("SOUND SETTINGS").c_str(), true, [this] { openSoundSettings(); }, "iconSound");

#if !defined(WIN32) || defined(_DEBUG)
	if (isFullUI)
		addEntry(_("NETWORK SETTINGS").c_str(), true, [this] { openNetworkSettings_batocera(); }, "iconNetwork");
#endif

	if (isFullUI)
	{
		addEntry(_("SCRAPE").c_str(), true, [this] { openScraperSettings(); }, "iconScraper");		
//		addEntry(_("SCRAPE").c_str(), true, [this] { openScraperSettings_batocera(); }, "iconScraper");
	}
	
	// SYSTEM
	if (isFullUI)
	{
		addEntry(_("UPDATES & DOWNLOADS"), true, [this] { openUpdatesSettings(); }, "iconUpdates");
		addEntry(_("SYSTEM SETTINGS").c_str(), true, [this] { openSystemSettings_batocera(); }, "iconSystem");
	}
	else
		addEntry(_("INFORMATIONS").c_str(), true, [this] { openSystemInformations_batocera(); }, "iconSystem");

#ifdef WIN32
	addEntry(_("QUIT").c_str(), false, [this] { openQuitMenu_batocera(); }, "iconQuit");
#else
	addEntry(_("QUIT").c_str(), true, [this] { openQuitMenu_batocera(); }, "iconQuit");
#endif
	
	addChild(&mMenu);
	addVersionInfo(); // batocera
	setSize(mMenu.getSize());

	if (Renderer::isSmallScreen())
		animateTo((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);
	else
		animateTo(
			Vector2f((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.9),
			Vector2f((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.15f));
}

void GuiMenu::openScraperSettings()
{
	auto s = new GuiSettings(mWindow, "SCRAPER");

	std::string scraper = Settings::getInstance()->getString("Scraper");

	// scrape from
	auto scraper_list = std::make_shared< OptionListComponent< std::string > >(mWindow, "SCRAPE FROM", false);
	std::vector<std::string> scrapers = getScraperList();

	// Select either the first entry of the one read from the settings, just in case the scraper from settings has vanished.
	for(auto it = scrapers.cbegin(); it != scrapers.cend(); it++)
		scraper_list->add(*it, *it, *it == scraper);

	s->addWithLabel(_("SCRAPE FROM"), scraper_list); // batocera
	s->addSaveFunc([scraper_list] { Settings::getInstance()->setString("Scraper", scraper_list->getSelected()); });


	if (scraper == "ScreenScraper")
	{
		// Image source : <image> tag
		std::string imageSourceName = Settings::getInstance()->getString("ScrapperImageSrc");
		auto imageSource = std::make_shared< OptionListComponent<std::string> >(mWindow, _("IMAGE SOURCE"), false);
		//imageSource->add(_("NONE"), "", imageSourceName.empty());
		imageSource->add(_("SCREENSHOT"), "ss", imageSourceName == "ss");
		imageSource->add(_("TITLE SCREENSHOT"), "sstitle", imageSourceName == "sstitle");
		imageSource->add(_("MIX V1"), "mixrbv1", imageSourceName == "mixrbv1");
		imageSource->add(_("MIX V2"), "mixrbv2", imageSourceName == "mixrbv2");
		imageSource->add(_("BOX 2D"), "box-2D", imageSourceName == "box-2D");
		imageSource->add(_("BOX 3D"), "box-3D", imageSourceName == "box-3D");

		if (!imageSource->hasSelection())
			imageSource->selectFirstItem();

		s->addWithLabel(_("IMAGE SOURCE"), imageSource);
		s->addSaveFunc([imageSource] { Settings::getInstance()->setString("ScrapperImageSrc", imageSource->getSelected()); });

		// Box source : <thumbnail> tag
		std::string thumbSourceName = Settings::getInstance()->getString("ScrapperThumbSrc");
		auto thumbSource = std::make_shared< OptionListComponent<std::string> >(mWindow, _("BOX SOURCE"), false);
		thumbSource->add(_("NONE"), "", thumbSourceName.empty());
		thumbSource->add(_("BOX 2D"), "box-2D", thumbSourceName == "box-2D");
		thumbSource->add(_("BOX 3D"), "box-3D", thumbSourceName == "box-3D");

		if (!thumbSource->hasSelection())
			thumbSource->selectFirstItem();

		s->addWithLabel(_("BOX SOURCE"), thumbSource);
		s->addSaveFunc([thumbSource] { Settings::getInstance()->setString("ScrapperThumbSrc", thumbSource->getSelected()); });

		imageSource->setSelectedChangedCallback([this, thumbSource](std::string value)
		{
			if (value == "box-2D")
				thumbSource->remove(_("BOX 2D"));
			else
				thumbSource->add(_("BOX 2D"), "box-2D", false);

			if (value == "box-3D")
				thumbSource->remove(_("BOX 3D"));
			else
				thumbSource->add(_("BOX 3D"), "box-3D", false);
		});

		// Logo source : <marquee> tag
		std::string logoSourceName = Settings::getInstance()->getString("ScrapperLogoSrc");
		auto logoSource = std::make_shared< OptionListComponent<std::string> >(mWindow, _("LOGO SOURCE"), false);
		logoSource->add(_("NONE"), "", logoSourceName.empty());
		logoSource->add(_("WHEEL"), "wheel", logoSourceName == "wheel");
		logoSource->add(_("MARQUEE"), "marquee", logoSourceName == "marquee");

		if (!logoSource->hasSelection())
			logoSource->selectFirstItem();

		s->addWithLabel(_("LOGO SOURCE"), logoSource);
		s->addSaveFunc([logoSource] { Settings::getInstance()->setString("ScrapperLogoSrc", logoSource->getSelected()); });

		// scrape ratings
		auto scrape_ratings = std::make_shared<SwitchComponent>(mWindow);
		scrape_ratings->setState(Settings::getInstance()->getBool("ScrapeRatings"));
		s->addWithLabel(_("SCRAPE RATINGS"), scrape_ratings); // batocera
		s->addSaveFunc([scrape_ratings] { Settings::getInstance()->setBool("ScrapeRatings", scrape_ratings->getState()); });

		// scrape video
		auto scrape_video = std::make_shared<SwitchComponent>(mWindow);
		scrape_video->setState(Settings::getInstance()->getBool("ScrapeVideos"));
		s->addWithLabel(_("SCRAPE VIDEOS"), scrape_video);
		s->addSaveFunc([scrape_video] { Settings::getInstance()->setBool("ScrapeVideos", scrape_video->getState()); });

		// Account
		createInputTextRow(s, _("USERNAME"), "ScreenScraperUser", false, true);
		createInputTextRow(s, _("PASSWORD"), "ScreenScraperPass", true, true);
	}
	else
	{
		// scrape ratings
		auto scrape_ratings = std::make_shared<SwitchComponent>(mWindow);
		scrape_ratings->setState(Settings::getInstance()->getBool("ScrapeRatings"));
		s->addWithLabel(_("SCRAPE RATINGS"), scrape_ratings); // batocera
		s->addSaveFunc([scrape_ratings] { Settings::getInstance()->setBool("ScrapeRatings", scrape_ratings->getState()); });
	}

	// scrape now
	ComponentListRow row;
	auto openScrapeNow = [this] 
	{ 
		if (ThreadedScraper::isRunning())
		{
			Window* window = mWindow;

			mWindow->pushGui(new GuiMsgBox(mWindow, _("SCRAPING IS RUNNING. DO YOU WANT TO STOP IT ?"), _("YES"), [this, window]
			{
				ThreadedScraper::stop();
			}, _("NO"), nullptr));

			return;
		}

		mWindow->pushGui(new GuiScraperStart(mWindow)); 
	};

	std::function<void()> openAndSave = openScrapeNow;
	openAndSave = [s, openAndSave] { s->save(); openAndSave(); };
	s->addEntry(_("SCRAPE NOW"), false, openAndSave, "iconScraper");
		
	scraper_list->setSelectedChangedCallback([this, s, scraper, scraper_list](std::string value)
	{		
		if (value != scraper && (scraper == "ScreenScraper" || value == "ScreenScraper"))
		{
			Settings::getInstance()->setString("Scraper", value);
			delete s;
			openScraperSettings();
		}
	});

	mWindow->pushGui(s);
}

void GuiMenu::openConfigInput()
{
	Window* window = mWindow;
	window->pushGui(new GuiMsgBox(window, "ARE YOU SURE YOU WANT TO CONFIGURE INPUT?", "YES",
		[window] {
			window->pushGui(new GuiDetectDevice(window, false, nullptr));
		}, "NO", nullptr)
	);
}

void GuiMenu::addVersionInfo()
{
	std::string  buildDate = (Settings::getInstance()->getBool("Debug") ? std::string( "   (" + Utils::String::toUpper(PROGRAM_BUILT_STRING) + ")") : (""));

	auto theme = ThemeData::getMenuTheme();

	mVersion.setFont(theme->Footer.font);
	mVersion.setColor(theme->Footer.color);

	mVersion.setLineSpacing(0);
	if (!ApiSystem::getInstance()->getVersion().empty())
		mVersion.setText("BATOCERA.LINUX ES V" + ApiSystem::getInstance()->getVersion() + buildDate);

	mVersion.setHorizontalAlignment(ALIGN_CENTER);
	mVersion.setVerticalAlignment(ALIGN_CENTER);
	addChild(&mVersion);
}

void GuiMenu::openScreensaverOptions() 
{
	mWindow->pushGui(new GuiGeneralScreensaverOptions(mWindow, _("SCREENSAVER SETTINGS").c_str()));
}

// new screensaver options for Batocera
void GuiMenu::openSlideshowScreensaverOptions() 
{
	mWindow->pushGui(new GuiSlideshowScreensaverOptions(mWindow, _("SLIDESHOW SETTINGS").c_str()));
}

// new screensaver options for Batocera
void GuiMenu::openVideoScreensaverOptions() {
	mWindow->pushGui(new GuiVideoScreensaverOptions(mWindow, _("RANDOM VIDEO SETTINGS").c_str()));
}


void GuiMenu::openCollectionSystemSettings() {
	mWindow->pushGui(new GuiCollectionSystemsOptions(mWindow));
}

void GuiMenu::onSizeChanged()
{
	float h = mMenu.getButtonGridHeight();

	mVersion.setSize(mSize.x(), h);
	mVersion.setPosition(0, mSize.y() - h); //  mVersion.getSize().y()
}

void GuiMenu::addEntry(std::string name, bool add_arrow, const std::function<void()>& func, const std::string iconName)
{
	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	// populate the list
	ComponentListRow row;

	if (!iconName.empty())
	{
		std::string iconPath = theme->getMenuIcon(iconName);
		if (!iconPath.empty())
		{
			// icon
			auto icon = std::make_shared<ImageComponent>(mWindow, true);
			icon->setImage(iconPath);
			icon->setColorShift(theme->Text.color);
			icon->setResize(0, theme->Text.font->getLetterHeight() * 1.25f);
			row.addElement(icon, false);

			// spacer between icon and text
			auto spacer = std::make_shared<GuiComponent>(mWindow);
			spacer->setSize(10, 0);
			row.addElement(spacer, false);
		}
	}

	row.addElement(std::make_shared<TextComponent>(mWindow, name, font, color), true);

	if (add_arrow)
	{
		std::shared_ptr<ImageComponent> bracket = makeArrow(mWindow);
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
	prompts.push_back(HelpPrompt("up/down", _("CHOOSE"))); // batocera
	prompts.push_back(HelpPrompt(BUTTON_OK, _("SELECT"))); // batocera
	prompts.push_back(HelpPrompt("start", _("CLOSE"))); // batocera
	return prompts;
}

void GuiMenu::openKodiLauncher_batocera()
{
  Window *window = mWindow;
  if (!ApiSystem::getInstance()->launchKodi(window)) {
    LOG(LogWarning) << "Shutdown terminated with non-zero result!";
  }
}

void GuiMenu::openSystemInformations_batocera()
{
	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;


	Window *window = mWindow;
	bool isFullUI = UIModeController::getInstance()->isUIModeFull();
	GuiSettings *informationsGui = new GuiSettings(window, _("INFORMATION").c_str());

	auto version = std::make_shared<TextComponent>(window, ApiSystem::getInstance()->getVersion(), font, color);
	informationsGui->addWithLabel(_("VERSION"), version);

	bool warning = ApiSystem::getInstance()->isFreeSpaceLimit();
	auto space = std::make_shared<TextComponent>(window,
		ApiSystem::getInstance()->getFreeSpaceInfo(),
		font,
		warning ? 0xFF0000FF : color);
	informationsGui->addWithLabel(_("DISK USAGE"), space);

	// various informations
	std::vector<std::string> infos = ApiSystem::getInstance()->getSystemInformations();
	for (auto it = infos.begin(); it != infos.end(); it++) {
		std::vector<std::string> tokens = Utils::String::split(*it, ':');

		if (tokens.size() >= 2) {
			// concatenat the ending words
			std::string vname = "";
			for (unsigned int i = 1; i < tokens.size(); i++) {
				if (i > 1) vname += " ";
				vname += tokens.at(i);
			}

			auto space = std::make_shared<TextComponent>(window,
				vname,
				font,
				color);
			informationsGui->addWithLabel(tokens.at(0), space);
		}
	}
	
	window->pushGui(informationsGui);
}

void GuiMenu::openDeveloperSettings()
{
	Window *window = mWindow;

	auto s = new GuiSettings(mWindow, _("DEVELOPER").c_str());
	
	// maximum vram
	auto max_vram = std::make_shared<SliderComponent>(mWindow, 40.f, 1000.f, 10.f, "Mb");
	max_vram->setValue((float)(Settings::getInstance()->getInt("MaxVRAM")));
	s->addWithLabel(_("VRAM LIMIT"), max_vram);
	s->addSaveFunc([max_vram] { Settings::getInstance()->setInt("MaxVRAM", (int)round(max_vram->getValue())); });
	
	// framerate
	auto framerate = std::make_shared<SwitchComponent>(mWindow);
	framerate->setState(Settings::getInstance()->getBool("DrawFramerate"));
	s->addWithLabel(_("SHOW FRAMERATE"), framerate);
	s->addSaveFunc(
		[framerate] { Settings::getInstance()->setBool("DrawFramerate", framerate->getState()); });
	
	// vsync
	auto vsync = std::make_shared<SwitchComponent>(mWindow);
	vsync->setState(Settings::getInstance()->getBool("VSync"));
	s->addWithLabel(_("VSYNC"), vsync);
	s->addSaveFunc([vsync]
	{
		if (Settings::getInstance()->setBool("VSync", vsync->getState()))
			Renderer::setSwapInterval();
	});

#if !defined(WIN32) || defined(_DEBUG)
	// overscan
	auto overscan_enabled = std::make_shared<SwitchComponent>(mWindow);
	overscan_enabled->setState(Settings::getInstance()->getBool("Overscan"));
	s->addWithLabel(_("OVERSCAN"), overscan_enabled);
	s->addSaveFunc([overscan_enabled] {
		if (Settings::getInstance()->getBool("Overscan") != overscan_enabled->getState()) {
			Settings::getInstance()->setBool("Overscan", overscan_enabled->getState());
			ApiSystem::getInstance()->setOverscan(overscan_enabled->getState());
		}
	});
#endif

	// preload UI
	auto preloadUI = std::make_shared<SwitchComponent>(mWindow);
	preloadUI->setState(Settings::getInstance()->getBool("PreloadUI"));
	s->addWithLabel(_("PRELOAD UI"), preloadUI);
	s->addSaveFunc([preloadUI] { Settings::getInstance()->setBool("PreloadUI", preloadUI->getState()); });

	// threaded loading
	auto threadedLoading = std::make_shared<SwitchComponent>(mWindow);
	threadedLoading->setState(Settings::getInstance()->getBool("ThreadedLoading"));
	s->addWithLabel(_("THREADED LOADING"), threadedLoading);
	s->addSaveFunc([threadedLoading] { Settings::getInstance()->setBool("ThreadedLoading", threadedLoading->getState()); });

	// threaded loading
	auto asyncImages = std::make_shared<SwitchComponent>(mWindow);
	asyncImages->setState(Settings::getInstance()->getBool("AsyncImages"));
	s->addWithLabel(_("ASYNC IMAGES LOADING"), asyncImages);
	s->addSaveFunc([asyncImages] { Settings::getInstance()->setBool("AsyncImages", asyncImages->getState()); });
	
	// optimizeVram
	auto optimizeVram = std::make_shared<SwitchComponent>(mWindow);
	optimizeVram->setState(Settings::getInstance()->getBool("OptimizeVRAM"));
	s->addWithLabel(_("OPTIMIZE IMAGES VRAM USE"), optimizeVram);
	s->addSaveFunc([optimizeVram] { Settings::getInstance()->setBool("OptimizeVRAM", optimizeVram->getState()); });
	
	// optimizeVideo
	auto optimizeVideo = std::make_shared<SwitchComponent>(mWindow);
	optimizeVideo->setState(Settings::getInstance()->getBool("OptimizeVideo"));
	s->addWithLabel(_("OPTIMIZE VIDEO VRAM USE"), optimizeVideo);
	s->addSaveFunc([optimizeVideo] { Settings::getInstance()->setBool("OptimizeVideo", optimizeVideo->getState()); });


	// enable filters (ForceDisableFilters)
	auto enable_filter = std::make_shared<SwitchComponent>(mWindow);
	enable_filter->setState(!Settings::getInstance()->getBool("ForceDisableFilters"));
	s->addWithLabel(_("ENABLE FILTERS"), enable_filter);
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
	s->addWithLabel(_("PARSE GAMESLISTS ONLY"), parse_gamelists);
	s->addSaveFunc([parse_gamelists] { Settings::getInstance()->setBool("ParseGamelistOnly", parse_gamelists->getState()); });


	auto local_art = std::make_shared<SwitchComponent>(mWindow);
	local_art->setState(Settings::getInstance()->getBool("LocalArt"));
	s->addWithLabel(_("SEARCH FOR LOCAL ART"), local_art);
	s->addSaveFunc([local_art] { Settings::getInstance()->setBool("LocalArt", local_art->getState()); });

	// retroarch.menu_driver = rgui
	auto retroarchRgui = std::make_shared<SwitchComponent>(mWindow);
	retroarchRgui->setState(SystemConf::getInstance()->get("global.retroarch.menu_driver") == "rgui");
	s->addWithLabel(_("USE RETROARCH RGUI MENU"), retroarchRgui);
	s->addSaveFunc([retroarchRgui]
	{ 
		SystemConf::getInstance()->set("global.retroarch.menu_driver", retroarchRgui->getState() ? "rgui" : "");
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
			Log::setupReportingLevel();
			Log::init();			
		}
	});

	// support
	s->addEntry(_("CREATE A SUPPORT FILE"), true, [window] {
		window->pushGui(new GuiMsgBox(window, _("CREATE A SUPPORT FILE ?"), _("YES"),
			[window] {
			if (ApiSystem::getInstance()->generateSupportFile()) {
				window->pushGui(new GuiMsgBox(window, _("FILE GENERATED SUCCESSFULLY"), _("OK")));
			}
			else {
				window->pushGui(new GuiMsgBox(window, _("FILE GENERATION FAILED"), _("OK")));
			}
		}, _("NO"), nullptr));
	});


#ifdef _RPI_
	// Video Player - VideoOmxPlayer
	auto omx_player = std::make_shared<SwitchComponent>(mWindow);
	omx_player->setState(Settings::getInstance()->getBool("VideoOmxPlayer"));
	s->addWithLabel("USE OMX PLAYER (HW ACCELERATED)", omx_player);
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
			window->endRenderLoadingScreen();
		}
	});
#endif

	mWindow->pushGui(s);
}

void GuiMenu::openUpdatesSettings()
{
	GuiSettings *updateGui = new GuiSettings(mWindow, _("UPDATES & DOWNLOADS").c_str());

	// Batocera themes installer/browser
	updateGui->addEntry(_("THEMES"), true, [this] { mWindow->pushGui(new GuiThemeInstallStart(mWindow)); });

	// Batocera integration with theBezelProject
	updateGui->addEntry(_("THE BEZEL PROJECT"), true, [this] { mWindow->pushGui(new GuiBezelInstallMenu(mWindow)); });

#if !defined(WIN32) || defined(_DEBUG)

	// Enable updates
	auto updates_enabled = std::make_shared<SwitchComponent>(mWindow);
	updates_enabled->setState(SystemConf::getInstance()->get("updates.enabled") == "1");
	updateGui->addWithLabel(_("AUTO UPDATES"), updates_enabled);
	updateGui->addSaveFunc([updates_enabled]
	{
		SystemConf::getInstance()->set("updates.enabled", updates_enabled->getState() ? "1" : "0");
		SystemConf::getInstance()->saveSystemConf();
	});

	// Start update
	updateGui->addEntry(GuiUpdate::state == GuiUpdateState::State::UPDATE_READY ? _("APPLY UPDATE") : _("START UPDATE"), true, [this]
	{
		if (GuiUpdate::state == GuiUpdateState::State::UPDATE_READY)
		{
			if (runRestartCommand() != 0)
				LOG(LogWarning) << "Reboot terminated with non-zero result!";
		}
		else if (GuiUpdate::state == GuiUpdateState::State::UPDATER_RUNNING)
			mWindow->pushGui(new GuiMsgBox(mWindow, _("UPDATE IS ALREADY RUNNING")));
		else
			mWindow->pushGui(new GuiUpdate(mWindow));
	});
#endif

	mWindow->pushGui(updateGui);
}

void GuiMenu::openSystemSettings_batocera() 
{
	Window *window = mWindow;

	auto s = new GuiSettings(mWindow, _("SYSTEM SETTINGS").c_str());
	bool isFullUI = UIModeController::getInstance()->isUIModeFull();

	// System informations
	s->addEntry(_("INFORMATION"), true, [this] { openSystemInformations_batocera(); }, "iconSystem");

	// Updates -> Moved one level down
	// s->addEntry(_("UPDATES & DOWNLOADS"), true, [this] { openUpdatesSettings(); }, "iconUpdates");

	// language choice
	auto language_choice = std::make_shared<OptionListComponent<std::string> >(window, _("LANGUAGE"), false);

	std::string language = SystemConf::getInstance()->get("system.language");
	if (language.empty()) 
		language = "en_US";

	language_choice->add("BASQUE", "eu_ES", language == "eu_ES");
	language_choice->add("正體中文", "zh_TW", language == "zh_TW");
	language_choice->add("简体中文", "zh_CN", language == "zh_CN");
	language_choice->add("DEUTSCH", "de_DE", language == "de_DE");
	language_choice->add("ENGLISH", "en_US", language == "en_US");
	language_choice->add("ESPAÑOL", "es_ES", language == "es_ES");
	language_choice->add("FRANÇAIS", "fr_FR", language == "fr_FR");
	language_choice->add("ITALIANO", "it_IT", language == "it_IT");
	language_choice->add("PORTUGUES BRASILEIRO", "pt_BR", language == "pt_BR");
	language_choice->add("PORTUGUES PORTUGAL", "pt_PT", language == "pt_PT");
	language_choice->add("SVENSKA", "sv_SE", language == "sv_SE");
	language_choice->add("TÜRKÇE", "tr_TR", language == "tr_TR");
	language_choice->add("CATALÀ", "ca_ES", language == "ca_ES");
	language_choice->add("ARABIC", "ar_YE", language == "ar_YE");
	language_choice->add("DUTCH", "nl_NL", language == "nl_NL");
	language_choice->add("GREEK", "el_GR", language == "el_GR");
	language_choice->add("KOREAN", "ko_KR", language == "ko_KR");
	language_choice->add("NORWEGIAN", "nn_NO", language == "nn_NO");
	language_choice->add("NORWEGIAN BOKMAL", "nb_NO", language == "nb_NO");
	language_choice->add("POLISH", "pl_PL", language == "pl_PL");
	language_choice->add("JAPANESE", "ja_JP", language == "ja_JP");
	language_choice->add("RUSSIAN", "ru_RU", language == "ru_RU");
	language_choice->add("HUNGARIAN", "hu_HU", language == "hu_HU");

	s->addWithLabel(_("LANGUAGE"), language_choice);



#if !defined(WIN32) || defined(_DEBUG)
	// video device
	auto optionsVideo = std::make_shared<OptionListComponent<std::string> >(mWindow, _("VIDEO OUTPUT"), false);
	std::string currentDevice = SystemConf::getInstance()->get("global.videooutput");
	if (currentDevice.empty()) currentDevice = "auto";

	std::vector<std::string> availableVideo = ApiSystem::getInstance()->getAvailableVideoOutputDevices();

	bool vfound = false;
	for (auto it = availableVideo.begin(); it != availableVideo.end(); it++) {
		optionsVideo->add((*it), (*it), currentDevice == (*it));
		if (currentDevice == (*it)) {
			vfound = true;
		}
	}
	if (vfound == false) {
		optionsVideo->add(currentDevice, currentDevice, true);
	}
	s->addWithLabel(_("VIDEO OUTPUT"), optionsVideo);

	s->addSaveFunc([this, optionsVideo, currentDevice] {
		if (optionsVideo->changed()) {
			SystemConf::getInstance()->set("global.videooutput", optionsVideo->getSelected());
			SystemConf::getInstance()->saveSystemConf();
			mWindow->displayNotificationMessage(_U("\uF011  ") + _("A REBOOT OF THE SYSTEM IS REQUIRED TO APPLY THE NEW CONFIGURATION"));
		}
	});

	// brighness
	int brighness;
	if (ApiSystem::getInstance()->getBrighness(brighness))
	{
		auto brightnessComponent = std::make_shared<SliderComponent>(mWindow, 5.f, 100.f, 5.f, "%");
		brightnessComponent->setValue(brighness);
		brightnessComponent->setOnValueChanged([](const float &newVal) 
		{ 
			ApiSystem::getInstance()->setBrighness((int)Math::round(newVal)); 
		});

		s->addWithLabel(_("BRIGHTNESS"), brightnessComponent);
	}

	// audio device
	auto optionsAudio = std::make_shared<OptionListComponent<std::string> >(mWindow, _("AUDIO OUTPUT"), false);

	std::vector<std::string> availableAudio = ApiSystem::getInstance()->getAvailableAudioOutputDevices();
	std::string selectedAudio = ApiSystem::getInstance()->getCurrentAudioOutputDevice();
	if (selectedAudio.empty())
		selectedAudio = "auto";

	if (SystemConf::getInstance()->get("system.es.menu") != "bartop")
	{
		bool vfound = false;
		for (auto it = availableAudio.begin(); it != availableAudio.end(); it++)
		{
			std::vector<std::string> tokens = Utils::String::split(*it, ' ');

			if (selectedAudio == (*it))
				vfound = true;

			if (tokens.size() >= 2)
			{
				// concatenat the ending words
				std::string vname = "";
				for (unsigned int i = 1; i < tokens.size(); i++)
				{
					if (i > 2) vname += " ";
					vname += tokens.at(i);
				}
				optionsAudio->add(vname, (*it), selectedAudio == (*it));
			}
			else
				optionsAudio->add((*it), (*it), selectedAudio == (*it));
		}

		if (vfound == false)
			optionsAudio->add(selectedAudio, selectedAudio, true);

		s->addWithLabel(_("AUDIO OUTPUT"), optionsAudio);
	}

	s->addSaveFunc([this, optionsAudio, selectedAudio] 
	{
		bool v_need_reboot = false;

		if (optionsAudio->changed()) {
			SystemConf::getInstance()->set("audio.device", optionsAudio->getSelected());
			ApiSystem::getInstance()->setAudioOutputDevice(optionsAudio->getSelected());
			v_need_reboot = true;
		}
		SystemConf::getInstance()->saveSystemConf();
		if (v_need_reboot)
			mWindow->displayNotificationMessage(_U("\uF011  ") + _("A REBOOT OF THE SYSTEM IS REQUIRED TO APPLY THE NEW CONFIGURATION"));
	});
#endif

	// Overclock choice
	auto overclock_choice = std::make_shared<OptionListComponent<std::string> >(window, _("OVERCLOCK"), false);

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

			overclock_choice->add(vname, tokens.at(0), isSet);
		}
	}

	if (isOneSet == false)
		overclock_choice->add(currentOverclock, currentOverclock, true);

#ifndef WIN32
	// overclocking
	s->addWithLabel(_("OVERCLOCK"), overclock_choice);
#endif

	// power saver
	auto power_saver = std::make_shared< OptionListComponent<std::string> >(mWindow, _("POWER SAVER MODES"), false);
	std::vector<std::string> modes;
	modes.push_back("disabled");
	modes.push_back("default");
	modes.push_back("enhanced");
	modes.push_back("instant");
	for (auto it = modes.cbegin(); it != modes.cend(); it++)
		power_saver->add(_(it->c_str()), *it, Settings::getInstance()->getString("PowerSaverMode") == *it);
	s->addWithLabel(_("POWER SAVER MODES"), power_saver);
	s->addSaveFunc([this, power_saver] {
		if (Settings::getInstance()->getString("PowerSaverMode") != "instant" && power_saver->getSelected() == "instant") {
			Settings::getInstance()->setString("TransitionStyle", "instant");
			Settings::getInstance()->setBool("MoveCarousel", false);
			Settings::getInstance()->setBool("EnableSounds", false);
		}
		Settings::getInstance()->setString("PowerSaverMode", power_saver->getSelected());
		PowerSaver::init();
	});

	// Storage device
	std::vector<std::string> availableStorage = ApiSystem::getInstance()->getAvailableStorageDevices();
	std::string selectedStorage = ApiSystem::getInstance()->getCurrentStorage();

	auto optionsStorage = std::make_shared<OptionListComponent<std::string> >(window, _("STORAGE DEVICE"), false);
	for (auto it = availableStorage.begin(); it != availableStorage.end(); it++)
	{
		if ((*it) != "RAM")
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
			}
			else {
				optionsStorage->add((*it), (*it), selectedStorage == (*it));
			}
		}
	}
#if !defined(WIN32) || defined(_DEBUG)
	s->addWithLabel(_("STORAGE DEVICE"), optionsStorage);
#endif

#if !defined(WIN32) || defined(_DEBUG)
	// backup
	s->addEntry(_("BACKUP USER DATA"), true, [this] { mWindow->pushGui(new GuiBackupStart(mWindow)); });
#endif

#ifdef _ENABLE_KODI_
	s->addEntry(_("KODI SETTINGS"), true, [this] {
		GuiSettings *kodiGui = new GuiSettings(mWindow, _("KODI SETTINGS").c_str());
		auto kodiEnabled = std::make_shared<SwitchComponent>(mWindow);
		kodiEnabled->setState(SystemConf::getInstance()->get("kodi.enabled") != "0");
		kodiGui->addWithLabel(_("ENABLE KODI"), kodiEnabled);
		auto kodiAtStart = std::make_shared<SwitchComponent>(mWindow);
		kodiAtStart->setState(
			SystemConf::getInstance()->get("kodi.atstartup") == "1");
		kodiGui->addWithLabel(_("KODI AT START"), kodiAtStart);
		auto kodiX = std::make_shared<SwitchComponent>(mWindow);
		kodiX->setState(SystemConf::getInstance()->get("kodi.xbutton") == "1");
		kodiGui->addWithLabel(_("START KODI WITH X"), kodiX);
		kodiGui->addSaveFunc([kodiEnabled, kodiAtStart, kodiX] {
			SystemConf::getInstance()->set("kodi.enabled",
				kodiEnabled->getState() ? "1" : "0");
			SystemConf::getInstance()->set("kodi.atstartup",
				kodiAtStart->getState() ? "1" : "0");
			SystemConf::getInstance()->set("kodi.xbutton",
				kodiX->getState() ? "1" : "0");
			SystemConf::getInstance()->saveSystemConf();
		});
		mWindow->pushGui(kodiGui);
	});
#endif

#if !defined(WIN32) || defined(_DEBUG)
	// Install
	s->addEntry(_("INSTALL BATOCERA ON A NEW DISK"), true, [this] { mWindow->pushGui(new GuiInstallStart(mWindow)); });

	// Security
	s->addEntry(_("SECURITY"), true, [this] {
		GuiSettings *securityGui = new GuiSettings(mWindow, _("SECURITY").c_str());
		auto securityEnabled = std::make_shared<SwitchComponent>(mWindow);
		securityEnabled->setState(SystemConf::getInstance()->get("system.security.enabled") == "1");
		securityGui->addWithLabel(_("ENFORCE SECURITY"), securityEnabled);

		auto rootpassword = std::make_shared<TextComponent>(mWindow,
			ApiSystem::getInstance()->getRootPassword(),
			ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);
		securityGui->addWithLabel(_("ROOT PASSWORD"), rootpassword);

		securityGui->addSaveFunc([this, securityEnabled] {
			Window* window = this->mWindow;
			bool reboot = false;

			if (securityEnabled->changed()) {
				SystemConf::getInstance()->set("system.security.enabled",
					securityEnabled->getState() ? "1" : "0");
				SystemConf::getInstance()->saveSystemConf();
				reboot = true;
			}

			if (reboot)
				window->displayNotificationMessage(_U("\uF011  ") + _("A REBOOT OF THE SYSTEM IS REQUIRED TO APPLY THE NEW CONFIGURATION"));
		});
		mWindow->pushGui(securityGui);
	});
#endif

	s->addSaveFunc([overclock_choice, window, language_choice, language, optionsStorage, selectedStorage] {
		bool reboot = false;
		if (optionsStorage->changed()) {
			ApiSystem::getInstance()->setStorage(optionsStorage->getSelected());
			reboot = true;
		}

		if (overclock_choice->changed()) {
			Settings::getInstance()->setString("Overclock", overclock_choice->getSelected());
			ApiSystem::getInstance()->setOverclock(overclock_choice->getSelected());
			reboot = true;
		}
		if (language_choice->changed()) {
			FileSorts::reset();
			SystemConf::getInstance()->set("system.language",
				language_choice->getSelected());
			SystemConf::getInstance()->saveSystemConf();
			reboot = true;
		}
		if (reboot)
			window->displayNotificationMessage(_U("\uF011  ") + _("A REBOOT OF THE SYSTEM IS REQUIRED TO APPLY THE NEW CONFIGURATION"));

	});

	// Developer options
	if (isFullUI)
		s->addEntry(_("DEVELOPER"), true, [this] { openDeveloperSettings(); });

	mWindow->pushGui(s);
}

void GuiMenu::openLatencyReductionConfiguration(Window* mWindow, std::string configName)
{
	GuiSettings* guiLatency = new GuiSettings(mWindow, _("LATENCY REDUCTION").c_str());

	auto runaheadValue = SystemConf::getInstance()->get(configName + ".runahead");

	// run-ahead
	auto runahead_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("RUN-AHEAD FRAMES"));
	runahead_enabled->add(_("AUTO"), "auto", runaheadValue < "1");
	runahead_enabled->add("1", "1", runaheadValue == "1");
	runahead_enabled->add("2", "2", runaheadValue == "2");
	runahead_enabled->add("3", "3", runaheadValue == "3");
	runahead_enabled->add("4", "4", runaheadValue == "4");
	runahead_enabled->add("5", "5", runaheadValue == "5");
	runahead_enabled->add("6", "6", runaheadValue == "6");

	if (!runahead_enabled->hasSelection())
		runahead_enabled->selectFirstItem();

	guiLatency->addWithLabel(_("USE RUN-AHEAD FRAMES"), runahead_enabled);

	// second instance
	auto secondinstanceValue = SystemConf::getInstance()->get(configName + ".secondinstance");

	auto secondinstance_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("RUN-AHEAD USE SECOND INSTANCE"));
	secondinstance_enabled->add(_("AUTO"), "auto", secondinstanceValue != "0" && secondinstanceValue != "1");
	secondinstance_enabled->add(_("ON"), "1", secondinstanceValue == "1");
	secondinstance_enabled->add(_("OFF"), "0", secondinstanceValue == "0");

	if (!secondinstance_enabled->hasSelection())
		secondinstance_enabled->selectFirstItem();

	guiLatency->addWithLabel(_("RUN-AHEAD USE SECOND INSTANCE"), secondinstance_enabled);

	guiLatency->addSaveFunc([configName, runahead_enabled, secondinstance_enabled]
	{
		SystemConf::getInstance()->set(configName + ".runahead", runahead_enabled->getSelected());
		SystemConf::getInstance()->set(configName + ".secondinstance", secondinstance_enabled->getSelected());
	});
	
	mWindow->pushGui(guiLatency);
}

void GuiMenu::openNetplaySettings()
{
	GuiSettings* settings = new GuiSettings(mWindow, _("NETPLAY SETTINGS").c_str());

	// Enable
	auto enableNetplay = std::make_shared<SwitchComponent>(mWindow);
	enableNetplay->setState(SystemConf::getInstance()->get("global.netplay") == "1");
	settings->addWithLabel(_("ENABLE NETPLAY"), enableNetplay);

	createInputTextRow(settings, _("NICKNAME"), "global.netplay.nickname", false);
	createInputTextRow(settings, _("PORT"), "global.netplay.port", false);

	// Mitm
	std::string mitm = SystemConf::getInstance()->get("global.netplay.relay");

	auto mitms = std::make_shared<OptionListComponent<std::string> >(mWindow, _("MITM"), false);
	mitms->add(_("NONE"), "none", mitm.empty() || mitm == "none");
	mitms->add("NEW YORK", "nyc", mitm == "nyc");
	mitms->add("MADRID", "madrid", mitm == "madrid");

	if (!mitms->hasSelection())
		mitms->selectFirstItem();

	settings->addWithLabel(_("MITM"), mitms);

	Window* window = mWindow;
	settings->addSaveFunc([enableNetplay, mitms, window] 
	{
		std::string mitm = mitms->getSelected();
		
		SystemConf::getInstance()->set("global.netplay.relay", mitm.empty() ? "" : mitm);		

		if (SystemConf::getInstance()->set("global.netplay", enableNetplay->getState() ? "1" : "0"))
		{
			if (!ThreadedHasher::isRunning() && enableNetplay->getState())
			{
				ThreadedHasher::start(window, false, true);
			}
		}
	});
	
	settings->addSubMenu(_("REINDEX ALL GAMES"), [this] { ThreadedHasher::start(mWindow, true); });
	settings->addSubMenu(_("INDEX MISSING GAMES"), [this] { ThreadedHasher::start(mWindow); });
	
	mWindow->pushGui(settings);
}

void GuiMenu::openGamesSettings_batocera() 
{
	Window* window = mWindow;

	auto s = new GuiSettings(mWindow, _("GAMES SETTINGS").c_str());
	if (SystemConf::getInstance()->get("system.es.menu") != "bartop") {

		// Screen ratio choice
		auto ratio_choice = createRatioOptionList(mWindow, "global");
		s->addWithLabel(_("GAME RATIO"), ratio_choice);
		s->addSaveFunc([ratio_choice] {
			if (ratio_choice->changed()) {
				SystemConf::getInstance()->set("global.ratio", ratio_choice->getSelected());
				SystemConf::getInstance()->saveSystemConf();
			}
		});
	}
	// smoothing
	auto smoothing_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SMOOTH GAMES"));
	smoothing_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.smooth") != "0" && SystemConf::getInstance()->get("global.smooth") != "1");
	smoothing_enabled->add(_("ON"), "1", SystemConf::getInstance()->get("global.smooth") == "1");
	smoothing_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get("global.smooth") == "0");
	s->addWithLabel(_("SMOOTH GAMES"), smoothing_enabled);

	// rewind
	auto rewind_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("REWIND"));
	rewind_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.rewind") != "0" && SystemConf::getInstance()->get("global.rewind") != "1");
	rewind_enabled->add(_("ON"), "1", SystemConf::getInstance()->get("global.rewind") == "1");
	rewind_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get("global.rewind") == "0");
	s->addWithLabel(_("REWIND"), rewind_enabled);

	// autosave/load
	auto autosave_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("AUTO SAVE/LOAD"));
	autosave_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.autosave") != "0" && SystemConf::getInstance()->get("global.autosave") != "1");
	autosave_enabled->add(_("ON"), "1", SystemConf::getInstance()->get("global.autosave") == "1");
	autosave_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get("global.autosave") == "0");
	s->addWithLabel(_("AUTO SAVE/LOAD"), autosave_enabled);

	// Shaders preset
	auto shaders_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SHADERS SET"),
		false);
	std::string currentShader = SystemConf::getInstance()->get("global.shaderset");
	if (currentShader.empty()) {
		currentShader = std::string("auto");
	}

	shaders_choices->add(_("AUTO"), "auto", currentShader == "auto");
	shaders_choices->add(_("NONE"), "none", currentShader == "none");
	shaders_choices->add(_("SCANLINES"), "scanlines", currentShader == "scanlines");
	shaders_choices->add(_("RETRO"), "retro", currentShader == "retro");
	shaders_choices->add(_("ENHANCED"), "enhanced", currentShader == "enhanced"); // batocera 5.23
	shaders_choices->add(_("CURVATURE"), "curvature", currentShader == "curvature"); // batocera 5.24
	shaders_choices->add(_("ZFAST"), "zfast", currentShader == "zfast"); // batocera 5.25
	shaders_choices->add(_("FLATTEN-GLOW"), "flatten-glow", currentShader == "flatten-glow"); // batocera 5.25
	s->addWithLabel(_("SHADERS SET"), shaders_choices);

	// Integer scale
	auto integerscale_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INTEGER SCALE (PIXEL PERFECT)"));
	integerscale_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.integerscale") != "0" && SystemConf::getInstance()->get("global.integerscale") != "1");
	integerscale_enabled->add(_("ON"), "1", SystemConf::getInstance()->get("global.integerscale") == "1");
	integerscale_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get("global.integerscale") == "0");
	s->addWithLabel(_("INTEGER SCALE (PIXEL PERFECT)"), integerscale_enabled);
	s->addSaveFunc([integerscale_enabled] {
		if (integerscale_enabled->changed()) {
			SystemConf::getInstance()->set("global.integerscale", integerscale_enabled->getSelected());
			SystemConf::getInstance()->saveSystemConf();
		}
	});

	// decorations
	{		
		auto sets = GuiMenu::getDecorationsSets(ViewController::get()->getState().getSystem());

		auto decorations = std::make_shared<OptionListComponent<std::string> >(mWindow, _("DECORATION"), false);
		decorations->setRowTemplate([window, sets](std::string data, ComponentListRow& row)
		{
			createDecorationItemTemplate(window, sets, data, row);		
		});
		
		std::vector<std::string> decorations_item;
		decorations_item.push_back(_("AUTO"));
		decorations_item.push_back(_("NONE"));			
		for(auto set : sets)		
			decorations_item.push_back(set.name);		

		for (auto it = decorations_item.begin(); it != decorations_item.end(); it++) 
		{
			decorations->add(*it, *it,
				(SystemConf::getInstance()->get("global.bezel") == *it)
				||
				(SystemConf::getInstance()->get("global.bezel") == "none" && *it == _("NONE"))
				||
				(SystemConf::getInstance()->get("global.bezel") == "" && *it == _("AUTO"))
			);
		}
		s->addWithLabel(_("DECORATION"), decorations);
		s->addSaveFunc([decorations] {
			if (decorations->changed()) {
				SystemConf::getInstance()->set("global.bezel", decorations->getSelected() == _("NONE") ? "none" : decorations->getSelected() == _("AUTO") ? "" : decorations->getSelected());
				SystemConf::getInstance()->saveSystemConf();
			}
		});
	}
	
	// latency reduction
	s->addEntry(_("LATENCY REDUCTION"), true, [this] { openLatencyReductionConfiguration(mWindow, "global"); });

	//AI-enabled translations
	s->addEntry(_("AI GAME TRANSLATION"), true, [this]
	{
		GuiSettings *ai_service = new GuiSettings(mWindow, _("AI GAME TRANSLATION").c_str());

		// AI service enabled?
		auto ai_service_enabled = std::make_shared<SwitchComponent>(mWindow);
		ai_service_enabled->setState(
			SystemConf::getInstance()->get("global.ai_service_enabled") == "1");
		ai_service->addWithLabel(_("ENABLE AI SERVICE"), ai_service_enabled);

		// Target language - order is: popular languages in the Batocera community first
		// then alphabetical order of the 2-char lang code (because the strings are localized)
		auto lang_choices = std::make_shared<OptionListComponent<std::string> >(mWindow,
			_("TARGET LANGUAGE"), false);
		std::string currentLang = SystemConf::getInstance()->get("global.ai_target_lang");
		if (currentLang.empty())
			currentLang = std::string("En");
		lang_choices->add("ENGLISH", "En", currentLang == "En");
		lang_choices->add("FRANÇAIS", "Fr", currentLang == "Fr");
		lang_choices->add("PORTUGUES", "Pt", currentLang == "Pt");
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
		lang_choices->add("RUSSIAN", "Ru", currentLang == "Ru");
		lang_choices->add("SVENSKA", "Sv", currentLang == "Sv");
		lang_choices->add("TÜRKÇE", "Tr", currentLang == "Tr");
		lang_choices->add("简体中文", "Zh", currentLang == "Zh");
		ai_service->addWithLabel(_("TARGET LANGUAGE"), lang_choices);

		// Service  URL
		createInputTextRow(ai_service, _("SERVICE URL"), "global.ai_service_url", false);

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

	if (SystemConf::getInstance()->get("system.es.menu") != "bartop")
	{
		// Retroachievements
		s->addEntry(_("RETROACHIEVEMENTS SETTINGS"), true, [this] 
		{
			GuiSettings *retroachievements = new GuiSettings(mWindow, _("RETROACHIEVEMENTS SETTINGS").c_str());

			// retroachievements_enable
			auto retroachievements_enabled = std::make_shared<SwitchComponent>(mWindow);
			retroachievements_enabled->setState(
				SystemConf::getInstance()->get("global.retroachievements") == "1");
			retroachievements->addWithLabel(_("RETROACHIEVEMENTS"), retroachievements_enabled);

			// retroachievements_hardcore_mode
			auto retroachievements_hardcore_enabled = std::make_shared<SwitchComponent>(mWindow);
			retroachievements_hardcore_enabled->setState(
				SystemConf::getInstance()->get("global.retroachievements.hardcore") == "1");
			retroachievements->addWithLabel(_("HARDCORE MODE"), retroachievements_hardcore_enabled);

			// retroachievements_leaderboards
			auto retroachievements_leaderboards_enabled = std::make_shared<SwitchComponent>(mWindow);
			retroachievements_leaderboards_enabled->setState(
				SystemConf::getInstance()->get("global.retroachievements.leaderboards") == "1");
			retroachievements->addWithLabel(_("LEADERBOARDS"), retroachievements_leaderboards_enabled);

			// retroachievements_verbose_mode
			auto retroachievements_verbose_enabled = std::make_shared<SwitchComponent>(mWindow);
			retroachievements_verbose_enabled->setState(
				SystemConf::getInstance()->get("global.retroachievements.verbose") == "1");
			retroachievements->addWithLabel(_("VERBOSE MODE"), retroachievements_verbose_enabled);

			// retroachievements_automatic_screenshot
			auto retroachievements_screenshot_enabled = std::make_shared<SwitchComponent>(mWindow);
			retroachievements_screenshot_enabled->setState(
				SystemConf::getInstance()->get("global.retroachievements.screenshot") == "1");
			retroachievements->addWithLabel(_("AUTOMATIC SCREENSHOT"), retroachievements_screenshot_enabled);

			// retroachievements, username, password
			createInputTextRow(retroachievements, _("USERNAME"), "global.retroachievements.username",
				false);
			createInputTextRow(retroachievements, _("PASSWORD"), "global.retroachievements.password",
				true);

			retroachievements->addSaveFunc([retroachievements_enabled, retroachievements_hardcore_enabled, retroachievements_leaderboards_enabled,
				retroachievements_verbose_enabled, retroachievements_screenshot_enabled] {
				SystemConf::getInstance()->set("global.retroachievements",
					retroachievements_enabled->getState() ? "1" : "0");
				SystemConf::getInstance()->set("global.retroachievements.hardcore",
					retroachievements_hardcore_enabled->getState() ? "1" : "0");
				SystemConf::getInstance()->set("global.retroachievements.leaderboards",
					retroachievements_leaderboards_enabled->getState() ? "1" : "0");
				SystemConf::getInstance()->set("global.retroachievements.verbose",
					retroachievements_verbose_enabled->getState() ? "1" : "0");
				SystemConf::getInstance()->set("global.retroachievements.screenshot",
					retroachievements_screenshot_enabled->getState() ? "1" : "0");
				SystemConf::getInstance()->saveSystemConf();
			});
			mWindow->pushGui(retroachievements);
		});		

		if (SystemData::isNetplayActivated())
			s->addEntry(_("NETPLAY SETTINGS"), true, [this] { openNetplaySettings(); }, "iconNetplay");

		// Custom config for systems
		s->addEntry(_("PER SYSTEM ADVANCED CONFIGURATION"), true, [this, s, window]
		{
			s->save();
			GuiSettings* configuration = new GuiSettings(window, _("PER SYSTEM ADVANCED CONFIGURATION").c_str());

			// For each activated system
			std::vector<SystemData *> systems = SystemData::sSystemVector;
			for (auto system = systems.begin(); system != systems.end(); system++)
			{
				if ((*system)->isCollection() || (*system)->isGroupSystem())
					continue;

				SystemData *systemData = (*system);
				configuration->addEntry((*system)->getFullName(), true, [this, systemData, window] {
					popSystemConfigurationGui(window, systemData, "");
				});
			}

			window->pushGui(configuration);
		});


		// Bios
		s->addEntry(_("MISSING BIOS"), true, [this, s]
		{
			GuiSettings *configuration = new GuiSettings(mWindow, _("MISSING BIOS").c_str());
			std::vector<BiosSystem> biosInformations = ApiSystem::getInstance()->getBiosInformations();

			if (biosInformations.size() == 0) {
				configuration->addEntry(_("NO MISSING BIOS"));
			}
			else 
			{
				for (auto systemBios = biosInformations.begin(); systemBios != biosInformations.end(); systemBios++) 
				{
					BiosSystem systemBiosData = (*systemBios);
					configuration->addEntry((*systemBios).name.c_str(), true, [this, systemBiosData]
					{
						GuiSettings* configurationInfo = new GuiSettings(mWindow, systemBiosData.name.c_str());
						for (auto biosFile = systemBiosData.bios.begin(); biosFile != systemBiosData.bios.end(); biosFile++)
						{
							auto theme = ThemeData::getMenuTheme();

							auto biosPath = std::make_shared<TextComponent>(mWindow, biosFile->path.c_str(),
								theme->Text.font,
								theme->TextSmall.color); // 0x000000FF -> Avoid black on themes with black backgrounds
							auto biosMd5 = std::make_shared<TextComponent>(mWindow, biosFile->md5.c_str(),
								theme->TextSmall.font,
								theme->TextSmall.color);
							auto biosStatus = std::make_shared<TextComponent>(mWindow, biosFile->status.c_str(),
								theme->TextSmall.font,
								theme->TextSmall.color);

							ComponentListRow biosFileRow;
							biosFileRow.addElement(biosPath, true);
							configurationInfo->addRow(biosFileRow);

							configurationInfo->addWithLabel("   MD5", biosMd5);
							configurationInfo->addWithLabel("   " + _("STATUS"), biosStatus);
						}
						mWindow->pushGui(configurationInfo);
					});
				}
			}
			mWindow->pushGui(configuration);
		});		

		// Game List Update
		s->addEntry(_("UPDATE GAMES LISTS"), false, [this, window] 
		{
			if (ThreadedScraper::isRunning())
			{
				window->pushGui(new GuiMsgBox(mWindow, _("SCRAPING IS RUNNING. DO YOU WANT TO STOP IT ?"), _("YES"), [this, window]
				{
					ThreadedScraper::stop();
				}, _("NO"), nullptr));

				return;
			}

			if (ThreadedHasher::isRunning())
			{
				window->pushGui(new GuiMsgBox(mWindow, _("GAME HASHING IS RUNNING. DO YOU WANT TO STOP IT ?"), _("YES"), [this, window]
				{
					ThreadedScraper::stop();
				}, _("NO"), nullptr));

				return;
			}			

			window->pushGui(new GuiMsgBox(window, _("REALLY UPDATE GAMES LISTS ?"), _("YES"),
				[this, window] 
			{
				window->renderLoadingScreen(_("Loading..."));

				ViewController::get()->goToStart();
				delete ViewController::get();
				ViewController::init(window);
				CollectionSystemManager::deinit();
				CollectionSystemManager::init(window);
				SystemData::loadConfig(window);

				GuiComponent *gui;
				while ((gui = window->peekGui()) != NULL) {
					window->removeGui(gui);
					delete gui;
				}
				ViewController::get()->reloadAll(nullptr, false); // Avoid reloading themes a second time
				window->endRenderLoadingScreen();

				window->pushGui(ViewController::get());
			}, _("NO"), nullptr));
		});
	}
	s->addSaveFunc([smoothing_enabled, rewind_enabled, shaders_choices, autosave_enabled] 
	{
		SystemConf::getInstance()->set("global.smooth", smoothing_enabled->getSelected());
		SystemConf::getInstance()->set("global.rewind", rewind_enabled->getSelected());
		SystemConf::getInstance()->set("global.shaderset", shaders_choices->getSelected());
		SystemConf::getInstance()->set("global.autosave", autosave_enabled->getSelected());
		SystemConf::getInstance()->saveSystemConf();
	});

	mWindow->pushGui(s);
}

void GuiMenu::openControllersSettings_batocera()
{

	GuiSettings *s = new GuiSettings(mWindow, _("CONTROLLERS SETTINGS").c_str());

	Window *window = mWindow;

	// CONFIGURE A CONTROLLER
	s->addEntry(_("CONFIGURE A CONTROLLER"), false, [window, this, s]
	{
		window->pushGui(new GuiMsgBox(window,
			_("YOU ARE GOING TO CONFIGURE A CONTROLLER. IF YOU HAVE ONLY ONE JOYSTICK, "
				"CONFIGURE THE DIRECTIONS KEYS AND SKIP JOYSTICK CONFIG BY HOLDING A BUTTON. "
				"IF YOU DO NOT HAVE A SPECIAL KEY FOR HOTKEY, CHOOSE THE SELECT BUTTON. SKIP "
				"ALL BUTTONS YOU DO NOT HAVE BY HOLDING A KEY. BUTTONS NAMES ARE BASED ON THE "
				"SNES CONTROLLER."), _("OK"),
			[window, this, s] {
			window->pushGui(new GuiDetectDevice(window, false, [this, s] {
				s->setSave(false);
				delete s;
				this->openControllersSettings_batocera();
			}));
		}));
	});

	// PAIR A BLUETOOTH CONTROLLER
	s->addEntry(_("PAIR A BLUETOOTH CONTROLLER"), false, [window] { ThreadedBluetooth::start(window); });

	// FORGET BLUETOOTH CONTROLLERS
	s->addEntry(_("FORGET BLUETOOTH CONTROLLERS"), false, [window, this, s] {
		ApiSystem::getInstance()->forgetBluetoothControllers();
		window->pushGui(new GuiMsgBox(window,
			_("CONTROLLERS LINKS HAVE BEEN DELETED."), _("OK")));
	});
	ComponentListRow row;

	// Here we go; for each player
	std::list<int> alreadyTaken = std::list<int>();

	// clear the current loaded inputs
	clearLoadedInput();

	std::vector<std::shared_ptr<OptionListComponent<StrInputConfig *>>> options;
	char strbuf[256];

	for (int player = 0; player < MAX_PLAYERS; player++) {
		std::stringstream sstm;
		sstm << "INPUT P" << player + 1;
		std::string confName = sstm.str() + "NAME";
		std::string confGuid = sstm.str() + "GUID";
		snprintf(strbuf, 256, _("INPUT P%i").c_str(), player + 1);

		LOG(LogInfo) << player + 1 << " " << confName << " " << confGuid;
		auto inputOptionList = std::make_shared<OptionListComponent<StrInputConfig *> >(mWindow, strbuf, false);
		options.push_back(inputOptionList);

		// Checking if a setting has been saved, else setting to default
		std::string configuratedName = Settings::getInstance()->getString(confName);
		std::string configuratedGuid = Settings::getInstance()->getString(confGuid);
		bool found = false;
		// For each available and configured input
		for (auto iter = InputManager::getInstance()->getJoysticks().begin(); iter != InputManager::getInstance()->getJoysticks().end(); iter++) {
			InputConfig* config = InputManager::getInstance()->getInputConfigByDevice(iter->first);
			if (config != NULL && config->isConfigured()) {
				// create name
				std::stringstream dispNameSS;
				dispNameSS << "#" << config->getDeviceId() << " ";
				std::string deviceName = config->getDeviceName();
				if (deviceName.size() > 25) {
					dispNameSS << deviceName.substr(0, 16) << "..." <<
						deviceName.substr(deviceName.size() - 5, deviceName.size() - 1);
				}
				else {
					dispNameSS << deviceName;
				}

				std::string displayName = dispNameSS.str();


				bool foundFromConfig = configuratedName == config->getDeviceName() &&
					configuratedGuid == config->getDeviceGUIDString();
				int deviceID = config->getDeviceId();
				// Si la manette est configurée, qu'elle correspond a la configuration, et qu'elle n'est pas
				// deja selectionnée on l'ajoute en séléctionnée
				StrInputConfig* newInputConfig = new StrInputConfig(config->getDeviceName(), config->getDeviceGUIDString());
				mLoadedInput.push_back(newInputConfig);

				if (foundFromConfig
					&& std::find(alreadyTaken.begin(), alreadyTaken.end(), deviceID) == alreadyTaken.end()
					&& !found) {
					found = true;
					alreadyTaken.push_back(deviceID);
					LOG(LogWarning) << "adding entry for player" << player << " (selected): " <<
						config->getDeviceName() << "  " << config->getDeviceGUIDString();
					inputOptionList->add(displayName, newInputConfig, true);
				}
				else {
					LOG(LogWarning) << "adding entry for player" << player << " (not selected): " <<
						config->getDeviceName() << "  " << config->getDeviceGUIDString();
					inputOptionList->add(displayName, newInputConfig, false);
				}
			}
		}
		if (configuratedName.compare("") == 0 || !found) {
			LOG(LogWarning) << "adding default entry for player " << player << "(selected : true)";
			inputOptionList->add(_("default"), NULL, true);
		}
		else {
			LOG(LogWarning) << "adding default entry for player" << player << "(selected : false)";
			inputOptionList->add(_("default"), NULL, false);
		}

		// ADD default config

		// Populate controllers list
		s->addWithLabel(strbuf, inputOptionList);
	}
	s->addSaveFunc([this, options, window] 
	{
		for (int player = 0; player < MAX_PLAYERS; player++) 
		{
			std::stringstream sstm;
			sstm << "INPUT P" << player + 1;
			std::string confName = sstm.str() + "NAME";
			std::string confGuid = sstm.str() + "GUID";

			auto input = options.at(player);

			StrInputConfig* selected = input->getSelected();
			if (selected == nullptr)
			{
				Settings::getInstance()->setString(confName, "DEFAULT");
				Settings::getInstance()->setString(confGuid, "");
			}
			else if (input->changed())
			{
				LOG(LogWarning) << "Found the selected controller ! : name in list  = " << input->getSelectedName();
				LOG(LogWarning) << "Found the selected controller ! : guid  = " << selected->deviceGUIDString;

				Settings::getInstance()->setString(confName, selected->deviceName);
				Settings::getInstance()->setString(confGuid, selected->deviceGUIDString);
			}			
		}

		Settings::getInstance()->saveFile();
		// this is dependant of this configuration, thus update it
		InputManager::getInstance()->computeLastKnownPlayersDeviceIndexes();
	});

	// CONTROLLER ACTIVITY
	auto activity = std::make_shared<SwitchComponent>(mWindow);
	activity->setState(Settings::getInstance()->getBool("ShowControllerActivity"));
	s->addWithLabel(_("SHOW CONTROLLER ACTIVITY"), activity);
	s->addSaveFunc([activity] { Settings::getInstance()->setBool("ShowControllerActivity", activity->getState()); });

	row.elements.clear();
	window->pushGui(s);
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
		mWindow->pushGui(new GuiMsgBox(mWindow, _("YOU MUST APPLY THE THEME BEFORE EDIT CONFIGURATION"), _("OK")));
		return;
	}

	Window* window = mWindow;

	auto system = ViewController::get()->getState().getSystem();
	auto theme = system->getTheme();

	auto themeconfig = new GuiSettings(mWindow, (systemTheme.empty() ? _("THEME CONFIGURATION") : _("VIEW CUSTOMISATION")).c_str());

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

	if (systemTheme.empty())
	{
		gamelist_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("GAMELIST VIEW STYLE"), false);

		std::vector<std::pair<std::string, std::string>> styles;
		styles.push_back(std::pair<std::string, std::string>("automatic", _("automatic")));

		if (system != NULL)
		{
			auto mViews = theme->getViewsOfTheme();
			for (auto it = mViews.cbegin(); it != mViews.cend(); ++it)
			{
				if (it->first == "basic" || it->first == "detailed" || it->first == "grid")
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

	for (std::string subset : theme->getSubSetNames(viewName))
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

			std::shared_ptr<OptionListComponent<std::string>> item = std::make_shared<OptionListComponent<std::string> >(mWindow, _(("THEME " + Utils::String::toUpper(subset)).c_str()), false);
			item->setTag(!perSystemSettingName.empty() ? perSystemSettingName : settingName);

			for (auto it = themeColorSets.begin(); it != themeColorSets.end(); it++)
			{
				std::string displayName = it->displayName;

				if (!systemTheme.empty())
				{
					std::string defaultValue = Settings::getInstance()->getString(settingName);
					if (defaultValue.empty())
						defaultValue = system->getTheme()->getDefaultSubSetValue(subset);

					if (it->name == defaultValue)
						displayName = displayName + " (" + _("DEFAULT") + ")";
				}

				item->add(displayName, it->name, it == selectedColorSet);
			}

			if (selectedColorSet == themeColorSets.end())
				item->selectFirstItem();

			if (!themeColorSets.empty())
			{
				std::string displayName = themeColorSets.cbegin()->subSetDisplayName;
				if (!displayName.empty())
				{
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

						if (!prefix.empty())
							prefix = " (" + prefix + ")";

					}

					themeconfig->addWithLabel(displayName + prefix, item);
				}
				else
					themeconfig->addWithLabel(_(("THEME " + Utils::String::toUpper(subset)).c_str()), item);
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
	
	if (systemTheme.empty())
	{
		themeconfig->addEntry(_("RESET GAMELIST CUSTOMISATIONS"), false, [s, themeconfig, window]
		{
			Settings::getInstance()->setString("GamelistViewStyle", "");
			Settings::getInstance()->setString("DefaultGridSize", "");

			for (auto sysIt = SystemData::sSystemVector.cbegin(); sysIt != SystemData::sSystemVector.cend(); sysIt++)
				(*sysIt)->setSystemViewMode("automatic", Vector2f(0, 0));

			themeconfig->setVariable("reloadAll", true);
			themeconfig->close();
		});
	}

	//  theme_colorset, theme_iconset, theme_menu, theme_systemview, theme_gamelistview, theme_region,
	themeconfig->addSaveFunc([systemTheme, system, themeconfig, theme_set, options, gamelist_style, mGridSize, window]
	{
		bool reloadAll = systemTheme.empty() ? Settings::getInstance()->setString("ThemeSet", theme_set == nullptr ? "" : theme_set->getSelected()) : false;

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

		if (reloadAll || themeconfig->getVariable("reloadAll"))
		{
			if (systemTheme.empty())
			{
				CollectionSystemManager::get()->updateSystemsList();
				ViewController::get()->reloadAll(window);
				window->endRenderLoadingScreen();

				if (theme_set != nullptr)
				{
					std::string oldTheme = Settings::getInstance()->getString("ThemeSet");
					Scripting::fireEvent("theme-changed", theme_set->getSelected(), oldTheme);
				}
			}
			else
			{
				system->loadTheme();
				system->resetFilters();
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

	auto s = new GuiSettings(mWindow, _("UI SETTINGS").c_str());

	// theme set
	auto theme = ThemeData::getMenuTheme();
	auto themeSets = ThemeData::getThemeSets();
	auto system = ViewController::get()->getState().getSystem();

	if (system != nullptr && !themeSets.empty())
	{
		auto selectedSet = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
		if (selectedSet == themeSets.end())
			selectedSet = themeSets.begin();

		auto theme_set = std::make_shared<OptionListComponent<std::string> >(mWindow, _("THEME SET"), false);
		for (auto it = themeSets.begin(); it != themeSets.end(); it++)
			theme_set->add(it->first, it->first, it == selectedSet);

		s->addWithLabel(_("THEME SET"), theme_set);
		s->addSaveFunc([s, theme_set, pthis, window]
		{
			std::string oldTheme = Settings::getInstance()->getString("ThemeSet");
			if (oldTheme != theme_set->getSelected())
			{
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

	//UI mode
	auto UImodeSelection = std::make_shared< OptionListComponent<std::string> >(mWindow, _("UI MODE"), false);
	std::vector<std::string> UImodes = UIModeController::getInstance()->getUIModes();
	for (auto it = UImodes.cbegin(); it != UImodes.cend(); it++)
		UImodeSelection->add(_(it->c_str()), *it, Settings::getInstance()->getString("UIMode") == *it);
	s->addWithLabel(_("UI MODE"), UImodeSelection);
	s->addSaveFunc([UImodeSelection, window]
	{
		std::string selectedMode = UImodeSelection->getSelected();
		if (selectedMode != "Full")
		{
			std::string msg = _("You are changing the UI to a restricted mode:\nThis will hide most menu-options to prevent changes to the system.\nTo unlock and return to the full UI, enter this code:") + "\n";
			msg += "\"" + UIModeController::getInstance()->getFormattedPassKeyStr() + "\"\n\n";
			msg += _("Do you want to proceed ?");
			window->pushGui(new GuiMsgBox(window, msg,
				_("YES"), [selectedMode] {
				LOG(LogDebug) << "Setting UI mode to " << selectedMode;
				Settings::getInstance()->setString("UIMode", selectedMode);
				Settings::getInstance()->saveFile();
			}, _("NO"), nullptr));
		}
	});
	
	// Optionally start in selected system
	auto systemfocus_list = std::make_shared< OptionListComponent<std::string> >(mWindow, _("START ON SYSTEM"), false);
	systemfocus_list->add(_("NONE"), "", Settings::getInstance()->getString("StartupSystem") == "");

	for (auto it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); it++)
		if ("retropie" != (*it)->getName() && (*it)->isVisible())
			systemfocus_list->add((*it)->getName(), (*it)->getName(), Settings::getInstance()->getString("StartupSystem") == (*it)->getName());

	s->addWithLabel(_("START ON SYSTEM"), systemfocus_list);
	s->addSaveFunc([systemfocus_list] {
		Settings::getInstance()->setString("StartupSystem", systemfocus_list->getSelected());
	});

	// Open gamelist at start
	auto startOnGamelist = std::make_shared<SwitchComponent>(mWindow);
	startOnGamelist->setState(Settings::getInstance()->getBool("StartupOnGameList"));
	s->addWithLabel(_("START ON GAMELIST"), startOnGamelist);
	s->addSaveFunc([startOnGamelist] { Settings::getInstance()->setBool("StartupOnGameList", startOnGamelist->getState()); });

	// Batocera: select systems to hide
	auto displayedSystems = std::make_shared<OptionListComponent<SystemData*>>(mWindow, _("SYSTEMS DISPLAYED"), true);
	for (auto it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); it++)
	{
		if ((*it)->isGroupChildSystem())
			continue;

		displayedSystems->add((*it)->getFullName(), *it, (SystemConf::getInstance()->get((*it)->getName() + ".hide") != "1"));
	}

	s->addWithLabel(_("SYSTEMS DISPLAYED"), displayedSystems);
	s->addSaveFunc([s, displayedSystems]
	{
		std::vector<SystemData*> sys = displayedSystems->getSelectedObjects();
		for (auto it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); it++)
		{
			if ((*it)->isGroupChildSystem())
				continue;

			std::string value_cfg_hidden = "1";
			for (auto selected = sys.cbegin(); selected != sys.cend(); selected++)
				if ((*it)->getName() == (*selected)->getName())
					value_cfg_hidden = "0";
			
			if (SystemConf::getInstance()->set((*it)->getName() + ".hide", value_cfg_hidden))
				s->setVariable("reloadAll", true);
		}
	});

	// transition style
	auto transition_style = std::make_shared<OptionListComponent<std::string> >(mWindow,
		_("TRANSITION STYLE"),
		false);

	std::vector<std::string> transitions;
	transitions.push_back("auto");
	transitions.push_back("fade");
	transitions.push_back("slide");
	transitions.push_back("instant");

	for (auto it = transitions.begin(); it != transitions.end(); it++)
		transition_style->add(_(it->c_str()), *it, Settings::getInstance()->getString("TransitionStyle") == *it);

	if (!transition_style->hasSelection())
		transition_style->selectFirstItem();

	s->addWithLabel(_("TRANSITION STYLE"), transition_style);
	s->addSaveFunc([transition_style] 
	{
		Settings::getInstance()->setString("TransitionStyle", transition_style->getSelected());		
	});

	s->addEntry(_("SCREENSAVER SETTINGS"), true, std::bind(&GuiMenu::openScreensaverOptions, this));

	// clock
	auto clock = std::make_shared<SwitchComponent>(mWindow);
	clock->setState(Settings::getInstance()->getBool("DrawClock"));
	s->addWithLabel(_("SHOW CLOCK"), clock);
	s->addSaveFunc(
		[clock] { Settings::getInstance()->setBool("DrawClock", clock->getState()); });

	// show help
	auto show_help = std::make_shared<SwitchComponent>(mWindow);
	show_help->setState(Settings::getInstance()->getBool("ShowHelpPrompts"));
	s->addWithLabel(_("ON-SCREEN HELP"), show_help);
	s->addSaveFunc([s, show_help] 
	{
		if (Settings::getInstance()->setBool("ShowHelpPrompts", show_help->getState()))
			s->setVariable("reloadAll", true);
	});

	// quick system select (left/right in game list view)
	auto quick_sys_select = std::make_shared<SwitchComponent>(mWindow);
	quick_sys_select->setState(Settings::getInstance()->getBool("QuickSystemSelect"));
	s->addWithLabel(_("QUICK SYSTEM SELECT"), quick_sys_select);
	s->addSaveFunc([quick_sys_select] {
		Settings::getInstance()->setBool("QuickSystemSelect", quick_sys_select->getState());
	});

	// Enable OSK (On-Screen-Keyboard)
	auto osk_enable = std::make_shared<SwitchComponent>(mWindow);
	osk_enable->setState(Settings::getInstance()->getBool("UseOSK"));
	s->addWithLabel(_("ON SCREEN KEYBOARD"), osk_enable);
	s->addSaveFunc([osk_enable] {
		Settings::getInstance()->setBool("UseOSK", osk_enable->getState()); });

	// carousel transition option
	auto move_carousel = std::make_shared<SwitchComponent>(mWindow);
	move_carousel->setState(Settings::getInstance()->getBool("MoveCarousel"));
	s->addWithLabel(_("CAROUSEL TRANSITIONS"), move_carousel);
	s->addSaveFunc([move_carousel] {
		if (move_carousel->getState()
			&& !Settings::getInstance()->getBool("MoveCarousel")
			&& PowerSaver::getMode() == PowerSaver::INSTANT)
		{
			Settings::getInstance()->setString("PowerSaverMode", "default");
			PowerSaver::init();
		}
		Settings::getInstance()->setBool("MoveCarousel", move_carousel->getState());
	});

	// filenames
	auto hidden_files = std::make_shared<SwitchComponent>(mWindow);
	hidden_files->setState(Settings::getInstance()->getBool("ShowFilenames"));
	s->addWithLabel(_("SHOW FILENAMES IN LISTS"), hidden_files);
	s->addSaveFunc([hidden_files, s] 
	{ 
		if (Settings::getInstance()->setBool("ShowFilenames", hidden_files->getState()))
		{
			FileData::resetSettings();
			s->setVariable("reloadCollections", true);
			s->setVariable("reloadAll", true);
		}
	});
	


	s->onFinalize([s, pthis, window]
	{
		if (s->getVariable("reloadCollections"))
			CollectionSystemManager::get()->updateSystemsList();

		if (s->getVariable("reloadAll"))
		{
			ViewController::get()->reloadAll(window);
			window->endRenderLoadingScreen();
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

	// volume
	auto volume = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
	volume->setValue((float)VolumeControl::getInstance()->getVolume());
	volume->setOnValueChanged([](const float &newVal) { VolumeControl::getInstance()->setVolume((int)Math::round(newVal)); });
	s->addWithLabel(_("SYSTEM VOLUME"), volume);
	s->addSaveFunc([this, volume] 
	{
		VolumeControl::getInstance()->setVolume((int)Math::round(volume->getValue()));
		SystemConf::getInstance()->set("audio.volume", std::to_string((int)round(volume->getValue())));
	});

	auto volumePopup = std::make_shared<SwitchComponent>(mWindow);
	volumePopup->setState(Settings::getInstance()->getBool("VolumePopup"));
	s->addWithLabel(_("SHOW OVERLAY WHEN VOLUME CHANGES"), volumePopup);
	s->addSaveFunc([volumePopup] { Settings::getInstance()->setBool("VolumePopup", volumePopup->getState()); });

	// disable sounds
	auto music_enabled = std::make_shared<SwitchComponent>(mWindow);
	music_enabled->setState(Settings::getInstance()->getBool("audio.bgmusic"));
	s->addWithLabel(_("FRONTEND MUSIC"), music_enabled);
	s->addSaveFunc([music_enabled] 
	{
		if (Settings::getInstance()->setBool("audio.bgmusic", music_enabled->getState()))
		{
			if (music_enabled->getState())
				AudioManager::getInstance()->playRandomMusic();
			else
				AudioManager::getInstance()->stopMusic();
		}
	});

	// batocera - display music titles
	auto display_titles = std::make_shared<SwitchComponent>(mWindow);
	display_titles->setState(Settings::getInstance()->getBool("audio.display_titles"));
	s->addWithLabel(_("DISPLAY SONG TITLES"), display_titles);
	s->addSaveFunc([display_titles] {
		Settings::getInstance()->setBool("audio.display_titles", display_titles->getState());
	});

	// batocera - how long to display the song titles?
	auto titles_time = std::make_shared<SliderComponent>(mWindow, 2.f, 120.f, 2.f, "s");
	titles_time->setValue(Settings::getInstance()->getInt("audio.display_titles_time"));
	s->addWithLabel(_("HOW MANY SECONDS FOR SONG TITLES"), titles_time);
	s->addSaveFunc([titles_time] {
		Settings::getInstance()->setInt("audio.display_titles_time", (int)Math::round(titles_time->getValue()));
	});

	// batocera - music per system
	auto music_per_system = std::make_shared<SwitchComponent>(mWindow);
	music_per_system->setState(Settings::getInstance()->getBool("audio.persystem"));
	s->addWithLabel(_("ONLY PLAY SYSTEM-SPECIFIC MUSIC FOLDER"), music_per_system);
	s->addSaveFunc([music_per_system] {
		if (Settings::getInstance()->setBool("audio.persystem", music_per_system->getState()))
			AudioManager::getInstance()->changePlaylist(ViewController::get()->getState().getSystem()->getTheme(), true);
	});

	// batocera - music per system
	auto enableThemeMusics = std::make_shared<SwitchComponent>(mWindow);
	enableThemeMusics->setState(Settings::getInstance()->getBool("audio.thememusics"));
	s->addWithLabel(_("PLAY THEME MUSICS"), enableThemeMusics);
	s->addSaveFunc([enableThemeMusics] {
		if (Settings::getInstance()->setBool("audio.thememusics", enableThemeMusics->getState()))
			AudioManager::getInstance()->changePlaylist(ViewController::get()->getState().getSystem()->getTheme(), true);
	});

	// disable sounds
	auto sounds_enabled = std::make_shared<SwitchComponent>(mWindow);
	sounds_enabled->setState(Settings::getInstance()->getBool("EnableSounds"));
	s->addWithLabel(_("ENABLE NAVIGATION SOUNDS"), sounds_enabled);
	s->addSaveFunc([sounds_enabled] {
	    if (sounds_enabled->getState()
		  && !Settings::getInstance()->getBool("EnableSounds")
		  && PowerSaver::getMode() == PowerSaver::INSTANT)
		{
		  Settings::getInstance()->setString("PowerSaverMode", "default");
		  PowerSaver::init();
		}
	    Settings::getInstance()->setBool("EnableSounds", sounds_enabled->getState());
	  });

	auto video_audio = std::make_shared<SwitchComponent>(mWindow);
	video_audio->setState(Settings::getInstance()->getBool("VideoAudio"));
	s->addWithLabel(_("ENABLE VIDEO AUDIO"), video_audio);
	s->addSaveFunc([video_audio] { Settings::getInstance()->setBool("VideoAudio", video_audio->getState()); });

	mWindow->pushGui(s);
}

void GuiMenu::openWifiSettings(Window* win, std::string title, std::string data, const std::function<void(std::string)>& onsave)
{
	win->pushGui(new GuiWifi(win, title, data, onsave));
}

void GuiMenu::openNetworkSettings_batocera()
{
	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	Window *window = mWindow;

	auto s = new GuiSettings(mWindow, _("NETWORK SETTINGS").c_str());
	auto status = std::make_shared<TextComponent>(mWindow,
		ApiSystem::getInstance()->ping() ? _("CONNECTED")
		: _("NOT CONNECTED"),
		font, color);
	s->addWithLabel(_("STATUS"), status);
	auto ip = std::make_shared<TextComponent>(mWindow, ApiSystem::getInstance()->getIpAdress(),
		font, color);
	s->addWithLabel(_("IP ADDRESS"), ip);
	// Hostname
	createInputTextRow(s, _("HOSTNAME"), "system.hostname", false);

	// Wifi enable
	auto enable_wifi = std::make_shared<SwitchComponent>(mWindow);
	bool baseEnabled = SystemConf::getInstance()->get("wifi.enabled") == "1";
	enable_wifi->setState(baseEnabled);
	s->addWithLabel(_("ENABLE WIFI"), enable_wifi);

	// window, title, settingstring,
	const std::string baseSSID = SystemConf::getInstance()->get("wifi.ssid");
	createInputTextRow(s, _("WIFI SSID"), "wifi.ssid", false, false, &openWifiSettings);

	const std::string baseKEY = SystemConf::getInstance()->get("wifi.key");
	createInputTextRow(s, _("WIFI KEY"), "wifi.key", true);

	s->addSaveFunc([baseEnabled, baseSSID, baseKEY, enable_wifi, window] {
		bool wifienabled = enable_wifi->getState();
		SystemConf::getInstance()->set("wifi.enabled", wifienabled ? "1" : "0");
		std::string newSSID = SystemConf::getInstance()->get("wifi.ssid");
		std::string newKey = SystemConf::getInstance()->get("wifi.key");
		SystemConf::getInstance()->saveSystemConf();
		if (wifienabled) {
			if (baseSSID != newSSID
				|| baseKEY != newKey
				|| !baseEnabled) {
				if (ApiSystem::getInstance()->enableWifi(newSSID, newKey)) {
					window->pushGui(
						new GuiMsgBox(window, _("WIFI ENABLED"))
					);
				}
				else {
					window->pushGui(
						new GuiMsgBox(window, _("WIFI CONFIGURATION ERROR"))
					);
				}
			}
		}
		else if (baseEnabled) {
			ApiSystem::getInstance()->disableWifi();
		}
	});

	mWindow->pushGui(s);
}

void GuiMenu::openScraperSettings_batocera() 
{
	auto s = new GuiSettings(mWindow, _("SCRAPE").c_str());

	s->addEntry(_("AUTOMATIC SCRAPER"), true, [this] {
		Window* window = mWindow;
		window->pushGui(new GuiMsgBox(window, _("REALLY SCRAPE?"), _("YES"),
			[window] {
			window->pushGui(new GuiAutoScrape(window));
		}, _("NO"), nullptr));
	});

	s->addEntry(_("MANUAL SCRAPER"), true, [this] { openScraperSettings(); });

	mWindow->pushGui(s);
}

void GuiMenu::openQuitMenu_batocera()
{
  GuiMenu::openQuitMenu_batocera_static(mWindow);
}

void GuiMenu::openQuitMenu_batocera_static(Window *window, bool forceWin32Menu)
{	
#ifdef WIN32
	if (!forceWin32Menu)
	{
		Scripting::fireEvent("quit");
		quitES("");
		return;
	}
#endif

	auto s = new GuiSettings(window, _("QUIT").c_str());
	
	if (forceWin32Menu)
	{
		s->setCloseButton("select");

		s->addEntry(_("LAUNCH SCREENSAVER"), false, [s, window] 
		{
			window->postToUiThread([](Window* w)
			{
				w->startScreenSaver();
				w->renderScreenSaver();
			});
			delete s;

		}, "iconScraper", true);
	}

	s->addEntry(_("RESTART SYSTEM"), false, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY RESTART?"), _("YES"),
			[] {
			if (ApiSystem::getInstance()->reboot() != 0) {
				LOG(LogWarning) <<
					"Restart terminated with non-zero result!";
			}
		}, _("NO"), nullptr));
	}, "iconRestart");

	s->addEntry(_("SHUTDOWN SYSTEM"), false, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY SHUTDOWN?"), _("YES"),
			[] {
			if (ApiSystem::getInstance()->shutdown() != 0) {
				LOG(LogWarning) <<
					"Shutdown terminated with non-zero result!";
			}
		}, _("NO"), nullptr));
	}, "iconShutdown");

	s->addEntry(_("FAST SHUTDOWN SYSTEM"), false, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY SHUTDOWN WITHOUT SAVING METADATAS?"), _("YES"),
			[] {
			if (ApiSystem::getInstance()->fastShutdown() != 0) {
				LOG(LogWarning) <<
					"Shutdown terminated with non-zero result!";
			}
		}, _("NO"), nullptr));
	}, "iconFastShutdown");

#ifdef WIN32
	if (Settings::getInstance()->getBool("ShowExit"))
	{
		s->addEntry(_("QUIT EMULATIONSTATION"), false, [window] {
			window->pushGui(new GuiMsgBox(window, _("REALLY QUIT?"), _("YES"),
				[] 
			{
				Scripting::fireEvent("quit");
				quitES("");
			}, _("NO"), nullptr));
		}, "iconQuit");
	}
#endif

	if (forceWin32Menu)
		s->getMenu().animateTo(Vector2f((Renderer::getScreenWidth() - s->getMenu().getSize().x()) / 2, (Renderer::getScreenHeight() - s->getMenu().getSize().y()) / 2));

	window->pushGui(s);
}

void GuiMenu::createInputTextRow(GuiSettings *gui, std::string title, const char *settingsID, bool password, bool storeInSettings
	, const std::function<void(Window*, std::string/*title*/, std::string /*value*/, const std::function<void(std::string)>& onsave)>& customEditor)
{
	

	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	// LABEL
	Window *window = mWindow;
	ComponentListRow row;

	auto lbl = std::make_shared<TextComponent>(window, title, font, color);
	row.addElement(lbl, true); // label

	std::shared_ptr<GuiComponent> ed;

	std::string value = storeInSettings ? Settings::getInstance()->getString(settingsID) : SystemConf::getInstance()->get(settingsID);

	ed = std::make_shared<TextComponent>(window, ((password && value != "") ? "*********" : value), font, color, ALIGN_RIGHT); // Font::get(FONT_SIZE_MEDIUM, FONT_PATH_LIGHT)
	row.addElement(ed, true);

	auto spacer = std::make_shared<GuiComponent>(mWindow);
	spacer->setSize(Renderer::getScreenWidth() * 0.005f, 0);
	row.addElement(spacer, false);

	auto bracket = std::make_shared<ImageComponent>(mWindow);
	bracket->setImage(theme->Icons.arrow);
	bracket->setResize(Vector2f(0, lbl->getFont()->getLetterHeight()));
	row.addElement(bracket, false);

	auto updateVal = [ed, settingsID, password, storeInSettings](const std::string &newVal) 
	{
		if (!password)
			ed->setValue(newVal);
		else
			ed->setValue("*********");

		if (storeInSettings)
			Settings::getInstance()->setString(settingsID, newVal);
		else
			SystemConf::getInstance()->set(settingsID, newVal);
	}; // ok callback (apply new value to ed)
	
	row.makeAcceptInputHandler([this, title, updateVal, settingsID, storeInSettings, customEditor]
	{
		std::string data = storeInSettings ? Settings::getInstance()->getString(settingsID) : SystemConf::getInstance()->get(settingsID);

		if (customEditor != nullptr)
			customEditor(mWindow, title, data, updateVal);
		else if (Settings::getInstance()->getBool("UseOSK"))
			mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, title, data, updateVal, false));
		else
			mWindow->pushGui(new GuiTextEditPopup(mWindow, title, data, updateVal, false));
	});

	gui->addRow(row);
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
	row.addElement(std::make_shared<TextComponent>(window, Utils::String::toUpper(Utils::String::replace(data, "_", " ")), font, color, ALIGN_LEFT), true, true);

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

void GuiMenu::popSystemConfigurationGui(Window* mWindow, SystemData *systemData, std::string previouslySelectedEmulator) {
  popSpecificConfigurationGui(mWindow, systemData->getFullName(), systemData->getName(), systemData, previouslySelectedEmulator);
}

void GuiMenu::popGameConfigurationGui(Window* mWindow, std::string title, std::string romFilename, SystemData *systemData, std::string previouslySelectedEmulator)
{
  popSpecificConfigurationGui(mWindow, title, romFilename, systemData, previouslySelectedEmulator);
}

void GuiMenu::popSpecificConfigurationGui(Window* mWindow, std::string title, std::string configName, SystemData *systemData, std::string previouslySelectedEmulator) 
{
	// The system configuration
	GuiSettings* systemConfiguration = new GuiSettings(mWindow, title.c_str());

	//Emulator choice
	auto emu_choice = std::make_shared<OptionListComponent<std::string>>(mWindow, "emulator", false);
	bool selected = false;
	std::string selectedEmulator = "";

	for (auto emulator : systemData->getEmulators()) 
	{
		bool found;
		std::string curEmulatorName = emulator.first;
		if (!previouslySelectedEmulator.empty())			
			found = previouslySelectedEmulator == curEmulatorName; // We just changed the emulator
		else
			found = (SystemConf::getInstance()->get(configName + ".emulator") == curEmulatorName);

		if (found)
			selectedEmulator = curEmulatorName;
		
		selected = selected || found;
		emu_choice->add(curEmulatorName, curEmulatorName, found);
	}
	emu_choice->add(_("AUTO"), "auto", !selected);
	emu_choice->setSelectedChangedCallback([mWindow, title, configName, systemConfiguration, systemData](std::string s) {
		popSpecificConfigurationGui(mWindow, title, configName, systemData, s);
		delete systemConfiguration;
	});
	systemConfiguration->addWithLabel(_("Emulator"), emu_choice);

	// Core choice
	auto core_choice = std::make_shared<OptionListComponent<std::string> >(mWindow, _("Core"), false);

	// search if one will be selected
	bool onefound = false;

	for (auto emulator : systemData->getEmulators()) 
	{
		if (selectedEmulator != emulator.first)
			continue;
		
		for (auto core : emulator.second.cores)
		{
			if ((SystemConf::getInstance()->get(configName + ".core") == core.name)) 
			{
				onefound = true;
				break;
			}
		}		
	}

	// add auto if emu_choice is auto
	if (emu_choice->getSelected() == "auto") { // allow auto only if emulator is auto
		core_choice->add(_("AUTO"), "auto", !onefound);
		onefound = true;
	}

	// list
	for (auto emulator : systemData->getEmulators())
	{
		if (selectedEmulator != emulator.first)
			continue;
		
		for (auto core : emulator.second.cores)
		{
			bool found = (SystemConf::getInstance()->get(configName + ".core") == core.name);
			core_choice->add(core.name, core.name, found || !onefound); // select the first one if none is selected
			onefound = true;
		}		
	}

	systemConfiguration->addWithLabel(_("Core"), core_choice);

	// Screen ratio choice
	auto ratio_choice = createRatioOptionList(mWindow, configName);
	systemConfiguration->addWithLabel(_("GAME RATIO"), ratio_choice);
	// video resolution mode
	auto videoResolutionMode_choice = createVideoResolutionModeOptionList(mWindow, configName);
	systemConfiguration->addWithLabel(_("VIDEO MODE"), videoResolutionMode_choice);
	// smoothing
	auto smoothing_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SMOOTH GAMES"));
	smoothing_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".smooth") != "0" && SystemConf::getInstance()->get(configName + ".smooth") != "1");
	smoothing_enabled->add(_("ON"), "1", SystemConf::getInstance()->get(configName + ".smooth") == "1");
	smoothing_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get(configName + ".smooth") == "0");
	systemConfiguration->addWithLabel(_("SMOOTH GAMES"), smoothing_enabled);
	// rewind
	auto rewind_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("REWIND"));
	rewind_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".rewind") != "0" && SystemConf::getInstance()->get(configName + ".rewind") != "1");
	rewind_enabled->add(_("ON"), "1", SystemConf::getInstance()->get(configName + ".rewind") == "1");
	rewind_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get(configName + ".rewind") == "0");
	systemConfiguration->addWithLabel(_("REWIND"), rewind_enabled);
	// autosave
	auto autosave_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("AUTO SAVE/LOAD"));
	autosave_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".autosave") != "0" && SystemConf::getInstance()->get(configName + ".autosave") != "1");
	autosave_enabled->add(_("ON"), "1", SystemConf::getInstance()->get(configName + ".autosave") == "1");
	autosave_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get(configName + ".autosave") == "0");
	systemConfiguration->addWithLabel(_("AUTO SAVE/LOAD"), autosave_enabled);

	// Shaders preset
	auto shaders_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SHADERS SET"),
		false);
	std::string currentShader = SystemConf::getInstance()->get(configName + ".shaderset");
	if (currentShader.empty()) {
		currentShader = std::string("auto");
	}

	shaders_choices->add(_("AUTO"), "auto", currentShader == "auto");
	shaders_choices->add(_("NONE"), "none", currentShader == "none");
	shaders_choices->add(_("SCANLINES"), "scanlines", currentShader == "scanlines");
	shaders_choices->add(_("RETRO"), "retro", currentShader == "retro");
	shaders_choices->add(_("ENHANCED"), "enhanced", currentShader == "enhanced"); // batocera 5.23
	shaders_choices->add(_("CURVATURE"), "curvature", currentShader == "curvature"); // batocera 5.24
	shaders_choices->add(_("ZFAST"), "zfast", currentShader == "zfast"); // batocera 5.25
	shaders_choices->add(_("FLATTEN-GLOW"), "flatten-glow", currentShader == "flatten-glow"); // batocera 5.25
	systemConfiguration->addWithLabel(_("SHADERS SET"), shaders_choices);

	// Integer scale
	auto integerscale_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INTEGER SCALE (PIXEL PERFECT)"));
	integerscale_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".integerscale") != "0" && SystemConf::getInstance()->get(configName + ".integerscale") != "1");
	integerscale_enabled->add(_("ON"), "1", SystemConf::getInstance()->get(configName + ".integerscale") == "1");
	integerscale_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get(configName + ".integerscale") == "0");
	systemConfiguration->addWithLabel(_("INTEGER SCALE (PIXEL PERFECT)"), integerscale_enabled);
	systemConfiguration->addSaveFunc([integerscale_enabled, configName] {
		if (integerscale_enabled->changed()) {
			SystemConf::getInstance()->set(configName + ".integerscale", integerscale_enabled->getSelected());
			SystemConf::getInstance()->saveSystemConf();
		}
	});

	// decorations
	{
		Window* window = mWindow;
		auto sets = GuiMenu::getDecorationsSets(systemData);
		auto decorations = std::make_shared<OptionListComponent<std::string> >(mWindow, _("DECORATION"), false);
		decorations->setRowTemplate([window, sets](std::string data, ComponentListRow& row)
		{
			createDecorationItemTemplate(window, sets, data, row);
		});

		std::vector<std::string> decorations_item;
		decorations_item.push_back(_("AUTO"));
		decorations_item.push_back(_("NONE"));


		for (auto set : sets)
			decorations_item.push_back(set.name);

		for (auto it = decorations_item.begin(); it != decorations_item.end(); it++) {
			decorations->add(*it, *it,
				(SystemConf::getInstance()->get(configName + ".bezel") == *it)
				||
				(SystemConf::getInstance()->get(configName + ".bezel") == "none" && *it == _("NONE"))
				||
				(SystemConf::getInstance()->get(configName + ".bezel") == "" && *it == _("AUTO"))
			);
		}
		systemConfiguration->addWithLabel(_("DECORATION"), decorations);
		systemConfiguration->addSaveFunc([decorations, configName] {
			if (decorations->changed()) {
				SystemConf::getInstance()->set(configName + ".bezel", decorations->getSelected() == _("NONE") ? "none" : decorations->getSelected() == _("AUTO") ? "" : decorations->getSelected());
				SystemConf::getInstance()->saveSystemConf();
			}
		});
	}

	systemConfiguration->addEntry(_("LATENCY REDUCTION"), true, [mWindow, configName] { openLatencyReductionConfiguration(mWindow, configName); });

	// gameboy colorize
	auto colorizations_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("COLORIZATION"), false);
	std::string currentColorization = SystemConf::getInstance()->get(configName + "-renderer.colorization");
	if (currentColorization.empty()) {
		currentColorization = std::string("auto");
	}
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
							 "Special 3" };
	int n_all_gambate_gc_colors_modes = 50;
	for (int i = 0; i < n_all_gambate_gc_colors_modes; i++) {
		colorizations_choices->add(all_gambate_gc_colors_modes[i], all_gambate_gc_colors_modes[i], currentColorization == std::string(all_gambate_gc_colors_modes[i]));
	}
	if (systemData->getName() == "gb" || systemData->getName() == "gbc" || systemData->getName() == "gb2players" || systemData->getName() == "gbc2players") // only for gb, gbc and gb2players
		systemConfiguration->addWithLabel(_("COLORIZATION"), colorizations_choices);

	// ps2 full boot
	auto fullboot_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("FULL BOOT"));
	fullboot_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".fullboot") != "0" && SystemConf::getInstance()->get(configName + ".fullboot") != "1");
	fullboot_enabled->add(_("ON"), "1", SystemConf::getInstance()->get(configName + ".fullboot") == "1");
	fullboot_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get(configName + ".fullboot") == "0");
	if (systemData->getName() == "ps2") // only for ps2
		systemConfiguration->addWithLabel(_("FULL BOOT"), fullboot_enabled);

	// wii emulated wiimotes
	auto emulatedwiimotes_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("EMULATED WIIMOTES"));
	emulatedwiimotes_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".emulatedwiimotes") != "0" && SystemConf::getInstance()->get(configName + ".emulatedwiimotes") != "1");
	emulatedwiimotes_enabled->add(_("ON"), "1", SystemConf::getInstance()->get(configName + ".emulatedwiimotes") == "1");
	emulatedwiimotes_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get(configName + ".emulatedwiimotes") == "0");
	if (systemData->getName() == "wii") // only for wii
		systemConfiguration->addWithLabel(_("EMULATED WIIMOTES"), emulatedwiimotes_enabled);

	// citra change screen layout
	auto changescreen_layout = std::make_shared<OptionListComponent<std::string>>(mWindow, _("CHANGE SCREEN LAYOUT"));
	changescreen_layout->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".layout_option") != "2" && SystemConf::getInstance()->get(configName + ".layout_option") != "3");
	changescreen_layout->add(_("LARGE SCREEN"), "2", SystemConf::getInstance()->get(configName + ".layout_option") == "2");
	changescreen_layout->add(_("SIDE BY SIDE"), "3", SystemConf::getInstance()->get(configName + ".layout_option") == "3");
	if (systemData->getName() == "3ds") // only for 3ds
		systemConfiguration->addWithLabel(_("CHANGE SCREEN LAYOUT"), changescreen_layout);

	// psp internal resolution
	auto internalresolution = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INTERNAL RESOLUTION"));
	internalresolution->add(_("AUTO"), "auto",
		SystemConf::getInstance()->get(configName + ".internalresolution") != "1" &&
		SystemConf::getInstance()->get(configName + ".internalresolution") != "2" &&
		SystemConf::getInstance()->get(configName + ".internalresolution") != "4" &&
		SystemConf::getInstance()->get(configName + ".internalresolution") != "8" &&
		SystemConf::getInstance()->get(configName + ".internalresolution") != "10");
	internalresolution->add("1", "1", SystemConf::getInstance()->get(configName + ".internalresolution") == "1");
	internalresolution->add("2", "2", SystemConf::getInstance()->get(configName + ".internalresolution") == "2");
	internalresolution->add("4", "4", SystemConf::getInstance()->get(configName + ".internalresolution") == "4");
	internalresolution->add("8", "8", SystemConf::getInstance()->get(configName + ".internalresolution") == "8");
	internalresolution->add("10", "10", SystemConf::getInstance()->get(configName + ".internalresolution") == "10");
	if (systemData->getName() == "psp") // only for psp
		systemConfiguration->addWithLabel(_("INTERNAL RESOLUTION"), internalresolution);

	systemConfiguration->addSaveFunc([configName, systemData, smoothing_enabled, rewind_enabled, ratio_choice, videoResolutionMode_choice, emu_choice, core_choice, autosave_enabled, shaders_choices, colorizations_choices, fullboot_enabled, emulatedwiimotes_enabled, changescreen_layout, internalresolution] 
	{
		SystemConf::getInstance()->set(configName + ".ratio", ratio_choice->getSelected());		
		SystemConf::getInstance()->set(configName + ".videomode", videoResolutionMode_choice->getSelected());		
		SystemConf::getInstance()->set(configName + ".rewind", rewind_enabled->getSelected());		
		SystemConf::getInstance()->set(configName + ".smooth", smoothing_enabled->getSelected());
		SystemConf::getInstance()->set(configName + ".autosave", autosave_enabled->getSelected());
		SystemConf::getInstance()->set(configName + ".shaderset", shaders_choices->getSelected());
		SystemConf::getInstance()->set(configName + "-renderer.colorization", colorizations_choices->getSelected());
		SystemConf::getInstance()->set(configName + ".fullboot", fullboot_enabled->getSelected());		
		SystemConf::getInstance()->set(configName + ".emulatedwiimotes", emulatedwiimotes_enabled->getSelected());
		SystemConf::getInstance()->set(configName + ".layout_option", changescreen_layout->getSelected());
		SystemConf::getInstance()->set(configName + ".internalresolution", internalresolution->getSelected());
		SystemConf::getInstance()->set(configName + ".emulator", emu_choice->getSelected());
		SystemConf::getInstance()->set(configName + ".core", core_choice->getSelected());
	});

	mWindow->pushGui(systemConfiguration);
}

std::shared_ptr<OptionListComponent<std::string>> GuiMenu::createRatioOptionList(Window *window,
                                                                                 std::string configname) {
  auto ratio_choice = std::make_shared<OptionListComponent<std::string> >(window, _("GAME RATIO"), false);
  std::string currentRatio = SystemConf::getInstance()->get(configname + ".ratio");
  if (currentRatio.empty()) {
    currentRatio = std::string("auto");
  }

  std::map<std::string, std::string> *ratioMap = LibretroRatio::getInstance()->getRatio();
  for (auto ratio = ratioMap->begin(); ratio != ratioMap->end(); ratio++) {
    ratio_choice->add(_(ratio->first.c_str()), ratio->second, currentRatio == ratio->second);
  }

  return ratio_choice;
}

std::shared_ptr<OptionListComponent<std::string>> GuiMenu::createVideoResolutionModeOptionList(Window *window, std::string configname) 
{
	auto videoResolutionMode_choice = std::make_shared<OptionListComponent<std::string> >(window, _("VIDEO MODE"), false);

	std::string currentVideoMode = SystemConf::getInstance()->get(configname + ".videomode");
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

		videoResolutionMode_choice->add(vname, tokens.at(0), currentVideoMode == tokens.at(0));
	}

	return videoResolutionMode_choice;
}

void GuiMenu::clearLoadedInput() {
  for(int i=0; i<mLoadedInput.size(); i++) {
    delete mLoadedInput[i];
  }
  mLoadedInput.clear();
}

GuiMenu::~GuiMenu() {
  clearLoadedInput();
}

std::vector<DecorationSetInfo> GuiMenu::getDecorationsSets(SystemData* system)
{
	std::vector<DecorationSetInfo> sets;

	static const size_t pathCount = 3;

	std::string paths[pathCount] = {
	  "/usr/share/batocera/datainit/decorations",
	  "/userdata/decorations",
	  Utils::FileSystem::getEsConfigPath() + "/decorations" // for win32 testings
	};

	Utils::FileSystem::stringList dirContent;
	std::string folder;

	for (size_t i = 0; i < pathCount; i++)
	{
		if (!Utils::FileSystem::isDirectory(paths[i]))
			continue;

		dirContent = Utils::FileSystem::getDirContent(paths[i]);
		for (Utils::FileSystem::stringList::const_iterator it = dirContent.cbegin(); it != dirContent.cend(); ++it)
		{
			if (Utils::FileSystem::isDirectory(*it))
			{
				folder = *it;

				DecorationSetInfo info;
				info.name = folder.substr(paths[i].size() + 1);
				info.path = folder;

				if (system != nullptr && info.name == "default")
				{
					std::string systemImg = paths[i] + "/default/systems/" + system->getName() + ".png";
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
