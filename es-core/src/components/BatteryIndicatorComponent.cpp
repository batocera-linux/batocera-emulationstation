#include "components/BatteryIndicatorComponent.h"

#include "resources/TextureResource.h"
#include "ThemeData.h"
#include "InputManager.h"
#include "Settings.h"
#include <SDL_power.h>
#include "utils/StringUtil.h"

BatteryIndicatorComponent::BatteryIndicatorComponent(Window* window) : ControllerActivityComponent(window) { }

void BatteryIndicatorComponent::init()
{
	ControllerActivityComponent::init();

	mHorizontalAlignment = ALIGN_RIGHT;

	if (Renderer::ScreenSettings::isSmallScreen())
	{
		setPosition(Renderer::getScreenWidth() * 0.915, Renderer::getScreenHeight() * 0.013);
		setSize(Renderer::getScreenWidth() * 0.07, Renderer::getScreenHeight() * 0.07);
	}
	else
	{
		setPosition(Renderer::getScreenWidth() * 0.955, Renderer::getScreenHeight() *0.0125);
		setSize(Renderer::getScreenWidth() * 0.033, Renderer::getScreenHeight() *0.033);
	}

	mView = ActivityView::BATTERY;	

	if (ResourceManager::getInstance()->fileExists(":/network.svg"))
	{
		mView |= ActivityView::NETWORK;
		mNetworkImage = TextureResource::get(ResourceManager::getInstance()->getResourcePath(":/network.svg"), false, true);
	}

	updateBatteryInfo();
}
