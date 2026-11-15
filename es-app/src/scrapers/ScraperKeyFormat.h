#pragma once

// Formatting and precedence of the runtime API keys, without Settings or HTTP.
// A user value takes precedence over the value compiled into the binary.

#include <string>

namespace ScraperKeyFormat
{
	std::string urlEncode(const std::string& value);

	std::string screenScraperDevLogin(const std::string& id, const std::string& password, const std::string& compiled);
	std::string gamesDBAPIKey(const std::string& userKey, const std::string& compiled);
	std::string hfsDBLogin(const std::string& user, const std::string& password, const std::string& compiled);
	std::string cheevosLogin(const std::string& user, const std::string& webAPIKey, const std::string& compiled);
}
