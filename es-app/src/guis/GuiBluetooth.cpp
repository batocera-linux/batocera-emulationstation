#include "guis/GuiBackup.h"
#include "guis/GuiMsgBox.h"
#include "Window.h"
#include <string>
#include "Log.h"
#include "Settings.h"
#include "ApiSystem.h"
#include "LocaleES.h"
#include "GuiBluetooth.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "GuiLoading.h"

GuiBluetooth::GuiBluetooth(Window* window)
	: GuiComponent(window), mMenu(window, _("FORGET A BLUETOOTH DEVICE").c_str())
{
	mWaitingLoad = false;

	auto theme = ThemeData::getMenuTheme();

	addChild(&mMenu);

	if (load())		
		mMenu.addButton(_("REMOVE ALL"), "manual input", [&] { onRemoveAll(); });

	mMenu.addButton(_("BACK"), "back", [&] { delete this; });

	if (Renderer::isSmallScreen())
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
	else
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.15f);
}

bool GuiBluetooth::load()
{
	std::vector<std::string> ssids = ApiSystem::getInstance()->getBluetoothDeviceList();

	mMenu.clear();

	if (ssids.size() == 0)
		mMenu.addEntry(_("NO BLUETOOTH DEVICES FOUND"), false);
	else
	{
		for (auto ssid : ssids)
			mMenu.addEntry(ssid, false, [this, ssid]() { GuiBluetooth::onRemoveDevice(ssid); });
	}

	mMenu.updateSize();

	if (Renderer::isSmallScreen())
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);	

	return ssids.size() > 0;
}

bool GuiBluetooth::input(InputConfig* config, Input input)
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

std::vector<HelpPrompt> GuiBluetooth::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	return prompts;
}


void GuiBluetooth::onRemoveAll()
{
	if (mWaitingLoad)
		return;

	Window* window = mWindow;

	mWindow->pushGui(new GuiLoading<bool>(mWindow, _("PLEASE WAIT"),
		[this, window](auto gui)
		{
			mWaitingLoad = true;
#if WIN32 && _DEBUG
			std::this_thread::sleep_for(std::chrono::milliseconds(2000));
#endif
			return ApiSystem::getInstance()->forgetBluetoothControllers();
		},
		[this, window](bool ret)
		{
			mWaitingLoad = false;
			mWindow->pushGui(new GuiMsgBox(mWindow, _("BLUETOOTH DEVICES HAVE BEEN DELETED."), _("OK")));
			delete this;
		}));	
}

void GuiBluetooth::onRemoveDevice(const std::string& value)
{
	if (mWaitingLoad)
		return;

	Window* window = mWindow;

	std::string macAddress = value;
	auto idx = macAddress.find(" ");
	if (idx != std::string::npos)
		macAddress = macAddress.substr(0, idx);

	mWindow->pushGui(new GuiLoading<bool>(mWindow, _("PLEASE WAIT"),
		[this, window, macAddress](auto gui)
		{
			mWaitingLoad = true;

	#if WIN32 && _DEBUG
			std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	#endif
			return ApiSystem::getInstance()->removeBluetoothDevice(macAddress);			
		},
		[this, window](bool ret)
		{
			mWaitingLoad = false;
			load();
		}));		
}
