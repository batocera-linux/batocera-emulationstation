#include "guis/GuiVideoScreensaverOptions.h"

#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiMsgBox.h"
#include "Settings.h"
#include "LocaleES.h"
#include "ApiSystem.h"

#include "guis/GuiTextEditPopup.h"
#include "guis/GuiTextEditPopupKeyboard.h"

GuiVideoScreensaverOptions::GuiVideoScreensaverOptions(Window* window, const char* title) : GuiScreensaverOptions(window, title)
{
	// timeout to swap videos
	auto swap = std::make_shared<SliderComponent>(mWindow, 10.f, 1000.f, 1.f, "s");
	swap->setValue((float)(Settings::getInstance()->getInt("ScreenSaverSwapVideoTimeout") / (1000)));
	addWithLabel(_("SWAP VIDEO AFTER (SECS)"), swap);
	addSaveFunc([swap] {
		int playNextTimeout = (int)Math::round(swap->getValue()) * (1000);
		Settings::getInstance()->setInt("ScreenSaverSwapVideoTimeout", playNextTimeout);
		PowerSaver::updateTimeouts();
	});

#ifdef _RPI_
	auto ss_omx = std::make_shared<SwitchComponent>(mWindow);
	ss_omx->setState(Settings::getInstance()->getBool("ScreenSaverOmxPlayer"));
	addWithLabel(_("USE OMX PLAYER FOR SCREENSAVER"), ss_omx);
	addSaveFunc([ss_omx, this] { Settings::getInstance()->setBool("ScreenSaverOmxPlayer", ss_omx->getState()); });
	ss_omx->setOnChangedCallback([this, ss_omx, window]()
	{
		if (Settings::getInstance()->setBool("ScreenSaverOmxPlayer", ss_omx->getState()))
		{
			Window* pw = mWindow;
			delete this;
			pw->pushGui(new GuiVideoScreensaverOptions(pw, _("VIDEO SCREENSAVER").c_str()));
		}
	});
#endif
	   
	// Render Video Game Name as subtitles
	auto ss_info = std::make_shared< OptionListComponent<std::string> >(mWindow, _("SHOW GAME INFO"), false);
	std::vector<std::string> info_type;
	info_type.push_back("always");
	info_type.push_back("start & end");
	info_type.push_back("never");
	for(auto it = info_type.cbegin(); it != info_type.cend(); it++)
		ss_info->add(_(it->c_str()), *it, Settings::getInstance()->getString("ScreenSaverGameInfo") == *it);
	addWithLabel(_("SHOW GAME INFO ON SCREENSAVER"), ss_info);
	addSaveFunc([ss_info, this] { Settings::getInstance()->setString("ScreenSaverGameInfo", ss_info->getSelected()); });

	bool advancedOptions = true;

#ifdef _RPI_
	advancedOptions = !Settings::getInstance()->getBool("ScreenSaverOmxPlayer");
#endif

	if (advancedOptions)
	{
		auto marquee_screensaver = std::make_shared<SwitchComponent>(mWindow);
		marquee_screensaver->setState(Settings::getInstance()->getBool("ScreenSaverMarquee"));
		addWithLabel(_("USE MARQUEE AS GAME INFO"), marquee_screensaver);
		addSaveFunc([marquee_screensaver] { Settings::getInstance()->setBool("ScreenSaverMarquee", marquee_screensaver->getState()); });

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DECORATIONS))
		{
			std::vector<std::string> decorationType;
			decorationType.push_back("none");
			decorationType.push_back("systems");
			decorationType.push_back("random");

			auto decoration_screensaver = std::make_shared<OptionListComponent<std::string>>(mWindow, _("DECORATIONS"), false);
			for (auto it = decorationType.cbegin(); it != decorationType.cend(); it++)
				decoration_screensaver->add(_(it->c_str()), *it, Settings::getInstance()->getString("ScreenSaverDecorations") == *it);

			if (decoration_screensaver->getSelectedName().empty())
				decoration_screensaver->selectFirstItem();

			addWithLabel(_("DECORATIONS"), decoration_screensaver);
			addSaveFunc([decoration_screensaver]
			{
				Settings::getInstance()->setString("ScreenSaverDecorations", decoration_screensaver->getSelected());
			});
		}
	}

	auto stretch_screensaver = std::make_shared<SwitchComponent>(mWindow);
	stretch_screensaver->setState(Settings::getInstance()->getBool("StretchVideoOnScreenSaver"));
	addWithLabel(_("STRETCH VIDEO ON SCREENSAVER"), stretch_screensaver);
	addSaveFunc([stretch_screensaver] { Settings::getInstance()->setBool("StretchVideoOnScreenSaver", stretch_screensaver->getState()); });

	auto ss_video_mute = std::make_shared<SwitchComponent>(mWindow);
	ss_video_mute->setState(Settings::getInstance()->getBool("ScreenSaverVideoMute"));
	addWithLabel(_("MUTE VIDEO AUDIO"), ss_video_mute);
	addSaveFunc([ss_video_mute] { Settings::getInstance()->setBool("ScreenSaverVideoMute", ss_video_mute->getState()); });







	auto theme = ThemeData::getMenuTheme();

	// video source
	auto sss_custom_source = std::make_shared<SwitchComponent>(mWindow);
	sss_custom_source->setState(Settings::getInstance()->getBool("SlideshowScreenSaverCustomVideoSource"));
	addWithLabel(_("USE CUSTOM VIDEOS"), sss_custom_source);
	addSaveFunc([sss_custom_source] { Settings::getInstance()->setBool("SlideshowScreenSaverCustomVideoSource", sss_custom_source->getState()); });

	// custom video directory
	auto sss_image_dir = std::make_shared<TextComponent>(mWindow, "", theme->TextSmall.font, theme->Text.color);
	addEditableTextComponent( _("CUSTOM VIDEO DIR"), sss_image_dir, Settings::getInstance()->getString("SlideshowScreenSaverVideoDir"));
	addSaveFunc([sss_image_dir] {
		Settings::getInstance()->setString("SlideshowScreenSaverVideoDir", sss_image_dir->getValue());
	});

	// recurse custom video directory
	auto sss_recurse = std::make_shared<SwitchComponent>(mWindow);
	sss_recurse->setState(Settings::getInstance()->getBool("SlideshowScreenSaverVideoRecurse"));
	addWithLabel(_("CUSTOM VIDEO DIR RECURSIVE"), sss_recurse);
	addSaveFunc([sss_recurse] {
		Settings::getInstance()->setBool("SlideshowScreenSaverVideoRecurse", sss_recurse->getState());
	});

	// custom video filter
	auto sss_image_filter = std::make_shared<TextComponent>(mWindow, "", theme->TextSmall.font, theme->Text.color);
	addEditableTextComponent(_("CUSTOM VIDEO FILTER"), sss_image_filter, Settings::getInstance()->getString("SlideshowScreenSaverVideoFilter"));
	addSaveFunc([sss_image_filter] {
		Settings::getInstance()->setString("SlideshowScreenSaverVideoFilter", sss_image_filter->getValue());
	});

}

