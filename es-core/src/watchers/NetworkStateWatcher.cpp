#include "NetworkStateWatcher.h"
#include "components/IExternalActivity.h"
#include "utils/Platform.h"

NetworkStateWatcher::NetworkStateWatcher() : mIsConnected(false), mIsPlaneMode(false)
{
	mIsPlaneModeSupported = IExternalActivity::Instance != nullptr && IExternalActivity::Instance->isReadPlaneModeSupported();
}

bool NetworkStateWatcher::check()
{
	bool networkConnected = !Utils::Platform::queryIPAddress().empty();
	bool planemodeEnabled = mIsPlaneModeSupported && IExternalActivity::Instance != nullptr && IExternalActivity::Instance->isPlaneMode();

	bool changed = networkConnected != mIsConnected || mIsPlaneMode != planemodeEnabled;

	mIsConnected = networkConnected;
	mIsPlaneMode = planemodeEnabled;
	
	return changed;
}
