#include "WebImageComponent.h"
#include "utils/StringUtil.h"
#include "HttpReq.h"
#include "utils/ZipFile.h"
#include "resources/TextureResource.h"

#include <algorithm>
#include <cctype>
#include <functional>

struct url
{
public:
	static url parse(const std::string& url_s)
	{
		url ret;

		using namespace std;

		const std::string prot_end("://");
		std::string::const_iterator prot_i = search(url_s.begin(), url_s.end(), prot_end.begin(), prot_end.end());
		ret.protocol.reserve(distance(url_s.begin(), prot_i));
		transform(url_s.begin(), prot_i, back_inserter(ret.protocol), ptr_fun<int, int>(tolower));
		if (prot_i == url_s.end())
			return ret;

		advance(prot_i, prot_end.length());
		string::const_iterator path_i = find(prot_i, url_s.end(), '/');
		ret.host.reserve(distance(prot_i, path_i));
		transform(prot_i, path_i, back_inserter(ret.host), ptr_fun<int, int>(tolower)); // host is icase
		string::const_iterator query_i = find(path_i, url_s.end(), '?');
		ret.path.assign(path_i, query_i);
		if (query_i != url_s.end())
			++query_i;
		ret.query.assign(query_i, url_s.end());
		return ret;
	}

	std::string protocol, host, path, query;
};

// keepInCacheDuration = 0 -> image expire immediately
// keepInCacheDuration = -1 -> image expire never
// keepInCacheDuration = 600 -> image expire in 10 minutes ( 60*10 )
// keepInCacheDuration = 86400 -> image expire in 24 hours ( 60*60*10 )

WebImageComponent::WebImageComponent(Window* window, double keepInCacheDuration) : ImageComponent(window, true), 
	mRequest(nullptr), mKeepInCacheDuration(keepInCacheDuration),
	mBusyAnim(window, "")
{
	mWaitLoaded = false;
	mOnLoaded = nullptr;

	mBusyAnim.setBackgroundVisible(false);
	mBusyAnim.setSize(mSize);	
}
 

void WebImageComponent::resize()
{
	ImageComponent::resize();
	
	if (mSize.x() == 0)
		mBusyAnim.setSize(mTargetSize);
	else 
		mBusyAnim.setSize(mSize);
}

WebImageComponent::~WebImageComponent()
{
	if (mRequest != nullptr)
		delete mRequest;

	if (mKeepInCacheDuration == 0 && Utils::FileSystem::exists(mLocalFile))
	{
		Utils::FileSystem::removeFile(mLocalFile);

		auto root = Utils::FileSystem::getGenericPath(Utils::FileSystem::getEsConfigPath() + "/tmp");
		auto file = Utils::FileSystem::getParent(Utils::FileSystem::getGenericPath(mLocalFile));

		while (!file.empty() && file != root && file != mLocalFile)
		{
			Utils::FileSystem::removeFile(file);
			file = Utils::FileSystem::getParent(file);
		}
	}
}

void WebImageComponent::setImage(std::string path, bool tile, MaxSizeInfo maxSize, bool checkFileExists)
{
	if (mRequest != nullptr)
	{
		delete mRequest;
		mRequest = nullptr;
	}

	mWaitLoaded = true;

	if (!Utils::String::startsWith(path, "http://") && !Utils::String::startsWith(path, "https://"))
	{
		if (path.empty())
			ImageComponent::setImage(nullptr, 0);
		else
		{
			ImageComponent::setImage(path, tile, maxSize, checkFileExists);
			resize();
		}
		return;
	}

	url uri = url::parse(path);
	if (uri.host.empty() || uri.path.empty())
		return;

	if (Utils::String::startsWith(uri.path, "/"))
		uri.path = uri.path.substr(1);

	std::string queryCrc;

	if (!uri.query.empty())
	{
		auto file_crc32 = Utils::Zip::ZipFile::computeCRC(0, uri.query.data(), uri.query.size());
		queryCrc = "-" + Utils::String::toHexString(file_crc32);
	}

	std::string localFile = Utils::FileSystem::getEsConfigPath() + "/tmp/" + uri.host + "/" + uri.path + queryCrc;
	if (Utils::FileSystem::exists(localFile))
	{
		bool keepLoadingLocal = true;

		if (mKeepInCacheDuration == 0)
		{
			keepLoadingLocal = false;
			Utils::FileSystem::removeFile(localFile);
		}
		else if (mKeepInCacheDuration > 0)
		{
			auto date = Utils::FileSystem::getFileCreationDate(localFile);
			auto duration = Utils::Time::DateTime::now().elapsedSecondsSince(date);
			if (duration > mKeepInCacheDuration)
			{
				keepLoadingLocal = false;
				Utils::FileSystem::removeFile(localFile);
			}
		}

		if (keepLoadingLocal)
		{
			ImageComponent::setImage(localFile, tile, maxSize);
			resize();
			return;
		}
	}

	mMaxSize = maxSize;
	mUrlToLoad = path;
	mLocalFile = localFile;
}

void WebImageComponent::update(int deltaTime)
{
	ImageComponent::update(deltaTime);

	if (mRequest == nullptr)
		return;

	mBusyAnim.update(deltaTime);

	auto status = mRequest->status();
	if (status == HttpReq::REQ_IN_PROGRESS)
		return;

	delete mRequest;
	mRequest = nullptr;	

	if (status == HttpReq::REQ_SUCCESS && Utils::FileSystem::exists(mLocalFile))
	{
		ImageComponent::setImage(mLocalFile, false, mMaxSize);
		resize();
	}
}

void WebImageComponent::render(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	Transform4x4f trans = parentTrans * getTransform();

	float xClipMargin = mSize.x() / 2.0f;
	int yClipMargin = mSize.y() / 2.0f;

	if (!Renderer::isVisibleOnScreen(
		trans.translation().x() - xClipMargin,
		trans.translation().y() - yClipMargin, 
		mSize.x() + 2 * xClipMargin,
		mSize.y() + 2 * yClipMargin))
		return;

	if (mRequest == nullptr && !mUrlToLoad.empty() && !mLocalFile.empty())
	{
		std::string localPath = Utils::FileSystem::getParent(mLocalFile);
		if (!Utils::FileSystem::exists(localPath))
			Utils::FileSystem::createDirectory(localPath);
		
		mRequest = new HttpReq(mUrlToLoad, mLocalFile);
		mUrlToLoad = "";
	}
	
	if (mRequest == nullptr && mWaitLoaded && mOnLoaded != nullptr && mLoadingTexture == nullptr && mTexture && mTexture->bind())
	{
		resize();
		mOnLoaded();
		mWaitLoaded = false;
	}

	ImageComponent::render(parentTrans);

	if (mRequest != nullptr)
		mBusyAnim.render(trans);
}