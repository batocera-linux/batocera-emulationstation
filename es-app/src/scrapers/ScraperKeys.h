#pragma once

// Runtime API keys for the web services.
//
// A key entered by the user in the scraper settings takes precedence over the
// key compiled into the binary from ApiKeys.h. The getters return an empty
// string when neither is configured.

#include <string>

namespace ScraperKeys
{
	// ScreenScraper developer credentials, as the URL parameters
	// "devid=<id>&devpassword=<password>".
	std::string screenScraperDevLogin();

	// TheGamesDB API key.
	std::string gamesDBAPIKey();

	// HFSDB credentials, as "<user>:<password>".
	std::string hfsDBLogin();

	// RetroAchievements web API parameters "z=<user>&y=<web API key>".
	std::string cheevosLogin();
}
