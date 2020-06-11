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
#include "guis/GuiBluetooth.h"
#include "scrapers/ThreadedScraper.h"
#include "FileSorts.h"
#include "ThreadedHasher.h"
#include "ThreadedBluetooth.h"
#include "views/gamelist/IGameListView.h"
#include "components/MultiLineMenuEntry.h"
#include "components/BatteryIndicatorComponent.h"
#include "GuiLoading.h"

#if WIN32
#include "Win32ApiSystem.h"
#endif

#define fake_gettext_fade _("fade")
#define fake_gettext_slide _("slide")
#define fake_gettext_instant _("instant")

// batocera-info
#define fake_gettext_system       _("System")
#define fake_gettext_architecture _("Architecture")
#define fake_gettext_temperature  _("Temperature")
#define fake_gettext_avail_memory _("Available memory")
#define fake_gettext_battery      _("Battery")
#define fake_gettext_cpu_model    _("Cpu model")
#define fake_gettext_cpu_number   _("Cpu number")
#define fake_gettext_cpu_feature  _("Cpu feature")

GuiMenu::GuiMenu(Window *window, bool animate) : GuiComponent(window), mMenu(window, _("MAIN MENU").c_str()), mVersion(window)
{
	// MAIN MENU
	bool isFullUI = UIModeController::getInstance()->isUIModeFull();
#ifdef _ENABLEEMUELEC
	bool isKidUI = UIModeController::getInstance()->isUIModeKid();
#endif

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
	if (SystemConf::getInstance()->getBool("kodi.enabled", true) && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::KODI))
		addEntry(_("KODI MEDIA CENTER").c_str(), false, [this] 
	{ 
		Window *window = mWindow;
		delete this;
		if (!ApiSystem::getInstance()->launchKodi(window))
			LOG(LogWarning) << "Shutdown terminated with non-zero result!";

	}, "iconKodi");	
#endif

#ifdef _ENABLEEMUELEC
	if (isFullUI)
		addEntry(_("EMUELEC SETTINGS").c_str(), true, [this] { openEmuELECSettings(); }); /* < emuelec */
#endif

	if (isFullUI &&
		ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RETROACHIVEMENTS) &&
		SystemConf::getInstance()->getBool("global.retroachievements") &&
		Settings::getInstance()->getBool("RetroachievementsMenuitem") && 
		SystemConf::getInstance()->get("global.retroachievements.username") != "")
		addEntry(_("RETROACHIEVEMENTS").c_str(), true, [this] { GuiRetroAchievements::show(mWindow); }, "iconRetroachievements");
	
	if (isFullUI)
	{
#if !defined(WIN32) || defined(_DEBUG)
		addEntry(_("GAMES SETTINGS").c_str(), true, [this] { openGamesSettings_batocera(); }, "iconGames");
		addEntry(_("CONTROLLERS SETTINGS").c_str(), true, [this] { openControllersSettings_batocera(); }, "iconControllers");
		addEntry(_("UI SETTINGS").c_str(), true, [this] { openUISettings(); }, "iconUI");
		addEntry(_("GAME COLLECTION SETTINGS").c_str(), true, [this] { openCollectionSystemSettings(); }, "iconAdvanced");
		addEntry(_("SOUND SETTINGS").c_str(), true, [this] { openSoundSettings(); }, "iconSound");

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::WIFI))
			addEntry(_("NETWORK SETTINGS").c_str(), true, [this] { openNetworkSettings_batocera(); }, "iconNetwork");
#else
		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
			addEntry(_("GAMES SETTINGS").c_str(), true, [this] { openGamesSettings_batocera(); }, "iconGames");

		addEntry(_("UI SETTINGS").c_str(), true, [this] { openUISettings(); }, "iconUI");

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))		
			addEntry(_("CONTROLLERS SETTINGS").c_str(), true, [this] { openControllersSettings_batocera(); }, "iconControllers");
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

		addEntry(_("SCRAPE").c_str(), true, [this] { openScraperSettings(); }, "iconScraper");		
		addEntry(_("UPDATES & DOWNLOADS"), true, [this] { openUpdatesSettings(); }, "iconUpdates");
		addEntry(_("SYSTEM SETTINGS").c_str(), true, [this] { openSystemSettings_batocera(); }, "iconSystem");
	}
	else
		addEntry(_("INFORMATIONS").c_str(), true, [this] { openSystemInformations_batocera(); }, "iconSystem");

#ifdef WIN32
	addEntry(_("QUIT").c_str(), !Settings::getInstance()->getBool("ShowOnlyExit"), [this] {openQuitMenu_batocera(); }, "iconQuit");
#else
#ifdef _ENABLEEMUELEC
if (!isKidUI) {
	addEntry(_("QUIT").c_str(), true, [this] { openQuitMenu_batocera(); }, "iconQuit");
}
#else
	addEntry(_("QUIT").c_str(), true, [this] { openQuitMenu_batocera(); }, "iconQuit");
