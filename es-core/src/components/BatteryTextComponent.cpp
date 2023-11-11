#include "components/BatteryTextComponent.h"
#include "Settings.h"
#include <time.h>
#include "watchers/BatteryLevelWatcher.h"

#define UPDATE_NETWORK_DELAY	2000

BatteryTextComponent::BatteryTextComponent(Window* window) : TextComponent(window), mDirty(true)
{		
	mBatteryInfo = Utils::Platform::BatteryInformation();

	WatchersManager::getInstance()->RegisterNotify(this);

	BatteryLevelWatcher* watcher = WatchersManager::GetComponent<BatteryLevelWatcher>();
	if (watcher != nullptr)
		mBatteryInfo = watcher->getBatteryInfo();
}

BatteryTextComponent::~BatteryTextComponent()
{
	WatchersManager::getInstance()->UnregisterNotify(this);
}

void BatteryTextComponent::OnWatcherChanged(IWatcher* component)
{
	BatteryLevelWatcher* watcher = dynamic_cast<BatteryLevelWatcher*>(component);
	if (watcher != nullptr)
	{
		mBatteryInfo = watcher->getBatteryInfo();
		mDirty = true;
	}
}

void BatteryTextComponent::update(int deltaTime)
{
	TextComponent::update(deltaTime);
		
	if (!mDirty)
		return;
	
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

	mDirty = false;
}
