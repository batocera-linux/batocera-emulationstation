#include "MetaData.h"

#include "utils/FileSystemUtil.h"
#include "Log.h"
#include <pugixml/src/pugixml.hpp>
#include "LocaleES.h"

std::vector<MetaDataDecl> gameMDD;
std::vector<MetaDataDecl> folderMDD;

                                // key,         type,                   default,            statistic,          name in GuiMetaDataEd,          prompt in GuiMetaDataEd
void initMetadata() {
  gameMDD.push_back(MetaDataDecl("name",	MD_STRING,		"", 			false,		_("Name"),			_("enter game name")));
  gameMDD.push_back(MetaDataDecl("sortname",	MD_STRING,		"", 			false,		_("Sort name"),			_("enter game sort name")));
  gameMDD.push_back(MetaDataDecl("desc",	MD_MULTILINE_STRING,	"", 			false,		_("Description"),		_("enter description")));
  gameMDD.push_back(MetaDataDecl("image",	MD_PATH,		"", 			false,		_("Image"),			_("enter path to image")));
  gameMDD.push_back(MetaDataDecl("video",	MD_PATH,		"", 			false,		_("Path to video"),			_("enter path to thumbnail")));
  gameMDD.push_back(MetaDataDecl("marquee",	MD_PATH,		"", 			false,		_("Path to marquee"),			_("enter path to thumbnail")));
  gameMDD.push_back(MetaDataDecl("thumbnail",	MD_PATH,		"", 			false,		_("Thumbnail"),			_("enter path to thumbnail")));
  gameMDD.push_back(MetaDataDecl("rating",	MD_RATING,		"0.000000", 		false,		_("Rating"),			_("enter rating")));
  gameMDD.push_back(MetaDataDecl("releasedate", MD_DATE,		"not-a-date-time", 	false,		_("Release date"),		_("enter release date")));
  gameMDD.push_back(MetaDataDecl("developer",	MD_STRING,		"unknown",		false,		_("Developer"),			_("enter game developer")));
  gameMDD.push_back(MetaDataDecl("publisher",	MD_STRING,		"unknown",		false,		_("Publisher"),			_("enter game publisher")));
  gameMDD.push_back(MetaDataDecl("genre",	MD_STRING,		"unknown",		false,		_("Genre"),			_("enter game genre")));
  gameMDD.push_back(MetaDataDecl("players",	MD_INT,			"1",			false,		_("Players"),			_("enter number of players")));
  gameMDD.push_back(MetaDataDecl("favorite",	MD_BOOL,		"false",		false,		_("Favorite"),			_("enter favorite")));
  gameMDD.push_back(MetaDataDecl("hidden",	MD_BOOL,		"false",		false,		_("Hidden"),			_("set hidden")));
  gameMDD.push_back(MetaDataDecl("kidgame",	MD_BOOL,		"false",		false,		_("Kidgame"),			_("set kidgame")));
  gameMDD.push_back(MetaDataDecl("playcount",	MD_INT,			"0",			true,		_("Play count"),		_("enter number of times played")));
  gameMDD.push_back(MetaDataDecl("lastplayed",	MD_TIME,		"0", 			true,		_("Last played"),		_("enter last played date")));

  folderMDD.push_back(MetaDataDecl("name",	MD_STRING,		"", 		false, _("name"),        _("enter game name")));
  folderMDD.push_back(MetaDataDecl("sortname",	MD_STRING,		"", 		false, _("sortname"),    _("enter game sort name")));
  folderMDD.push_back(MetaDataDecl("desc",	MD_MULTILINE_STRING,	"", 		false, _("description"), _("enter description")));
  folderMDD.push_back(MetaDataDecl("image",	MD_PATH,		"", 		false, _("image"),       _("enter path to image")));
  folderMDD.push_back(MetaDataDecl("thumbnail",	MD_PATH,		"", 		false, _("thumbnail"),   _("enter path to thumbnail")));
  folderMDD.push_back(MetaDataDecl("video",	MD_PATH,		"", 		false, _("video"),       _("enter path to video")));
  folderMDD.push_back(MetaDataDecl("marquee",	MD_PATH,		"", 		false, _("marquee"),     _("enter path to marquee")));
  folderMDD.push_back(MetaDataDecl("rating",	MD_RATING,		"0.000000", 		false,		_("Rating"),			_("enter rating")));
  folderMDD.push_back(MetaDataDecl("releasedate", MD_DATE,		"not-a-date-time", 	false,		_("Release date"),		_("enter release date")));
  folderMDD.push_back(MetaDataDecl("developer",	MD_STRING,		"unknown",		false,		_("Developer"),			_("enter game developer")));
  folderMDD.push_back(MetaDataDecl("publisher",	MD_STRING,		"unknown",		false,		_("Publisher"),			_("enter game publisher")));
  folderMDD.push_back(MetaDataDecl("genre",	MD_STRING,		"unknown",		false,		_("Genre"),			_("enter game genre")));
  folderMDD.push_back(MetaDataDecl("players",	MD_INT,			"1",			false,		_("Players"),			_("enter number of players")));
}