#endif
#endif
	
	addChild(&mMenu);
	addVersionInfo(); // batocera
	setSize(mMenu.getSize());

	if (animate)
	{
		if (Renderer::isSmallScreen())
			animateTo((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);
		else
			animateTo(
				Vector2f((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.9),
				Vector2f((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.15f));
	}
	else
	{
		if (Renderer::isSmallScreen())
			setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);
		else
			setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.15f);
	}
}
#ifdef _ENABLEEMUELEC
/* < emuelec */
void GuiMenu::openEmuELECSettings()
{
	auto s = new GuiSettings(mWindow, "EmuELEC Settings");

	Window* window = mWindow;
	std::string a;
	auto emuelec_video_mode = std::make_shared< OptionListComponent<std::string> >(mWindow, "VIDEO MODE", false);
        std::vector<std::string> videomode;
  /* for(std::stringstream ss(getShOutput(R"(~/.config/emuelec/scripts/get_supported_resolutions.sh)")); getline(ss, a, ','); ) {
        videomode.push_back(a);
	}*/
		videomode.push_back("1080p60hz");
		videomode.push_back("1080i60hz");
		videomode.push_back("720p60hz");
		videomode.push_back("720p50hz");
		videomode.push_back("480p60hz");
		videomode.push_back("480cvbs");
		videomode.push_back("576p50hz");
		videomode.push_back("1080p50hz");
		videomode.push_back("1080i50hz");
		videomode.push_back("576cvbs");
		videomode.push_back("Custom");
		for (auto it = videomode.cbegin(); it != videomode.cend(); it++) {
		emuelec_video_mode->add(*it, *it, SystemConf::getInstance()->get("ee_videomode") == *it); }
		s->addWithLabel(_("VIDEO MODE"), emuelec_video_mode);
	   	s->addSaveFunc([emuelec_video_mode, window] {
			if (emuelec_video_mode->changed()) {
			std::string selectedVideoMode = emuelec_video_mode->getSelected();
			if (emuelec_video_mode->getSelected() != "Custom") {
			std::string msg = _("You are about to set EmuELEC resolution to:") +"\n" + selectedVideoMode + "\n";
			if(Utils::FileSystem::exists("/ee_s905")) {
			msg += _("Emulationstation will restart") + ".\n";
		}
			msg += _("Do you want to proceed ?");
			window->pushGui(new GuiMsgBox(window, msg,
				_("YES"), [selectedVideoMode] {
					runSystemCommand("echo "+selectedVideoMode+" > /sys/class/display/mode", "", nullptr);
					SystemConf::getInstance()->set("ee_videomode", selectedVideoMode);
					LOG(LogInfo) << "Setting video to " << selectedVideoMode;
					runSystemCommand("/storage/.config/emuelec/scripts/setres.sh", "", nullptr);
					SystemConf::getInstance()->saveSystemConf();
				if(Utils::FileSystem::exists("/ee_s905")) {
					runSystemCommand("systemctl restart emustation", "", nullptr); 
				}
			}, _("NO"),nullptr));
		} else { 
			if(Utils::FileSystem::exists("/storage/.config/EE_VIDEO_MODE")) {
				runSystemCommand("echo $(cat /storage/.config/EE_VIDEO_MODE) > /sys/class/display/mode", "", nullptr);
				LOG(LogInfo) << "Setting custom video mode from /storage/.config/EE_VIDEO_MODE to " << runSystemCommand("cat /storage/.config/EE_VIDEO_MODE", "", nullptr);
				SystemConf::getInstance()->set("ee_videomode", selectedVideoMode);
				SystemConf::getInstance()->saveSystemConf();
			} else { 
				if(Utils::FileSystem::exists("/flash/EE_VIDEO_MODE")) {
				runSystemCommand("echo $(cat /flash/EE_VIDEO_MODE) > /sys/class/display/mode", "", nullptr);
				LOG(LogInfo) << "Setting custom video mode from /flash/EE_VIDEO_MODE to " << runSystemCommand("cat /flash/EE_VIDEO_MODE", "", nullptr);
				SystemConf::getInstance()->set("ee_videomode", selectedVideoMode);
				SystemConf::getInstance()->saveSystemConf();
					} else {
					runSystemCommand("echo " + SystemConf::getInstance()->get("ee_videomode")+ " > /sys/class/display/mode", "", nullptr);
					std::string msg = "/storage/.config/EE_VIDEO_MODE or /flash/EE_VIDEO_MODE not found";
					window->pushGui(new GuiMsgBox(window, msg,
				"OK", [selectedVideoMode] {
					LOG(LogInfo) << "EE_VIDEO_MODE was not found! Setting video mode to " + SystemConf::getInstance()->get("ee_videomode");
			}));
					}
				}
			}
		 }
		});
		
		auto emuelec_audiodev_def = std::make_shared< OptionListComponent<std::string> >(mWindow, "AUDIO DEVICE", false);
		std::vector<std::string> Audiodevices;
		Audiodevices.push_back("auto");
		Audiodevices.push_back("0,0");
		Audiodevices.push_back("0,1");
		Audiodevices.push_back("1,0");
		Audiodevices.push_back("1,1");
		
		auto AudiodevicesS = SystemConf::getInstance()->get("ee_audio_device");
		if (AudiodevicesS.empty())
		AudiodevicesS = "auto";
		
		for (auto it = Audiodevices.cbegin(); it != Audiodevices.cend(); it++)
		emuelec_audiodev_def->add(*it, *it, AudiodevicesS == *it);
		
		s->addWithLabel(_("AUDIO DEVICE"), emuelec_audiodev_def);
		s->addSaveFunc([emuelec_audiodev_def] {
			if (emuelec_audiodev_def->changed()) {
				std::string selectedaudiodev = emuelec_audiodev_def->getSelected();
				SystemConf::getInstance()->set("ee_audio_device", selectedaudiodev);
				SystemConf::getInstance()->saveSystemConf();
			}
		});
		
       auto sshd_enabled = std::make_shared<SwitchComponent>(mWindow);
		bool baseEnabled = SystemConf::getInstance()->get("ee_ssh.enabled") == "1";
		sshd_enabled->setState(baseEnabled);
		s->addWithLabel(_("ENABLE SSH"), sshd_enabled);
		s->addSaveFunc([sshd_enabled] {
			if (sshd_enabled->changed()) {
			if (sshd_enabled->getState() == false) {
				runSystemCommand("systemctl stop sshd", "", nullptr); 
				runSystemCommand("rm /storage/.cache/services/sshd.conf", "", nullptr); 
			} else { 
				runSystemCommand("mkdir -p /storage/.cache/services/", "", nullptr);
				runSystemCommand("touch /storage/.cache/services/sshd.conf", "", nullptr);
				runSystemCommand("systemctl start sshd", "", nullptr);
			}
                bool sshenabled = sshd_enabled->getState();
                SystemConf::getInstance()->set("ee_ssh.enabled", sshenabled ? "1" : "0");
				SystemConf::getInstance()->saveSystemConf();
			}
		});
			
		auto emuelec_boot_def = std::make_shared< OptionListComponent<std::string> >(mWindow, "START AT BOOT", false);
		std::vector<std::string> devices;
		devices.push_back("Emulationstation");
		devices.push_back("Retroarch");
		for (auto it = devices.cbegin(); it != devices.cend(); it++)
		emuelec_boot_def->add(*it, *it, SystemConf::getInstance()->get("ee_boot") == *it);
		s->addWithLabel(_("START AT BOOT"), emuelec_boot_def);
		s->addSaveFunc([emuelec_boot_def] {
			if (emuelec_boot_def->changed()) {
				std::string selectedBootMode = emuelec_boot_def->getSelected();
				SystemConf::getInstance()->set("ee_boot", selectedBootMode);
				SystemConf::getInstance()->saveSystemConf();
			}
		});
       
       auto bezels_enabled = std::make_shared<SwitchComponent>(mWindow);
		bool bezelsEnabled = SystemConf::getInstance()->get("global.bezel") == "1";
		bezels_enabled->setState(bezelsEnabled);
		s->addWithLabel(_("ENABLE RA BEZELS"), bezels_enabled);
		s->addSaveFunc([bezels_enabled] {
			bool bezelsenabled = bezels_enabled->getState();
                SystemConf::getInstance()->set("global.bezel", bezelsenabled ? "1" : "0");
				SystemConf::getInstance()->saveSystemConf();
			});	
       
       auto splash_enabled = std::make_shared<SwitchComponent>(mWindow);
		bool splashEnabled = SystemConf::getInstance()->get("ee_splash.enabled") == "1";
		splash_enabled->setState(splashEnabled);
		s->addWithLabel(_("ENABLE RA SPLASH"), splash_enabled);
		s->addSaveFunc([splash_enabled] {
                bool splashenabled = splash_enabled->getState();
                SystemConf::getInstance()->set("ee_splash.enabled", splashenabled ? "1" : "0");
				SystemConf::getInstance()->saveSystemConf();
			});

	auto enable_bootvideo = std::make_shared<SwitchComponent>(mWindow);
	bool bootEnabled = SystemConf::getInstance()->get("ee_bootvideo.enabled") == "1";
	enable_bootvideo->setState(bootEnabled);
	s->addWithLabel(_("ALWAYS SHOW BOOT VIDEO"), enable_bootvideo);
	
	s->addSaveFunc([enable_bootvideo, window] {
		bool bootvideoenabled = enable_bootvideo->getState();
		SystemConf::getInstance()->set("ee_bootvideo.enabled", bootvideoenabled ? "1" : "0");
		SystemConf::getInstance()->saveSystemConf();
	});
	
	auto enable_advmamegp = std::make_shared<SwitchComponent>(mWindow);
	bool advgpEnabled = SystemConf::getInstance()->get("advmame_auto_gamepad") == "1";
	enable_advmamegp->setState(advgpEnabled);
	s->addWithLabel(_("AUTO CONFIG ADVANCEMAME GAMEPAD"), enable_advmamegp);
	
	s->addSaveFunc([enable_advmamegp, window] {
		bool advmamegpenabled = enable_advmamegp->getState();
		SystemConf::getInstance()->set("advmame_auto_gamepad", advmamegpenabled ? "1" : "0");
		SystemConf::getInstance()->saveSystemConf();
	});
	
	if (UIModeController::getInstance()->isUIModeFull())
	{
	
	
	//Danger zone options
		s->addEntry(_("DANGER ZONE!"), true, [this]
		{
	Window* window = mWindow;
	ComponentListRow row;
	GuiSettings *danger_zone = new GuiSettings(mWindow, _("DANGER ZONE!").c_str());
	row.makeAcceptInputHandler([window] {
		window->pushGui(new GuiMsgBox(window, _("WARNING THIS WILL RESTART EMULATIONSTATION!\n\nAFTER THE SCRIPT IS DONE REMEMBER TO COPY THE FILE /storage/downloads/ee_backup_config.zip TO SOME PLACE SAFE OR IT WILL BE DELETED ON NEXT REBOOT!\n\nBACKUP CURRENT CONFIG AND RESTART?"), _("YES"),
				[] { 
				runSystemCommand("systemd-run /emuelec/scripts/ee_backup.sh b", "", nullptr);
				}, _("NO"), nullptr));
	});
	row.addElement(std::make_shared<TextComponent>(window, _("BACKUP EMUELEC CONFIGS"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
	danger_zone->addRow(row);
	row.elements.clear();
	row.makeAcceptInputHandler([window] {
		window->pushGui(new GuiMsgBox(window, _("WARNING: SYSTEM WILL RESET SCRIPTS AND BINARIES !\nUPDATE, DOWNLOADS, THEMES, BLUETOOTH PAIRINGS AND ROMS FOLDER WILL NOT BE AFFECTED.\n\nRESET SCRIPTS AND BINARIES TO DEFAULT AND RESTART?"), _("YES"),
				[] { 
				runSystemCommand("systemd-run /emuelec/scripts/clearconfig.sh EMUS", "", nullptr);
				}, _("NO"), nullptr));
	});
	row.addElement(std::make_shared<TextComponent>(window, _("RESET EMUELEC SCRIPTS AND BINARIES TO DEFAULT"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
	danger_zone->addRow(row);
	row.elements.clear();
	row.makeAcceptInputHandler([window] {
		window->pushGui(new GuiMsgBox(window, _("WARNING: ALL CONFIGURATIONS WILL BE RESET AND NO BACKUP WILL BE CREATED!\n\nIF YOU WANT TO KEEP YOUR SETTINGS MAKE A BACKUP AND SAVE IT ON AN EXTERNAL DRIVE BEFORE RUNING THIS OPTION!\n\nRESET SYSTEM TO DEFAULT CONFIG AND RESTART?"), _("YES"),
				[] { 
				runSystemCommand("systemd-run /emuelec/scripts/clearconfig.sh ALL", "", nullptr);
				}, _("NO"), nullptr));
	});
	row.addElement(std::make_shared<TextComponent>(window, _("RESET SYSTEM TO DEFAULT CONFIG"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
	danger_zone->addRow(row);
	row.elements.clear();
			
			mWindow->pushGui(danger_zone);
		});
	
	mWindow->pushGui(s);
 }
}
/*  emuelec >*/
#endif
void GuiMenu::openScraperSettings()
{	
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

	auto s = new GuiSettings(mWindow, 
		_("SCRAPER"), 
		_("SCRAPE NOW"), [openScrapeNow](GuiSettings* settings)
	{
		settings->save();
		openScrapeNow();
	});

	std::string scraper = Settings::getInstance()->getString("Scraper");

	// scrape from
	auto scraper_list = std::make_shared< OptionListComponent< std::string > >(mWindow, _("SCRAPE FROM"), false);
	std::vector<std::string> scrapers = Scraper::getScraperList();

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
		imageSource->add(_("FAN ART"), "fanart", imageSourceName == "fanart");

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
		
		// SCRAPE FANART
		auto scrape_fanart = std::make_shared<SwitchComponent>(mWindow);
		scrape_fanart->setState(Settings::getInstance()->getBool("ScrapeFanart"));
		s->addWithLabel(_("SCRAPE FANART"), scrape_fanart);
		s->addSaveFunc([scrape_fanart] { Settings::getInstance()->setBool("ScrapeFanart", scrape_fanart->getState()); });
		
		// SCRAPE TITLESHOT
		/*
		auto scrape_titleshot = std::make_shared<SwitchComponent>(mWindow);
		scrape_titleshot->setState(Settings::getInstance()->getBool("ScrapeTitleShot"));
		s->addWithLabel(_("SCRAPE TITLESHOT"), scrape_titleshot);
		s->addSaveFunc([scrape_titleshot] { Settings::getInstance()->setBool("ScrapeTitleShot", scrape_titleshot->getState()); });
		
		// SCRAPE MAP		
		auto scrape_map = std::make_shared<SwitchComponent>(mWindow);
		scrape_map->setState(Settings::getInstance()->getBool("ScrapeMap"));
		s->addWithLabel(_("SCRAPE MAP"), scrape_map);
		s->addSaveFunc([scrape_map] { Settings::getInstance()->setBool("ScrapeMap", scrape_map->getState()); });
		
		// SCRAPE CARTRIDGE
		auto scrape_cartridge = std::make_shared<SwitchComponent>(mWindow);
		scrape_cartridge->setState(Settings::getInstance()->getBool("ScrapeCartridge"));
		s->addWithLabel(_("SCRAPE CARTRIDGE"), scrape_cartridge);
		s->addSaveFunc([scrape_cartridge] { Settings::getInstance()->setBool("ScrapeCartridge", scrape_cartridge->getState()); });
		*/
		// SCRAPE MANUAL
		auto scrape_manual = std::make_shared<SwitchComponent>(mWindow);
		scrape_manual->setState(Settings::getInstance()->getBool("ScrapeManual"));
		s->addWithLabel(_("SCRAPE MANUAL"), scrape_manual);
		s->addSaveFunc([scrape_manual] { Settings::getInstance()->setBool("ScrapeManual", scrape_manual->getState()); });
		

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

	/*
	std::function<void()> openAndSave = openScrapeNow;
	openAndSave = [s, openAndSave] { s->save(); openAndSave(); };
	s->addEntry(_("SCRAPE NOW"), false, openAndSave, "iconScraper");
	*/
		
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
	window->pushGui(new GuiMsgBox(window, _("ARE YOU SURE YOU WANT TO CONFIGURE INPUT?"), 
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

	if (!ApiSystem::getInstance()->getVersion().empty())
	{
#if WIN32
		std::string aboutInfo;

		std::string localVersionFile = Utils::FileSystem::getExePath() + "/about.info";
		if (Utils::FileSystem::exists(localVersionFile))
		{
			aboutInfo = Utils::FileSystem::readAllText(localVersionFile);
		}
			aboutInfo = Utils::String::replace(Utils::String::replace(aboutInfo, "\r", ""), "\n", "");

		if (!aboutInfo.empty())
			mVersion.setText(aboutInfo + buildDate);
		else
#endif
#ifdef _ENABLEEMUELEC	
		mVersion.setText("EMUELEC ES V" + ApiSystem::getInstance()->getVersion() + buildDate + " IP:" + getShOutput(R"(/storage/.emulationstation/scripts/ip.sh)"));
#else
		mVersion.setText("BATOCERA.LINUX ES V" + ApiSystem::getInstance()->getVersion() + buildDate);
#endif
	}
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


void GuiMenu::openCollectionSystemSettings() 
{
	if (ThreadedScraper::isRunning() || ThreadedHasher::isRunning())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("THIS FUNCTION IS DISABLED WHEN SCRAPING IS RUNNING")));
		return;
	}

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
	prompts.push_back(HelpPrompt("up/down", _("CHOOSE"))); // batocera
	prompts.push_back(HelpPrompt(BUTTON_OK, _("SELECT"))); // batocera
	prompts.push_back(HelpPrompt("start", _("CLOSE"))); // batocera
	return prompts;
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
	auto userspace = std::make_shared<TextComponent>(window,
		ApiSystem::getInstance()->getFreeSpaceUserInfo(),
		font,
		warning ? 0xFF0000FF : color);
	informationsGui->addWithLabel(_("USER DISK USAGE"), userspace);

	auto systemspace = std::make_shared<TextComponent>(window,
		ApiSystem::getInstance()->getFreeSpaceSystemInfo(),
		font,
		color);
	informationsGui->addWithLabel(_("SYSTEM DISK USAGE"), systemspace);

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
			informationsGui->addWithLabel(_(tokens.at(0).c_str()), space);
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

#if !defined(WIN32) && !defined _ENABLEEMUELEC || defined(_DEBUG)
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

	s->addEntry(_("RESET FILE EXTENSIONS"), false, [this, s]
	{
		for (auto system : SystemData::sSystemVector)
			Settings::getInstance()->setString(system->getName() + ".HiddenExt", "");

		Settings::getInstance()->saveFile();
		reloadAllGames(mWindow, false);		
	});

	s->addEntry(_("REDETECT GAMES LANG/REGION"), false, [this] 
	{ 
		Window* window = mWindow;
		window->pushGui(new GuiLoading<int>(window, _("PLEASE WAIT"), []
		{
			for (auto system : SystemData::sSystemVector)
			{
				if (system->isCollection() || system->isGroupSystem())
					continue;

				for (auto game : system->getRootFolder()->getFilesRecursive(GAME))
					game->detectLanguageAndRegion(true);
			}

			return 0;
		}));
	});

#if defined(WIN32) && !defined(_DEBUG)
	// full exit
	auto fullExitMenu = std::make_shared<SwitchComponent>(mWindow);
	fullExitMenu->setState(!Settings::getInstance()->getBool("ShowOnlyExit"));
	s->addWithLabel(_("COMPLETE QUIT MENU"), fullExitMenu);
	s->addSaveFunc([fullExitMenu] { Settings::getInstance()->setBool("ShowOnlyExit", !fullExitMenu->getState()); });
#endif

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
	{
		// retroarch.menu_driver = rgui
		auto retroarchRgui = std::make_shared<SwitchComponent>(mWindow);
		retroarchRgui->setState(SystemConf::getInstance()->get("global.retroarch.menu_driver") == "rgui");
		s->addWithLabel(_("USE RETROARCH RGUI MENU"), retroarchRgui);
		s->addSaveFunc([retroarchRgui]
		{
			SystemConf::getInstance()->set("global.retroarch.menu_driver", retroarchRgui->getState() ? "rgui" : "");
		});

#if defined(WIN32)
		auto autoControllers = std::make_shared<SwitchComponent>(mWindow);
		autoControllers->setState(SystemConf::getInstance()->get("global.disableautocontrollers") != "1");
		s->addWithLabel(_("AUTOCONFIGURE EMULATORS CONTROLLERS"), autoControllers);
		s->addSaveFunc([autoControllers]
		{
			SystemConf::getInstance()->set("global.disableautocontrollers", autoControllers->getState() ? "" : "1");
		});
#endif
	}

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

#if !defined(WIN32) && !defined _ENABLEEMUELEC || defined(_DEBUG)
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
#endif
#ifndef _ENABLEEMUELEC
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DISKFORMAT))
		s->addEntry(_("FORMAT A DISK"), true, [this] { openFormatDriveSettings(); });
#endif
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
			window->closeSplashScreen();
		}
	});
#endif

	mWindow->pushGui(s);
}

void GuiMenu::openUpdatesSettings()
{
	GuiSettings *updateGui = new GuiSettings(mWindow, _("UPDATES & DOWNLOADS").c_str());

	updateGui->addGroup(_("DOWNLOADS"));

	// Batocera themes installer/browser
	updateGui->addEntry(_("THEMES"), true, [this] { mWindow->pushGui(new GuiThemeInstallStart(mWindow)); });

	// Batocera integration with theBezelProject
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DECORATIONS))
		updateGui->addEntry(_("THE BEZEL PROJECT"), true, [this] { mWindow->pushGui(new GuiBezelInstallMenu(mWindow)); });

#ifndef _ENABLEEMUELEC
	updateGui->addGroup(_("SOFTWARE UPDATES"));

	// Enable updates
	auto updates_enabled = std::make_shared<SwitchComponent>(mWindow);
	updates_enabled->setState(SystemConf::getInstance()->getBool("updates.enabled"));
	
	updateGui->addWithLabel(_("CHECK FOR UPDATES"), updates_enabled);
	updateGui->addSaveFunc([updates_enabled]
	{
		SystemConf::getInstance()->setBool("updates.enabled", updates_enabled->getState());
	});

	auto updatesTypeList = std::make_shared<OptionListComponent<std::string> >(mWindow, _("UPDATE TYPE"), false);
	
	std::string updatesType = SystemConf::getInstance()->get("updates.type");
	if (updatesType.empty() || updatesType != "beta")
		updatesType = "stable";
	
	updatesTypeList->add("stable", "stable", updatesType == "stable");
	updatesTypeList->add("beta", "beta", updatesType != "stable");

	updateGui->addWithLabel(_("UPDATE TYPE"), updatesTypeList);
	updateGui->addSaveFunc([updatesTypeList]
	{
		SystemConf::getInstance()->set("updates.type", updatesTypeList->getSelected());
	});

	// Start update
	updateGui->addEntry(GuiUpdate::state == GuiUpdateState::State::UPDATE_READY ? _("APPLY UPDATE") : _("START UPDATE"), true, [this]
	{
		if (GuiUpdate::state == GuiUpdateState::State::UPDATE_READY)
			quitES(QuitMode::RESTART);
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

	s->addGroup(_("SYSTEM"));

	// System informations
	s->addEntry(_("INFORMATION"), true, [this] { openSystemInformations_batocera(); });

	// language choice
	auto language_choice = std::make_shared<OptionListComponent<std::string> >(window, _("LANGUAGE"), false);

	std::string language = SystemConf::getInstance()->get("system.language");
	if (language.empty()) 
		language = "en_US";

	language_choice->add("ARABIC",               "ar_YE", language == "ar_YE");
	language_choice->add("CATALÀ",               "ca_ES", language == "ca_ES");
	language_choice->add("DEUTSCH", 	     "de_DE", language == "de_DE");
	language_choice->add("GREEK",                "el_GR", language == "el_GR");
	language_choice->add("ENGLISH", 	     "en_US", language == "en_US" || language == "en");
	language_choice->add("ESPAÑOL", 	     "es_ES", language == "es_ES" || language == "es");
	language_choice->add("ESPAÑOL MEXICANO",     "es_MX", language == "es_MX");
	language_choice->add("BASQUE",               "eu_ES", language == "eu_ES");
	language_choice->add("FRANÇAIS",             "fr_FR", language == "fr_FR" || language == "fr");
	language_choice->add("HUNGARIAN",            "hu_HU", language == "hu_HU");
	language_choice->add("ITALIANO",             "it_IT", language == "it_IT");
	language_choice->add("JAPANESE", 	     "ja_JP", language == "ja_JP");
	language_choice->add("KOREAN",   	     "ko_KR", language == "ko_KR" || language == "ko");
	language_choice->add("NORWEGIAN BOKMAL",     "nb_NO", language == "nb_NO");
	language_choice->add("DUTCH",                "nl_NL", language == "nl_NL");
	language_choice->add("NORWEGIAN",            "nn_NO", language == "nn_NO");
	language_choice->add("POLISH",               "pl_PL", language == "pl_PL");
	language_choice->add("PORTUGUES BRASILEIRO", "pt_BR", language == "pt_BR");
	language_choice->add("PORTUGUES PORTUGAL",   "pt_PT", language == "pt_PT");
	language_choice->add("RUSSIAN",              "ru_RU", language == "ru_RU");
	language_choice->add("SVENSKA", 	     "sv_SE", language == "sv_SE");
	language_choice->add("TÜRKÇE",  	     "tr_TR", language == "tr_TR");
	language_choice->add("Українська",           "uk_UA", language == "uk_UA");
	language_choice->add("简体中文", 	     "zh_CN", language == "zh_CN");
	language_choice->add("正體中文", 	     "zh_TW", language == "zh_TW");
	s->addWithLabel(_("LANGUAGE"), language_choice);

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
		if (Settings::getInstance()->getString("PowerSaverMode") != "instant" && power_saver->getSelected() == "instant") 
		{						
			Settings::getInstance()->setBool("EnableSounds", false);
		}
		Settings::getInstance()->setString("PowerSaverMode", power_saver->getSelected());
		PowerSaver::init();
	});

#ifdef _ENABLE_KODI_
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::KODI))
	{
		s->addEntry(_("KODI SETTINGS"), true, [this] 
		{
			GuiSettings* kodiGui = new GuiSettings(mWindow, _("KODI SETTINGS").c_str());

			auto kodiEnabled = std::make_shared<SwitchComponent>(mWindow);
			kodiEnabled->setState(SystemConf::getInstance()->getBool("kodi.enabled", true));
			kodiGui->addWithLabel(_("ENABLE KODI"), kodiEnabled);

			auto kodiAtStart = std::make_shared<SwitchComponent>(mWindow);
			kodiAtStart->setState(SystemConf::getInstance()->getBool("kodi.atstartup"));
			kodiGui->addWithLabel(_("KODI AT START"), kodiAtStart);

			auto kodiX = std::make_shared<SwitchComponent>(mWindow);
			kodiX->setState(SystemConf::getInstance()->getBool("kodi.xbutton"));
			kodiGui->addWithLabel(_("START KODI WITH X"), kodiX);

			kodiGui->addSaveFunc([kodiEnabled, kodiAtStart, kodiX] 
			{
				SystemConf::getInstance()->setBool("kodi.enabled", kodiEnabled->getState());
				SystemConf::getInstance()->setBool("kodi.atstartup", kodiAtStart->getState());
				SystemConf::getInstance()->setBool("kodi.xbutton", kodiX->getState());				
			});

			mWindow->pushGui(kodiGui);
		});
	}
#endif


#if !defined(WIN32) && !defined _ENABLEEMUELEC || defined(_DEBUG)
	s->addGroup(_("HARDWARE"));

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
#else
	if (!ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
	{
		// Retroachievements
		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RETROACHIVEMENTS))
			s->addEntry(_("RETROACHIEVEMENTS SETTINGS"), true, [this] { openRetroachievementsSettings(); });

		if (SystemData::isNetplayActivated() && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::NETPLAY))
			s->addEntry(_("NETPLAY SETTINGS"), true, [this] { openNetplaySettings(); }, "iconNetplay");

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BIOSINFORMATION))
			s->addEntry(_("MISSING BIOS"), true, [this, s] { openMissingBiosSettings(); });
	}
