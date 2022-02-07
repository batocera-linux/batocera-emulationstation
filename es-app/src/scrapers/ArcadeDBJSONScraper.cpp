#include <exception>
#include <map>

#include "scrapers/ArcadeDBJSONScraper.h"

#include "FileData.h"
#include "Log.h"
#include "PlatformId.h"
#include "Settings.h"
#include "SystemData.h"
#include "utils/TimeUtil.h"
#include "utils/StringUtil.h"
#include "Genres.h"

// Current version is release 4 let's test for this value in JSON result
#ifndef ARCADE_DB_RELEASE_VERSION
#define ARCADE_DB_RELEASE_VERSION 4
#endif

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

using namespace PlatformIds;
using namespace rapidjson;

void ArcadeDBScraper::generateRequests(const ScraperSearchParams& params,
	std::queue<std::unique_ptr<ScraperRequest>>& requests, std::vector<ScraperSearchResult>& results)
{
	std::string path = "http://adb.arcadeitalia.net";

	std::string cleanName = params.nameOverride;
	if (!cleanName.empty())
		path += "/service_scraper.php?ajax=query_mame&lang=en&use_parent=1&game_name=" + HttpReq::urlEncode(cleanName);
	else
	{
		cleanName = Utils::String::removeParenthesis(Utils::FileSystem::getStem(params.game->getPath()));
		path += "/service_scraper.php?ajax=query_mame&lang=en&use_parent=1&game_name=" + HttpReq::urlEncode(cleanName);
	}

    requests.push(std::unique_ptr<ScraperRequest>(new ArcadeDBJSONRequest(results, path)));
}

bool ArcadeDBScraper::isSupportedPlatform(SystemData* system)
{
	return system && system->hasPlatformId(PlatformIds::ARCADE) || system->hasPlatformId(PlatformIds::NEOGEO) || system->hasPlatformId(PlatformIds::LCD_GAMES);
}

const std::set<Scraper::ScraperMediaSource>& ArcadeDBScraper::getSupportedMedias()
{
	static std::set<ScraperMediaSource> mdds = {
		ScraperMediaSource::Screenshot,
		ScraperMediaSource::Box2d,
		ScraperMediaSource::Marquee,
		ScraperMediaSource::TitleShot,
		ScraperMediaSource::Video,
		ScraperMediaSource::ShortTitle
	};

	return mdds;
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

std::vector<std::string> getMediaTagNames(std::string imageSource)
{
	if (imageSource == "ss" || imageSource == "mixrbv2" || imageSource == "mixrbv1" || imageSource == "mixrbv")
		return { "url_image_ingame" };
	if (imageSource == "sstitle")
		return { "url_image_title" };
	if (imageSource == "box-2D" || imageSource == "box-3D")
		return { "url_image_flyer" };
	if (imageSource == "wheel")
		return { "url_image_marquee" };
	if (imageSource == "marquee")
		return { "url_image_marquee" };
	if (imageSource == "video")
		return { "url_video_shortplay", "url_video_shortplay_hd" };

	return std::vector<std::string>();
}

std::string findMedia(const Value& v, std::string scrapeSource)
{
	for (std::string key : getMediaTagNames(scrapeSource))
		if (v.HasMember(key.c_str()) && v[key.c_str()].IsString())
			return v[key.c_str()].GetString();

	return "";
}

void processGame(const Value& game, std::vector<ScraperSearchResult>& results)
{
	ScraperSearchResult result("ArcadeDB");
	
	if (Settings::getInstance()->getBool("ScrapeShortTitle") && game.HasMember("short_title") && game["short_title"].IsString())
		result.mdl.set(MetaDataId::Name, getStringOrThrow(game, "short_title"));
	else
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
	{
		result.mdl.set(MetaDataId::Genre, getStringOrThrow(game, "genre"));
		Genres::convertGenreToGenreIds(&result.mdl);
	}

	if (game.HasMember("players") && game["players"].IsInt())
		result.mdl.set(MetaDataId::Players, std::to_string(game["players"].GetInt()));

	result.mdl.set(MetaDataId::Rating, "-1");

	// Process medias
	auto art = findMedia(game, Settings::getInstance()->getString("ScrapperImageSrc"));
	if (!art.empty())
		result.urls[MetaDataId::Image] = ScraperSearchItem(art, ".png");

	if (!Settings::getInstance()->getString("ScrapperThumbSrc").empty() && Settings::getInstance()->getString("ScrapperThumbSrc") != Settings::getInstance()->getString("ScrapperImageSrc"))
	{
		auto art = findMedia(game, Settings::getInstance()->getString("ScrapperThumbSrc"));
		if (!art.empty())
			result.urls[MetaDataId::Thumbnail] = ScraperSearchItem(art, ".png");
	}

	if (!Settings::getInstance()->getString("ScrapperLogoSrc").empty())
	{
		auto art = findMedia(game, Settings::getInstance()->getString("ScrapperLogoSrc"));
		if (!art.empty())
			result.urls[MetaDataId::Marquee] = ScraperSearchItem(art, ".png");
	}

	if (Settings::getInstance()->getBool("ScrapeTitleShot"))
	{
		auto art = findMedia(game, "sstitle");
		if (!art.empty())
			result.urls[MetaDataId::TitleShot] = ScraperSearchItem(art, ".png");
	}

	if (Settings::getInstance()->getBool("ScrapeVideos"))
	{
		auto art = findMedia(game, "video");
		if (!art.empty())
			result.urls[MetaDataId::Video] = ScraperSearchItem(art);
	}

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
		std::string err = std::string("ArcadeDBJSONRequest - Error parsing JSON. \n\t") + GetParseError_En(doc.GetParseError());
		setError(err);
		LOG(LogError) << err;
		return true;
	}

    if (!doc.HasMember("release") || !doc["release"].IsInt() || doc["release"].GetInt() < ARCADE_DB_RELEASE_VERSION)
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
