#include "scrapers/Scraper.h"

#include "FileData.h"
#include "GamesDBJSONScraper.h"
#include "ScreenScraper.h"
#include "Log.h"
#include "Settings.h"
#include "SystemData.h"
#include <FreeImage.h>
#include <fstream>
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <thread>

// batocera
const std::map<std::string, generate_scraper_requests_func> scraper_request_funcs {
	{ "ScreenScraper", &screenscraper_generate_scraper_requests },
	{ "TheGamesDB", &thegamesdb_generate_json_scraper_requests }
};

std::unique_ptr<ScraperSearchHandle> startScraperSearch(const ScraperSearchParams& params)
{
	const std::string& name = Settings::getInstance()->getString("Scraper");
	std::unique_ptr<ScraperSearchHandle> handle(new ScraperSearchHandle());

	// Check if the Scraper in the settings still exists as a registered scraping source.
	if (scraper_request_funcs.find(name) == scraper_request_funcs.end())
	{
		LOG(LogWarning) << "Configured scraper (" << name << ") unavailable, scraping aborted.";
	}
	else
	{
		scraper_request_funcs.at(name)(params, handle->mRequestQueue, handle->mResults);
	}

	return handle;
}

std::vector<std::string> getScraperList()
{
	std::vector<std::string> list;
	for(auto it = scraper_request_funcs.cbegin(); it != scraper_request_funcs.cend(); it++)
	{
		list.push_back(it->first);
	}

	return list;
}

bool isValidConfiguredScraper()
{
	const std::string& name = Settings::getInstance()->getString("Scraper");
	return scraper_request_funcs.find(name) != scraper_request_funcs.end();
}

// ScraperSearchHandle
ScraperSearchHandle::ScraperSearchHandle()
{
	setStatus(ASYNC_IN_PROGRESS);
}

void ScraperSearchHandle::update()
{
	if(mStatus == ASYNC_DONE)
		return;

	if(!mRequestQueue.empty())
	{
		// a request can add more requests to the queue while running,
		// so be careful with references into the queue
		auto& req = *(mRequestQueue.front());
		AsyncHandleStatus status = req.status();

		if(status == ASYNC_ERROR)
		{
			// propegate error
			setError(req.getStatusString());

			// empty our queue
			while(!mRequestQueue.empty())
				mRequestQueue.pop();

			return;
		}

		// finished this one, see if we have any more
		if(status == ASYNC_DONE)
		{
			mRequestQueue.pop();
		}

		// status == ASYNC_IN_PROGRESS
	}

	// we finished without any errors!
	if(mRequestQueue.empty() && mStatus != ASYNC_ERROR)
	{
		setStatus(ASYNC_DONE);
		return;
	}
}



// ScraperRequest
ScraperRequest::ScraperRequest(std::vector<ScraperSearchResult>& resultsWrite) : mResults(resultsWrite)
{
}


// ScraperHttpRequest
ScraperHttpRequest::ScraperHttpRequest(std::vector<ScraperSearchResult>& resultsWrite, const std::string& url) 
	: ScraperRequest(resultsWrite)
{
	setStatus(ASYNC_IN_PROGRESS);
	mReq = std::unique_ptr<HttpReq>(new HttpReq(url));
	mRetryCount = 0;
}