#endif

	std::shared_ptr<OptionListComponent<std::string>> overclock_choice;

	// Overclock choice
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::OVERCLOCK))
	{
		overclock_choice = std::make_shared<OptionListComponent<std::string> >(window, _("OVERCLOCK"), false);

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
	}

#if !defined(WIN32) && !defined _ENABLEEMUELEC || defined(_DEBUG)
	s->addGroup(_("STORAGE"));
#endif

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
#if !defined(WIN32) && !defined _ENABLEEMUELEC || defined(_DEBUG)
	s->addWithLabel(_("STORAGE DEVICE"), optionsStorage);
#endif

#if !defined(WIN32) && !defined _ENABLEEMUELEC || defined(_DEBUG)
	// backup
	s->addEntry(_("BACKUP USER DATA"), true, [this] { mWindow->pushGui(new GuiBackupStart(mWindow)); });
#endif

#if !defined(WIN32) && !defined _ENABLEEMUELEC || defined(_DEBUG)
	// Install
	s->addEntry(_("INSTALL BATOCERA ON A NEW DISK"), true, [this] { mWindow->pushGui(new GuiInstallStart(mWindow)); });

	s->addGroup(_("ADVANCED"));

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
#else
	if (isFullUI)
		s->addGroup(_("ADVANCED"));
#endif

	s->addSaveFunc([overclock_choice, window, language_choice, language, optionsStorage, selectedStorage, s] 
	{
		bool reboot = false;
		if (optionsStorage->changed()) 
		{
			ApiSystem::getInstance()->setStorage(optionsStorage->getSelected());
			reboot = true;
		}

		if (overclock_choice && overclock_choice->changed() && Settings::getInstance()->setString("Overclock", overclock_choice->getSelected()))
		{
			ApiSystem::getInstance()->setOverclock(overclock_choice->getSelected());
			reboot = true;
		}

		if (language_choice->changed()) 
		{	
#ifdef _ENABLEEMUELEC
			std::string selectedLanguage = language_choice->getSelected();
			std::string msg = _("You are about to set EmuELEC Language to:") +"\n" +  selectedLanguage + "\n";
			msg += _("Emulationstation will restart")+"\n";
			msg += _("Do you want to proceed ?");
			window->pushGui(new GuiMsgBox(window, msg, _("YES"), [selectedLanguage] {
			SystemConf::getInstance()->set("system.language", selectedLanguage);
			SystemConf::getInstance()->saveSystemConf();
					runSystemCommand("systemctl restart emustation", "", nullptr); 
			}, "NO",nullptr));
#else
			if (SystemConf::getInstance()->set("system.language", language_choice->getSelected()))
			{
				FileSorts::reset();
				MetaDataList::initMetadata();

				s->setVariable("reloadGuiMenu", true);
#ifdef HAVE_INTL
				reboot = true;
#endif
			}			
#endif
		}

		if (reboot)
			window->displayNotificationMessage(_U("\uF011  ") + _("A REBOOT OF THE SYSTEM IS REQUIRED TO APPLY THE NEW CONFIGURATION"));

	});

	// Developer options
	if (isFullUI)
		s->addEntry(_("DEVELOPER"), true, [this] { openDeveloperSettings(); });

	auto pthis = this;
	s->onFinalize([s, pthis, window]
	{
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
#ifdef _ENABLEEMUELEC
	auto runaheadValue = getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".runahead'");
#else
	auto runaheadValue = SystemConf::getInstance()->get(configName + ".runahead");
#endif
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
#ifdef _ENABLEEMUELEC
	auto secondinstanceValue = getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".secondinstance'");
