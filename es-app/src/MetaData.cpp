#include "MetaData.h"

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include <pugixml/src/pugixml.hpp>
#include "SystemData.h"
#include "LocaleES.h"
#include "Settings.h"
#include "FileData.h"
#include "ImageIO.h"

std::vector<MetaDataDecl> MetaDataList::mMetaDataDecls;

static std::map<MetaDataId, int> mMetaDataIndexes;
static std::string* mDefaultGameMap = nullptr;
static MetaDataType* mGameTypeMap = nullptr;
static std::map<std::string, MetaDataId> mGameIdMap;

void MetaDataList::initMetadata()
{
	MetaDataDecl gameDecls[] = 
	{
		// key,             type,                   default,            statistic,  name in GuiMetaDataEd,  prompt in GuiMetaDataEd
		{ Name,             "name",        MD_STRING,              "",                 false,      _("Name"),                 _("this game's name"),			true },
	//	{ SortName,         "sortname",    MD_STRING,              "",                 false,      _("sortname"),             _("enter game sort name"),	true },
		{ Desc,             "desc",        MD_MULTILINE_STRING,    "",                 false,      _("Description"),          _("this game's description"),		true },

#if WIN32 && !_DEBUG
		{ Emulator,         "emulator",    MD_LIST,				 "",                 false,       _("Emulator"),			 _("emulator"),					false },
		{ Core,             "core",	      MD_LIST,				 "",                 false,       _("Core"),				 _("core"),						false },
#else
		// Windows & recalbox gamelist.xml compatiblity -> Set as statistic to hide it from metadata editor
		{ Emulator,         "emulator",    MD_LIST,				 "",                 true,        _("Emulator"),			 _("emulator"),					false },
		{ Core,             "core",	     MD_LIST,				 "",                 true,        _("Core"),				 _("core"),						false },
#endif

		{ Image,            "image",       MD_PATH,                "",                 false,      _("Image"),                _("enter path to image"),		 true },
		{ Video,            "video",       MD_PATH,                "",                 false,      _("Video"),                _("enter path to video"),		 false },
		{ Marquee,          "marquee",     MD_PATH,                "",                 false,      _("Logo"),                 _("enter path to logo"),	     true },
		{ Thumbnail,        "thumbnail",   MD_PATH,                "",                 false,      _("Box"),				  _("enter path to box"),		 false },

		{ FanArt,           "fanart",      MD_PATH,                "",                 false,      _("Fan art"),              _("enter path to fanart"),	 true },
		{ TitleShot,        "titleshot",   MD_PATH,                "",                 false,      _("Title shot"),           _("enter path to title shot"), true },
		{ Manual,			"manual",	   MD_PATH,                "",                 false,      _("Manual"),               _("enter path to manual"),     true },
		{ Magazine,			"magazine",	   MD_PATH,                "",                 false,      _("Magazine"),             _("enter path to magazine"),     true },
		{ Map,			    "map",	       MD_PATH,                "",                 false,      _("Map"),                  _("enter path to map"),		 true },
		{ Bezel,            "bezel",       MD_PATH,                "",                 false,      _("Bezel (16:9)"),         _("enter path to bezel (16:9)"),	 true },

		// Non scrappable /editable medias
		{ Cartridge,        "cartridge",   MD_PATH,                "",                 true,       _("Cartridge"),            _("enter path to cartridge"),  true },
		{ BoxArt,			"boxart",	   MD_PATH,                "",                 true,       _("Alt BoxArt"),		      _("enter path to alt boxart"), true },
		{ BoxBack,			"boxback",	   MD_PATH,                "",                 false,      _("Box backside"),		  _("enter path to box background"), true },
		{ Wheel,			"wheel",	   MD_PATH,                "",                 true,       _("Wheel"),		          _("enter path to wheel"),      true },
		{ Mix,			    "mix",	       MD_PATH,                "",                 true,       _("Mix"),                  _("enter path to mix"),		 true },
		
		{ Rating,           "rating",      MD_RATING,              "0.000000",         false,      _("Rating"),               _("enter rating"),			false },
		{ ReleaseDate,      "releasedate", MD_DATE,                "not-a-date-time",  false,      _("Release date"),         _("enter release date"),		false },
		{ Developer,        "developer",   MD_STRING,              "",                 false,      _("Developer"),            _("this game's developer"),	false },
		{ Publisher,        "publisher",   MD_STRING,              "",                 false,      _("Publisher"),            _("this game's publisher"),	false },


		{ Genre,            "genre",       MD_STRING,              "",                 false,      _("Genre"),                _("enter game genre"),		false }, 
		{ Family,           "family",      MD_STRING,              "",                 false,      _("Game family"),		  _("this game's game family"),		false },

		// GenreIds is not serialized
		{ GenreIds,         "genres",      MD_STRING,              "",                 false,      _("Genres"),				  _("enter game genres"),		false },

		{ ArcadeSystemName, "arcadesystemname",  MD_STRING,        "",                 false,      _("Arcade system"),        _("this game's arcade system"), false },

		{ Players,          "players",     MD_INT,                 "",                false,       _("Players"),              _("this game's number of players"),	false },
		{ Favorite,         "favorite",    MD_BOOL,                "false",            false,      _("Favorite"),             _("enter favorite"),			false },
		{ Hidden,           "hidden",      MD_BOOL,                "false",            false,      _("Hidden"),               _("enter hidden"),			true },
		{ KidGame,          "kidgame",     MD_BOOL,                "false",            false,      _("Kidgame"),              _("enter kidgame"),			false },
		{ PlayCount,        "playcount",   MD_INT,                 "0",                true,       _("Play count"),           _("enter number of times played"), false },
		{ LastPlayed,       "lastplayed",  MD_TIME,                "0",                true,       _("Last played"),          _("enter last played date"), false },

		{ Crc32,            "crc32",       MD_STRING,              "",                 true,       _("Crc32"),                _("Crc32 checksum"),			false },
		{ Md5,              "md5",		   MD_STRING,              "",                 true,       _("Md5"),                  _("Md5 checksum"),			false },

		{ GameTime,         "gametime",    MD_INT,                 "0",                true,       _("Game time"),            _("how long the game has been played in total (seconds)"), false },

		{ Language,         "lang",        MD_STRING,              "",                 false,      _("Languages"),            _("this game's languages"),				false },
		{ Region,           "region",      MD_STRING,              "",                 false,      _("Region"),               _("this game's region"),					false },

		{ CheevosHash,      "cheevosHash", MD_STRING,              "",                 true,       _("Cheevos Hash"),          _("Cheevos checksum"),	    false },
		{ CheevosId,        "cheevosId",   MD_INT,                 "",				   true,       _("Cheevos Game ID"),       _("Cheevos Game ID"),		false },

		{ ScraperId,        "id",		   MD_INT,                 "",				   true,       _("Screenscraper Game ID"), _("Screenscraper Game ID"),	false, true }
	};
	
	mMetaDataDecls = std::vector<MetaDataDecl>(gameDecls, gameDecls + sizeof(gameDecls) / sizeof(gameDecls[0]));
	
	mMetaDataIndexes.clear();
	for (int i = 0 ; i < mMetaDataDecls.size() ; i++)
		mMetaDataIndexes[mMetaDataDecls[i].id] = i;

	int maxID = mMetaDataDecls.size() + 1;

	if (mDefaultGameMap != nullptr) 
		delete[] mDefaultGameMap;

	if (mGameTypeMap != nullptr) 
		delete[] mGameTypeMap;

	mDefaultGameMap = new std::string[maxID];
	mGameTypeMap = new MetaDataType[maxID];

	for (int i = 0; i < maxID; i++)
		mGameTypeMap[i] = MD_STRING;
		
	for (auto iter = mMetaDataDecls.cbegin(); iter != mMetaDataDecls.cend(); iter++)
	{
		mDefaultGameMap[iter->id] = iter->defaultValue;
		mGameTypeMap[iter->id] = iter->type;
		mGameIdMap[iter->key] = iter->id;
	}
}

