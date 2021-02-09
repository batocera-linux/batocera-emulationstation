#include "components/ScraperSearchComponent.h"

#include "components/ComponentList.h"
#include "components/DateTimeEditComponent.h"
#include "components/ImageComponent.h"
#include "components/WebImageComponent.h"
#include "components/RatingComponent.h"
#include "components/TextComponent.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "resources/Font.h"
#include "utils/StringUtil.h"
#include "FileData.h"
#include "Log.h"
#include "Window.h"
#include "LocaleES.h"

ScraperSearchComponent::ScraperSearchComponent(Window* window, SearchType type) : GuiComponent(window),
	mGrid(window, Vector2i(5, 5)), mBusyAnim(window),
	mSearchType(type)
{
	mInfoPaneCursor = -1;

	auto theme = ThemeData::getMenuTheme();
	auto font = theme->TextSmall.font; // this gets replaced in onSizeChanged() so its just a placeholder
	const unsigned int mdColor = theme->Text.color;
	const unsigned int mdLblColor = theme->TextSmall.color;

	mBusyAnim.setText(_("Searching"));

	addChild(&mGrid);

	mBlockAccept = false;

	// left spacer (empty component, needed for borders)
	mGrid.setEntry(std::make_shared<GuiComponent>(mWindow), Vector2i(0, 0), false, false, Vector2i(1, 6), GridFlags::BORDER_TOP | GridFlags::BORDER_BOTTOM);

	// selected result name
	mResultName = std::make_shared<TextComponent>(mWindow, "Result name", theme->Text.font, theme->Text.color);

	// selected result thumbnail
	mResultThumbnail = std::make_shared<WebImageComponent>(mWindow, 86400); // 24 hours
	mResultThumbnail->setAllowFading(false);	
	mResultThumbnail->setOnImageLoaded([this]() { mGrid.onSizeChanged(); });

	mGrid.setEntry(mResultThumbnail, Vector2i(1, 1), false, false);

	// selected result desc + container
	mResultDesc = std::make_shared<TextComponent>(mWindow, "Result desc", theme->TextSmall.font, theme->Text.color);	
	mResultDesc->setVerticalAlignment(Alignment::ALIGN_TOP);
	mResultDesc->setAutoScroll(TextComponent::AutoScrollType::VERTICAL);

	// metadata
	mMD_Rating = std::make_shared<RatingComponent>(mWindow);
	mMD_ReleaseDate = std::make_shared<DateTimeEditComponent>(mWindow); mMD_ReleaseDate->setColor(mdColor);
	mMD_Developer = std::make_shared<TextComponent>(mWindow, "", font, mdColor);
	mMD_Publisher = std::make_shared<TextComponent>(mWindow, "", font, mdColor);
	mMD_Genre = std::make_shared<TextComponent>(mWindow, "", font, mdColor);
	mMD_Players = std::make_shared<TextComponent>(mWindow, "", font, mdColor);

	mMD_Pairs.push_back(MetaDataPair(std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(_("Publisher") + " :"), font, mdLblColor), mMD_Publisher));
	mMD_Pairs.push_back(MetaDataPair(std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(_("Developer") + " :"), font, mdLblColor), mMD_Developer));
	mMD_Pairs.push_back(MetaDataPair(std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(_("Genre") + " :"), font, mdLblColor), mMD_Genre));
	mMD_Pairs.push_back(MetaDataPair(std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(_("Players") + " :"), font, mdLblColor), mMD_Players));
	mMD_Pairs.push_back(MetaDataPair(std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(_("Released") + " :"), font, mdLblColor), mMD_ReleaseDate));
	mMD_Pairs.push_back(MetaDataPair(std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(_("Rating") + " :"), font, mdLblColor), mMD_Rating, false));

	mMD_Grid = std::make_shared<ComponentGrid>(mWindow, Vector2i(2, (int)mMD_Pairs.size() * 2)); //  - 1

	unsigned int i = 0;
	for (auto it = mMD_Pairs.cbegin(); it != mMD_Pairs.cend(); it++)
	{
		mMD_Grid->setEntry(it->first, Vector2i(0, i), false, true);
		mMD_Grid->setEntry(it->second, Vector2i(1, i), false, it->resize);
		i += 2;
	}

	mGrid.setEntry(mMD_Grid, Vector2i(2, 1), false, false);

	// result list
	mResultList = std::make_shared<ComponentList>(mWindow);
	mResultList->setCursorChangedCallback([this](CursorState state) { if (state == CURSOR_STOPPED) updateInfoPane(); });

	updateViewStyle();
}

