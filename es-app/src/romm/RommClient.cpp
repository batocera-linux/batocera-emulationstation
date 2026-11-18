#include "romm/RommClient.h"
#include "utils/FileSystemUtil.h"
#include "romm/RommConfig.h"

#include "HttpReq.h"
#include "Log.h"
#include "utils/StringUtil.h"

#include <chrono>
#include <memory>
#include <thread>
#include <cstdio>

namespace Romm
{
	static const int API_TIMEOUT_SECONDS = 120;        // whole request
	static const int DOWNLOAD_STALL_SECONDS = 60;      // no byte received for that long

	RommClient::RommClient(const std::string& baseUrl, const std::string& token, bool allowSelfSigned)
		: mBaseUrl(normalizeUrl(baseUrl)), mToken(token), mAllowSelfSigned(allowSelfSigned)
	{
	}

	RommClient::RommClient(const RommConfig& config)
		: RommClient(config.baseUrl(), config.token, config.allowSelfSigned)
	{
	}

	std::string RommClient::url(const std::string& pathAndQuery) const
	{
		if (pathAndQuery.empty() || pathAndQuery[0] != '/')
			return mBaseUrl + "/" + pathAndQuery;

		return mBaseUrl + pathAndQuery;
	}

	HttpResult RommClient::get(const std::string& pathAndQuery, const std::atomic<bool>* cancel)
	{
		return perform(pathAndQuery, nullptr, "", nullptr, cancel, API_TIMEOUT_SECONDS, false);
	}

	HttpResult RommClient::postJson(const std::string& path, const std::string& json, const std::atomic<bool>* cancel, bool quietErrors)
	{
		return perform(path, &json, "", nullptr, cancel, API_TIMEOUT_SECONDS, false, quietErrors);
	}

	HttpResult RommClient::download(const std::string& pathAndQuery, const std::string& destFile,
		const std::function<void(int, long long)>& progress, const std::atomic<bool>* cancel)
	{
		auto start = std::chrono::steady_clock::now();
		HttpResult result = perform(pathAndQuery, nullptr, destFile, progress, cancel, DOWNLOAD_STALL_SECONDS, true);

		// Games (not media): size, time and speed, to compare with other tools
		long long size = result.ok() ? (long long)Utils::FileSystem::getFileSize(destFile) : 0;
		if (size >= 1024 * 1024)
		{
			double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
			char line[160];
			snprintf(line, sizeof(line), "%.1f MB in %.1f s (%.1f MB/s)", size / 1e6, seconds, seconds > 0 ? size / 1e6 / seconds : 0.0);
			LOG(LogInfo) << "RomM: downloaded " << Utils::FileSystem::getFileName(destFile) << ": " << line;
		}

		return result;
	}

	HttpResult RommClient::perform(const std::string& pathAndQuery, const std::string* postData, const std::string& outputFile,
		const std::function<void(int, long long)>& progress, const std::atomic<bool>* cancel, int timeoutSeconds, bool stallTimeout, bool quietErrors)
	{
		HttpResult result;

		HttpReqOptions options;
		options.useCookieManager = false;
		options.outputFilename = outputFile;
		options.verifySsl = Utils::String::startsWith(Utils::String::toLower(mBaseUrl), "https://") && !mAllowSelfSigned;
		options.acceptCompressed = outputFile.empty(); // API responses only, never rom files
		options.quietErrors = quietErrors;
		options.customHeaders.push_back("Accept: application/json");

		if (!mToken.empty())
			options.customHeaders.push_back("Authorization: Bearer " + mToken);

		if (postData != nullptr)
		{
			options.customHeaders.push_back("Content-Type: application/json");
			// HttpReq only switches to POST when there is data: an empty JSON object is still valid JSON
			options.dataToPost = postData->empty() ? std::string("{}") : *postData;
		}

		std::string fullUrl = url(pathAndQuery);
		std::unique_ptr<HttpReq> req(new HttpReq(fullUrl, &options));

		auto start = std::chrono::steady_clock::now();
		auto lastActivity = start;
		long long lastPosition = -1;

		while (req->status() == HttpReq::REQ_IN_PROGRESS)
		{
			if (cancel != nullptr && cancel->load())
			{
				result.status = -1;
				result.cancelled = true;
				result.body = "cancelled";
				return result; // deleting the request aborts the transfer and removes the temporary file
			}

			auto now = std::chrono::steady_clock::now();
			bool moved = false;

			if (progress != nullptr || stallTimeout)
			{
				long long position = (long long)req->getPosition();
				if (position != lastPosition)
				{
					moved = true;
					lastPosition = position;
					lastActivity = now;

					if (progress != nullptr)
						progress(req->getPercent(), position < 0 ? 0 : position);
				}
			}

			auto reference = stallTimeout ? lastActivity : start;
			if (std::chrono::duration_cast<std::chrono::seconds>(now - reference).count() > timeoutSeconds)
			{
				LOG(LogWarning) << "RomM: request timed out " << fullUrl;
				result.status = -1;
				result.body = "timeout";
				return result;
			}

			// Data only moves when status() runs, and over HTTPS one call reads a part of what is
			// waiting: poll again at once while the transfer moves, pause only when it does not
			if (!moved)
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}

		result.status = (int)req->status();

		if (result.ok())
			result.body = outputFile.empty() ? req->getContent() : "";
		else
		{
			result.body = req->getErrorMsg();
			if (quietErrors)
			{
				LOG(LogDebug) << "RomM: " << fullUrl << " -> " << result.status << " " << result.body.substr(0, 300);
			}
			else
			{
				LOG(LogWarning) << "RomM: " << fullUrl << " -> " << result.status << " " << result.body.substr(0, 300);
			}
		}

		return result;
	}