#else
	auto secondinstanceValue = SystemConf::getInstance()->get(configName + ".secondinstance");
#endif
	auto secondinstance_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("RUN-AHEAD USE SECOND INSTANCE"));
	secondinstance_enabled->add(_("AUTO"), "auto", secondinstanceValue != "0" && secondinstanceValue != "1");
	secondinstance_enabled->add(_("ON"), "1", secondinstanceValue == "1");
	secondinstance_enabled->add(_("OFF"), "0", secondinstanceValue == "0");

	if (!secondinstance_enabled->hasSelection())
		secondinstance_enabled->selectFirstItem();

	guiLatency->addWithLabel(_("RUN-AHEAD USE SECOND INSTANCE"), secondinstance_enabled);

	guiLatency->addSaveFunc([configName, runahead_enabled, secondinstance_enabled]
	{
#ifdef _ENABLEEMUELEC
		getShOutput(R"(/emuelec/scripts/setemu.sh set ')" + configName + ".runahead' " + runahead_enabled->getSelected());
		getShOutput(R"(/emuelec/scripts/setemu.sh set ')" + configName + ".secondinstance' " + secondinstance_enabled->getSelected());
#else
		SystemConf::getInstance()->set(configName + ".runahead", runahead_enabled->getSelected());
		SystemConf::getInstance()->set(configName + ".secondinstance", secondinstance_enabled->getSelected());
#endif
	});
	
	mWindow->pushGui(guiLatency);
}

void GuiMenu::openRetroachievementsSettings()
{
	GuiSettings *retroachievements = new GuiSettings(mWindow, _("RETROACHIEVEMENTS SETTINGS").c_str());

	// retroachievements_enable
	auto retroachievements_enabled = std::make_shared<SwitchComponent>(mWindow);
	retroachievements_enabled->setState(SystemConf::getInstance()->getBool("global.retroachievements"));
	retroachievements->addWithLabel(_("RETROACHIEVEMENTS"), retroachievements_enabled);
	retroachievements->addSaveFunc([retroachievements_enabled] { SystemConf::getInstance()->setBool("global.retroachievements", retroachievements_enabled->getState()); });

	// retroachievements_hardcore_mode
	auto retroachievements_hardcore_enabled = std::make_shared<SwitchComponent>(mWindow);
	retroachievements_hardcore_enabled->setState(SystemConf::getInstance()->getBool("global.retroachievements.hardcore"));
	retroachievements->addWithLabel(_("HARDCORE MODE"), retroachievements_hardcore_enabled);
	retroachievements->addSaveFunc([retroachievements_hardcore_enabled] { SystemConf::getInstance()->setBool("global.retroachievements.hardcore", retroachievements_hardcore_enabled->getState()); });

	// retroachievements_leaderboards
	auto retroachievements_leaderboards_enabled = std::make_shared<SwitchComponent>(mWindow);
	retroachievements_leaderboards_enabled->setState(SystemConf::getInstance()->getBool("global.retroachievements.leaderboards"));
	retroachievements->addWithLabel(_("LEADERBOARDS"), retroachievements_leaderboards_enabled);
	retroachievements->addSaveFunc([retroachievements_leaderboards_enabled] { SystemConf::getInstance()->setBool("global.retroachievements.leaderboards", retroachievements_leaderboards_enabled->getState()); });

	// retroachievements_verbose_mode
	auto retroachievements_verbose_enabled = std::make_shared<SwitchComponent>(mWindow);
	retroachievements_verbose_enabled->setState(SystemConf::getInstance()->getBool("global.retroachievements.verbose"));
	retroachievements->addWithLabel(_("VERBOSE MODE"), retroachievements_verbose_enabled);
	retroachievements->addSaveFunc([retroachievements_verbose_enabled] { SystemConf::getInstance()->setBool("global.retroachievements.verbose", retroachievements_verbose_enabled->getState()); });

	// retroachievements_automatic_screenshot
	auto retroachievements_screenshot_enabled = std::make_shared<SwitchComponent>(mWindow);
	retroachievements_screenshot_enabled->setState(SystemConf::getInstance()->getBool("global.retroachievements.screenshot"));
	retroachievements->addWithLabel(_("AUTOMATIC SCREENSHOT"), retroachievements_screenshot_enabled);
	retroachievements->addSaveFunc([retroachievements_screenshot_enabled] { SystemConf::getInstance()->setBool("global.retroachievements.screenshot", retroachievements_screenshot_enabled->getState()); });

	// retroachievements, username, password
	createInputTextRow(retroachievements, _("USERNAME"), "global.retroachievements.username", false);
	createInputTextRow(retroachievements, _("PASSWORD"), "global.retroachievements.password", true);

	// retroachievements_hardcore_mode
	auto retroachievements_menuitem = std::make_shared<SwitchComponent>(mWindow);
	retroachievements_menuitem->setState(Settings::getInstance()->getBool("RetroachievementsMenuitem"));
	retroachievements->addWithLabel(_("SHOW IN MAIN MENU"), retroachievements_menuitem);
	retroachievements->addSaveFunc([retroachievements_menuitem] { Settings::getInstance()->setBool("RetroachievementsMenuitem", retroachievements_menuitem->getState()); });


	mWindow->pushGui(retroachievements);
}

void GuiMenu::openNetplaySettings()
{
	GuiSettings* settings = new GuiSettings(mWindow, _("NETPLAY SETTINGS").c_str());

	settings->addGroup(_("SETTINGS"));

	// Enable
	auto enableNetplay = std::make_shared<SwitchComponent>(mWindow);
	enableNetplay->setState(SystemConf::getInstance()->getBool("global.netplay"));
	settings->addWithLabel(_("ENABLE NETPLAY"), enableNetplay);

	std::string port = SystemConf::getInstance()->get("global.netplay.port");
	if (port.empty())
		SystemConf::getInstance()->set("global.netplay.port", "55435");
			
	createInputTextRow(settings, _("NICKNAME"), "global.netplay.nickname", false);
	createInputTextRow(settings, _("PORT"), "global.netplay.port", false);

	// RELAY SERVER
	std::string mitm = SystemConf::getInstance()->get("global.netplay.relay");

	auto mitms = std::make_shared<OptionListComponent<std::string> >(mWindow, _("USE RELAY SERVER"), false);
	mitms->add(_("NONE"), "none", mitm.empty() || mitm == "none");
	mitms->add("NEW YORK", "nyc", mitm == "nyc");
	mitms->add("MADRID", "madrid", mitm == "madrid");
	mitms->add("MONTREAL", "montreal", mitm == "montreal");
	mitms->add("SAO PAULO", "saopaulo", mitm == "saopaulo");

	if (!mitms->hasSelection())
		mitms->selectFirstItem();

	settings->addWithLabel(_("USE RELAY SERVER"), mitms);

	settings->addGroup(_("GAME INDEXES"));

	// CheckOnStart
	auto checkOnStart = std::make_shared<SwitchComponent>(mWindow);
	checkOnStart->setState(Settings::getInstance()->getBool("NetPlayCheckIndexesAtStart"));
	settings->addWithLabel(_("CHECK MISSING INDEXES AT STARTUP"), checkOnStart);
	
	Window* window = mWindow;
	settings->addSaveFunc([enableNetplay, checkOnStart, mitms, window]
	{
		std::string mitm = mitms->getSelected();
		
		Settings::getInstance()->setBool("NetPlayCheckIndexesAtStart", checkOnStart->getState());
		SystemConf::getInstance()->set("global.netplay.relay", mitm.empty() ? "" : mitm);		

		if (SystemConf::getInstance()->setBool("global.netplay", enableNetplay->getState()))
		{
			if (!ThreadedHasher::isRunning() && enableNetplay->getState())
			{
				ThreadedHasher::start(window, false, true);
			}
		}
	});
	
	settings->addEntry(_("REINDEX ALL GAMES"), false, [this] { ThreadedHasher::start(mWindow, true); });
	settings->addEntry(_("INDEX MISSING GAMES"), false, [this] { ThreadedHasher::start(mWindow); });
	
	mWindow->pushGui(settings);
}

void GuiMenu::openGamesSettings_batocera() 
{
	Window* window = mWindow;

	auto s = new GuiSettings(mWindow, _("GAMES SETTINGS").c_str());

	if (SystemConf::getInstance()->get("system.es.menu") != "bartop")
	{
		s->addGroup(_("TOOLS"));

		// Game List Update
		s->addEntry(_("UPDATE GAMES LISTS"), false, [this, window] { updateGameLists(window); });

		if (SystemConf::getInstance()->getBool("global.retroachievements") &&
			!Settings::getInstance()->getBool("RetroachievementsMenuitem") &&
			SystemConf::getInstance()->get("global.retroachievements.username") != "")
		{
			s->addEntry(_("RETROACHIEVEMENTS").c_str(), true, [this] { GuiRetroAchievements::show(mWindow); }/*, "iconRetroachievements"*/);
		}
	}

	s->addGroup(_("DEFAULT SETTINGS"));

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
	
#ifdef _ENABLEEMUELEC
	// bezel
	auto bezel_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("ENABLE RA BEZELS"));
	bezel_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.bezel") != "0" && SystemConf::getInstance()->get("global.bezel") != "1");
	bezel_enabled->add(_("ON"), "1", SystemConf::getInstance()->get("global.bezel") == "1");
	bezel_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get("global.bezel") == "0");
	s->addWithLabel(_("ENABLE RA BEZELS"), bezel_enabled);
	
	//maxperf
	auto maxperf_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("ENABLE MAX PERFORMANCE"));
	maxperf_enabled->add(_("ON"), "1", SystemConf::getInstance()->get("global.maxperf") == "1" || SystemConf::getInstance()->get("global.maxperf") != "0");
	maxperf_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get("global.maxperf") == "0");
	s->addWithLabel(_("ENABLE MAX PERFORMANCE"), maxperf_enabled);
#endif

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
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SHADERS))
	{
		auto shaders_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SHADERS SET"), false);
		std::string currentShader = SystemConf::getInstance()->get("global.shaderset");
		if (currentShader.empty())
			currentShader = std::string("auto");

		shaders_choices->add(_("AUTO"), "auto", currentShader == "auto");
		shaders_choices->add(_("NONE"), "none", currentShader == "none");
#ifdef _ENABLEEMUELEC	
	std::string a;
	for(std::stringstream ss(getShOutput(R"(~/.emulationstation/scripts/getshaders.sh)")); getline(ss, a, ','); )
		shaders_choices->add(a, a, currentShader == a); // emuelec
#else
		shaders_choices->add(_("SCANLINES"), "scanlines", currentShader == "scanlines");
		shaders_choices->add(_("RETRO"), "retro", currentShader == "retro");
		shaders_choices->add(_("ENHANCED"), "enhanced", currentShader == "enhanced"); // batocera 5.23
		shaders_choices->add(_("CURVATURE"), "curvature", currentShader == "curvature"); // batocera 5.24
		shaders_choices->add(_("ZFAST"), "zfast", currentShader == "zfast"); // batocera 5.25
		shaders_choices->add(_("FLATTEN-GLOW"), "flatten-glow", currentShader == "flatten-glow"); // batocera 5.25
#endif
		s->addWithLabel(_("SHADERS SET"), shaders_choices);
		s->addSaveFunc([shaders_choices] { SystemConf::getInstance()->set("global.shaderset", shaders_choices->getSelected()); });
	}

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

