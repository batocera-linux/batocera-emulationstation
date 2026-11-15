#include "scrapers/ScraperKeys.h"

#include "ApiKeys.h"
#include "Settings.h"
#include "SystemConf.h"
#include "scrapers/ScraperKeyFormat.h"

std::string ScraperKeys::screenScraperDevLogin()
{
	auto settings = Settings::getInstance();

#ifdef SCREENSCRAPER_DEV_LOGIN
	const std::string compiled = SCREENSCRAPER_DEV_LOGIN;
#else
	const std::string compiled;
#endif

	return ScraperKeyFormat::screenScraperDevLogin(
		settings->getString("ScreenScraperDevID"),
		settings->getString("ScreenScraperDevPassword"),
		compiled);
}

std::string ScraperKeys::gamesDBAPIKey()
{
#ifdef GAMESDB_APIKEY
	const std::string compiled = GAMESDB_APIKEY;
#else
	const std::string compiled;
#endif

	return ScraperKeyFormat::gamesDBAPIKey(Settings::getInstance()->getString("GamesDBAPIKey"), compiled);
}

std::string ScraperKeys::hfsDBLogin()
{
	auto settings = Settings::getInstance();

#ifdef HFS_DEV_LOGIN
	const std::string compiled = HFS_DEV_LOGIN;
#else
	const std::string compiled;
#endif

	return ScraperKeyFormat::hfsDBLogin(
		settings->getString("HfsDBUser"),
		settings->getString("HfsDBPassword"),
		compiled);
}

std::string ScraperKeys::cheevosLogin()
{
#ifdef CHEEVOS_DEV_LOGIN
	const std::string compiled = CHEEVOS_DEV_LOGIN;
#else
	const std::string compiled;
#endif

	return ScraperKeyFormat::cheevosLogin(
		SystemConf::getInstance()->get("global.retroachievements.username"),
		Settings::getInstance()->getString("CheevosWebAPIKey"),
		compiled);
}