void GuiVideoScreensaverOptions::addEditableTextComponent(const std::string label, std::shared_ptr<GuiComponent> ed, std::string value)
{
	ComponentListRow row;
	auto theme = ThemeData::getMenuTheme();
	row.elements.clear();

	auto lbl = std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(label), theme->Text.font, theme->Text.color);
	row.addElement(lbl, true); // label

	row.addElement(ed, true);

	auto spacer = std::make_shared<GuiComponent>(mWindow);
	spacer->setSize(Renderer::getScreenWidth() * 0.005f, 0);
	row.addElement(spacer, false);

	auto bracket = std::make_shared<ImageComponent>(mWindow);
	bracket->setImage(ThemeData::getMenuTheme()->Icons.arrow);
	bracket->setResize(Vector2f(0, lbl->getFont()->getLetterHeight()));
	row.addElement(bracket, false);

	auto updateVal = [ed](const std::string& newVal) { ed->setValue(newVal); }; // ok callback (apply new value to ed)
	row.makeAcceptInputHandler([this, label, ed, updateVal] {
		if (Settings::getInstance()->getBool("UseOSK")) {
			mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, label, ed->getValue(), updateVal, false));
		}
		else {
			mWindow->pushGui(new GuiTextEditPopup(mWindow, label, ed->getValue(), updateVal, false));
		}
	});

	assert(ed);
	addRow(row);
	ed->setValue(value);
}


GuiVideoScreensaverOptions::~GuiVideoScreensaverOptions()
{
}

void GuiVideoScreensaverOptions::save()
{
#ifdef _RPI_
	bool startingStatusNotRisky = (Settings::getInstance()->getString("ScreenSaverGameInfo") == "never" || !Settings::getInstance()->getBool("ScreenSaverOmxPlayer"));
#endif
	GuiScreensaverOptions::save();

#ifdef _RPI_
	bool endStatusRisky = (Settings::getInstance()->getString("ScreenSaverGameInfo") != "never" && Settings::getInstance()->getBool("ScreenSaverOmxPlayer"));
	if (startingStatusNotRisky && endStatusRisky) {
		// if before it wasn't risky but now there's a risk of problems, show warning
		mWindow->pushGui(new GuiMsgBox(mWindow,
		"Using OMX Player and displaying Game Info may result in the video flickering in some TV modes. If that happens, consider:\n\n• Disabling the \"Show Game Info\" option;\n• Disabling \"Overscan\" on the Pi configuration menu might help:\nRetroPie > Raspi-Config > Advanced Options > Overscan > \"No\".\n• Disabling the use of OMX Player for the screensaver.",
			"GOT IT!", [] { return; }));
	}
#endif
}