#ifndef _ENABLEEMUELEC
	// decorations
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DECORATIONS))
	{		
		auto sets = GuiMenu::getDecorationsSets(ViewController::get()->getState().getSystem());
		if (sets.size() > 0)
		{
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

			for (auto it = decorations_item.begin(); it != decorations_item.end(); it++)
				decorations->add(*it, *it,
				(SystemConf::getInstance()->get("global.bezel") == *it) ||
					(SystemConf::getInstance()->get("global.bezel") == "none" && *it == _("NONE")) ||
					(SystemConf::getInstance()->get("global.bezel") == "" && *it == _("AUTO")));

			s->addWithLabel(_("DECORATION"), decorations);
			s->addSaveFunc([decorations]
			{
				SystemConf::getInstance()->set("global.bezel", decorations->getSelected() == _("NONE") ? "none" : decorations->getSelected() == _("AUTO") ? "" : decorations->getSelected());
			});
		}
	}
#endif	
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
	
	// Custom config for systems
	s->addEntry(_("PER SYSTEM ADVANCED CONFIGURATION"), true, [this, s, window]
	{
		s->save();
		GuiSettings* configuration = new GuiSettings(window, _("PER SYSTEM ADVANCED CONFIGURATION").c_str());

		// For each activated system
		std::vector<SystemData *> systems = SystemData::sSystemVector;
		for (auto system : systems)
		{
			if (system->isCollection() || system->isGroupSystem())
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

	if (SystemConf::getInstance()->get("system.es.menu") != "bartop")
	{
		s->addGroup(_("SYSTEM SETTINGS"));

		// Retroachievements
		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RETROACHIVEMENTS))
		{
			/*
			if (SystemConf::getInstance()->getBool("global.retroachievements") &&
				!Settings::getInstance()->getBool("RetroachievementsMenuitem") &&
				SystemConf::getInstance()->get("global.retroachievements.username") != "")
				s->addEntry(_("RETROACHIEVEMENTS").c_str(), true, [this] { GuiRetroAchievements::show(mWindow); }, "iconRetroachievements");
				*/
			s->addEntry(_("RETROACHIEVEMENTS SETTINGS"), true, [this] { openRetroachievementsSettings(); });
		}

		// Netplay
		if (SystemData::isNetplayActivated() && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::NETPLAY))
			s->addEntry(_("NETPLAY SETTINGS"), true, [this] { openNetplaySettings(); }, "iconNetplay");

		// Missing Bios
		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BIOSINFORMATION))
			s->addEntry(_("MISSING BIOS"), true, [this, s] { openMissingBiosSettings(); });

		// Game List Update
		// s->addEntry(_("UPDATE GAMES LISTS"), false, [this, window] { updateGameLists(window); });
	}
#ifdef _ENABLEEMUELEC
	s->addSaveFunc([maxperf_enabled]
	{
		SystemConf::getInstance()->set("global.maxperf", maxperf_enabled->getSelected());
	});
#endif

	s->addSaveFunc([smoothing_enabled, rewind_enabled, autosave_enabled] 
	{
		SystemConf::getInstance()->set("global.smooth", smoothing_enabled->getSelected());
		SystemConf::getInstance()->set("global.rewind", rewind_enabled->getSelected());
		SystemConf::getInstance()->set("global.autosave", autosave_enabled->getSelected());
	});

	mWindow->pushGui(s);
}

void GuiMenu::openMissingBiosSettings()
{
	auto menuTheme = ThemeData::getMenuTheme();

	GuiSettings *configuration = new GuiSettings(mWindow, _("MISSING BIOS").c_str());
	std::vector<BiosSystem> biosInformations = ApiSystem::getInstance()->getBiosInformations();

	if (biosInformations.size() == 0)
		configuration->addEntry(_("NO MISSING BIOS"));
	else
	{
		for (auto systemBios = biosInformations.begin(); systemBios != biosInformations.end(); systemBios++)
		{
			BiosSystem systemBiosData = (*systemBios);

			int invalidCount = 0;
			int missingCount = 0;

			for (auto biosFile = systemBiosData.bios.begin(); biosFile != systemBiosData.bios.end(); biosFile++)
			{
				if (biosFile->status == "INVALID")
					invalidCount++;

				if (biosFile->status == "MISSING")
					missingCount++;
			}

			std::string info = "";

			if (missingCount > 0)
			{
				if (info.length() > 0)
					info = info + " - ";

				info = info + _("MISSING") + " : " + std::to_string(missingCount);
			}

			if (invalidCount > 0)
			{
				if (info.length() > 0)
					info = info + " - ";

				info = info + _("INVALID") + " : " + std::to_string(invalidCount);
			}

#define INVALID_ICON _U("\uF071")
#define MISSING_ICON _U("\uF127")

			ComponentListRow row;

			auto icon = std::make_shared<TextComponent>(mWindow);
			icon->setText(invalidCount > 0 ? INVALID_ICON : MISSING_ICON);
			icon->setColor(menuTheme->Text.color);
			icon->setFont(menuTheme->Text.font);
			icon->setSize(menuTheme->Text.font->getLetterHeight() * 1.5f, 0);
			row.addElement(icon, false);

			auto spacer = std::make_shared<GuiComponent>(mWindow);
			spacer->setSize(14, 0);
			row.addElement(spacer, false);

			auto grid = std::make_shared<MultiLineMenuEntry>(mWindow, (*systemBios).name, info);
			row.addElement(grid, true);

			row.addElement(makeArrow(mWindow), false);

			auto bracket = std::make_shared<ImageComponent>(mWindow);
			bracket->setImage(menuTheme->Icons.arrow); // ":/arrow.svg");
			bracket->setColorShift(menuTheme->Text.color);
			bracket->setResize(0, round(menuTheme->Text.font->getLetterHeight()));

			row.makeAcceptInputHandler([this, systemBiosData]
			{
				GuiSettings* configurationInfo = new GuiSettings(mWindow, systemBiosData.name.c_str());
				for (auto biosFile = systemBiosData.bios.begin(); biosFile != systemBiosData.bios.end(); biosFile++)
				{
					auto theme = ThemeData::getMenuTheme();

					ComponentListRow biosFileRow;

					auto icon = std::make_shared<TextComponent>(mWindow);
					icon->setText(biosFile->status == "INVALID" ? INVALID_ICON : MISSING_ICON);
					icon->setColor(theme->Text.color);
					icon->setFont(theme->Text.font);
					icon->setSize(theme->Text.font->getLetterHeight() * 1.5f, 0);
					biosFileRow.addElement(icon, false);

					auto spacer = std::make_shared<GuiComponent>(mWindow);
					spacer->setSize(14, 0);
					biosFileRow.addElement(spacer, false);

					std::string status = _(biosFile->status.c_str()) + " - MD5 : " + biosFile->md5;

					auto line = std::make_shared<MultiLineMenuEntry>(mWindow, biosFile->path, status);
					biosFileRow.addElement(line, true);

					configurationInfo->addRow(biosFileRow);
				}
				mWindow->pushGui(configurationInfo);
			});

			configuration->addRow(row);
		}
	}
	mWindow->pushGui(configuration);
}

void GuiMenu::updateGameLists(Window* window, bool confirm)
{
	if (ThreadedScraper::isRunning())
	{
		window->pushGui(new GuiMsgBox(window, _("SCRAPING IS RUNNING. DO YOU WANT TO STOP IT ?"),
			_("YES"), [] { ThreadedScraper::stop(); }, 
			_("NO"), nullptr));

		return;
	}

	if (ThreadedHasher::isRunning())
	{
		window->pushGui(new GuiMsgBox(window, _("GAME HASHING IS RUNNING. DO YOU WANT TO STOP IT ?"),
			_("YES"), [] { ThreadedScraper::stop(); }, 
			_("NO"), nullptr));

		return;
	}
	
	if (!confirm)
	{
		reloadAllGames(window, true);
		return;
	}

	window->pushGui(new GuiMsgBox(window, _("REALLY UPDATE GAMES LISTS ?"), _("YES"), [window]
		{
			reloadAllGames(window, true);
		}, 
		_("NO"), nullptr));
}