void ScraperHttpRequest::update()
{
	HttpReq::Status status = mReq->status();
	if(status == HttpReq::REQ_SUCCESS)
	{
		setStatus(ASYNC_DONE); // if process() has an error, status will be changed to ASYNC_ERROR
		process(mReq, mResults);		
		return;
	}

	if (status == HttpReq::REQ_429_TOOMANYREQUESTS)
	{
		mRetryCount++;
		if (mRetryCount > 4)
			return;

		setStatus(ASYNC_IN_PROGRESS);

		std::string url = mReq->getUrl();
		std::this_thread::sleep_for(std::chrono::seconds(15));
		mReq = std::unique_ptr<HttpReq>(new HttpReq(url));		
		return;
	}

	if (status == HttpReq::REQ_404_NOTFOUND)
	{
		setStatus(ASYNC_DONE);
		return;
	}

	if (status == HttpReq::REQ_426_BLACKLISTED)
	{
		setError("THE SOFTWARE HAS BEEN BLACKLISTED (426)");
		return;
	}

	// not ready yet
	if(status == HttpReq::REQ_IN_PROGRESS)
		return;
	
	// everything else is some sort of error
	LOG(LogError) << "ScraperHttpRequest network error (status: " << status << ") - " << mReq->getErrorMsg();
	setError(mReq->getErrorMsg());
}


// metadata resolving stuff

std::unique_ptr<MDResolveHandle> resolveMetaDataAssets(const ScraperSearchResult& result, const ScraperSearchParams& search)
{
	return std::unique_ptr<MDResolveHandle>(new MDResolveHandle(result, search));
}

MDResolveHandle::MDResolveHandle(const ScraperSearchResult& result, const ScraperSearchParams& search) : mResult(result)
{
	mPercent = -1;

	std::string ext;

	// If we have a file extension returned by the scraper, then use it.
	// Otherwise, try to guess it by the name of the URL, which point to an image.
	if (!result.imageType.empty())
	{
		ext = result.imageType;
	}
	else 
	{
		size_t dot = result.imageUrl.find_last_of('.');

		if (dot != std::string::npos)
			ext = result.imageUrl.substr(dot, std::string::npos);
	}

	bool ss = Settings::getInstance()->getString("Scraper") == "ScreenScraper";

	auto tmp = Settings::getInstance()->getString("ScrapperImageSrc");
	auto md = search.game->metadata.get("image");

	if (!search.overWriteMedias && ss && !Settings::getInstance()->getString("ScrapperImageSrc").empty() && Utils::FileSystem::exists(search.game->metadata.get("image")))
		mResult.mdl.set("image", search.game->metadata.get("image"));
	else if (!result.imageUrl.empty())
	{
		std::string imgPath = getSaveAsPath(search, "image", ext);

		if (!search.overWriteMedias && Utils::FileSystem::exists(imgPath))
		{
			mResult.mdl.set("image", imgPath);

			if (mResult.thumbnailUrl.find(mResult.imageUrl) == 0)
				mResult.thumbnailUrl = "";

			mResult.imageUrl = "";
		}
		else

			mFuncs.push_back(new ResolvePair(
				[this, result, imgPath]
		{
			return downloadImageAsync(result.imageUrl, imgPath);
		},
			[this, imgPath]
		{
			mResult.mdl.set("image", imgPath);

			if (mResult.thumbnailUrl.find(mResult.imageUrl) == 0)
				mResult.thumbnailUrl = "";

			mResult.imageUrl = "";
		}, "image", result.mdl.getName()));
	}

	if (!search.overWriteMedias && ss && !Settings::getInstance()->getString("ScrapperThumbSrc").empty() && Utils::FileSystem::exists(search.game->metadata.get("thumbnail")))
		mResult.mdl.set("thumbnail", search.game->metadata.get("thumbnail"));
	else if (!result.thumbnailUrl.empty() && result.thumbnailUrl.find(result.imageUrl) != 0)
	{
		std::string thumbPath = getSaveAsPath(search, "thumb", ext);

		if (!search.overWriteMedias && Utils::FileSystem::exists(thumbPath))
		{
			mResult.mdl.set("thumbnail", thumbPath);
			mResult.thumbnailUrl = "";
		}
		else

			mFuncs.push_back(new ResolvePair(
				[this, result, thumbPath]
		{
			return downloadImageAsync(result.thumbnailUrl, thumbPath);
		},
			[this, thumbPath]
		{
			mResult.mdl.set("thumbnail", thumbPath);
			mResult.thumbnailUrl = "";
		}, "thumbnail", result.mdl.getName()));
	}

	if (!search.overWriteMedias && ss && !Settings::getInstance()->getString("ScrapperLogoSrc").empty() && Utils::FileSystem::exists(search.game->metadata.get("marquee")))
		mResult.mdl.set("marquee", search.game->metadata.get("marquee"));
	else if (!result.marqueeUrl.empty())
	{
		std::string marqueePath = getSaveAsPath(search, "marquee", ext);

		if (!search.overWriteMedias && Utils::FileSystem::exists(marqueePath))
		{
			mResult.mdl.set("marquee", marqueePath);
			mResult.marqueeUrl = "";
		}
		else

			mFuncs.push_back(new ResolvePair(
				[this, result, marqueePath]
		{
			return downloadImageAsync(result.marqueeUrl, marqueePath);
		},
			[this, marqueePath]
		{
			mResult.mdl.set("marquee", marqueePath);
			mResult.marqueeUrl = "";
		}, "marquee", result.mdl.getName()));
	}

	if (!search.overWriteMedias && Settings::getInstance()->getBool("ScrapeVideos") && Utils::FileSystem::exists(search.game->metadata.get("video")))
		mResult.mdl.set("video", search.game->metadata.get("video"));
	else if (!result.videoUrl.empty())
	{
		std::string videoPath = getSaveAsPath(search, "video", ".mp4");

		if (!search.overWriteMedias && Utils::FileSystem::exists(videoPath))
		{
			mResult.mdl.set("video", videoPath);
			mResult.videoUrl = "";
		}
		else

			mFuncs.push_back(new ResolvePair(
				[this, result, videoPath]
		{
			return downloadImageAsync(result.videoUrl, videoPath);
		},
			[this, videoPath]
		{
			mResult.mdl.set("video", videoPath);
			mResult.videoUrl = "";
		}, "video", result.mdl.getName()));
	}

	auto it = mFuncs.cbegin();
	if (it == mFuncs.cend())
		setStatus(ASYNC_DONE);
	else
	{
		mSource = (*it)->source;
		mCurrentItem = (*it)->name;
		(*it)->Run();		
	}
}