ScraperSearchComponent::~ScraperSearchComponent()
{
	for (auto scrapper : mScrapEngines)
		delete scrapper;
}

void ScraperSearchComponent::onSizeChanged()
{
	mGrid.setSize(mSize);

	if (mSize.x() == 0 || mSize.y() == 0)
		return;

	if (mSearchType == ALWAYS_ACCEPT_FIRST_RESULT)
	{
		mGrid.setColWidthPerc(0, 0.02f); // looks better when this is higher in auto mode
		mGrid.setColWidthPerc(1, 0.22f);
		mGrid.setColWidthPerc(2, 0.28f);	
		mGrid.setColWidthPerc(4, 0.02f);

		mGrid.setRowHeightPerc(0, (mResultName->getFont()->getHeight() * 1.6f) / mGrid.getSize().y()); // result name
		mGrid.setRowHeightPerc(2, 0.2f);
		mGrid.setRowHeightPerc(3, 0.001f);
		mGrid.setRowHeightPerc(4, 0.001f);		
	}
	else
	{
		mGrid.setColWidthPerc(0, 0.02f);
		mGrid.setColWidthPerc(1, 0.22f);
		mGrid.setColWidthPerc(2, 0.28f);
		mGrid.setColWidthPerc(3, 0.02f);
		

		mGrid.setRowHeightPerc(0, 0.05f); // hide name but do padding
		mGrid.setRowHeightPerc(1, 0.50f);
		mGrid.setRowHeightPerc(2, 0.05f);		
		// 3 is desc
		mGrid.setRowHeightPerc(4, 0.05f);
	}

	const float boxartCellScale = 0.9f;

	// limit thumbnail size using setMaxHeight - we do this instead of letting mGrid call setSize because it maintains the aspect ratio
	// we also pad a little so it doesn't rub up against the metadata labels
	mResultThumbnail->setMaxSize(mGrid.getColWidth(1) * boxartCellScale, mGrid.getRowHeight(1));

	// metadata
	resizeMetadata();

	mGrid.onSizeChanged();

	mBusyAnim.setSize(mSize);
}

void ScraperSearchComponent::resizeMetadata()
{
	mMD_Grid->setSize(mGrid.getColWidth(2), mGrid.getRowHeight(1));
	if (mMD_Grid->getSize().y() > mMD_Pairs.size())
	{
		auto font = ThemeData::getMenuTheme()->TextSmall.font;

		// update label fonts
		float maxLblWidth = 0;
		for (auto it = mMD_Pairs.cbegin(); it != mMD_Pairs.cend(); it++)
		{
			it->first->setFont(font);
			it->first->setSize(0, 0);
			if (it->first->getSize().x() > maxLblWidth)
				maxLblWidth = it->first->getSize().x(); // +6;
		}

		float rowHeight = (font->getLetterHeight() + 2) / mMD_Grid->getSize().y();

		for (unsigned int i = 0; i < mMD_Pairs.size(); i++)
		{
			//if (i > 0)
				// mMD_Grid->setRowHeightPerc((i-1) * 2 + 1, 0.1);

			mMD_Grid->setRowHeightPerc(i * 2, rowHeight);
		}

		// update component fonts
		mMD_ReleaseDate->setFont(font);
		mMD_Developer->setFont(font);
		mMD_Publisher->setFont(font);
		mMD_Genre->setFont(font);
		mMD_Players->setFont(font);

		mMD_Grid->setColWidthPerc(0, (maxLblWidth * 1.2f) / mMD_Grid->getSize().x());

		// rating is manually sized
		mMD_Rating->setSize(mMD_Grid->getColWidth(1), font->getHeight() * 0.65f);
		mMD_Grid->onSizeChanged();

		mResultDesc->setFont(font);
	}
}

