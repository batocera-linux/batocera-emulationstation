#pragma once
#ifndef ES_APP_ROMM_ROMM_WORK_H
#define ES_APP_ROMM_ROMM_WORK_H

#include <atomic>
#include <cstddef>
#include <functional>

namespace Romm
{
	// Background work for RomM, kind to the device:
	// - a few workers at most, fewer when the system is busy (CPU cores, load average)
	// - background threads run at a lower priority (nice 10 on Linux)
	// - paused while a game runs

	// Number of parallel workers to use now: CPU cores - 1, between 1 and 4, reduced when the load is high
	int backgroundWorkers();

	// Runs work(i) for i in [0, count) on up to 'workers' threads; stops taking new items when 'stop' is set
	void runInBackground(size_t count, int workers, const std::function<void(size_t)>& work, const std::atomic<bool>& stop);

	// Lowers the priority of the calling thread (no-op where not supported)
	void lowerThreadPriority();

	// FileData::launchGame sets it while a game runs; background work waits meanwhile
	void setGameRunning(bool running);
	bool isGameRunning();

	// PAUSE SYNC: background work waits at its next step until resumed
	void setBackgroundPaused(bool paused);
	bool isBackgroundPaused();

	// CANCEL SYNC: background work stops at its next step, paused or not
	void setBackgroundAborted(bool aborted);
	bool isBackgroundAborted();

	// Waits while a game runs or the work is paused; returns at once when stopped or aborted
	void waitWhileGameRunning(const std::atomic<bool>& stop);
}

#endif
