#include "guis/GuiScraperStart.h"

#include "components/OptionListComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiScraperMulti.h"
#include "views/ViewController.h"
#include "FileData.h"
#include "SystemData.h"
#include "scrapers/ThreadedScraper.h"
#include "LocaleES.h"
#include "GuiLoading.h"

GuiScraperStart::GuiScraperStart(Window* window) : GuiComponent(window),
  mMenu(window, _("SCRAPE NOW").c_str()) // batocera
{
	mOverwriteMedias = true;

	addChild(&mMenu);

	// add filters (with first one selected)
	mFilters = std::make_shared< OptionListComponent<GameFilterFunc> >(mWindow, _("SCRAPE THESE GAMES"), false); // batocera
	mFilters->add(_("All Games"), // batocera
		[](SystemData*, FileData*) -> bool { return true; }, false);

	auto scraper = Settings::getInstance()->getString("Scraper");

	mFilters->add(_("Only missing medias"), [this, scraper](SystemData*, FileData* g) -> bool
	{ 
		mOverwriteMedias = false;

		if (scraper == "ScreenScraper")
		{
			if (!Settings::getInstance()->getString("ScrapperImageSrc").empty() && !Utils::FileSystem::exists(g->getMetadata(MetaDataId::Image)))
				return true;

			if (!Settings::getInstance()->getString("ScrapperThumbSrc").empty() && !Utils::FileSystem::exists(g->getMetadata(MetaDataId::Thumbnail)))
				return true;

			if (!Settings::getInstance()->getString("ScrapperLogoSrc").empty() && !Utils::FileSystem::exists(g->getMetadata(MetaDataId::Marquee)))
				return true;

			if (Settings::getInstance()->getBool("ScrapeVideos") && !Utils::FileSystem::exists(g->getMetadata(MetaDataId::Video)))
				return true;

			if (Settings::getInstance()->getBool("ScrapeFanart") && !Utils::FileSystem::exists(g->getMetadata(MetaDataId::FanArt)))
				return true;

			if (Settings::getInstance()->getBool("ScrapeTitleShot") && !Utils::FileSystem::exists(g->getMetadata(MetaDataId::TitleShot)))
				return true;

			if (Settings::getInstance()->getBool("ScrapeMap") && !Utils::FileSystem::exists(g->getMetadata(MetaDataId::Map)))
				return true;

			if (Settings::getInstance()->getBool("ScrapeManual") && !Utils::FileSystem::exists(g->getMetadata(MetaDataId::Manual)))
				return true;

			if (Settings::getInstance()->getBool("ScrapeCartridge") && !Utils::FileSystem::exists(g->getMetadata(MetaDataId::Cartridge)))
				return true;

			return false;
		}
		
		return !Utils::FileSystem::exists(g->getMetadata(MetaDataId::Image));
	}, true);

	mMenu.addWithLabel(_("FILTER"), mFilters); // batocera


	std::string currentSystem;

	if (ViewController::get()->getState().viewing == ViewController::GAME_LIST)
		currentSystem = ViewController::get()->getState().getSystem()->getName();


	//add systems (all with a platformidz specified selected)
	mSystems = std::make_shared< OptionListComponent<SystemData*> >(mWindow, _("SCRAPE THESE SYSTEMS"), true); // batocera
	for(auto it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); it++)
	{
		if ((*it)->isGroupSystem())
			continue;

		if(!(*it)->hasPlatformId(PlatformIds::PLATFORM_IGNORE))
			mSystems->add((*it)->getFullName(), *it, 
				currentSystem.empty() ? 
				!(*it)->getPlatformIds().empty() : 
				(*it)->getName() == currentSystem && !(*it)->getPlatformIds().empty());
	}

	mMenu.addWithLabel(_("SYSTEMS"), mSystems); // batocera

	mApproveResults = std::make_shared<SwitchComponent>(mWindow);
	mApproveResults->setState(false);
	mMenu.addWithLabel(_("USER DECIDES ON CONFLICTS"), mApproveResults);

	mMenu.addButton(_("START"), _("START"), std::bind(&GuiScraperStart::pressedStart, this));
	mMenu.addButton(_("BACK"), _("BACK"), [&] { delete this; });

	if (Renderer::isSmallScreen())
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
	else
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiScraperStart::pressedStart()
{
	std::vector<SystemData*> sys = mSystems->getSelectedObjects();
	for(auto it = sys.cbegin(); it != sys.cend(); it++)
	{
		if((*it)->getPlatformIds().empty())
		{
			mWindow->pushGui(new GuiMsgBox(mWindow, 
				_("WARNING: SOME OF YOUR SELECTED SYSTEMS DO NOT HAVE A PLATFORM SET. RESULTS MAY BE EVEN MORE INACCURATE THAN USUAL!\nCONTINUE ANYWAY?"),  // batocera
						       _("YES"), std::bind(&GuiScraperStart::start, this),  // batocera
						       _("NO"), nullptr)); // batocera
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

		mWindow->pushGui(new GuiMsgBox(mWindow, _("SCRAPING IS RUNNING. DO YOU WANT TO STOP IT ?"), _("YES"), [this, window]
		{
			ThreadedScraper::stop();
		}, _("NO"), nullptr));

		return;
	}

	mWindow->pushGui(new GuiLoading<std::queue<ScraperSearchParams>>(mWindow, _("PLEASE WAIT"),
		[this]
		{
			return getSearches(mSystems->getSelectedObjects(), mFilters->getSelected());
		},
		[this](std::queue<ScraperSearchParams> searches)
		{
			if (searches.empty())
				mWindow->pushGui(new GuiMsgBox(mWindow, _("NO GAMES FIT THAT CRITERIA."))); // batocera
			else
			{			
				if (mApproveResults->getState())
					mWindow->pushGui(new GuiScraperMulti(mWindow, searches, mApproveResults->getState()));
				else
					ThreadedScraper::start(mWindow, searches);

				delete this;
			}
		}));	
}

std::queue<ScraperSearchParams> GuiScraperStart::getSearches(std::vector<SystemData*> systems, GameFilterFunc selector)
{
	std::queue<ScraperSearchParams> queue;
	for(auto sys = systems.cbegin(); sys != systems.cend(); sys++)
	{
		std::vector<FileData*> games = (*sys)->getRootFolder()->getFilesRecursive(GAME);
		for(auto game = games.cbegin(); game != games.cend(); game++)
		{
			if(selector((*sys), (*game)))
			{
				ScraperSearchParams search;
				search.game = *game;
				search.system = *sys;
				search.overWriteMedias = mOverwriteMedias;

				queue.push(search);
			}
		}
	}

	return queue;
}

bool GuiScraperStart::input(InputConfig* config, Input input)
{
	bool consumed = GuiComponent::input(config, input);
	if(consumed)
		return true;
	
	if(input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		delete this;
		return true;
	}

	if(config->isMappedTo("start", input) && input.value != 0)
	{
		// close everything
		Window* window = mWindow;
		while(window->peekGui() && window->peekGui() != ViewController::get())
			delete window->peekGui();
	}


	return false;
}

std::vector<HelpPrompt> GuiScraperStart::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("start", _("CLOSE"))); // batocera
	return prompts;
}
