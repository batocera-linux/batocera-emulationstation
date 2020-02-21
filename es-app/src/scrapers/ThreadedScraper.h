#pragma once

#include <thread>
#include "Scraper.h"
#include "components/AsyncNotificationComponent.h"

class ThreadedScraper
{
public:
	static void start(Window* window, const std::queue<ScraperSearchParams>& searches);
	static void stop();
	static bool isRunning() { return mInstance != nullptr; }
	
	static void pause() { mPaused = true; }
	static void resume() { mPaused = false; }

private:
	ThreadedScraper(Window* window, const std::queue<ScraperSearchParams>& searches);
	~ThreadedScraper();

	Window* mWindow;
	AsyncNotificationComponent* mWndNotification;
	std::string		mCurrentAction;

	std::vector<std::string> mErrors;

	void run();

	std::thread* mHandle;
	std::queue<ScraperSearchParams> mSearchQueue;

	ScraperSearchParams mLastSearch;
	std::unique_ptr<ScraperSearchHandle> mSearchHandle;
	std::unique_ptr<MDResolveHandle> mMDResolveHandle;

	void search(const ScraperSearchParams& params);
	void processMedias(ScraperSearchResult result);
	void acceptResult(const ScraperSearchResult& result);
	void processError(int status, const std::string statusString);

	std::string formatGameName(FileData* game);

	int mTotal;
	bool mExit;

	static bool mPaused;
	static ThreadedScraper* mInstance;
};

