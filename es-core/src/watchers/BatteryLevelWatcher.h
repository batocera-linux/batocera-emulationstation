#pragma once

#include "WatchersManager.h"
#include "utils/Platform.h"

class BatteryLevelWatcher : public IWatcher
{
public:
	Utils::Platform::BatteryInformation& getBatteryInfo() { return mBatteryInfo; }

protected:
	bool enabled() override { return true; };

	int  initialUpdateTime() override { return 0; }		// Immediate
	int  updateTime() override { return 5 * 1000; }		// 5 seconds

	bool check() override;

private:
	Utils::Platform::BatteryInformation mBatteryInfo;	
};
