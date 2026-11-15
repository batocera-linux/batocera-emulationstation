#include "scrapers/ScraperKeyFormat.h"

#include <cstdio>
#include <cstring>

namespace ScraperKeyFormat
{
	std::string urlEncode(const std::string& value)
	{
		// Same unreserved set as HttpReq::urlEncode.
		static const char* unreserved = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";

		std::string escaped;
		for (unsigned char c : value)
		{
			if (std::strchr(unreserved, c) != nullptr)
			{
				escaped.push_back(static_cast<char>(c));
				continue;
			}

			char buffer[4];
			std::snprintf(buffer, sizeof(buffer), "%%%02X", c);
			escaped += buffer;
		}

		return escaped;
	}

	std::string screenScraperDevLogin(const std::string& id, const std::string& password, const std::string& compiled)
	{
		if (!id.empty() && !password.empty())
			return "devid=" + urlEncode(id) + "&devpassword=" + urlEncode(password);

		return compiled;
	}

	std::string gamesDBAPIKey(const std::string& userKey, const std::string& compiled)
	{
		return userKey.empty() ? compiled : userKey;
	}

	std::string hfsDBLogin(const std::string& user, const std::string& password, const std::string& compiled)
	{
		if (!user.empty() && !password.empty())
			return user + ":" + password;

		return compiled;
	}

	std::string cheevosLogin(const std::string& user, const std::string& webAPIKey, const std::string& compiled)
	{
		if (!user.empty() && !webAPIKey.empty())
			return "z=" + urlEncode(user) + "&y=" + urlEncode(webAPIKey);

		return compiled;
	}
}
