#include "NetworkStateWatcher.h"
#include "components/IExternalActivity.h"
#include "utils/Platform.h"
#include <mutex>

namespace
{
	std::mutex networkSnapshotMutex;
	std::mutex networkQueryMutex;
	NetworkStateWatcher::Snapshot networkSnapshot;
	bool networkSnapshotInitialized = false;

	NetworkStateWatcher::Snapshot refreshNetworkSnapshot(bool onlyIfMissing)
	{
		std::lock_guard<std::mutex> queryLock(networkQueryMutex);

		if (onlyIfMissing)
		{
			std::lock_guard<std::mutex> snapshotLock(networkSnapshotMutex);
			if (networkSnapshotInitialized)
				return networkSnapshot;
		}

		NetworkStateWatcher::Snapshot state;
		state.connected = !Utils::Platform::queryIPAddress().empty();

		auto activity = IExternalActivity::Instance;
		state.planeMode = activity != nullptr && activity->isReadPlaneModeSupported() && activity->isPlaneMode();

		{
			std::lock_guard<std::mutex> snapshotLock(networkSnapshotMutex);
			networkSnapshot = state;
			networkSnapshotInitialized = true;
		}

		return state;
	}
}

NetworkStateWatcher::Snapshot NetworkStateWatcher::getSnapshot()
{
	{
		std::lock_guard<std::mutex> lock(networkSnapshotMutex);
		if (networkSnapshotInitialized)
			return networkSnapshot;
	}

	return refreshNetworkSnapshot(true);
}

NetworkStateWatcher::NetworkStateWatcher() : mIsConnected(false), mIsPlaneMode(false)
{
}

bool NetworkStateWatcher::check()
{
	auto state = refreshNetworkSnapshot(false);

	bool changed = state.connected != mIsConnected ||
		state.planeMode != mIsPlaneMode;

	mIsConnected = state.connected;
	mIsPlaneMode = state.planeMode;

	return changed;
}
