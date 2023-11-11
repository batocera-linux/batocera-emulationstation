#include "BatteryLevelWatcher.h"

bool BatteryLevelWatcher::check()
{
	auto batteryInfo = Utils::Platform::queryBatteryInformation();
	bool changed = batteryInfo.hasBattery != mBatteryInfo.hasBattery || batteryInfo.isCharging != mBatteryInfo.isCharging || batteryInfo.level != mBatteryInfo.level;
	mBatteryInfo = batteryInfo;
	return changed;
}