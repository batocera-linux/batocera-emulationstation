#include "MetaData.h"

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include <pugixml/src/pugixml.hpp>
#include "SystemData.h"
#include "LocaleES.h"
#include "Settings.h"

static std::vector<MetaDataDecl> gameMDD;
static std::vector<MetaDataDecl> folderMDD;

static std::string* mDefaultGameMap = nullptr;
static MetaDataType* mGameTypeMap = nullptr;

static std::string* mDefaultFolderMap = nullptr;
static MetaDataType* mFolderTypeMap = nullptr;

static std::map<std::string, MetaDataId> mGameIdMap;
static std::map<std::string, MetaDataId> mFolderIdMap;

void MetaDataList::initMetadata()
{
	MetaDataDecl gameDecls[] = 
	{
		// key,             type,                   default,            statistic,  name in GuiMetaDataEd,  prompt in GuiMetaDataEd
		{ Name,             "name",        MD_STRING,              "",                 false,      _("Name"),                 _("enter game name") },
	//	{ SortName,         "sortname",    MD_STRING,              "",                 false,      _("sortname"),             _("enter game sort name") },
		{ Desc,             "desc",        MD_MULTILINE_STRING,    "",                 false,      _("Description"),          _("enter description") },

#if WIN32 && !_DEBUG
		{ Emulator,         "emulator",    MD_LIST,				 "",                 false,       _("Emulator"),			 _("emulator") },
		{ Core,             "core",	      MD_LIST,				 "",                 false,       _("Core"),				 _("core") },
#else
		// Windows & recalbox gamelist.xml compatiblity -> Set as statistic to hide it from metadata editor
		{ Emulator,         "emulator",    MD_LIST,				 "",                 true,        _("Emulator"),			 _("emulator") },
		{ Core,             "core",	     MD_LIST,				 "",                 true,        _("Core"),				 _("core") },
#endif

		{ Image,            "image",       MD_PATH,                "",                 false,      _("Image"),                _("enter path to image") },
		{ Video,            "video",       MD_PATH,                "",                 false,      _("Video"),                _("enter path to video") },
		{ Marquee,          "marquee",     MD_PATH,                "",                 false,      _("Marquee"),              _("enter path to marquee") },
		{ Thumbnail,        "thumbnail",   MD_PATH,                "",                 false,      _("Thumbnail"),            _("enter path to thumbnail") },
		{ Rating,           "rating",      MD_RATING,              "0.000000",         false,      _("Rating"),               _("enter rating") },
		{ ReleaseDate,      "releasedate", MD_DATE,                "not-a-date-time",  false,      _("Release date"),         _("enter release date") },
		{ Developer,        "developer",   MD_STRING,              "",                 false,      _("Developer"),            _("enter game developer") },
		{ Publisher,        "publisher",   MD_STRING,              "",                 false,      _("Publisher"),            _("enter game publisher") },
		{ Genre,            "genre",       MD_STRING,              "",                 false,      _("Genre"),                _("enter game genre") },

		{ ArcadeSystemName, "arcadesystemname",  MD_STRING,        "",                 false,      _("Arcade system"),        _("enter game arcade system") },

		{ Players,          "players",     MD_INT,                 "1",                false,      _("Players"),              _("enter number of players") },
		{ Favorite,         "favorite",    MD_BOOL,                "false",            false,      _("Favorite"),             _("enter favorite") },
		{ Hidden,           "hidden",      MD_BOOL,                "false",            false,      _("Hidden"),               _("enter hidden") },
		{ KidGame,          "kidgame",     MD_BOOL,                "false",            false,      _("Kidgame"),              _("enter kidgame") },
		{ PlayCount,        "playcount",   MD_INT,                 "0",                true,       _("Play count"),           _("enter number of times played") },
		{ LastPlayed,       "lastplayed",  MD_TIME,                "0",                true,       _("Last played"),          _("enter last played date") },

		{ Crc32,            "crc32",       MD_STRING,              "",                 true,       _("Crc32"),                _("Crc32 checksum") },
		{ Md5,              "md5",         MD_STRING,              "",                 true,       _("Md5"),                  _("Md5 checksum") },

		{ GameTime,         "gametime",    MD_INT,                 "0",                true,       _("Game time"),            _("how long the game has been played in total (seconds)") },

		{ Language,         "lang",        MD_STRING,              "",                 false,       _("Languages"),            _("Languages") },
		{ Region,           "region",      MD_STRING,              "",                 false,       _("Region"),               _("Region") }
	};
	
	MetaDataDecl folderDecls[] = 
	{
		{ Name,             "name",        MD_STRING,              "",                 false,       _("Name"),                 _("enter game name") },
	//	{ SortName,         "sortname",    MD_STRING,              "",                 false,       _("sortname"),             _("enter game sort name") },
		{ Desc,             "desc",        MD_MULTILINE_STRING,    "",                 false,       _("Description"),          _("enter description") },
		{ Image,            "image",       MD_PATH,                "",                 false,       _("Image"),                _("enter path to image") },
		{ Thumbnail,        "thumbnail",   MD_PATH,                "",                 false,       _("Thumbnail"),            _("enter path to thumbnail") },
		{ Video,            "video",       MD_PATH,                "",                 false,       _("Video"),                _("enter path to video") },
		{ Marquee,          "marquee",     MD_PATH,                "",                 false,       _("Marquee"),              _("enter path to marquee") },
		{ Rating,           "rating",      MD_RATING,              "0.000000",         false,       _("Rating"),               _("enter rating") },
		{ ReleaseDate,      "releasedate", MD_DATE,                "not-a-date-time",  false,       _("Release date"),         _("enter release date") },
		{ Developer,        "developer",   MD_STRING,              "",                 false,       _("Developer"),            _("enter game developer") },
		{ Publisher,        "publisher",   MD_STRING,              "",                 false,       _("Publisher"),            _("enter game publisher") },
		{ Genre,            "genre",       MD_STRING,              "",                 false,       _("Genre"),                _("enter game genre") },
		{ Players,          "players",     MD_INT,                 "1",                false,       _("Players"),              _("enter number of players") },
		{ Hidden,           "hidden",      MD_BOOL,                "false",            false,       _("Hidden"),               _("enter hidden") },
		{ Favorite,         "favorite",    MD_BOOL,                "false",            false,       _("favorite"),             _("enter favorite") },
		// Some games are folders ( dosbox )
		{ KidGame,          "kidgame",     MD_BOOL,                "false",            false,      _("Kidgame"),              _("enter kidgame") },
		{ PlayCount,        "playcount",   MD_INT,                 "0",                true,       _("Play count"),            _("enter number of times played") },
		{ LastPlayed,       "lastplayed",  MD_TIME,                "0",                true,       _("Last played"),           _("enter last played date") },
		{ GameTime,         "gametime",    MD_INT,                 "0",                true,       _("Game time"),             _("how long the game has been played in total (seconds)") },
		{ Language,         "lang",        MD_STRING,              "",                 false,       _("Languages"),             _("Languages") },
		{ Region,           "region",      MD_STRING,              "",                 false,       _("Region"),                _("Region") }
	};

	// Build Game maps
	gameMDD = std::vector<MetaDataDecl>(gameDecls, gameDecls + sizeof(gameDecls) / sizeof(gameDecls[0]));

	int maxID = 40; // Maximum ids of metadata

	{
		const std::vector<MetaDataDecl>& mdd = getMDDByType(GAME_METADATA);

		if (mDefaultGameMap != nullptr) delete[] mDefaultGameMap;
		if (mGameTypeMap != nullptr) delete[] mGameTypeMap;

		mDefaultGameMap = new std::string[maxID];
		mGameTypeMap = new MetaDataType[maxID];

		for (int i = 0; i < maxID; i++)
			mGameTypeMap[i] = MD_STRING;
		
		for (auto iter = mdd.cbegin(); iter != mdd.cend(); iter++)
		{
			mDefaultGameMap[iter->id] = iter->defaultValue;
			mGameTypeMap[iter->id] = iter->type;
			mGameIdMap[iter->key] = iter->id;
		}
	}

	// Build Folder maps
	folderMDD = std::vector<MetaDataDecl>(folderDecls, folderDecls + sizeof(folderDecls) / sizeof(folderDecls[0]));

	{
		const std::vector<MetaDataDecl>& mdd = getMDDByType(FOLDER_METADATA);
		
		if (mDefaultFolderMap != nullptr) delete[] mDefaultFolderMap;
		if (mFolderTypeMap != nullptr) delete[] mFolderTypeMap;

		mDefaultFolderMap = new std::string[maxID];
		mFolderTypeMap = new MetaDataType[maxID];

		for (int i = 0; i < maxID; i++)
			mFolderTypeMap[i] = MD_STRING;

		for (auto iter = mdd.cbegin(); iter != mdd.cend(); iter++)
		{
			mDefaultFolderMap[iter->id] = iter->defaultValue;
			mFolderTypeMap[iter->id] = iter->type;
			mFolderIdMap[iter->key] = iter->id;
		}
	}
}

