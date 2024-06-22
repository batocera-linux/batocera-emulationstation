#ifdef HFS_DEV_LOGIN

#include "scrapers/HfsDBScraper.h"
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

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

using namespace PlatformIds;
using namespace rapidjson;

namespace httplib { namespace detail { extern std::string base64_encode(const std::string &in); } } // see declaration in httplib.h

const std::map<PlatformId, std::string> hfsdb_platformids
{ 
	{ ARCADE, "29794" }, // Has special filtering
	{ TEKNOPARROT, "29794" },

	{ THREEDO, "82826" },
	{ NINTENDO_3DS, "148845" },
	{ ADAM, "240844" },
	{ AMIGA, "36003" },
	{ AMIGACD32, "136126" },
	{ AMIGACDTV, "35808" },
	{ AMIGA, "36003" },
	{ AMSTRAD_CPC, "239404" },
	{ APPLE2GS, "240837" },
	{ APPLE_II, "139807" },
	{ ARCHIMEDES, "137569" },
	{ ASTROCADE, "75808" },
	{ ATARI_2600, "84224" },
	{ ATARI_5200, "84223" },
	{ ATARI_7800, "84225" },
	{ ATARI_800, "89769" },
	{ ATARI_JAGUAR, "84220" },
	{ ATARI_JAGUAR_CD, "84220" },
	{ ATARI_LYNX, "33756" },
	{ ATARI_ST, "82160" },
	{ ATARI_XE, "240841" },
	{ ATOMISWAVE, "29731" },
	{ ACORN_ATOM, "138925" },
	{ ACORN_BBC_MICRO, "190502" },
	{ ACORN_ELECTRON, "124274"},
	{ VIC20, "174662" },
	{ COMMODORE_64, "168000" },
	{ CAVESTORY, "75768" },
	{ PHILIPS_CDI, "190532" },
	{ CHANNELF, "35853" },
	{ COLECOVISION, "83499" },
	{ PC, "82805" },
	{ MOONLIGHT, "82805,84756" }, // "Windows"	
	{ SEGA_DREAMCAST, "83702" },
	{ EASYRPG, "82805" },
//	{ FLASH, "139081" },
	{ FMTOWNS, "238067" },
	{ NINTENDO_GAME_AND_WATCH, "190542" },
	{ NINTENDO_GAMECUBE, "82825" },
	{ SEGA_GAME_GEAR, "33807" },
	{ GAME_BOY_ADVANCE, "89234" },
	{ GAME_BOY_COLOR, "89235" },
	{ GAME_BOY, "36022" },
	{ SEGA_GENESIS, "33149" },
	{ GX4000, "30250" },
	{ INTELLIVISION, "84444" },
	{ SEGA_MASTER_SYSTEM, "82166" },
	{ SEGA_PICO, "175183" },
	{ THOMSON_TO_MO, "183673" },
	{ MSX, "113089" },
	{ NOKIA_NGAGE, "128704" },
	{ NINTENDO_64, "32765" },
	{ NINTENDO_64_DISK_DRIVE, "33139" },
	{ NAOMI, "36453" },
	{ NINTENDO_DS, "98132" },
	{ NEOGEO_CD, "237944" },
	{ NEOGEO, "29794" },
	{ NINTENDO_ENTERTAINMENT_SYSTEM, "36012" },
	{ NEOGEO_POCKET_COLOR, "33980" },
	{ NEOGEO_POCKET, "33980" },
	{ ORICATMOS, "240911" },
	{ PCFX, "240904" },
	{ PC_88, "240898" },
	{ PC_98, "240901" },
	{ TURBOGRAFX_16, "30273" },
	{ TURBOGRAFX_CD, "138489" },
	{ COMMODORE_PET, "240850" },
	{ COMMODORE_PLUS4, "240851" },
	{ PLAYSTATION_3, "91841" },
	{ PLAYSTATION_2, "83494" },
	{ PLAYSTATION_VITA, "91844" },
	{ PLAYSTATION_PORTABLE, "91845" },
	{ PLAYSTATION, "82937" },
	{ SATELLAVIEW, "190500" },
	{ SEGA_SATURN, "82924" },
	{ SUPER_CASSETTE_VISION, "189145" },
	{ SCUMMVM, "82805" },
	{ SEGA_32X, "33731" },
	{ SEGA_CD, "83493" },
	{ SEGA_SG1000, "75225" },
	{ SUPER_NINTENDO, "82351" },
	{ SUPER_NINTENDO_MSU1, "82351" },
	{ SOLARUS, "82805" },
	{ SUFAMITURBO, "190497" },	
	{ WATARA_SUPERVISION, "190515" },
	{ SUPER_GAME_BOY, "36022" },
	{ SUPERGRAFX, "30443" },
	{ NINTENDO_SWITCH, "83485" },
	{ TI99, "240924" },	
	{ TRS80_COLOR_COMPUTER, "240922" },
	{ VECTREX, "80424" },
	{ VIDEOPAC_ODYSSEY2, "35882" },	
	{ NINTENDO_VIRTUAL_BOY, "36396" },
	{ NINTENDO_WII_U, "88837" },
	{ NINTENDO_WII, "89530" },
	{ WONDERSWAN_COLOR, "36345" },
	{ WONDERSWAN, "36312" },
	{ SHARP_X1, "248941" },
	{ SHARP_X6800, "239578" },
	{ XBOX_360, "145692" },
	{ XBOX, "144719" },
	{ ZX81, "190538" },
	{ ZX_SPECTRUM, "100295" },
	{ FUJITSU_FM7, "240860" },
	{ CASIO_PV1000, "190512" },
	{ ENTEX_ADVENTURE_VISION, "35850" },
	{ EMERSON_ARCADIA_2001, "129592" },
	{ VTECH_CREATIVISION, "190291" },
	{ HARTUNG_GAME_MASTER, "240872" },
	{ CREATONIC_MEGA_DUCK, "239486" },
	{ FUNTECH_SUPER_A_CAN, "240863" },
	{ TOMY_TUTOR, "240926" },
	{ APF_MP_1000, "139722" },
	{ CAMPUTER_LYNX, "240842" },
	{ EPOCH_GAMEPOCKET, "190501" },
	{ GP32, "240869"},
	{ VTECH_SOCRATES, "240929"},
	{ SEGA_CHIHIRO, "75535"}
};

