#pragma once
#ifndef ES_APP_ROMM_ROMM_PLAY_SESSIONS_H
#define ES_APP_ROMM_ROMM_PLAY_SESSIONS_H

#include <ctime>
#include <string>

namespace Romm
{
	// Play sessions of RomM games, sent to RomM (POST /api/play-sessions, scope roms.user.write).
	// Sessions are queued on disk first (configs/romm/play-sessions.txt), so nothing is lost when the
	// server cannot be reached: the queue is sent after each game and at every sync.
	class RommPlaySessions
	{
	public:
		// FileData::launchGame, where ES adds to <gametime>. No-op unless "SYNC PLAY TIME" is on
		// and the game is a RomM game.
		static void record(const std::string& gamePath, time_t start, time_t end);

		// Sends the queue now, on the calling thread (worker threads only)
		static void flush();

		// Sends the queue in the background
		static void flushAsync();

		// ES shutdown: waits for a send in progress
		static void stop();

		static size_t pendingCount();
		static bool missingPermission(); // the token cannot write play sessions: pair again

		static std::string getQueuePath();
	};
}

#endif
