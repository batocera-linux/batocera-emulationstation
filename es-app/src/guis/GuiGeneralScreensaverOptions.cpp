#include "guis/GuiGeneralScreensaverOptions.h"

#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiMsgBox.h"
#include "Settings.h"
#include "LocaleES.h"
#include "ApiSystem.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "guis/GuiFileBrowser.h"

#define fake_gettext_dim			_("dim")
#define fake_gettext_black			_("black")
#define fake_gettext_randomvideo	_("random video")
#define fake_gettext_slideshow		_("slideshow")
#define fake_gettext_suspend		_("suspend")

#define fake_gettext_always			_("always")
#define fake_gettext_start_end		_("start & end")
#define fake_gettext_never			_("never")

#define fake_gettext_none			_("none")
#define fake_gettext_systems		_("systems")
#define fake_gettext_random			_("random")

GuiGeneralScreensaverOptions::GuiGeneralScreensaverOptions(Window* window, int selectItem) : GuiSettings(window, _("SCREENSAVER SETTINGS"))
{
	std::string ssBehavior = Settings::getInstance()->getString("ScreenSaverBehavior");

	mMenu.addGroup(_("SETTINGS"));

	// Screensaver time
	auto ctlTime = std::make_shared<SliderComponent>(mWindow, 0.f, 120.0f, 1.f, "m");
	ctlTime->setValue((float)(Settings::getInstance()->getInt("ScreenSaverTime") / (1000 * 60)));
	addWithLabel(_("START SCREENSAVER AFTER"), ctlTime);
	addSaveFunc([ctlTime]
	{
	    Settings::getInstance()->setInt("ScreenSaverTime", (int)Math::round(ctlTime->getValue()) * (1000 * 60));
	    PowerSaver::updateTimeouts();
	});
	
	// Screensaver behavior
	auto ctlBehavior = std::make_shared< OptionListComponent<std::string> >(mWindow, _("SCREENSAVER TYPE"), false);

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SUSPEND))
		ctlBehavior->addRange({ "dim", "black", "random video", "slideshow", "suspend" }, ssBehavior);
	else
		ctlBehavior->addRange({ "dim", "black", "random video", "slideshow" }, ssBehavior);

	addWithLabel(_("SCREENSAVER TYPE"), ctlBehavior, selectItem == 1);
	ctlBehavior->setSelectedChangedCallback([this](const std::string& name)
	{
		if (Settings::getInstance()->setString("ScreenSaverBehavior", name))
		{
			PowerSaver::updateTimeouts();

			Window* pw = mWindow;
			delete this;
			pw->pushGui(new GuiGeneralScreensaverOptions(pw, 1));
		}
	});

	// Screensaver stops music 
	if (Settings::getInstance()->getBool("audio.bgmusic"))
	{
		auto ctlStopMusic = std::make_shared<SwitchComponent>(mWindow);
		ctlStopMusic->setState(Settings::getInstance()->getBool("StopMusicOnScreenSaver"));
		addWithLabel(_("STOP MUSIC ON SCREENSAVER"), ctlStopMusic);
		addSaveFunc([ctlStopMusic] { Settings::getInstance()->setBool("StopMusicOnScreenSaver", ctlStopMusic->getState()); });
	}

	if (ssBehavior == "random video")
		addVideoScreensaverOptions(selectItem);
	else if (ssBehavior == "slideshow")
		addSlideShowScreensaverOptions(selectItem);
}

