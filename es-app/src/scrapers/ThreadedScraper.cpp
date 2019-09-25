#include "ThreadedScraper.h"
#include "Window.h"
#include "FileData.h"

#include "components/BusyComponent.h"
#include "LocaleES.h"

#define GUIICON _U("\uF03E ")

ThreadedScraper* ThreadedScraper::mInstance = nullptr;
bool ThreadedScraper::mPaused = false;

ThreadedScraper::ThreadedScraper(Window* window, const std::queue<ScraperSearchParams>& searches)
	: mSearchQueue(searches), mWindow(window)
{
	mExit = false;
	mTotal = (int) mSearchQueue.size();

	mWndNotification = new GuiScraperProgress(window);

	mWindow->registerChild(mWndNotification);
	search(mSearchQueue.front());
	mHandle = new std::thread(&ThreadedScraper::run, this);	
}

ThreadedScraper::~ThreadedScraper()
{
	mWindow->unRegisterChild(mWndNotification);
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
				mErrors.push_back(statusString);
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

	search.game->metadata.importScrappedMetadata(result.mdl);
}

void ThreadedScraper::acceptResult(const ScraperSearchResult& result)
{
	ScraperSearchParams& search = mSearchQueue.front();
	search.game->metadata = result.mdl;
}


#include "ThemeData.h"
#include "PowerSaver.h"
#include "components/ComponentGrid.h"
#include "components/NinePatchComponent.h"
#include "components/TextComponent.h"
#include "LocaleES.h"

#define PADDING_PX  (Renderer::getScreenWidth()*0.01)

GuiScraperProgress::GuiScraperProgress(Window* window) 
	: GuiComponent(window)
{
	mPercent = -1;

	float width = Renderer::getScreenWidth() * 0.14f;

	auto theme = ThemeData::getMenuTheme();

	mTitle = std::make_shared<TextComponent>(mWindow, GUIICON + _("SCRAPING"), theme->TextSmall.font, theme->TextSmall.color, ALIGN_LEFT);
	mGameName = std::make_shared<TextComponent>(mWindow, "name", theme->TextSmall.font, theme->Text.color, ALIGN_LEFT);
	mAction = std::make_shared<TextComponent>(mWindow, "action", theme->TextSmall.font, theme->Text.color, ALIGN_LEFT);

	Vector2f fullSize(width + 2 * PADDING_PX, 2 * PADDING_PX + mTitle->getSize().y() + mGameName->getSize().y() + mAction->getSize().y());
	Vector2f gridSize(width, mTitle->getSize().y() + mGameName->getSize().y() + mAction->getSize().y());

	setSize(fullSize);

	mFrame = new NinePatchComponent(window);
	mFrame->setImagePath(theme->Background.path);
	mFrame->setCenterColor(theme->Background.color);
	mFrame->setEdgeColor(theme->Background.color);
	mFrame->fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));
	addChild(mFrame);

	mGrid = new ComponentGrid(window, Vector2i(1, 3));
	mGrid->setPosition((fullSize.x() - gridSize.x()) / 2.0, (fullSize.y() - gridSize.y()) / 2.0);
	mGrid->setSize(gridSize);
	mGrid->setEntry(mTitle, Vector2i(0, 0), false, true);
	mGrid->setEntry(mGameName, Vector2i(0, 1), false, true);
	mGrid->setEntry(mAction, Vector2i(0, 2), false, true);
	addChild(mGrid);

	float posX = Renderer::getScreenWidth()*0.5f - mSize.x()*0.5f;
	float posY = Renderer::getScreenHeight() * 0.02f;

	// FCA TopRight
	posX = Renderer::getScreenWidth()*0.99f - mSize.x();
	posY = Renderer::getScreenHeight() * 0.02f;

	setPosition(posX, posY, 0);
	setOpacity(200);

	PowerSaver::pause();
}

GuiScraperProgress::~GuiScraperProgress()
{
	delete mFrame;
	delete mGrid;

	PowerSaver::resume();
}

void GuiScraperProgress::updateText(const std::string text, const std::string action)
{
	mGameName->setText(text);
	mAction->setText(action);
}

void GuiScraperProgress::updatePercent(int percent)
{
	mPercent = percent;
}

void GuiScraperProgress::updateTitle(const std::string text)
{
	mTitle->setText(text);
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




void GuiScraperProgress::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();

	mFrame->render(trans);
	
	float x = mGrid->getPosition().x() + mAction->getPosition().x();
	float y = mGrid->getPosition().y() + mAction->getPosition().y();

	float w = mAction->getSize().x();
	float h = mAction->getSize().y();

	h /= 10.0;
	y += mAction->getSize().y();

	Renderer::setMatrix(trans);
	//Renderer::drawRect(x, y, w, h, 0xA0A0A050);

	if (mPercent >= 0)
	{
		float percent = mPercent / 100.0;
		if (percent < 0)
			percent = 0;
		if (percent > 100)
			percent = 100;

		auto theme = ThemeData::getMenuTheme();
		auto color = (theme->Text.selectedColor & 0xFFFFFF00) | 0x40;
		Renderer::drawRect(x, y, (w*percent), h, color);
	}

	mGrid->render(trans);
}