	std::string errorDetail(const HttpResult& result)
	{
		if (result.cancelled)
			return "cancelled";

		rapidjson::Document doc;
		doc.Parse(result.body.c_str());
		if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("detail"))
		{
			if (doc["detail"].IsString())
				return doc["detail"].GetString();

			if (doc["detail"].IsArray() && doc["detail"].Size() > 0 && doc["detail"][0].IsObject())
				return Json::str(doc["detail"][0], "msg");
		}

		if (result.body.empty())
			return "HTTP " + std::to_string(result.status);

		return result.body.substr(0, 200);
	}

	namespace Json
	{
		std::string str(const rapidjson::Value& obj, const char* key)
		{
			if (!obj.IsObject())
				return "";

			auto it = obj.FindMember(key);
			if (it == obj.MemberEnd() || !it->value.IsString())
				return "";

			return it->value.GetString();
		}

		long long integer(const rapidjson::Value& obj, const char* key, long long def)
		{
			if (!obj.IsObject())
				return def;

			auto it = obj.FindMember(key);
			if (it == obj.MemberEnd())
				return def;

			if (it->value.IsInt64())
				return it->value.GetInt64();

			if (it->value.IsUint64())
				return (long long)it->value.GetUint64();

			if (it->value.IsDouble())
				return (long long)it->value.GetDouble();

			return def;
		}

		double number(const rapidjson::Value& obj, const char* key, double def)
		{
			if (!obj.IsObject())
				return def;

			auto it = obj.FindMember(key);
			if (it == obj.MemberEnd() || !it->value.IsNumber())
				return def;

			return it->value.GetDouble();
		}

		bool boolean(const rapidjson::Value& obj, const char* key, bool def)
		{
			if (!obj.IsObject())
				return def;

			auto it = obj.FindMember(key);
			if (it == obj.MemberEnd() || !it->value.IsBool())
				return def;

			return it->value.GetBool();
		}

		std::string join(const rapidjson::Value& obj, const char* key, const std::string& separator)
		{
			if (!obj.IsObject())
				return "";

			auto it = obj.FindMember(key);
			if (it == obj.MemberEnd() || !it->value.IsArray())
				return "";

			std::string ret;
			for (auto& item : it->value.GetArray())
			{
				if (!item.IsString())
					continue;

				if (!ret.empty())
					ret += separator;

				ret += item.GetString();
			}

			return ret;
		}

		std::string escape(const std::string& s)
		{
			std::string ret;
			ret.reserve(s.size() + 8);

			for (unsigned char c : s)
			{
				switch (c)
				{
				case '"':  ret += "\\\""; break;
				case '\\': ret += "\\\\"; break;
				case '\n': ret += "\\n"; break;
				case '\r': ret += "\\r"; break;
				case '\t': ret += "\\t"; break;
				default:
					if (c < 0x20)
					{
						char buf[8];
						snprintf(buf, sizeof(buf), "\\u%04x", c);
						ret += buf;
					}
					else
						ret += (char)c;
				}
			}

			return ret;
		}
	}
}