void GuiGeneralScreensaverOptions::addVideoScreensaverOptions(int selectItem)
{
	mMenu.addGroup(_("RANDOM VIDEO SCREENSAVER SETTINGS"));

	auto theme = ThemeData::getMenuTheme();

	bool useCustomVideoSource = Settings::getInstance()->getBool("SlideshowScreenSaverCustomVideoSource");

	// timeout to swap videos
	auto swap = std::make_shared<SliderComponent>(mWindow, 10.f, 1000.f, 1.f, "s");
	swap->setValue((float)(Settings::getInstance()->getInt("ScreenSaverSwapVideoTimeout") / (1000)));
	addWithLabel(_("VIDEO DURATION (SECS)"), swap);
	addSaveFunc([swap] {
		int playNextTimeout = (int)Math::round(swap->getValue()) * (1000);
		Settings::getInstance()->setInt("ScreenSaverSwapVideoTimeout", playNextTimeout);
		PowerSaver::updateTimeouts();
	});

#ifdef _RPI_
	auto ss_omx = std::make_shared<SwitchComponent>(mWindow);
	ss_omx->setState(Settings::getInstance()->getBool("ScreenSaverOmxPlayer"));
	addWithLabel(_("USE OMX PLAYER FOR SCREENSAVER"), ss_omx, selectItem == 4);
	addSaveFunc([ss_omx, this] { Settings::getInstance()->setBool("ScreenSaverOmxPlayer", ss_omx->getState()); });
	ss_omx->setOnChangedCallback([this, ss_omx]()
	{
		if (Settings::getInstance()->setBool("ScreenSaverOmxPlayer", ss_omx->getState()))
		{
			Window* pw = mWindow;
			delete this;
			pw->pushGui(new GuiGeneralScreensaverOptions(pw, 4));
		}
	});
#endif

	// Render Video Game Name as subtitles
	auto ss_info = std::make_shared< OptionListComponent<std::string> >(mWindow, _("SHOW GAME INFO"), false);	
	ss_info->addRange({ "always", "start & end", "never" }, Settings::getInstance()->getString("ScreenSaverGameInfo"));

	std::vector<std::string> info_type = { "always", "start & end", "never" };
	for (auto it = info_type.cbegin(); it != info_type.cend(); it++)
		ss_info->add(_(it->c_str()), *it, Settings::getInstance()->getString("ScreenSaverGameInfo") == *it);

	addWithLabel(_("SHOW GAME INFO"), ss_info);
	addSaveFunc([ss_info, this] { Settings::getInstance()->setString("ScreenSaverGameInfo", ss_info->getSelected()); });

	// SHOW DATE TIME
	{
		bool showDateTime = Settings::getInstance()->getBool("ScreenSaverDateTime");

		auto datetime_screensaver = std::make_shared<SwitchComponent>(mWindow);
		datetime_screensaver->setState(showDateTime);
		addWithLabel(_("SHOW DATE TIME"), datetime_screensaver, selectItem == 5);

		datetime_screensaver->setOnChangedCallback([this, datetime_screensaver]()
			{
				if (Settings::getInstance()->setBool("ScreenSaverDateTime", datetime_screensaver->getState()))
				{
					Window* pw = mWindow;
					delete this;
					pw->pushGui(new GuiGeneralScreensaverOptions(pw, 5));
				}
			});

		if (showDateTime)
		{
			auto sss_date_format = std::make_shared< OptionListComponent<std::string> >(mWindow, _("DATE FORMAT"), false);
			sss_date_format->addRange({ "%Y-%m-%d", "%d-%m-%Y", "%A, %B %d", "%b %d, %Y" }, Settings::getInstance()->getString("ScreenSaverDateFormat"));
			addWithLabel(_("DATE FORMAT"), sss_date_format);
			addSaveFunc([sss_date_format] { Settings::getInstance()->setString("ScreenSaverDateFormat", sss_date_format->getSelected()); });

			auto sss_time_format = std::make_shared< OptionListComponent<std::string> >(mWindow, _("TIME FORMAT"), false);
			sss_time_format->addRange({ "%H:%M:%S", "%I:%M %p", "%p %I:%M" }, Settings::getInstance()->getString("ScreenSaverTimeFormat"));
			addWithLabel(_("TIME FORMAT"), sss_time_format);
			addSaveFunc([sss_time_format] { Settings::getInstance()->setString("ScreenSaverTimeFormat", sss_time_format->getSelected()); });
		}
	}

	bool advancedOptions = true;

#ifdef _RPI_
	advancedOptions = !Settings::getInstance()->getBool("ScreenSaverOmxPlayer");
#endif

	if (advancedOptions && !useCustomVideoSource)
	{
		auto marquee_screensaver = std::make_shared<SwitchComponent>(mWindow);
		marquee_screensaver->setState(Settings::getInstance()->getBool("ScreenSaverMarquee"));
		addWithLabel(_("USE MARQUEE AS GAME INFO"), marquee_screensaver);
		addSaveFunc([marquee_screensaver] { Settings::getInstance()->setBool("ScreenSaverMarquee", marquee_screensaver->getState()); });

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DECORATIONS))
		{
			auto decoration_screensaver = std::make_shared<OptionListComponent<std::string>>(mWindow, _("DECORATION SET USED"), false);
			decoration_screensaver->addRange({ "none", "systems", "random" }, Settings::getInstance()->getString("ScreenSaverDecorations"));
			addWithLabel(_("DECORATION SET USED"), decoration_screensaver);
			addSaveFunc([decoration_screensaver] { Settings::getInstance()->setString("ScreenSaverDecorations", decoration_screensaver->getSelected()); });
		}
	}

	auto stretch_screensaver = std::make_shared<SwitchComponent>(mWindow);
	stretch_screensaver->setState(Settings::getInstance()->getBool("StretchVideoOnScreenSaver"));
	addWithLabel(_("STRETCH VIDEOS"), stretch_screensaver);
	addSaveFunc([stretch_screensaver] { Settings::getInstance()->setBool("StretchVideoOnScreenSaver", stretch_screensaver->getState()); });

	auto ss_video_mute = std::make_shared<SwitchComponent>(mWindow);
	ss_video_mute->setState(Settings::getInstance()->getBool("ScreenSaverVideoMute"));
	addWithLabel(_("MUTE VIDEO AUDIO"), ss_video_mute);
	addSaveFunc([ss_video_mute] { Settings::getInstance()->setBool("ScreenSaverVideoMute", ss_video_mute->getState()); });

	// Allow ScreenSaver Controls - ScreenSaverControls
	auto controls = std::make_shared<SwitchComponent>(mWindow);
	controls->setState(Settings::getInstance()->getBool("ScreenSaverControls"));
	addWithLabel(_("SCREENSAVER CONTROLS"), controls);
	addSaveFunc([controls] { Settings::getInstance()->setBool("ScreenSaverControls", controls->getState()); });

	// video source
	auto sss_custom_source = std::make_shared<SwitchComponent>(mWindow);
	sss_custom_source->setState(useCustomVideoSource);
	addWithLabel(_("USE CUSTOM VIDEOS"), sss_custom_source, selectItem == 2);
	sss_custom_source->setOnChangedCallback([this, sss_custom_source]()
	{
		if (Settings::getInstance()->setBool("SlideshowScreenSaverCustomVideoSource", sss_custom_source->getState()))
		{
			Window* pw = mWindow;
			delete this;
			pw->pushGui(new GuiGeneralScreensaverOptions(pw, 2));
		}
	});

	if (useCustomVideoSource)
	{
		// custom video directory
		auto sss_image_dir = addBrowsablePath(_("CUSTOM VIDEO DIRECTORY"), Settings::getInstance()->getString("SlideshowScreenSaverVideoDir"));
		addSaveFunc([sss_image_dir] { Settings::getInstance()->setString("SlideshowScreenSaverVideoDir", sss_image_dir->getValue()); });

		// recurse custom video directory
		auto sss_recurse = std::make_shared<SwitchComponent>(mWindow, Settings::getInstance()->getBool("SlideshowScreenSaverVideoRecurse"));
		addWithLabel(_("USE VIDEOS IN SUBFOLDERS OF CUSTOM DIRECTORY"), sss_recurse);
		addSaveFunc([sss_recurse] { Settings::getInstance()->setBool("SlideshowScreenSaverVideoRecurse", sss_recurse->getState()); });

		// custom video filter
		auto sss_image_filter = addEditableTextComponent(_("CUSTOM VIDEO FILE EXTENSIONS"), Settings::getInstance()->getString("SlideshowScreenSaverVideoFilter"));
		addSaveFunc([sss_image_filter] { Settings::getInstance()->setString("SlideshowScreenSaverVideoFilter", sss_image_filter->getValue()); });
	}
}