MetaDataType MetaDataList::getType(MetaDataId id) const
{
	return mGameTypeMap[id];
}

MetaDataType MetaDataList::getType(const std::string name) const
{
	return getType(getId(name));
}

MetaDataId MetaDataList::getId(const std::string& key) const
{
	return mGameIdMap[key];
}

MetaDataList::MetaDataList(MetaDataListType type) : mType(type), mWasChanged(false), mRelativeTo(nullptr)
{

}

MetaDataList MetaDataList::createFromXML(MetaDataListType type, pugi::xml_node& node, SystemData* system)
{
	MetaDataList mdl(type);
	mdl.mRelativeTo = system;
	std::string value;
	std::string relativeTo = mdl.mRelativeTo->getStartPath();

	bool preloadMedias = Settings::PreloadMedias();

	for (pugi::xml_node xelement : node.children())
	{
		std::string name = xelement.name();
		auto it = mGameIdMap.find(name);
		if (it == mGameIdMap.cend())
		{
			if (name == "hash" || name == "path")
				continue;

			value = xelement.text().get();
			if (!value.empty())
				mdl.mUnKnownElements.push_back(std::tuple<std::string, std::string, bool>(name, value, true));

			continue;
		}

		MetaDataDecl& mdd = mMetaDataDecls[mMetaDataIndexes[it->second]];
		if (mdd.isAttribute)
			continue;

		value = xelement.text().get();

		if (mdd.id == MetaDataId::Name)
		{
			mdl.mName = value;
			continue;
		}

		if (mdd.id == MetaDataId::GenreIds)
			continue;

		if (value == mdd.defaultValue)
			continue;

		if (mdd.type == MD_BOOL)
			value = Utils::String::toLower(value);
		
		if (preloadMedias && mdd.type == MD_PATH && (mdd.id == MetaDataId::Image || mdd.id == MetaDataId::Thumbnail || mdd.id == MetaDataId::Marquee || mdd.id == MetaDataId::Video) &&
			!Utils::FileSystem::exists(Utils::FileSystem::resolveRelativePath(value, relativeTo, true)))
			continue;
		
		// Players -> remove "1-"
		if (type == GAME_METADATA && mdd.id == MetaDataId::Players && Utils::String::startsWith(value, "1-"))
			value = Utils::String::replace(value, "1-", "");

		mdl.set(mdd.id, value);
	}

	for (pugi::xml_attribute xattr : node.attributes())
	{
		std::string name = xattr.name();
		auto it = mGameIdMap.find(name);
		if (it == mGameIdMap.cend())
		{
			value = xattr.value();
			if (!value.empty())
				mdl.mUnKnownElements.push_back(std::tuple<std::string, std::string, bool>(name, value, false));

			continue;
		}

		MetaDataDecl& mdd = mMetaDataDecls[mMetaDataIndexes[it->second]];
		if (!mdd.isAttribute)
			continue;

		value = xattr.value();

		if (value == mdd.defaultValue)
			continue;

		if (mdd.type == MD_BOOL)
			value = Utils::String::toLower(value);

		// Players -> remove "1-"
		if (type == GAME_METADATA && mdd.id == MetaDataId::Players && Utils::String::startsWith(value, "1-"))
			value = Utils::String::replace(value, "1-", "");

		if (mdd.id == MetaDataId::Name)
			mdl.mName = value;
		else
			mdl.set(mdd.id, value);
	}

	return mdl;
}

