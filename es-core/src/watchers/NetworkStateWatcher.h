#pragma once

#include "WatchersManager.h"

class NetworkStateWatcher : public IWatcher
{
public:
	struct Snapshot
	{
		bool connected = false;
		bool planeMode = false;
	};

	static Snapshot getSnapshot();
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
};
