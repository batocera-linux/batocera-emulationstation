#include "HttpReq.h"

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include <assert.h>

#include <SDL.h>

#ifdef WIN32
#include <time.h>
#else
#include <unistd.h>
#endif

#include <mutex>
static std::mutex mMutex;

CURLM* HttpReq::s_multi_handle = curl_multi_init();

std::map<CURL*, HttpReq*> HttpReq::s_requests;

std::string HttpReq::urlEncode(const std::string &s)
{
    const std::string unreserved = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";

    std::string escaped="";
    for(size_t i=0; i<s.length(); i++)
    {
        if (unreserved.find_first_of(s[i]) != std::string::npos)
        {
            escaped.push_back(s[i]);
        }
        else
        {
            escaped.append("%");
            char buf[3];
            sprintf(buf, "%.2X", (unsigned char)s[i]);
            escaped.append(buf);
        }
    }
    return escaped;
}

bool HttpReq::isUrl(const std::string& str)
{
	//the worst guess
	return (!str.empty() && !Utils::FileSystem::exists(str) && 
		(str.find("http://") != std::string::npos || str.find("https://") != std::string::npos || str.find("www.") != std::string::npos));
}

#ifdef WIN32
LONG _regGetDWORD(HKEY hKey, const std::string &strPath, const std::string &strValueName)
{
	HKEY hSubKey;
	LONG nRet = ::RegOpenKeyEx(hKey, strPath.c_str(), 0L, KEY_QUERY_VALUE, &hSubKey);
	if (nRet == ERROR_SUCCESS)
	{
		DWORD dwBufferSize(sizeof(DWORD));
		DWORD nResult(0);

		nRet = ::RegQueryValueExA(hSubKey, strValueName.c_str(), 0, NULL, reinterpret_cast<LPBYTE>(&nResult), &dwBufferSize);
		::RegCloseKey(hSubKey);

		if (nRet == ERROR_SUCCESS)
			return nResult;
	}

	return 0;
}

std::string _regGetString(HKEY hKey, const std::string &strPath, const std::string &strValueName)
{
	std::string ret;

	HKEY hSubKey;
	LONG nRet = ::RegOpenKeyEx(hKey, strPath.c_str(), 0L, KEY_QUERY_VALUE, &hSubKey);
	if (nRet == ERROR_SUCCESS)
	{
		char szBuffer[1024];
		DWORD dwBufferSize = sizeof(szBuffer);

		nRet = ::RegQueryValueExA(hSubKey, strValueName.c_str(), 0, NULL, (LPBYTE)szBuffer, &dwBufferSize);
		::RegCloseKey(hSubKey);

		if (nRet == ERROR_SUCCESS)
			ret = szBuffer;
	}

	return ret;
}
#endif