// Add migration for alternative formats & old tags
void MetaDataList::migrate(FileData* file, pugi::xml_node& node)
{
	std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(file->getPath()));

	if (get(MetaDataId::Crc32).empty())
	{
		pugi::xml_node xelement = node.child("hash");
		if (xelement)
			set(MetaDataId::Crc32, xelement.text().get());
	}
}

void MetaDataList::appendToXML(pugi::xml_node& parent, bool ignoreDefaults, const std::string& relativeTo, bool fullPaths) const
{
	const std::vector<MetaDataDecl>& mdd = getMDD();

	for(auto mddIter = mdd.cbegin(); mddIter != mdd.cend(); mddIter++)
	{
		if (mddIter->id == 0)
		{
			parent.append_child("name").text().set(mName.c_str());
			continue;
		}

		// Don't save GenreIds
		if (mddIter->id == MetaDataId::GenreIds)
			continue;

		auto mapIter = mMap.find(mddIter->id);
		if(mapIter != mMap.cend())
		{
			// we have this value!
			// if it's just the default (and we ignore defaults), don't write it
			if (ignoreDefaults && mapIter->second == mddIter->defaultValue)
				continue;

			// try and make paths relative if we can
			std::string value = mapIter->second;
			if (mddIter->type == MD_PATH)
			{
				if (fullPaths && mRelativeTo != nullptr)
					value = Utils::FileSystem::resolveRelativePath(value, mRelativeTo->getStartPath(), true);
				else
					value = Utils::FileSystem::createRelativePath(value, relativeTo, true);
			}
						
			if (mddIter->isAttribute)
				parent.append_attribute(mddIter->key.c_str()).set_value(value.c_str());
			else
				parent.append_child(mddIter->key.c_str()).text().set(value.c_str());
		}
	}

	for (std::tuple<std::string, std::string, bool> element : mUnKnownElements)
	{	
		bool isElement = std::get<2>(element);
		if (isElement)
			parent.append_child(std::get<0>(element).c_str()).text().set(std::get<1>(element).c_str());
		else 
			parent.append_attribute(std::get<0>(element).c_str()).set_value(std::get<1>(element).c_str());
	}
}

