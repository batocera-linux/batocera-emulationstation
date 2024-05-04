#include "GuiScraperSettings.h"

#include "components/SwitchComponent.h"
#include "components/OptionListComponent.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiScraperStart.h"
#include "scrapers/ThreadedScraper.h"

GuiScraperSettings::GuiScraperSettings(Window* window) : GuiSettings(window, _("SCRAPER SETTINGS").c_str())
{
	auto scrap = Scraper::getScraper();
	if (scrap == nullptr)
		return;

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

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Marquee) || scrap->isMediaSupported(Scraper::ScraperMediaSource::Wheel))
	{
		// Logo source : <marquee> tag
		std::string logoSourceName = Settings::getInstance()->getString("ScrapperLogoSrc");

		auto logoSource = std::make_shared< OptionListComponent<std::string> >(mWindow, _("LOGO SOURCE"), false);
		logoSource->add(_("NONE"), "", logoSourceName.empty());

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

	// ScreenScraper Account		
	if (scraper == "ScreenScraper")
	{
		addGroup(_("ACCOUNT"));
		addInputTextRow(_("USERNAME"), "ScreenScraperUser", false, true);
		addInputTextRow(_("PASSWORD"), "ScreenScraperPass", true, true);
	}
}