#include <exception>
#include <map>

#include "scrapers/ArcadeDBJSONScraper.h"
#include "scrapers/ArcadeDBJSONScraperResources.h"

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

namespace
{
ArcadeDBJSONRequestResources resources;
}


void ArcadeDBScraper::generateRequests(const ScraperSearchParams& params,
	std::queue<std::unique_ptr<ScraperRequest>>& requests, std::vector<ScraperSearchResult>& results)
{
	resources.prepare();
	std::string path = "http://adb.arcadeitalia.net";
	//const std::string apiKey = std::string("apikey=") + resources.getApiKey();
	std::string cleanName = params.nameOverride;
	if (!cleanName.empty() /*&& cleanName.substr(0, 3) == "id:"*/)
	{
		std::string gameID = cleanName; //.substr(3);
		path += "/service_scraper.php?ajax=query_mame&lang=en&game_name=" +
				HttpReq::urlEncode(gameID);
	} else
	{
		if (cleanName.empty())
			cleanName = params.game->getCleanName();
		path += "/service_scraper.php?ajax=query_mame&lang=en&game_name=" +
                HttpReq::urlEncode(cleanName);
	}

    requests.push(std::unique_ptr<ScraperRequest>(new ArcadeDBJSONRequest(results, path)));
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

std::string getBoxartImage(const Value& v)
{
	if (!v.IsArray() || v.Size() == 0)
	{
		return "";
	}
	for (int i = 0; i < (int)v.Size(); ++i)
	{
		auto& im = v[i];
		std::string type = getStringOrThrow(im, "type");
		std::string side = getStringOrThrow(im, "side");
		if (type == "boxart" && side == "front")
		{
			return getStringOrThrow(im, "filename");
		}
	}
	return getStringOrThrow(v[0], "filename");
}

std::string getManufacturerString(const Value& v)
{
	if (!v.IsArray())
	{
		return "";
	}
	std::string out = "";
	bool first = true;
	for (int i = 0; i < (int)v.Size(); ++i)
	{
		auto mapIt = resources.gamesdb_new_developers_map.find(getIntOrThrow(v[i]));
		if (mapIt == resources.gamesdb_new_developers_map.cend())
		{
			continue;
		}
		if (!first)
		{
			out += ", ";
		}
		out += mapIt->second;
		first = false;
	}
	return out;
}

std::string getGenreString(const Value& v)
{
	if (!v.IsArray())
	{
		return "";
	}
	std::string out = "";
	bool first = true;
	for (int i = 0; i < (int)v.Size(); ++i)
	{
		auto mapIt = resources.gamesdb_new_genres_map.find(getIntOrThrow(v[i]));
		if (mapIt == resources.gamesdb_new_genres_map.cend())
		{
			continue;
		}
		if (!first)
		{
			out += ", ";
		}
		out += mapIt->second;
		first = false;
	}
	return out;
}

void processGame(const Value& game, std::vector<ScraperSearchResult>& results)
{
	//std::string baseImageUrlThumb = getStringOrThrow(boxart["base_url"], "thumb");
    //std::string baseImageUrlLarge = getStringOrThrow(boxart["base_url"], "large");

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

	if (game.HasMember("genre") && game["genre"].IsString())result.mdl.set(MetaDataId::Genre, getStringOrThrow(game, "genre"));

	if (game.HasMember("players") && game["players"].IsInt())
		result.mdl.set(MetaDataId::Players, std::to_string(game["players"].GetInt()));
	
	/*if (boxart.HasMember("data") && boxart["data"].IsObject())
	{
		std::string id = std::to_string(getIntOrThrow(game, "id"));
		if (boxart["data"].HasMember(id.c_str()))
		{
		    std::string image = getBoxartImage(boxart["data"][id.c_str()]);
			result.urls[MetaDataId::Image] = ScraperSearchItem(baseImageUrlLarge + "/" + image);
			result.urls[MetaDataId::Thumbnail] = ScraperSearchItem(baseImageUrlThumb + "/" + image);
		}
	}*/

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

	/*if (!doc.HasMember("include") || !doc["include"].HasMember("boxart"))
	{
		std::string warn = "ArcadeDBJSONRequest - Response had no include boxart data.\n";
		LOG(LogWarning) << warn;
		return true;
	}

	const Value& boxart = doc["include"]["boxart"];

	if (!boxart.HasMember("base_url") || !boxart.HasMember("data") || !boxart.IsObject())
	{
		std::string warn = "ArcadeDBJSONRequest - Response include had no usable boxart data.\n";
		LOG(LogWarning) << warn;
		return true;
	}*/

	resources.ensureResources();
	
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
