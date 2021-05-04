#include <exception>
#include <map>

#include "scrapers/GamesDBJSONScraper.h"
#include "scrapers/GamesDBJSONScraperResources.h"

#ifdef GAMESDB_APIKEY

#include "FileData.h"
#include "Log.h"
#include "PlatformId.h"
#include "Settings.h"
#include "SystemData.h"
#include "utils/TimeUtil.h"
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>


using namespace PlatformIds;
using namespace rapidjson;

namespace
{
TheGamesDBJSONRequestResources resources;
}

const std::map<PlatformId, std::string> gamesdb_new_platformid_map{
	{ THREEDO, "25" },
	{ AMIGA, "4911" },
	{ AMSTRAD_CPC, "4914" },
	{ APPLE_II, "4942" },
	{ ATARI_800, "4943" },
	{ ATARI_2600, "22" },
	{ ATARI_5200, "26" },
	{ ATARI_7800, "27" },
	{ ATARI_JAGUAR, "28" },
	{ ATARI_JAGUAR_CD, "29" },
	{ ATARI_LYNX, "4924" },
	{ ATARI_ST, "4937" },
	{ ATARI_XE, "30" },
	{ COLECOVISION, "31" },	
	{ COMMODORE_64, "40" },
	{ COMMODORE_VIC20, "4945" },
	{ INTELLIVISION, "32" },
	{ MAC_OS, "37" },
	{ XBOX, "14" },
	{ XBOX_360, "15" },
	{ MSX, "4929" },
	{ NEOGEO, "24" },
	{ NEOGEO_POCKET, "4922" },
	{ NEOGEO_POCKET_COLOR, "4923" },
	{ NINTENDO_3DS, "4912" },
	{ NINTENDO_64, "3" },
	{ NINTENDO_DS, "8" },
	{ FAMICOM_DISK_SYSTEM, "4936" },
	{ NINTENDO_ENTERTAINMENT_SYSTEM, "7" },
	{ GAME_BOY, "4" },
	{ GAME_BOY_ADVANCE, "5" },
	{ GAME_BOY_COLOR, "41" },
	{ NINTENDO_GAMECUBE, "2" },
	{ NINTENDO_WII, "9" },
	{ NINTENDO_WII_U, "38" },
	{ NINTENDO_VIRTUAL_BOY, "4918" },
	{ NINTENDO_GAME_AND_WATCH, "4950" },
	{ SEGA_32X, "33" },
	{ SEGA_CD, "21" },
	{ SEGA_DREAMCAST, "16" },
	{ SEGA_GAME_GEAR, "20" },
	{ SEGA_GENESIS, "18" },
	{ SEGA_MASTER_SYSTEM, "35" },
	{ SEGA_MEGA_DRIVE, "36" },
	{ SEGA_SATURN, "17" },
	{ SEGA_SG1000, "4949" },
	{ PLAYSTATION, "10" },
	{ PLAYSTATION_2, "11" },
	{ PLAYSTATION_3, "12" },
	{ PLAYSTATION_4, "4919" },
	{ PLAYSTATION_VITA, "39" },
	{ PLAYSTATION_PORTABLE, "13" },
	{ SUPER_NINTENDO, "6" },
	{ TURBOGRAFX_16, "34" },   // HuCards only
	{ TURBOGRAFX_CD, "4955" }, // CD-ROMs only
	{ WONDERSWAN, "4925" },
	{ WONDERSWAN_COLOR, "4926" },
	{ ZX_SPECTRUM, "4913" },
	{ VIDEOPAC_ODYSSEY2, "4927" },
	{ VECTREX, "4939" },
	{ TRS80_COLOR_COMPUTER, "4941" },
	{ TANDY, "4941" },	
	{ SUPERGRAFX, "34" }, // The code is TurboGrafx 16, but they manage SUPERGRAFX into this one....
	{ AMIGACD32, "4947" },	
	{ NEOGEO_CD, "4956" },
	{ PCFX, "4930" },
	{ POKEMINI, "4957" },	
	{ SATELLAVIEW, "6" },
	{ SUFAMITURBO, "6" },
	{ PC_88, "4933" },
	{ PC_98, "4934" },
	{ SHARP_X1, "4977" },
	{ SHARP_X6800, "4931" },
	{ NINTENDO_SWITCH, "4971" },
	{ TI99, "4953" },
	{ VIC20, "4945" },
	{ CHANNELF, "4928" },
	{ SAMCOUPE, "4979" },
	{ SUPER_CASSETTE_VISION , "4966" },
	{ ARCHIMEDES, "4944" },
	{ ACORN_ELECTRON , "4954" },
	{ ASTROCADE , "4968" },
	{ FMTOWNS, "4932" },
	{ PHILIPS_CDI, "4917" },
	{ WATARA_SUPERVISION, "4959" },

	// 1 = PC
	{ PC, "1" },
	{ MOONLIGHT, "1" },
	{ PRBOOM, "1" },

	// 23 = Arcade
	{ ARCADE, "23" },
	{ NAOMI, "23" },
	{ ATOMISWAVE, "23" },
	{ DAPHNE, "23" }

	/* Non existing systems
	{ AMIGACDTV, "129" },
	{ CAVESTORY, "135" },
	{ ATOMISWAVE, "53" },
	{ LUTRO, "206" },
	{ GX4000, "87" },
	{ ZX81, "77" },
	{ VISUALPINBALL, "198" },
	{ FUTUREPINBALL, "199" },
	{ ORICATMOS, "131" },
	{ SOLARUS, "223" },
	{ THOMSON_TO_MO, "141" },
	{ OPENBOR, "214" },
	{ UZEBOX, "216" },
	{ APPLE2GS, "217" },
	{ SPECTRAVIDEO, "218" },
	{ PALMOS, "219" },
	*/
};

