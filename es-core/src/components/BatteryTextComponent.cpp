#include "components/BatteryTextComponent.h"
#include "Settings.h"
#include <time.h>
#include "watchers/BatteryLevelWatcher.h"

#define UPDATE_NETWORK_DELAY	2000

BatteryTextComponent::BatteryTextComponent(Window* window) : TextComponent(window), mDirty(true)
{
	WatchersManager::getInstance()->RegisterNotify(this);
	mBatteryInfo = BatteryLevelWatcher::getSnapshot();
}

BatteryTextComponent::~BatteryTextComponent()
{
	WatchersManager::getInstance()->UnregisterNotify(this);
}

void BatteryTextComponent::OnWatcherChanged(IWatcher* component)
{
	if (dynamic_cast<BatteryLevelWatcher*>(component) != nullptr)
		mDirty.store(true);
}

void BatteryTextComponent::update(int deltaTime)
{
	TextComponent::update(deltaTime);

	if (!mDirty.exchange(false))
		return;

	mBatteryInfo = BatteryLevelWatcher::getSnapshot();

	if (Settings::getInstance()->getString("ShowBattery") != "text")
		mBatteryInfo.hasBattery = false;

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
}
