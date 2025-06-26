#include "scrapers/IGDBScraper.h"
#include <exception>
#include <map>
#include "FileData.h"
#include "Log.h"
#include "PlatformId.h"
#include "Settings.h"
#include "SystemData.h"
#include "utils/TimeUtil.h"
#include "utils/StringUtil.h"
#include "Genres.h"
#include "SystemConf.h"
#include "ApiSystem.h"
#include "LocaleES.h"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

using namespace PlatformIds;
using namespace rapidjson;

const std::map<PlatformId, std::string> igdb_platformids
{
	{ ARCADE, "52" }, // Has special filtering
	{ TEKNOPARROT, "52" },
	{ THREEDO, "50" },
	{ NINTENDO_3DS, "37" },
	{ ADAM, "68" },
	{ AMIGA, "16" },
	{ AMIGACD32, "114" },
	{ AMIGACDTV, "158" },
	{ AMIGA, "16" },
	{ AMSTRAD_CPC, "25" },
	{ APPLE2GS, "115" },
	{ APPLE_II, "75" },
	{ ARCHIMEDES, "116" },
	{ ASTROCADE, "91" },
	{ ATARI_2600, "59" },
	{ ATARI_5200, "66" },
	{ ATARI_7800, "60" },
	{ ATARI_800, "89769" },
	{ ATARI_JAGUAR, "62" },
	{ ATARI_JAGUAR_CD, "410" },
	{ ATARI_LYNX, "61" },
	{ ATARI_ST, "63" },
	{ ATARI_XE, "65" },
	{ ATOMISWAVE, "52" },
	{ ACORN_ATOM, "116" },
	{ ACORN_BBC_MICRO, "69" },
	{ ACORN_ELECTRON, "134"},
	{ VIC20, "71" },
	{ COMMODORE_64, "15" },
	{ PHILIPS_CDI, "117" },
	{ CHANNELF, "127" },
	{ COLECOVISION, "68" },
	{ PC, "13" },
	{ MOONLIGHT, "13,6" }, // "Windows"	
	{ SEGA_DREAMCAST, "23" },
	{ FMTOWNS, "118" },
	{ NINTENDO_GAME_AND_WATCH, "307" },
	{ NINTENDO_GAMECUBE, "21" },
	{ SEGA_GAME_GEAR, "35" },
	{ GAME_BOY_ADVANCE, "24" },
	{ GAME_BOY_COLOR, "22" },
	{ GAME_BOY, "33" },
	{ SEGA_GENESIS, "29" },
	{ GX4000, "25" },
	{ INTELLIVISION, "67" },
	{ SEGA_MASTER_SYSTEM, "64" },
	{ SEGA_PICO, "339" },
	{ THOMSON_TO_MO, "156" },
	{ MSX, "27" },
	{ NINTENDO_64, "4" },
	{ NINTENDO_64_DISK_DRIVE, "4" },
	{ NINTENDO_DS, "20" },
	{ NEOGEO_CD, "136" },
	{ NEOGEO, "79" },
	{ NINTENDO_ENTERTAINMENT_SYSTEM, "18" },
	{ NEOGEO_POCKET_COLOR, "120" },
	{ NEOGEO_POCKET, "119" },
	{ PCFX, "274" },
	{ PC_88, "125" },
	{ PC_98, "149" },
	{ TURBOGRAFX_16, "86" },
	{ TURBOGRAFX_CD, "150" },
	{ COMMODORE_PET, "90" },
	{ COMMODORE_PLUS4, "94" },
	{ PLAYSTATION_3, "9" },
	{ PLAYSTATION_2, "8" },
	{ PLAYSTATION_VITA, "46" },
	{ PLAYSTATION_PORTABLE, "38" },
	{ PLAYSTATION, "131" },
	{ SATELLAVIEW, "306" },
	{ SEGA_SATURN, "32" },
	{ SUPER_CASSETTE_VISION, "376" },
	{ SCUMMVM, "6" },
	{ SEGA_32X, "30" },
	{ SEGA_CD, "78" },
	{ SEGA_SG1000, "84" },
	{ SUPER_NINTENDO, "19" },
	{ SUPER_NINTENDO_MSU1, "19" },
	{ WATARA_SUPERVISION, "415" },
	{ SUPERGRAFX, "128" },
	{ NINTENDO_SWITCH, "130" },
	{ TI99, "129" },
	{ TRS80_COLOR_COMPUTER, "126" },
	{ VECTREX, "70" },
	{ VIDEOPAC_ODYSSEY2, "133" },
	{ NINTENDO_VIRTUAL_BOY, "87" },
	{ NINTENDO_WII_U, "41" },
	{ NINTENDO_WII, "5" },
	{ WONDERSWAN_COLOR, "123" },
	{ WONDERSWAN, "57" },
	{ SHARP_X1, "77" },
	{ SHARP_X6800, "121" },
	{ XBOX_360, "12" },
	{ XBOX, "11" },
	{ ZX81, "373" },
	{ ZX_SPECTRUM, "2" },
	{ FUJITSU_FM7, "152" },
	{ SEGA_CHIHIRO, "52"}
};