void MetaDataList::set(MetaDataId id, const std::string& value)
{
	if (id == MetaDataId::Name)
	{
		if (mName == value)
			return;

		mName = value;
		mWasChanged = true;
		return;
	}

	// Players -> remove "1-"
	if (mType == GAME_METADATA && id == 12 && Utils::String::startsWith(value, "1-")) // "players"
	{
		mMap[id] = Utils::String::replace(value, "1-", "");
		return;
	}

	auto prev = mMap.find(id);
	if (prev != mMap.cend() && prev->second == value)
		return;

	if (mGameTypeMap[id] == MD_PATH && mRelativeTo != nullptr) // if it's a path, resolve relative paths				
		mMap[id] = Utils::FileSystem::createRelativePath(value, mRelativeTo->getStartPath(), true);
	else
		mMap[id] = Utils::String::trim(value);

	mWasChanged = true;
}

const std::string MetaDataList::get(MetaDataId id, bool resolveRelativePaths) const
{
	if (id == MetaDataId::Name)
		return mName;

	auto it = mMap.find(id);
	if (it != mMap.end())
	{
		if (resolveRelativePaths && mGameTypeMap[id] == MD_PATH && mRelativeTo != nullptr) // if it's a path, resolve relative paths				
			return Utils::FileSystem::resolveRelativePath(it->second, mRelativeTo->getStartPath(), true);

		return it->second;
	}

	return mDefaultGameMap[id];
}

void MetaDataList::set(const std::string& key, const std::string& value)
{
	if (mGameIdMap.find(key) == mGameIdMap.cend())
		return;

	set(getId(key), value);
}

const std::string MetaDataList::get(const std::string& key, bool resolveRelativePaths) const
{
	if (mGameIdMap.find(key) == mGameIdMap.cend())
		return "";

	return get(getId(key), resolveRelativePaths);
}

int MetaDataList::getInt(MetaDataId id) const
{
	return atoi(get(id).c_str());
}

float MetaDataList::getFloat(MetaDataId id) const
{
	return Utils::String::toFloat(get(id));
}

bool MetaDataList::wasChanged() const
{
	return mWasChanged;
}

void MetaDataList::resetChangedFlag()
{
	mWasChanged = false;
}

