#pragma once
#ifndef ES_APP_ROMM_ROMM_CLIENT_H
#define ES_APP_ROMM_ROMM_CLIENT_H

#include <atomic>
#include <functional>
#include <string>
#include <rapidjson/document.h>

namespace Romm
{
	struct RommConfig;

	struct HttpResult
	{
		int status = 0;        // HTTP status, or 3/4 (HttpReq IO errors), or -1 when cancelled / timed out
		std::string body;      // response body (success) or error body / message (failure)
		bool cancelled = false;

		bool ok() const { return status >= 200 && status <= 299; }
		bool unauthorized() const { return status == 401 || status == 403; }
	};

	// Thin synchronous client over HttpReq. Meant to be used from worker threads.
	class RommClient
	{
	public:
		RommClient(const std::string& baseUrl, const std::string& token, bool allowSelfSigned);
		explicit RommClient(const RommConfig& config);

		HttpResult get(const std::string& pathAndQuery, const std::atomic<bool>* cancel = nullptr);
		// quietErrors: HTTP errors that are an expected answer (device pairing polls) are logged at debug level only
		HttpResult postJson(const std::string& path, const std::string& json, const std::atomic<bool>* cancel = nullptr, bool quietErrors = false);

		// Streams to destFile (HttpReq writes destFile.tmp then renames it on success).
		HttpResult download(const std::string& pathAndQuery, const std::string& destFile,
			const std::function<void(int percent, long long bytes)>& progress, const std::atomic<bool>* cancel);

		std::string url(const std::string& pathAndQuery) const;
		const std::string& baseUrl() const { return mBaseUrl; }

	private:
		HttpResult perform(const std::string& pathAndQuery, const std::string* postData, const std::string& outputFile,
			const std::function<void(int, long long)>& progress, const std::atomic<bool>* cancel, int timeoutSeconds, bool stallTimeout, bool quietErrors = false);

		std::string mBaseUrl;
		std::string mToken;
		bool mAllowSelfSigned;
	};

	// rapidjson helpers tolerant to missing keys and nulls
	namespace Json
	{
		std::string str(const rapidjson::Value& obj, const char* key);
		long long integer(const rapidjson::Value& obj, const char* key, long long def = 0);
		double number(const rapidjson::Value& obj, const char* key, double def = 0);
		bool boolean(const rapidjson::Value& obj, const char* key, bool def = false);
		std::string join(const rapidjson::Value& obj, const char* key, const std::string& separator = ", ");
		std::string escape(const std::string& s);
	}

	// Extracts {"detail": "..."} from a FastAPI error body (or returns the body itself)
	std::string errorDetail(const HttpResult& result);
}

#endif
