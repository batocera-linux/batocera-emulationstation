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
	mExit = false;
	mTotal = (int) mSearchQueue.size();

	mWndNotification = new AsyncNotificationComponent(window);

	mWindow->registerNotificationComponent(mWndNotification);
	search(mSearchQueue.front());
	mHandle = new std::thread(&ThreadedScraper::run, this);	
}

ThreadedScraper::~ThreadedScraper()
{
	mWindow->unRegisterNotificationComponent(mWndNotification);
	delete mWndNotification;

	ThreadedScraper::mInstance = nullptr;
}

std::string ThreadedScraper::formatGameName(FileData* game)
{
	return "["+game->getSystemName()+"] " + game->getName();
}

void ThreadedScraper::search(const ScraperSearchParams& params)
{
	LOG(LogDebug) << "ThreadedScraper::formatGameName";

	std::string gameName = formatGameName(params.game);

	LOG(LogInfo) << "ThreadedScraper::search >> " << gameName;

	mCurrentAction = "";
	mLastSearch = params;
	mSearchHandle = startScraperSearch(params);

	std::string idx = std::to_string(mTotal + 1 - mSearchQueue.size()) + "/" + std::to_string(mTotal);

	mWndNotification->updateTitle(GUIICON + _("SCRAPING") + "... " + idx);
	mWndNotification->updateText(gameName, _("Searching")+"...");
	mWndNotification->updatePercent(-1);

	LOG(LogDebug) << "ThreadedScraper::search <<";
}

void ThreadedScraper::processError(int status, const std::string statusString)
{
	if (status == HttpReq::REQ_430_TOOMANYSCRAPS || status == HttpReq::REQ_430_TOOMANYFAILURES || 
		status == HttpReq::REQ_426_BLACKLISTED || status == HttpReq::REQ_FILESTREAM_ERROR || status == HttpReq::REQ_426_SERVERMAINTENANCE ||
		status == HttpReq::REQ_403_BADLOGIN || status == HttpReq::REQ_401_FORBIDDEN)
	{
		mExit = true;
		mWindow->postToUiThread([statusString](Window* w) { w->pushGui(new GuiMsgBox(w, _("SCRAPE FAILED") + " : " + statusString)); });
	}
	else
		mErrors.push_back(statusString);
}

void ThreadedScraper::run()
{
	while (!mExit && !mSearchQueue.empty())
	{
		if (mPaused)
		{
			while (!mExit && mPaused)
			{
				std::this_thread::yield();
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}
		}

		if (mSearchHandle && mSearchHandle->status() != ASYNC_IN_PROGRESS)
		{
			auto status = mSearchHandle->status();
			auto results = mSearchHandle->getResults();
			auto statusString = mSearchHandle->getStatusString();
			auto httpCode = mSearchHandle->getErrorCode();

			LOG(LogDebug) << "ThreadedScraper::SearchResponse : " << httpCode << " " << statusString;

			mSearchHandle.reset();

			if (status == ASYNC_DONE)
			{
				if (results.size() > 0)
				{
					if (results[0].hadMedia())
						processMedias(results[0]);
					else
						acceptResult(results[0]);
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

			LOG(LogDebug) << "ThreadedScraper::ResolveResponse : " << statusString;

			mCurrentAction = "";
			mMDResolveHandle.reset();

			if (status == ASYNC_DONE)
				acceptResult(result);
			else if (status == ASYNC_ERROR)
				processError(httpCode, statusString);
		}

		if (mMDResolveHandle && mMDResolveHandle->status() == ASYNC_IN_PROGRESS)
		{
			std::string action = mMDResolveHandle->getCurrentItem();
			if (action != mCurrentAction)
			{
				mCurrentAction = action;
				mWndNotification->updateText(formatGameName(mLastSearch.game), _("Downloading") + " " + mCurrentAction);
			}

			mWndNotification->updatePercent(mMDResolveHandle->getPercent());
		}

		if (mSearchHandle == nullptr && mMDResolveHandle == nullptr)
		{
			mSearchQueue.pop();

			if (mSearchQueue.empty())
			{
				LOG(LogDebug) << "ThreadedScraper::finished";
				break;
			}
			
			search(mSearchQueue.front());
		}
		else
		{
			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
	
	if (!mExit)
		mWindow->displayNotificationMessage(GUIICON + _("SCRAPING FINISHED. REFRESH UPDATE GAMES LISTS TO APPLY CHANGES."));

	delete this;
	ThreadedScraper::mInstance = nullptr;
}

void ThreadedScraper::processMedias(ScraperSearchResult result)
{
	LOG(LogDebug) << "ThreadedScraper::processMedias >>";
	mMDResolveHandle = resolveMetaDataAssets(result, mLastSearch);
	LOG(LogDebug) << "ThreadedScraper::processMedias <<";
}

void ThreadedScraper::acceptResult(const ScraperSearchResult& result)
{
	LOG(LogDebug) << "ThreadedScraper::acceptResult >>";

	ScraperSearchParams& search = mSearchQueue.front();

	auto game = search.game;

	mWindow->postToUiThread([game, result](Window* w)
	{
		LOG(LogDebug) << "ThreadedScraper::importScrappedMetadata";
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
		thread->mExit = true;
	}
	catch (...) {}
}

