#pragma once
#ifndef ES_APP_META_DATA_H
#define ES_APP_META_DATA_H

#include <map>
#include <vector>
#include <functional>
#include <string>

class SystemData;
class FileData;

namespace pugi { class xml_node; }

enum MetaDataType
{
	//generic types
	MD_STRING,
	MD_INT,
	MD_FLOAT,
	MD_BOOL,

	//specialized types
	MD_MULTILINE_STRING,
	MD_PATH,
	MD_RATING,
	MD_DATE,
	MD_TIME, //used for lastplayed
        MD_LIST // batocera
};

enum MetaDataId
{
	Name = 0,
	SortName = 1,
	Desc = 2,
	Emulator = 3,
	Core = 4,
	Image = 5,
	Video = 6,
	Marquee = 7,
	Thumbnail = 8,
	Rating = 9,
	ReleaseDate = 10,
	Developer = 11,
	Publisher = 12,
	Genre = 13,
	ArcadeSystemName = 14,
	Players = 15,
	Favorite = 16,
	Hidden = 17,
	KidGame = 18,
	PlayCount = 19,
	LastPlayed = 20,
	Crc32 = 21,
	Md5 = 22,
	GameTime = 23,
	Language = 24,
	Region = 25,
	FanArt = 26,
	TitleShot = 27,
	Cartridge = 28,
	Map = 29,
	Manual = 30,
	BoxArt = 31,
	Wheel = 32,
	Mix = 33,
	CheevosHash = 34,
	CheevosId = 35,
	ScraperId = 36,
	BoxBack = 37,
	Magazine = 38,
	GenreIds = 39,
	Family = 40
};

namespace MetaDataImportType
{
	enum Types : int
	{
		IMAGE = 1,
		THUMB = 2,
		VIDEO = 4,
		MARQUEE = 8,
		FANART = 16,
		MANUAL = 32,		
		MAP = 64,
		CARTRIDGE = 128,
		TITLESHOT = 256,
		BOXBACK = 512,

		ALL = IMAGE | THUMB | VIDEO | MARQUEE | FANART | MANUAL | MAP | CARTRIDGE | TITLESHOT | BOXBACK
	};
}

struct MetaDataDecl
{
	MetaDataId   id;
	std::string  key;
	MetaDataType type;
	std::string  defaultValue;
	bool         isStatistic; //if true, ignore scraper values for this metadata
	std::string  displayName; // displayed as this in editors
	std::string  displayPrompt; // phrase displayed in editors when prompted to enter value (currently only for strings)
	bool		 visibleForFolder;
	bool		 isAttribute;

	MetaDataDecl(MetaDataId id, std::string key, MetaDataType type, std::string defaultValue, bool isStatistic, std::string displayName, std::string displayPrompt, bool folderMetadata, bool isAttribute = false)
	{
		this->id = id;
		this->key = key;
		this->type = type;
		this->defaultValue = defaultValue;
		this->isStatistic = isStatistic;
		this->displayName = displayName;
		this->displayPrompt = displayPrompt;
		this->visibleForFolder = folderMetadata;
		this->isAttribute = isAttribute;
	}
};

enum MetaDataListType
{
	GAME_METADATA,
	FOLDER_METADATA
};

class MetaDataList
{
public:
	static void initMetadata();

	static MetaDataList createFromXML(MetaDataListType type, pugi::xml_node& node, SystemData* system);
	void appendToXML(pugi::xml_node& parent, bool ignoreDefaults, const std::string& relativeTo) const;

	void migrate(FileData* file, pugi::xml_node& node);

	MetaDataList(MetaDataListType type);
	
	void set(MetaDataId id, const std::string& value);

	const std::string get(MetaDataId id, bool resolveRelativePaths = true) const;
	
	void set(const std::string& key, const std::string& value);
	const std::string get(const std::string& key, bool resolveRelativePaths = true) const;

	int getInt(MetaDataId id) const;
	float getFloat(MetaDataId id) const;

	MetaDataType getType(MetaDataId id) const;
	MetaDataType getType(const std::string name) const;

	MetaDataId getId(const std::string& key) const;

	bool wasChanged() const;
	void resetChangedFlag();
	const void setDirty() 
	{ 
		mWasChanged = true; 
	}

	inline MetaDataListType getType() const { return mType; }
	static const std::vector<MetaDataDecl>& getMDD() { return mMetaDataDecls; }
	inline const std::string& getName() const { return mName; }
	
	void importScrappedMetadata(const MetaDataList& source);

	std::string getRelativeRootPath();

private:
	std::string		mName;
	MetaDataListType mType;
	std::map<MetaDataId, std::string> mMap;
	bool mWasChanged;
	SystemData*		mRelativeTo;

	static std::vector<MetaDataDecl> mMetaDataDecls;

	std::vector<std::tuple<std::string, std::string, bool>> mUnKnownElements;
};

#endif // ES_APP_META_DATA_H