void GuiMenu::openSystemEmulatorSettings(SystemData* system)
{
	auto theme = ThemeData::getMenuTheme();

	GuiSettings* s = new GuiSettings(mWindow, system->getFullName().c_str());

	auto emul_choice = std::make_shared<OptionListComponent<std::string>>(mWindow, _("Emulator"), false);
	auto core_choice = std::make_shared<OptionListComponent<std::string>>(mWindow, _("Core"), false);

	std::string currentEmul = Settings::getInstance()->getString(system->getName() + ".emulator");
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
		std::string currentCore = Settings::getInstance()->getString(system->getName() + ".core");
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

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BLUETOOTH))
	{
		// PAIR A BLUETOOTH CONTROLLER
		s->addEntry(_("PAIR A BLUETOOTH CONTROLLER"), false, [window] { ThreadedBluetooth::start(window); });

		// FORGET BLUETOOTH CONTROLLERS
		s->addEntry(_("FORGET A BLUETOOTH CONTROLLER"), false, [window, this, s] 
		{
			window->pushGui(new GuiBluetooth(window));
		});
	}

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
				dispNameSS << "#" << config->getDeviceIndex() << " ";
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

	if (systemTheme.empty() || showGridFeatures && system != NULL && theme->hasView("grid"))
		themeconfig->addGroup("GAMELIST STYLE");
	


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

	Utils::String::stringVector subsetNames = theme->getSubSetNames(viewName);

	// push appliesTo at end of list
	std::sort(subsetNames.begin(), subsetNames.end(), [themeSubSets](const std::string& a, const std::string& b) -> bool
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

			std::shared_ptr<OptionListComponent<std::string>> item = std::make_shared<OptionListComponent<std::string> >(mWindow, _(("THEME " + Utils::String::toUpper(subset)).c_str()), false);
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
				std::string displayName = themeColorSets.cbegin()->subSetDisplayName;
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

					if (!prefix.empty())
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
		auto defHid = Settings::getInstance()->getBool("ShowHiddenFiles") ? _("YES") : _("NO");
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

		// File extensions
		if (!system->isCollection() && !system->isGroupSystem())
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

		themeconfig->addEntry(_("RESET GAMELIST CUSTOMISATIONS"), false, [s, themeconfig, window]
		{
			Settings::getInstance()->setString("GamelistViewStyle", "");
			Settings::getInstance()->setString("DefaultGridSize", "");

			for (auto system : SystemData::sSystemVector)
			{
				system->setSystemViewMode("automatic", Vector2f(0, 0));

				Settings::getInstance()->setString(system->getName() + ".FavoritesFirst", "");
				Settings::getInstance()->setString(system->getName() + ".ShowHiddenFiles", "");
				Settings::getInstance()->setString(system->getName() + ".FolderViewMode", "");
			}
			
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
			if (themeconfig->getVariable("forceReloadGames"))
			{
				reloadAllGames(window, false);
			}
			else if (systemTheme.empty())
			{				
				CollectionSystemManager::get()->updateSystemsList();
				ViewController::get()->reloadAll(window);
				window->closeSplashScreen();

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

void GuiMenu::reloadAllGames(Window* window, bool deleteCurrentGui)
{
	Utils::FileSystem::FileSystemCacheActivator fsc;

	auto viewMode = ViewController::get()->getViewMode();
	auto systemName = ViewController::get()->getSelectedSystem()->getName();	

	window->renderSplashScreen(_("Loading..."));

	if (!deleteCurrentGui)
	{
		GuiComponent* topGui = window->peekGui();
		window->removeGui(topGui);
	}

	GuiComponent *gui;
	while ((gui = window->peekGui()) != NULL)
	{
		window->removeGui(gui);
		delete gui;
	}

	ViewController::init(window);
	CollectionSystemManager::deinit();
	CollectionSystemManager::init(window);
	SystemData::loadConfig(window);

	ViewController::get()->goToSystemView(systemName, true, viewMode);
	ViewController::get()->reloadAll(nullptr, false); // Avoid reloading themes a second time
	window->closeSplashScreen();

	window->pushGui(ViewController::get());
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

	s->addGroup(_("APPEARENCE"));

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
	
	s->addGroup(_("STARTUP SETTINGS"));

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

	// Select systems to hide
	auto hiddenSystems = Utils::String::split(Settings::getInstance()->getString("HiddenSystems"), ';');

	auto displayedSystems = std::make_shared<OptionListComponent<SystemData*>>(mWindow, _("SYSTEMS DISPLAYED"), true);


	if (SystemData::isManufacturerSupported() && Settings::getInstance()->getString("SortSystems") == "manufacturer")
	{
		std::string man;
		for (auto system : SystemData::sSystemVector)
		{
			if (system->isCollection() || system->isGroupChildSystem())
				continue;

			if (man != system->getSystemMetadata().manufacturer)
			{
				displayedSystems->addGroup(system->getSystemMetadata().manufacturer);
				man = system->getSystemMetadata().manufacturer;
			}

			displayedSystems->add(system->getFullName(), system, std::find(hiddenSystems.cbegin(), hiddenSystems.cend(), system->getName()) == hiddenSystems.cend());
		}
	}
	else
	{
		for (auto system : SystemData::sSystemVector)
			if (!system->isCollection() && !system->isGroupChildSystem())
				displayedSystems->add(system->getFullName(), system, std::find(hiddenSystems.cbegin(), hiddenSystems.cend(), system->getName()) == hiddenSystems.cend());
	}

	s->addWithLabel(_("SYSTEMS DISPLAYED"), displayedSystems);
	s->addSaveFunc([s, displayedSystems]
	{
		std::string hiddenSystems;

		std::vector<SystemData*> sys = displayedSystems->getSelectedObjects();

		for (auto system : SystemData::sSystemVector)
		{
			if (system->isCollection() || system->isGroupChildSystem())
				continue;

			if (std::find(sys.cbegin(), sys.cend(), system) == sys.cend())
			{
				if (hiddenSystems.empty())
					hiddenSystems = system->getName();
				else
					hiddenSystems = hiddenSystems + ";" + system->getName();
			}
		}

		if (Settings::getInstance()->setString("HiddenSystems", hiddenSystems))
		{
			Settings::getInstance()->saveFile();
			s->setVariable("reloadAll", true);
		}
	});

	s->addGroup(_("DISPLAY OPTIONS"));

	s->addEntry(_("SCREENSAVER SETTINGS"), true, std::bind(&GuiMenu::openScreensaverOptions, this));

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
	s->addSaveFunc([transition_style] { Settings::getInstance()->setString("TransitionStyle", transition_style->getSelected()); });

	// game transition style
	auto transitionOfGames_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("GAME LAUNCH TRANSITION"), false);

	for (auto it = transitions.begin(); it != transitions.end(); it++)
		transitionOfGames_style->add(_(it->c_str()), *it, Settings::getInstance()->getString("GameTransitionStyle") == *it);

	if (!transitionOfGames_style->hasSelection())
		transitionOfGames_style->selectFirstItem();

	s->addWithLabel(_("GAME LAUNCH TRANSITION"), transitionOfGames_style);
	s->addSaveFunc([transitionOfGames_style] { Settings::getInstance()->setString("GameTransitionStyle", transitionOfGames_style->getSelected()); });
	
	// carousel transition option
	auto move_carousel = std::make_shared<SwitchComponent>(mWindow);
	move_carousel->setState(Settings::getInstance()->getBool("MoveCarousel"));
	s->addWithLabel(_("CAROUSEL TRANSITIONS"), move_carousel);
	s->addSaveFunc([move_carousel] { Settings::getInstance()->setBool("MoveCarousel", move_carousel->getState()); });

	// quick system select (left/right in game list view)
	auto quick_sys_select = std::make_shared<SwitchComponent>(mWindow);
	quick_sys_select->setState(Settings::getInstance()->getBool("QuickSystemSelect"));
	s->addWithLabel(_("QUICK SYSTEM SELECT"), quick_sys_select);
	s->addSaveFunc([quick_sys_select] {
		Settings::getInstance()->setBool("QuickSystemSelect", quick_sys_select->getState());
	});

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

	// Battery indicator
	if (mWindow->getBatteryIndicator() && mWindow->getBatteryIndicator()->hasBattery())
	{
		auto batteryStatus = std::make_shared<SwitchComponent>(mWindow);
		batteryStatus->setState(Settings::getInstance()->getBool("ShowBatteryIndicator"));
		s->addWithLabel(_("SHOW BATTERY STATUS"), batteryStatus);
		s->addSaveFunc([batteryStatus] { Settings::getInstance()->setBool("ShowBatteryIndicator", batteryStatus->getState()); });
	}

	// Enable OSK (On-Screen-Keyboard)
	auto osk_enable = std::make_shared<SwitchComponent>(mWindow);
	osk_enable->setState(Settings::getInstance()->getBool("UseOSK"));
	s->addWithLabel(_("ON SCREEN KEYBOARD"), osk_enable);
	s->addSaveFunc([osk_enable] {
		Settings::getInstance()->setBool("UseOSK", osk_enable->getState()); });


#if defined(_WIN32)
	// Hide EmulationStation Window when running a game ( windows only )
	auto hideWindowScreen = std::make_shared<SwitchComponent>(mWindow);
	hideWindowScreen->setState(Settings::getInstance()->getBool("HideWindow"));
	s->addWithLabel(_("HIDE WHEN RUNNING A GAME"), hideWindowScreen);
	s->addSaveFunc([hideWindowScreen] { Settings::getInstance()->setBool("HideWindow", hideWindowScreen->getState()); });
#endif

	s->addGroup(_("GAMELIST OPTIONS"));

	// Show favorites first in gamelists
	auto favoritesFirstSwitch = std::make_shared<SwitchComponent>(mWindow);
	favoritesFirstSwitch->setState(Settings::getInstance()->getBool("FavoritesFirst"));
	s->addWithLabel(_("SHOW FAVORITES ON TOP"), favoritesFirstSwitch);
	s->addSaveFunc([s, favoritesFirstSwitch]
	{
		if (Settings::getInstance()->setBool("FavoritesFirst", favoritesFirstSwitch->getState()))
			s->setVariable("reloadAll", true);
	});

	// hidden files
	auto hidden_files = std::make_shared<SwitchComponent>(mWindow);
	hidden_files->setState(Settings::getInstance()->getBool("ShowHiddenFiles"));
	s->addWithLabel(_("SHOW HIDDEN FILES"), hidden_files);
	s->addSaveFunc([s, hidden_files]
	{
		if (Settings::getInstance()->setBool("ShowHiddenFiles", hidden_files->getState()))
			s->setVariable("reloadAll", true);
	});

	// Folder View Mode
	auto foldersBehavior = std::make_shared< OptionListComponent<std::string> >(mWindow, _("SHOW FOLDERS"), false);

	foldersBehavior->add(_("always"), "always", Settings::getInstance()->getString("FolderViewMode") == "always");
	foldersBehavior->add(_("never"), "never", Settings::getInstance()->getString("FolderViewMode") == "never");
	foldersBehavior->add(_("having multiple games"), "having multiple games", Settings::getInstance()->getString("FolderViewMode") == "having multiple games");

	s->addWithLabel(_("SHOW FOLDERS"), foldersBehavior);
	s->addSaveFunc([s, foldersBehavior]
	{
		if (Settings::getInstance()->setString("FolderViewMode", foldersBehavior->getSelected()))
			s->setVariable("reloadAll", true);
	});

	// Show parent folder
	auto parentFolder = std::make_shared<SwitchComponent>(mWindow);
	parentFolder->setState(Settings::getInstance()->getBool("ShowParentFolder"));
	s->addWithLabel(_("SHOW '..' PARENT FOLDER"), parentFolder);
	s->addSaveFunc([s, parentFolder]
	{
		Settings::getInstance()->setBool("ShowParentFolder", parentFolder->getState());
	});

	// filenames
	auto showFilesnames = std::make_shared<SwitchComponent>(mWindow);
	showFilesnames->setState(Settings::getInstance()->getBool("ShowFilenames"));
	s->addWithLabel(_("SHOW FILENAMES IN LISTS"), showFilesnames);
	s->addSaveFunc([showFilesnames, s]
	{ 
		if (Settings::getInstance()->setBool("ShowFilenames", showFilesnames->getState()))
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

		auto volumePopup = std::make_shared<SwitchComponent>(mWindow);
		volumePopup->setState(Settings::getInstance()->getBool("VolumePopup"));
		s->addWithLabel(_("SHOW OVERLAY WHEN VOLUME CHANGES"), volumePopup);
		s->addSaveFunc([volumePopup] { Settings::getInstance()->setBool("VolumePopup", volumePopup->getState()); });
	}

	s->addGroup(_("MUSIC"));

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

	auto videolowermusic = std::make_shared<SwitchComponent>(mWindow);
	videolowermusic->setState(Settings::getInstance()->getBool("VideoLowersMusic"));
	s->addWithLabel(_("LOWER MUSIC WHEN PLAYING VIDEO"), videolowermusic);
	s->addSaveFunc([videolowermusic] { Settings::getInstance()->setBool("VideoLowersMusic", videolowermusic->getState()); });

	s->addGroup(_("SOUNDS"));

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

void GuiMenu::openNetworkSettings_batocera(bool selectWifiEnable)
{
	bool baseWifiEnabled = SystemConf::getInstance()->getBool("wifi.enabled");

	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	Window *window = mWindow;

	auto s = new GuiSettings(mWindow, _("NETWORK SETTINGS").c_str());
	s->addGroup(_("INFORMATIONS"));

	auto status = std::make_shared<TextComponent>(mWindow, ApiSystem::getInstance()->ping() ? _("CONNECTED") : _("NOT CONNECTED"), font, color);
	s->addWithLabel(_("STATUS"), status);

	auto ip = std::make_shared<TextComponent>(mWindow, ApiSystem::getInstance()->getIpAdress(), font, color);
	s->addWithLabel(_("IP ADDRESS"), ip);

	s->addGroup(_("SETTINGS"));

#if !WIN32
#ifndef _ENABLEEMUELEC
	// Hostname
	createInputTextRow(s, _("HOSTNAME"), "system.hostname", false);
#endif
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
		createInputTextRow(s, _("WIFI SSID"), "wifi.ssid", false, false, &openWifiSettings);
		createInputTextRow(s, _("WIFI KEY"), "wifi.key", true);
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
			openNetworkSettings_batocera(true);
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
	if (!forceWin32Menu && Settings::getInstance()->getBool("ShowOnlyExit"))
	{
		quitES(QuitMode::QUIT);
		return;
	}
#endif

	auto s = new GuiSettings(window, _("QUIT").c_str());
	
	// Don't like one of the songs? Press next
	if (AudioManager::getInstance()->isSongPlaying())
		s->addEntry(_("SKIP TO NEXT SONG"), false, [window] {
			AudioManager::getInstance()->playRandomMusic(false);
		});

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
	
#ifdef _ENABLEEMUELEC
	s->addEntry(_("RESTART EMULATIONSTATION"), false, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY RESTART EMULATIONSTATION?"), _("YES"),
			[] {
    		   /*runSystemCommand("systemctl restart emustation.service", "", nullptr);*/
    		   Scripting::fireEvent("quit", "restart");
			   quitES(QuitMode::QUIT);
		}, _("NO"), nullptr));
	}, "iconRestart");

	s->addEntry(_("START RETROARCH"), false, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY START RETROARCH?"), _("YES"),
			[] {
			remove("/var/lock/start.games");
            runSystemCommand("touch /var/lock/start.retro", "", nullptr);
			runSystemCommand("systemctl start retroarch.service", "", nullptr);
			Scripting::fireEvent("quit", "retroarch");
			quitES(QuitMode::QUIT);
		}, _("NO"), nullptr));
	}, "iconControllers");
	
	s->addEntry(_("REBOOT FROM NAND"), false, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY REBOOT FROM NAND?"), _("YES"),
			[] {
			Scripting::fireEvent("quit", "nand");
			runSystemCommand("rebootfromnand", "", nullptr);
			runSystemCommand("sync", "", nullptr);
			runSystemCommand("systemctl reboot", "", nullptr);
			quitES(QuitMode::QUIT);
		}, _("NO"), nullptr));
	}, "iconAdvanced");

