#include "scrapers/Scraper.h"

#include "FileData.h"
#include "ArcadeDBJSONScraper.h"
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
#include <SDL_timer.h>

std::vector<std::pair<std::string, Scraper*>> Scraper::scrapers
{
	{ "ScreenScraper", new ScreenScraperScraper() },
	{ "TheGamesDB", new TheGamesDBScraper() },
	{ "ArcadeDB", new ArcadeDBScraper() }
};

Scraper* Scraper::getScraper(const std::string name)
{	
	auto scraper = name;
	if(scraper.empty())
		scraper = Settings::getInstance()->getString("Scraper");
	
	for (auto scrap : Scraper::scrapers)
		if (scrap.first == scraper)
			return scrap.second;

	return nullptr;
}

bool Scraper::isValidConfiguredScraper()
{
	return getScraper() != nullptr;
}

bool Scraper::hasMissingMedia(FileData* file)
{
	return !Utils::FileSystem::exists(file->getMetadata(MetaDataId::Image));
}

std::unique_ptr<ScraperSearchHandle> Scraper::search(const ScraperSearchParams& params)
{
	std::unique_ptr<ScraperSearchHandle> handle(new ScraperSearchHandle());
	generateRequests(params, handle->mRequestQueue, handle->mResults);
	return handle;
}

std::vector<std::string> Scraper::getScraperList()
{
	std::vector<std::string> list;
	for(auto& it : Scraper::scrapers)
		list.push_back(it.first);

	return list;
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
			// propagate error
			setError(req.getErrorCode(), Utils::String::removeHtmlTags(req.getStatusString()));

			// empty our queue
			while(!mRequestQueue.empty())
				mRequestQueue.pop();

			return;
		}

		// finished this one, see if we have any more
		if (status == ASYNC_DONE)
		{
			// If we have results, exit, else process the next request
			if (mResults.size() > 0)
			{
				while (!mRequestQueue.empty())
					mRequestQueue.pop();
			}
			else
				mRequestQueue.pop();
		}
	}

	// we finished without any errors!
	if(mRequestQueue.empty() && mStatus != ASYNC_ERROR)
	{
		setStatus(ASYNC_DONE);
		return;
	}
}

// ScraperHttpRequest
ScraperHttpRequest::ScraperHttpRequest(std::vector<ScraperSearchResult>& resultsWrite, const std::string& url) 
	: ScraperRequest(resultsWrite)
{
	setStatus(ASYNC_IN_PROGRESS);
	mRequest = new HttpReq(url);
	mRetryCount = 0;
	mOverQuotaPendingTime = 0;
}

ScraperHttpRequest::~ScraperHttpRequest()
{
	delete mRequest;	
}

void ScraperHttpRequest::update()
{
	if (mOverQuotaPendingTime > 0)
	{
		int lastTime = SDL_GetTicks();
		if (lastTime - mOverQuotaPendingTime > 5000)
		{
			mOverQuotaPendingTime = 0;

			LOG(LogDebug) << "REQ_429_TOOMANYREQUESTS : Retrying";

			std::string url = mRequest->getUrl();
			delete mRequest;
			mRequest = new HttpReq(url);
		}

		return;
	}

	HttpReq::Status status = mRequest->status();

	// not ready yet
	if (status == HttpReq::REQ_IN_PROGRESS)
		return;

	if(status == HttpReq::REQ_SUCCESS)
	{
		setStatus(ASYNC_DONE); // if process() has an error, status will be changed to ASYNC_ERROR
		process(mRequest, mResults);
		return;
	}

	if (status == HttpReq::REQ_429_TOOMANYREQUESTS)
	{
		mRetryCount++;
		if (mRetryCount > 4)
		{
			setStatus(ASYNC_DONE); // Ignore error
			return;
		}

		setStatus(ASYNC_IN_PROGRESS);

		mOverQuotaPendingTime = SDL_GetTicks();
		LOG(LogDebug) << "REQ_429_TOOMANYREQUESTS : Retrying in 5 seconds";
		return;
	}

	// Ignored errors
	if (status == HttpReq::REQ_404_NOTFOUND || status == HttpReq::REQ_IO_ERROR)
	{
		setStatus(ASYNC_DONE);
		return;
	}

	// Blocking errors
	if (status != HttpReq::REQ_SUCCESS)
	{		
		setError(status, Utils::String::removeHtmlTags(mRequest->getErrorMsg()));
		return;
	}	

	// everything else is some sort of error
	LOG(LogError) << "ScraperHttpRequest network error (status: " << status << ") - " << mRequest->getErrorMsg();
	setError(Utils::String::removeHtmlTags(mRequest->getErrorMsg()));
}