const std::set<Scraper::ScraperMediaSource>& HfsDBScraper::getSupportedMedias()
{
	static std::set<ScraperMediaSource> mdds =
	{
		ScraperMediaSource::Screenshot,
		ScraperMediaSource::Box2d,
		ScraperMediaSource::Box3d,
		ScraperMediaSource::Marquee,
		ScraperMediaSource::Wheel,
		ScraperMediaSource::TitleShot,
		ScraperMediaSource::Video,
		ScraperMediaSource::FanArt,
		ScraperMediaSource::Manual,
		ScraperMediaSource::BoxBack
	};

	return mdds;
}

void HfsDBScraper::generateRequests(const ScraperSearchParams& params, std::queue<std::unique_ptr<ScraperRequest>>& requests, std::vector<ScraperSearchResult>& results)
{
	if (mToken.empty() || Utils::Time::DateTime::now().elapsedSecondsSince(mTokenDate) > 1800) // make Token expire after 30mn
	{
		mToken = "";

		std::string token;

		HttpReqOptions options;

		std::string login = HFS_DEV_LOGIN;
		
		auto idx = login.find(":");
		if (idx == std::string::npos)
			return; // Bad format ?

		std::string user = login.substr(0, idx);
		std::string pass = login.substr(idx+1);
		
		std::string basicAuth = httplib::detail::base64_encode(HFS_DEV_LOGIN);

		options.dataToPost = "&username="+ user +"&password=" + pass;
		options.customHeaders.push_back("Authorization: Basic "+ basicAuth);

		HttpReq request("https://db.hfsplay.fr/api/v1/auth/token", &options);
		if (request.wait())
		{
			Document doc;
			auto json = request.getContent();
			doc.Parse(json.c_str());

			if (!doc.HasParseError() && doc.HasMember("token"))
				mToken = doc["token"].GetString();
		}
	}

	if (mToken.empty())
		return;

	mTokenDate = Utils::Time::DateTime::now();

	std::string path = "https://db.hfsplay.fr/api/v1/";

	std::string cleanName = params.nameOverride;
	if (cleanName.empty())
		cleanName = Utils::String::removeParenthesis(Utils::FileSystem::getStem(params.game->getPath()));
	
	std::vector<std::string> urls;
	
	bool useMd5 = false;

	if (params.nameOverride.length() == 0)
	{	
		if (params.system->hasPlatformId(PlatformIds::ARCADE))
		{
			urls.push_back(path + "games?medias__description=" + HttpReq::urlEncode(cleanName) + "&system_id=29794");
			useMd5 = true;
		}
		/*else
		{
			std::string fileNameToHash = params.game->getFullPath();
			auto length = Utils::FileSystem::getFileSize(fileNameToHash);

			if (length > 1024 * 1024 && !params.game->getMetadata(MetaDataId::Md5).empty()) // 1Mb
			{
				useMd5 = true;
				urls.push_back(path + "games?medias__md5=" + params.game->getMetadata(MetaDataId::Md5));
			}
			else if (length > 0 && length <= 131072 * 1024) // 128 Mb max
			{
				std::string val = ApiSystem::getInstance()->getMD5(fileNameToHash, params.system->shouldExtractHashesFromArchives());
				if (!val.empty())
				{
					useMd5 = true;
					params.game->setMetadata(MetaDataId::Md5, val);
					urls.push_back(path + "games?medias__md5=" + val);
				}
			}
		}*/
	}

	if (!useMd5)
	{
		if (!params.system->hasPlatformId(PlatformIds::ARCADE)) // Do special post-processing for arcade systems
		{
			auto& platforms = params.system->getPlatformIds();
			if (!platforms.empty())
			{
				for (auto platform : platforms)
				{
					auto it = hfsdb_platformids.find(platform);
					if (it != hfsdb_platformids.cend())
					{
						for (auto plaformId : Utils::String::split(it->second, ',', true))
							urls.push_back(path + "games?search=" + HttpReq::urlEncode(cleanName) + "&system=" + HttpReq::urlEncode(Utils::String::trim(plaformId)) + "&limit=5");
					}
				}
			}
		}

		if (urls.size() == 0)
			urls.push_back(path + "games?search=" + HttpReq::urlEncode(cleanName) + "&limit=5");
	}
	
	HttpReqOptions tokenAuth;
	tokenAuth.customHeaders.push_back("Authorization: Token " + mToken);
	
	for (auto url : urls)
		requests.push(std::unique_ptr<ScraperRequest>(new HfsDBRequest(results, url, &tokenAuth, params.system->hasPlatformId(PlatformIds::ARCADE), params.isManualScrape)));
}

