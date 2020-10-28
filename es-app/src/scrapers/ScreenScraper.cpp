#include "scrapers/ScreenScraper.h"

#include "utils/TimeUtil.h"
#include "utils/StringUtil.h"
#include "FileData.h"
#include "Log.h"
#include "PlatformId.h"
#include "Settings.h"
#include "SystemData.h"
#include <pugixml/src/pugixml.hpp>
#include <cstring>
#include "SystemConf.h"
#include "md5.h"
#include <thread>
#include "LangParser.h"

using namespace PlatformIds;

std::string ScreenScraperRequest::ensureUrl(const std::string url)
{
	return Utils::String::replace(
		Utils::String::replace(url, " ", "%20") ,
		"#screenscraperserveur#", "https://www.screenscraper.fr/");
}

/**
	List of systems and thein IDs from
	https://www.screenscraper.fr/api/systemesListe.php?devid=xxx&devpassword=yyy&softname=zzz&output=XML
**/
const std::map<PlatformId, unsigned short> screenscraper_platformid_map{
	{ THREEDO, 29 },
	{ AMIGA, 64 },
	{ AMSTRAD_CPC, 65 },
	{ APPLE_II, 86 },
	{ ARCADE, 75 },
	{ ATARI_800, 43 },
	{ ATARI_2600, 26 },
	{ ATARI_5200, 40 },
	{ ATARI_7800, 41 },
	{ ATARI_JAGUAR, 27 },
	{ ATARI_JAGUAR_CD, 171 },
	{ ATARI_LYNX, 28 },
	{ ATARI_ST, 42},
	// missing Atari XE ?
	{ COLECOVISION, 48 },
	{ COMMODORE_64, 66 },
	{ INTELLIVISION, 115 },
	{ MAC_OS, 146 },
	{ XBOX, 32 },
	{ XBOX_360, 33 },
	{ MSX, 113 },
	{ NEOGEO, 142 },
	{ NEOGEO_POCKET, 25},
	{ NEOGEO_POCKET_COLOR, 82 },
	{ NINTENDO_3DS, 17 },
	{ NINTENDO_64, 14 },
	{ NINTENDO_DS, 15 },
	{ FAMICOM_DISK_SYSTEM, 106 },
	{ NINTENDO_ENTERTAINMENT_SYSTEM, 3 },
	{ GAME_BOY, 9 },
	{ GAME_BOY_ADVANCE, 12 },
	{ GAME_BOY_COLOR, 10 },
	{ NINTENDO_GAMECUBE, 13 },
	{ NINTENDO_WII, 16 },
	{ NINTENDO_WII_U, 18 },
	{ NINTENDO_VIRTUAL_BOY, 11 },
	{ NINTENDO_GAME_AND_WATCH, 52 },
	{ PC, 135 },
	{ PC_88, 221},
	{ PC_98, 208},
	{ SCUMMVM, 123},
	{ SEGA_32X, 19 },
	{ SEGA_CD, 20 },
	{ SEGA_DREAMCAST, 23 },
	{ SEGA_GAME_GEAR, 21 },
	{ SEGA_GENESIS, 1 },
	{ SEGA_MASTER_SYSTEM, 2 },
	{ SEGA_MEGA_DRIVE, 1 },
	{ SEGA_SATURN, 22 },
	{ SEGA_SG1000, 109 },
	{ SHARP_X1, 220 },
	{ SHARP_X6800, 79},
	{ PLAYSTATION, 57 },
	{ PLAYSTATION_2, 58 },
	{ PLAYSTATION_3, 59 },
	// missing Sony Playstation 4 ?
	{ PLAYSTATION_VITA, 62 },
	{ PLAYSTATION_PORTABLE, 61 },
	{ SUPER_NINTENDO, 4 },
	{ TURBOGRAFX_16, 31 },
	{ TURBOGRAFX_CD, 114 },
	{ WONDERSWAN, 45 },
	{ WONDERSWAN_COLOR, 46 },
	{ ZX_SPECTRUM, 76 },
	{ VIDEOPAC_ODYSSEY2, 104 },
	{ VECTREX, 102 },
	{ TRS80_COLOR_COMPUTER, 144 },
	{ TANDY, 144 },	
	{ SUPERGRAFX, 105 },
	
	{ AMIGACD32, 130 },
	{ AMIGACDTV, 129 },
	{ ATOMISWAVE, 53 },
	{ CAVESTORY, 135 },
	{ GX4000, 87 },
	{ LUTRO, 206 },
	{ NAOMI, 56 },
	{ NEOGEO_CD, 70 },
	{ PCFX, 72 },
	{ POKEMINI, 211 },
	{ PRBOOM, 135 },
	{ SATELLAVIEW, 107 },
	{ SUFAMITURBO, 108 },
	{ ZX81, 77 },
	{ TIC80, 222 },
	{ MOONLIGHT, 138 }, // "PC Windows"

	// Windows
	{ VISUALPINBALL, 198 },
	{ FUTUREPINBALL, 199 },

	// Misc
	{ ORICATMOS, 131 },
	{ CHANNELF, 80 },
	{ THOMSON_TO_MO, 141 },
	{ SAMCOUPE, 213 },
	{ OPENBOR, 214 },
	{ UZEBOX, 216 },
	{ APPLE2GS, 217 },
	{ SPECTRAVIDEO, 218 },
	{ PALMOS, 219 },
	{ DAPHNE, 49 },
	{ SOLARUS, 223 }	
};

