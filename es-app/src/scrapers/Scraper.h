#pragma once
#ifndef ES_APP_SCRAPERS_SCRAPER_H
#define ES_APP_SCRAPERS_SCRAPER_H

#include "AsyncHandle.h"
#include "HttpReq.h"
#include "MetaData.h"
#include <functional>
#include <memory>
#include <queue>
#include <utility>
#include <assert.h>
#include "FileData.h"

class FileData;
class SystemData;
class MDResolveHandle;

struct ScraperSearchParams
{
	ScraperSearchParams()
	{ 
		overWriteMedias = true; 
	}

	SystemData* system;
	FileData* game;

	bool overWriteMedias;
	std::string nameOverride;

	std::string getGameName()
	{
		return "[" + game->getSystemName() + "] " + game->getName();
	}
};

struct ScraperSearchItem
{
	ScraperSearchItem() {};
	ScraperSearchItem(const std::string url) { this->url = url; };
	ScraperSearchItem(const std::string url, const std::string format) { this->url = url; this->format = format; };

	std::string url;
	std::string format;
};

struct ScraperSearchResult
{
	ScraperSearchResult() : mdl(GAME_METADATA) {};

	MetaDataList mdl;
	std::map<MetaDataId, ScraperSearchItem> urls;
	std::string p2k;

	bool hasMedia()
	{
		for (auto url : urls)
			if (!url.second.url.empty())
				return true;

		return false;
	}

	std::unique_ptr<MDResolveHandle> resolveMetaDataAssets(const ScraperSearchParams& search);
};

class ScraperRequest : public AsyncHandle
{
public:
	ScraperRequest(std::vector<ScraperSearchResult>& resultsWrite) : mResults(resultsWrite) {};

	// returns "true" once we're done
	virtual void update() = 0;
	
protected:
	std::vector<ScraperSearchResult>& mResults;
};

// a single HTTP request that needs to be processed to get the results
class ScraperHttpRequest : public ScraperRequest
{
public:
	ScraperHttpRequest(std::vector<ScraperSearchResult>& resultsWrite, const std::string& url);
	~ScraperHttpRequest();

	virtual void update() override;

protected:
	virtual bool process(HttpReq* request, std::vector<ScraperSearchResult>& results) = 0;

private:
	HttpReq* mRequest;
	int	mRetryCount;

	int mOverQuotaPendingTime;
};

// a request to get a list of results
class ScraperSearchHandle : public AsyncHandle
{
	friend class Scraper;

public:
	ScraperSearchHandle();

	void update();
	inline const std::vector<ScraperSearchResult>& getResults() const { assert(mStatus != ASYNC_IN_PROGRESS); return mResults; }

protected:
	std::queue< std::unique_ptr<ScraperRequest> > mRequestQueue;
	std::vector<ScraperSearchResult> mResults;
};

typedef void (*generate_scraper_requests_func)(const ScraperSearchParams& params, std::queue< std::unique_ptr<ScraperRequest> >& requests, std::vector<ScraperSearchResult>& results);

// -------------------------------------------------------------------------

class ImageDownloadHandle : public AsyncHandle
{
public:
	ImageDownloadHandle(const std::string& url, const std::string& path, int maxWidth, int maxHeight);
	~ImageDownloadHandle();

	void update() override;

	virtual int getPercent();
	std::string getImageFileName() { return mSavePath; }

private:
	HttpReq* mRequest;
	int	mRetryCount;
	int mOverQuotaPendingTime;

	std::string mSavePath;
	int mMaxWidth;
	int mMaxHeight;
};


// Meta data asset downloading stuff.
class MDResolveHandle : public AsyncHandle
{
public:
	MDResolveHandle(const ScraperSearchResult& result, const ScraperSearchParams& search);

	void update() override;
	inline const ScraperSearchResult& getResult() const { return mResult; } //  assert(mStatus == ASYNC_DONE); -> FCA : Why ???

	std::string getCurrentItem() {
		return mCurrentItem;
	}

	std::string getCurrentSource() {
		return mSource;
	}

	int getPercent() {
		return mPercent;
	}

	std::unique_ptr<ImageDownloadHandle> downloadImageAsync(const std::string& url, const std::string& saveAs, bool resize = true);

private:
	ScraperSearchResult mResult;

	class ResolvePair
	{	
	public:
		ResolvePair(std::function<std::unique_ptr<ImageDownloadHandle>()> _invoker, std::function<void(ImageDownloadHandle* result)> _function, std::string _name, std::string _source)
		{
			func = _invoker;
			onFinished = _function;
			name = _name;
			source = _source;
		}

		void Run()
		{
			handle = func();
		}
	
		std::function<void(ImageDownloadHandle* result)> onFinished;
		std::string name;
		std::string source;

		std::unique_ptr<ImageDownloadHandle> handle;

	private:
		std::function<std::unique_ptr<ImageDownloadHandle>()> func;
	};

	std::vector<ResolvePair*> mFuncs;
	std::string mCurrentItem;
	std::string mSource;
	int mPercent;
};

class Scraper
{
public:
	static std::vector<std::pair<std::string, Scraper*>> scrapers;
	
	static Scraper* getScraper(const std::string name = "");
	static std::vector<std::string> getScraperList();
	static bool isValidConfiguredScraper();

	//About the same as "~/.emulationstation/downloaded_images/[system_name]/[game_name].[url's extension]".
	//Will create the "downloaded_images" and "subdirectory" directories if they do not exist.
	static std::string getSaveAsPath(FileData* game, const MetaDataId metadataId, const std::string& url);

	virtual	bool isSupportedPlatform(SystemData* system) = 0;

	virtual	bool hasMissingMedia(FileData* file);

	std::unique_ptr<ScraperSearchHandle> search(const ScraperSearchParams& params);

	virtual	int getThreadCount(std::string &result) {
		return 1;
	}

protected:
	virtual void generateRequests(const ScraperSearchParams& params,
		std::queue<std::unique_ptr<ScraperRequest>>& requests,
		std::vector<ScraperSearchResult>& results) = 0;
};

//You can pass 0 for maxWidth or maxHeight to automatically keep the aspect ratio.
//Will overwrite the image at [path] with the new resized one.
//Returns true if successful, false otherwise.
bool resizeImage(const std::string& path, int maxWidth, int maxHeight);

#endif // ES_APP_SCRAPERS_SCRAPER_H