bool HfsDBScraper::isSupportedPlatform(SystemData* system)
{
	std::string platformQueryParam;
	auto& platforms = system->getPlatformIds();

	for (auto platform : platforms)
		if (hfsdb_platformids.find(platform) != hfsdb_platformids.cend())
			return true;

	return false;
}

namespace
{
static std::vector<std::string> getMediaTagNames(std::string imageSource)
{
	if (imageSource == "ss" || imageSource == "mixrbv2" || imageSource == "mixrbv1" || imageSource == "mixrbv")
		return { "screenshot/in game", "screenshot/title", "screenshot" };
	
	if (imageSource == "sstitle")
		return { "screenshot/title",  "screenshot/in game", "screenshot" };

	if (imageSource == "box-2D")
		return { "cover2d/front", "cover2d", "artwork/Flyer", "cover3d" }; 
	
	if (imageSource == "box-3D")
		return{ "cover3d", "cover2d/front" };

	if (imageSource == "wheel")
		return { "logo" };

	if (imageSource == "marquee")
		return { "wheel", "artwork/Marquee" };

	if (imageSource == "video")
		return { "video" };

	if (imageSource == "manual")
		return{ "manual" };

	if (imageSource == "fanart")
		return{ "wallpaper", "artwork" };

	if (imageSource == "box-2D-back")
		return{ "cover2d/back" };

	return std::vector<std::string>();
}

static std::string findMedia(const Value& v, std::string scrapeSource)
{
	if (!v.HasMember("medias") || !v["medias"].IsArray())
		return "";	

	std::set<std::string> tags;
	for (auto tag : getMediaTagNames(scrapeSource))
	{
		std::string tagName = tag;
		std::string tagType;

		auto idx = tagName.find("/");
		if (idx != std::string::npos)
		{
			tagType = tagName.substr(idx + 1);
			tagName = tagName.substr(0, idx);
		}

		const Value& medias = v["medias"];
		for (int i = 0; i < (int)medias.Size(); ++i)
		{
			auto& media = medias[i];
			if (!media.HasMember("type") || !media["type"].IsString() || !media.HasMember("file") || !media["file"].IsString())
				continue;

			if (tagName != media["type"].GetString())
				continue;
			
			if (!tagType.empty())
			{
				if (media.HasMember("metadata") && media["metadata"].IsArray())
				{
					const Value& metadatas = media["metadata"];
					for (int i = 0; i < (int)metadatas.Size(); ++i)
					{
						auto& metadata = metadatas[i];

						if (!metadata.HasMember("name") || !metadata["name"].IsString() || !metadata.HasMember("value") || !metadata["value"].IsString())
							continue;

						std::string name = metadata["name"].GetString();
						std::string value = metadata["value"].GetString();
						if ((name == tagName + "type") && value == tagType)
							return media["file"].GetString();
					}
				}
				
				if (media.HasMember("description") && media["description"].IsString())
				{
					auto desc = media["description"].GetString();
					if (desc == tagType)
						return media["file"].GetString();
				}

				continue;
			}

			return media["file"].GetString();
		}
	}

	return "";
}

static void processGame(const Value& game, std::vector<ScraperSearchResult>& results, bool arcadeGame)
{
	// Special post-processing filtering for ARCADE systems
	if (arcadeGame && game.HasMember("system"))
	{
		const Value& system = game["system"];
		if (system.HasMember("id") && system["id"].IsInt())
		{
			int systemId = system["id"].GetInt();
				
			for (auto item : hfsdb_platformids)
				if (item.first != ARCADE && 
					item.first != NEOGEO && 
					item.first != MODEL3 &&
					Utils::String::toInteger(item.second) == systemId)
					return;
		}
	}

	std::string language = SystemConf::getInstance()->get("system.language");
	if (language.empty())
		language = "en";
	else
	{
		auto shortNameDivider = language.find("_");
		if (shortNameDivider != std::string::npos)
			language = Utils::String::toLower(language.substr(0, shortNameDivider));
	}

	ScraperSearchResult result("HfsDB");

	std::vector<std::string> langs = { language, "en" };
	for (auto lang : langs)
	{
		std::string name = "name_" + lang;
		if (game.HasMember(name.c_str()) && game[name.c_str()].IsString())
		{
			result.mdl.set(MetaDataId::Name, game[name.c_str()].GetString());
			break;
		}
	}

	for (auto lang : langs)
	{
		std::string desc = "description_" + lang;
		if (game.HasMember(desc.c_str()) && game[desc.c_str()].IsString())
		{
			result.mdl.set(MetaDataId::Desc, game[desc.c_str()].GetString());
			break;
		}
	}

	if (game.HasMember("metadata") && game["metadata"].IsArray())
	{
		const Value& metadatas = game["metadata"];
		for (int i = 0; i < (int)metadatas.Size(); ++i)
		{
			auto& metadata = metadatas[i];
						
			if (!metadata.HasMember("name") || !metadata["name"].IsString())
				continue;

			if (!metadata.HasMember("value") || !metadata["value"].IsString())
				continue;

			std::string name = metadata["name"].GetString();
			std::string value = metadata["value"].GetString();
			if (value.empty())
				continue;

			if (name == "genre")
				result.mdl.set(MetaDataId::Genre, value);
			else if (name == "players")
			{
				value = Utils::String::replace(value, " joueurs", "");
				value = Utils::String::replace(value, " joueur", "");
				value = Utils::String::replace(value, "+ de ", "");

				result.mdl.set(MetaDataId::Players, value);
			}
			else if (name == "editor")
				result.mdl.set(MetaDataId::Publisher, value);
			else if (name == "developer")
				result.mdl.set(MetaDataId::Developer, value);
			else if (name == "manufacturer")
				result.mdl.set(MetaDataId::Publisher, value);
			else if (name != "system" && name != "language")
			{
				LOG(LogDebug) << "unknown metadata : " << name << " = " << value;
			}
		}
	}


	Utils::Time::DateTime releaseDate;	

	for (auto release : { "released_at_WORLD", "released_at_US", "released_at_PAL", "released_at_JPN" })
	{
		if (!game.HasMember(release) || !game[release].IsString())
			continue;

		Utils::Time::DateTime dt = Utils::Time::stringToTime(game[release].GetString(), "%Y-%m-%dT%H:%M:%S");
		if (dt.isValid() && !releaseDate.isValid())
		{
			releaseDate = dt;
			break;
		}
	}

	if (releaseDate.isValid())
		result.mdl.set(MetaDataId::ReleaseDate, releaseDate);

	result.mdl.set(MetaDataId::Rating, "-1");

	// Process medias
	
	auto art = findMedia(game, Settings::getInstance()->getString("ScrapperImageSrc"));
	if (!art.empty())
		result.urls[MetaDataId::Image] = ScraperSearchItem(art, Utils::FileSystem::getExtension(art));
		
	if (!Settings::getInstance()->getString("ScrapperThumbSrc").empty() && Settings::getInstance()->getString("ScrapperThumbSrc") != Settings::getInstance()->getString("ScrapperImageSrc"))
	{
		auto art = findMedia(game, Settings::getInstance()->getString("ScrapperThumbSrc"));
		if (!art.empty())
			result.urls[MetaDataId::Thumbnail] = ScraperSearchItem(art, Utils::FileSystem::getExtension(art));
	}

	if (!Settings::getInstance()->getString("ScrapperLogoSrc").empty())
	{
		auto art = findMedia(game, Settings::getInstance()->getString("ScrapperLogoSrc"));
		if (!art.empty())
			result.urls[MetaDataId::Marquee] = ScraperSearchItem(art, Utils::FileSystem::getExtension(art));
	}

	if (Settings::getInstance()->getBool("ScrapeTitleShot"))
	{
		auto art = findMedia(game, "sstitle");
		if (!art.empty())
			result.urls[MetaDataId::TitleShot] = ScraperSearchItem(art, Utils::FileSystem::getExtension(art));
	}

	if (Settings::getInstance()->getBool("ScrapeVideos"))
	{
		auto art = findMedia(game, "video");
		if (!art.empty())
			result.urls[MetaDataId::Video] = ScraperSearchItem(art);
	}
	
	if (Settings::getInstance()->getBool("ScrapeManual"))
	{
		auto art = findMedia(game, "manual");
		if (!art.empty())
			result.urls[MetaDataId::Manual] = ScraperSearchItem(art);
	}

	if (Settings::getInstance()->getBool("ScrapeFanart"))
	{
		auto art = findMedia(game, "fanart");
		if (!art.empty())
			result.urls[MetaDataId::FanArt] = ScraperSearchItem(art, Utils::FileSystem::getExtension(art));
	}

	if (Settings::getInstance()->getBool("ScrapeBoxBack"))
	{
		auto art = findMedia(game, "box-2D-back");
		if (!art.empty())
			result.urls[MetaDataId::BoxBack] = ScraperSearchItem(art);
	}

	results.push_back(result);
}
} // namespace

  // Process should return false only when we reached a maximum scrap by minute, to retry
