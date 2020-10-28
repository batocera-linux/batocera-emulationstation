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

	if (Renderer::isSmallScreen())
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

	if (ResourceManager::getInstance()->fileExists(":/battery/incharge.svg"))
		mIncharge = ResourceManager::getInstance()->getResourcePath(":/battery/incharge.svg");

	if (ResourceManager::getInstance()->fileExists(":/battery/full.svg"))
		mFull = ResourceManager::getInstance()->getResourcePath(":/battery/full.svg");

	if (ResourceManager::getInstance()->fileExists(":/battery/75.svg"))
		mAt75 = ResourceManager::getInstance()->getResourcePath(":/battery/75.svg");

	if (ResourceManager::getInstance()->fileExists(":/battery/50.svg"))
		mAt50 = ResourceManager::getInstance()->getResourcePath(":/battery/50.svg");

	if (ResourceManager::getInstance()->fileExists(":/battery/25.svg"))
		mAt25 = ResourceManager::getInstance()->getResourcePath(":/battery/25.svg");

	if (ResourceManager::getInstance()->fileExists(":/battery/empty.svg"))
		mEmpty = ResourceManager::getInstance()->getResourcePath(":/battery/empty.svg");	

	if (ResourceManager::getInstance()->fileExists(":/battery/empty.svg"))
		mEmpty = ResourceManager::getInstance()->getResourcePath(":/battery/empty.svg");

	if (ResourceManager::getInstance()->fileExists(":/network.svg"))
	{
		mView |= ActivityView::NETWORK;
		mNetworkImage = TextureResource::get(ResourceManager::getInstance()->getResourcePath(":/network.svg"), false, true);
	}

	updateBatteryInfo();
}
