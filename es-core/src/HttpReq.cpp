#include "HttpReq.h"

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include <assert.h>
#include <thread>

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

HttpReq::HttpReq(const std::string& url, const std::string outputFilename)
	: mStatus(REQ_IN_PROGRESS), mHandle(NULL), mFile(NULL)
{
	mUrl = url;
	mFilePath = outputFilename;

	mPosition = -1;
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

	// Ignore expired SSL certificates
	curl_easy_setopt(mHandle, CURLOPT_SSL_VERIFYPEER, 0L);

	//set curl to handle redirects
	err = curl_easy_setopt(mHandle, CURLOPT_CONNECTTIMEOUT, 10L);
	if (err != CURLE_OK)
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

	// Set fake user agent
	err = curl_easy_setopt(mHandle, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT x.y; Win64; x64; rv:10.0) Gecko/20100101 Firefox/10.0");
	if (err != CURLE_OK)
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

	if (!mFilePath.empty())
	{
		mTempStreamPath = outputFilename + ".tmp";
		
		Utils::FileSystem::removeFile(mTempStreamPath);

#if defined(_WIN32)
		mFile = _wfopen(Utils::String::convertToWideString(mTempStreamPath).c_str(), L"wb");
#else
		mFile = fopen(mTempStreamPath.c_str(), "wb");		
#endif

		if (mFile == nullptr)
		{
			mStatus = REQ_IO_ERROR;
			onError("IO Error (disk is Readonly ?)");			
			return;
		}

		mPosition = 0;
		Utils::FileSystem::removeFile(outputFilename);
	}

	//add the handle to our multi
	CURLMcode merr = curl_multi_add_handle(s_multi_handle, mHandle);
	if(merr != CURLM_OK)
	{
		closeStream();

		mStatus = REQ_IO_ERROR;
		onError(curl_multi_strerror(merr));
		return;
	}

	s_requests[mHandle] = this;
}

void HttpReq::closeStream()
{
	if (mFile)
	{
		fflush(mFile);
		fclose(mFile);
		mFile = nullptr;
	}
}

HttpReq::~HttpReq()
{
	std::unique_lock<std::mutex> lock(mMutex);

	closeStream();
	
	if (!mTempStreamPath.empty())
		Utils::FileSystem::removeFile(mTempStreamPath);

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
	if (mStatus == REQ_IN_PROGRESS)
	{
		std::unique_lock<std::mutex> lock(mMutex);

		int handle_count;
		CURLMcode merr = curl_multi_perform(s_multi_handle, &handle_count);
		if (merr != CURLM_OK && merr != CURLM_CALL_MULTI_PERFORM)
		{
			closeStream();

			mStatus = REQ_IO_ERROR;
			onError(curl_multi_strerror(merr));
			return mStatus;
		}

		int msgs_left;
		CURLMsg* msg;
		while ((msg = curl_multi_info_read(s_multi_handle, &msgs_left)) != nullptr)
		{
			if (msg->msg == CURLMSG_DONE)
			{
				HttpReq* req = s_requests[msg->easy_handle];
				if (req == NULL)
				{
					LOG(LogError) << "Cannot find easy handle!";
					continue;
				}

				req->closeStream();

				if (req->mStatus == REQ_FILESTREAM_ERROR)
				{
					std::string err = "File stream error (disk full ?)";
					req->onError(err.c_str());
				}
				else if (msg->data.result == CURLE_OK)
				{
					int http_status_code;
					curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &http_status_code);

					char *ct = NULL;
					if (!curl_easy_getinfo(msg->easy_handle, CURLINFO_CONTENT_TYPE, &ct) && ct)
						req->mResponseContentType = ct;

					if (http_status_code < 200 || http_status_code > 299)
					{
						std::string err;

						if (http_status_code >= 400 && http_status_code <= 500)
						{
							if (req->mFilePath.empty())
								err = req->getContent();

							req->mStatus = (Status)http_status_code;
						}
						else
							req->mStatus = REQ_IO_ERROR;

						if (err.empty())
							err = "HTTP status " + std::to_string(http_status_code);

						req->onError(err.c_str());
					}
					else
					{
						if (!req->mFilePath.empty())
						{
							bool renamed = Utils::FileSystem::renameFile(req->mTempStreamPath.c_str(), req->mFilePath.c_str());
							if (!renamed)
							{
								// Strange behaviour on Windows : sometimes std::rename fails if it's done too early after closing stream
								// Copy file instead & try to delete it
								if (Utils::FileSystem::copyFile(req->mTempStreamPath, req->mFilePath))
									renamed = true;
							}

							if (renamed)
								req->mStatus = REQ_SUCCESS;
							else
							{
								req->mStatus = REQ_IO_ERROR;
								req->onError("file rename failed");
							}
						}
						else
							req->mStatus = REQ_SUCCESS;
					}
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
	if (mFilePath.empty())
		return mContent.str();

	try
	{
		closeStream();

		if (!Utils::FileSystem::exists(mTempStreamPath))
			return "";

		std::ifstream ifs(mTempStreamPath, std::ios_base::in | std::ios_base::binary);
		if (ifs.bad())
			return "";

		std::stringstream ofs;
		ofs << ifs.rdbuf();
		ifs.close();

		return ofs.str();
	}
	catch (...)
	{
		LOG(LogError) << "Error getting Http request content";
	}

	return "";
}

void HttpReq::onError(const char* msg)
{
	mErrorMsg = msg;
	LOG(LogError) << "HttpReq::onError (" + std::to_string(mStatus) << ") : " + mErrorMsg;
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
		
	if (request->mFilePath.empty())
	{
		((HttpReq*)req_ptr)->mContent.write((char*)buff, size * nmemb);
		return size * nmemb;
	}

	FILE* file = request->mFile;
	if (file == nullptr)
		return 0;

	size_t rs = size * nmemb;
	fwrite(buff, 1, rs, file) != rs;	
	if (ferror(file))
	{
		request->closeStream();			
		request->mStatus = REQ_FILESTREAM_ERROR;
		request->mErrorMsg = "IO ERROR (DISK FULL?)";		

		return 0;
	}

	double cl;
	if (!curl_easy_getinfo(request->mHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &cl))
	{		
		request->mPosition += rs;

		if (cl <= 0)
			request->mPercent = -1;
		else
			request->mPercent = (int) (request->mPosition * 100.0 / cl);
	}

	return nmemb;
}

bool HttpReq::wait()
{
	while (status() == HttpReq::REQ_IN_PROGRESS)
		std::this_thread::sleep_for(std::chrono::milliseconds(20));

	return status() == HttpReq::REQ_SUCCESS;
}