#pragma once

#include <thread>
#include <queue>
#include <set>
#include "components/AsyncNotificationComponent.h"

class FileData;

class ThreadedHasher
{
public:
	enum HasherType : unsigned int
	{
		HASH_NETPLAY_CRC = 1,
		HASH_CHEEVOS_MD5 = 2,
		HASH_ALL = HASH_NETPLAY_CRC | HASH_CHEEVOS_MD5,
	};

	static void start(Window* window, HasherType type, bool forceAllGames=false, bool silent=false);
	static void stop();
	static bool isRunning() { return mInstance != nullptr; }
	
	static void pause() { mPaused = true; }
	static void resume() { mPaused = false; }

private:
	ThreadedHasher(Window* window, HasherType type, std::queue<FileData*> searchQueue, bool forceAllGames = false);
	~ThreadedHasher();

	void updateUI(const std::string label);
	static std::string formatGameName(FileData* game);

	std::queue<FileData*> mSearchQueue;

	Window* mWindow;
	AsyncNotificationComponent* mWndNotification;
	std::string		mCurrentAction;

	std::vector<std::string> mErrors;
	std::map<std::string, std::string>    mCheevosHashes;

	HasherType mType;

	void run();

	//std::thread* mHandle;
	std::vector<std::thread*>	mThreads;
	int							mThreadCount;

	int mTotal;
	bool mExit;
	bool mForce;

	static bool mPaused;
	static ThreadedHasher* mInstance;
};

