#pragma once

#include "WatchersManager.h"

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
	bool mIsConnected;
	bool mIsPlaneMode;
	bool mIsPlaneModeSupported;
};
