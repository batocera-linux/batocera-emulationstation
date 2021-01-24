#include <exception>
#include <map>

#include "scrapers/ArcadeDBJSONScraper.h"

#include "FileData.h"
#include "Log.h"
#include "PlatformId.h"
#include "Settings.h"
#include "SystemData.h"
#include "utils/TimeUtil.h"
#include <pugixml/src/pugixml.hpp>

// Current version is release 4 let's test for this value in JSON result
#ifndef ARCADE_DB_RELEASE_VERSION
#define ARCADE_DB_RELEASE_VERSION 4
#endif

/* When raspbian will get an up to date version of rapidjson we'll be
   able to have it throw in case of error with the following:
#ifndef RAPIDJSON_ASSERT
#define RAPIDJSON_ASSERT(x)                                                    \
  if (!(x)) {                                                                  \
	throw std::runtime_error("rapidjson internal assertion failure: " #x);     \
  }
#endif // RAPIDJSON_ASSERT
*/

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

using namespace PlatformIds;
using namespace rapidjson;

void ArcadeDBScraper::generateRequests(const ScraperSearchParams& params,
	std::queue<std::unique_ptr<ScraperRequest>>& requests, std::vector<ScraperSearchResult>& results)
{
	std::string path = "http://adb.arcadeitalia.net";
	std::string cleanName = params.nameOverride;
	if (!cleanName.empty() /*&& cleanName.substr(0, 3) == "id:"*/)
	{
		std::string gameID = cleanName; //.substr(3);
		path += "/service_scraper.php?ajax=query_mame&lang=en&use_parent=1&game_name=" +
				HttpReq::urlEncode(gameID);
	} else
	{
		if (cleanName.empty())
			cleanName = params.game->getCleanName();
		path += "/service_scraper.php?ajax=query_mame&lang=en&use_parent=1&game_name=" +
                HttpReq::urlEncode(cleanName);
	}

    requests.push(std::unique_ptr<ScraperRequest>(new ArcadeDBJSONRequest(results, path)));
}

bool ArcadeDBScraper::isSupportedPlatform(SystemData* system)
{
	return system && system->hasPlatformId(PlatformIds::ARCADE) || system->hasPlatformId(PlatformIds::NEOGEO);
}

namespace
{

std::string getStringOrThrow(const Value& v, const std::string& key)
{
	if (!v.HasMember(key.c_str()) || !v[key.c_str()].IsString())
	{
		throw std::runtime_error("rapidjson internal assertion failure: missing or non string key:" + key);
	}
	return v[key.c_str()].GetString();
}

int getIntOrThrow(const Value& v, const std::string& key)
{
	if (!v.HasMember(key.c_str()) || !v[key.c_str()].IsInt())
	{
		throw std::runtime_error("rapidjson internal assertion failure: missing or non int key:" + key);
	}
	return v[key.c_str()].GetInt();
}

int getIntOrThrow(const Value& v)
{
	if (!v.IsInt())
	{
		throw std::runtime_error("rapidjson internal assertion failure: not an int");
	}
	return v.GetInt();
}


void processGame(const Value& game, std::vector<ScraperSearchResult>& results)
{
	ScraperSearchResult result;

	result.mdl.set(MetaDataId::Name, getStringOrThrow(game, "title"));

	if (game.HasMember("history") && game["history"].IsString())
		result.mdl.set(MetaDataId::Desc, getStringOrThrow(game, "history"));

	if (game.HasMember("year") && game["year"].IsString())
		result.mdl.set(MetaDataId::ReleaseDate, Utils::Time::DateTime(Utils::Time::stringToTime(getStringOrThrow(game, "year"), "%Y")));

	if (game.HasMember("manufacturer") && game["manufacturer"].IsString())
    {
        result.mdl.set(MetaDataId::Developer, getStringOrThrow(game, "manufacturer"));
        result.mdl.set(MetaDataId::Publisher, getStringOrThrow(game, "manufacturer"));
    }

	if (game.HasMember("genre") && game["genre"].IsString())
	    result.mdl.set(MetaDataId::Genre, getStringOrThrow(game, "genre"));

	if (game.HasMember("players") && game["players"].IsInt())
		result.mdl.set(MetaDataId::Players, std::to_string(game["players"].GetInt()));

    if (game.HasMember("url_image_title") && game["url_image_title"].IsString())
	    result.urls[MetaDataId::TitleShot] = ScraperSearchItem( getStringOrThrow(game, "url_image_title"));

    if (game.HasMember("url_image_ingame") && game["url_image_ingame"].IsString())
        result.urls[MetaDataId::Image] = ScraperSearchItem( getStringOrThrow(game, "url_image_ingame"));

    if (game.HasMember("url_image_marquee") && game["url_image_marquee"].IsString())
        result.urls[MetaDataId::Marquee] = ScraperSearchItem( getStringOrThrow(game, "url_image_marquee"));

    if (game.HasMember("url_video_shortplay") && game["url_video_shortplay"].IsString())
        result.urls[MetaDataId::Video] = ScraperSearchItem( getStringOrThrow(game, "url_video_shortplay"));

    // Map flyer to boxart
    if (game.HasMember("url_image_flyer") && game["url_image_flyer"].IsString())
        result.urls[MetaDataId::BoxArt] = ScraperSearchItem( getStringOrThrow(game, "url_image_flyer"));

	results.push_back(result);
}
} // namespace

  // Process should return false only when we reached a maximum scrap by minute, to retry
bool ArcadeDBJSONRequest::process(HttpReq* request, std::vector<ScraperSearchResult>& results)
{
	assert(request->status() == HttpReq::REQ_SUCCESS);

	Document doc;
	doc.Parse(request->getContent().c_str());

	if (doc.HasParseError())
	{
		std::string err =
			std::string("ArcadeDBJSONRequest - Error parsing JSON. \n\t") + GetParseError_En(doc.GetParseError());
		setError(err);
		LOG(LogError) << err;
		return true;
	}

    if (!doc.HasMember("release") || !doc["release"].IsInt() || doc["release"] != ARCADE_DB_RELEASE_VERSION)
    {
        std::string warn = "ArcadeDBJSONRequest - Response had wrong format.\n";
        LOG(LogWarning) << warn;
        return true;
    }

	if (!doc.HasMember("result") || !doc["result"].IsArray())
	{
		std::string warn = "ArcadeDBJSONRequest - Response had no game data.\n";
		LOG(LogWarning) << warn;
		return true;
	}

    const Value& games = doc["result"];

	for (int i = 0; i < (int)games.Size(); ++i)
	{
		auto& v = games[i];
		try
		{
			processGame(v, results);
		}
		catch (std::runtime_error& e)
		{
			LOG(LogError) << "Error while processing game: " << e.what();
		}
	}

	return true;
}