MetaDataType MetaDataList::getType(MetaDataId id) const
{
	return mType == GAME_METADATA ? mGameTypeMap[id] : mFolderTypeMap[id];
}

MetaDataId MetaDataList::getId(const std::string& key) const
{
	return mType == GAME_METADATA ? mGameIdMap[key] : mFolderIdMap[key];
}

const std::vector<MetaDataDecl>& getMDDByType(MetaDataListType type)
{
	return type == FOLDER_METADATA ? folderMDD : gameMDD;
}

MetaDataList::MetaDataList(MetaDataListType type) : mType(type), mWasChanged(false), mRelativeTo(nullptr)
{

}


MetaDataList MetaDataList::createFromXML(MetaDataListType type, pugi::xml_node& node, SystemData* system)
{
	MetaDataList mdl(type);
	mdl.mRelativeTo = system;

	//std::string relativeTo = system->getStartPath();

	const std::vector<MetaDataDecl>& mdd = mdl.getMDD();

	for(auto iter = mdd.cbegin(); iter != mdd.cend(); iter++)
	{
		pugi::xml_node md = node.child(iter->key.c_str());
		if(md)
		{
			// if it's a path, resolve relative paths
			std::string value = md.text().get();

//			if (iter->type == MD_PATH)
//				value = Utils::FileSystem::resolveRelativePath(value, relativeTo, true);

			if (value == iter->defaultValue)
				continue;

			if (iter->type == MD_BOOL)
				value = Utils::String::toLower(value);

			// Players -> remove "1-"
			if (type == GAME_METADATA && iter->id == 12 && iter->type == MD_INT && Utils::String::startsWith(value, "1-")) // "players"
				value = Utils::String::replace(value, "1-", "");
			
			if (iter->id == 0)
				mdl.mName = value;
			else
				mdl.set(iter->key, value);
		}
	}

	return mdl;
}

