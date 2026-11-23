#pragma once
#ifndef ES_APP_ROMM_ROMM_SAVES_H
#define ES_APP_ROMM_ROMM_SAVES_H

#include "romm/RommSaveLayout.h"

#include <functional>
#include <string>
#include <vector>

class FileData;
class Window;

namespace Romm
{
	// Save sync through RomM channels and snapshots. A per-device ledger records the snapshot each game
	// last synced; pushes name the current they build on, so RomM keeps a stale push as a branch (409).

	// Taken from the game on the UI thread
	struct SaveJob
	{
		std::string path;    // ledger key
		std::string name;
		std::string system;
		int romFileId = 0;
		SaveLayout layout;
		bool hardcore = false; // RetroAchievements hardcore session
	};

	struct SaveConflict
	{
		std::string gamePath;
		std::string gameName;
		bool afterPlay = false;     // found after the game (409), else before launch
		bool hardcore = false;      // RomM refused to replace a hardcore save
		long long currentId = 0;
		std::string currentDevice;  // who pushed the current
		std::string currentDate;    // RomM's date
		std::string localDate;      // this device's save
	};

	enum class SaveCheck
	{
		Ready,     // launch
		Offline,   // launch with the local save, synced after the game
		Conflict,  // the player chooses
		Failed     // launch with the local save (logged)
	};

	enum class SaveChoice
	{
		UseServer,       // the local save is backed up
		KeepLocal,       // the other stays in history
		KeepBoth,        // a channel of its own for this device
		ApproveHardcore  // softcore save replaces a hardcore one
	};

	// argosy-launcher's SnapshotDecision table
	enum class SaveAction { Nothing, Push, Download, Adopt, Conflict };
	SaveAction decideSave(long long held, const std::string& heldPrint, bool heldByChoice,
		long long current, const std::string& currentPrint, bool localEmpty, const std::string& localPrint);

	class RommSaves
	{
	public:
		static bool isEnabled();
		// UI thread. False when the game does not sync (not from RomM, no save layout)
		static bool prepare(FileData* game, SaveJob& job);

		// Worker thread
		static SaveCheck beforeLaunch(const SaveJob& job, SaveConflict& conflict, std::string& message);
		static bool afterGame(const SaveJob& job, std::string& message);
		static bool resolve(const SaveJob& job, SaveChoice choice, std::string& message);

		// The session replaced an autosave that came from another device (conflict.currentDevice)
		static bool replacedCloudAutosave(const SaveJob& job, SaveConflict& conflict);
		// Restores a slot of the held snapshot, the local one backed up
		static bool restoreHeldSlot(const SaveJob& job, const std::string& slot, std::string& message);

		// Achievements and hardcore on for this game (game, system, then global setting), and it has achievements
		static bool isHardcoreSession(FileData* game);
		// The save a hardcore session would continue was made in softcore (device empty: this one)
		static bool softcoreSaveForHardcore(const SaveJob& job, std::string& device);

		// A conflict stored after the game
		static bool pendingConflict(const std::string& path, SaveConflict& conflict);

		// UI thread
		static void recoverAsync(Window* window);    // games played and not pushed yet

		// The launch right after a sync does not sync again
		static void markChecked(const std::string& path);
		static bool consumeChecked(const std::string& path);
		static bool isChecked(const std::string& path); // not consumed

		// Tests
		static void setLedgerPath(const std::string& path);
		static std::string ledgerPath();
	};
}

#endif