HttpReq::HttpReq(const std::string& url)
	: mStatus(REQ_IN_PROGRESS), mHandle(NULL)
{
	mPercent = -1;
	mHandle = curl_easy_init();

	if(mHandle == NULL)
	{
		mStatus = REQ_IO_ERROR;
		onError("curl_easy_init failed");
		return;
	}

	//set the url
	CURLcode err = curl_easy_setopt(mHandle, CURLOPT_URL, url.c_str());
	if(err != CURLE_OK)
	{
		mStatus = REQ_IO_ERROR;
		onError(curl_easy_strerror(err));
		return;
	}

	//set curl to handle redirects
	err = curl_easy_setopt(mHandle, CURLOPT_FOLLOWLOCATION, 1L);
	if(err != CURLE_OK)
	{
		mStatus = REQ_IO_ERROR;
		onError(curl_easy_strerror(err));
		return;
	}

	//set curl max redirects
	err = curl_easy_setopt(mHandle, CURLOPT_MAXREDIRS, 2L);
	if(err != CURLE_OK)
	{
		mStatus = REQ_IO_ERROR;
		onError(curl_easy_strerror(err));
		return;
	}

	//set curl restrict redirect protocols
	err = curl_easy_setopt(mHandle, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS); 
	if(err != CURLE_OK)
	{
		mStatus = REQ_IO_ERROR;
		onError(curl_easy_strerror(err));
		return;
	}

	//tell curl how to write the data
	err = curl_easy_setopt(mHandle, CURLOPT_WRITEFUNCTION, &HttpReq::write_content);
	if(err != CURLE_OK)
	{
		mStatus = REQ_IO_ERROR;
		onError(curl_easy_strerror(err));
		return;
	}

	//give curl a pointer to this HttpReq so we know where to write the data *to* in our write function
	err = curl_easy_setopt(mHandle, CURLOPT_WRITEDATA, this);
	if(err != CURLE_OK)
	{
		mStatus = REQ_IO_ERROR;
		onError(curl_easy_strerror(err));
		return;
	}

#ifdef WIN32
	// Setup system proxy on Windows if required
	if (_regGetDWORD(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", "ProxyEnable"))
	{
		auto proxyServer = _regGetString(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", "ProxyServer");
		if (!proxyServer.empty())
		{
			std::string protocol = (url.find("https:/") == 0 ? "https=" : "http=");

			size_t pxs = proxyServer.find(protocol);
			if (pxs != std::string::npos)
			{
				size_t pxe = proxyServer.find(";", pxs);
				if (pxe == std::string::npos)
					pxe = proxyServer.size() - 1;

				proxyServer = proxyServer.substr(pxs + protocol.size(), pxe - pxs - protocol.size());
			}

			if (!proxyServer.empty())
			{
				CURLcode ret;
				curl_easy_setopt(mHandle, CURLOPT_PROXY, proxyServer.c_str());
				curl_easy_setopt(mHandle, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
			}
		}
	}
#endif
	
	std::unique_lock<std::mutex> lock(mMutex);

#if defined(WIN32)
	srand(time(NULL) % getpid());
	std::string TempPath;
	char lpTempPathBuffer[MAX_PATH];
	if (GetTempPathA(MAX_PATH, lpTempPathBuffer))
	{

		TCHAR szTempFileName[MAX_PATH];

		if (GetTempFileName(lpTempPathBuffer, TEXT("httpreq"), 0, szTempFileName))
			mStreamPath = std::string(szTempFileName);
		else
		{
			do { mStreamPath = std::string(lpTempPathBuffer) + "httpreq" + std::to_string(rand() % 99999) + ".tmp"; } 
			while (Utils::FileSystem::exists(mStreamPath));	
		}
	}
	
#else
	srand(time(NULL) % getpid() + getppid());

	do { mStreamPath = "/tmp/httpreq" + std::to_string(rand() % 99999) + ".tmp"; }
	while (Utils::FileSystem::exists(mStreamPath));
#endif
	
	mStream.open(mStreamPath, std::ios_base::out | std::ios_base::binary);

	//add the handle to our multi
	CURLMcode merr = curl_multi_add_handle(s_multi_handle, mHandle);
	if(merr != CURLM_OK)
	{
		if (mStream.is_open())
			mStream.close();

		mStatus = REQ_IO_ERROR;
		onError(curl_multi_strerror(merr));
		return;
	}

	s_requests[mHandle] = this;
}

HttpReq::~HttpReq()
{
	std::unique_lock<std::mutex> lock(mMutex);

	if (mStream.is_open())
	{
		mStream.flush();
		mStream.close();
	}

	Utils::FileSystem::removeFile(mStreamPath);

	if(mHandle)
	{
		s_requests.erase(mHandle);

		CURLMcode merr = curl_multi_remove_handle(s_multi_handle, mHandle);

		if(merr != CURLM_OK)
			LOG(LogError) << "Error removing curl_easy handle from curl_multi: " << curl_multi_strerror(merr);

		curl_easy_cleanup(mHandle);
	}
}

HttpReq::Status HttpReq::status()
{
	std::unique_lock<std::mutex> lock(mMutex);

	if(mStatus == REQ_IN_PROGRESS)
	{
		int handle_count;
		CURLMcode merr = curl_multi_perform(s_multi_handle, &handle_count);
		if(merr != CURLM_OK && merr != CURLM_CALL_MULTI_PERFORM)
		{
			if (mStream.is_open())
				mStream.close();

			mStatus = REQ_IO_ERROR;
			onError(curl_multi_strerror(merr));
			return mStatus;
		}

		int msgs_left;
		CURLMsg* msg;
		while((msg = curl_multi_info_read(s_multi_handle, &msgs_left)) != nullptr)
		{
			if(msg->msg == CURLMSG_DONE)
			{
				HttpReq* req = s_requests[msg->easy_handle];				
				if(req == NULL)
				{
					LOG(LogError) << "Cannot find easy handle!";
					continue;
				}

				if (req->mStream.is_open())
				{
					req->mStream.flush();
					req->mStream.close();
				}

				if(msg->data.result == CURLE_OK)
				{
					req->mStatus = REQ_SUCCESS;
				}
				else
				{
					req->mStatus = REQ_IO_ERROR;
					req->onError(curl_easy_strerror(msg->data.result));
				}
			}
		}
	}

	return mStatus;
}

std::string HttpReq::getContent() 
{
	assert(mStatus == REQ_SUCCESS);

	if (mStream.is_open())
	{
		mStream.flush();
		mStream.close();
	}

	std::ifstream t(mStreamPath);
	t.seekg(0, std::ios::end);
	size_t size = t.tellg();
	std::string buffer(size, ' ');
	t.seekg(0);
	t.read(&buffer[0], size);

	return buffer; // mContent.str();
}

void HttpReq::onError(const char* msg)
{
	mErrorMsg = msg;
}

std::string HttpReq::getErrorMsg()
{
	return mErrorMsg;
}

//used as a curl callback
//size = size of an element, nmemb = number of elements
//return value is number of elements successfully read
size_t HttpReq::write_content(void* buff, size_t size, size_t nmemb, void* req_ptr)
{
	HttpReq* request = ((HttpReq*)req_ptr);
		
	std::ofstream& ss = request->mStream;
	ss.write((char*)buff, size * nmemb);

	double cl;
	if (!curl_easy_getinfo(request->mHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &cl))
	{		
		if (cl <= 0)
			request->mPercent = -1;
		else
		{
			double position = (double)ss.tellp();
			request->mPercent = (int) (position * 100.0 / cl);
		}
	}

	return nmemb;
}

int HttpReq::saveContent(const std::string filename, bool checkMedia)
{
	assert(mStatus == REQ_SUCCESS);

	if (mStream.is_open())
	{
		mStream.flush();
		mStream.close();
	}

	if (!Utils::FileSystem::exists(mStreamPath))
		return false;

	if (checkMedia && Utils::FileSystem::getFileSize(mStreamPath) < 300)
	{
		auto data = Utils::String::toUpper(getContent());
		if (data.find("NOMEDIA") != std::string::npos || data.find("ERREUR") != std::string::npos || data.find("ERROR") != std::string::npos || data.find("PROBL") != std::string::npos)
			return 2;
	}
	
	std::ifstream ifs(mStreamPath, std::ios_base::in | std::ios_base::binary);
	if (ifs.bad())
		return 1;

	if (Utils::FileSystem::exists(filename))
		Utils::FileSystem::removeFile(filename);

	std::ofstream ofs(filename, std::ios_base::out | std::ios_base::binary);
	if (ofs.bad())
		return 1;

	ofs << ifs.rdbuf();

	ifs.close();
	ofs.close();
	if (ofs.bad())
		return 1;
		
	return 0;
}