// Help XML parsing method, finding an direct child XML node starting from the parent and filtering by an attribute value list.
static pugi::xml_node find_child_by_attribute_list(const pugi::xml_node& node_parent, const std::string& node_name, const std::string& attribute_name, const std::vector<std::string> attribute_values)
{
	for (auto _val : attribute_values)
		for (pugi::xml_node node : node_parent.children(node_name.c_str()))
			if (strcmp(node.attribute(attribute_name.c_str()).value(), _val.c_str()) == 0)
				return node;

	return pugi::xml_node(NULL);
}

void ScreenScraperScraper::generateRequests(const ScraperSearchParams& params,
	std::queue<std::unique_ptr<ScraperRequest>>& requests,
	std::vector<ScraperSearchResult>& results)
{
	std::string path;

	ScreenScraperRequest::ScreenScraperConfig ssConfig;

	// FCA Fix for names override not working on Retropie
	if (params.nameOverride.length() == 0)
	{
		path = ssConfig.getGameSearchUrl(params.game->getFileName());
		path += "&romtype=rom";

		if (!params.game->getMetadata(MetaDataId::Md5).empty())
			path += "&md5=" + params.game->getMetadata(MetaDataId::Md5);
		else
		{
			// Use md5 to search scrapped game
			size_t length = Utils::FileSystem::getFileSize(params.game->getFullPath());
			if (length > 0 && length <= 131072 * 1024) // 128 Mb max
			{
				try
				{
					// 64 Kb blocks
#define MD5BUFFERSIZE 64 * 1024

					char* buffer = new char[MD5BUFFERSIZE];
					if (buffer)
					{
						size_t size;

						FILE* file = fopen(params.game->getFullPath().c_str(), "rb");
						if (file)
						{
							MD5 md5 = MD5();

							while (size = fread(buffer, 1, MD5BUFFERSIZE, file))
								md5.update(buffer, size);

							md5.finalize();

							std::string val = md5.hexdigest();
							if (!val.empty())
							{
								params.game->setMetadata("md5", val);
								path += "&md5=" + val;
							}

							fclose(file);
						}

						delete buffer;
					}
				}
				catch (std::bad_alloc& ex) 
				{
					path += "&romtaille=" + std::to_string(length);
				}
			}
			else
				path += "&romtaille=" + std::to_string(length);
		}
	}
	else
	{
		std::string name = Utils::String::replace(params.nameOverride, "_", " ");
		name = Utils::String::replace(name, "-", " ");		

		path = ssConfig.getGameSearchUrl(name, true);
	}
	
	auto& platforms = params.system->getPlatformIds();
	std::vector<unsigned short> p_ids;

	// Get the IDs of each platform from the ScreenScraper list
	for (auto platformIt = platforms.cbegin(); platformIt != platforms.cend(); platformIt++)
	{
		auto mapIt = screenscraper_platformid_map.find(*platformIt);
		if (mapIt != screenscraper_platformid_map.cend())
			p_ids.push_back(mapIt->second);
		else
		{
			LOG(LogWarning) << "ScreenScraper: no support for platform " << getPlatformName(*platformIt);
			// Add the scrape request without a platform/system ID
			requests.push(std::unique_ptr<ScraperRequest>(new ScreenScraperRequest(requests, results, path, params.game->getFileName())));
		}
	}

	// Sort the platform IDs and remove duplicates
	std::sort(p_ids.begin(), p_ids.end());
	auto last = std::unique(p_ids.begin(), p_ids.end());
	p_ids.erase(last, p_ids.end());

	for (auto platform = p_ids.cbegin(); platform != p_ids.cend(); platform++)
	{
		path += "&systemeid=";
		path += HttpReq::urlEncode(std::to_string(*platform));
		requests.push(std::unique_ptr<ScraperRequest>(new ScreenScraperRequest(requests, results, path, params.game->getFileName())));
	}
}

