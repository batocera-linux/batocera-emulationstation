#pragma once

#include <thread>
#include "Scraper.h"
#include "GuiComponent.h"
#include "Window.h"

class ComponentGrid;
class NinePatchComponent;
class TextComponent;

class GuiScraperProgress : public GuiComponent
{
public:
	GuiScraperProgress(Window* window);
	~GuiScraperProgress();

	void updateTitle(const std::string text);
	void updateText(const std::string text, const std::string action);
	void updatePercent(int percent);

	void render(const Transform4x4f& parentTrans) override;

private:
	std::shared_ptr<TextComponent> mTitle;
	std::shared_ptr<TextComponent> mGameName;
	std::shared_ptr<TextComponent> mAction;
	int mPercent;

	ComponentGrid* mGrid;
	NinePatchComponent* mFrame;
};

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
	GuiScraperProgress* mWndNotification;
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
	
	std::string formatGameName(FileData* game);

	int mTotal;
	bool mExit;

	static bool mPaused;
	static ThreadedScraper* mInstance;
};