void MetaDataList::importScrappedMetadata(const MetaDataList& source)
{
	int type = MetaDataImportType::Types::ALL;

	if (Settings::getInstance()->getString("Scraper") == "ScreenScraper")
	{
		if (Settings::getInstance()->getString("ScrapperImageSrc").empty())
			type &= ~MetaDataImportType::Types::IMAGE;

		if (Settings::getInstance()->getString("ScrapperThumbSrc").empty())
			type &= ~MetaDataImportType::Types::THUMB;

		if (Settings::getInstance()->getString("ScrapperLogoSrc").empty())
			type &= ~MetaDataImportType::Types::MARQUEE;

		if (!Settings::getInstance()->getBool("ScrapeVideos"))
			type &= ~MetaDataImportType::Types::VIDEO;

		if (!Settings::getInstance()->getBool("ScrapeFanart"))
			type &= ~MetaDataImportType::Types::FANART;

		if (!Settings::getInstance()->getBool("ScrapeBoxBack"))
			type &= ~MetaDataImportType::Types::BOXBACK;

		if (!Settings::getInstance()->getBool("ScrapeTitleShot"))
			type &= ~MetaDataImportType::Types::TITLESHOT;

		if (!Settings::getInstance()->getBool("ScrapeMap"))
			type &= ~MetaDataImportType::Types::MAP;

		if (!Settings::getInstance()->getBool("ScrapeManual"))
			type &= ~MetaDataImportType::Types::MANUAL;

		if (!Settings::getInstance()->getBool("ScrapeCartridge"))
			type &= ~MetaDataImportType::Types::CARTRIDGE;		
	}

	for (auto mdd : getMDD())
	{
		if (mdd.isStatistic && mdd.id != MetaDataId::ScraperId)
			continue;

		if (mdd.id == MetaDataId::KidGame) // Not scrapped yet
			continue;

		if (mdd.id == MetaDataId::Region || mdd.id == MetaDataId::Language) // Not scrapped
			continue;

		if (mdd.id == MetaDataId::Favorite || mdd.id == MetaDataId::Hidden || mdd.id == MetaDataId::Emulator || mdd.id == MetaDataId::Core)
			continue;

		if (mdd.id == MetaDataId::Image && (source.get(mdd.id).empty() || (type & MetaDataImportType::Types::IMAGE) != MetaDataImportType::Types::IMAGE))
			continue;		

		if (mdd.id == MetaDataId::Thumbnail && (source.get(mdd.id).empty() || (type & MetaDataImportType::Types::THUMB) != MetaDataImportType::Types::THUMB))
			continue;

		if (mdd.id == MetaDataId::Marquee && (source.get(mdd.id).empty() || (type & MetaDataImportType::Types::MARQUEE) != MetaDataImportType::Types::MARQUEE))
			continue;

		if (mdd.id == MetaDataId::Video && (source.get(mdd.id).empty() || (type & MetaDataImportType::Types::VIDEO) != MetaDataImportType::Types::VIDEO))
			continue;

		if (mdd.id == MetaDataId::TitleShot && (source.get(mdd.id).empty() || (type & MetaDataImportType::Types::TITLESHOT) != MetaDataImportType::Types::TITLESHOT))
			continue;

		if (mdd.id == MetaDataId::FanArt && (source.get(mdd.id).empty() || (type & MetaDataImportType::Types::FANART) != MetaDataImportType::Types::FANART))
			continue;

		if (mdd.id == MetaDataId::BoxBack && (source.get(mdd.id).empty() || (type & MetaDataImportType::Types::BOXBACK) != MetaDataImportType::Types::BOXBACK))
			continue;

		if (mdd.id == MetaDataId::Map && (source.get(mdd.id).empty() || (type & MetaDataImportType::Types::MAP) != MetaDataImportType::Types::MAP))
			continue;

		if (mdd.id == MetaDataId::Manual && (source.get(mdd.id).empty() || (type & MetaDataImportType::Types::MANUAL) != MetaDataImportType::Types::MANUAL))
			continue;

		if (mdd.id == MetaDataId::Cartridge && (source.get(mdd.id).empty() || (type & MetaDataImportType::Types::CARTRIDGE) != MetaDataImportType::Types::CARTRIDGE))
			continue;

		if (mdd.id == MetaDataId::Rating && source.getFloat(mdd.id) < 0)
			continue;

		set(mdd.id, source.get(mdd.id));


		if (mdd.type == MetaDataType::MD_PATH)
		{
			ImageIO::removeImageCache(source.get(mdd.id));

			unsigned int x, y;
			ImageIO::loadImageSize(source.get(mdd.id).c_str(), &x, &y);
		}
	}

	if (Utils::String::startsWith(source.getName(), "ZZZ(notgame)"))
		set(MetaDataId::Hidden, "true");
}

std::string MetaDataList::getRelativeRootPath()
{
	if (mRelativeTo)
		return mRelativeTo->getStartPath();

	return "";
}