void ScraperSearchComponent::updateViewStyle()
{
	// unlink description and result list and result name
	mGrid.removeEntry(mResultName);
	mGrid.removeEntry(mResultDesc);
	mGrid.removeEntry(mResultList);

	// add them back depending on search type
	if (mSearchType == ALWAYS_ACCEPT_FIRST_RESULT)
	{
		// show name
		mGrid.setEntry(mResultName, Vector2i(1, 0), false, true, Vector2i(4, 1), GridFlags::BORDER_TOP);

		// need a border on the bottom left
		mGrid.setEntry(std::make_shared<GuiComponent>(mWindow), Vector2i(0, 2), false, false, Vector2i(3, 2));

		// show description on the right
		mGrid.setEntry(mResultDesc, Vector2i(3, 1), false, true, Vector2i(1, 3));

		// fake row where name would be
		mGrid.setEntry(std::make_shared<GuiComponent>(mWindow), Vector2i(0, 4), false, true, Vector2i(5, 1), GridFlags::BORDER_BOTTOM);
	}
	else 
	{
		// fake row where name would be
		mGrid.setEntry(std::make_shared<GuiComponent>(mWindow), Vector2i(1, 0), false, true, Vector2i(3, 1), GridFlags::BORDER_TOP);

		// show result list on the right
		mGrid.setEntry(mResultList, Vector2i(4, 0), true, true, Vector2i(1, 5), GridFlags::BORDER_LEFT | GridFlags::BORDER_TOP | GridFlags::BORDER_BOTTOM);

		// fake row where name would be
		// mGrid.setEntry(std::make_shared<GuiComponent>(mWindow), Vector2i(0, 2), false, true, Vector2i(4, 1), GridFlags::BORDER_BOTTOM);

		// show description under image/info
		mGrid.setEntry(mResultDesc, Vector2i(1, 3), false, true, Vector2i(2, 1));

		// fake row where name would be
		mGrid.setEntry(std::make_shared<GuiComponent>(mWindow), Vector2i(0, 4), false, true, Vector2i(4, 1), GridFlags::BORDER_BOTTOM);

	}
}

void ScraperSearchComponent::search(const ScraperSearchParams& params)
{
	mInfoPaneCursor = -1;
	mInitialSearch = params;
	mBlockAccept = true;

	for (auto scrapper : mScrapEngines)
		delete scrapper;

	mScrapEngines.clear();

	mResultList->clear();
	mMDResolveHandle.reset();
	updateInfoPane();

	if (mSearchType == NEVER_AUTO_ACCEPT)
	{
		for (auto scraperName : Scraper::getScraperList())
		{
			auto scraper = Scraper::getScraper(scraperName);
			if (scraper == nullptr || !scraper->isSupportedPlatform(params.system))
				continue;

			ScraperSearch* ss = new ScraperSearch();
			ss->name = scraperName;
			ss->params = params;
			ss->searchHandle = scraper->search(params);
			mScrapEngines.push_back(ss);
		}
	}
	
	if (mScrapEngines.size() == 0)
	{
		ScraperSearch* ss = new ScraperSearch();
		ss->name = Settings::getInstance()->getString("Scraper");
		ss->params = params;
		ss->searchHandle = Scraper::getScraper(ss->name)->search(params);
		mScrapEngines.push_back(ss);
	}
}

void ScraperSearchComponent::stop()
{
	for (auto engine : mScrapEngines)
		engine->searchHandle.reset();

	mMDResolveHandle.reset();
	mBlockAccept = false;
}

