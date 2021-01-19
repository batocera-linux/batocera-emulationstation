#include "guis/GuiBackup.h"
#include "guis/GuiMsgBox.h"
#include "Window.h"
#include <string>
#include "Log.h"
#include "Settings.h"
#include "ApiSystem.h"
#include "LocaleES.h"
#include "GuiWifi.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "GuiLoading.h"

GuiWifi::GuiWifi(Window* window, const std::string title, std::string data, const std::function<void(std::string)>& onsave)
	: GuiComponent(window), mMenu(window, title.c_str())
{
	mTitle = title;
	mInitialData = data;
	mSaveFunction = onsave;
	mWaitingLoad = false;

	auto theme = ThemeData::getMenuTheme();

	addChild(&mMenu);

	std::vector<std::string> ssids = ApiSystem::getInstance()->getWifiNetworks();
	if (ssids.empty())
		mWindow->postToUiThread([this]() { onRefresh(); });		
	else
		load(ssids);

	mMenu.addButton(_("REFRESH"), "refresh", [&] { onRefresh(); });
	mMenu.addButton(_("MANUAL INPUT"), "manual input", [&] { onManualInput(); });
	mMenu.addButton(_("BACK"), "back", [&] { delete this; });

	if (Renderer::isSmallScreen())
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
	else
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiWifi::load(std::vector<std::string> ssids)
{
	mMenu.clear();

	if (ssids.size() == 0)
		mMenu.addEntry(_("NO WIFI NETWORKS FOUND"), false, std::bind(&GuiWifi::onRefresh, this));
	else
	{
		for (auto ssid : ssids)
			mMenu.addEntry(ssid, false, [this, ssid]() { GuiWifi::onSave(ssid); });
	}

	mMenu.updateSize();

	if (Renderer::isSmallScreen())
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);

	mWaitingLoad = false;
}

void GuiWifi::onManualInput()
{
	if (Settings::getInstance()->getBool("UseOSK"))
		mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, mTitle, mInitialData, [this](const std::string& value) { onSave(value); }, false));
	else
		mWindow->pushGui(new GuiTextEditPopup(mWindow, mTitle, mInitialData, [this](const std::string& value) { onSave(value); }, false));
}

void GuiWifi::onSave(const std::string& value)
{
	if (mWaitingLoad)
		return;

	mSaveFunction(value);
	delete this;
}

bool GuiWifi::input(InputConfig* config, Input input)
{
	if (GuiComponent::input(config, input))
		return true;

	if (input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		if (!mWaitingLoad)
			delete this;

		return true;
	}

	return false;
}

std::vector<HelpPrompt> GuiWifi::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	return prompts;
}

void GuiWifi::onRefresh()
{		
	Window* window = mWindow;

	mWindow->pushGui(new GuiLoading<std::vector<std::string>>(mWindow, _("SEARCHING WIFI NETWORKS"), 
		[this, window]
		{
			mWaitingLoad = true;
			return ApiSystem::getInstance()->getWifiNetworks(true);
		},
		[this, window](std::vector<std::string> ssids)
		{
			mWaitingLoad = false;
			load(ssids);
		}));	
}
