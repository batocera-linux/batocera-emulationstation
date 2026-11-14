#pragma once
#ifndef ES_APP_ROMM_ROMM_DOWNLOADER_H
#define ES_APP_ROMM_ROMM_DOWNLOADER_H

#include <atomic>
#include <functional>
#include <string>

class FileData;

namespace Romm
{
	// Worker thread: downloads the remote game whose ES path is 'path' to that exact path.
	bool downloadGame(const std::string& path, const std::function<void(int percent, long long bytes)>& progress,
		const std::atomic<bool>* cancel, std::string& error);

	// UI thread, after a successful download: the entry becomes a local game (media next to the rom,
	// metadata saved to the gamelist). refreshViews = false for batches (one reload at the end).
	void onGameDownloaded(FileData* game, bool refreshViews = true);

	// Before ES deletes a game's files: a PS3 PSN game is uninstalled from RPCS3 (Batocera)
	void onGameDeleting(FileData* game);

	// A RomM game deleted from the device becomes a cloud game again (true). False: not a RomM game.
	bool onGameDeleted(FileData* game);

	// Media of a game on the device downloaded again: fetched on a worker thread, then placed next to
	// the rom on the UI thread (by FileData, or by path for the background job: nothing if not loaded).
	bool downloadGameMedia(const std::string& path, const std::atomic<bool>* cancel, std::string& error);
	void onGameMediaDownloaded(FileData* game);
	void onGameMediaDownloadedAt(const std::string& path);

	// Games downloaded from RomM by ES (configs/romm/downloaded.txt), forgotten when deleted. Games
	// copied to the device by the user, even ones RomM has, are not in it.
	bool isDownloadedFromRomm(const std::string& path);
	std::string getDownloadedRecordPath();
}

#endif
