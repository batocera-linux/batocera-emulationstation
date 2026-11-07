#include "NetworkStateWatcher.h"
#include "components/IExternalActivity.h"
#include "utils/Platform.h"

NetworkStateWatcher::NetworkStateWatcher() : mIsConnected(false), mIsPlaneMode(false)
{
	mIsPlaneModeSupported = IExternalActivity::Instance != nullptr && IExternalActivity::Instance->isReadPlaneModeSupported();
}

bool NetworkStateWatcher::check()
{
	std::string ipAddress = Utils::Platform::queryIPAddress();
	bool planemodeEnabled = mIsPlaneModeSupported && IExternalActivity::Instance != nullptr && IExternalActivity::Instance->isPlaneMode();

	bool changed = ipAddress != mIPAddress || mIsPlaneMode != planemodeEnabled;

	mIPAddress = ipAddress;
	mIsConnected = !ipAddress.empty();
	mIsPlaneMode = planemodeEnabled;
	
	return changed;
}