const std::vector<MetaDataDecl>& getMDDByType(MetaDataListType type)
{
	switch(type)
	{
	case GAME_METADATA:
		return gameMDD;
	case FOLDER_METADATA:
		return folderMDD;
	}

	LOG(LogError) << "Invalid MDD type";
	return gameMDD;
}



MetaDataList::MetaDataList(MetaDataListType type)
	: mType(type), mWasChanged(false)
{
	const std::vector<MetaDataDecl>& mdd = getMDD();
	for(auto iter = mdd.cbegin(); iter != mdd.cend(); iter++)
		set(iter->key, iter->defaultValue);
}


MetaDataList MetaDataList::createFromXML(MetaDataListType type, pugi::xml_node& node, const std::string& relativeTo)
{
	MetaDataList mdl(type);

	const std::vector<MetaDataDecl>& mdd = mdl.getMDD();

	for(auto iter = mdd.cbegin(); iter != mdd.cend(); iter++)
	{
		pugi::xml_node md = node.child(iter->key.c_str());
		if(md)
		{
			// if it's a path, resolve relative paths
			std::string value = md.text().get();
			if (iter->type == MD_PATH)
			{
				value = Utils::FileSystem::resolveRelativePath(value, relativeTo, true);
			}
			mdl.set(iter->key, value);
		}else{
			mdl.set(iter->key, iter->defaultValue);
		}
	}

	return mdl;
}

void MetaDataList::appendToXML(pugi::xml_node& parent, bool ignoreDefaults, const std::string& relativeTo) const
{
	const std::vector<MetaDataDecl>& mdd = getMDD();

	for(auto mddIter = mdd.cbegin(); mddIter != mdd.cend(); mddIter++)
	{
		auto mapIter = mMap.find(mddIter->key);
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

			parent.append_child(mapIter->first.c_str()).text().set(value.c_str());
		}
	}
}

void MetaDataList::set(const std::string& key, const std::string& value)
{
	mMap[key] = value;
	mWasChanged = true;
}

const std::string& MetaDataList::get(const std::string& key) const
{
	return mMap.at(key);
}

int MetaDataList::getInt(const std::string& key) const
{
	return atoi(get(key).c_str());
}

float MetaDataList::getFloat(const std::string& key) const
{
	return (float)atof(get(key).c_str());
}

bool MetaDataList::isDefault()
{
	const std::vector<MetaDataDecl>& mdd = getMDD();

	for (unsigned int i = 1; i < mMap.size(); i++) {
		if (mMap.at(mdd[i].key) != mdd[i].defaultValue) return false;
	}

	return true;
}

bool MetaDataList::wasChanged() const
{
	return mWasChanged;
}

void MetaDataList::resetChangedFlag()
{
	mWasChanged = false;
}