// Process should return false only when we reached a maximum scrap by minute, to retry
bool ScreenScraperRequest::process(HttpReq* request, std::vector<ScraperSearchResult>& results)
{
	assert(request->status() == HttpReq::REQ_SUCCESS);

	auto content = request->getContent();
	if (content.empty())
		return false;

	if (content.find("<html") == 0)
	{
		setError(Utils::String::removeHtmlTags(content));
		return false;
	}

	pugi::xml_document doc;
	pugi::xml_parse_result parseResult = doc.load(content.c_str());

	if (!parseResult)
	{
		std::stringstream ss;
		ss << "ScreenScraperRequest - Error parsing XML." << std::endl << parseResult.description() << "";
		std::string err = ss.str();
		//setError(err); Don't consider it an error -> Request is a success. Simply : Game is not found		
		LOG(LogWarning) << err;
				
		if (Utils::String::toLower(content).find("maximum threads per minute reached") != std::string::npos)
			return false;
		
		return true;
	}

	processGame(doc, results);
	return true;
}

pugi::xml_node ScreenScraperRequest::findMedia(pugi::xml_node media_list, std::vector<std::string> mediaNames, std::string region)
{
	for (std::string media : mediaNames)
	{
		pugi::xml_node art = findMedia(media_list, media, region);
		if (art)
			return art;
	}

	return pugi::xml_node(NULL);
}

pugi::xml_node ScreenScraperRequest::findMedia(pugi::xml_node media_list, std::string mediaName, std::string region)
{
	pugi::xml_node art = pugi::xml_node(NULL);

	// Do an XPath query for media[type='$media_type'], then filter by region
	// We need to do this because any child of 'medias' has the form
	// <media type="..." region="..." format="..."> 
	// and we need to find the right media for the region.

	pugi::xpath_node_set results = media_list.select_nodes((static_cast<std::string>("media[@type='") + mediaName + "']").c_str());
	if (!results.size())
		return art;

	// Region fallback: WOR(LD), US, CUS(TOM?), JP, EU
	for (auto _region : std::vector<std::string>{ region, "wor", "us", "eu", "jp", "ss", "cus", "" })
	{
		if (art)
			break;

		for (auto node : results)
		{
			if (node.node().attribute("region").value() == _region)
			{
				art = node.node();
				break;
			}
		}
	}

	return art;
}

std::vector<std::string> ScreenScraperRequest::getRipList(std::string imageSource)
{
	std::vector<std::string> ripList;

	if (imageSource == "ss")
		ripList = { "ss", "sstitle", "mixrbv1", "mixrbv2", "box-2D", "box-3D" };
	else if (imageSource == "sstitle")
		ripList = { "sstitle", "ss", "mixrbv1", "mixrbv2", "box-2D", "box-3D" };
	else if (imageSource == "mixrbv1" || imageSource == "mixrbv2" || imageSource == "mixrbv")
		ripList = { "mixrbv1", "mixrbv2", "ss", "box-3D", "box-2D" };
	else if (imageSource == "box-2D")
		ripList = { "box-2D", "box-3D" };
	else if (imageSource == "box-3D")
		ripList = { "box-3D", "box-2D" };
	else if (imageSource == "wheel")
		ripList = { "wheel", "wheel-hd", "wheel-steel", "wheel-carbon", "screenmarqueesmall", "screenmarquee" };
	else if (imageSource == "marquee")
		ripList = { "screenmarqueesmall", "screenmarquee", "wheel", "wheel-hd", "wheel-steel", "wheel-carbon" };
	else if (imageSource == "video")
		ripList = { "video-normalized", "video" };
	else 
		ripList = { imageSource };

	return ripList;
}

