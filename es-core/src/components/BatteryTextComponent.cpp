#include "components/BatteryTextComponent.h"
#include "Settings.h"
#include <time.h>

#define UPDATE_NETWORK_DELAY	2000

BatteryTextComponent::BatteryTextComponent(Window* window) : TextComponent(window)
{	
	mBatteryInfoCheckTime = UPDATE_NETWORK_DELAY;
	mBatteryInfo = Utils::Platform::BatteryInformation();
}

void BatteryTextComponent::update(int deltaTime)
{
	TextComponent::update(deltaTime);
	
	mBatteryInfoCheckTime += deltaTime;
	if (mBatteryInfoCheckTime >= UPDATE_NETWORK_DELAY)
	{
		if (Settings::getInstance()->getString("ShowBattery") != "text")
			mBatteryInfo.hasBattery = false;
		else 
			mBatteryInfo = Utils::Platform::queryBatteryInformation();

		setVisible(mBatteryInfo.hasBattery && mBatteryInfo.level >= 0);

		auto batteryText = std::to_string(mBatteryInfo.level) + "%";

		float sx = mSize.x();

		mAutoCalcExtent.x() = 1;

		setText(batteryText);

		if (mSize.x() != sx)
		{
			auto sz = mSize;
			mSize.x() = sx;
			setSize(sz);
		}

		mBatteryInfoCheckTime = 0;
	}
}
