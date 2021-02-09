#pragma once

#include <thread>
#include "Scraper.h"
#include "components/AsyncNotificationComponent.h"

class ScraperThread
{
public:
	ScraperThread(int threadId);
	void run(const ScraperSearchParams& params);
	int updateState();

	ScraperSearchParams& getSearchParams() { return mSearch; }
	ScraperSearchResult& getResult() { return mResult; }

	int getError() { return mErrorStatus; }
	std::string getErrorString() { return mStatusString; }

	int mThreadId;

private:
	void acceptResult(ScraperSearchResult& result)
	{
		mResult = result;
		mStatus = ASYNC_DONE;
		mErrorStatus = 0;
	}

	void processError(int status, const std::string statusString)
	{
		mStatus = ASYNC_ERROR;
		mErrorStatus = status;
		mStatusString = statusString;
	}

	
	int mStatus;
	int mErrorStatus;
	std::string mStatusString;

	ScraperSearchResult mResult;
	ScraperSearchParams mSearch;
	std::unique_ptr<ScraperSearchHandle> mSearchHandle;
	std::unique_ptr<MDResolveHandle> mMDResolveHandle;
};


class ThreadedScraper
{
public:
	static void start(Window* window, const std::queue<ScraperSearchParams>& searches);
	static void stop();
	static bool isRunning() { return mInstance != nullptr; }
	
	static void pause() { mPaused = true; }
	static void resume() { mPaused = false; }

	static std::string formatGameName(FileData* game);

private:
	ThreadedScraper(Window* window, const std::queue<ScraperSearchParams>& searches, int threadCount);
	~ThreadedScraper();

	void ProcessNextGame(ScraperThread* thread);

	Window* mWindow;
	AsyncNotificationComponent* mWndNotification;
	
	std::string		mCurrentGame;

	std::vector<std::string> mErrors;

	void run();

	std::thread* mHandle;
	std::queue<ScraperSearchParams> mSearchQueue;

	std::vector<ScraperThread*> mScraperThreads;
	
	void acceptResult(ScraperThread& thread);
	void processError(int status, const std::string statusString);
	void updateUI();

	int mTotal;
	int mExitCode;

	static bool mPaused;
	static ThreadedScraper* mInstance;
};

