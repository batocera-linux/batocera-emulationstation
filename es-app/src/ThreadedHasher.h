#pragma once

#include <thread>
#include <queue>
#include "components/AsyncNotificationComponent.h"

class FileData;

class ThreadedHasher
{
public:
	static void start(Window* window, bool forceAllGames=false, bool silent=false);
	static void stop();
	static bool isRunning() { return mInstance != nullptr; }
	
	static void pause() { mPaused = true; }
	static void resume() { mPaused = false; }

private:
	ThreadedHasher(Window* window, std::queue<FileData*> searchQueue);
	~ThreadedHasher();

	void hashFile(FileData* fileData);
	std::string formatGameName(FileData* game);

	std::queue<FileData*> mSearchQueue;

	Window* mWindow;
	AsyncNotificationComponent* mWndNotification;
	std::string		mCurrentAction;

	std::vector<std::string> mErrors;

	void run();

	std::thread* mHandle;

	int mTotal;
	bool mExit;

	static bool mPaused;
	static ThreadedHasher* mInstance;
};