void ScreenScraperRequest::processGame(const pugi::xml_document& xmldoc, std::vector<ScraperSearchResult>& out_results)
{
	LOG(LogDebug) << "ScreenScraperRequest::processGame >>";

	pugi::xml_node data = xmldoc.child("Data");
	if (data.child("jeux"))
		data = data.child("jeux");

	for (pugi::xml_node game = data.child("jeu"); game; game = game.next_sibling("jeu"))
	{
		ScraperSearchResult result;
		ScreenScraperRequest::ScreenScraperConfig ssConfig;

		std::string region = Utils::String::toLower(ssConfig.region);

		// Detect ROM region
		auto info = LangInfo::parse(mFileName, nullptr);
		if (!info.region.empty())
			region = info.region;

		std::string language = SystemConf::getInstance()->get("system.language");
		if (language.empty())
			language = "en";
		else
		{
			auto shortNameDivider = language.find("_");
			if (shortNameDivider != std::string::npos)
			{
				if (info.region.empty())
					region = Utils::String::toLower(language.substr(shortNameDivider + 1));

				language = Utils::String::toLower(language.substr(0, shortNameDivider));
			}

			// Region fix
			if (info.region.empty() && (shortNameDivider != std::string::npos || region == "US"))
			{
				if (language == "es" && region == "mx")
					region = "mx";
				else if (language == "pt" && region == "br")
					region = "br";
				else if (language == "fr" || language == "es" || language == "ca" || language == "el" || language == "hu" || language == "it" || language == "sv" || language == "uk" || language == "gr" || language == "no" || language == "sw" || language == "nl" || language == "de")
					region = "eu";
			}
		}

		// Name fallback: US, WOR(LD). ( Xpath: Data/jeu[0]/noms/nom[*] ). 
		result.mdl.set("name", find_child_by_attribute_list(game.child("noms"), "nom", "region", { region, "wor", "us" , "ss", "eu", "jp" }).text().get());

		// Description fallback language: EN, WOR(LD)
		std::string description = find_child_by_attribute_list(game.child("synopsis"), "synopsis", "langue", { language, "en", "wor" }).text().get();

		if (!description.empty())
			result.mdl.set("desc", Utils::String::replace(description, "&nbsp;", " "));

		// Genre fallback language: EN. ( Xpath: Data/jeu[0]/genres/genre[*] )
		result.mdl.set("genre", find_child_by_attribute_list(game.child("genres"), "genre", "langue", { language, "en" }).text().get());
		//LOG(LogDebug) << "Genre: " << result.mdl.get("genre");

		// Get the date proper. The API returns multiple 'date' children nodes to the 'dates' main child of 'jeu'.
		// Date fallback: WOR(LD), US, SS, JP, EU
		std::string _date = find_child_by_attribute_list(game.child("dates"), "date", "region", { region, "wor", "us", "ss", "eu", "jp" }).text().get();
		//LOG(LogDebug) << "Release Date (unparsed): " << _date;

		// Date can be YYYY-MM-DD or just YYYY.
		if (_date.length() > 4)
		{
			result.mdl.set("releasedate", Utils::Time::DateTime(Utils::Time::stringToTime(_date, "%Y-%m-%d")));
		} else if (_date.length() > 0)
		{
			result.mdl.set("releasedate", Utils::Time::DateTime(Utils::Time::stringToTime(_date, "%Y")));
		}

		//LOG(LogDebug) << "Release Date (parsed): " << result.mdl.get("releasedate");

		/// Developer for the game( Xpath: Data/jeu[0]/developpeur )
		std::string developer = game.child("developpeur").text().get();
		if (!developer.empty())
			result.mdl.set("developer", Utils::String::replace(developer, "&nbsp;", " "));

		// Publisher for the game ( Xpath: Data/jeu[0]/editeur )
		std::string publisher = game.child("editeur").text().get();
		if (!publisher.empty())
			result.mdl.set("publisher", Utils::String::replace(publisher, "&nbsp;", " "));

		// Players
		result.mdl.set("players", game.child("joueurs").text().get());

        if(game.child("systeme").attribute("id"))
        {
            int systemId = game.child("systeme").attribute("id").as_int();
			
			auto arcadeSystem = ArcadeSystems.find(systemId);
            if(arcadeSystem != ArcadeSystems.cend())
                result.mdl.set("arcadesystemname", arcadeSystem->second.first);
		}

        // TODO: Validate rating
		if (Settings::getInstance()->getBool("ScrapeRatings") && game.child("note"))
		{
			float ratingVal = (game.child("note").text().as_int() / 20.0f);
			std::stringstream ss;
			ss << ratingVal;
			result.mdl.set("rating", ss.str());
		}

		// Media super-node
		pugi::xml_node media_list = game.child("medias");

		if (media_list)
		{
			std::vector<std::string> ripList = getRipList(Settings::getInstance()->getString("ScrapperImageSrc"));
			if (!ripList.empty())
			{
				pugi::xml_node art = findMedia(media_list, ripList, region);
				if (art)
				{
					// Sending a 'softname' containing space will make the image URLs returned by the API also contain the space. 
					//  Escape any spaces in the URL here
					result.urls[MetaDataId::Image] = ScraperSearchItem(ensureUrl(art.text().get()), art.attribute("format") ? "." + std::string(art.attribute("format").value()) : "");

					// Ask for the same image, but with a smaller size, for the thumbnail displayed during scraping
					// result.urls[MetaDataId::Thumbnail] = imageUrl + "&maxheight=250";
				}
				else
					LOG(LogDebug) << "Failed to find media XML node for image";
			}

			if (!Settings::getInstance()->getString("ScrapperThumbSrc").empty() &&
				Settings::getInstance()->getString("ScrapperThumbSrc") != Settings::getInstance()->getString("ScrapperImageSrc"))
			{
				ripList = getRipList(Settings::getInstance()->getString("ScrapperThumbSrc"));
				if (!ripList.empty())
				{
					pugi::xml_node art = findMedia(media_list, ripList, region);
					if (art)
						result.urls[MetaDataId::Thumbnail] = ScraperSearchItem(ensureUrl(art.text().get()), art.attribute("format") ? "." + std::string(art.attribute("format").value()) : "");
					else
						LOG(LogDebug) << "Failed to find media XML node for thumbnail";
				}
			}

			if (!Settings::getInstance()->getString("ScrapperLogoSrc").empty())
			{
				ripList = getRipList(Settings::getInstance()->getString("ScrapperLogoSrc"));
				if (!ripList.empty())
				{
					pugi::xml_node art = findMedia(media_list, ripList, region);
					if (art)
						result.urls[MetaDataId::Marquee] = ScraperSearchItem(ensureUrl(art.text().get()), art.attribute("format") ? "." + std::string(art.attribute("format").value()) : "");
					else
						LOG(LogDebug) << "Failed to find media XML node for video";
				}
			}

			if (Settings::getInstance()->getBool("ScrapeVideos"))
			{
				ripList = getRipList("video");
				if (!ripList.empty())
				{
					pugi::xml_node art = findMedia(media_list, ripList, region);
					if (art)
						result.urls[MetaDataId::Video] = ScraperSearchItem(ensureUrl(art.text().get()), art.attribute("format") ? "." + std::string(art.attribute("format").value()) : "");
					else
						LOG(LogDebug) << "Failed to find media XML node for video";
				}
			}

			if (Settings::getInstance()->getBool("ScrapeFanart"))
			{
				ripList = getRipList("fanart");
				if (!ripList.empty())
				{
					pugi::xml_node art = findMedia(media_list, ripList, region);
					if (art)
						result.urls[MetaDataId::FanArt] = ScraperSearchItem(ensureUrl(art.text().get()), art.attribute("format") ? "." + std::string(art.attribute("format").value()) : "");
					else
						LOG(LogDebug) << "Failed to find media XML node for video";
				}
			}

			if (Settings::getInstance()->getBool("ScrapeManual"))
			{
				ripList = getRipList("manuel");
				if (!ripList.empty())
				{
					pugi::xml_node art = findMedia(media_list, ripList, region);
					if (art)
						result.urls[MetaDataId::Manual] = ScraperSearchItem(ensureUrl(art.text().get()), art.attribute("format") ? "." + std::string(art.attribute("format").value()) : "");
					else
						LOG(LogDebug) << "Failed to find media XML node for video";
				}
			}

			if (Settings::getInstance()->getBool("ScrapeMap"))
			{
				ripList = getRipList("maps");
				if (!ripList.empty())
				{
					pugi::xml_node art = findMedia(media_list, ripList, region);
					if (art)
						result.urls[MetaDataId::Map] = ScraperSearchItem(ensureUrl(art.text().get()), art.attribute("format") ? "." + std::string(art.attribute("format").value()) : "");
					else
						LOG(LogDebug) << "Failed to find media XML node for video";
				}
			}

			if (Settings::getInstance()->getBool("ScrapeTitleShot"))
			{
				ripList = getRipList("sstitle");
				if (!ripList.empty())
				{
					pugi::xml_node art = findMedia(media_list, ripList, region);
					if (art)
						result.urls[MetaDataId::TitleShot] = ScraperSearchItem(ensureUrl(art.text().get()), art.attribute("format") ? "." + std::string(art.attribute("format").value()) : "");
					else
						LOG(LogDebug) << "Failed to find media XML node for video";
				}
			}
		}

		out_results.push_back(result);
	} // game

	LOG(LogDebug) << "ScreenScraperRequest::processGame <<";
}

