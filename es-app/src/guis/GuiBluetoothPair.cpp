#include "guis/GuiBackup.h"
#include "guis/GuiMsgBox.h"
#include "Window.h"
#include <string>
#include "Log.h"
#include "Settings.h"
#include "ApiSystem.h"
#include "LocaleES.h"
#include "GuiBluetoothPair.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "GuiLoading.h"

#define WINDOW_WIDTH (float)Math::min(Renderer::getScreenHeight() * 1.125f, Renderer::getScreenWidth() * 0.90f)

GuiBluetoothPair* GuiBluetoothPair::Instance = nullptr;

GuiBluetoothPair::GuiBluetoothPair(Window* window)
	: MenuComponent(window, _("PAIR A BLUETOOTH DEVICE")), mBusyAnim(window)
{
	auto theme = ThemeData::getMenuTheme();
		
	addButton(_("CANCEL"), "back", [&] { delete this; });

	if (Renderer::ScreenSettings::fullScreenMenus())
	{
		setPosition((Renderer::getScreenWidth() - getSize().x()) / 2, (Renderer::getScreenHeight() - getSize().y()) / 2);
		setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	}
	else
	{
		setPosition((Renderer::getScreenWidth() - getSize().x()) / 2, Renderer::getScreenHeight() * 0.15f);
		setSize(getSize().x(), Renderer::getScreenHeight() * 0.60f);
	}

	mBusyAnim.setText(_("PLEASE WAIT"));
	mBusyAnim.setSize(getSize());

	Instance = this;
	mThread = new std::thread(&GuiBluetoothPair::loadDevicesAsync, this);
}

GuiBluetoothPair::~GuiBluetoothPair()
{
	Instance = nullptr;
	ApiSystem::getInstance()->stopBluetoothLiveDevices();
}

void GuiBluetoothPair::updateSize()
{
	MenuComponent::onSizeChanged();
}

void GuiBluetoothPair::render(const Transform4x4f& parentTrans)
{
	MenuComponent::render(parentTrans);

	Transform4x4f trans = parentTrans * getTransform();		

	if (size() == 0)
		mBusyAnim.render(trans);
}

void GuiBluetoothPair::update(int deltaTime)
{
	GuiComponent::update(deltaTime);
	mBusyAnim.update(deltaTime);
}

void GuiBluetoothPair::loadDevicesAsync()
{
	Window* window = mWindow;

	ApiSystem::getInstance()->startBluetoothLiveDevices([window](const std::string deviceInfo)
	{
		if (Instance != nullptr && Utils::String::startsWith(deviceInfo, "<device "))
		{
			auto id = Utils::String::extractString(deviceInfo, "id=\"", "\"", false);
			auto name = Utils::String::extractString(deviceInfo, "name=\"", "\"", false);
			auto status = Utils::String::extractString(deviceInfo, "status=\"", "\"", false);
			auto type = Utils::String::extractString(deviceInfo, "type=\"", "\"", false);

			window->postToUiThread([id, name, status, type]
			{
				if (Instance == nullptr)
					return;

				if (status == "removed")
					Instance->removeEntry(id);
				else
				{
					std::string icon = type.empty() ? "unknown" : type;
					Instance->addWithDescription(name, id, nullptr, [id]()
					{
						if (Instance != nullptr)
							Instance->onPairDevice(id);
					}, icon, false, false, id, false);
				}
			});
		}
	});	
}

bool GuiBluetoothPair::input(InputConfig* config, Input input)
{
	if (MenuComponent::input(config, input))
		return true;

	if (input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{			
		delete this;
		return true;
	}

	return false;
}

std::vector<HelpPrompt> GuiBluetoothPair::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = MenuComponent::getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("CANCEL")));
	prompts.push_back(HelpPrompt(BUTTON_OK, _("PAIR")));
	return prompts;
}

void GuiBluetoothPair::onPairDevice(const std::string& macAddress)
{
	Window* window = mWindow;

	window->pushGui(new GuiLoading<bool>(window, _("PLEASE WAIT"),
		[this, window, macAddress](auto gui)
	{
		return ApiSystem::getInstance()->pairBluetoothDevice(macAddress);
	},
		[this](bool ret)
	{
		if (ret)
			delete this;
	}));

}
