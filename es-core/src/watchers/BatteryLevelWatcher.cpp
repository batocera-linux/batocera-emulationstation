#include "BatteryLevelWatcher.h"
#include <mutex>

namespace
{
	std::mutex batterySnapshotMutex;
	std::mutex batteryQueryMutex;
	Utils::Platform::BatteryInformation batterySnapshot;
	bool batterySnapshotInitialized = false;

	Utils::Platform::BatteryInformation refreshBatterySnapshot(
		bool onlyIfMissing)
	{
		// Only one thread may read the battery at a time.
		std::lock_guard<std::mutex> queryLock(batteryQueryMutex);

		if (onlyIfMissing)
		{
			std::lock_guard<std::mutex> snapshotLock(batterySnapshotMutex);
			if (batterySnapshotInitialized)
				return batterySnapshot;
		}

		auto info = Utils::Platform::queryBatteryInformation();

		{
			std::lock_guard<std::mutex> snapshotLock(batterySnapshotMutex);
			batterySnapshot = info;
			batterySnapshotInitialized = true;
		}

		return info;
	}
}

Utils::Platform::BatteryInformation BatteryLevelWatcher::getSnapshot()
{
	{
		std::lock_guard<std::mutex> lock(batterySnapshotMutex);
		if (batterySnapshotInitialized)
			return batterySnapshot;
	}

	return refreshBatterySnapshot(true);
}

bool BatteryLevelWatcher::check()
{
	auto batteryInfo = refreshBatterySnapshot(false);
	bool changed = batteryInfo.hasBattery != mBatteryInfo.hasBattery || batteryInfo.isCharging != mBatteryInfo.isCharging || batteryInfo.level != mBatteryInfo.level;
	mBatteryInfo = batteryInfo;

	return changed;
}
