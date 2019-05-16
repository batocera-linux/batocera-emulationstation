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
#include "guis/GuiSettings.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "EmulationStation.h"
#include "Scripting.h"
#include "SystemData.h"
#include "VolumeControl.h"
#include <SDL_events.h>
#include <algorithm>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/classification.hpp>
#include "SystemConf.h"
#include "ApiSystem.h"
#include "InputManager.h"
#include "AudioManager.h"
#include <LibretroRatio.h>
#include "GuiLoading.h"
#include "guis/GuiAutoScrape.h"
#include "guis/GuiUpdate.h"
#include "guis/GuiInstallStart.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "guis/GuiBackupStart.h"
#include "guis/GuiTextEditPopup.h"

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
  if (SystemConf::getInstance()->get("kodi.enabled") != "0") {
    addEntry(_("KODI MEDIA CENTER").c_str(), 0x777777FF, true, [this] { openKodiLauncher_batocera(); });
  }
#endif

  if (isFullUI &&
      SystemConf::getInstance()->get("global.retroachievements") == "1" &&
      SystemConf::getInstance()->get("global.retroachievements.username") != "")
    addEntry(_("RETROACHIEVEMENTS").c_str(), 0x777777FF, true, [this] { openRetroAchievements_batocera(); });

  // GAMES SETTINGS
  if (isFullUI)
    addEntry(_("GAMES SETTINGS").c_str(), 0x777777FF, true, [this] { openGamesSettings_batocera(); });

  // CONTROLLERS SETTINGS
  if (isFullUI)
    addEntry(_("CONTROLLERS SETTINGS").c_str(), 0x777777FF, true, [this] { openControllersSettings_batocera(); });
  
  if (isFullUI)
    addEntry(_("UI SETTINGS").c_str(), 0x777777FF, true, [this] { openUISettings_batocera(); });

  if (isFullUI)
    addEntry(_("SOUND SETTINGS").c_str(), 0x777777FF, true, [this] { openSoundSettings_batocera(); });

  if (isFullUI)
    addEntry(_("NETWORK SETTINGS").c_str(), 0x777777FF, true, [this] { openNetworkSettings_batocera(); });

  if (isFullUI)
    addEntry(_("SCRAPE").c_str(), 0x777777FF, true, [this] { openScraperSettings_batocera(); });

  // SYSTEM
  if (isFullUI) {
    addEntry(_("SYSTEM SETTINGS").c_str(), 0x777777FF, true, [this] { openSystemSettings_batocera(); });
  } else {
    addEntry(_("INFORMATIONS").c_str(), 0x777777FF, true, [this] { openSystemInformations_batocera(); });
  }

  addEntry(_("QUIT").c_str(), 0x777777FF, true, [this] { openQuitMenu_batocera(); });


  //addEntry("SOUND SETTINGS", 0x777777FF, true, [this] { openSoundSettings(); });

  //if (isFullUI)
  //addEntry("UI SETTINGS", 0x777777FF, true, [this] { openUISettings(); });

  // batocera
  //if (isFullUI)
  //	addEntry("GAME COLLECTION SETTINGS", 0x777777FF, true, [this] { openCollectionSystemSettings(); });

  //if (isFullUI)
  //addEntry("OTHER SETTINGS", 0x777777FF, true, [this] { openOtherSettings(); });

  // batocera
  //if (isFullUI)
  //	addEntry("CONFIGURE INPUT", 0x777777FF, true, [this] { openConfigInput(); });
  
  // batocera
  //addEntry("QUIT", 0x777777FF, true, [this] {openQuitMenu(); });

  addChild(&mMenu);
  //addVersionInfo(); // batocera
  setSize(mMenu.getSize());
  setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiMenu::openScraperSettings()
{
	auto s = new GuiSettings(mWindow, "SCRAPER");

	// scrape from
	auto scraper_list = std::make_shared< OptionListComponent< std::string > >(mWindow, "SCRAPE FROM", false);
	std::vector<std::string> scrapers = getScraperList();

	// Select either the first entry of the one read from the settings, just in case the scraper from settings has vanished.
	for(auto it = scrapers.cbegin(); it != scrapers.cend(); it++)
		scraper_list->add(*it, *it, *it == Settings::getInstance()->getString("Scraper"));

	s->addWithLabel(_("SCRAPE FROM"), scraper_list); // batocera
	s->addSaveFunc([scraper_list] { Settings::getInstance()->setString("Scraper", scraper_list->getSelected()); });

	// scrape ratings
	auto scrape_ratings = std::make_shared<SwitchComponent>(mWindow);
	scrape_ratings->setState(Settings::getInstance()->getBool("ScrapeRatings"));
	s->addWithLabel(_("SCRAPE RATINGS"), scrape_ratings); // batocera
	s->addSaveFunc([scrape_ratings] { Settings::getInstance()->setBool("ScrapeRatings", scrape_ratings->getState()); });

	// scrape now
	ComponentListRow row;
	auto openScrapeNow = [this] { mWindow->pushGui(new GuiScraperStart(mWindow)); };
	std::function<void()> openAndSave = openScrapeNow;
	openAndSave = [s, openAndSave] { s->save(); openAndSave(); };
	row.makeAcceptInputHandler(openAndSave);

	auto scrape_now = std::make_shared<TextComponent>(mWindow, _("SCRAPE NOW"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF); // batocera
	auto bracket = makeArrow(mWindow);
	row.addElement(scrape_now, true);
	row.addElement(bracket, false);
	s->addRow(row);

	mWindow->pushGui(s);
}

void GuiMenu::openSoundSettings()
{
	auto s = new GuiSettings(mWindow, "SOUND SETTINGS");

	// volume
	auto volume = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
	volume->setValue((float)VolumeControl::getInstance()->getVolume());
	s->addWithLabel("SYSTEM VOLUME", volume);
	s->addSaveFunc([volume] { VolumeControl::getInstance()->setVolume((int)Math::round(volume->getValue())); });

	if (UIModeController::getInstance()->isUIModeFull())
	{
#if defined(__linux__)
		// audio card
		auto audio_card = std::make_shared< OptionListComponent<std::string> >(mWindow, "AUDIO CARD", false);
		std::vector<std::string> audio_cards;
	#ifdef _RPI_
		// RPi Specific  Audio Cards
		audio_cards.push_back("local");
		audio_cards.push_back("hdmi");
		audio_cards.push_back("both");
	#endif
		audio_cards.push_back("default");
		audio_cards.push_back("sysdefault");
		audio_cards.push_back("dmix");
		audio_cards.push_back("hw");
		audio_cards.push_back("plughw");
		audio_cards.push_back("null");
		if (Settings::getInstance()->getString("AudioCard") != "") {
			if(std::find(audio_cards.begin(), audio_cards.end(), Settings::getInstance()->getString("AudioCard")) == audio_cards.end()) {
				audio_cards.push_back(Settings::getInstance()->getString("AudioCard"));
			}
		}
		for(auto ac = audio_cards.cbegin(); ac != audio_cards.cend(); ac++)
			audio_card->add(*ac, *ac, Settings::getInstance()->getString("AudioCard") == *ac);
		s->addWithLabel("AUDIO CARD", audio_card);
		s->addSaveFunc([audio_card] {
			Settings::getInstance()->setString("AudioCard", audio_card->getSelected());
			VolumeControl::getInstance()->deinit();
			VolumeControl::getInstance()->init();
		});

		// volume control device
		auto vol_dev = std::make_shared< OptionListComponent<std::string> >(mWindow, "AUDIO DEVICE", false);
		std::vector<std::string> transitions;
		transitions.push_back("PCM");
		transitions.push_back("Speaker");
		transitions.push_back("Master");
		transitions.push_back("Digital");
		transitions.push_back("Analogue");
		if (Settings::getInstance()->getString("AudioDevice") != "") {
			if(std::find(transitions.begin(), transitions.end(), Settings::getInstance()->getString("AudioDevice")) == transitions.end()) {
				transitions.push_back(Settings::getInstance()->getString("AudioDevice"));
			}
		}
		for(auto it = transitions.cbegin(); it != transitions.cend(); it++)
			vol_dev->add(*it, *it, Settings::getInstance()->getString("AudioDevice") == *it);
		s->addWithLabel("AUDIO DEVICE", vol_dev);
		s->addSaveFunc([vol_dev] {
			Settings::getInstance()->setString("AudioDevice", vol_dev->getSelected());
			VolumeControl::getInstance()->deinit();
			VolumeControl::getInstance()->init();
		});
#endif

		// disable sounds
		auto sounds_enabled = std::make_shared<SwitchComponent>(mWindow);
		sounds_enabled->setState(Settings::getInstance()->getBool("EnableSounds"));
		s->addWithLabel("ENABLE NAVIGATION SOUNDS", sounds_enabled);
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
		s->addWithLabel("ENABLE VIDEO AUDIO", video_audio);
		s->addSaveFunc([video_audio] { Settings::getInstance()->setBool("VideoAudio", video_audio->getState()); });

#ifdef _RPI_
		// OMX player Audio Device
		auto omx_audio_dev = std::make_shared< OptionListComponent<std::string> >(mWindow, "OMX PLAYER AUDIO DEVICE", false);
		std::vector<std::string> omx_cards;
		// RPi Specific  Audio Cards
		omx_cards.push_back("local");
		omx_cards.push_back("hdmi");
		omx_cards.push_back("both");
		omx_cards.push_back("alsa:hw:0,0");
		omx_cards.push_back("alsa:hw:1,0");
		if (Settings::getInstance()->getString("OMXAudioDev") != "") {
			if (std::find(omx_cards.begin(), omx_cards.end(), Settings::getInstance()->getString("OMXAudioDev")) == omx_cards.end()) {
				omx_cards.push_back(Settings::getInstance()->getString("OMXAudioDev"));
			}
		}
		for (auto it = omx_cards.cbegin(); it != omx_cards.cend(); it++)
			omx_audio_dev->add(*it, *it, Settings::getInstance()->getString("OMXAudioDev") == *it);
		s->addWithLabel("OMX PLAYER AUDIO DEVICE", omx_audio_dev);
		s->addSaveFunc([omx_audio_dev] {
			if (Settings::getInstance()->getString("OMXAudioDev") != omx_audio_dev->getSelected())
				Settings::getInstance()->setString("OMXAudioDev", omx_audio_dev->getSelected());
		});
#endif
	}

	mWindow->pushGui(s);

}

void GuiMenu::openUISettings()
{
	auto s = new GuiSettings(mWindow, "UI SETTINGS");

	//UI mode
	auto UImodeSelection = std::make_shared< OptionListComponent<std::string> >(mWindow, "UI MODE", false);
	std::vector<std::string> UImodes = UIModeController::getInstance()->getUIModes();
	for (auto it = UImodes.cbegin(); it != UImodes.cend(); it++)
		UImodeSelection->add(*it, *it, Settings::getInstance()->getString("UIMode") == *it);
	s->addWithLabel("UI MODE", UImodeSelection);
	Window* window = mWindow;
	s->addSaveFunc([ UImodeSelection, window]
	{
		std::string selectedMode = UImodeSelection->getSelected();
		if (selectedMode != "Full")
		{
			std::string msg = "You are changing the UI to a restricted mode:\n" + selectedMode + "\n";
			msg += "This will hide most menu-options to prevent changes to the system.\n";
			msg += "To unlock and return to the full UI, enter this code: \n";
			msg += "\"" + UIModeController::getInstance()->getFormattedPassKeyStr() + "\"\n\n";
			msg += "Do you want to proceed?";
			window->pushGui(new GuiMsgBox(window, msg, 
				"YES", [selectedMode] {
					LOG(LogDebug) << "Setting UI mode to " << selectedMode;
					Settings::getInstance()->setString("UIMode", selectedMode);
					Settings::getInstance()->saveFile();
			}, "NO",nullptr));
		}
	});

	// screensaver
	ComponentListRow screensaver_row;
	screensaver_row.elements.clear();
	screensaver_row.addElement(std::make_shared<TextComponent>(mWindow, "SCREENSAVER SETTINGS", Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
	screensaver_row.addElement(makeArrow(mWindow), false);
	screensaver_row.makeAcceptInputHandler(std::bind(&GuiMenu::openScreensaverOptions, this));
	s->addRow(screensaver_row);

	// quick system select (left/right in game list view)
	auto quick_sys_select = std::make_shared<SwitchComponent>(mWindow);
	quick_sys_select->setState(Settings::getInstance()->getBool("QuickSystemSelect"));
	s->addWithLabel("QUICK SYSTEM SELECT", quick_sys_select);
	s->addSaveFunc([quick_sys_select] { Settings::getInstance()->setBool("QuickSystemSelect", quick_sys_select->getState()); });

	// carousel transition option
	auto move_carousel = std::make_shared<SwitchComponent>(mWindow);
	move_carousel->setState(Settings::getInstance()->getBool("MoveCarousel"));
	s->addWithLabel("CAROUSEL TRANSITIONS", move_carousel);
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

	// transition style
	auto transition_style = std::make_shared< OptionListComponent<std::string> >(mWindow, "TRANSITION STYLE", false);
	std::vector<std::string> transitions;
	transitions.push_back("fade");
	transitions.push_back("slide");
	transitions.push_back("instant");
	for(auto it = transitions.cbegin(); it != transitions.cend(); it++)
		transition_style->add(*it, *it, Settings::getInstance()->getString("TransitionStyle") == *it);
	s->addWithLabel("TRANSITION STYLE", transition_style);
	s->addSaveFunc([transition_style] {
		if (Settings::getInstance()->getString("TransitionStyle") == "instant"
			&& transition_style->getSelected() != "instant"
			&& PowerSaver::getMode() == PowerSaver::INSTANT)
		{
			Settings::getInstance()->setString("PowerSaverMode", "default");
			PowerSaver::init();
		}
		Settings::getInstance()->setString("TransitionStyle", transition_style->getSelected());
	});

	// theme set
	auto themeSets = ThemeData::getThemeSets();

	if(!themeSets.empty())
	{
		std::map<std::string, ThemeSet>::const_iterator selectedSet = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
		if(selectedSet == themeSets.cend())
			selectedSet = themeSets.cbegin();

		auto theme_set = std::make_shared< OptionListComponent<std::string> >(mWindow, "THEME SET", false);
		for(auto it = themeSets.cbegin(); it != themeSets.cend(); it++)
			theme_set->add(it->first, it->first, it == selectedSet);
		s->addWithLabel("THEME SET", theme_set);

		Window* window = mWindow;
		s->addSaveFunc([window, theme_set]
		{
			bool needReload = false;
			std::string oldTheme = Settings::getInstance()->getString("ThemeSet");
			if(oldTheme != theme_set->getSelected())
				needReload = true;

			Settings::getInstance()->setString("ThemeSet", theme_set->getSelected());

			if(needReload)
			{
				Scripting::fireEvent("theme-changed", theme_set->getSelected(), oldTheme);
				CollectionSystemManager::get()->updateSystemsList();
				ViewController::get()->goToStart();
				ViewController::get()->reloadAll(); // TODO - replace this with some sort of signal-based implementation
			}
		});
	}

	// GameList view style
	auto gamelist_style = std::make_shared< OptionListComponent<std::string> >(mWindow, "GAMELIST VIEW STYLE", false);
	std::vector<std::string> styles;
	styles.push_back("automatic");
	styles.push_back("basic");
	styles.push_back("detailed");
	styles.push_back("video");
	styles.push_back("grid");

	for (auto it = styles.cbegin(); it != styles.cend(); it++)
		gamelist_style->add(*it, *it, Settings::getInstance()->getString("GamelistViewStyle") == *it);
	s->addWithLabel("GAMELIST VIEW STYLE", gamelist_style);
	s->addSaveFunc([gamelist_style] {
		bool needReload = false;
		if (Settings::getInstance()->getString("GamelistViewStyle") != gamelist_style->getSelected())
			needReload = true;
		Settings::getInstance()->setString("GamelistViewStyle", gamelist_style->getSelected());
		if (needReload)
			ViewController::get()->reloadAll();
	});

	// Optionally start in selected system
	auto systemfocus_list = std::make_shared< OptionListComponent<std::string> >(mWindow, "START ON SYSTEM", false);
	systemfocus_list->add("NONE", "", Settings::getInstance()->getString("StartupSystem") == "");
	for (auto it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); it++)
	{
		if ("retropie" != (*it)->getName())
		{
			systemfocus_list->add((*it)->getName(), (*it)->getName(), Settings::getInstance()->getString("StartupSystem") == (*it)->getName());
		}
	}
	s->addWithLabel("START ON SYSTEM", systemfocus_list);
	s->addSaveFunc([systemfocus_list] {
		Settings::getInstance()->setString("StartupSystem", systemfocus_list->getSelected());
	});

	// show help
	auto show_help = std::make_shared<SwitchComponent>(mWindow);
	show_help->setState(Settings::getInstance()->getBool("ShowHelpPrompts"));
	s->addWithLabel("ON-SCREEN HELP", show_help);
	s->addSaveFunc([show_help] { Settings::getInstance()->setBool("ShowHelpPrompts", show_help->getState()); });

	// enable filters (ForceDisableFilters)
	auto enable_filter = std::make_shared<SwitchComponent>(mWindow);
	enable_filter->setState(!Settings::getInstance()->getBool("ForceDisableFilters"));
	s->addWithLabel("ENABLE FILTERS", enable_filter);
	s->addSaveFunc([enable_filter] { 
		bool filter_is_enabled = !Settings::getInstance()->getBool("ForceDisableFilters");
		Settings::getInstance()->setBool("ForceDisableFilters", !enable_filter->getState()); 
		if (enable_filter->getState() != filter_is_enabled) ViewController::get()->ReloadAndGoToStart();
	});

	mWindow->pushGui(s);

}

void GuiMenu::openOtherSettings()
{
	auto s = new GuiSettings(mWindow, "OTHER SETTINGS");

	// maximum vram
	auto max_vram = std::make_shared<SliderComponent>(mWindow, 0.f, 1000.f, 10.f, "Mb");
	max_vram->setValue((float)(Settings::getInstance()->getInt("MaxVRAM")));
	s->addWithLabel("VRAM LIMIT", max_vram);
	s->addSaveFunc([max_vram] { Settings::getInstance()->setInt("MaxVRAM", (int)Math::round(max_vram->getValue())); });

	// power saver
	auto power_saver = std::make_shared< OptionListComponent<std::string> >(mWindow, "POWER SAVER MODES", false);
	std::vector<std::string> modes;
	modes.push_back("disabled");
	modes.push_back("default");
	modes.push_back("enhanced");
	modes.push_back("instant");
	for (auto it = modes.cbegin(); it != modes.cend(); it++)
		power_saver->add(*it, *it, Settings::getInstance()->getString("PowerSaverMode") == *it);
	s->addWithLabel("POWER SAVER MODES", power_saver);
	s->addSaveFunc([this, power_saver] {
		if (Settings::getInstance()->getString("PowerSaverMode") != "instant" && power_saver->getSelected() == "instant") {
			Settings::getInstance()->setString("TransitionStyle", "instant");
			Settings::getInstance()->setBool("MoveCarousel", false);
			Settings::getInstance()->setBool("EnableSounds", false);
		}
		Settings::getInstance()->setString("PowerSaverMode", power_saver->getSelected());
		PowerSaver::init();
	});

	// gamelists
	auto save_gamelists = std::make_shared<SwitchComponent>(mWindow);
	save_gamelists->setState(Settings::getInstance()->getBool("SaveGamelistsOnExit"));
	s->addWithLabel("SAVE METADATA ON EXIT", save_gamelists);
	s->addSaveFunc([save_gamelists] { Settings::getInstance()->setBool("SaveGamelistsOnExit", save_gamelists->getState()); });

	auto parse_gamelists = std::make_shared<SwitchComponent>(mWindow);
	parse_gamelists->setState(Settings::getInstance()->getBool("ParseGamelistOnly"));
	s->addWithLabel("PARSE GAMESLISTS ONLY", parse_gamelists);
	s->addSaveFunc([parse_gamelists] { Settings::getInstance()->setBool("ParseGamelistOnly", parse_gamelists->getState()); });

	auto local_art = std::make_shared<SwitchComponent>(mWindow);
	local_art->setState(Settings::getInstance()->getBool("LocalArt"));
	s->addWithLabel("SEARCH FOR LOCAL ART", local_art);
	s->addSaveFunc([local_art] { Settings::getInstance()->setBool("LocalArt", local_art->getState()); });

	// hidden files
	auto hidden_files = std::make_shared<SwitchComponent>(mWindow);
	hidden_files->setState(Settings::getInstance()->getBool("ShowHiddenFiles"));
	s->addWithLabel("SHOW HIDDEN FILES", hidden_files);
	s->addSaveFunc([hidden_files] { Settings::getInstance()->setBool("ShowHiddenFiles", hidden_files->getState()); });

#ifdef _RPI_
	// Video Player - VideoOmxPlayer
	auto omx_player = std::make_shared<SwitchComponent>(mWindow);
	omx_player->setState(Settings::getInstance()->getBool("VideoOmxPlayer"));
	s->addWithLabel("USE OMX PLAYER (HW ACCELERATED)", omx_player);
	s->addSaveFunc([omx_player]
	{
		// need to reload all views to re-create the right video components
		bool needReload = false;
		if(Settings::getInstance()->getBool("VideoOmxPlayer") != omx_player->getState())
			needReload = true;

		Settings::getInstance()->setBool("VideoOmxPlayer", omx_player->getState());

		if(needReload)
			ViewController::get()->reloadAll();
	});

#endif

	// framerate
	auto framerate = std::make_shared<SwitchComponent>(mWindow);
	framerate->setState(Settings::getInstance()->getBool("DrawFramerate"));
	s->addWithLabel("SHOW FRAMERATE", framerate);
	s->addSaveFunc([framerate] { Settings::getInstance()->setBool("DrawFramerate", framerate->getState()); });


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

void GuiMenu::openQuitMenu()
{
	auto s = new GuiSettings(mWindow, "QUIT");

	Window* window = mWindow;

	ComponentListRow row;
	if (UIModeController::getInstance()->isUIModeFull())
	{
		row.makeAcceptInputHandler([window] {
			window->pushGui(new GuiMsgBox(window, "REALLY RESTART?", "YES",
				[] {
				Scripting::fireEvent("quit");
				if(quitES("/tmp/es-restart") != 0)
					LOG(LogWarning) << "Restart terminated with non-zero result!";
			}, "NO", nullptr));
		});
		row.addElement(std::make_shared<TextComponent>(window, "RESTART EMULATIONSTATION", Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
		s->addRow(row);



		if(Settings::getInstance()->getBool("ShowExit"))
		{
			row.elements.clear();
			row.makeAcceptInputHandler([window] {
				window->pushGui(new GuiMsgBox(window, "REALLY QUIT?", "YES",
					[] {
					Scripting::fireEvent("quit");
					quitES("");
				}, "NO", nullptr));
			});
			row.addElement(std::make_shared<TextComponent>(window, "QUIT EMULATIONSTATION", Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
			s->addRow(row);
		}
	}
	row.elements.clear();
	row.makeAcceptInputHandler([window] {
		window->pushGui(new GuiMsgBox(window, "REALLY RESTART?", "YES",
			[] {
			Scripting::fireEvent("quit", "reboot");
			Scripting::fireEvent("reboot");
			if (quitES("/tmp/es-sysrestart") != 0)
				LOG(LogWarning) << "Restart terminated with non-zero result!";
		}, "NO", nullptr));
	});
	row.addElement(std::make_shared<TextComponent>(window, "RESTART SYSTEM", Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
	s->addRow(row);

	row.elements.clear();
	row.makeAcceptInputHandler([window] {
		window->pushGui(new GuiMsgBox(window, "REALLY SHUTDOWN?", "YES",
			[] {
			Scripting::fireEvent("quit", "shutdown");
			Scripting::fireEvent("shutdown");
			if (quitES("/tmp/es-shutdown") != 0)
				LOG(LogWarning) << "Shutdown terminated with non-zero result!";
		}, "NO", nullptr));
	});
	row.addElement(std::make_shared<TextComponent>(window, "SHUTDOWN SYSTEM", Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
	s->addRow(row);

	mWindow->pushGui(s);
}

void GuiMenu::addVersionInfo()
{
	std::string  buildDate = (Settings::getInstance()->getBool("Debug") ? std::string( "   (" + Utils::String::toUpper(PROGRAM_BUILT_STRING) + ")") : (""));

	mVersion.setFont(Font::get(FONT_SIZE_SMALL));
	mVersion.setColor(0x5E5E5EFF);
	mVersion.setText("EMULATIONSTATION V" + Utils::String::toUpper(PROGRAM_VERSION_STRING) + buildDate);
	mVersion.setHorizontalAlignment(ALIGN_CENTER);
	addChild(&mVersion);
}

void GuiMenu::openScreensaverOptions() {
	mWindow->pushGui(new GuiGeneralScreensaverOptions(mWindow, "SCREENSAVER SETTINGS"));
}

// new screensaver options for Batocera
void GuiMenu::openSlideshowScreensaverOptions() {
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
	mVersion.setSize(mSize.x(), 0);
	mVersion.setPosition(0, mSize.y() - mVersion.getSize().y());
}

void GuiMenu::addEntry(const char* name, unsigned int color, bool add_arrow, const std::function<void()>& func)
{
	std::shared_ptr<Font> font = Font::get(FONT_SIZE_MEDIUM);

	// populate the list
	ComponentListRow row;
	row.addElement(std::make_shared<TextComponent>(mWindow, name, font, color), true);

	if(add_arrow)
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

	if((config->isMappedTo("a", input) || config->isMappedTo("start", input)) && input.value != 0) // batocera
	{
		delete this;
		return true;
	}

	return false;
}

HelpStyle GuiMenu::getHelpStyle()
{
  HelpStyle style = HelpStyle();
  //style.applyTheme(ViewController::get()->getState().getSystem()->getTheme(), "system"); // batocera ; make game lists reload crashing (viewcontroller delete) ; seems not usefull
  return style;
}

std::vector<HelpPrompt> GuiMenu::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt("up/down", _("CHOOSE"))); // batocera
	prompts.push_back(HelpPrompt("b", _("SELECT"))); // batocera
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

void GuiMenu::openSystemInformations_batocera() {
  Window *window = mWindow;
  bool isFullUI = UIModeController::getInstance()->isUIModeFull();
  GuiSettings *informationsGui = new GuiSettings(window, _("INFORMATION").c_str());

  auto version = std::make_shared<TextComponent>(window,
						 ApiSystem::getInstance()->getVersion(),
						 Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
  informationsGui->addWithLabel(_("VERSION"), version);
  bool warning = ApiSystem::getInstance()->isFreeSpaceLimit();
  auto space = std::make_shared<TextComponent>(window,
					       ApiSystem::getInstance()->getFreeSpaceInfo(),
					       Font::get(FONT_SIZE_MEDIUM),
					       warning ? 0xFF0000FF : 0x777777FF);
  informationsGui->addWithLabel(_("DISK USAGE"), space);

  // various informations
  std::vector<std::string> infos = ApiSystem::getInstance()->getSystemInformations();
  for(auto it = infos.begin(); it != infos.end(); it++) {
    std::vector<std::string> tokens;
    boost::split( tokens, (*it), boost::is_any_of(":") );
    if(tokens.size()>= 2){
      // concatenat the ending words
      std::string vname = "";
      for(unsigned int i=1; i<tokens.size(); i++) {
	if(i > 1) vname += " ";
	vname += tokens.at(i);
      }

      auto space = std::make_shared<TextComponent>(window,
						   vname,
						   Font::get(FONT_SIZE_MEDIUM),
						   0x777777FF);
      informationsGui->addWithLabel(tokens.at(0), space);
    }
  }

  // support
  if(isFullUI) {
    ComponentListRow row;
    row.makeAcceptInputHandler([window] {
	window->pushGui(new GuiMsgBox(window, _("CREATE A SUPPORT FILE ?"), _("YES"),
				      [window] {
					if(ApiSystem::getInstance()->generateSupportFile()) {
					  window->pushGui(new GuiMsgBox(window, _("FILE GENERATED SUCCESSFULLY"), _("OK")));
					} else {
					  window->pushGui(new GuiMsgBox(window, _("FILE GENERATION FAILED"), _("OK")));
					}
				      }, _("NO"), nullptr));
      });
    auto supportFile = std::make_shared<TextComponent>(window, _("CREATE A SUPPORT FILE"),
						       Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
    row.addElement(supportFile, false);
    informationsGui->addRow(row);
  }

  window->pushGui(informationsGui);
}

void GuiMenu::openSystemSettings_batocera() {
  Window *window = mWindow;

  auto s = new GuiSettings(mWindow, _("SYSTEM SETTINGS").c_str());
  bool isFullUI = UIModeController::getInstance()->isUIModeFull();
  
  // system informations
  {
    ComponentListRow row;
    std::function<void()> openGui = [this] {
      openSystemInformations_batocera();
    };
    row.makeAcceptInputHandler(openGui);
    auto informationsSettings = std::make_shared<TextComponent>(mWindow, _("INFORMATION"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
    auto bracket = makeArrow(mWindow);
    row.addElement(informationsSettings, true);
    row.addElement(bracket, false);
    s->addRow(row);
  }
  
  std::vector<std::string> availableStorage = ApiSystem::getInstance()->getAvailableStorageDevices();
  std::string selectedStorage = ApiSystem::getInstance()->getCurrentStorage();
  
  // Storage device
  auto optionsStorage = std::make_shared<OptionListComponent<std::string> >(window, _("STORAGE DEVICE"),
									    false);
  for(auto it = availableStorage.begin(); it != availableStorage.end(); it++){
    if((*it) != "RAM"){
      if (boost::starts_with((*it), "DEV")){
	std::vector<std::string> tokens;
	boost::split( tokens, (*it), boost::is_any_of(" ") );
	if(tokens.size()>= 3){
	  // concatenat the ending words
	  std::string vname = "";
	  for(unsigned int i=2; i<tokens.size(); i++) {
	    if(i > 2) vname += " ";
	    vname += tokens.at(i);
	  }
	  optionsStorage->add(vname, (*it), selectedStorage == std::string("DEV "+tokens.at(1)));
	}
      } else {
	optionsStorage->add((*it), (*it), selectedStorage == (*it));
      }
    }
  }
  s->addWithLabel(_("STORAGE DEVICE"), optionsStorage);
  
  
  // language choice
  auto language_choice = std::make_shared<OptionListComponent<std::string> >(window, _("LANGUAGE"),
									     false);
  std::string language = SystemConf::getInstance()->get("system.language");
  if (language.empty()) language = "en_US";
  language_choice->add("BASQUE",              "eu_ES", language == "eu_ES");
  language_choice->add("正體中文",             "zh_TW", language == "zh_TW");
  language_choice->add("简体中文",             "zh_CN", language == "zh_CN");
  language_choice->add("DEUTSCH",             "de_DE", language == "de_DE");
  language_choice->add("ENGLISH",             "en_US", language == "en_US");
  language_choice->add("ESPAÑOL",             "es_ES", language == "es_ES");
  language_choice->add("FRANÇAIS",            "fr_FR", language == "fr_FR");
  language_choice->add("ITALIANO",            "it_IT", language == "it_IT");
  language_choice->add("PORTUGUES BRASILEIRO", "pt_BR", language == "pt_BR");
  language_choice->add("PORTUGUES PORTUGAL",  "pt_PT", language == "pt_PT");
  language_choice->add("SVENSKA",             "sv_SE", language == "sv_SE");
  language_choice->add("TÜRKÇE",              "tr_TR", language == "tr_TR");
  language_choice->add("CATALÀ",              "ca_ES", language == "ca_ES");
  language_choice->add("ARABIC",              "ar_YE", language == "ar_YE");
  language_choice->add("DUTCH",               "nl_NL", language == "nl_NL");
  language_choice->add("GREEK",               "el_GR", language == "el_GR");
  language_choice->add("KOREAN",              "ko_KR", language == "ko_KR");
  language_choice->add("NORWEGIAN",           "nn_NO", language == "nn_NO");
  language_choice->add("NORWEGIAN BOKMAL",    "nb_NO", language == "nb_NO");
  language_choice->add("POLISH",              "pl_PL", language == "pl_PL");
  language_choice->add("JAPANESE",            "ja_JP", language == "ja_JP");
  language_choice->add("RUSSIAN",             "ru_RU", language == "ru_RU");
  language_choice->add("HUNGARIAN",           "hu_HU", language == "hu_HU");
  
  s->addWithLabel(_("LANGUAGE"), language_choice);
  
  // Overclock choice
  auto overclock_choice = std::make_shared<OptionListComponent<std::string> >(window, _("OVERCLOCK"),
									      false);
  std::string currentOverclock = Settings::getInstance()->getString("Overclock");
  if(currentOverclock == "") currentOverclock = "none";
  std::vector<std::string> availableOverclocking = ApiSystem::getInstance()->getAvailableOverclocking();
  
  // Overclocking device
  bool isOneSet = false;
  for(auto it = availableOverclocking.begin(); it != availableOverclocking.end(); it++){
    std::vector<std::string> tokens;
    boost::split( tokens, (*it), boost::is_any_of(" ") );
    if(tokens.size()>= 2){
      // concatenat the ending words
      std::string vname = "";
      for(unsigned int i=1; i<tokens.size(); i++) {
	if(i > 1) vname += " ";
	vname += tokens.at(i);
      }
      bool isSet = currentOverclock == std::string(tokens.at(0));
      if(isSet) {
	isOneSet = true;
      }
      overclock_choice->add(vname, tokens.at(0), isSet);
    }
  }
  if(isOneSet == false) {
    overclock_choice->add(currentOverclock, currentOverclock, true);
  }
  s->addWithLabel(_("OVERCLOCK"), overclock_choice);
  
  // Updates
  {
    ComponentListRow row;
    std::function<void()> openGuiD = [this] {
      GuiSettings *updateGui = new GuiSettings(mWindow, _("UPDATES").c_str());
      // Enable updates
      auto updates_enabled = std::make_shared<SwitchComponent>(mWindow);
      updates_enabled->setState(
				SystemConf::getInstance()->get("updates.enabled") == "1");
      updateGui->addWithLabel(_("AUTO UPDATES"), updates_enabled);
      
      // Start update
      {
	ComponentListRow updateRow;
	std::function<void()> openGui = [this] { mWindow->pushGui(new GuiUpdate(mWindow)); };
	updateRow.makeAcceptInputHandler(openGui);
	auto update = std::make_shared<TextComponent>(mWindow, _("START UPDATE"),
						      Font::get(FONT_SIZE_MEDIUM),
						      0x777777FF);
	auto bracket = makeArrow(mWindow);
	updateRow.addElement(update, true);
	updateRow.addElement(bracket, false);
	updateGui->addRow(updateRow);
      }
      updateGui->addSaveFunc([updates_enabled] {
	  SystemConf::getInstance()->set("updates.enabled",
					 updates_enabled->getState() ? "1" : "0");
	  SystemConf::getInstance()->saveSystemConf();
	});
      mWindow->pushGui(updateGui);
      
    };
    row.makeAcceptInputHandler(openGuiD);
    auto update = std::make_shared<TextComponent>(mWindow, _("UPDATES"), Font::get(FONT_SIZE_MEDIUM),
						  0x777777FF);
    auto bracket = makeArrow(mWindow);
    row.addElement(update, true);
    row.addElement(bracket, false);
    s->addRow(row);
  }
  
  // backup
  {
    ComponentListRow row;
    auto openBackupNow = [this] { mWindow->pushGui(new GuiBackupStart(mWindow)); };
    row.makeAcceptInputHandler(openBackupNow);
    auto backupSettings = std::make_shared<TextComponent>(mWindow, _("BACKUP USER DATA"),
							  Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
    auto bracket = makeArrow(mWindow);
    row.addElement(backupSettings, true);
    row.addElement(bracket, false);
    s->addRow(row);
  }
  
#ifdef _ENABLE_KODI_
  //Kodi
  {
    ComponentListRow row;
    std::function<void()> openGui = [this] {
      GuiSettings *kodiGui = new GuiSettings(mWindow, _("KODI SETTINGS").c_str());
      auto kodiEnabled = std::make_shared<SwitchComponent>(mWindow);
      kodiEnabled->setState(SystemConf::getInstance()->get("kodi.enabled") == "1");
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
    };
    row.makeAcceptInputHandler(openGui);
    auto kodiSettings = std::make_shared<TextComponent>(mWindow, _("KODI SETTINGS"),
							Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
    auto bracket = makeArrow(mWindow);
    row.addElement(kodiSettings, true);
    row.addElement(bracket, false);
    s->addRow(row);
  }
#endif
  // install
  {
    ComponentListRow row;
    auto openInstallNow = [this] { mWindow->pushGui(new GuiInstallStart(mWindow)); };
    row.makeAcceptInputHandler(openInstallNow);
    auto installSettings = std::make_shared<TextComponent>(mWindow, _("INSTALL BATOCERA ON A NEW DISK"),
							   Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
    auto bracket = makeArrow(mWindow);
    row.addElement(installSettings, true);
    row.addElement(bracket, false);
    s->addRow(row);
  }
  
  //Security
  {
    ComponentListRow row;
    std::function<void()> openGui = [this] {
      GuiSettings *securityGui = new GuiSettings(mWindow, _("SECURITY").c_str());
      auto securityEnabled = std::make_shared<SwitchComponent>(mWindow);
      securityEnabled->setState(SystemConf::getInstance()->get("system.security.enabled") == "1");
      securityGui->addWithLabel(_("ENFORCE SECURITY"), securityEnabled);
      
      auto rootpassword = std::make_shared<TextComponent>(mWindow,
							  ApiSystem::getInstance()->getRootPassword(),
							  Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
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
	  
	  if (reboot) {
	    window->displayMessage(_("A REBOOT OF THE SYSTEM IS REQUIRED TO APPLY THE NEW CONFIGURATION"));
	  }
	});
      mWindow->pushGui(securityGui);
    };
    row.makeAcceptInputHandler(openGui);
    auto securitySettings = std::make_shared<TextComponent>(mWindow, _("SECURITY"),
							    Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
    auto bracket = makeArrow(mWindow);
    row.addElement(securitySettings, true);
    row.addElement(bracket, false);
    s->addRow(row);
  }
  
  s->addSaveFunc([overclock_choice, window, language_choice, language, optionsStorage, selectedStorage] {
      bool reboot = false;
      if (optionsStorage->changed()) {
	ApiSystem::getInstance()->setStorage(optionsStorage->getSelected());
	reboot = true;
      }
      
      if(overclock_choice->changed()) {
	Settings::getInstance()->setString("Overclock", overclock_choice->getSelected());
	ApiSystem::getInstance()->setOverclock(overclock_choice->getSelected());
	reboot = true;
      }
      if(language_choice->changed()) {
	SystemConf::getInstance()->set("system.language",
				       language_choice->getSelected());
	SystemConf::getInstance()->saveSystemConf();
	reboot = true;
      }
      if (reboot) {
	window->displayMessage(_("A REBOOT OF THE SYSTEM IS REQUIRED TO APPLY THE NEW CONFIGURATION"));
      }
      
    });
  mWindow->pushGui(s);
}

void GuiMenu::openGamesSettings_batocera() {
  auto s = new GuiSettings(mWindow, _("GAMES SETTINGS").c_str());
  if (SystemConf::getInstance()->get("system.es.menu") != "bartop") {

    // Screen ratio choice
    auto ratio_choice = createRatioOptionList(mWindow, "global");
    s->addWithLabel(_("GAME RATIO"), ratio_choice);
    s->addSaveFunc([ratio_choice] {
	if(ratio_choice->changed()) {
	  SystemConf::getInstance()->set("global.ratio", ratio_choice->getSelected());
	  SystemConf::getInstance()->saveSystemConf();
	}
      });
  }
  // smoothing
  auto smoothing_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SMOOTH GAMES"));
  smoothing_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.smooth") != "0" && SystemConf::getInstance()->get("global.smooth") != "1");
  smoothing_enabled->add(_("ON"),   "1",    SystemConf::getInstance()->get("global.smooth") == "1");
  smoothing_enabled->add(_("OFF"),  "0",    SystemConf::getInstance()->get("global.smooth") == "0");
  s->addWithLabel(_("SMOOTH GAMES"), smoothing_enabled);

  // rewind
  auto rewind_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("REWIND"));
  rewind_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.rewind") != "0" && SystemConf::getInstance()->get("global.rewind") != "1");
  rewind_enabled->add(_("ON"),   "1",    SystemConf::getInstance()->get("global.rewind") == "1");
  rewind_enabled->add(_("OFF"),  "0",    SystemConf::getInstance()->get("global.rewind") == "0");
  s->addWithLabel(_("REWIND"), rewind_enabled);

  // autosave/load
  auto autosave_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("AUTO SAVE/LOAD"));
  autosave_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.autosave") != "0" && SystemConf::getInstance()->get("global.autosave") != "1");
  autosave_enabled->add(_("ON"),   "1",    SystemConf::getInstance()->get("global.autosave") == "1");
  autosave_enabled->add(_("OFF"),  "0",    SystemConf::getInstance()->get("global.autosave") == "0");
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
  s->addWithLabel(_("SHADERS SET"), shaders_choices);
  // Integer scale
  auto integerscale_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INTEGER SCALE (PIXEL PERFECT)"));
  integerscale_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.integerscale") != "0" && SystemConf::getInstance()->get("global.integerscale") != "1");
  integerscale_enabled->add(_("ON"),   "1",    SystemConf::getInstance()->get("global.integerscale") == "1");
  integerscale_enabled->add(_("OFF"),  "0",    SystemConf::getInstance()->get("global.integerscale") == "0");
  s->addWithLabel(_("INTEGER SCALE (PIXEL PERFECT)"), integerscale_enabled);
  s->addSaveFunc([integerscale_enabled] {
      if(integerscale_enabled->changed()) {
	SystemConf::getInstance()->set("global.integerscale", integerscale_enabled->getSelected());
	SystemConf::getInstance()->saveSystemConf();
      }
    });

  // decorations
  {
    auto decorations = std::make_shared<OptionListComponent<std::string> >(mWindow, _("DECORATION"), false);
    std::vector<std::string> decorations_item;
    decorations_item.push_back(_("AUTO"));
    decorations_item.push_back(_("NONE"));

    std::vector<std::string> sets = GuiMenu::getDecorationsSets();
    for(auto it = sets.begin(); it != sets.end(); it++) {
      decorations_item.push_back(*it);
    }

    for (auto it = decorations_item.begin(); it != decorations_item.end(); it++) {
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
	if(decorations->changed()) {
	  SystemConf::getInstance()->set("global.bezel", decorations->getSelected() == _("NONE") ? "none" : decorations->getSelected() == _("AUTO") ? "" : decorations->getSelected());
	  SystemConf::getInstance()->saveSystemConf();
	}
      });
  }

  if (SystemConf::getInstance()->get("system.es.menu") != "bartop") {

    // Retroachievements
    {
      ComponentListRow row;
      std::function<void()> openGui = [this] {
	GuiSettings *retroachievements = new GuiSettings(mWindow,
							 _("RETROACHIEVEMENTS SETTINGS").c_str());
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
      };
      row.makeAcceptInputHandler(openGui);
      auto retroachievementsSettings = std::make_shared<TextComponent>(mWindow,
								       _("RETROACHIEVEMENTS SETTINGS"),
								       Font::get(FONT_SIZE_MEDIUM),
								       0x777777FF);
      auto bracket = makeArrow(mWindow);
      row.addElement(retroachievementsSettings, true);
      row.addElement(bracket, false);
      s->addRow(row);
    }

    // Bios
    {
      std::function<void()> openGuiD = [this, s] {
	GuiSettings *configuration = new GuiSettings(mWindow, _("MISSING BIOS").c_str());
	std::vector<BiosSystem> biosInformations = ApiSystem::getInstance()->getBiosInformations();

	if(biosInformations.size() == 0) {
	  ComponentListRow noRow;
	  auto biosText = std::make_shared<TextComponent>(mWindow, _("NO MISSING BIOS").c_str(),
							  Font::get(FONT_SIZE_MEDIUM),
							  0x777777FF);
	  noRow.addElement(biosText, true);
	  configuration->addRow(noRow);
	} else {

	  for (auto systemBios = biosInformations.begin(); systemBios != biosInformations.end(); systemBios++) {
	    ComponentListRow biosRow;
	    auto biosText = std::make_shared<TextComponent>(mWindow, (*systemBios).name.c_str(),
							    Font::get(FONT_SIZE_MEDIUM),
							    0x777777FF);
	    BiosSystem systemBiosData = (*systemBios);
	    std::function<void()> openGuiDBios = [this, systemBiosData] {
	      GuiSettings *configurationInfo = new GuiSettings(mWindow, systemBiosData.name.c_str());
	      for (auto biosFile = systemBiosData.bios.begin(); biosFile != systemBiosData.bios.end(); biosFile++) {
		auto biosPath = std::make_shared<TextComponent>(mWindow, biosFile->path.c_str(),
								Font::get(FONT_SIZE_MEDIUM),
								0x000000FF);
		auto biosMd5 = std::make_shared<TextComponent>(mWindow, biosFile->md5.c_str(),
							       Font::get(FONT_SIZE_SMALL),
							       0x777777FF);
		auto biosStatus = std::make_shared<TextComponent>(mWindow, biosFile->status.c_str(),
								  Font::get(FONT_SIZE_SMALL),
								  0x777777FF);
		ComponentListRow biosFileRow;
		biosFileRow.addElement(biosPath, true);
		configurationInfo->addRow(biosFileRow);

		configurationInfo->addWithLabel(_("MD5"), biosMd5);
		configurationInfo->addWithLabel(_("STATUS"), biosStatus);
	      }
	      mWindow->pushGui(configurationInfo);
	    };
	    biosRow.makeAcceptInputHandler(openGuiDBios);
	    auto bracket = makeArrow(mWindow);
	    biosRow.addElement(biosText, true);
	    biosRow.addElement(bracket, false);
	    configuration->addRow(biosRow);
	  }
	}
	mWindow->pushGui(configuration);
      };
      // bios button
      ComponentListRow row;
      row.makeAcceptInputHandler(openGuiD);
      auto bios = std::make_shared<TextComponent>(mWindow, _("MISSING BIOS"),
						  Font::get(FONT_SIZE_MEDIUM),
						  0x777777FF);
      auto bracket = makeArrow(mWindow);
      row.addElement(bios, true);
      row.addElement(bracket, false);
      s->addRow(row);
    }

    // Custom config for systems
    {
      ComponentListRow row;
      std::function<void()> openGuiD = [this, s] {
	s->save();
	GuiSettings *configuration = new GuiSettings(mWindow, _("ADVANCED").c_str());
	// For each activated system
	std::vector<SystemData *> systems = SystemData::sSystemVector;
	for (auto system = systems.begin(); system != systems.end(); system++) {
	  //if ((*system) != SystemData::getFavoriteSystem()) {
	  ComponentListRow systemRow;
	  auto systemText = std::make_shared<TextComponent>(mWindow, (*system)->getFullName(),
							    Font::get(FONT_SIZE_MEDIUM),
							    0x777777FF);
	  auto bracket = makeArrow(mWindow);
	  systemRow.addElement(systemText, true);
	  systemRow.addElement(bracket, false);
	  SystemData *systemData = (*system);
	  systemRow.makeAcceptInputHandler([this, systemData] {
	      popSystemConfigurationGui(systemData, "");
	    });
	  configuration->addRow(systemRow);
	}
	//}
	mWindow->pushGui(configuration);

      };
      // Advanced button
      row.makeAcceptInputHandler(openGuiD);
      auto advanced = std::make_shared<TextComponent>(mWindow, _("ADVANCED"),
						      Font::get(FONT_SIZE_MEDIUM),
						      0x777777FF);
      auto bracket = makeArrow(mWindow);
      row.addElement(advanced, true);
      row.addElement(bracket, false);
      s->addRow(row);
    }
    // Game List Update
    {
      ComponentListRow row;
      Window *window = mWindow;

      row.makeAcceptInputHandler([this, window] {
	  window->pushGui(new GuiMsgBox(window, _("REALLY UPDATE GAMES LISTS ?"), _("YES"),
					[this, window] {
					  ViewController::get()->goToStart();
					  delete ViewController::get();
					  SystemData::deleteSystems();
					  SystemData::loadConfig();
					  GuiComponent *gui;
					  while ((gui = window->peekGui()) != NULL) {
					    window->removeGui(gui);
					    delete gui;
					  }
					  ViewController::init(window);
					  ViewController::get()->reloadAll();
					  window->pushGui(ViewController::get());
					}, _("NO"), nullptr));
	});
      row.addElement(
		     std::make_shared<TextComponent>(window, _("UPDATE GAMES LISTS"),
						     Font::get(FONT_SIZE_MEDIUM),
						     0x777777FF), true);
      s->addRow(row);
    }
  }
  s->addSaveFunc([smoothing_enabled, rewind_enabled, shaders_choices, autosave_enabled] {
      if(smoothing_enabled->changed())
	SystemConf::getInstance()->set("global.smooth", smoothing_enabled->getSelected());
      if(rewind_enabled->changed())
	SystemConf::getInstance()->set("global.rewind", rewind_enabled->getSelected());
      if(shaders_choices->changed())
	SystemConf::getInstance()->set("global.shaderset", shaders_choices->getSelected());
      if(autosave_enabled->changed())
	SystemConf::getInstance()->set("global.autosave", autosave_enabled->getSelected());
      SystemConf::getInstance()->saveSystemConf();
    });
  mWindow->pushGui(s);
}

void GuiMenu::openControllersSettings_batocera() {

  GuiSettings *s = new GuiSettings(mWindow, _("CONTROLLERS SETTINGS").c_str());

  Window *window = mWindow;

  ComponentListRow row;
  row.makeAcceptInputHandler([window, this, s] {
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


  row.addElement(
		 std::make_shared<TextComponent>(window, _("CONFIGURE A CONTROLLER"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF),
		 true);
  s->addRow(row);

  row.elements.clear();

  std::function<void(void *)> showControllerResult = [window, this, s](void *success) {
    bool result = (bool) success;
      
    if(result) {
      window->pushGui(new GuiMsgBox(window, _("CONTROLLER PAIRED"), _("OK")));
    } else {
      window->pushGui(new GuiMsgBox(window, _("UNABLE TO PAIR CONTROLLER"), _("OK")));
    }
  };
    
  row.makeAcceptInputHandler([window, this, s, showControllerResult] {
      window->pushGui(new GuiLoading(window, [] {
	    bool success = ApiSystem::getInstance()->scanNewBluetooth();
	    return (void *) success;
	  }, showControllerResult));
    });


  row.addElement(
		 std::make_shared<TextComponent>(window, _("PAIR A BLUETOOTH CONTROLLER"), Font::get(FONT_SIZE_MEDIUM),
						 0x777777FF),
		 true);
  s->addRow(row);
  row.elements.clear();

  row.makeAcceptInputHandler([window, this, s] {
      ApiSystem::getInstance()->forgetBluetoothControllers();
      window->pushGui(new GuiMsgBox(window,
				    _("CONTROLLERS LINKS HAVE BEEN DELETED."), _("OK")));
    });
  row.addElement(
		 std::make_shared<TextComponent>(window, _("FORGET BLUETOOTH CONTROLLERS"), Font::get(FONT_SIZE_MEDIUM),
						 0x777777FF),
		 true);
  s->addRow(row);
  row.elements.clear();



  row.elements.clear();

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
    snprintf(strbuf, 256, _("INPUT P%i").c_str(), player+1);

    LOG(LogInfo) << player + 1 << " " << confName << " " << confGuid;
    auto inputOptionList = std::make_shared<OptionListComponent<StrInputConfig *> >(mWindow, strbuf, false);
    options.push_back(inputOptionList);

    // Checking if a setting has been saved, else setting to default
    std::string configuratedName = Settings::getInstance()->getString(confName);
    std::string configuratedGuid = Settings::getInstance()->getString(confGuid);
    bool found = false;
    // For each available and configured input
    for(auto iter = InputManager::getInstance()->getJoysticks().begin(); iter != InputManager::getInstance()->getJoysticks().end(); iter++) {
      InputConfig* config = InputManager::getInstance()->getInputConfigByDevice(iter->first);
      if (config != NULL && config->isConfigured()) {
	// create name
	std::stringstream dispNameSS;
	dispNameSS << "#" << config->getDeviceId() << " ";
	std::string deviceName = config->getDeviceName();
	if (deviceName.size() > 25) {
	  dispNameSS << deviceName.substr(0, 16) << "..." <<
	    deviceName.substr(deviceName.size() - 5, deviceName.size() - 1);
	} else {
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
	} else {
	  LOG(LogWarning) << "adding entry for player" << player << " (not selected): " <<
	    config->getDeviceName() << "  " << config->getDeviceGUIDString();
	  inputOptionList->add(displayName, newInputConfig, false);
	}
      }
    }
    if (configuratedName.compare("") == 0 || !found) {
      LOG(LogWarning) << "adding default entry for player " << player << "(selected : true)";
      inputOptionList->add("default", NULL, true);
    } else {
      LOG(LogWarning) << "adding default entry for player" << player << "(selected : false)";
      inputOptionList->add("default", NULL, false);
    }

    // ADD default config

    // Populate controllers list
    s->addWithLabel(strbuf, inputOptionList);
  }
  s->addSaveFunc([this, options, window] {
      for (int player = 0; player < MAX_PLAYERS; player++) {
	std::stringstream sstm;
	sstm << "INPUT P" << player + 1;
	std::string confName = sstm.str() + "NAME";
	std::string confGuid = sstm.str() + "GUID";

	auto input_p1 = options.at(player);
	std::string selectedName = input_p1->getSelectedName();

	if (selectedName.compare("default") == 0) {
	  Settings::getInstance()->setString(confName, "DEFAULT");
	  Settings::getInstance()->setString(confGuid, "");
	} else {
	  if(input_p1->changed()) {
	    LOG(LogWarning) << "Found the selected controller ! : name in list  = " << selectedName;
	    LOG(LogWarning) << "Found the selected controller ! : guid  = " << input_p1->getSelected()->deviceGUIDString;

	    Settings::getInstance()->setString(confName, input_p1->getSelected()->deviceName);
	    Settings::getInstance()->setString(confGuid, input_p1->getSelected()->deviceGUIDString);
	  }
	}
      }

      Settings::getInstance()->saveFile();
      // this is dependant of this configuration, thus update it
      InputManager::getInstance()->computeLastKnownPlayersDeviceIndexes();
    });

  row.elements.clear();
  window->pushGui(s);
}

void GuiMenu::openUISettings_batocera() {
  auto s = new GuiSettings(mWindow, _("UI SETTINGS").c_str());

  //UI mode
  auto UImodeSelection = std::make_shared< OptionListComponent<std::string> >(mWindow, _("UI MODE"), false);
  std::vector<std::string> UImodes = UIModeController::getInstance()->getUIModes();
  for (auto it = UImodes.cbegin(); it != UImodes.cend(); it++)
    UImodeSelection->add(*it, *it, Settings::getInstance()->getString("UIMode") == *it);
  s->addWithLabel(_("UI MODE"), UImodeSelection);
  Window* window = mWindow;
  s->addSaveFunc([ UImodeSelection, window]
		 {
		   std::string selectedMode = UImodeSelection->getSelected();
		   if (selectedMode != "Full")
		     {
		       std::string msg = "You are changing the UI to a restricted mode:\n" + selectedMode + "\n";
		       msg += "This will hide most menu-options to prevent changes to the system.\n";
		       msg += "To unlock and return to the full UI, enter this code: \n";
		       msg += "\"" + UIModeController::getInstance()->getFormattedPassKeyStr() + "\"\n\n";
		       msg += "Do you want to proceed?";
		       window->pushGui(new GuiMsgBox(window, msg, 
						     "YES", [selectedMode] {
						       LOG(LogDebug) << "Setting UI mode to " << selectedMode;
						       Settings::getInstance()->setString("UIMode", selectedMode);
						       Settings::getInstance()->saveFile();
						     }, "NO",nullptr));
		     }
		 });
  
  // video device
  auto optionsVideo = std::make_shared<OptionListComponent<std::string> >(mWindow, _("VIDEO OUTPUT"), false);
  std::string currentDevice = SystemConf::getInstance()->get("global.videooutput");
  if (currentDevice.empty()) currentDevice = "auto";

  std::vector<std::string> availableVideo = ApiSystem::getInstance()->getAvailableVideoOutputDevices();

  bool vfound=false;
  for(auto it = availableVideo.begin(); it != availableVideo.end(); it++){
    optionsVideo->add((*it), (*it), currentDevice == (*it));
    if(currentDevice == (*it)) {
      vfound = true;
    }
  }
  if(vfound == false) {
    optionsVideo->add(currentDevice, currentDevice, true);
  }
  s->addWithLabel(_("VIDEO OUTPUT"), optionsVideo);

  s->addSaveFunc([this, optionsVideo, currentDevice] {
      if (optionsVideo->changed()) {
	SystemConf::getInstance()->set("global.videooutput", optionsVideo->getSelected());
	SystemConf::getInstance()->saveSystemConf();
	mWindow->displayMessage(_("A REBOOT OF THE SYSTEM IS REQUIRED TO APPLY THE NEW CONFIGURATION"));
      }
    });

  // theme set
  auto themeSets = ThemeData::getThemeSets();

  if (!themeSets.empty()) {
    auto selectedSet = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
    if (selectedSet == themeSets.end())
      selectedSet = themeSets.begin();

    auto theme_set = std::make_shared<OptionListComponent<std::string> >(mWindow, _("THEME SET"),
									 false);
    for (auto it = themeSets.begin(); it != themeSets.end(); it++)
      theme_set->add(it->first, it->first, it == selectedSet);
    s->addWithLabel(_("THEME SET"), theme_set);

    Window *window = mWindow;
    s->addSaveFunc([window, theme_set] {
	bool needReload = false;
	if (theme_set->changed()) {
	  needReload = true;
	  Settings::getInstance()->setString("ThemeSet", theme_set->getSelected());
	}

	if (needReload)
	  ViewController::get()->reloadAll(); // TODO - replace this with some sort of signal-based implementation
      });
  }

  // Batocera themes installer/browser
  {
	  std::function<void()> openThemesListD = [this, s] {
		  GuiSettings *thlist = new GuiSettings(mWindow, _("BATOCERA THEMES").c_str());
		  ComponentListRow row;
		  std::vector<std::string> availableThemes = ApiSystem::getInstance()->getBatoceraThemesList();
		  for(auto it = availableThemes.begin(); it != availableThemes.end(); it++){
			  auto itstring = std::make_shared<TextComponent>(mWindow,
					  (*it).c_str(), Font::get(FONT_SIZE_SMALL), 0x777777FF);
			  char *tmp=new char [(*it).length()+1];
			  std::strcpy (tmp, (*it).c_str());
			  char *thname = strtok (tmp, " ");
			  // Get theme name (from the command line '[A] Theme_name http://url_of_this_theme
			  thname = strtok (NULL, " ");
			  row.makeAcceptInputHandler([this, thname] {
					  std::string msg;
					  if(ApiSystem::getInstance()->installBatoceraTheme(thname)) {
					  std::use_facet<std::ctype<char>>(std::locale()).toupper(&thname[0], &thname[0] + strlen(thname));
					  std::string locname = thname;
					  msg = "THEME '" + locname + "' INSTALLED SUCCESSFULLY";
					  } else {
					  std::use_facet<std::ctype<char>>(std::locale()).toupper(&thname[0], &thname[0] + strlen(thname));
					  std::string locname = thname;
					  msg = "THEME '" + locname + "' INSTALL FAILED";
					  }
					  mWindow->pushGui(new GuiMsgBox(mWindow, msg, _("OK")));
					  });
			  row.addElement(itstring, true);
			  thlist->addRow(row);
			  row.elements.clear();
		  }
		  mWindow->pushGui(thlist);
	  };
	  // themes button
	  ComponentListRow row;
	  row.makeAcceptInputHandler(openThemesListD);
	  auto themesL = std::make_shared<TextComponent>(mWindow, _("INSTALL BATOCERA THEMES"),
			  Font::get(FONT_SIZE_MEDIUM),
			  0x777777FF);
	  auto bracket = makeArrow(mWindow);
	  row.addElement(themesL, true);
	  row.addElement(bracket, false);
	  s->addRow(row);
  }

  // GameList view style
  auto gamelist_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("GAMELIST VIEW STYLE"), false);
  std::vector<std::string> styles;
  styles.push_back("automatic");
  styles.push_back("basic");
  styles.push_back("detailed");
  styles.push_back("video");
  styles.push_back("grid");

  for (auto it = styles.cbegin(); it != styles.cend(); it++)
    gamelist_style->add(*it, *it, Settings::getInstance()->getString("GamelistViewStyle") == *it);
  s->addWithLabel(_("GAMELIST VIEW STYLE"), gamelist_style);
  s->addSaveFunc([gamelist_style] {
      bool needReload = false;
      if (Settings::getInstance()->getString("GamelistViewStyle") != gamelist_style->getSelected())
	needReload = true;
      Settings::getInstance()->setString("GamelistViewStyle", gamelist_style->getSelected());
      if (needReload)
	ViewController::get()->reloadAll();
    });

  // Optionally start in selected system
  auto systemfocus_list = std::make_shared< OptionListComponent<std::string> >(mWindow, _("START ON SYSTEM"), false);
  systemfocus_list->add("NONE", "", Settings::getInstance()->getString("StartupSystem") == "");
  for (auto it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); it++)
    {
      if ("retropie" != (*it)->getName())
	{
	  systemfocus_list->add((*it)->getName(), (*it)->getName(), Settings::getInstance()->getString("StartupSystem") == (*it)->getName());
	}
    }
  s->addWithLabel(_("START ON SYSTEM"), systemfocus_list);
  s->addSaveFunc([systemfocus_list] {
      Settings::getInstance()->setString("StartupSystem", systemfocus_list->getSelected());
    });

  // transition style
  auto transition_style = std::make_shared<OptionListComponent<std::string> >(mWindow,
									      _("TRANSITION STYLE"),
									      false);
  std::vector<std::string> transitions;
  transitions.push_back("fade");
  transitions.push_back("slide");
  transitions.push_back("instant");
  for (auto it = transitions.begin(); it != transitions.end(); it++)
    transition_style->add(*it, *it, Settings::getInstance()->getString("TransitionStyle") == *it);
  s->addWithLabel(_("TRANSITION STYLE"), transition_style);
  s->addSaveFunc([transition_style] {
      if(transition_style->changed()) {
	Settings::getInstance()->setString("TransitionStyle", transition_style->getSelected());
      }
    });

  // screensaver time
  auto screensaver_time = std::make_shared<SliderComponent>(mWindow, 0.f, 30.f, 1.f, "m");
  screensaver_time->setValue(
                             (float) (Settings::getInstance()->getInt("ScreenSaverTime") / (1000 * 60)));
  s->addWithLabel(_("SCREENSAVER AFTER"), screensaver_time);
  s->addSaveFunc([screensaver_time] {
      Settings::getInstance()->setInt("ScreenSaverTime",
				      (int) round(screensaver_time->getValue()) * (1000 * 60));
    });

  // Batocera screensavers: added "random video" (aka "demo mode") and slideshow at the same time,
  // for those who don't scrape videos and stick with pictures
  auto screensaver_behavior = std::make_shared<OptionListComponent<std::string> >(mWindow,
								_("TRANSITION STYLE"), false);
  std::vector<std::string> screensavers;
  screensavers.push_back("dim");
  screensavers.push_back("black");
  screensavers.push_back("random video");
  screensavers.push_back("slideshow");
  for(auto it = screensavers.cbegin(); it != screensavers.cend(); it++)
	  screensaver_behavior->add(*it, *it, Settings::getInstance()->getString("ScreenSaverBehavior") == *it);
  s->addWithLabel(_("SCREENSAVER BEHAVIOR"), screensaver_behavior);
  s->addSaveFunc([this, screensaver_behavior] {
		  if (Settings::getInstance()->getString("ScreenSaverBehavior") != "random video" 
						&& screensaver_behavior->getSelected() == "random video") {
			// if before it wasn't risky but now there's a risk of problems, show warning
			mWindow->pushGui(new GuiMsgBox(mWindow,
			_("THE \"RANDOM VIDEO\" SCREENSAVER SHOWS VIDEOS FROM YOUR GAMELIST.\nIF YOU DON'T HAVE VIDEOS, OR IF NONE OF THEM CAN BE PLAYED AFTER A FEW ATTEMPTS, IT WILL DEFAULT TO \"BLACK\".\nMORE OPTIONS IN THE \"UI SETTINGS\" -> \"RANDOM VIDEO SCREENSAVER SETTINGS\" MENU."),
				_("OK"), [] { return; }));
		}
		Settings::getInstance()->setString("ScreenSaverBehavior", screensaver_behavior->getSelected());
		PowerSaver::updateTimeouts();
		});

  ComponentListRow row;
  // show filtered menu
  row.elements.clear();
  row.addElement(std::make_shared<TextComponent>(mWindow, _("RANDOM VIDEO SCREENSAVER SETTINGS"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
  row.addElement(makeArrow(mWindow), false);
  row.makeAcceptInputHandler(std::bind(&GuiMenu::openVideoScreensaverOptions, this));
  s->addRow(row);

  row.elements.clear();
  row.addElement(std::make_shared<TextComponent>(mWindow, _("SLIDESHOW SCREENSAVER SETTINGS"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
  row.addElement(makeArrow(mWindow), false);
  row.makeAcceptInputHandler(std::bind(&GuiMenu::openSlideshowScreensaverOptions, this));
  s->addRow(row);

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
  s->addSaveFunc(
		 [show_help] {
		   Settings::getInstance()->setBool("ShowHelpPrompts", show_help->getState());
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
      Settings::getInstance()->setBool("UseOSK", osk_enable->getState()); } );

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

  // enable filters (ForceDisableFilters)
  auto enable_filter = std::make_shared<SwitchComponent>(mWindow);
  enable_filter->setState(!Settings::getInstance()->getBool("ForceDisableFilters"));
  s->addWithLabel(_("ENABLE FILTERS"), enable_filter);
  s->addSaveFunc([enable_filter] { 
      bool filter_is_enabled = !Settings::getInstance()->getBool("ForceDisableFilters");
      Settings::getInstance()->setBool("ForceDisableFilters", !enable_filter->getState()); 
      if (enable_filter->getState() != filter_is_enabled) ViewController::get()->ReloadAndGoToStart();
    });

  // framerate
  auto framerate = std::make_shared<SwitchComponent>(mWindow);
  framerate->setState(Settings::getInstance()->getBool("DrawFramerate"));
  s->addWithLabel(_("SHOW FRAMERATE"), framerate);
  s->addSaveFunc(
		 [framerate] { Settings::getInstance()->setBool("DrawFramerate", framerate->getState()); });

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
  
  // gamelists
  auto save_gamelists = std::make_shared<SwitchComponent>(mWindow);
  save_gamelists->setState(Settings::getInstance()->getBool("SaveGamelistsOnExit"));
  s->addWithLabel(_("SAVE METADATA ON EXIT"), save_gamelists);
  s->addSaveFunc([save_gamelists] { Settings::getInstance()->setBool("SaveGamelistsOnExit", save_gamelists->getState()); });

  auto parse_gamelists = std::make_shared<SwitchComponent>(mWindow);
  parse_gamelists->setState(Settings::getInstance()->getBool("ParseGamelistOnly"));
  s->addWithLabel(_("PARSE GAMESLISTS ONLY"), parse_gamelists);
  s->addSaveFunc([parse_gamelists] { Settings::getInstance()->setBool("ParseGamelistOnly", parse_gamelists->getState()); });

  auto local_art = std::make_shared<SwitchComponent>(mWindow);
  local_art->setState(Settings::getInstance()->getBool("LocalArt"));
  s->addWithLabel(_("SEARCH FOR LOCAL ART"), local_art);
  s->addSaveFunc([local_art] { Settings::getInstance()->setBool("LocalArt", local_art->getState()); });

  // hidden files
  auto hidden_files = std::make_shared<SwitchComponent>(mWindow);
  hidden_files->setState(Settings::getInstance()->getBool("ShowHiddenFiles"));
  s->addWithLabel(_("SHOW HIDDEN FILES"), hidden_files);
  s->addSaveFunc([hidden_files] { Settings::getInstance()->setBool("ShowHiddenFiles", hidden_files->getState()); });

  // maximum vram
  auto max_vram = std::make_shared<SliderComponent>(mWindow, 0.f, 1000.f, 10.f, "Mb");
  max_vram->setValue((float)(Settings::getInstance()->getInt("MaxVRAM")));
  s->addWithLabel(_("VRAM LIMIT"), max_vram);
  s->addSaveFunc([max_vram] { Settings::getInstance()->setInt("MaxVRAM", (int)round(max_vram->getValue())); });
  
  mWindow->pushGui(s);
}

void GuiMenu::openSoundSettings_batocera() {
  auto s = new GuiSettings(mWindow, _("SOUND SETTINGS").c_str());

  // volume
  auto volume = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
  volume->setValue((float) VolumeControl::getInstance()->getVolume());
  s->addWithLabel(_("SYSTEM VOLUME"), volume);

  // disable sounds
  auto music_enabled = std::make_shared<SwitchComponent>(mWindow);
  music_enabled->setState(!(SystemConf::getInstance()->get("audio.bgmusic") == "0"));
  s->addWithLabel(_("FRONTEND MUSIC"), music_enabled);
  s->addSaveFunc([music_enabled] {
      SystemConf::getInstance()->set("audio.bgmusic", music_enabled->getState() ? "1" : "0");
      if (music_enabled->getState())
	AudioManager::getInstance()->playRandomMusic();
      else
	AudioManager::getInstance()->stopMusic();
    });

  // disable sounds
  //auto sounds_enabled = std::make_shared<SwitchComponent>(mWindow);
  //sounds_enabled->setState(Settings::getInstance()->getBool("EnableSounds"));
  //s->addWithLabel(_("ENABLE NAVIGATION SOUNDS"), sounds_enabled);
  //s->addSaveFunc([sounds_enabled] {
  //    if (sounds_enabled->getState()
  //	  && !Settings::getInstance()->getBool("EnableSounds")
  //	  && PowerSaver::getMode() == PowerSaver::INSTANT)
  //	{
  //	  Settings::getInstance()->setString("PowerSaverMode", "default");
  //	  PowerSaver::init();
  //	}
  //    Settings::getInstance()->setBool("EnableSounds", sounds_enabled->getState());
  //  });

  auto video_audio = std::make_shared<SwitchComponent>(mWindow);
  video_audio->setState(Settings::getInstance()->getBool("VideoAudio"));
  s->addWithLabel(_("ENABLE VIDEO AUDIO"), video_audio);
  s->addSaveFunc([video_audio] { Settings::getInstance()->setBool("VideoAudio", video_audio->getState()); });
  
  // audio device
  auto optionsAudio = std::make_shared<OptionListComponent<std::string> >(mWindow, _("OUTPUT DEVICE"),
									  false);
  std::vector<std::string> availableAudio = ApiSystem::getInstance()->getAvailableAudioOutputDevices();
  std::string selectedAudio = ApiSystem::getInstance()->getCurrentAudioOutputDevice();
  if (selectedAudio.empty()) selectedAudio = "auto";

  if (SystemConf::getInstance()->get("system.es.menu") != "bartop") {
    bool vfound=false;
    for(auto it = availableAudio.begin(); it != availableAudio.end(); it++){
      std::vector<std::string> tokens;
      boost::split( tokens, (*it), boost::is_any_of(" ") );
      if(selectedAudio == (*it)) {
	vfound = true;
      }
      if(tokens.size()>= 2){
	// concatenat the ending words
	std::string vname = "";
	for(unsigned int i=1; i<tokens.size(); i++) {
	  if(i > 2) vname += " ";
	  vname += tokens.at(i);
	}
	optionsAudio->add(vname, (*it), selectedAudio == (*it));
      } else {
	optionsAudio->add((*it), (*it), selectedAudio == (*it));
      }
    }

    if(vfound == false) {
      optionsAudio->add(selectedAudio, selectedAudio, true);
    }
    s->addWithLabel(_("OUTPUT DEVICE"), optionsAudio);
  }

  s->addSaveFunc([this, optionsAudio, selectedAudio, volume] {
      bool v_need_reboot = false;

      VolumeControl::getInstance()->setVolume((int) round(volume->getValue()));
      SystemConf::getInstance()->set("audio.volume",
				     std::to_string((int) round(volume->getValue())));

      if (optionsAudio->changed()) {
	SystemConf::getInstance()->set("audio.device", optionsAudio->getSelected());
	ApiSystem::getInstance()->setAudioOutputDevice(optionsAudio->getSelected());
	v_need_reboot = true;
      }
      SystemConf::getInstance()->saveSystemConf();
      if(v_need_reboot) {
	mWindow->displayMessage(_("A REBOOT OF THE SYSTEM IS REQUIRED TO APPLY THE NEW CONFIGURATION"));
      }
    });

  mWindow->pushGui(s);
}

void GuiMenu::openNetworkSettings_batocera() {
  Window *window = mWindow;

  auto s = new GuiSettings(mWindow, _("NETWORK SETTINGS").c_str());
  auto status = std::make_shared<TextComponent>(mWindow,
						ApiSystem::getInstance()->ping() ? _("CONNECTED")
						: _("NOT CONNECTED"),
						Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
  s->addWithLabel(_("STATUS"), status);
  auto ip = std::make_shared<TextComponent>(mWindow, ApiSystem::getInstance()->getIpAdress(),
					    Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
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
  createInputTextRow(s, _("WIFI SSID"), "wifi.ssid", false);
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
	  } else {
	    window->pushGui(
			    new GuiMsgBox(window, _("WIFI CONFIGURATION ERROR"))
			    );
	  }
	}
      } else if (baseEnabled) {
	ApiSystem::getInstance()->disableWifi();
      }
    });
  mWindow->pushGui(s);
}

void GuiMenu::openRetroAchievements_batocera() {
  Window *window = mWindow;
  auto s = new GuiSettings(mWindow, _("RETROACHIEVEMENTS").c_str());
  ComponentListRow row;
  std::string RetroAchievementsString = ApiSystem::getInstance()->getRetroAchievements();
    auto RAString = std::make_shared<TextComponent>(mWindow,
						    RetroAchievementsString,
						    Font::get(FONT_SIZE_SMALL), 0x777777FF);
    row.addElement(RAString, false);
    s->addRow(row);
    mWindow->pushGui(s);
}

void GuiMenu::openScraperSettings_batocera() {
  auto s = new GuiSettings(mWindow, _("SCRAPE").c_str());

  // automatic
  {
    std::function<void()> autoAct = [this] {
      Window* window = mWindow;
      window->pushGui(new GuiMsgBox(window, _("REALLY SCRAPE?"), _("YES"),
				    [window] {
				      window->pushGui(new GuiAutoScrape(window));
				    }, _("NO"), nullptr));
    };

    ComponentListRow row;
    row.makeAcceptInputHandler(autoAct);
    auto sc_auto = std::make_shared<TextComponent>(mWindow, _("AUTOMATIC SCRAPER"),
						   Font::get(FONT_SIZE_MEDIUM),
						   0x777777FF);
    auto bracket = makeArrow(mWindow);
    row.addElement(sc_auto, false);
    s->addRow(row);
  }

  // manual
  {
    std::function<void()> manualAct = [this] { openScraperSettings(); };
    ComponentListRow row;
    row.makeAcceptInputHandler(manualAct);
    auto sc_manual = std::make_shared<TextComponent>(mWindow, _("MANUAL SCRAPER"),
						     Font::get(FONT_SIZE_MEDIUM),
						     0x777777FF);
    auto bracket = makeArrow(mWindow);
    row.addElement(sc_manual, false);
    s->addRow(row);
  }

  mWindow->pushGui(s);
}

void GuiMenu::openQuitMenu_batocera()
{
  GuiMenu::openQuitMenu_batocera_static(mWindow);
}

void GuiMenu::openQuitMenu_batocera_static(Window *window)
{
  auto s = new GuiSettings(window, _("QUIT").c_str());

  ComponentListRow row;
  row.makeAcceptInputHandler([window] {
      window->pushGui(new GuiMsgBox(window, _("REALLY RESTART?"), _("YES"),
				    [] {
				      if (ApiSystem::getInstance()->reboot() != 0) {
					LOG(LogWarning) <<
					  "Restart terminated with non-zero result!";
				      }
				    }, _("NO"), nullptr));
    });
  row.addElement(std::make_shared<TextComponent>(window, _("RESTART SYSTEM"), Font::get(FONT_SIZE_MEDIUM),
						 0x777777FF), true);
  s->addRow(row);

  row.elements.clear();
  row.makeAcceptInputHandler([window] {
      window->pushGui(new GuiMsgBox(window, _("REALLY SHUTDOWN?"), _("YES"),
				    [] {
				      if (ApiSystem::getInstance()->shutdown() != 0) {
					LOG(LogWarning) <<
					  "Shutdown terminated with non-zero result!";
				      }
				    }, _("NO"), nullptr));
    });
  row.addElement(std::make_shared<TextComponent>(window, _("SHUTDOWN SYSTEM"), Font::get(FONT_SIZE_MEDIUM),
						 0x777777FF), true);
  s->addRow(row);

  row.elements.clear();
  row.makeAcceptInputHandler([window] {
      window->pushGui(new GuiMsgBox(window, _("REALLY SHUTDOWN WITHOUT SAVING METADATAS?"), _("YES"),
				    [] {
				      if (ApiSystem::getInstance()->fastShutdown() != 0) {
					LOG(LogWarning) <<
					  "Shutdown terminated with non-zero result!";
				      }
				    }, _("NO"), nullptr));
    });
  row.addElement(std::make_shared<TextComponent>(window, _("FAST SHUTDOWN SYSTEM"), Font::get(FONT_SIZE_MEDIUM),
						 0x777777FF), true);
  s->addRow(row);
  /*if(Settings::getInstance()->getBool("ShowExit"))
    {
    row.elements.clear();
    row.makeAcceptInputHandler([window] {
    window->pushGui(new GuiMsgBox(window, _("REALLY QUIT?"), _("YES"),
    [] {
    SDL_Event ev;
    ev.type = SDL_QUIT;
    SDL_PushEvent(&ev);
    }, _("NO"), nullptr));
    });
    row.addElement(std::make_shared<TextComponent>(window, _("QUIT EMULATIONSTATION"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
    s->addRow(row);
    }*/
  //ViewController::get()->reloadAll();

  window->pushGui(s);
}

void GuiMenu::createInputTextRow(GuiSettings *gui, std::string title, const char *settingsID, bool password) {
  // LABEL
  Window *window = mWindow;
  ComponentListRow row;

  auto lbl = std::make_shared<TextComponent>(window, title, Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
  row.addElement(lbl, true); // label

  std::shared_ptr<GuiComponent> ed;

  ed = std::make_shared<TextComponent>(window, ((password &&
						 SystemConf::getInstance()->get(settingsID) != "")
						? "*********" : SystemConf::getInstance()->get(
											       settingsID)), Font::get(FONT_SIZE_MEDIUM, FONT_PATH_LIGHT), 0x777777FF, ALIGN_RIGHT);
  row.addElement(ed, true);

  auto spacer = std::make_shared<GuiComponent>(mWindow);
  spacer->setSize(Renderer::getScreenWidth() * 0.005f, 0);
  row.addElement(spacer, false);

  auto bracket = std::make_shared<ImageComponent>(mWindow);
  bracket->setImage(":/arrow.svg");
  bracket->setResize(Vector2f(0, lbl->getFont()->getLetterHeight()));
  row.addElement(bracket, false);

  auto updateVal = [ed, settingsID, password](const std::string &newVal) {
    if (!password)
      ed->setValue(newVal);
    else {
      ed->setValue("*********");
    }
    SystemConf::getInstance()->set(settingsID, newVal);
  }; // ok callback (apply new value to ed)

  row.makeAcceptInputHandler([this, title, updateVal, settingsID] {
      if (Settings::getInstance()->getBool("UseOSK")) {
	mWindow->pushGui(
			 new GuiTextEditPopupKeyboard(mWindow, title, SystemConf::getInstance()->get(settingsID),
						      updateVal, false));
      } else {
	mWindow->pushGui(
			 new GuiTextEditPopup(mWindow, title, SystemConf::getInstance()->get(settingsID),
					      updateVal, false));
      }
    });
  gui->addRow(row);
}

void GuiMenu::popSystemConfigurationGui(SystemData *systemData, std::string previouslySelectedEmulator) const {
  // The system configuration
  GuiSettings *systemConfiguration = new GuiSettings(mWindow,
						     systemData->getFullName().c_str());
  //Emulator choice
  auto emu_choice = std::make_shared<OptionListComponent<std::string>>(mWindow, "emulator", false);
  bool selected = false;
  std::string selectedEmulator = "";

  for (auto it = systemData->getEmulators()->begin(); it != systemData->getEmulators()->end(); it++) {
    bool found;
    std::string curEmulatorName = it->first;
    if (previouslySelectedEmulator != "") {
      // We just changed the emulator
      found = previouslySelectedEmulator == curEmulatorName;
    } else {
      found = (SystemConf::getInstance()->get(systemData->getName() + ".emulator") == curEmulatorName);
    }
    if (found) {
      selectedEmulator = curEmulatorName;
    }
    selected = selected || found;
    emu_choice->add(curEmulatorName, curEmulatorName, found);
  }

  emu_choice->add(_("AUTO"), "auto", !selected);
  emu_choice->setSelectedChangedCallback([this, systemConfiguration, systemData](std::string s) {
      popSystemConfigurationGui(systemData, s);
      delete systemConfiguration;
    });
  systemConfiguration->addWithLabel(_("Emulator"), emu_choice);

  // Core choice
  auto core_choice = std::make_shared<OptionListComponent<std::string> >(mWindow, _("Core"), false);
  selected = false;
  for (auto emulator = systemData->getEmulators()->begin();
       emulator != systemData->getEmulators()->end(); emulator++) {
    if (selectedEmulator == emulator->first) {
      for (auto core = emulator->second->begin(); core != emulator->second->end(); core++) {
	bool found = (SystemConf::getInstance()->get(systemData->getName() + ".core") == *core);
	selected = selected || found;
	core_choice->add(*core, *core, found);
      }
    }
  }
  core_choice->add(_("AUTO"), "auto", !selected);
  systemConfiguration->addWithLabel(_("Core"), core_choice);


  // Screen ratio choice
  auto ratio_choice = createRatioOptionList(mWindow, systemData->getName());
  systemConfiguration->addWithLabel(_("GAME RATIO"), ratio_choice);
  // video resolution mode
  auto videoResolutionMode_choice = createVideoResolutionModeOptionList(mWindow, systemData->getName());
  systemConfiguration->addWithLabel(_("VIDEO MODE"), videoResolutionMode_choice);
  // smoothing
  auto smoothing_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SMOOTH GAMES"));
  smoothing_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(systemData->getName() + ".smooth") != "0" && SystemConf::getInstance()->get(systemData->getName() + ".smooth") != "1");
  smoothing_enabled->add(_("ON"),   "1",    SystemConf::getInstance()->get(systemData->getName() + ".smooth") == "1");
  smoothing_enabled->add(_("OFF"),  "0",    SystemConf::getInstance()->get(systemData->getName() + ".smooth") == "0");
  systemConfiguration->addWithLabel(_("SMOOTH GAMES"), smoothing_enabled);
  // rewind
  auto rewind_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("REWIND"));
  rewind_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(systemData->getName() + ".rewind") != "0" && SystemConf::getInstance()->get(systemData->getName() + ".rewind") != "1");
  rewind_enabled->add(_("ON"),   "1",    SystemConf::getInstance()->get(systemData->getName() + ".rewind") == "1");
  rewind_enabled->add(_("OFF"),  "0",    SystemConf::getInstance()->get(systemData->getName() + ".rewind") == "0");
  systemConfiguration->addWithLabel(_("REWIND"), rewind_enabled);
  // autosave
  auto autosave_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("AUTO SAVE/LOAD"));
  autosave_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(systemData->getName() + ".autosave") != "0" && SystemConf::getInstance()->get(systemData->getName() + ".autosave") != "1");
  autosave_enabled->add(_("ON"),   "1",    SystemConf::getInstance()->get(systemData->getName() + ".autosave") == "1");
  autosave_enabled->add(_("OFF"),  "0",    SystemConf::getInstance()->get(systemData->getName() + ".autosave") == "0");
  systemConfiguration->addWithLabel(_("AUTO SAVE/LOAD"), autosave_enabled);

  systemConfiguration->addSaveFunc(
				   [systemData, smoothing_enabled, rewind_enabled, ratio_choice, videoResolutionMode_choice, emu_choice, core_choice, autosave_enabled] {
				     if(ratio_choice->changed()){
				       SystemConf::getInstance()->set(systemData->getName() + ".ratio", ratio_choice->getSelected());
				     }
				     if(videoResolutionMode_choice->changed()){
				       SystemConf::getInstance()->set(systemData->getName() + ".videomode", videoResolutionMode_choice->getSelected());
				     }
				     if(rewind_enabled->changed()) {
				       SystemConf::getInstance()->set(systemData->getName() + ".rewind", rewind_enabled->getSelected());
				     }
				     if(smoothing_enabled->changed()){
				       SystemConf::getInstance()->set(systemData->getName() + ".smooth", smoothing_enabled->getSelected());
				     }

				     // the menu GuiMenu::popSystemConfigurationGui is a hack
				     // if you change any option except emulator and change the emulator, the value is lost
				     // this is especially bad for core while the changed() value is lost too.
				     // to avoid this issue, instead of reprogramming this part, i will force a save of the emulator and core
				     // at each GuiMenu::popSystemConfigurationGui call, including the ending saving one (when changed() values are bad)
				     if(emu_choice->changed()) {
				       SystemConf::getInstance()->set(systemData->getName() + ".emulator", emu_choice->getSelected());
				     }
				     if(core_choice->changed()){
				       SystemConf::getInstance()->set(systemData->getName() + ".core", core_choice->getSelected());
				     }
				     if(autosave_enabled->changed()) {
				       SystemConf::getInstance()->set(systemData->getName() + ".autosave", autosave_enabled->getSelected());
				     }
				     SystemConf::getInstance()->saveSystemConf();
				   });
  mWindow->pushGui(systemConfiguration);
}

std::shared_ptr<OptionListComponent<std::string>> GuiMenu::createRatioOptionList(Window *window,
                                                                                 std::string configname) const {
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

std::shared_ptr<OptionListComponent<std::string>> GuiMenu::createVideoResolutionModeOptionList(Window *window,
											       std::string configname) const {
  auto videoResolutionMode_choice = std::make_shared<OptionListComponent<std::string> >(window, _("VIDEO MODE"), false);
  std::string currentVideoMode = SystemConf::getInstance()->get(configname + ".videomode");
  if (currentVideoMode.empty()) {
    currentVideoMode = std::string("auto");
  }

  std::vector<std::string> videoResolutionModeMap = ApiSystem::getInstance()->getVideoModes();
  videoResolutionMode_choice->add(_("AUTO"), "auto", currentVideoMode == "auto");
  for (auto videoMode = videoResolutionModeMap.begin(); videoMode != videoResolutionModeMap.end(); videoMode++) {
    std::vector<std::string> tokens;
    boost::split( tokens, (*videoMode), boost::is_any_of(":") );
    // concatenat the ending words
    std::string vname = "";
    for(unsigned int i=1; i<tokens.size(); i++) {
      if(i > 1) vname += ":";
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

std::vector<std::string> GuiMenu::getDecorationsSets()
{
  std::vector<std::string> sets;

  static const size_t pathCount = 2;
  std::string paths[pathCount] = {
    "/usr/share/batocera/datainit/decorations",
    "/userdata/decorations"
  };
  Utils::FileSystem::stringList dirContent;
  std::string folder;

  for(size_t i = 0; i < pathCount; i++) {
    if(!Utils::FileSystem::isDirectory(paths[i])) continue;
    dirContent = Utils::FileSystem::getDirContent(paths[i]);
    for (Utils::FileSystem::stringList::const_iterator it = dirContent.cbegin(); it != dirContent.cend(); ++it) {
      if(Utils::FileSystem::isDirectory(*it)) {
	folder = *it;
	folder = folder.substr(paths[i].size()+1);
	sets.push_back(folder);
      }
    }
  }

  // sort and remove duplicates
  sort(sets.begin(), sets.end());
  sets.erase(unique(sets.begin(), sets.end()), sets.end());
  return sets;
}

GuiMenu::~GuiMenu() {
  clearLoadedInput();
}