const std::set<Scraper::ScraperMediaSource>& IGDBScraper::getSupportedMedias()
{
	static std::set<ScraperMediaSource> mdds =
	{
		ScraperMediaSource::Screenshot,
		ScraperMediaSource::Box2d,
		ScraperMediaSource::FanArt
	};

	return mdds;
}

bool IGDBScraper::ensureToken()
{
	if (!mToken.empty() && Utils::Time::DateTime::now().elapsedSecondsSince(mTokenDate) < 1800) // make Token expire after 30mn
		return true;

	mToken = "";

	if (Settings::getInstance()->getString("IGDBClientID").empty() || Settings::getInstance()->getString("IGDBSecret").empty())
		return false;

	HttpReqOptions options;
	options.customHeaders.push_back("Content-Type: application/x-www-form-urlencoded");
	options.dataToPost = "client_id=" + Settings::getInstance()->getString("IGDBClientID") + "&client_secret=" + Settings::getInstance()->getString("IGDBSecret") + "&grant_type=client_credentials";

	HttpReq request("https://id.twitch.tv/oauth2/token", &options);
	if (request.wait())
	{
		Document doc;
		auto json = request.getContent();
		doc.Parse(json.c_str());

		if (!doc.HasParseError() && doc.HasMember("access_token"))
		{
			mToken = doc["access_token"].GetString();
			return true;
		}
	}

	return false;
}

void IGDBScraper::generateRequests(const ScraperSearchParams& params, std::queue<std::unique_ptr<ScraperRequest>>& requests, std::vector<ScraperSearchResult>& results)
{
	if (!ensureToken())
	{
		if (!params.isManualScrape)
			throw std::runtime_error(_("INVALID CREDENTIALS"));

		return;
	}

	mTokenDate = Utils::Time::DateTime::now();

	std::string cleanName = params.nameOverride;
	if (cleanName.empty())
		cleanName = Utils::String::removeParenthesis(Utils::FileSystem::getStem(params.game->getPath()));

	std::string data = "fields id, name, platforms.name, genres.name, game_modes.name, multiplayer_modes.offlinemax, release_dates.date, release_dates.region, release_dates.platform, cover.*, screenshots.*, artworks.*, url, summary, aggregated_rating, involved_companies.company.name, involved_companies.developer, involved_companies.publisher ;";
	data += " search \"" + cleanName + "\";" +
	data += " limit 10;";

	std::string url = "https://api.igdb.com/v4/games";

	HttpReqOptions tokenAuth;
	tokenAuth.customHeaders.push_back("Client-ID: " + Settings::getInstance()->getString("IGDBClientID"));
	tokenAuth.customHeaders.push_back("Authorization: Bearer " + mToken);
	tokenAuth.customHeaders.push_back("ContentType: text/plain");
	tokenAuth.customHeaders.push_back("Accept: application/json");

	auto& platforms = params.system->getPlatformIds();
	if (!platforms.empty())
	{
		for (auto platform : platforms)
		{
			auto it = igdb_platformids.find(platform);
			if (it != igdb_platformids.cend())
			{
				for (auto plaformId : Utils::String::split(it->second, ',', true))
				{
					HttpReqOptions options = tokenAuth;
					options.dataToPost = data + " where release_dates.platform = " + plaformId + ";";
					requests.push(std::unique_ptr<ScraperRequest>(new IGDBRequest(results, url, &options, params.system->hasPlatformId(PlatformIds::ARCADE), params.isManualScrape)));
				}
			}
		}
	}

	if (requests.size() == 0)
	{
		HttpReqOptions options = tokenAuth;
		options.dataToPost = data;
		requests.push(std::unique_ptr<ScraperRequest>(new IGDBRequest(results, url, &options, params.system->hasPlatformId(PlatformIds::ARCADE), params.isManualScrape)));
	}	
}

bool IGDBScraper::isSupportedPlatform(SystemData* system)
{
	std::string platformQueryParam;
	auto& platforms = system->getPlatformIds();

	for (auto platform : platforms)
		if (igdb_platformids.find(platform) != igdb_platformids.cend())
			return true;

	return false;
}

