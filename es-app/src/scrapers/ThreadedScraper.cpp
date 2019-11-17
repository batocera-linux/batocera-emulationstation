#include "ThreadedScraper.h"
#include "Window.h"
#include "FileData.h"
#include "components/AsyncNotificationComponent.h"
#include "LocaleES.h"
#include "guis/GuiMsgBox.h"

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
	mCurrentAction = "";
	mLastSearch = params;
	mSearchHandle = startScraperSearch(params);

	std::string idx = std::to_string(mTotal + 1- mSearchQueue.size()) + "/" + std::to_string(mTotal);

	mWndNotification->updateTitle(GUIICON + _("SCRAPING") + "... " + idx);
	mWndNotification->updateText(formatGameName(params.game), _("Searching")+"...");
	mWndNotification->updatePercent(-1);
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
			{
				if (httpCode == 426) // Blacklist
				{
					mExit = true;
					mWindow->postToUiThread([](Window* w)
					{
						w->pushGui(new GuiMsgBox(w, _("SCRAPE FAILED : THE APPLICATION HAS BEEN BLACKLISTED")));
					});
					break;
				}
				else if (httpCode == 400) // Too many scraps
				{
					mExit = true;
					mWindow->postToUiThread([](Window* w)
					{
						w->pushGui(new GuiMsgBox(w, _("SCRAPE FAILED : SCRAP LIMIT REACHED TODAY")));
					});
					break;
				}
				else
					mErrors.push_back(statusString);
			}
		}

		if (mMDResolveHandle && mMDResolveHandle->status() != ASYNC_IN_PROGRESS)
		{
			auto status = mMDResolveHandle->status();
			auto result = mMDResolveHandle->getResult();
			auto statusString = mMDResolveHandle->getStatusString();

			mCurrentAction = "";
			mMDResolveHandle.reset();

			if (status == ASYNC_DONE)
				acceptResult(result);
			else if (status == ASYNC_ERROR)
				mErrors.push_back(statusString);
		}

		if (mMDResolveHandle && mMDResolveHandle->status() == ASYNC_IN_PROGRESS)
		{
			std::string action = mMDResolveHandle->getCurrentItem();
			if (action != mCurrentAction)
			{
				mCurrentAction = action;
				mWndNotification->updateText(formatGameName(mLastSearch.game), "Downloading "+ mCurrentAction);
			}

			mWndNotification->updatePercent(mMDResolveHandle->getPercent());
		}

		if (mSearchHandle == nullptr && mMDResolveHandle == nullptr)
		{
			mSearchQueue.pop();

			if (!mSearchQueue.empty())
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
	ScraperSearchParams& search = mSearchQueue.front();

	if (result.hadMedia())
		mMDResolveHandle = resolveMetaDataAssets(result, mLastSearch);

	search.game->getMetadata().importScrappedMetadata(result.mdl);
}

void ThreadedScraper::acceptResult(const ScraperSearchResult& result)
{
	ScraperSearchParams& search = mSearchQueue.front();
	search.game->getMetadata().importScrappedMetadata(result.mdl);// = result.mdl;
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

