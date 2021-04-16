#pragma once

#ifndef ES_APP_SCRAPERS_SCREEN_SCRAPER_H
#define ES_APP_SCRAPERS_SCREEN_SCRAPER_H

#include "scrapers/Scraper.h"
#include "EmulationStation.h"

#if defined(SCREENSCRAPER_DEV_LOGIN)

#define VERSIONED_SOFT_NAME std::string(SCREENSCRAPER_SOFTNAME) + " " + static_cast<std::string>(PROGRAM_VERSION_STRING)

namespace pugi { class xml_document; }

class ScreenScraperScraper : public Scraper
{
public:
	void generateRequests(const ScraperSearchParams& params,
		std::queue<std::unique_ptr<ScraperRequest>>& requests,
		std::vector<ScraperSearchResult>& results) override;

	bool isSupportedPlatform(SystemData* system) override;
	bool hasMissingMedia(FileData* file) override;
	int getThreadCount(std::string &result) override;
};

struct ScreenScraperUser
{
	ScreenScraperUser()
	{
		maxthreads = 0;
		requestsToday = 0;
		requestsKoToday = 0;
		maxRequestsPerMin = 0;
		maxRequestsPerDay = 0;
		maxRequestsKoPerDay = 0;
	}

	std::string id;
	int maxthreads;

	int requestsToday;
	int requestsKoToday;

	int maxRequestsPerMin;
	int maxRequestsPerDay;
	int maxRequestsKoPerDay;
};

class ScreenScraperRequest : public ScraperHttpRequest
{
public:
	// ctor for a GetGameList request
	ScreenScraperRequest(
		std::queue< std::unique_ptr<ScraperRequest> >& requestsWrite, 
		std::vector<ScraperSearchResult>& resultsWrite, 
		const std::string& url, 
		const std::string& fileName) 
		: ScraperHttpRequest(resultsWrite, url), mRequestQueue(&requestsWrite) 
	{
		mFileName = fileName;
	}

	// Settings for the scraper
	static const struct ScreenScraperConfig 
	{
		ScreenScraperConfig() {};

		std::string getGameSearchUrl(const std::string gameName, bool jeuRecherche=false) const;
		std::string getUserInfoUrl() const;

		// Access to the API
		const std::string API_URL_BASE = "https://www.screenscraper.fr/api2";
		std::string region = "US";

	} configuration;

	static ScreenScraperUser processUserInfo(const pugi::xml_document& xmldoc);

protected:
	bool process(HttpReq* request, std::vector<ScraperSearchResult>& results) override;
	std::string ensureUrl(const std::string url);
	
	void processGame(const pugi::xml_document& xmldoc, std::vector<ScraperSearchResult>& results);
	bool isGameRequest() { return !mRequestQueue; }

	std::queue< std::unique_ptr<ScraperRequest> >* mRequestQueue;

private:
	std::vector<std::string>	getRipList(std::string imageSource);
	pugi::xml_node				findMedia(pugi::xml_node media_list, std::vector<std::string> mediaNames, std::string region);
	pugi::xml_node				findMedia(pugi::xml_node media_list, std::string mediaName, std::string region);

	std::string mFileName;
};
#endif

#endif // ES_APP_SCRAPERS_SCREEN_SCRAPER_H