static void processIgdbGame(const Value& game, std::vector<ScraperSearchResult>& results, bool arcadeGame)
{
	ScraperSearchResult result("IGDB");
	if (game.HasMember("name") && game["name"].IsString())
		result.mdl.set(MetaDataId::Name, game["name"].GetString());

	if (game.HasMember("summary") && game["summary"].IsString())
		result.mdl.set(MetaDataId::Desc, game["summary"].GetString());

	result.mdl.set(MetaDataId::Rating, "-1");

	if (game.HasMember("involved_companies") && game["involved_companies"].IsArray())
	{
		const Value& involved_companies = game["involved_companies"];
		for (int i = 0; i < (int)involved_companies.Size(); ++i)
		{
			auto& involved_company = involved_companies[i];
			if (involved_company.HasMember("company") && involved_company["company"].IsObject() && involved_company["company"].HasMember("name") && involved_company["company"]["name"].IsString())
			{
				std::string companyName = involved_company["company"]["name"].GetString();
				if (!companyName.empty())
				{
					if (involved_company.HasMember("developer") && involved_company["developer"].IsBool() && involved_company["developer"].GetBool())
						result.mdl.set(MetaDataId::Developer, companyName);

					if (involved_company.HasMember("publisher") && involved_company["publisher"].IsBool() && involved_company["publisher"].GetBool())
						result.mdl.set(MetaDataId::Publisher, companyName);
				}
			}
		}
	}

	if (game.HasMember("cover") && game["cover"].IsObject() && game["cover"].HasMember("image_id") && game["cover"]["image_id"].IsString())
		result.urls[MetaDataId::Thumbnail] = "https://images.igdb.com/igdb/image/upload/t_original/" + std::string(game["cover"]["image_id"].GetString()) + ".jpg";

	if (game.HasMember("screenshots") && game["screenshots"].IsArray())
	{
		const Value& screenshots = game["screenshots"];
		for (int i = 0; i < (int)screenshots.Size(); ++i)
		{
			auto& screenshot = screenshots[i];
			if (!screenshot.HasMember("image_id") || !screenshot["image_id"].IsString())
				continue;

			result.urls[MetaDataId::Image] = "https://images.igdb.com/igdb/image/upload/t_screenshot_huge/" + std::string(screenshot["image_id"].GetString()) + ".jpg";
			break;
		}
	}

	if (Settings::getInstance()->getBool("ScrapeFanart") && game.HasMember("artworks") && game["artworks"].IsArray())
	{
		const Value& artworks = game["artworks"];
		for (int i = 0; i < (int)artworks.Size(); ++i)
		{
			auto& artwork = artworks[i];
			if (!artwork.HasMember("image_id") || !artwork["image_id"].IsString())
				continue;

			result.urls[MetaDataId::FanArt] = "https://images.igdb.com/igdb/image/upload/t_1080p/" + std::string(artwork["image_id"].GetString()) + ".jpg";
			break;
		}
	}
	/*
	if (Settings::getInstance()->getBool("ScrapeVideos") && game.HasMember("videos") && game["videos"].IsArray())
	{
		const Value& videos = game["videos"];
		for (int i = 0; i < (int)videos.Size(); ++i)
		{
			auto& video = videos[i];
			if (!video.HasMember("video_id") || !video["video_id"].IsString())
				continue;

			result.urls[MetaDataId::Video] = "https://www.youtube.com/watch?v=" + std::string(video["video_id"].GetString());
			break;
		}
	}
	*/
	results.push_back(result);
}

// Process should return false only when we reached a maximum scrap by minute, to retry
bool IGDBRequest::process(HttpReq* request, std::vector<ScraperSearchResult>& results)
{
	assert(request->status() == HttpReq::REQ_SUCCESS);

	Document doc;
	doc.Parse(request->getContent().c_str());

	if (doc.HasParseError())
	{
		std::string err = std::string("IGDBRequest - Error parsing JSON. \n\t") + GetParseError_En(doc.GetParseError());
		setError(err);
		LOG(LogError) << err;
		return false;
	}

	if (!doc.IsArray() || !doc.Size())
	{
		std::string warn = "IGDBRequest - Response is not valid.\n";
		LOG(LogWarning) << warn;
		return false;
	}

	for (int i = 0; i < (int)doc.Size(); ++i)
	{
		auto& v = doc[i];

		try
		{
			processIgdbGame(v, results, mIsArcade);
		}
		catch (std::runtime_error& e)
		{
			LOG(LogError) << "Error while processing game: " << e.what();
		}
	}

	return true;
}
