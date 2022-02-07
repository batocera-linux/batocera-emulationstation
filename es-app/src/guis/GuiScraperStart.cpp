#include "guis/GuiScraperStart.h"
#include "components/OptionListComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiMsgBox.h"
#include "views/ViewController.h"
#include "FileData.h"
#include "SystemData.h"
#include "scrapers/ThreadedScraper.h"
#include "LocaleES.h"
#include "GuiLoading.h"
#include "GuiScraperSettings.h"
#include "views/gamelist/IGameListView.h"

GuiScraperStart::GuiScraperStart(Window* window)
	: GuiSettings(window, _("SCRAPER"))
{
	mOverwriteMedias = true;

	std::string scraperName = Settings::getInstance()->getString("Scraper");

	// scrape from
	auto scraper_list = std::make_shared< OptionListComponent< std::string > >(mWindow, _("SCRAPING DATABASE"), false);

	// Select either the first entry of the one read from the settings, just in case the scraper from settings has vanished.
	for (auto engine : Scraper::getScraperList())
		scraper_list->add(engine, engine, engine == scraperName);

	addGroup(_("SOURCE"));
	addWithLabel(_("SCRAPE FROM"), scraper_list);

	if (!scraper_list->hasSelection())
	{
		scraper_list->selectFirstItem();
		scraperName = scraper_list->getSelected();
	}

	addEntry(_("SCRAPER SETTINGS"), true, std::bind(&GuiScraperStart::onShowScraperSettings, this));

	addGroup(_("FILTERS"));

	auto scraper = Scraper::getScraper(scraperName);

	// Media Filter
	mFilters = std::make_shared< OptionListComponent<FilterFunc> >(mWindow, _("GAMES TO SCRAPE FOR"), false);
	mFilters->add(_("ALL"), [](FileData*) -> bool { return true; }, false);
	mFilters->add(_("GAMES MISSING ANY MEDIA"), [this, scraper](FileData* g) -> bool { mOverwriteMedias = false; return scraper->hasMissingMedia(g); }, true);
	mFilters->add(_("GAMES MISSING ALL MEDIA"), [this, scraper](FileData* g) -> bool { mOverwriteMedias = true; return !scraper->hasAnyMedia(g); }, false);
	addWithLabel(_("GAMES TO SCRAPE FOR"), mFilters);

	// Date Filter
	auto now = Utils::Time::now();
	auto isOlderThan = [now](const std::string& scraper, FileData* f, int days)
	{
		auto date = f->getMetadata().getScrapeDate(scraper);
		if (date == nullptr || !date->isValid())
			return true;

		return date->getTime() <= (now - (days * 86400));
	};

//	int idx = Settings::RecentlyScrappedFilter();
//	if (idx < 0 || idx > 6)		
	int idx = 3;

	mDateFilters = std::make_shared< OptionListComponent<FilterFunc> >(mWindow, _("IGNORE RECENTLY SCRAPED GAMES"), false);
	mDateFilters->add(_("NO"), [](FileData*) -> bool { return true; }, idx == 0);
	mDateFilters->add(_("LAST DAY"), [this, scraperName, isOlderThan](FileData* g) -> bool { return isOlderThan(scraperName, g, 1); }, idx == 1);
	mDateFilters->add(_("LAST WEEK"), [this, scraperName, isOlderThan](FileData* g) -> bool { return isOlderThan(scraperName, g, 7); }, idx == 2);
	mDateFilters->add(_("LAST 15 DAYS"), [this, scraperName, isOlderThan](FileData* g) -> bool { return isOlderThan(scraperName, g, 15); }, idx == 3);
	mDateFilters->add(_("LAST MONTH"), [this, scraperName, isOlderThan](FileData* g) -> bool { return isOlderThan(scraperName, g, 31); }, idx == 4);
	mDateFilters->add(_("LAST 3 MONTHS"), [this, scraperName, isOlderThan](FileData* g) -> bool { return isOlderThan(scraperName, g, 90); }, idx == 5);
	mDateFilters->add(_("LAST YEAR"), [this, scraperName, isOlderThan](FileData* g) -> bool { return isOlderThan(scraperName, g, 365); }, idx == 6);
	addWithLabel(_("IGNORE RECENTLY SCRAPED GAMES"), mDateFilters);

	// System Filter
	std::string currentSystem;

	if (ViewController::get()->getState().viewing == ViewController::GAME_LIST)
	{
		auto uiSystem = ViewController::get()->getState().getSystem();
		if (uiSystem != nullptr)
		{
			if (uiSystem->isGroupSystem())
			{
				auto gl = ViewController::get()->getGameListView(uiSystem, false);
				if (gl != nullptr && gl->getCursor() != nullptr)
					uiSystem = gl->getCursor()->getSourceFileData()->getSystem();
			}

			if (uiSystem != nullptr)
				currentSystem = uiSystem->getName();
		}
	}

	mSystems = std::make_shared<OptionListComponent<SystemData*>>(mWindow, _("SYSTEMS INCLUDED"), true);

	for (auto system : SystemData::sSystemVector)
		if (system->isGameSystem() && !system->isCollection() && !system->hasPlatformId(PlatformIds::PLATFORM_IGNORE) && scraper->isSupportedPlatform(system))
			mSystems->add(system->getFullName(), system, currentSystem.empty() ? !system->getPlatformIds().empty() : system->getName() == currentSystem && !system->getPlatformIds().empty());

	addWithLabel(_("SYSTEMS INCLUDED"), mSystems);

	// mApproveResults = std::make_shared<SwitchComponent>(mWindow);
	// mApproveResults->setState(false);
	// addWithLabel(_("USER DECIDES ON CONFLICTS"), mApproveResults);

	mMenu.clearButtons();
	mMenu.addButton(_("SCRAPE NOW"), _("START"), std::bind(&GuiScraperStart::pressedStart, this));
	mMenu.addButton(_("BACK"), _("go back"), [this] { close(); });
	/*
	addSaveFunc([this, scraper_list] 
	{ 	
		int filterIndex = mDateFilters->getSelectedIndex();
		if (filterIndex >= 0)
			Settings::setRecentlyScrappedFilter(filterIndex);
		
		Settings::getInstance()->setString("Scraper", scraper_list->getSelected()); 
	});
	*/

	scraper_list->setSelectedChangedCallback([this, scraperName, scraper_list](std::string value)
	{
		Settings::getInstance()->setString("Scraper", value);
	});

	if (Renderer::isSmallScreen())
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
	else
		mMenu.setPosition((mSize.x() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiScraperStart::onShowScraperSettings()
{
	mWindow->pushGui(new GuiScraperSettings(mWindow));
}

void GuiScraperStart::pressedStart()
{
	std::vector<SystemData*> systems = mSystems->getSelectedObjects();
	for(auto system : systems)
	{
		if (system->getPlatformIds().empty())
		{
			mWindow->pushGui(new GuiMsgBox(mWindow, 
				_("WARNING: SOME OF YOUR SELECTED SYSTEMS DO NOT HAVE A PLATFORM SET. RESULTS MAY BE FOR DIFFERENT SYSTEMS!\nCONTINUE ANYWAY?"),  
				_("YES"), std::bind(&GuiScraperStart::start, this),  
				_("NO"), nullptr)); 

			return;
		}
	}

	start();
}

void GuiScraperStart::start()
{
	if (ThreadedScraper::isRunning())
	{
		Window* window = mWindow;

		mWindow->pushGui(new GuiMsgBox(mWindow,
			_("SCRAPING IS RUNNING. DO YOU WANT TO STOP IT?"),
			_("YES"), [this, window] { ThreadedScraper::stop(); },
			_("NO"), nullptr));

		return;
	}

	mWindow->pushGui(new GuiLoading<std::queue<ScraperSearchParams>>(mWindow, _("PLEASE WAIT"),
		[this](IGuiLoadingHandler* gui)
		{
			return getSearches(mSystems->getSelectedObjects(), mFilters->getSelected(), mDateFilters->getSelected(), gui);
		},
		[this](std::queue<ScraperSearchParams> searches)
		{
			if (searches.empty())
				mWindow->pushGui(new GuiMsgBox(mWindow, _("NO GAMES FIT THAT CRITERIA.")));
			else
			{
				ThreadedScraper::start(mWindow, searches);
				close();
			}
		}));
}

std::queue<ScraperSearchParams> GuiScraperStart::getSearches(std::vector<SystemData*> systems, FilterFunc mediaSelector, FilterFunc dateSelector, IGuiLoadingHandler* handler)
{
	std::queue<ScraperSearchParams> queue;

	int pos = 1;
	for (auto system : systems)
	{		
		if (systems.size() > 0 && handler != nullptr)
		{
			int percent = (pos * 100) / systems.size();
			handler->setText(_("PLEASE WAIT") + " " + std::to_string(percent) + "%");
		}

		auto games = system->getRootFolder()->getFilesRecursive(GAME);
		for(auto game : games)
		{
			if (dateSelector(game) && mediaSelector(game))
			{
				ScraperSearchParams search;
				search.game = game;
				search.system = system;
				search.overWriteMedias = mOverwriteMedias;

				queue.push(search);
			}
		}

		pos++;
	}

	return queue;
}

bool GuiScraperStart::input(InputConfig* config, Input input)
{
	bool consumed = GuiComponent::input(config, input);
	if (consumed)
		return true;

	if (input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		close();
		return true;
	}

	if (config->isMappedTo("start", input) && input.value != 0)
	{
		// close everything
		Window* window = mWindow;
		while (window->peekGui() && window->peekGui() != ViewController::get())
			delete window->peekGui();
	}

	return false;
}

std::vector<HelpPrompt> GuiScraperStart::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("start", _("CLOSE")));
	return prompts;
}