void MetaDataList::appendToXML(pugi::xml_node& parent, bool ignoreDefaults, const std::string& relativeTo) const
{
	const std::vector<MetaDataDecl>& mdd = getMDD();

	for(auto mddIter = mdd.cbegin(); mddIter != mdd.cend(); mddIter++)
	{
		if (mddIter->id == 0)
		{
			parent.append_child("name").text().set(mName.c_str());
			continue;
		}

		auto mapIter = mMap.find(mddIter->id);
		if(mapIter != mMap.cend())
		{
			// we have this value!
			// if it's just the default (and we ignore defaults), don't write it
			if(ignoreDefaults && mapIter->second == mddIter->defaultValue)
				continue;
			
			// try and make paths relative if we can
			std::string value = mapIter->second;
			if (mddIter->type == MD_PATH)
				value = Utils::FileSystem::createRelativePath(value, relativeTo, true);

			parent.append_child(mddIter->key.c_str()).text().set(value.c_str());
		}
	}
}

const std::string& MetaDataList::getName() const
{
	return mName;
}

void MetaDataList::set(const std::string& key, const std::string& value)
{
	if (key == "name")
	{
		if (mName == value)
			return;

		mName = value;
	}
	else
	{
		auto id = getId(key);
		
		// Players -> remove "1-"
		if (mType == GAME_METADATA && id == 12 && Utils::String::startsWith(value, "1-")) // "players"
		{
			mMap[id] = Utils::String::replace(value, "1-", "");
			return;
		}

		auto prev = mMap.find(id);
		if (prev != mMap.cend() && prev->second == value)
			return;

		if (getType(id) == MD_PATH && mRelativeTo != nullptr) // if it's a path, resolve relative paths				
			mMap[id] = Utils::FileSystem::createRelativePath(value, mRelativeTo->getStartPath(), true);
		else
			mMap[id] = Utils::String::trim(value);
	}

	mWasChanged = true;
}

const std::string MetaDataList::get(MetaDataId id, bool resolveRelativePaths) const
{
	if (id == MetaDataId::Name)
		return mName;

	auto it = mMap.find(id);
	if (it != mMap.end())
	{
		if (resolveRelativePaths && getType(id) == MD_PATH && mRelativeTo != nullptr) // if it's a path, resolve relative paths				
			return Utils::FileSystem::resolveRelativePath(it->second, mRelativeTo->getStartPath(), true);

		return it->second;
	}

	if (mType == GAME_METADATA)
		return mDefaultGameMap[id];

	return mDefaultFolderMap[id];
}

const std::string MetaDataList::get(const std::string& key, bool resolveRelativePaths) const
{
	return get(getId(key), resolveRelativePaths);
}

int MetaDataList::getInt(const std::string& key) const
{
	return atoi(get(key).c_str());
}

float MetaDataList::getFloat(const std::string& key) const
{
	return (float)atof(get(key).c_str());
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
	}

	for (auto mdd : getMDD())
	{
		if (mdd.key == "favorite" || mdd.key == "playcount" || mdd.key == "lastplayed" || mdd.key == "crc32" || mdd.key == "md5")
			continue;

		if (mdd.key == "image" && (type & MetaDataImportType::Types::IMAGE) != MetaDataImportType::Types::IMAGE)
			continue;

		if (mdd.key == "thumbnail" && (type & MetaDataImportType::Types::THUMB) != MetaDataImportType::Types::THUMB)
			continue;

		if (mdd.key == "marquee" && (type & MetaDataImportType::Types::MARQUEE) != MetaDataImportType::Types::MARQUEE)
			continue;

		if (mdd.key == "video" && (type & MetaDataImportType::Types::VIDEO) != MetaDataImportType::Types::VIDEO)
			continue;

		set(mdd.key, source.get(mdd.key));
	}
}
