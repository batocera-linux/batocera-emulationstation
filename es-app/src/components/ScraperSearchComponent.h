#pragma once
#ifndef ES_APP_COMPONENTS_SCRAPER_SEARCH_COMPONENT_H
#define ES_APP_COMPONENTS_SCRAPER_SEARCH_COMPONENT_H

#include "components/BusyComponent.h"
#include "components/ComponentGrid.h"
#include "scrapers/Scraper.h"
#include "GuiComponent.h"

class ComponentList;
class DateTimeEditComponent;
class WebImageComponent;
class RatingComponent;
class TextComponent;

class ScraperSearch
{
public:
	std::string name;
	std::unique_ptr<ScraperSearchHandle> searchHandle;

	ScraperSearchParams params;
	std::vector<ScraperSearchResult> results;
	std::string error;
};

class ScraperSearchComponent : public GuiComponent
{
public:
	ScraperSearchComponent(Window* window);
	~ScraperSearchComponent();

	void search(const ScraperSearchParams& params);
	void openInputScreen(ScraperSearchParams& from);
	void stop();

	// Metadata assets will be resolved before calling the accept callback (e.g. result.mdl's "image" is automatically downloaded and properly set).
	inline void setAcceptCallback(const std::function<void(const ScraperSearchResult&)>& acceptCallback) { mAcceptCallback = acceptCallback; }
	inline void setSkipCallback(const std::function<void()>& skipCallback) { mSkipCallback = skipCallback; };
	inline void setCancelCallback(const std::function<void()>& cancelCallback) { mCancelCallback = cancelCallback; }

	inline void setSearchStartingCallback(const std::function<void()>& searchStartingCallback) { mSearchStartingCallback = searchStartingCallback; }
	inline void setSearchDoneCallback(const std::function<void()>& searchDoneCallback) { mSearchDoneCallback = searchDoneCallback; }

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;
	std::vector<HelpPrompt> getHelpPrompts() override;
	void onSizeChanged() override;	
	void onFocusGained() override;
	void onFocusLost() override;

private:
	void updateBusyAnim();
	void updateViewStyle();
	void updateInfoPane();

	void resizeMetadata();

	void onSearchError(const std::string& error);
	void onSearchDone(bool isReallyFinished = true);

	int getSelectedIndex();

	// resolve any metadata assets that need to be downloaded and return
	void returnResult(ScraperSearchResult result);

	ComponentGrid mGrid;
	BusyComponent mBusyAnim;

	std::shared_ptr<TextComponent> mResultName;	
	std::shared_ptr<TextComponent> mResultDesc;
	std::shared_ptr<WebImageComponent> mResultThumbnail;
	std::shared_ptr<ComponentList> mResultList;

	std::shared_ptr<ComponentGrid> mMD_Grid;
	std::shared_ptr<RatingComponent> mMD_Rating;
	std::shared_ptr<DateTimeEditComponent> mMD_ReleaseDate;
	std::shared_ptr<TextComponent> mMD_Developer;
	std::shared_ptr<TextComponent> mMD_Publisher;
	std::shared_ptr<TextComponent> mMD_Genre;
	std::shared_ptr<TextComponent> mMD_Players;

	// label-component pair
	struct MetaDataPair
	{
		std::shared_ptr<TextComponent> first;
		std::shared_ptr<GuiComponent> second;
		bool resize;

		MetaDataPair(const std::shared_ptr<TextComponent>& f, const std::shared_ptr<GuiComponent>& s, bool r = true) : first(f), second(s), resize(r) {};
	};
	
	std::vector<MetaDataPair> mMD_Pairs;

	std::function<void(const ScraperSearchResult&)> mAcceptCallback;
	std::function<void()> mSkipCallback;
	std::function<void()> mCancelCallback;
	std::function<void()> mSearchDoneCallback;
	std::function<void()> mSearchStartingCallback;

	std::unique_ptr<MDResolveHandle> mMDResolveHandle;

	ScraperSearchParams mInitialSearch;
	std::vector<ScraperSearch*> mScrapEngines;

	int	 mInfoPaneCursor;
	bool mBlockAccept;

};

#endif // ES_APP_COMPONENTS_SCRAPER_SEARCH_COMPONENT_H