bool HfsDBRequest::process(HttpReq* request, std::vector<ScraperSearchResult>& results)
{
	assert(request->status() == HttpReq::REQ_SUCCESS);

	Document doc;
	doc.Parse(request->getContent().c_str());

	if (doc.HasParseError())
	{
		std::string err = std::string("HfsDBRequest - Error parsing JSON. \n\t") + GetParseError_En(doc.GetParseError());
		setError(err);
		LOG(LogError) << err;
		return true;
	}

    if (!doc.HasMember("count") || !doc["count"].IsInt() || doc["count"].GetInt() <= 0)
    {
        std::string warn = "HfsDBRequest - Response had no game data.\n";
        LOG(LogWarning) << warn;
        return true;
    }

	if (!doc.HasMember("results") || !doc["results"].IsArray())
	{
		std::string warn = "HfsDBRequest - Response results is not an array.\n";
		LOG(LogWarning) << warn;
		return true;
	}

    const Value& games = doc["results"];

	for (int i = 0; i < (int)games.Size(); ++i)
	{
		auto& v = games[i];

		try
		{
			processGame(v, results, mIsArcade);

			if (request->getUrl().find("medias__description=") != std::string::npos)
				break;
		}
		catch (std::runtime_error& e)
		{
			LOG(LogError) << "Error while processing game: " << e.what();
		}
	}

	return true;
}

#endif
