#pragma once
#ifndef ES_APP_SCRAPERS_ARCADE_DB_JSON_SCRAPER_H
#define ES_APP_SCRAPERS_ARCADE_DB_JSON_SCRAPER_H

#include "scrapers/Scraper.h"

namespace pugi
{
class xml_document;
}

class ArcadeDBScraper : public Scraper
{
public:
	void generateRequests(const ScraperSearchParams& params,
		std::queue<std::unique_ptr<ScraperRequest>>& requests,
		std::vector<ScraperSearchResult>& results) override;

	bool isSupportedPlatform(SystemData* system) override;
	bool hasMissingMedia(FileData* file) override;
};

class ArcadeDBJSONRequest : public ScraperHttpRequest
{
  public:
	// ctor for a GetGameList request
	ArcadeDBJSONRequest(std::queue<std::unique_ptr<ScraperRequest>>& requestsWrite,
		std::vector<ScraperSearchResult>& resultsWrite, const std::string& url)
		: ScraperHttpRequest(resultsWrite, url), mRequestQueue(&requestsWrite)
	{
	}
	// ctor for a GetGame request
	ArcadeDBJSONRequest(std::vector<ScraperSearchResult>& resultsWrite, const std::string& url)
		: ScraperHttpRequest(resultsWrite, url), mRequestQueue(nullptr)
	{
	}

  protected:
	bool process(HttpReq* request, std::vector<ScraperSearchResult>& results) override;
	bool isGameRequest() { return !mRequestQueue; }

	std::queue<std::unique_ptr<ScraperRequest>>* mRequestQueue;
};

#endif // ES_APP_SCRAPERS_ARCADE_DB_JSON_SCRAPER_H
