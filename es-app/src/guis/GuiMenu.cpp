#include <list>
#include <algorithm>
#include <string>
#include <sstream>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include <functional>
#include <LibretroRatio.h>

#include "EmulationStation.h"
#include "guis/GuiMenu.h"
#include "Window.h"
#include "Sound.h"
#include "Log.h"
#include "Settings.h"
#include "RecalboxSystem.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiSettings.h"
#include "guis/GuiScraperStart.h"
#include "guis/GuiBackupStart.h"
#include "guis/GuiInstallStart.h"
#include "guis/GuiDetectDevice.h"
#include "guis/GuiUpdate.h"
#include "guis/GuiAutoScrape.h"
#include "guis/GuiRomsManager.h"
#include "views/ViewController.h"
#include "AudioManager.h"

#include "components/ButtonComponent.h"
#include "components/SwitchComponent.h"
#include "components/SliderComponent.h"
#include "components/TextComponent.h"
#include "components/OptionListComponent.h"
#include "components/MenuComponent.h"
#include "VolumeControl.h"
#include "scrapers/GamesDBScraper.h"

#include "guis/GuiTextEditPopup.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "GuiLoading.h"

#include "RecalboxConf.h"

namespace fs = boost::filesystem;

void GuiMenu::createInputTextRow(GuiSettings *gui, std::string title, const char *settingsID, bool password) {
    // LABEL
    Window *window = mWindow;
    ComponentListRow row;

    auto lbl = std::make_shared<TextComponent>(window, title, Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
    row.addElement(lbl, true); // label

    std::shared_ptr<GuiComponent> ed;

    ed = std::make_shared<TextComponent>(window, ((password &&
                                                   RecalboxConf::getInstance()->get(settingsID) != "")
                                                  ? "*********" : RecalboxConf::getInstance()->get(
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
        RecalboxConf::getInstance()->set(settingsID, newVal);
    }; // ok callback (apply new value to ed)

    row.makeAcceptInputHandler([this, title, updateVal, settingsID] {
        if (Settings::getInstance()->getBool("UseOSK")) {
            mWindow->pushGui(
                new GuiTextEditPopupKeyboard(mWindow, title, RecalboxConf::getInstance()->get(settingsID),
                                        updateVal, false));
        } else {
            mWindow->pushGui(
                new GuiTextEditPopup(mWindow, title, RecalboxConf::getInstance()->get(settingsID),
                                       updateVal, false));
        }
    });
    gui->addRow(row);
}

GuiMenu::GuiMenu(Window *window) : GuiComponent(window), mMenu(window, _("MAIN MENU").c_str()), mVersion(window) {
    // MAIN MENU

    // KODI >
    // ROM MANAGER >
    // SYSTEM >
    // GAMES >
    // CONTROLLERS >
    // UI SETTINGS >
    // SOUND SETTINGS >
    // NETWORK >
    // SCRAPER >
    // QUIT >

#if ENABLE_KODI == 1
    if (RecalboxConf::getInstance()->get("kodi.enabled") == "1") {
      addEntry(_("KODI MEDIA CENTER").c_str(), 0x777777FF, true,
                 [this] {
                     Window *window = mWindow;

                     if (!RecalboxSystem::getInstance()->launchKodi(window)) {
                         LOG(LogWarning) << "Shutdown terminated with non-zero result!";
                     }
                 });
    }
#endif

    if (Settings::getInstance()->getBool("RomsManager")) {
      addEntry("ROMS MANAGER", 0x777777FF, true, [this] {
            mWindow->pushGui(new GuiRomsManager(mWindow));
        });
    }
    if (RecalboxConf::getInstance()->get("system.es.menu") != "bartop") {
      addEntry(_("SYSTEM SETTINGS").c_str(), 0x777777FF, true,
                 [this] {
                     Window *window = mWindow;

                     auto s = new GuiSettings(mWindow, _("SYSTEM SETTINGS").c_str());

                     // system informations
                     {
                         ComponentListRow row;
                         std::function<void()> openGui = [this] {
			   GuiSettings *informationsGui = new GuiSettings(mWindow, _("INFORMATION").c_str());

			   auto version = std::make_shared<TextComponent>(mWindow,
			   						  RecalboxSystem::getInstance()->getVersion(),
			   						  Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
			   informationsGui->addWithLabel(_("VERSION"), version);
			   bool warning = RecalboxSystem::getInstance()->isFreeSpaceLimit();
			   auto space = std::make_shared<TextComponent>(mWindow,
			   						RecalboxSystem::getInstance()->getFreeSpaceInfo(),
			   						Font::get(FONT_SIZE_MEDIUM),
			   						warning ? 0xFF0000FF : 0x777777FF);
			   informationsGui->addWithLabel(_("DISK USAGE"), space);

			   // various informations
			   std::vector<std::string> infos = RecalboxSystem::getInstance()->getSystemInformations();
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

			       auto space = std::make_shared<TextComponent>(mWindow,
									    vname,
									    Font::get(FONT_SIZE_MEDIUM),
									    0x777777FF);
			       informationsGui->addWithLabel(tokens.at(0), space);
			     }
			   }

			   // support
			   {
			     ComponentListRow row;
			     row.makeAcceptInputHandler([this] {
				 mWindow->pushGui(new GuiMsgBox(mWindow, _("CREATE A SUPPORT FILE ?"), _("YES"),
								[this] {
								  if(RecalboxSystem::getInstance()->generateSupportFile()) {
								    mWindow->pushGui(new GuiMsgBox(mWindow, _("FILE GENERATED SUCCESSFULLY"), _("OK")));
								  } else {
								    mWindow->pushGui(new GuiMsgBox(mWindow, _("FILE GENERATION FAILED"), _("OK")));
								  }
								}, _("NO"), nullptr));
			       });
			     auto supportFile = std::make_shared<TextComponent>(mWindow, _("CREATE A SUPPORT FILE"),
										Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
			     row.addElement(supportFile, false);
			     informationsGui->addRow(row);
			   }

			   mWindow->pushGui(informationsGui);
                         };
                         row.makeAcceptInputHandler(openGui);
                         auto informationsSettings = std::make_shared<TextComponent>(mWindow, _("INFORMATION"),
                                                                             Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
                         auto bracket = makeArrow(mWindow);
                         row.addElement(informationsSettings, true);
                         row.addElement(bracket, false);
                         s->addRow(row);
                     }

                     std::vector<std::string> availableStorage = RecalboxSystem::getInstance()->getAvailableStorageDevices();
                     std::string selectedStorage = RecalboxSystem::getInstance()->getCurrentStorage();

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
                     std::string language = RecalboxConf::getInstance()->get("system.language");
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
                     std::vector<std::string> availableOverclocking = RecalboxSystem::getInstance()->getAvailableOverclocking();

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
                                     RecalboxConf::getInstance()->get("updates.enabled") == "1");
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
                                 RecalboxConf::getInstance()->set("updates.enabled",
                                                                  updates_enabled->getState() ? "1" : "0");
                                 RecalboxConf::getInstance()->saveRecalboxConf();
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

#if ENABLE_KODI == 1
                     //Kodi
                     {
                         ComponentListRow row;
                         std::function<void()> openGui = [this] {
			   GuiSettings *kodiGui = new GuiSettings(mWindow, _("KODI SETTINGS").c_str());
                             auto kodiEnabled = std::make_shared<SwitchComponent>(mWindow);
                             kodiEnabled->setState(RecalboxConf::getInstance()->get("kodi.enabled") == "1");
			   kodiGui->addWithLabel(_("ENABLE KODI"), kodiEnabled);
                             auto kodiAtStart = std::make_shared<SwitchComponent>(mWindow);
                             kodiAtStart->setState(
                                     RecalboxConf::getInstance()->get("kodi.atstartup") == "1");
			   kodiGui->addWithLabel(_("KODI AT START"), kodiAtStart);
                             auto kodiX = std::make_shared<SwitchComponent>(mWindow);
                             kodiX->setState(RecalboxConf::getInstance()->get("kodi.xbutton") == "1");
			   kodiGui->addWithLabel(_("START KODI WITH X"), kodiX);
                             kodiGui->addSaveFunc([kodiEnabled, kodiAtStart, kodiX] {
                                 RecalboxConf::getInstance()->set("kodi.enabled",
                                                                  kodiEnabled->getState() ? "1" : "0");
                                 RecalboxConf::getInstance()->set("kodi.atstartup",
                                                                  kodiAtStart->getState() ? "1" : "0");
                                 RecalboxConf::getInstance()->set("kodi.xbutton",
                                                                  kodiX->getState() ? "1" : "0");
                                 RecalboxConf::getInstance()->saveRecalboxConf();
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
			   securityEnabled->setState(RecalboxConf::getInstance()->get("system.security.enabled") == "1");
			   securityGui->addWithLabel(_("ENFORCE SECURITY"), securityEnabled);

			   auto rootpassword = std::make_shared<TextComponent>(mWindow,
									       RecalboxSystem::getInstance()->getRootPassword(),
									       Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
			   securityGui->addWithLabel(_("ROOT PASSWORD"), rootpassword);

			   securityGui->addSaveFunc([this, securityEnabled] {
			       Window* window = this->mWindow;
			       bool reboot = false;

			       if (securityEnabled->changed()) {
				 RecalboxConf::getInstance()->set("system.security.enabled",
								  securityEnabled->getState() ? "1" : "0");
				 RecalboxConf::getInstance()->saveRecalboxConf();
				 reboot = true;
			       }

			       if (reboot) {
			       	 window->pushGui(
						 new GuiMsgBox(window, _("THE SYSTEM WILL NOW REBOOT"), _("OK"),
							       [window] {
								 if (runRestartCommand() != 0) {
								   LOG(LogWarning) << "Reboot terminated with non-zero result!";
								 }
							       })
						 );
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
                             RecalboxSystem::getInstance()->setStorage(optionsStorage->getSelected());
                             reboot = true;
                         }

                         if (Settings::getInstance()->getString("Overclock") != overclock_choice->getSelected() ||
			     (Settings::getInstance()->getString("Overclock") == "" && overclock_choice->getSelected() != "none")) {
                             Settings::getInstance()->setString("Overclock", overclock_choice->getSelected());
                             RecalboxSystem::getInstance()->setOverclock(overclock_choice->getSelected());
                             reboot = true;
                         }
                         if (language != language_choice->getSelected()) {
                             RecalboxConf::getInstance()->set("system.language",
                                                              language_choice->getSelected());
                             RecalboxConf::getInstance()->saveRecalboxConf();
                             reboot = true;
                         }
                         if (reboot) {
                             window->pushGui(
					     new GuiMsgBox(window, _("THE SYSTEM WILL NOW REBOOT"), _("OK"),
                                           [window] {
                                               if (runRestartCommand() != 0) {
                                                   LOG(LogWarning) << "Reboot terminated with non-zero result!";
                                               }
                                           })
                             );
                         }

                     });
                     mWindow->pushGui(s);

                 });
    }

    addEntry(_("GAMES SETTINGS").c_str(), 0x777777FF, true,
             [this] {
                 auto s = new GuiSettings(mWindow, _("GAMES SETTINGS").c_str());
                 if (RecalboxConf::getInstance()->get("system.es.menu") != "bartop") {

                     // Screen ratio choice
                     auto ratio_choice = createRatioOptionList(mWindow, "global");
                     s->addWithLabel(_("GAME RATIO"), ratio_choice);
                     s->addSaveFunc([ratio_choice] {
                         RecalboxConf::getInstance()->set("global.ratio", ratio_choice->getSelected());
                         RecalboxConf::getInstance()->saveRecalboxConf();
                     });
                 }
                 // smoothing
                 auto smoothing_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SMOOTH GAMES"));
		 smoothing_enabled->add(_("AUTO"), "auto", RecalboxConf::getInstance()->get("global.smooth") != "0" && RecalboxConf::getInstance()->get("global.smooth") != "1");
		 smoothing_enabled->add(_("ON"),   "1",    RecalboxConf::getInstance()->get("global.smooth") == "1");
		 smoothing_enabled->add(_("OFF"),  "0",    RecalboxConf::getInstance()->get("global.smooth") == "0");
                 s->addWithLabel(_("SMOOTH GAMES"), smoothing_enabled);

                 // rewind
                 auto rewind_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("REWIND"));
		 rewind_enabled->add(_("AUTO"), "auto", RecalboxConf::getInstance()->get("global.rewind") != "0" && RecalboxConf::getInstance()->get("global.rewind") != "1");
		 rewind_enabled->add(_("ON"),   "1",    RecalboxConf::getInstance()->get("global.rewind") == "1");
		 rewind_enabled->add(_("OFF"),  "0",    RecalboxConf::getInstance()->get("global.rewind") == "0");
                 s->addWithLabel(_("REWIND"), rewind_enabled);

                 // autosave/load
                 auto autosave_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("AUTO SAVE/LOAD"));
		 autosave_enabled->add(_("AUTO"), "auto", RecalboxConf::getInstance()->get("global.autosave") != "0" && RecalboxConf::getInstance()->get("global.autosave") != "1");
		 autosave_enabled->add(_("ON"),   "1",    RecalboxConf::getInstance()->get("global.autosave") == "1");
		 autosave_enabled->add(_("OFF"),  "0",    RecalboxConf::getInstance()->get("global.autosave") == "0");
                 s->addWithLabel(_("AUTO SAVE/LOAD"), autosave_enabled);

                 // Shaders preset

                 auto shaders_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SHADERS SET"),
                                                                                            false);
                 std::string currentShader = RecalboxConf::getInstance()->get("global.shaderset");
                 if (currentShader.empty()) {
                     currentShader = std::string("none");
                 }

                 shaders_choices->add(_("NONE"), "none", currentShader == "none");
                 shaders_choices->add(_("SCANLINES"), "scanlines", currentShader == "scanlines");
                 shaders_choices->add(_("RETRO"), "retro", currentShader == "retro");
                 s->addWithLabel(_("SHADERS SET"), shaders_choices);
                 // Integer scale
                 auto integerscale_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INTEGER SCALE (PIXEL PERFECT)"));
		 integerscale_enabled->add(_("AUTO"), "auto", RecalboxConf::getInstance()->get("global.integerscale") != "0" && RecalboxConf::getInstance()->get("global.integerscale") != "1");
		 integerscale_enabled->add(_("ON"),   "1",    RecalboxConf::getInstance()->get("global.integerscale") == "1");
		 integerscale_enabled->add(_("OFF"),  "0",    RecalboxConf::getInstance()->get("global.integerscale") == "0");
                 s->addWithLabel(_("INTEGER SCALE (PIXEL PERFECT)"), integerscale_enabled);
                 s->addSaveFunc([integerscale_enabled] {
                     RecalboxConf::getInstance()->set("global.integerscale", integerscale_enabled->getSelected());
                     RecalboxConf::getInstance()->saveRecalboxConf();
                 });

		 // decorations
		 {
		   auto decorations = std::make_shared<OptionListComponent<std::string> >(mWindow, _("DECORATION"), false);
		   std::vector<std::string> decorations_item;
		   decorations_item.push_back(_("NONE"));

		   std::vector<std::string> sets = GuiMenu::getDecorationsSets();
		   for(auto it = sets.begin(); it != sets.end(); it++) {
		     decorations_item.push_back(*it);
		   }

		   for (auto it = decorations_item.begin(); it != decorations_item.end(); it++) {
		     decorations->add(*it, *it,
				      (RecalboxConf::getInstance()->get("global.bezel") == *it)
				      ||
				      (RecalboxConf::getInstance()->get("global.bezel") == "" && *it == _("NONE"))
				      );
		   }
		   s->addWithLabel(_("DECORATION"), decorations);
		   s->addSaveFunc([decorations] {
		       RecalboxConf::getInstance()->set("global.bezel", decorations->getSelected() == _("NONE") ? "" : decorations->getSelected());
		       RecalboxConf::getInstance()->saveRecalboxConf();
		     });
		 }

                 if (RecalboxConf::getInstance()->get("system.es.menu") != "bartop") {

                     // Retroachievements
                     {
                         ComponentListRow row;
                         std::function<void()> openGui = [this] {
                             GuiSettings *retroachievements = new GuiSettings(mWindow,
                                                                              _("RETROACHIEVEMENTS SETTINGS").c_str());
                             // retroachievements_enable
                             auto retroachievements_enabled = std::make_shared<SwitchComponent>(mWindow);
                             retroachievements_enabled->setState(
                                     RecalboxConf::getInstance()->get("global.retroachievements") == "1");
                             retroachievements->addWithLabel(_("RETROACHIEVEMENTS"), retroachievements_enabled);

                             // retroachievements_hardcore_mode
                             auto retroachievements_hardcore_enabled = std::make_shared<SwitchComponent>(mWindow);
                             retroachievements_hardcore_enabled->setState(
                                     RecalboxConf::getInstance()->get("global.retroachievements.hardcore") == "1");
                             retroachievements->addWithLabel(_("HARDCORE MODE"), retroachievements_hardcore_enabled);

                             // retroachievements_leaderboards
                             auto retroachievements_leaderboards_enabled = std::make_shared<SwitchComponent>(mWindow);
                             retroachievements_leaderboards_enabled->setState(
                                     RecalboxConf::getInstance()->get("global.retroachievements.leaderboards") == "1");
                             retroachievements->addWithLabel(_("LEADERBOARDS"), retroachievements_leaderboards_enabled);

                             // retroachievements_verbose_mode
                             auto retroachievements_verbose_enabled = std::make_shared<SwitchComponent>(mWindow);
                             retroachievements_verbose_enabled->setState(
                                     RecalboxConf::getInstance()->get("global.retroachievements.verbose") == "1");
                             retroachievements->addWithLabel(_("VERBOSE MODE"), retroachievements_verbose_enabled);

                             // retroachievements_automatic_screenshot
                             auto retroachievements_screenshot_enabled = std::make_shared<SwitchComponent>(mWindow);
                             retroachievements_screenshot_enabled->setState(
                                     RecalboxConf::getInstance()->get("global.retroachievements.screenshot") == "1");
                             retroachievements->addWithLabel(_("AUTOMATIC SCREESHOT"), retroachievements_screenshot_enabled);

                             // retroachievements, username, password
                             createInputTextRow(retroachievements, _("USERNAME"), "global.retroachievements.username",
                                                false);
                             createInputTextRow(retroachievements, _("PASSWORD"), "global.retroachievements.password",
                                                true);


                             retroachievements->addSaveFunc([retroachievements_enabled, retroachievements_hardcore_enabled, retroachievements_leaderboards_enabled,
                                                             retroachievements_verbose_enabled, retroachievements_screenshot_enabled] {
                                 RecalboxConf::getInstance()->set("global.retroachievements",
                                                                  retroachievements_enabled->getState() ? "1" : "0");
                                 RecalboxConf::getInstance()->set("global.retroachievements.hardcore",
                                                                  retroachievements_hardcore_enabled->getState() ? "1" : "0");
                                 RecalboxConf::getInstance()->set("global.retroachievements.leaderboards",
                                                                  retroachievements_leaderboards_enabled->getState() ? "1" : "0");
                                 RecalboxConf::getInstance()->set("global.retroachievements.verbose",
                                                                  retroachievements_verbose_enabled->getState() ? "1" : "0");
                                 RecalboxConf::getInstance()->set("global.retroachievements.screenshot",
                                                                  retroachievements_screenshot_enabled->getState() ? "1" : "0");
                                 RecalboxConf::getInstance()->saveRecalboxConf();
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
			 std::vector<BiosSystem> biosInformations = RecalboxSystem::getInstance()->getBiosInformations();

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
                                 if ((*system) != SystemData::getFavoriteSystem()) {
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
                             }
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
                                                               window->renderShutdownScreen();
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
                     RecalboxConf::getInstance()->set("global.smooth", smoothing_enabled->getSelected());
                     RecalboxConf::getInstance()->set("global.rewind", rewind_enabled->getSelected());
                     RecalboxConf::getInstance()->set("global.shaderset", shaders_choices->getSelected());
                     RecalboxConf::getInstance()->set("global.autosave", autosave_enabled->getSelected());
                     RecalboxConf::getInstance()->saveRecalboxConf();
                 });
                 mWindow->pushGui(s);
             }

    );

    if (RecalboxConf::getInstance()->get("system.es.menu") != "bartop") {
      addEntry(_("CONTROLLERS SETTINGS").c_str(), 0x777777FF, true, [this] { this->createConfigInput(); });
    }
    if (RecalboxConf::getInstance()->get("system.es.menu") != "bartop") {

      addEntry(_("UI SETTINGS").c_str(), 0x777777FF, true,
                 [this] {
		 auto s = new GuiSettings(mWindow, _("UI SETTINGS").c_str());

 		     // video device
		     auto optionsVideo = std::make_shared<OptionListComponent<std::string> >(mWindow, _("VIDEO OUTPUT"), false);
		     std::string currentDevice = RecalboxConf::getInstance()->get("global.videooutput");
		     if (currentDevice.empty()) currentDevice = "auto";

		     std::vector<std::string> availableVideo = RecalboxSystem::getInstance()->getAvailableVideoOutputDevices();

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
			 if (currentDevice != optionsVideo->getSelected()) {
			   RecalboxConf::getInstance()->set("global.videooutput", optionsVideo->getSelected());
			   RecalboxConf::getInstance()->saveRecalboxConf();
			   this->mWindow->pushGui(
					   new GuiMsgBox(this->mWindow, _("THE SYSTEM WILL NOW REBOOT"), _("OK"),
							 [] {
							   if (runRestartCommand() != 0) {
							     LOG(LogWarning) << "Reboot terminated with non-zero result!";
							   }
							 })
					   );
			 }
		       });

                     // overscan
                     auto overscan_enabled = std::make_shared<SwitchComponent>(mWindow);
                     overscan_enabled->setState(Settings::getInstance()->getBool("Overscan"));
                     s->addWithLabel(_("OVERSCAN"), overscan_enabled);
                     s->addSaveFunc([overscan_enabled] {
                         if (Settings::getInstance()->getBool("Overscan") != overscan_enabled->getState()) {
                             Settings::getInstance()->setBool("Overscan", overscan_enabled->getState());
                             RecalboxSystem::getInstance()->setOverscan(overscan_enabled->getState());
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

                     // screensaver behavior
                     auto screensaver_behavior = std::make_shared<OptionListComponent<std::string> >(mWindow,
                                                                                                     _("TRANSITION STYLE"),
                                                                                                     false);
                     std::vector<std::string> screensavers;
                     screensavers.push_back("dim");
                     screensavers.push_back("black");
                     for (auto it = screensavers.begin(); it != screensavers.end(); it++)
                         screensaver_behavior->add(*it, *it,
                                                   Settings::getInstance()->getString("ScreenSaverBehavior") == *it);
                     s->addWithLabel(_("SCREENSAVER BEHAVIOR"), screensaver_behavior);
                     s->addSaveFunc([screensaver_behavior] {
                         Settings::getInstance()->setString("ScreenSaverBehavior", screensaver_behavior->getSelected());
                     });

                     // framerate
                     auto framerate = std::make_shared<SwitchComponent>(mWindow);
                     framerate->setState(Settings::getInstance()->getBool("DrawFramerate"));
                     s->addWithLabel(_("SHOW FRAMERATE"), framerate);
                     s->addSaveFunc(
                             [framerate] { Settings::getInstance()->setBool("DrawFramerate", framerate->getState()); });

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
                         Settings::getInstance()->setString("TransitionStyle", transition_style->getSelected());
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
                         s->addSaveFunc([this, window, theme_set] {
                             bool needReload = true;
                             if (Settings::getInstance()->getString("ThemeSet") != theme_set->getSelected())
                                 needReload = true;

                             Settings::getInstance()->setString("ThemeSet", theme_set->getSelected());

							if (needReload) {
								window->pushGui(new GuiMsgBox(window, _("REALLY UPDATE GAMES LISTS ?"), _("YES"),
									[this, window] {
									ViewController::get()->goToStart();
									window->renderShutdownScreen();
									delete ViewController::get();
									SystemData::reloadSystemsTheme();
									GuiComponent *gui;
									while ((gui = window->peekGui()) != NULL) {
										window->removeGui(gui);
										delete gui;
									}
									ViewController::init(window);
									ViewController::get()->reloadAll();
									window->pushGui(ViewController::get());
									}, _("NO"), nullptr)
								);
								//ViewController::get()->reloadAll(); // TODO - replace this with some sort of signal-based implementation
							}
                         });
                     }

		     // maximum vram
		     auto max_vram = std::make_shared<SliderComponent>(mWindow, 0.f, 1000.f, 10.f, "Mb");
		     max_vram->setValue((float)(Settings::getInstance()->getInt("MaxVRAM")));
		     s->addWithLabel("VRAM LIMIT", max_vram);
		     s->addSaveFunc([max_vram] { Settings::getInstance()->setInt("MaxVRAM", (int)round(max_vram->getValue())); });

                     mWindow->pushGui(s);
                 });
    }

    addEntry(_("SOUND SETTINGS").c_str(), 0x777777FF, true,
             [this] {
	       auto s = new GuiSettings(mWindow, _("SOUND SETTINGS").c_str());

                 // volume
                 auto volume = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
                 volume->setValue((float) VolumeControl::getInstance()->getVolume());
                 s->addWithLabel(_("SYSTEM VOLUME"), volume);

                 // disable sounds
                 auto sounds_enabled = std::make_shared<SwitchComponent>(mWindow);
                 sounds_enabled->setState(!(RecalboxConf::getInstance()->get("audio.bgmusic") == "0"));
                 s->addWithLabel(_("FRONTEND MUSIC"), sounds_enabled);

		 // audio device
		 auto optionsAudio = std::make_shared<OptionListComponent<std::string> >(mWindow, _("OUTPUT DEVICE"),
											 false);
                 std::string currentDevice = RecalboxConf::getInstance()->get("audio.device");
                 if (currentDevice.empty()) currentDevice = "auto";

		 std::vector<std::string> availableAudio = RecalboxSystem::getInstance()->getAvailableAudioOutputDevices();
		 std::string selectedAudio = RecalboxSystem::getInstance()->getCurrentAudioOutputDevice();

		 if (RecalboxConf::getInstance()->get("system.es.menu") != "bartop") {
		   for(auto it = availableAudio.begin(); it != availableAudio.end(); it++){
		     std::vector<std::string> tokens;
		     boost::split( tokens, (*it), boost::is_any_of(" ") );
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
		   s->addWithLabel(_("OUTPUT DEVICE"), optionsAudio);
		 }

                 s->addSaveFunc([this, optionsAudio, currentDevice, sounds_enabled, volume] {
		     bool v_need_reboot = false;

                     VolumeControl::getInstance()->setVolume((int) round(volume->getValue()));
                     RecalboxConf::getInstance()->set("audio.volume",
                                                      std::to_string((int) round(volume->getValue())));

                     RecalboxConf::getInstance()->set("audio.bgmusic",
                                                      sounds_enabled->getState() ? "1" : "0");
                     if (!sounds_enabled->getState())
                         AudioManager::getInstance()->stopMusic();
                     else
                         AudioManager::getInstance()->playRandomMusic();

                     if (currentDevice != optionsAudio->getSelected()) {
                         RecalboxConf::getInstance()->set("audio.device", optionsAudio->getSelected());
                         RecalboxSystem::getInstance()->setAudioOutputDevice(optionsAudio->getSelected());
			 v_need_reboot = true;
                     }
                     RecalboxConf::getInstance()->saveRecalboxConf();
		     if(v_need_reboot) {
		       this->mWindow->pushGui(
					      new GuiMsgBox(this->mWindow, _("YOU NEED TO REBOOT THE SYSTEM TO COMPLETLY APPLY THIS OPTION."), _("OK"),
							    [] {
							    })
					      );
		     }
                 });

                 mWindow->pushGui(s);
             });

    if (RecalboxConf::getInstance()->get("system.es.menu") != "bartop") {
      addEntry(_("NETWORK SETTINGS").c_str(), 0x777777FF, true,
                 [this] {
                     Window *window = mWindow;

                     auto s = new GuiSettings(mWindow, _("NETWORK SETTINGS").c_str());
                     auto status = std::make_shared<TextComponent>(mWindow,
                                                                   RecalboxSystem::getInstance()->ping() ? _("CONNECTED")
								   : _("NOT CONNECTED"),
                                                                   Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
                     s->addWithLabel(_("STATUS"), status);
                     auto ip = std::make_shared<TextComponent>(mWindow, RecalboxSystem::getInstance()->getIpAdress(),
                                                               Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
                     s->addWithLabel(_("IP ADDRESS"), ip);
                     // Hostname
                     createInputTextRow(s, _("HOSTNAME"), "system.hostname", false);

                     // Wifi enable
                     auto enable_wifi = std::make_shared<SwitchComponent>(mWindow);
                     bool baseEnabled = RecalboxConf::getInstance()->get("wifi.enabled") == "1";
                     enable_wifi->setState(baseEnabled);
                     s->addWithLabel(_("ENABLE WIFI"), enable_wifi);

                     // window, title, settingstring,
                     const std::string baseSSID = RecalboxConf::getInstance()->get("wifi.ssid");
                     createInputTextRow(s, _("WIFI SSID"), "wifi.ssid", false);
                     const std::string baseKEY = RecalboxConf::getInstance()->get("wifi.key");
                     createInputTextRow(s, _("WIFI KEY"), "wifi.key", true);

                     s->addSaveFunc([baseEnabled, baseSSID, baseKEY, enable_wifi, window] {
                         bool wifienabled = enable_wifi->getState();
                         RecalboxConf::getInstance()->set("wifi.enabled", wifienabled ? "1" : "0");
                         std::string newSSID = RecalboxConf::getInstance()->get("wifi.ssid");
                         std::string newKey = RecalboxConf::getInstance()->get("wifi.key");
                         RecalboxConf::getInstance()->saveRecalboxConf();
                         if (wifienabled) {
                             if (baseSSID != newSSID
                                 || baseKEY != newKey
                                 || !baseEnabled) {
                                 if (RecalboxSystem::getInstance()->enableWifi(newSSID, newKey)) {
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
                             RecalboxSystem::getInstance()->disableWifi();
                         }
                     });
                     mWindow->pushGui(s);


                 });
    }

    if (RecalboxConf::getInstance()->get("system.es.menu") != "bartop") {
      // manual or automatic ?
    addEntry(_("SCRAPE").c_str(), 0x777777FF, true,
             [this] {
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
		 std::function<void()> manualAct = [this] {
		   auto openScrapeNow = [this] { mWindow->pushGui(new GuiScraperStart(mWindow)); };
		   auto s = new GuiSettings(mWindow, _("MANUAL SCRAPER").c_str());

		   // scrape from
		   auto scraper_list = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SCRAPE FROM"),
											   false);
		   std::vector<std::string> scrapers = getScraperList();
		   for (auto it = scrapers.begin(); it != scrapers.end(); it++)
		     scraper_list->add(*it, *it, *it == Settings::getInstance()->getString("Scraper"));

		   s->addWithLabel(_("SCRAPE FROM"), scraper_list);
		   s->addSaveFunc([scraper_list] {
		       Settings::getInstance()->setString("Scraper", scraper_list->getSelected());
                     });

		   // scrape ratings
		   auto scrape_ratings = std::make_shared<SwitchComponent>(mWindow);
		   scrape_ratings->setState(Settings::getInstance()->getBool("ScrapeRatings"));
		   s->addWithLabel(_("SCRAPE RATINGS"), scrape_ratings);
		   s->addSaveFunc([scrape_ratings] {
		       Settings::getInstance()->setBool("ScrapeRatings", scrape_ratings->getState());
                     });

		   // scrape now
		   ComponentListRow row;
		   std::function<void()> openAndSave = openScrapeNow;
		   openAndSave = [s, openAndSave] {
		     s->save();
		     openAndSave();
		   };
		   row.makeAcceptInputHandler(openAndSave);

		   auto scrape_now = std::make_shared<TextComponent>(mWindow, _("SCRAPE NOW"),
								     Font::get(FONT_SIZE_MEDIUM),
                                                                       0x777777FF);
		   auto bracket = makeArrow(mWindow);
		   row.addElement(scrape_now, true);
		   row.addElement(bracket, false);
		   s->addRow(row);

		   mWindow->pushGui(s);
		 };
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
             });
     }

    addEntry(_("QUIT").c_str(), 0x777777FF, true,
             [this] {
	       auto s = new GuiSettings(mWindow, _("QUIT").c_str());

                 Window *window = mWindow;

                 ComponentListRow row;
                 row.makeAcceptInputHandler([window] {
                     window->pushGui(new GuiMsgBox(window, _("REALLY RESTART?"), _("YES"),
                                                   [] {
                                                       if (RecalboxSystem::getInstance()->reboot() != 0) {
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
                                                       if (RecalboxSystem::getInstance()->shutdown() != 0) {
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
                                                       if (RecalboxSystem::getInstance()->fastShutdown() != 0) {
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

                 mWindow->pushGui(s);
             });

    mVersion.setFont(Font::get(FONT_SIZE_SMALL));
    mVersion.setColor(0xC6C6C6FF);
    mVersion.setText("EMULATIONSTATION V" + strToUpper(PROGRAM_VERSION_STRING));
    mVersion.setAlignment(ALIGN_CENTER);

    addChild(&mMenu);
    addChild(&mVersion);

    setSize(mMenu.getSize());
    setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.1f);
}

GuiMenu::~GuiMenu() {
  clearLoadedInput();
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
            found = (RecalboxConf::getInstance()->get(systemData->getName() + ".emulator") == curEmulatorName);
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
                bool found = (RecalboxConf::getInstance()->get(systemData->getName() + ".core") == *core);
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
    smoothing_enabled->add(_("AUTO"), "auto", RecalboxConf::getInstance()->get(systemData->getName() + ".smooth") != "0" && RecalboxConf::getInstance()->get(systemData->getName() + ".smooth") != "1");
    smoothing_enabled->add(_("ON"),   "1",    RecalboxConf::getInstance()->get(systemData->getName() + ".smooth") == "1");
    smoothing_enabled->add(_("OFF"),  "0",    RecalboxConf::getInstance()->get(systemData->getName() + ".smooth") == "0");
    systemConfiguration->addWithLabel(_("SMOOTH GAMES"), smoothing_enabled);
    // rewind
    auto rewind_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("REWIND"));
    rewind_enabled->add(_("AUTO"), "auto", RecalboxConf::getInstance()->get(systemData->getName() + ".rewind") != "0" && RecalboxConf::getInstance()->get(systemData->getName() + ".rewind") != "1");
    rewind_enabled->add(_("ON"),   "1",    RecalboxConf::getInstance()->get(systemData->getName() + ".rewind") == "1");
    rewind_enabled->add(_("OFF"),  "0",    RecalboxConf::getInstance()->get(systemData->getName() + ".rewind") == "0");
    systemConfiguration->addWithLabel(_("REWIND"), rewind_enabled);
    // autosave
    auto autosave_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("AUTO SAVE/LOAD"));
    autosave_enabled->add(_("AUTO"), "auto", RecalboxConf::getInstance()->get(systemData->getName() + ".autosave") != "0" && RecalboxConf::getInstance()->get(systemData->getName() + ".autosave") != "1");
    autosave_enabled->add(_("ON"),   "1",    RecalboxConf::getInstance()->get(systemData->getName() + ".autosave") == "1");
    autosave_enabled->add(_("OFF"),  "0",    RecalboxConf::getInstance()->get(systemData->getName() + ".autosave") == "0");
    systemConfiguration->addWithLabel(_("AUTO SAVE/LOAD"), autosave_enabled);

    systemConfiguration->addSaveFunc(
				     [systemData, smoothing_enabled, rewind_enabled, ratio_choice, videoResolutionMode_choice, emu_choice, core_choice, autosave_enabled] {
                if(ratio_choice->changed()){
                    RecalboxConf::getInstance()->set(systemData->getName() + ".ratio",
                                                     ratio_choice->getSelected());
                }
		if(videoResolutionMode_choice->changed()){
                    RecalboxConf::getInstance()->set(systemData->getName() + ".videomode",
                                                     videoResolutionMode_choice->getSelected());
                }
                if(rewind_enabled->changed()) {
                    RecalboxConf::getInstance()->set(systemData->getName() + ".rewind",
                                                     rewind_enabled->getSelected());
                }
                if(smoothing_enabled->changed()){
                    RecalboxConf::getInstance()->set(systemData->getName() + ".smooth",
                                                     smoothing_enabled->getSelected());
                }
                if(emu_choice->changed()) {
                    RecalboxConf::getInstance()->set(systemData->getName() + ".emulator", emu_choice->getSelected());
                }
                if(core_choice->changed()){
                    RecalboxConf::getInstance()->set(systemData->getName() + ".core", core_choice->getSelected());
                }
                if(autosave_enabled->changed()) {
                    RecalboxConf::getInstance()->set(systemData->getName() + ".autosave",
                                                     autosave_enabled->getSelected());
                }
                RecalboxConf::getInstance()->saveRecalboxConf();
            });
    mWindow->pushGui(systemConfiguration);
}

void GuiMenu::createConfigInput() {

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
                                              this->createConfigInput();
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
	      bool success = RecalboxSystem::getInstance()->scanNewBluetooth();
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
        RecalboxSystem::getInstance()->forgetBluetoothControllers();
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
        for (auto it = 0; it < InputManager::getInstance()->getNumJoysticks(); it++) {
            InputConfig *config = InputManager::getInstance()->getInputConfigByDevice(it);
            if (config->isConfigured()) {
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
            std::string name;
            std::string selectedName = input_p1->getSelectedName();

            if (selectedName.compare(strToUpper("default")) == 0) {
	      name = "DEFAULT";
	      Settings::getInstance()->setString(confName, name);
	      Settings::getInstance()->setString(confGuid, "");
            } else {
	      if(input_p1->getSelected() != NULL) {
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

void GuiMenu::onSizeChanged() {
    mVersion.setSize(mSize.x(), 0);
    mVersion.setPosition(0, mSize.y() - mVersion.getSize().y());
}

void GuiMenu::addEntry(const char *name, unsigned int color, bool add_arrow, const std::function<void()> &func) {
    std::shared_ptr<Font> font = Font::get(FONT_SIZE_MEDIUM);

    // populate the list
    ComponentListRow row;
    row.addElement(std::make_shared<TextComponent>(mWindow, name, font, color), true);

    if (add_arrow) {
        std::shared_ptr<ImageComponent> bracket = makeArrow(mWindow);
        row.addElement(bracket, false);
    }

    row.makeAcceptInputHandler(func);

    mMenu.addRow(row);
}

bool GuiMenu::input(InputConfig *config, Input input) {
    if (GuiComponent::input(config, input))
        return true;

    if ((config->isMappedTo("a", input) || config->isMappedTo("start", input)) && input.value != 0) {
        delete this;
        return true;
    }

    return false;
}

std::vector<HelpPrompt> GuiMenu::getHelpPrompts() {
    std::vector<HelpPrompt> prompts;
    prompts.push_back(HelpPrompt("up/down", _("CHOOSE")));
    prompts.push_back(HelpPrompt("b", _("SELECT")));
    prompts.push_back(HelpPrompt("start", _("CLOSE")));
    return prompts;
}

std::shared_ptr<OptionListComponent<std::string>> GuiMenu::createRatioOptionList(Window *window,
                                                                                 std::string configname) const {
    auto ratio_choice = std::make_shared<OptionListComponent<std::string> >(window, _("GAME RATIO"), false);
    std::string currentRatio = RecalboxConf::getInstance()->get(configname + ".ratio");
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
    std::string currentVideoMode = RecalboxConf::getInstance()->get(configname + ".videomode");
    if (currentVideoMode.empty()) {
        currentVideoMode = std::string("auto");
    }

    std::vector<std::string> videoResolutionModeMap = RecalboxSystem::getInstance()->getVideoModes();
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
	fs::path paths[pathCount] = {
		"/recalbox/share_init/decorations",
		"/recalbox/share/decorations"
	};

	fs::directory_iterator end;

	for(size_t i = 0; i < pathCount; i++)
	{
		if(!fs::is_directory(paths[i]))
			continue;

		for(fs::directory_iterator it(paths[i]); it != end; ++it)
		{
			if(fs::is_directory(*it))
			{
			  sets.push_back(it->path().filename().string());
			}
		}
	}

	// sort and remove duplicates
	sort(sets.begin(), sets.end());
	sets.erase(unique(sets.begin(), sets.end()), sets.end());
	return sets;
}
