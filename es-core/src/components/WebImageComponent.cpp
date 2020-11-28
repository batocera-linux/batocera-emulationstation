#include "WebImageComponent.h"
#include "utils/StringUtil.h"
#include "HttpReq.h"

#include <algorithm>
#include <cctype>
#include <functional>

// keepInCacheDuration = 0 -> image expire immediately
// keepInCacheDuration = -1 -> image expire never
// keepInCacheDuration = 600 -> image expire in 10 minutes ( 60*10 )
WebImageComponent::WebImageComponent(Window* window, double keepInCacheDuration) : ImageComponent(window), mRequest(nullptr), mKeepInCacheDuration(keepInCacheDuration)
{

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

void WebImageComponent::setImage(std::string path, bool tile, MaxSizeInfo maxSize)
{
	if (!Utils::String::startsWith(path, "http://") && !Utils::String::startsWith(path, "https://"))
	{
		ImageComponent::setImage(path, tile, maxSize);
		return;
	}

	url uri = url::parse(path);
	if (uri.host.empty() || uri.path.empty())
		return;

	std::string localFile = Utils::FileSystem::getEsConfigPath() + "/tmp/" + uri.host + "/" + uri.path;
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
	
	auto status = mRequest->status();
	if (status == HttpReq::REQ_IN_PROGRESS)
		return;

	if (status == HttpReq::REQ_SUCCESS && Utils::FileSystem::exists(mLocalFile))
		ImageComponent::setImage(mLocalFile, false, mMaxSize);

	delete mRequest;
	mRequest = nullptr;	
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

	ImageComponent::render(parentTrans);
}