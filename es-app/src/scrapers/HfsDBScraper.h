#pragma once

#ifndef ES_APP_SCRAPERS_HFSDB_SCRAPER_H
#define ES_APP_SCRAPERS_HFSDB_SCRAPER_H

#ifdef HFS_DEV_LOGIN

#include "scrapers/Scraper.h"
#include "utils/TimeUtil.h"

namespace pugi
{
class xml_document;
}

class HfsDBScraper : public Scraper
{
public:
	void generateRequests(const ScraperSearchParams& params,
		std::queue<std::unique_ptr<ScraperRequest>>& requests,
		std::vector<ScraperSearchResult>& results) override;

	bool isSupportedPlatform(SystemData* system) override;

	const std::set<ScraperMediaSource>& getSupportedMedias() override;

private:
	std::string mToken;
	Utils::Time::DateTime mTokenDate;
};

class HfsDBRequest : public ScraperHttpRequest
{
  public:
	// ctor for a GetGameList request
	HfsDBRequest(std::queue<std::unique_ptr<ScraperRequest>>& requestsWrite, std::vector<ScraperSearchResult>& resultsWrite, const std::string& url, HttpReqOptions* options, bool isArcade)
		: ScraperHttpRequest(resultsWrite, url, options), mRequestQueue(&requestsWrite)
	{
		  mIsArcade = isArcade;
	}

	// ctor for a GetGame request
	HfsDBRequest(std::vector<ScraperSearchResult>& resultsWrite, const std::string& url, HttpReqOptions* options, bool isArcade)
		: ScraperHttpRequest(resultsWrite, url, options), mRequestQueue(nullptr)
	{		
		  mIsArcade = isArcade;
	}

  protected:
	bool process(HttpReq* request, std::vector<ScraperSearchResult>& results) override;
	bool isGameRequest() { return !mRequestQueue; }

	bool mIsArcade;
	std::queue<std::unique_ptr<ScraperRequest>>* mRequestQueue;
};

#endif

#endif // ES_APP_SCRAPERS_HFSDB_SCRAPER_H