bool TheGamesDBScraper::isSupportedPlatform(SystemData* system)
{
	std::string platformQueryParam;
	auto& platforms = system->getPlatformIds();

	for (auto platform : platforms)
		if (gamesdb_new_platformid_map.find(platform) != gamesdb_new_platformid_map.cend())
			return true;

	return false;
}

bool TheGamesDBScraper::hasMissingMedia(FileData* file)
{
	if (!Settings::getInstance()->getString("ScrapperImageSrc").empty() && !Utils::FileSystem::exists(file->getMetadata(MetaDataId::Image)))
		return true;

	if (!Settings::getInstance()->getString("ScrapperThumbSrc").empty() && !Utils::FileSystem::exists(file->getMetadata(MetaDataId::Thumbnail)))
		return true;

	if (!Settings::getInstance()->getString("ScrapperLogoSrc").empty() && !Utils::FileSystem::exists(file->getMetadata(MetaDataId::Marquee)))
		return true;

	if (Settings::getInstance()->getBool("ScrapeFanart") && !Utils::FileSystem::exists(file->getMetadata(MetaDataId::FanArt)))
		return true;

	if (Settings::getInstance()->getBool("ScrapeTitleShot") && !Utils::FileSystem::exists(file->getMetadata(MetaDataId::TitleShot)))
		return true;

	return false;
}