#endif

	s->addEntry(_("RESTART SYSTEM"), false, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY RESTART?"), 
			_("YES"), [] { quitES(QuitMode::REBOOT); }, 
			_("NO"), nullptr));
	}, "iconRestart");


	s->addEntry(_("SHUTDOWN SYSTEM"), false, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY SHUTDOWN?"), 
			_("YES"), [] { quitES(QuitMode::SHUTDOWN); }, 
			_("NO"), nullptr));
	}, "iconShutdown");

#ifndef _ENABLEEMUELEC
	s->addEntry(_("FAST SHUTDOWN SYSTEM"), false, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY SHUTDOWN WITHOUT SAVING METADATAS?"), 
			_("YES"), [] { quitES(QuitMode::FAST_SHUTDOWN); },
			_("NO"), nullptr));
	}, "iconFastShutdown");
#endif

#ifdef WIN32
	if (Settings::getInstance()->getBool("ShowExit"))
	{
		s->addEntry(_("QUIT EMULATIONSTATION"), false, [window] {
			window->pushGui(new GuiMsgBox(window, _("REALLY QUIT?"), 
				_("YES"), [] { quitES(QuitMode::QUIT); }, 
				_("NO"), nullptr));
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
	if (EsLocale::isRTL())
		lbl->setHorizontalAlignment(Alignment::ALIGN_RIGHT);

	row.addElement(lbl, true); // label

	std::string value = storeInSettings ? Settings::getInstance()->getString(settingsID) : SystemConf::getInstance()->get(settingsID);

	std::shared_ptr<TextComponent> ed = std::make_shared<TextComponent>(window, ((password && value != "") ? "*********" : value), font, color, ALIGN_RIGHT);
	if (EsLocale::isRTL())
		ed->setHorizontalAlignment(Alignment::ALIGN_LEFT);

	row.addElement(ed, true);

	auto spacer = std::make_shared<GuiComponent>(mWindow);
	spacer->setSize(Renderer::getScreenWidth() * 0.005f, 0);
	row.addElement(spacer, false);

	auto bracket = std::make_shared<ImageComponent>(mWindow);
	bracket->setImage(theme->Icons.arrow);
	bracket->setResize(Vector2f(0, lbl->getFont()->getLetterHeight()));

	if (EsLocale::isRTL())
		bracket->setFlipX(true);

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
		fileData->getCleanName(), // Utils::FileSystem::getFileName(fileData->getFileName()),
		fileData->getConfigurationName(),
		fileData->getSourceFileData()->getSystem(),
		fileData);
}

void GuiMenu::popSpecificConfigurationGui(Window* mWindow, std::string title, std::string configName, SystemData *systemData, FileData* fileData, bool selectCoreLine) 
{
	// The system configuration
	GuiSettings* systemConfiguration = new GuiSettings(mWindow, title.c_str());

#ifndef _ENABLEEMUELEC
	if (fileData != nullptr)
		systemConfiguration->setSubTitle(systemData->getFullName());

	std::string currentEmulator = fileData != nullptr ? fileData->getEmulator(false) : SystemConf::getInstance()->get(configName + ".emulator");
	std::string currentCore = fileData != nullptr ? fileData->getCore(false) : SystemConf::getInstance()->get(configName + ".core");

	if (systemData->hasEmulatorSelection())
	{
		std::string defaultCore = currentCore;
		if (defaultCore.empty() || defaultCore == "auto")
			defaultCore = systemData->getDefaultCore(currentEmulator);

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
					bool selected = (emul.name == currentEmulator && core.name == defaultCore);

					if (emul.name == core.name)
						emulChoice->add(emul.name, emul.name + "/" + core.name, selected);
					else
						emulChoice->add(emul.name + " / " + Utils::String::replace(core.name, "_", " "), emul.name + "/" + core.name, selected);
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

	// Screen ratio choice
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::ratio))
	{
		auto ratio_choice = createRatioOptionList(mWindow, configName);
		systemConfiguration->addWithLabel(_("GAME RATIO"), ratio_choice);
		systemConfiguration->addSaveFunc([configName, ratio_choice] { SystemConf::getInstance()->set(configName + ".ratio", ratio_choice->getSelected()); });
	}

	// video resolution mode
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::videomode))
	{
		auto videoResolutionMode_choice = createVideoResolutionModeOptionList(mWindow, configName);
		systemConfiguration->addWithLabel(_("VIDEO MODE"), videoResolutionMode_choice);
		systemConfiguration->addSaveFunc([configName, videoResolutionMode_choice] { SystemConf::getInstance()->set(configName + ".videomode", videoResolutionMode_choice->getSelected()); });
	}

	// smoothing
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::smooth))
	{
		auto smoothing_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SMOOTH GAMES"));
		smoothing_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".smooth") != "0" && SystemConf::getInstance()->get(configName + ".smooth") != "1");
		smoothing_enabled->add(_("ON"), "1", SystemConf::getInstance()->get(configName + ".smooth") == "1");
		smoothing_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get(configName + ".smooth") == "0");
		systemConfiguration->addWithLabel(_("SMOOTH GAMES"), smoothing_enabled);
		systemConfiguration->addSaveFunc([configName, smoothing_enabled] { SystemConf::getInstance()->set(configName + ".smooth", smoothing_enabled->getSelected()); });
	}

	// rewind
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::rewind))
	{
		auto rewind_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("REWIND"));
		rewind_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".rewind") != "0" && SystemConf::getInstance()->get(configName + ".rewind") != "1");
		rewind_enabled->add(_("ON"), "1", SystemConf::getInstance()->get(configName + ".rewind") == "1");
		rewind_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get(configName + ".rewind") == "0");
		systemConfiguration->addWithLabel(_("REWIND"), rewind_enabled);
		systemConfiguration->addSaveFunc([configName, rewind_enabled] { SystemConf::getInstance()->set(configName + ".rewind", rewind_enabled->getSelected()); });
	}

	// autosave
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::autosave))
	{
		auto autosave_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("AUTO SAVE/LOAD"));
		autosave_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".autosave") != "0" && SystemConf::getInstance()->get(configName + ".autosave") != "1");
		autosave_enabled->add(_("ON"), "1", SystemConf::getInstance()->get(configName + ".autosave") == "1");
		autosave_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get(configName + ".autosave") == "0");
		systemConfiguration->addWithLabel(_("AUTO SAVE/LOAD"), autosave_enabled);
		systemConfiguration->addSaveFunc([configName, autosave_enabled] { SystemConf::getInstance()->set(configName + ".autosave", autosave_enabled->getSelected()); });
	}

	// Shaders preset
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SHADERS) &&
		systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::shaders))
	{
		auto shaders_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SHADERS SET"), false);
		std::string currentShader = SystemConf::getInstance()->get(configName + ".shaderset");
		if (currentShader.empty())
			currentShader = std::string("auto");

		shaders_choices->add(_("AUTO"), "auto", currentShader == "auto");
		shaders_choices->add(_("NONE"), "none", currentShader == "none");
		shaders_choices->add(_("SCANLINES"), "scanlines", currentShader == "scanlines");
		shaders_choices->add(_("RETRO"), "retro", currentShader == "retro");
		shaders_choices->add(_("ENHANCED"), "enhanced", currentShader == "enhanced"); // batocera 5.23
		shaders_choices->add(_("CURVATURE"), "curvature", currentShader == "curvature"); // batocera 5.24
		shaders_choices->add(_("ZFAST"), "zfast", currentShader == "zfast"); // batocera 5.25
		shaders_choices->add(_("FLATTEN-GLOW"), "flatten-glow", currentShader == "flatten-glow"); // batocera 5.25
		systemConfiguration->addWithLabel(_("SHADERS SET"), shaders_choices);
		systemConfiguration->addSaveFunc([configName, shaders_choices] { SystemConf::getInstance()->set(configName + ".shaderset", shaders_choices->getSelected()); });
	}

	// Integer scale
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::pixel_perfect))
	{
		auto integerscale_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INTEGER SCALE (PIXEL PERFECT)"));
		integerscale_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".integerscale") != "0" && SystemConf::getInstance()->get(configName + ".integerscale") != "1");
		integerscale_enabled->add(_("ON"), "1", SystemConf::getInstance()->get(configName + ".integerscale") == "1");
		integerscale_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get(configName + ".integerscale") == "0");
		systemConfiguration->addWithLabel(_("INTEGER SCALE (PIXEL PERFECT)"), integerscale_enabled);
		systemConfiguration->addSaveFunc([integerscale_enabled, configName] { SystemConf::getInstance()->set(configName + ".integerscale", integerscale_enabled->getSelected()); });
	}

	// decorations
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DECORATIONS))
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::decoration))
	{
		Window* window = mWindow;
		auto sets = GuiMenu::getDecorationsSets(systemData);
		if (sets.size() > 0)
		{
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
			systemConfiguration->addSaveFunc([decorations, configName]
			{
				SystemConf::getInstance()->set(configName + ".bezel", decorations->getSelected() == _("NONE") ? "none" : decorations->getSelected() == _("AUTO") ? "" : decorations->getSelected());
			});
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
		
		if (SystemData::es_features_loaded || (!SystemData::es_features_loaded && (systemData->getName() == "gb" || systemData->getName() == "gbc" || systemData->getName() == "gb2players" || systemData->getName() == "gbc2players")))  // only for gb, gbc and gb2players
		{
			systemConfiguration->addWithLabel(_("COLORIZATION"), colorizations_choices);
			systemConfiguration->addSaveFunc([colorizations_choices, configName] { SystemConf::getInstance()->set(configName + "-renderer.colorization", colorizations_choices->getSelected()); });
		}		
	}

	// ps2 full boot
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::fullboot))
	{
		auto fullboot_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("FULL BOOT"));
		fullboot_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".fullboot") != "0" && SystemConf::getInstance()->get(configName + ".fullboot") != "1");
		fullboot_enabled->add(_("ON"), "1", SystemConf::getInstance()->get(configName + ".fullboot") == "1");
		fullboot_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get(configName + ".fullboot") == "0");

		if (SystemData::es_features_loaded || (!SystemData::es_features_loaded && systemData->getName() == "ps2")) // only for ps2			
		{
			systemConfiguration->addWithLabel(_("FULL BOOT"), fullboot_enabled);
			systemConfiguration->addSaveFunc([fullboot_enabled, configName] { SystemConf::getInstance()->set(configName + ".fullboot", fullboot_enabled->getSelected()); });
		}
	}

	// wii emulated wiimotes
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::emulated_wiimotes))
	{
		auto emulatedwiimotes_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("EMULATED WIIMOTES"));
		emulatedwiimotes_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".emulatedwiimotes") != "0" && SystemConf::getInstance()->get(configName + ".emulatedwiimotes") != "1");
		emulatedwiimotes_enabled->add(_("ON"), "1", SystemConf::getInstance()->get(configName + ".emulatedwiimotes") == "1");
		emulatedwiimotes_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get(configName + ".emulatedwiimotes") == "0");

		if (SystemData::es_features_loaded || (!SystemData::es_features_loaded && systemData->getName() == "wii"))  // only for wii
		{
			systemConfiguration->addWithLabel(_("EMULATED WIIMOTES"), emulatedwiimotes_enabled);
			systemConfiguration->addSaveFunc([emulatedwiimotes_enabled, configName] { SystemConf::getInstance()->set(configName + ".emulatedwiimotes", emulatedwiimotes_enabled->getSelected()); });
		}
	}

	// citra change screen layout
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::screen_layout))
	{
		auto changescreen_layout = std::make_shared<OptionListComponent<std::string>>(mWindow, _("CHANGE SCREEN LAYOUT"));
		changescreen_layout->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".layout_option") != "2" && SystemConf::getInstance()->get(configName + ".layout_option") != "3");
		changescreen_layout->add(_("LARGE SCREEN"), "2", SystemConf::getInstance()->get(configName + ".layout_option") == "2");
		changescreen_layout->add(_("SIDE BY SIDE"), "3", SystemConf::getInstance()->get(configName + ".layout_option") == "3");

		if (SystemData::es_features_loaded || (!SystemData::es_features_loaded && systemData->getName() == "3ds"))  // only for 3ds
		{
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

		if (SystemData::es_features_loaded || (!SystemData::es_features_loaded && (systemData->getName() == "psp" || systemData->getName() == "wii" || systemData->getName() == "gamecube"))) // only for psp, wii, gamecube
		{
			systemConfiguration->addWithLabel(_("INTERNAL RESOLUTION"), internalresolution);
			systemConfiguration->addSaveFunc([internalresolution, configName] { SystemConf::getInstance()->set(configName + ".internalresolution", internalresolution->getSelected()); });
		}
	}
