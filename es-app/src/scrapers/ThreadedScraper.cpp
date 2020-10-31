#include "ThreadedScraper.h"
#include "Window.h"
#include "FileData.h"
#include "components/AsyncNotificationComponent.h"
#include "LocaleES.h"
#include "guis/GuiMsgBox.h"
#include "Gamelist.h"
#include "Log.h"

#define GUIICON _U("\uF03E ")

ThreadedScraper* ThreadedScraper::mInstance = nullptr;
bool ThreadedScraper::mPaused = false;

ThreadedScraper::ThreadedScraper(Window* window, const std::queue<ScraperSearchParams>& searches)
	: mSearchQueue(searches), mWindow(window)
{
	mExitCode = ASYNC_IN_PROGRESS;
	mTotal = (int) mSearchQueue.size();

	int threadCount = Scraper::getScraper()->getThreadCount();
	if (threadCount <= 0)
		threadCount = 1;

	mWndNotification = mWindow->createAsyncNotificationComponent();
	mWndNotification->updateTitle(GUIICON + _("SCRAPING"));

	for (int i = 0; i < threadCount; i++)
	{
		if (mSearchQueue.size() == 0)
			break;

		ScraperThread* thread = new ScraperThread(i);
		mScraperThreads.push_back(thread);
		ProcessNextGame(thread);
	}	

	mHandle = new std::thread(&ThreadedScraper::run, this);	
}

void ThreadedScraper::ProcessNextGame(ScraperThread* thread)
{
	auto item = mSearchQueue.front();
	mSearchQueue.pop();
	mCurrentGame = item.getGameName();

	LOG(LogInfo) << "[Thread " << thread->mThreadId << "] ProcessNextGame : " << mCurrentGame;

	thread->run(item);

	updateUI();
}

ThreadedScraper::~ThreadedScraper()
{
	mWndNotification->close();
	mWndNotification = nullptr;

	for (auto scraperThread : mScraperThreads)
		delete scraperThread;

	mScraperThreads.clear();

	ThreadedScraper::mInstance = nullptr;
}

std::string ThreadedScraper::formatGameName(FileData* game)
{
	return "["+game->getSystemName()+"] " + game->getName();
}

ScraperThread::ScraperThread(int threadId)
{
	mThreadId = threadId;
	mErrorStatus = 0;
	mStatus = ASYNC_IN_PROGRESS;
}

void ScraperThread::run(const ScraperSearchParams& params)
{
	mResult = ScraperSearchResult();
	mErrorStatus = 0;
	mStatusString = "";
	mStatus = ASYNC_IN_PROGRESS;
	mSearch = params;
	mMDResolveHandle.reset();

	mSearchHandle = Scraper::getScraper()->search(params);
}

int ScraperThread::updateState()
{
	if (mSearchHandle && mSearchHandle->status() != ASYNC_IN_PROGRESS)
	{
		auto status = mSearchHandle->status();
		auto results = mSearchHandle->getResults();
		auto statusString = mSearchHandle->getStatusString();
		auto httpCode = mSearchHandle->getErrorCode();

		LOG(LogInfo) << "[Thread " << mThreadId << "] ThreadedScraper::SearchResponse : " << httpCode << " " << statusString;

		mSearchHandle.reset();

		if (status == ASYNC_DONE)
		{
			if (results.size() > 0)
			{
				if (results[0].hasMedia())
					mMDResolveHandle = results[0].resolveMetaDataAssets(mSearch);
				else
					acceptResult(results[0]);
			}
			else
			{
				mStatus = ASYNC_DONE;
				mErrorStatus = 0;
			}
		}
		else if (status == ASYNC_ERROR)
			processError(httpCode, statusString);
	}

	if (mMDResolveHandle && mMDResolveHandle->status() != ASYNC_IN_PROGRESS)
	{
		auto status = mMDResolveHandle->status();
		auto result = mMDResolveHandle->getResult();
		auto statusString = mMDResolveHandle->getStatusString();
		auto httpCode = mMDResolveHandle->getErrorCode();

		LOG(LogInfo) << "[Thread " << mThreadId << "] ResolveResponse : " << statusString;

		mMDResolveHandle.reset();

		if (status == ASYNC_DONE)
			acceptResult(result);
		else if (status == ASYNC_ERROR)
			processError(httpCode, statusString);
	}

	return mStatus;
}