std::string ScreenScraperRequest::ScreenScraperConfig::getGameSearchUrl(const std::string gameName, bool jeuRecherche) const
{	
	std::string ret =  API_URL_BASE
		+ "/jeuInfos.php?devid=" + Utils::String::scramble(API_DEV_U, API_DEV_KEY)
		+ "&devpassword=" + Utils::String::scramble(API_DEV_P, API_DEV_KEY)
		+ "&softname=" + HttpReq::urlEncode(API_SOFT_NAME)
		+ "&output=xml"
		+ "&romnom=" + HttpReq::urlEncode(gameName);

	if (jeuRecherche)
	{
		ret = API_URL_BASE
			+ "/jeuRecherche.php?devid=" + Utils::String::scramble(API_DEV_U, API_DEV_KEY)
			+ "&devpassword=" + Utils::String::scramble(API_DEV_P, API_DEV_KEY)
			+ "&softname=" + HttpReq::urlEncode(API_SOFT_NAME)
			+ "&output=xml"
			+ "&recherche=" + HttpReq::urlEncode(gameName);
	}

	std::string user = Settings::getInstance()->getString("ScreenScraperUser");
	std::string pass = Settings::getInstance()->getString("ScreenScraperPass");

	if (!user.empty() && !pass.empty())
		ret = ret + "&ssid=" + HttpReq::urlEncode(user) + "&sspassword=" + HttpReq::urlEncode(pass);

	return ret;
}