#else
  std::string a;
		if (fileData != nullptr)
		systemConfiguration->setSubTitle(systemData->getFullName());

		std::string currentEmulator = fileData != nullptr ? fileData->getEmulator(false) : getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".emulator' ");
		std::string currentCore = fileData != nullptr ? fileData->getCore(false) : getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".core' ");

	if (systemData->hasEmulatorSelection())
	{
		std::string defaultCore = currentCore;
		if (defaultCore.empty() || defaultCore == "auto")
			defaultCore = systemData->getDefaultCore(currentEmulator);

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
					bool selected = (emul.name == currentEmulator && core.name == defaultCore);

					if (emul.name == core.name)
						emulChoice->add(emul.name, emul.name + "/" + core.name, selected);
					else
						emulChoice->add(emul.name + " / " + Utils::String::replace(core.name, "_", " "), emul.name + "/" + core.name, selected);
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

				getShOutput(R"(/emuelec/scripts/setemu.sh set ')" + configName + ".emulator' " + newEmul);
				getShOutput(R"(/emuelec/scripts/setemu.sh set ')" + configName + ".core' " + newCore);
				
			}

			popSpecificConfigurationGui(mWindow, title, configName, systemData, fileData);
			delete systemConfiguration;

		});

		systemConfiguration->addWithLabel(_("Emulator"), emulChoice);
	}

	// Screen ratio choice
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::ratio))
	{
		auto ratio_choice = createRatioOptionList(mWindow, configName);
		systemConfiguration->addWithLabel(_("GAME RATIO"), ratio_choice);
		systemConfiguration->addSaveFunc([configName, ratio_choice] { getShOutput(R"(/emuelec/scripts/setemu.sh set ')" + configName + ".ratio' " + ratio_choice->getSelected()); });
	}

	// bezel
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::decoration))
	{
		auto bezel_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("BEZEL"));
		bezel_enabled->add(_("AUTO"), "auto", getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".bezel'") != "0" && getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".bezel'") != "1");
		bezel_enabled->add(_("YES"), "1", getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".bezel'") == "1");
		bezel_enabled->add(_("NO"), "0", getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".bezel'") == "0");
		systemConfiguration->addWithLabel(_("BEZEL"), bezel_enabled);
		systemConfiguration->addSaveFunc([configName, bezel_enabled] { getShOutput(R"(/emuelec/scripts/setemu.sh set ')" + configName + ".bezel' " + bezel_enabled->getSelected()); });
	}
	
	// maxperf
		auto maxperf_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("ENABLE MAX PERFORMANCE"));
		maxperf_enabled->add(_("AUTO"), "auto", getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".maxperf'") != "0" && getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".maxperf'") != "1");
		maxperf_enabled->add(_("YES"), "1", getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".maxperf'") == "1");
		maxperf_enabled->add(_("NO"), "0", getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".maxperf'") == "0");
		systemConfiguration->addWithLabel(_("ENABLE MAX PERFORMANCE"), maxperf_enabled);
		systemConfiguration->addSaveFunc([configName, maxperf_enabled] { getShOutput(R"(/emuelec/scripts/setemu.sh set ')" + configName + ".maxperf' " + maxperf_enabled->getSelected()); });
	
	// smoothing
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::smooth))
	{
		auto smoothing_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SMOOTH GAMES"));
		smoothing_enabled->add(_("AUTO"), "auto", getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".smooth'") != "0" && getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".smooth'") != "1");
		smoothing_enabled->add(_("ON"), "1", getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".smooth'") == "1");
		smoothing_enabled->add(_("OFF"), "0", getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".smooth'") == "0");
		systemConfiguration->addWithLabel(_("SMOOTH GAMES"), smoothing_enabled);
		systemConfiguration->addSaveFunc([configName, smoothing_enabled] { getShOutput(R"(/emuelec/scripts/setemu.sh set ')" + configName + ".smooth' " + smoothing_enabled->getSelected()); });
	}

	// rewind
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::rewind))
	{
		auto rewind_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("REWIND"));
		rewind_enabled->add(_("AUTO"), "auto", getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".rewind'") != "0" && getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".rewind'") != "1");
		rewind_enabled->add(_("ON"), "1", getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".rewind'") == "1");
		rewind_enabled->add(_("OFF"), "0", getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".rewind'") == "0");
		systemConfiguration->addWithLabel(_("REWIND"), rewind_enabled);
		systemConfiguration->addSaveFunc([configName, rewind_enabled] { getShOutput(R"(/emuelec/scripts/setemu.sh set ')" + configName + ".rewind' " + rewind_enabled->getSelected());	});
	}

	// autosave
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::autosave))
	{
		auto autosave_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("AUTO SAVE/LOAD"));
		autosave_enabled->add(_("AUTO"), "auto", getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".autosave'") != "0" && getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".autosave'") != "1");
		autosave_enabled->add(_("ON"), "1", getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".autosave'") == "1");
		autosave_enabled->add(_("OFF"), "0", getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".autosave'") == "0");
		systemConfiguration->addWithLabel(_("AUTO SAVE/LOAD"), autosave_enabled);
		systemConfiguration->addSaveFunc([configName, autosave_enabled] { getShOutput(R"(/emuelec/scripts/setemu.sh set ')" + configName + ".autosave' " + autosave_enabled->getSelected()); });
	}

	// Shaders preset
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SHADERS) &&
		systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::shaders))
	{
		auto shaders_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SHADERS SET"),false);
		std::string currentShader = getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".shaderset'");
		if (currentShader.empty()) {
			currentShader = std::string("auto");
		}

		shaders_choices->add(_("AUTO"), "auto", currentShader == "auto");
		shaders_choices->add(_("NONE"), "none", currentShader == "none");
		for(std::stringstream ss(getShOutput(R"(~/.emulationstation/scripts/getshaders.sh)")); getline(ss, a, ','); )
		shaders_choices->add(a, a, currentShader == a); // emuelec
		systemConfiguration->addWithLabel(_("SHADERS SET"), shaders_choices);
		systemConfiguration->addSaveFunc([configName, shaders_choices] { getShOutput(R"(/emuelec/scripts/setemu.sh set ')" + configName + ".shaderset' " + shaders_choices->getSelected()); });
	}

	// Integer scale
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::pixel_perfect))
	{
		auto integerscale_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INTEGER SCALE (PIXEL PERFECT)"));
		integerscale_enabled->add(_("AUTO"), "auto", getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".integerscale'") != "0" && getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".integerscale'") != "1");
		integerscale_enabled->add(_("ON"), "1", getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".integerscale'") == "1");
		integerscale_enabled->add(_("OFF"), "0", getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + ".integerscale'") == "0");
		systemConfiguration->addWithLabel(_("INTEGER SCALE (PIXEL PERFECT)"), integerscale_enabled);
		systemConfiguration->addSaveFunc([integerscale_enabled, configName] { getShOutput(R"(/emuelec/scripts/setemu.sh set ')" + configName + ".integerscale' " + integerscale_enabled->getSelected()); });
	}

	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::latency_reduction))	
		systemConfiguration->addEntry(_("LATENCY REDUCTION"), true, [mWindow, configName] { openLatencyReductionConfiguration(mWindow, configName); });
		
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::colorization))
	{
		// gameboy colorize
		auto colorizations_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("COLORIZATION"), false);
		std::string currentColorization = getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configName + "-renderer.colorization'");
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
	for (int i = 0; i < n_all_gambate_gc_colors_modes; i++) {
		colorizations_choices->add(all_gambate_gc_colors_modes[i], all_gambate_gc_colors_modes[i], currentColorization == std::string(all_gambate_gc_colors_modes[i]));
	}
	if (systemData->getName() == "gb" || systemData->getName() == "gbc" || systemData->getName() == "gb2players" || systemData->getName() == "gbc2players" || systemData->getName() == "gbh" || systemData->getName() == "gbch") // only for gb, gbc and gb2players
		{
			systemConfiguration->addWithLabel(_("COLORIZATION"), colorizations_choices);
			systemConfiguration->addSaveFunc([colorizations_choices, configName] { getShOutput(R"(/emuelec/scripts/setemu.sh set ')" + configName + "-renderer.colorization' " + colorizations_choices->getSelected()); });
		}		
	}
#endif 
	std::vector<CustomFeature> customFeatures = systemData->getCustomFeatures(currentEmulator, currentCore);
	for (auto feat : customFeatures)
	{
		std::string storageName = configName + "." + feat.value;
		std::string storedValue = SystemConf::getInstance()->get(storageName);

		auto cf = std::make_shared<OptionListComponent<std::string>>(mWindow, _(feat.name.c_str()));
		cf->add(_("AUTO"), "", storedValue.empty() || storedValue == "auto");

		for(auto fval : feat.choices)
			cf->add(_(fval.name.c_str()), fval.value, storedValue == fval.value);

		if (!cf->hasSelection())
			cf->selectFirstItem();

		systemConfiguration->addWithLabel(_(feat.name.c_str()), cf);
		systemConfiguration->addSaveFunc([cf, storageName] 
		{			
			SystemConf::getInstance()->set(storageName, cf->getSelected());
		});
	}

	mWindow->pushGui(systemConfiguration);
}

std::shared_ptr<OptionListComponent<std::string>> GuiMenu::createRatioOptionList(Window *window, std::string configname)
{
	auto ratio_choice = std::make_shared<OptionListComponent<std::string> >(window, _("GAME RATIO"), false);
#ifdef _ENABLEEMUELEC 
    std::string currentRatio;
	if (configname == "global") {
	currentRatio = SystemConf::getInstance()->get(configname + ".ratio");
	} else {
   currentRatio = getShOutput(R"(/emuelec/scripts/setemu.sh get ')" + configname + ".ratio'");
	}	
#else
	std::string currentRatio = SystemConf::getInstance()->get(configname + ".ratio");
#endif
	if (currentRatio.empty())
		currentRatio = std::string("auto");

	std::map<std::string, std::string> *ratioMap = LibretroRatio::getInstance()->getRatio();
	for (auto ratio = ratioMap->begin(); ratio != ratioMap->end(); ratio++)
		ratio_choice->add(_(ratio->first.c_str()), ratio->second, currentRatio == ratio->second);	

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

	
#if WIN32
	std::vector<std::string> paths = 
	{
		Utils::FileSystem::getEsConfigPath() + "/decorations" // for win32 testings
	};

	std::string win32path = Win32ApiSystem::getEmulatorLauncherPath("system.decorations");
	if (!win32path.empty())
		paths[0] = win32path; 
	
	win32path = Win32ApiSystem::getEmulatorLauncherPath("decorations");
	if (!win32path.empty())
		paths.push_back(win32path);


#else
#ifdef _ENABLEEMUELEC
	std::vector<std::string> paths = {
		"/storage/roms/bezels",
		"/tmp/overlays/bezels"
	};
#else
	std::vector<std::string> paths = {
		"/usr/share/batocera/datainit/decorations",
		"/userdata/decorations"
	};
#endif
#endif


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

			window->pushGui(new GuiMsgBox(window, _("ARE YOU SURE YOU WANT TO FORMAT THIS DRIVE ?"), _("YES"), [s, window, disk, fs]
			{
				ThreadedFormatter::start(window, disk, fs);
				s->close();
			}, _("NO"), nullptr));
			
		});

	mWindow->pushGui(s);
}