void ThreadedScraper::processError(int status, const std::string statusString)
{
	if (status == HttpReq::REQ_430_TOOMANYSCRAPS || status == HttpReq::REQ_430_TOOMANYFAILURES || 
		status == HttpReq::REQ_426_BLACKLISTED || status == HttpReq::REQ_FILESTREAM_ERROR || status == HttpReq::REQ_426_SERVERMAINTENANCE ||
		status == HttpReq::REQ_403_BADLOGIN || status == HttpReq::REQ_401_FORBIDDEN)
	{
		mExitCode = ASYNC_ERROR;
		mWindow->postToUiThread([statusString](Window* w) { w->pushGui(new GuiMsgBox(w, _("SCRAPE FAILED") + " : " + statusString)); });
	}
	else
		mErrors.push_back(statusString);
}

void ThreadedScraper::run()
{
	while (mExitCode == ASYNC_IN_PROGRESS)
	{
		if (mPaused)
		{
			while (mExitCode == ASYNC_IN_PROGRESS && mPaused)
			{
				std::this_thread::yield();
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}
		}
		
		for (auto iter = mScraperThreads.cbegin(); iter != mScraperThreads.cend(); ++iter)
		{
			if (mExitCode != ASYNC_IN_PROGRESS)
				break;

			auto mScraperThread = *iter;

			int state = mScraperThread->updateState();
			switch (state)
			{
			case ASYNC_DONE:
				acceptResult(*mScraperThread);
				break;

			case ASYNC_ERROR:
				processError(mScraperThread->getError(), mScraperThread->getErrorString());
				break;

			default:
				//std::this_thread::yield();
				// std::this_thread::sleep_for(std::chrono::milliseconds(10));
				break;
			}

			if (mExitCode == ASYNC_IN_PROGRESS && state != ASYNC_IN_PROGRESS)
			{
				if (!mSearchQueue.empty())
					ProcessNextGame(mScraperThread);
				else
				{					
					mScraperThreads.erase(iter);
					if (mScraperThreads.size() == 0)
					{
						mExitCode = ASYNC_DONE;
						LOG(LogDebug) << "ThreadedScraper::finished";
					}
					break;
				}
			}
		}
	}
	
	if (mExitCode == ASYNC_DONE)
		mWindow->displayNotificationMessage(GUIICON + _("SCRAPING FINISHED. REFRESH UPDATE GAMES LISTS TO APPLY CHANGES."));

	delete this;
	ThreadedScraper::mInstance = nullptr;
}

void ThreadedScraper::updateUI()
{
	int remaining = mTotal + 1 - mSearchQueue.size() - mScraperThreads.size();
	if (remaining < 0)
		remaining = 0;

	std::string idx = std::to_string(remaining) + "/" + std::to_string(mTotal);	
	int percentDone = remaining * 100 / (mTotal + 1);

	mWndNotification->updateTitle(GUIICON + _("SCRAPING") + " " + idx);
	mWndNotification->updateText(mCurrentGame);
	mWndNotification->updatePercent(percentDone);
}

void ThreadedScraper::acceptResult(ScraperThread& thread)
{
	LOG(LogDebug) << "ThreadedScraper::acceptResult >>";

	ScraperSearchResult& result = thread.getResult();
	if (result.mdl.getName().empty())
		return;

	ScraperSearchParams& search = thread.getSearchParams();
	auto game = search.game;

	mWindow->postToUiThread([game, result](Window* w)
	{
		LOG(LogDebug) << "ThreadedScraper::importScrappedMetadata";
		game->importP2k(result.p2k);
		game->getMetadata().importScrappedMetadata(result.mdl);
		game->detectLanguageAndRegion(true);

		LOG(LogDebug) << "ThreadedScraper::saveToGamelistRecovery";
		saveToGamelistRecovery(game);
	});

	LOG(LogDebug) << "ThreadedScraper::acceptResult <<";
}

void ThreadedScraper::start(Window* window, const std::queue<ScraperSearchParams>& searches)
{
	if (ThreadedScraper::mInstance != nullptr)
		return;

	ThreadedScraper::mInstance = new ThreadedScraper(window, searches);
}

void ThreadedScraper::stop()
{
	auto thread = ThreadedScraper::mInstance;
	if (thread == nullptr)
		return;

	try
	{
		thread->mExitCode = ASYNC_DONE;
	}
	catch (...) {}
}