void GuiGeneralScreensaverOptions::addSlideShowScreensaverOptions(int selectItem)
{
	mMenu.addGroup(_("SLIDESHOW SCREENSAVER SETTINGS"));

	auto theme = ThemeData::getMenuTheme();

	// image duration (seconds)
	auto sss_image_sec = std::make_shared<SliderComponent>(mWindow, 1.f, 60.f, 1.f, "s");
	sss_image_sec->setValue((float)(Settings::getInstance()->getInt("ScreenSaverSwapImageTimeout") / (1000)));
	addWithLabel(_("IMAGE DURATION (SECS)"), sss_image_sec);
	addSaveFunc([sss_image_sec] {
		int playNextTimeout = (int)Math::round(sss_image_sec->getValue()) * (1000);
		Settings::getInstance()->setInt("ScreenSaverSwapImageTimeout", playNextTimeout);
		PowerSaver::updateTimeouts();
	});

	// SHOW GAME NAME
	auto ss_controls = std::make_shared<SwitchComponent>(mWindow);
	ss_controls->setState(Settings::getInstance()->getBool("SlideshowScreenSaverGameName"));
	addWithLabel(_("SHOW GAME INFO"), ss_controls);
	addSaveFunc([ss_controls] { Settings::getInstance()->setBool("SlideshowScreenSaverGameName", ss_controls->getState()); });

	// SHOW DATE TIME
	{
		bool showDateTime = Settings::getInstance()->getBool("ScreenSaverDateTime");

		auto datetime_screensaver = std::make_shared<SwitchComponent>(mWindow);
		datetime_screensaver->setState(showDateTime);
		addWithLabel(_("SHOW DATE TIME"), datetime_screensaver, selectItem == 6);

		datetime_screensaver->setOnChangedCallback([this, datetime_screensaver]()
			{
				if (Settings::getInstance()->setBool("ScreenSaverDateTime", datetime_screensaver->getState()))
				{
					Window* pw = mWindow;
					delete this;
					pw->pushGui(new GuiGeneralScreensaverOptions(pw, 6));
				}
			});

		if (showDateTime)
		{
			auto sss_date_format = std::make_shared< OptionListComponent<std::string> >(mWindow, _("DATE FORMAT"), false);
			sss_date_format->addRange({ "%Y-%m-%d", "%d-%m-%Y", "%A, %B %d", "%b %d, %Y"}, Settings::getInstance()->getString("ScreenSaverDateFormat"));
			addWithLabel(_("DATE FORMAT"), sss_date_format);
			addSaveFunc([sss_date_format] { Settings::getInstance()->setString("ScreenSaverDateFormat", sss_date_format->getSelected()); });

			auto sss_time_format = std::make_shared< OptionListComponent<std::string> >(mWindow, _("TIME FORMAT"), false);
			sss_time_format->addRange({ "%H:%M:%S", "%I:%M %p", "%p %I:%M"}, Settings::getInstance()->getString("ScreenSaverTimeFormat"));
			addWithLabel(_("TIME FORMAT"), sss_time_format);
			addSaveFunc([sss_time_format] { Settings::getInstance()->setString("ScreenSaverTimeFormat", sss_time_format->getSelected()); });
		}
	}

	auto marquee_screensaver = std::make_shared<SwitchComponent>(mWindow);
	marquee_screensaver->setState(Settings::getInstance()->getBool("ScreenSaverMarquee"));
	addWithLabel(_("USE MARQUEE AS GAME INFO"), marquee_screensaver);
	addSaveFunc([marquee_screensaver] { Settings::getInstance()->setBool("ScreenSaverMarquee", marquee_screensaver->getState()); });

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DECORATIONS))
	{
		auto decoration_screensaver = std::make_shared<OptionListComponent<std::string>>(mWindow, _("DECORATION SET USED"), false);
		decoration_screensaver->addRange({ "none", "systems", "random" }, Settings::getInstance()->getString("ScreenSaverDecorations"));
		addWithLabel(_("DECORATION SET USED"), decoration_screensaver);
		addSaveFunc([decoration_screensaver] { Settings::getInstance()->setString("ScreenSaverDecorations", decoration_screensaver->getSelected()); });
	}

	// stretch
	auto sss_stretch = std::make_shared<SwitchComponent>(mWindow);
	sss_stretch->setState(Settings::getInstance()->getBool("SlideshowScreenSaverStretch"));
	addWithLabel(_("STRETCH IMAGES"), sss_stretch);
	addSaveFunc([sss_stretch] {
		Settings::getInstance()->setBool("SlideshowScreenSaverStretch", sss_stretch->getState());
	});


	// Allow ScreenSaver Controls - ScreenSaverControls
	auto controls = std::make_shared<SwitchComponent>(mWindow);
	controls->setState(Settings::getInstance()->getBool("ScreenSaverControls"));
	addWithLabel(_("SCREENSAVER CONTROLS"), controls);
	addSaveFunc([controls] { Settings::getInstance()->setBool("ScreenSaverControls", controls->getState()); });

	/*
	// background audio file
	auto sss_bg_audio_file = std::make_shared<TextComponent>(mWindow, "", theme->TextSmall.font, theme->Text.color);
	addEditableTextComponent(row, _("BACKGROUND AUDIO"), sss_bg_audio_file, Settings::getInstance()->getString("SlideshowScreenSaverBackgroundAudioFile"));
	addSaveFunc([sss_bg_audio_file] {
	Settings::getInstance()->setString("SlideshowScreenSaverBackgroundAudioFile", sss_bg_audio_file->getValue());
	});
	*/

	bool customSource = Settings::getInstance()->getBool("SlideshowScreenSaverCustomImageSource");

	// image source
	auto sss_custom_source = std::make_shared<SwitchComponent>(mWindow);
	sss_custom_source->setState(customSource);
	addWithLabel(_("USE CUSTOM IMAGES"), sss_custom_source, selectItem == 3);

	sss_custom_source->setOnChangedCallback([this, sss_custom_source]()
	{
		if (Settings::getInstance()->setBool("SlideshowScreenSaverCustomImageSource", sss_custom_source->getState()))
		{
			Window* pw = mWindow;
			delete this;
			pw->pushGui(new GuiGeneralScreensaverOptions(pw, 3));
		}
	});

	if (customSource)
	{
		// custom image directory
		auto sss_image_dir = addBrowsablePath(_("CUSTOM IMAGE DIRECTORY"), Settings::getInstance()->getString("SlideshowScreenSaverImageDir"));
		addSaveFunc([sss_image_dir] { Settings::getInstance()->setString("SlideshowScreenSaverImageDir", sss_image_dir->getValue()); });

		// recurse custom image directory
		auto sss_recurse = std::make_shared<SwitchComponent>(mWindow, Settings::getInstance()->getBool("SlideshowScreenSaverRecurse"));
		addWithLabel(_("USE IMAGES IN SUBFOLDERS OF CUSTOM DIRECTORY"), sss_recurse);
		addSaveFunc([sss_recurse] { Settings::getInstance()->setBool("SlideshowScreenSaverRecurse", sss_recurse->getState()); });

		// custom image filter
		auto sss_image_filter = addEditableTextComponent(_("CUSTOM IMAGE FILE EXTENSIONS"), Settings::getInstance()->getString("SlideshowScreenSaverImageFilter"));
		addSaveFunc([sss_image_filter] { Settings::getInstance()->setString("SlideshowScreenSaverImageFilter", sss_image_filter->getValue()); });
	}
}

