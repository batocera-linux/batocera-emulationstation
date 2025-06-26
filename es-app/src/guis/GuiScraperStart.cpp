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
#include "views/gamelist/IGameListView.h"

GuiScraperStart::GuiScraperStart(Window* window)
	: GuiSettings(window, _("SCRAPER"), true)
{
	addTab(_("SCRAPE"));
	addTab(_("OPTIONS"));
	addTab(_("ACCOUNTS"));
	setOnTabIndexChanged([&] { loadActivePage(); });

	loadActivePage();

	onFinalize([] { Settings::getInstance()->saveFile(); });

	if (Renderer::ScreenSettings::fullScreenMenus())
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
	else
		mMenu.setPosition((mSize.x() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiScraperStart::loadActivePage()
{	
	save();
	clearSaveFuncs();

	mMenu.clear();
	mMenu.clearButtons();

	switch (this->getTabIndex())
	{
	case 0:
		loadScrapPage();
		break;
	case 1:
		loadSettingsPage();
		break;
	case 2:
		loadAccountsPage();
		break;
	}

	mMenu.addButton(_("BACK"), _("go back"), [this] { close(); });
}

void GuiScraperStart::loadScrapPage() 
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

	mMenu.clearButtons();
	mMenu.addButton(_("SCRAPE NOW"), _("START"), std::bind(&GuiScraperStart::pressedStart, this));

	scraper_list->setSelectedChangedCallback([this, scraperName, scraper_list](std::string value) { Settings::getInstance()->setString("Scraper", value); });
}

void GuiScraperStart::loadSettingsPage() 
{
	auto scrap = Scraper::getScraper();
	if (scrap == nullptr)
		return;

	addGroup(_("SETTINGS"));

	std::string scraper = Scraper::getScraperName(scrap);

	std::string imageSourceName = Settings::getInstance()->getString("ScrapperImageSrc");
	auto imageSource = std::make_shared< OptionListComponent<std::string> >(mWindow, _("IMAGE SOURCE"), false);

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Screenshot) ||
		scrap->isMediaSupported(Scraper::ScraperMediaSource::Box2d) ||
		scrap->isMediaSupported(Scraper::ScraperMediaSource::Box3d) ||
		scrap->isMediaSupported(Scraper::ScraperMediaSource::Mix) ||
		scrap->isMediaSupported(Scraper::ScraperMediaSource::FanArt) ||
		scrap->isMediaSupported(Scraper::ScraperMediaSource::TitleShot))
	{
		// Image source : <image> tag

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Screenshot))
			imageSource->add(_("SCREENSHOT"), "ss", imageSourceName == "ss");

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::TitleShot))
			imageSource->add(_("TITLE SCREENSHOT"), "sstitle", imageSourceName == "sstitle");

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Mix))
		{
			imageSource->add(_("MIX V1"), "mixrbv1", imageSourceName == "mixrbv1");
			imageSource->add(_("MIX V2"), "mixrbv2", imageSourceName == "mixrbv2");
		}

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Box2d))
			imageSource->add(_("BOX 2D"), "box-2D", imageSourceName == "box-2D");

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Box3d))
			imageSource->add(_("BOX 3D"), "box-3D", imageSourceName == "box-3D");

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::FanArt))
			imageSource->add(_("FAN ART"), "fanart", imageSourceName == "fanart");

		imageSource->add(_("NONE"), "", imageSourceName.empty());

		if (!imageSource->hasSelection())
			imageSource->selectFirstItem();

		addWithLabel(_("IMAGE SOURCE"), imageSource);
		addSaveFunc([imageSource] { Settings::getInstance()->setString("ScrapperImageSrc", imageSource->getSelected()); });
	}

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Box2d) || scrap->isMediaSupported(Scraper::ScraperMediaSource::Box3d))
	{
		// Box source : <thumbnail> tag
		std::string thumbSourceName = Settings::getInstance()->getString("ScrapperThumbSrc");
		auto thumbSource = std::make_shared< OptionListComponent<std::string> >(mWindow, _("BOX SOURCE"), false);
		thumbSource->add(_("NONE"), "", thumbSourceName.empty());

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Box2d))
			thumbSource->add(_("BOX 2D"), "box-2D", thumbSourceName == "box-2D");

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Box3d))//if (scraper == "HfsDB")
			thumbSource->add(_("BOX 3D"), "box-3D", thumbSourceName == "box-3D");

		if (!thumbSource->hasSelection())
			thumbSource->selectFirstItem();

		addWithLabel(_("BOX SOURCE"), thumbSource);
		addSaveFunc([thumbSource] { Settings::getInstance()->setString("ScrapperThumbSrc", thumbSource->getSelected()); });

		imageSource->setSelectedChangedCallback([this, scrap, thumbSource](std::string value)
			{
				if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Box2d))
				{
					if (value == "box-2D")
						thumbSource->remove(_("BOX 2D"));
					else
						thumbSource->add(_("BOX 2D"), "box-2D", false);
				}

				if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Box3d))
				{
					if (value == "box-3D")
						thumbSource->remove(_("BOX 3D"));
					else
						thumbSource->add(_("BOX 3D"), "box-3D", false);
				}
			});
	}

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Marquee) || scrap->isMediaSupported(Scraper::ScraperMediaSource::Wheel) || scrap->isMediaSupported(Scraper::ScraperMediaSource::WheelHD))
	{
		// Logo source : <marquee> tag
		std::string logoSourceName = Settings::getInstance()->getString("ScrapperLogoSrc");

		auto logoSource = std::make_shared< OptionListComponent<std::string> >(mWindow, _("LOGO SOURCE"), false);
		logoSource->add(_("NONE"), "", logoSourceName.empty());

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Wheel))
			logoSource->add(_("HD WHEEL"), "wheel-hd", logoSourceName == "wheel-hd");

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Wheel))
			logoSource->add(_("WHEEL"), "wheel", logoSourceName == "wheel");

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Marquee)) // if (scraper == "HfsDB")
			logoSource->add(_("MARQUEE"), "marquee", logoSourceName == "marquee");

		if (!logoSource->hasSelection())
			logoSource->selectFirstItem();

		addWithLabel(_("LOGO SOURCE"), logoSource);
		addSaveFunc([logoSource] { Settings::getInstance()->setString("ScrapperLogoSrc", logoSource->getSelected()); });
	}

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Region))
	{
		std::string region = Settings::getInstance()->getString("ScraperRegion");
		auto regionCtrl = std::make_shared< OptionListComponent<std::string> >(mWindow, _("PREFERED REGION"), false);
		regionCtrl->addRange({ { _("AUTO"), "" }, { "EUROPE", "eu" },  { "USA", "us" }, { "JAPAN", "jp" } , { "WORLD", "wor" } }, region);

		if (!regionCtrl->hasSelection())
			regionCtrl->selectFirstItem();

		addWithLabel(_("PREFERED REGION"), regionCtrl);
		addSaveFunc([regionCtrl] { Settings::getInstance()->setString("ScraperRegion", regionCtrl->getSelected()); });
	}

	addSwitch(_("OVERWRITE NAMES"), "ScrapeNames", true);
	addSwitch(_("OVERWRITE DESCRIPTIONS"), "ScrapeDescription", true);
	addSwitch(_("OVERWRITE MEDIAS"), "ScrapeOverWrite", true);

	addGroup(_("SCRAPE FOR"));

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::ShortTitle))
		addSwitch(_("SHORT NAME"), "ScrapeShortTitle", true);

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Ratings))
		addSwitch(_("COMMUNITY RATING"), "ScrapeRatings", true);

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Video))
		addSwitch(_("VIDEO"), "ScrapeVideos", true);

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::FanArt))
		addSwitch(_("FANART"), "ScrapeFanart", true);

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Bezel_16_9))
		addSwitch(_("BEZEL (16:9)"), "ScrapeBezel", true);

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::BoxBack))
		addSwitch(_("BOX BACKSIDE"), "ScrapeBoxBack", true);

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Map))
		addSwitch(_("MAP"), "ScrapeMap", true);

	/*
	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::TitleShot))
	addSwitch(_("SCRAPE TITLESHOT"), "ScrapeTitleShot", true);

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Cartridge))
	addSwitch(_("SCRAPE CARTRIDGE"), "ScrapeCartridge", true);
	*/

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Manual))
		addSwitch(_("MANUAL"), "ScrapeManual", true);

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::PadToKey))
		addSwitch(_("PADTOKEY SETTINGS"), "ScrapePadToKey", true);
}

void GuiScraperStart::loadAccountsPage() 
{
	addGroup(_("SCREENSCRAPER"));
	addInputTextRow(_("USERNAME"), "ScreenScraperUser", false, true);
	addInputTextRow(_("PASSWORD"), "ScreenScraperPass", true, true);

	addGroup(_("IGDB"));
	addInputTextRow(_("CLIENT ID"), "IGDBClientID", false, true);
	addInputTextRow(_("CLIENT SECRET"), "IGDBSecret", true, true);
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
