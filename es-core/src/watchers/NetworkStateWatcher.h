#pragma once

#include "WatchersManager.h"
#include <string>

class NetworkStateWatcher : public IWatcher
{
public:
	NetworkStateWatcher();

	bool isConnected() { return mIsConnected; }
	bool isPlaneMode() { return mIsPlaneMode; }

protected:
	bool enabled() override { return true; };

	int  initialUpdateTime() override { return 0; }		// Immediate
	int  updateTime() override { return 5 * 1000; }		// 5 seconds

	bool check() override;

private:
	std::string mIPAddress;
	bool mIsConnected;
	bool mIsPlaneMode;
	bool mIsPlaneModeSupported;
};