std::shared_ptr<TextComponent> GuiGeneralScreensaverOptions::addEditableTextComponent(const std::string label, std::string value)
{
	auto theme = ThemeData::getMenuTheme();

	ComponentListRow row;

	auto lbl = std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(label), theme->Text.font, theme->Text.color);
	row.addElement(lbl, true); // label

	std::shared_ptr<TextComponent> ed = std::make_shared<TextComponent>(mWindow, "", theme->Text.font, theme->Text.color, ALIGN_RIGHT);
	row.addElement(ed, true);

	auto spacer = std::make_shared<GuiComponent>(mWindow);
	spacer->setSize(Renderer::getScreenWidth() * 0.005f, 0);
	row.addElement(spacer, false);

	auto bracket = std::make_shared<ImageComponent>(mWindow);
	bracket->setImage(ThemeData::getMenuTheme()->Icons.arrow);
	bracket->setResize(Vector2f(0, lbl->getFont()->getLetterHeight()));
	row.addElement(bracket, false);

	auto updateVal = [ed](const std::string& newVal) { ed->setValue(newVal); }; // ok callback (apply new value to ed)
	row.makeAcceptInputHandler([this, label, ed, updateVal] 
	{
		if (Settings::getInstance()->getBool("UseOSK"))
			mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, label, ed->getValue(), updateVal, false));
		else
			mWindow->pushGui(new GuiTextEditPopup(mWindow, label, ed->getValue(), updateVal, false));		
	});

	ed->setValue(value);

	mMenu.addRow(row);
	return ed;
}


