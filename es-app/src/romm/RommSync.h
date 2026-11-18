#pragma once
#ifndef ES_APP_ROMM_ROMM_SYNC_H
#define ES_APP_ROMM_ROMM_SYNC_H

#include "romm/RommMedia.h"

#include <atomic>
#include <vector>
#include <mutex>
#include <string>
#include <thread>

class Window;

namespace Romm
{
	// Background synchronisation of the RomM library into the local snapshot.
	// Never touches the game lists of a running ES: changes are applied by the next
	// load of the systems (ES start, "apply changes" in the RomM menu, any reload).
	class RommSync
	{
	public:
		static void start(Window* window);   // ES startup: one incremental sync (retries while the network comes up)
		static void stop();                  // ES shutdown: cancels and joins

		static bool requestSync(bool full);

		static Window* getWindow() { return sWindow; }

		// Media of these games on the device downloaded again, in the background like a sync. False: busy.
		static bool requestMediaRefresh(const std::vector<std::string>& gamePaths);
		static bool isRunning() { return sRunning; }

		static std::string getStatus();
		static bool hasPendingChanges() { return sPendingChanges; }

		// Media settings changed: what the media downloads fetch (a type turned off stops its downloads)
		static void applyMediaSettings();

		// A cancelled synchronization keeps nothing: the next one does the work again.
		static void pause();
		static void resume();
		static bool isPaused();
		static bool cancel();

		// Turning a type off also stops its downloads in progress
		static void setMediaSyncEnabled(Media type, bool enabled) { sMediaEnabled[mediaIndex(type)] = enabled; }
		static void clearPendingChanges() { sPendingChanges = false; }

	private:
		static bool launch(bool full, bool retryNetwork);
		static void run(bool full, bool retryNetwork);
		static void runMediaRefresh(std::vector<std::string> gamePaths);
		static void setStatus(const std::string& status);          // a final status (summary, error)

		// Progress while running ("SYNCHRONIZING... 1234"): ignored once the run failed (parallel workers
		// must not hide the error), replaced by SYNC INTERRUPTED if the run ends on it
		static void setProgressStatus(const std::string& status);
		static std::atomic<bool> sStatusIsProgress;
		static std::atomic<bool> sRunFailed;
		static void notify(const std::string& message);

		// Progress box in the corner of the screen (worker side, marshalled to the UI thread)
		static void showProgress(const std::string& text, const std::string& counts, int percent);
		static void closeProgress();

		static Window* sWindow;
		static std::thread sThread;
		static std::mutex sMutex;
		static std::string sStatus;
		static std::atomic<bool> sRunning;
		static std::atomic<bool> sCancel;
		static std::atomic<bool> sPendingChanges;
		static std::atomic<bool> sMediaEnabled[(size_t)Media::Count];
	};

	// Exposed for the downloader: absolute URL path of a cover on the server
	std::string coverPath(const std::string& coverUrl);
}

#endif