void MDResolveHandle::update()
{
	if(mStatus == ASYNC_DONE || mStatus == ASYNC_ERROR)
		return;
	
	auto it = mFuncs.cbegin();
	if (it == mFuncs.cend())
	{
		setStatus(ASYNC_DONE);
		return;
	}

	ResolvePair* pPair = (*it);
		
	if (pPair->handle->status() == ASYNC_IN_PROGRESS)
		mPercent = pPair->handle->getPercent();

	if (pPair->handle->status() == ASYNC_ERROR)
	{
		setError(pPair->handle->getStatusString());
		for (auto fc : mFuncs)
			delete fc;

		return;
	}
	else if (pPair->handle->status() == ASYNC_DONE)
	{
		pPair->onFinished();
		mFuncs.erase(it);
		delete pPair;

		auto next = mFuncs.cbegin();
		if (next != mFuncs.cend())
		{
			mSource = (*next)->source;
			mCurrentItem = (*next)->name;
			(*next)->Run();
		}
	}
	
	if(mFuncs.empty())
		setStatus(ASYNC_DONE);
}

std::unique_ptr<ImageDownloadHandle> downloadImageAsync(const std::string& url, const std::string& saveAs)
{
	return std::unique_ptr<ImageDownloadHandle>(new ImageDownloadHandle(url, saveAs, 
		Settings::getInstance()->getInt("ScraperResizeWidth"), Settings::getInstance()->getInt("ScraperResizeHeight")));
}

ImageDownloadHandle::ImageDownloadHandle(const std::string& url, const std::string& path, int maxWidth, int maxHeight) : 
	mSavePath(path), mMaxWidth(maxWidth), mMaxHeight(maxHeight), mReq(new HttpReq(url))
{
}