void ScraperSearchComponent::onSearchDone()
{
	mResultList->clear();

	auto theme = ThemeData::getMenuTheme();
	auto font = theme->Text.font;
	unsigned int color = theme->Text.color;

	bool hasResults = false;

	for (auto engine : mScrapEngines)
		hasResults |= (engine->results.size() > 0);

	if (!hasResults)
	{
		// Check if the scraper used is still valid
		if (!Scraper::isValidConfiguredScraper())
		{
			mWindow->pushGui(new GuiMsgBox(mWindow, Utils::String::toUpper("Configured scraper is no longer available.\nPlease change the scraping source in the settings."),
				"FINISH", mSkipCallback));
		}
		else
		{
			ComponentListRow row;
			row.addElement(std::make_shared<TextComponent>(mWindow, _("NO GAMES FOUND - SKIP"), font, color), true); // batocera

			if (mSkipCallback)
				row.makeAcceptInputHandler(mSkipCallback);

			mResultList->addRow(row);
			mGrid.resetCursor();
		}
	}
	else
	{
		int i = 0;
		for (auto scraperName : Scraper::getScraperList())
		{
			auto pEngine = std::find_if(mScrapEngines.cbegin(), mScrapEngines.cend(), [scraperName](ScraperSearch* ss) { return ss->name == scraperName; });
			if (pEngine == mScrapEngines.cend())
				continue;

			auto engine = *pEngine;
			if (engine->results.empty())
				continue;

			if (mScrapEngines.size() > 0)
				mResultList->addGroup(Utils::String::toUpper(engine->name));

			ComponentListRow row;
			for (auto result : engine->results)
			{
				std::string icons;

				if (result.urls.find(MetaDataId::Image) != result.urls.cend())
					icons = _U(" \uF03E");

				if (result.urls.find(MetaDataId::Video) != result.urls.cend())
					icons += _U(" \uF03D");

				if (result.urls.find(MetaDataId::Marquee) != result.urls.cend())
					icons += _U(" \uF009");

				row.elements.clear();
				row.addElement(std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(result.mdl.get(MetaDataId::Name)) + " " + icons, font, color), true);
				row.makeAcceptInputHandler([this, result] { returnResult(result); });
				mResultList->addRow(row, false, true, std::to_string(i));

				i++;
			}
		}

		mGrid.resetCursor();
	}

	mBlockAccept = false;
	updateInfoPane();

	if (mSearchType == ALWAYS_ACCEPT_FIRST_RESULT)
	{
		if (!hasResults)
			mSkipCallback();
		else
		{
			for (auto engine : mScrapEngines)
			{
				for (auto result : engine->results)
				{
					returnResult(result);
					return;
				}
			}
		}
	}
	else if (mSearchType == ALWAYS_ACCEPT_MATCHING_CRC)
	{
		// TODO
	}
}

void ScraperSearchComponent::onSearchError(const std::string& error)
{
	LOG(LogInfo) << "ScraperSearchComponent search error: " << error;

	mWindow->pushGui(new GuiMsgBox(mWindow, _("AN ERROR OCCURED") + " :\n" + Utils::String::toUpper(error),
		_("RETRY"), std::bind(&ScraperSearchComponent::search, this, mInitialSearch),
		_("SKIP"), mSkipCallback, // batocera
		_("CANCEL"), mCancelCallback, ICON_ERROR)); // batocera
}

int ScraperSearchComponent::getSelectedIndex()
{
	if (!std::any_of(mScrapEngines.cbegin(), mScrapEngines.cend(), [](ScraperSearch* x) { return x->results.size() > 0; }))
		return -1;

	if (mGrid.getSelectedComponent() != mResultList)
		return -1;

	return Utils::String::toInteger(mResultList->getSelectedUserData());
}