std::string ScreenScraperRequest::ScreenScraperConfig::getUserInfoUrl() const
{
	std::string ret = API_URL_BASE
		+ "/ssuserInfos.php?devid=" + Utils::String::scramble(API_DEV_U, API_DEV_KEY)
		+ "&devpassword=" + Utils::String::scramble(API_DEV_P, API_DEV_KEY)
		+ "&softname=" + HttpReq::urlEncode(API_SOFT_NAME)
		+ "&output=xml";

	std::string user = Settings::getInstance()->getString("ScreenScraperUser");
	std::string pass = Settings::getInstance()->getString("ScreenScraperPass");

	if (!user.empty() && !pass.empty())
		ret = ret + "&ssid=" + HttpReq::urlEncode(user) + "&sspassword=" + HttpReq::urlEncode(pass);

	return ret;
}

ScreenScraperUser ScreenScraperRequest::processUserInfo(const pugi::xml_document& xmldoc)
{
	LOG(LogDebug) << "ScreenScraperRequest::processUserInfo >>";

	ScreenScraperUser user;

	pugi::xml_node data = xmldoc.child("Data");
	if (!data.child("ssuser"))
		return user;
		
	data = data.child("ssuser");

	if (data.child("id"))
		user.id = data.child("id").text().get();

	if (data.child("maxthreads"))
		user.maxthreads = data.child("maxthreads").text().as_int();

	if (data.child("requeststoday"))
		user.requestsToday = data.child("requeststoday").text().as_int();

	if (data.child("requestskotoday"))
		user.requestsKoToday = data.child("requestskotoday").text().as_int();

	if (data.child("maxrequestspermin"))
		user.maxRequestsPerMin = data.child("maxrequestspermin").text().as_int();

	if (data.child("maxrequestsperday"))
		user.maxRequestsPerDay = data.child("maxrequestsperday").text().as_int();

	if (data.child("maxrequestskoperday"))
		user.maxRequestsKoPerDay = data.child("maxrequestskoperday").text().as_int();
	
	return user;
}

int ScreenScraperScraper::getThreadCount()
{
	ScreenScraperRequest::ScreenScraperConfig ssConfig;
	std::string url = ssConfig.getUserInfoUrl();

	HttpReq httpreq(url);
	httpreq.wait();
	
	auto content = httpreq.getContent();

	pugi::xml_document doc;
	pugi::xml_parse_result parseResult = doc.load(content.c_str());
	if (parseResult)
	{
		auto userInfo = ScreenScraperRequest::processUserInfo(doc);
		
		// userInfo.maxRequestsPerMin / userInfo.maxthreads;

		if (userInfo.maxthreads > 0)
			return userInfo.maxthreads;
	}	

	return 1;
}