std::unique_ptr<MDResolveHandle> ScraperSearchResult::resolveMetaDataAssets(const ScraperSearchParams& search)
{
	return std::unique_ptr<MDResolveHandle>(new MDResolveHandle(*this, search));
}

// metadata resolving stuff
MDResolveHandle::MDResolveHandle(const ScraperSearchResult& result, const ScraperSearchParams& search) : mResult(result)
{
	mPercent = -1;

	for (auto& url : result.urls)
	{
		if (url.second.url.empty())
			continue;
		
		if (!search.overWriteMedias && Utils::FileSystem::exists(search.game->getMetadata(url.first)))
		{
			mResult.mdl.set(url.first, search.game->getMetadata(url.first));
			continue;
		}

		bool resize = true;

		std::string suffix = "image";
		switch (url.first)
		{
		case MetaDataId::Thumbnail: suffix = "thumb"; break;
		case MetaDataId::Marquee: suffix = "marquee"; break;
		case MetaDataId::Video: suffix = "video";  resize = false; break;
		case MetaDataId::FanArt: suffix = "fanart"; resize = false; break;
		case MetaDataId::BoxBack: suffix = "boxback"; resize = false; break;
		case MetaDataId::BoxArt: suffix = "box"; resize = false; break;
		case MetaDataId::Wheel: suffix = "wheel"; resize = false; break;		
		case MetaDataId::TitleShot: suffix = "titleshot"; break;
		case MetaDataId::Manual: suffix = "manual"; resize = false;  break;
		case MetaDataId::Map: suffix = "map"; resize = false; break;
		case MetaDataId::Cartridge: suffix = "cartridge"; break;
		}

		auto ext = url.second.format;
		if (ext.empty())
			ext = Utils::FileSystem::getExtension(url.second.url);

		std::string resourcePath = getSaveAsPath(search, suffix, ext);

		if (!search.overWriteMedias && Utils::FileSystem::exists(resourcePath))
		{
			mResult.mdl.set(url.first, resourcePath);
			if (mResult.urls.find(url.first) != mResult.urls.cend())
				mResult.urls[url.first].url = "";
		}
		else
		{
			mFuncs.push_back(new ResolvePair(
				[this, url, resourcePath, resize] 
				{ 
					return downloadImageAsync(url.second.url, resourcePath, resize); 
				},
				[this, url](ImageDownloadHandle* result)
				{
					auto finalFile = result->getImageFileName();
					mResult.mdl.set(url.first, finalFile);

					if (mResult.urls.find(url.first) != mResult.urls.cend())
						mResult.urls[url.first].url = "";
				},
				suffix, result.mdl.getName())); // "thumbnail"
		}
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
		setError(pPair->handle->getErrorCode(), pPair->handle->getStatusString());
		for (auto fc : mFuncs)
			delete fc;

		return;
	}
	else if (pPair->handle->status() == ASYNC_DONE)
	{		
		pPair->onFinished(pPair->handle.get());
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

std::unique_ptr<ImageDownloadHandle> MDResolveHandle::downloadImageAsync(const std::string& url, const std::string& saveAs, bool resize)
{
	LOG(LogDebug) << "downloadImageAsync : " << url << " -> " << saveAs;

	return std::unique_ptr<ImageDownloadHandle>(new ImageDownloadHandle(url, saveAs, 
		resize ? Settings::getInstance()->getInt("ScraperResizeWidth") : 0,
		resize ? Settings::getInstance()->getInt("ScraperResizeHeight") : 0));
}

ImageDownloadHandle::ImageDownloadHandle(const std::string& url, const std::string& path, int maxWidth, int maxHeight) : 
	mSavePath(path), mMaxWidth(maxWidth), mMaxHeight(maxHeight)
{
	mOverQuotaPendingTime = 0;

	if (url.find("screenscraper") != std::string::npos && (path.find(".jpg") != std::string::npos || path.find(".png") != std::string::npos) && url.find("media=map") == std::string::npos)
	{
		if (maxWidth > 0 && maxHeight > 0)
			mRequest = new HttpReq(url + "&maxwidth=" + std::to_string(maxWidth), path);
		else if (maxWidth > 0)
			mRequest = new HttpReq(url + "&maxwidth=" + std::to_string(maxWidth), path);
		else if (maxHeight > 0)
			mRequest = new HttpReq(url + "&maxheight=" + std::to_string(maxHeight), path);
		else 
			mRequest = new HttpReq(url, path);
	}
	else
		mRequest = new HttpReq(url, path);
}

ImageDownloadHandle::~ImageDownloadHandle()
{
	delete mRequest;
}

int ImageDownloadHandle::getPercent()
{
	if (mRequest->status() == HttpReq::REQ_IN_PROGRESS)
		return mRequest->getPercent();

	return -1;
}

void ImageDownloadHandle::update()
{
	if (mOverQuotaPendingTime > 0)
	{
		int lastTime = SDL_GetTicks();
		if (lastTime - mOverQuotaPendingTime > 5000)
		{
			mOverQuotaPendingTime = 0;

			LOG(LogDebug) << "REQ_429_TOOMANYREQUESTS : Retrying";

			std::string url = mRequest->getUrl();
			delete mRequest;
			mRequest = new HttpReq(url, mSavePath);
		}

		return;
	}

	HttpReq::Status status = mRequest->status();

	if (status == HttpReq::REQ_IN_PROGRESS)
		return;
	
	if (status == HttpReq::REQ_429_TOOMANYREQUESTS)
	{
		mRetryCount++;
		if (mRetryCount > 4)
		{
			setStatus(ASYNC_DONE); // Ignore error
			return;
		}

		setStatus(ASYNC_IN_PROGRESS);

		mOverQuotaPendingTime = SDL_GetTicks();
		LOG(LogDebug) << "REQ_429_TOOMANYREQUESTS : Retrying in 5 seconds";
		return;
	}


	// Ignored errors
	if (status == HttpReq::REQ_404_NOTFOUND || status == HttpReq::REQ_IO_ERROR)
	{
		setStatus(ASYNC_DONE);
		return;
	}

	// Blocking errors
	if (status != HttpReq::REQ_SUCCESS)
	{
		setError(status, Utils::String::removeHtmlTags(mRequest->getErrorMsg()));
		return;
	}

	if (status == HttpReq::REQ_SUCCESS && mStatus == ASYNC_IN_PROGRESS)
	{
		std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(mSavePath));

		// Make sure extension is the good one, according to the response 'content-type'
		std::string contentType = mRequest->getResponseContentType();
		if (!contentType.empty())
		{
			std::string trueExtension;
			if (Utils::String::startsWith(contentType, "image/"))
			{
				trueExtension = "." + contentType.substr(6);
				if (trueExtension == ".jpeg")
					trueExtension = ".jpg";
				else if (trueExtension == ".svg+xml")
					trueExtension = ".svg";

			}
			else if (Utils::String::startsWith(contentType, "video/"))
			{
				trueExtension = "." + contentType.substr(6);
				if (trueExtension == ".quicktime")
					trueExtension = ".mov";
			}

			if (!trueExtension.empty() && trueExtension != ext)
			{
				auto newFileName = Utils::FileSystem::changeExtension(mSavePath, trueExtension);
				if (Utils::FileSystem::renameFile(mSavePath, newFileName))
				{
					mSavePath = newFileName;
					ext = trueExtension;
				}
			}
		}

		// It's an image ?
		if (mSavePath.find("-fanart") == std::string::npos && mSavePath.find("-map") == std::string::npos && (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" || ext == ".gif"))
		{
			try { resizeImage(mSavePath, mMaxWidth, mMaxHeight); }
			catch(...) { }
		}
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

	if (width == 0 || height == 0)
	{
		FreeImage_Unload(image);
		return true;
	}

	if(maxWidth == 0)
		maxWidth = (int)((maxHeight / height) * width);
	else if(maxHeight == 0)
		maxHeight = (int)((maxWidth / width) * height);
	
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
	else if (suffix == "manual")
		subFolder = "manuals";

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