int ImageDownloadHandle::getPercent()
{
	if (mReq->status() == HttpReq::REQ_IN_PROGRESS)
		return mReq->getPercent();

	return -1;
}

void ImageDownloadHandle::update()
{
	if(mReq->status() == HttpReq::REQ_IN_PROGRESS)
		return;

	if(mReq->status() != HttpReq::REQ_SUCCESS)
	{
		std::stringstream ss;
		ss << "Network error: " << mReq->getErrorMsg();
		setError(ss.str());
		return;
	}

	if (mStatus == ASYNC_IN_PROGRESS)
	{
		int ret = mReq->saveContent(mSavePath, true);
		if (ret == 2)
		{
			setError("Failed to save media : The server response is invalid");
			return;
		}
		else if (ret == 1)
		{
			setError("Failed to save image on disk. Disk full?");
			return;
		}
	
		// It's an image ?
		std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(mSavePath));
		if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" || ext == ".gif")
			resizeImage(mSavePath, mMaxWidth, mMaxHeight);
	}

	setStatus(ASYNC_DONE);
}

//you can pass 0 for width or height to keep aspect ratio
bool resizeImage(const std::string& path, int maxWidth, int maxHeight)
{
	// nothing to do
	if(maxWidth == 0 && maxHeight == 0)
		return true;

	FREE_IMAGE_FORMAT format = FIF_UNKNOWN;
	FIBITMAP* image = NULL;
	
	//detect the filetype
	format = FreeImage_GetFileType(path.c_str(), 0);
	if(format == FIF_UNKNOWN)
		format = FreeImage_GetFIFFromFilename(path.c_str());
	if(format == FIF_UNKNOWN)
	{
		LOG(LogError) << "Error - could not detect filetype for image \"" << path << "\"!";
		return false;
	}

	//make sure we can read this filetype first, then load it
	if(FreeImage_FIFSupportsReading(format))
	{
		image = FreeImage_Load(format, path.c_str());
	}else{
		LOG(LogError) << "Error - file format reading not supported for image \"" << path << "\"!";
		return false;
	}

	float width = (float)FreeImage_GetWidth(image);
	float height = (float)FreeImage_GetHeight(image);

	if(maxWidth == 0)
	{
		maxWidth = (int)((maxHeight / height) * width);
	}else if(maxHeight == 0)
	{
		maxHeight = (int)((maxWidth / width) * height);
	}
	
	if (width <= maxWidth && height <= maxHeight)
	{
		FreeImage_Unload(image);
		return true;
	}
		
	FIBITMAP* imageRescaled = FreeImage_Rescale(image, maxWidth, maxHeight, FILTER_BILINEAR);
	FreeImage_Unload(image);

	if(imageRescaled == NULL)
	{
		LOG(LogError) << "Could not resize image! (not enough memory? invalid bitdepth?)";
		return false;
	}

	bool saved = false;
	
	try
	{
		saved = (FreeImage_Save(format, imageRescaled, path.c_str()) != 0);
	}
	catch(...) { }

	FreeImage_Unload(imageRescaled);

	if(!saved)
		LOG(LogError) << "Failed to save resized image!";

	return saved;
}

std::string getSaveAsPath(const ScraperSearchParams& params, const std::string& suffix, const std::string& extension)
{
	const std::string subdirectory = params.system->getName();
	const std::string name = Utils::FileSystem::getStem(params.game->getPath()) + "-" + suffix;

	std::string subFolder = "images";
	if (suffix == "video")
		subFolder = "videos";

	std::string path = params.system->getRootFolder()->getPath() + "/" + subFolder + "/"; // batocera

	if(!Utils::FileSystem::exists(path))
		Utils::FileSystem::createDirectory(path);

	// batocera
	//path += subdirectory + "/";
	//
	//if(!Utils::FileSystem::exists(path))
	//	Utils::FileSystem::createDirectory(path);

	path += name + extension;
	return path;
}