std::shared_ptr<TextComponent> GuiGeneralScreensaverOptions::addBrowsablePath(const std::string label, std::string value)
{
	auto theme = ThemeData::getMenuTheme();

	ComponentListRow row;

	auto lbl = std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(label), theme->Text.font, theme->Text.color);
	row.addElement(lbl, true); // label

	std::shared_ptr<TextComponent> ed = std::make_shared<TextComponent>(mWindow, "", theme->Text.font, theme->Text.color, ALIGN_RIGHT);
	row.addElement(ed, true);

	auto spacer = std::make_shared<GuiComponent>(mWindow);
	spacer->setSize(Renderer::getScreenWidth() * 0.005f, 0);
	row.addElement(spacer, false);

	auto bracket = std::make_shared<ImageComponent>(mWindow);
	bracket->setImage(ThemeData::getMenuTheme()->Icons.arrow);
	bracket->setResize(Vector2f(0, lbl->getFont()->getLetterHeight()));
	row.addElement(bracket, false);

	auto updateVal = [ed](const std::string& newVal) 
	{ 
		ed->setValue(newVal); 
	}; // ok callback (apply new value to ed)

	row.makeAcceptInputHandler([this, label, ed, updateVal]
	{
		auto parent = Utils::FileSystem::getParent(ed->getValue());
		mWindow->pushGui(new GuiFileBrowser(mWindow, parent, ed->getValue(), GuiFileBrowser::DIRECTORY, updateVal, label));
	});

	ed->setValue(value);

	mMenu.addRow(row);
	return ed;
}