void TheGamesDBScraper::generateRequests(const ScraperSearchParams& params,
	std::queue<std::unique_ptr<ScraperRequest>>& requests, std::vector<ScraperSearchResult>& results)
{
	resources.prepare();
	std::string path = "https://api.thegamesdb.net/v1";
	bool usingGameID = false;
	const std::string apiKey = std::string("apikey=") + resources.getApiKey();
	std::string cleanName = params.nameOverride;

	if (!cleanName.empty() && cleanName.substr(0, 3) == "id:")
	{
		std::string gameID = cleanName.substr(3);
		path += "/Games/ByGameID?" + apiKey +
				"&fields=players,publishers,genres,overview,last_updated,rating,"
				"platform,coop,youtube,os,processor,ram,hdd,video,sound,alternates&"
				"include=boxart&id=" +
				HttpReq::urlEncode(gameID);
		usingGameID = true;
	} 
	else
	{
		if (cleanName.empty())
			cleanName = params.game->getCleanName();

		path += "/Games/ByGameName?" + apiKey +
				"&fields=players,publishers,genres,overview,last_updated,rating,"
				"platform,coop,youtube,os,processor,ram,hdd,video,sound,alternates&"
				"include=boxart&name=" +
				HttpReq::urlEncode(cleanName);
	}

	if (usingGameID)
	{
		// if we have the ID already, we don't need the GetGameList request
		requests.push(std::unique_ptr<ScraperRequest>(new TheGamesDBJSONRequest(results, path)));
	} 
	else
	{
		std::string platformQueryParam;
		auto& platforms = params.system->getPlatformIds();
		if (!platforms.empty())
		{
			bool first = true;
			platformQueryParam += "&filter%5Bplatform%5D=";
			for (auto platformIt = platforms.cbegin(); platformIt != platforms.cend(); platformIt++)
			{
				auto mapIt = gamesdb_new_platformid_map.find(*platformIt);
				if (mapIt != gamesdb_new_platformid_map.cend())
				{
					if (!first)
						platformQueryParam += ",";

					platformQueryParam += HttpReq::urlEncode(mapIt->second);
					first = false;
				} 
				else
				{
					LOG(LogWarning) << "TheGamesDB scraper warning - no support for platform "
									<< getPlatformName(*platformIt);
				}
			}
			path += platformQueryParam;
		}

		requests.push(std::unique_ptr<ScraperRequest>(new TheGamesDBJSONRequest(requests, results, path)));
	}
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

	std::string getDeveloperString(const Value& v)
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

	std::string getPublisherString(const Value& v)
	{
		if (!v.IsArray())
		{
			return "";
		}
		std::string out = "";
		bool first = true;
		for (int i = 0; i < (int)v.Size(); ++i)
		{
			auto mapIt = resources.gamesdb_new_publishers_map.find(getIntOrThrow(v[i]));
			if (mapIt == resources.gamesdb_new_publishers_map.cend())
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

	std::vector<std::string> getMediaTagNames(std::string imageSource)
	{
		if (imageSource == "ss" || imageSource == "mixrbv2" || imageSource == "mixrbv1" || imageSource == "mixrbv")
			return { "screenshot" };
		if (imageSource == "sstitle")
			return { "titlescreen" };
		if (imageSource == "box-2D" || imageSource == "box-3D")
			return { "boxart" };
		if (imageSource == "wheel" || imageSource == "marquee")
			return { "clearlogo" };

		return{ imageSource };
	}

	std::string findMedia(const std::map<std::string, std::string>& map, std::string scrapeSource)
	{
		for (std::string key : getMediaTagNames(scrapeSource))
		{
			auto it = map.find(key);
			if (it != map.cend())
				return it->second;
		}

		return "";
	}

	void processGame(const Value& game, const Value& boxart, std::vector<ScraperSearchResult>& results)
	{
		std::string baseImageUrlThumb = getStringOrThrow(boxart["base_url"], "thumb");
		std::string baseImageUrlLarge = getStringOrThrow(boxart["base_url"], "large");

		ScraperSearchResult result;

		result.mdl.set(MetaDataId::Name, getStringOrThrow(game, "game_title"));

		if (game.HasMember("overview") && game["overview"].IsString())
			result.mdl.set(MetaDataId::Desc, game["overview"].GetString());

		if (game.HasMember("release_date") && game["release_date"].IsString())
			result.mdl.set(MetaDataId::ReleaseDate, Utils::Time::DateTime(Utils::Time::stringToTime(game["release_date"].GetString(), "%Y-%m-%d")));

		if (game.HasMember("developers") && game["developers"].IsArray())
			result.mdl.set(MetaDataId::Developer, getDeveloperString(game["developers"]));

		if (game.HasMember("publishers") && game["publishers"].IsArray())
			result.mdl.set(MetaDataId::Publisher, getPublisherString(game["publishers"]));

		if (game.HasMember("genres") && game["genres"].IsArray())
			result.mdl.set(MetaDataId::Genre, getGenreString(game["genres"]));

		if (game.HasMember("players") && game["players"].IsInt())
			result.mdl.set(MetaDataId::Players, std::to_string(game["players"].GetInt()));

		result.mdl.set(MetaDataId::Rating, "-1");

		std::string id = std::to_string(getIntOrThrow(game, "id"));
		std::map<std::string, std::string> medias;

		if (boxart.HasMember("data") && boxart["data"].IsObject())
		{
			if (boxart["data"].HasMember(id.c_str()))
			{
				std::string image = getBoxartImage(boxart["data"][id.c_str()]);
				if (!image.empty())
					medias["boxart"] = baseImageUrlThumb + image;
			}
		}

		const std::string apiKey = std::string("apikey=") + resources.getApiKey();
		HttpReq req("https://api.thegamesdb.net/v1/Games/Images?" + apiKey + "&games_id=" + id);
		if (req.wait())
		{
			Document imageDoc;
			auto json = req.getContent();
			imageDoc.Parse(json.c_str());

			std::string pathMedium;
			std::string pathLarge;

			if (imageDoc.HasMember("data"))
			{
				if (imageDoc["data"].HasMember("base_url") && imageDoc["data"]["base_url"].HasMember("medium"))
					pathMedium = imageDoc["data"]["base_url"]["medium"].GetString();

				if (imageDoc["data"].HasMember("base_url") && imageDoc["data"]["base_url"].HasMember("large"))
					pathLarge = imageDoc["data"]["base_url"]["large"].GetString();

				if (imageDoc["data"].HasMember("images") && imageDoc["data"]["images"].HasMember(id.c_str()) && imageDoc["data"]["images"][id.c_str()].IsArray())
				{
					const Value& images = imageDoc["data"]["images"][id.c_str()];

					for (int i = 0; i < (int)images.Size(); ++i)
					{
						auto& v = images[i];

						std::string type;
						std::string fileName;
						std::string side;

						if (v.HasMember("type"))
							type = v["type"].GetString();

						if (v.HasMember("filename"))
							fileName = v["filename"].GetString();

						if (v.HasMember("side") && v["side"].IsString())
							side = v["side"].GetString();

						if (type == "boxart" && side == "back")
							type = "box-2D-back";

						if (medias.find(type) == medias.cend())
							medias[type] = (type == "fanart" ? pathLarge : pathMedium) + fileName;
					}
				}
			}
		}

		// Process medias
		auto art = findMedia(medias, Settings::getInstance()->getString("ScrapperImageSrc"));
		if (!art.empty())
			result.urls[MetaDataId::Image] = ScraperSearchItem(art);

		if (!Settings::getInstance()->getString("ScrapperThumbSrc").empty() && Settings::getInstance()->getString("ScrapperThumbSrc") != Settings::getInstance()->getString("ScrapperImageSrc"))
		{
			auto art = findMedia(medias, Settings::getInstance()->getString("ScrapperThumbSrc"));
			if (!art.empty())
				result.urls[MetaDataId::Thumbnail] = ScraperSearchItem(art);
		}

		if (!Settings::getInstance()->getString("ScrapperLogoSrc").empty())
		{
			auto art = findMedia(medias, Settings::getInstance()->getString("ScrapperLogoSrc"));
			if (!art.empty())
				result.urls[MetaDataId::Marquee] = ScraperSearchItem(art);
		}

		if (Settings::getInstance()->getBool("ScrapeTitleShot"))
		{
			auto art = findMedia(medias, "sstitle");
			if (!art.empty())
				result.urls[MetaDataId::TitleShot] = ScraperSearchItem(art);
		}

		if (Settings::getInstance()->getBool("ScrapeFanart"))
		{
			auto art = findMedia(medias, "fanart");
			if (!art.empty())
				result.urls[MetaDataId::FanArt] = ScraperSearchItem(art);
		}
		
		if (Settings::getInstance()->getBool("ScrapeBoxBack"))
		{
			auto art = findMedia(medias, "box-2D-back");
			if (!art.empty())
				result.urls[MetaDataId::BoxBack] = ScraperSearchItem(art);
		}

		results.push_back(result);
	}
} // namespace

  // Process should return false only when we reached a maximum scrap by minute, to retry
bool TheGamesDBJSONRequest::process(HttpReq* request, std::vector<ScraperSearchResult>& results)
{
	if (request->status() != HttpReq::REQ_SUCCESS)
		return false;

	Document doc;
	doc.Parse(request->getContent().c_str());

	if (doc.HasParseError())
	{
		std::string err =
			std::string("TheGamesDBJSONRequest - Error parsing JSON. \n\t") + GetParseError_En(doc.GetParseError());
		setError(err);
		LOG(LogError) << err;
		return true;
	}

	if (!doc.HasMember("data") || !doc["data"].HasMember("games") || !doc["data"]["games"].IsArray())
	{
		std::string warn = "TheGamesDBJSONRequest - Response had no game data.\n";
		LOG(LogWarning) << warn;
		return true;
	}
	const Value& games = doc["data"]["games"];

	if (!doc.HasMember("include") || !doc["include"].HasMember("boxart"))
	{
		std::string warn = "TheGamesDBJSONRequest - Response had no include boxart data.\n";
		LOG(LogWarning) << warn;
		return true;
	}

	const Value& boxart = doc["include"]["boxart"];

	if (!boxart.HasMember("base_url") || !boxart.HasMember("data") || !boxart.IsObject())
	{
		std::string warn = "TheGamesDBJSONRequest - Response include had no usable boxart data.\n";
		LOG(LogWarning) << warn;
		return true;
	}

	resources.ensureResources();
	
	for (int i = 0; i < (int)games.Size(); ++i)
	{
		auto& v = games[i];

		try
		{
			processGame(v, boxart, results);
		}
		catch (std::runtime_error& e)
		{
			LOG(LogError) << "Error while processing game: " << e.what();
		}
	}

	return true;
}

#endif