void ScraperSearchComponent::updateInfoPane()
{
	if (mResultList && mResultList->getCursorIndex() == mInfoPaneCursor)
		return;

	mInfoPaneCursor = mResultList->getCursorIndex();

	int i = getSelectedIndex();

	if (mSearchType == ALWAYS_ACCEPT_FIRST_RESULT && std::any_of(mScrapEngines.cbegin(), mScrapEngines.cend(), [](ScraperSearch* x) { return x->results.size() > 0; }))
		i = 0;

	std::vector<std::pair<std::string, ScraperSearchResult>> allResults;

	for (auto engine : mScrapEngines)
		for (auto result : engine->results)
			allResults.push_back(std::pair<std::string, ScraperSearchResult>(engine->name, result));

	if (i != -1 && (int)allResults.size() > i)
	{
		ScraperSearchResult& res = allResults.at(i).second;
		mResultName->setText(res.mdl.get(MetaDataId::Name));
		mResultDesc->setText(res.mdl.get(MetaDataId::Desc));

		mResultThumbnail->setImage("");

		if (mSearchType != ALWAYS_ACCEPT_FIRST_RESULT)
		{
			// Don't ask for thumbs in automatic mode to boost scraping -> mResultThumbnail is assigned after downloading first image 
			auto url = res.urls.find(MetaDataId::Thumbnail);
			if (url == res.urls.cend() || url->second.url.empty())
				url = res.urls.find(MetaDataId::Image);

			if (url != res.urls.cend() && !url->second.url.empty())
			{
				if (allResults.at(i).first == "ScreenScraper")
					mResultThumbnail->setImage(url->second.url + "&maxheight=250");
				else
					mResultThumbnail->setImage(url->second.url);
			}
			else
				mResultThumbnail->setImage("");
		}

		// metadata
		mMD_Rating->setVisible(res.mdl.getFloat(MetaDataId::Rating) >= 0);
		mMD_Rating->setValue(Utils::String::toUpper(res.mdl.get(MetaDataId::Rating)));

		mMD_ReleaseDate->setValue(Utils::String::toUpper(res.mdl.get(MetaDataId::ReleaseDate)));
		mMD_Developer->setText(res.mdl.get(MetaDataId::Developer));
		mMD_Publisher->setText(res.mdl.get(MetaDataId::Publisher));
		mMD_Genre->setText(res.mdl.get(MetaDataId::Genre));
		mMD_Players->setText(res.mdl.get(MetaDataId::Players));
		mGrid.onSizeChanged();
	}
	else 
	{
		mResultName->setText("");
		mResultDesc->setText("");
		mResultThumbnail->setImage("");

		// metadata
		mMD_Rating->setValue("");
		mMD_ReleaseDate->setValue("");
		mMD_Developer->setText("");
		mMD_Publisher->setText("");
		mMD_Genre->setText("");
		mMD_Players->setText("");
	}
}

bool ScraperSearchComponent::input(InputConfig* config, Input input)
{
	if (config->isMappedTo(BUTTON_OK, input) && input.value != 0)
	{
		if (mBlockAccept)
			return true;
	}

	return GuiComponent::input(config, input);
}

void ScraperSearchComponent::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();

	renderChildren(trans);

	if (mBlockAccept)
	{
		Renderer::setMatrix(trans);
		Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), 0x00000011, 0x00000011);

		mBusyAnim.render(trans);
	}
}

void ScraperSearchComponent::returnResult(ScraperSearchResult result)
{
	mBlockAccept = true;

	// resolve metadata image before returning
	if (result.hasMedia())
	{
		mMDResolveHandle = result.resolveMetaDataAssets(mInitialSearch);
		return;
	}

	mAcceptCallback(result);
}

void ScraperSearchComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mBlockAccept)
	{
		if (mMDResolveHandle && mMDResolveHandle->status() == ASYNC_IN_PROGRESS)
		{
			if (mSearchType == ALWAYS_ACCEPT_FIRST_RESULT && !mResultThumbnail->hasImage())
			{
				ScraperSearchResult result = mMDResolveHandle->getResult();

				if (!result.mdl.get(MetaDataId::Thumbnail).empty())
					mResultThumbnail->setImage(result.mdl.get(MetaDataId::Thumbnail));
				else if (!result.mdl.get(MetaDataId::Image).empty())
					mResultThumbnail->setImage(result.mdl.get(MetaDataId::Image));
			}

			mBusyAnim.setText(_("Downloading") + " " + Utils::String::toUpper(mMDResolveHandle->getCurrentItem()));
		}
		else
		{
			for (auto engine : mScrapEngines)
			{
				if (engine->searchHandle && engine->searchHandle->status() == ASYNC_IN_PROGRESS)
				{
					mBusyAnim.setText(_("Searching"));
					break;
				}
			}
		}

		mBusyAnim.update(deltaTime);
	}

	bool checkDone = false;

	for (auto engine : mScrapEngines)
	{
		if (engine->searchHandle == nullptr || engine->searchHandle->status() == ASYNC_IN_PROGRESS)
			continue;
		
		auto status = engine->searchHandle->status();
		auto results = engine->searchHandle->getResults();
		auto statusString = engine->searchHandle->getStatusString();
		
		if (status == ASYNC_DONE && results.size() == 0 && mSearchType == NEVER_AUTO_ACCEPT && engine->params.nameOverride.empty())
		{
			// ScreenScraper in UI mode -> jeuInfo has no result, try with jeuRecherche
			engine->params.nameOverride = engine->params.game->getName();
			engine->searchHandle = Scraper::getScraper(engine->name)->search(engine->params);
		}
		else
		{
			// we reset here because onSearchDone in auto mode can call mSkipCallback() which can call 
			// another search() which will set our mSearchHandle to something important
			engine->searchHandle.reset();

			if (status == ASYNC_DONE)
			{
				engine->results = results;
				checkDone = true;
			}
			else if (status == ASYNC_ERROR)
			{
				if (mScrapEngines.size() > 1)
					checkDone = true;
				else
					onSearchError(statusString);
			}
		}
	}

	if (checkDone && !std::any_of(mScrapEngines.cbegin(), mScrapEngines.cend(), [](ScraperSearch* x) { return x->searchHandle != nullptr; }))
		onSearchDone();

	if (mMDResolveHandle && mMDResolveHandle->status() != ASYNC_IN_PROGRESS)
	{
		if (mMDResolveHandle->status() == ASYNC_DONE)
		{
			ScraperSearchResult result = mMDResolveHandle->getResult();
			mMDResolveHandle.reset();

			// this might end in us being deleted, depending on mAcceptCallback - so make sure this is the last thing we do in update()
			returnResult(result);
		}
		else if (mMDResolveHandle->status() == ASYNC_ERROR)
		{
			onSearchError(mMDResolveHandle->getStatusString());
			mMDResolveHandle.reset();
		}
	}
}

void ScraperSearchComponent::openInputScreen(ScraperSearchParams& params)
{
	auto searchForFunc = [&](const std::string& name)
	{
		params.nameOverride = name;
		search(params);
	};

	stop();

	// batocera
	if (Settings::getInstance()->getBool("UseOSK"))
	{
		mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, "SEARCH FOR",
			// initial value is last search if there was one, otherwise the clean path name
			params.nameOverride.empty() ? params.game->getCleanName() : params.nameOverride,
			searchForFunc, false, "SEARCH"));
	}
	else
	{
		mWindow->pushGui(new GuiTextEditPopup(mWindow, "SEARCH FOR",
			// initial value is last search if there was one, otherwise the clean path name
			params.nameOverride.empty() ? params.game->getCleanName() : params.nameOverride,
			searchForFunc, false, "SEARCH"));
	}
}

std::vector<HelpPrompt> ScraperSearchComponent::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mGrid.getHelpPrompts();
	if (getSelectedIndex() != -1)
		prompts.push_back(HelpPrompt(BUTTON_OK, _("ACCEPT RESULT")));

	return prompts;
}

void ScraperSearchComponent::onFocusGained()
{
	mGrid.onFocusGained();
}

void ScraperSearchComponent::onFocusLost()
{
	mGrid.onFocusLost();
}
