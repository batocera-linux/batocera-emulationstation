#include "FileFilterIndex.h"

#include "utils/StringUtil.h"
#include "views/UIModeController.h"
#include "FileData.h"
#include "Log.h"
#include "Settings.h"
#include "LocaleES.h"

#include <pugixml/src/pugixml.hpp>

#include "SystemData.h"
#include "FileData.h"
#include "CollectionSystemManager.h"

#define UNKNOWN_LABEL "UNKNOWN"
#define INCLUDE_UNKNOWN false;

FileFilterIndex::FileFilterIndex()
	: filterByFavorites(false), filterByGenre(false), filterByKidGame(false), filterByPlayers(false), filterByPubDev(false), filterByRatings(false), filterByYear(false)
{
	clearAllFilters();
	FilterDataDecl filterDecls[] = 
	{
		//type 				//allKeys 				//filteredBy 		//filteredKeys 				//primaryKey 	//hasSecondaryKey 	//secondaryKey 	//menuLabel
		{ FAVORITES_FILTER, &favoritesIndexAllKeys, &filterByFavorites,	&favoritesIndexFilteredKeys,"favorite",		false,				"",				_("FAVORITES")	},
		{ GENRE_FILTER, 	&genreIndexAllKeys, 	&filterByGenre,		&genreIndexFilteredKeys, 	"genre",		true,				"genre",		_("GENRE")	},
		{ PLAYER_FILTER, 	&playersIndexAllKeys, 	&filterByPlayers,	&playersIndexFilteredKeys, 	"players",		false,				"",				_("PLAYERS")	},
		{ PUBDEV_FILTER, 	&pubDevIndexAllKeys, 	&filterByPubDev,	&pubDevIndexFilteredKeys, 	"developer",	true,				"publisher",	_("PUBLISHER / DEVELOPER")	},
		{ RATINGS_FILTER, 	&ratingsIndexAllKeys, 	&filterByRatings,	&ratingsIndexFilteredKeys, 	"rating",		false,				"",				_("RATING")	},
		{ YEAR_FILTER, 		&yearIndexAllKeys, 		&filterByYear,		&yearIndexFilteredKeys, 	"year",			false,				"",				_("YEAR") },
		{ LANG_FILTER, 	    &langIndexAllKeys,      &filterByLang,	    &langIndexFilteredKeys, 	"lang",		    false,				"",				_("LANGUAGE") },
		{ REGION_FILTER, 	&regionIndexAllKeys,    &filterByRegion,	&regionIndexFilteredKeys, 	"region",		false,				"",				_("REGION") },
		{ KIDGAME_FILTER, 	&kidGameIndexAllKeys, 	&filterByKidGame,	&kidGameIndexFilteredKeys, 	"kidgame",		false,				"",				_("KIDGAME") },
		{ PLAYED_FILTER, 	&playedIndexAllKeys,    &filterByPlayed,	&playedIndexFilteredKeys, 	"played",		false,				"",				_("ALREADY PLAYED") }
	};

	std::vector<FilterDataDecl> filterDataDecl = std::vector<FilterDataDecl>(filterDecls, filterDecls + sizeof(filterDecls) / sizeof(filterDecls[0]));
	for (auto decl : filterDataDecl)
		mFilterDecl[(int) decl.type] = decl;

	resetIndex();
}

FileFilterIndex::~FileFilterIndex()
{
	resetIndex();
}

std::vector<FilterDataDecl> FileFilterIndex::getFilterDataDecls()
{
	std::vector<FilterDataDecl> ret;
	for (auto it : mFilterDecl)
		ret.push_back(it.second);
	return ret;
}

void FileFilterIndex::importIndex(FileFilterIndex* indexToImport)
{
	struct IndexImportStructure
	{
		std::map<std::string, int>* destinationIndex;
		std::map<std::string, int>* sourceIndex;
	};

	IndexImportStructure indexStructDecls[] = {
		{ &genreIndexAllKeys, &(indexToImport->genreIndexAllKeys) },
		{ &playersIndexAllKeys, &(indexToImport->playersIndexAllKeys) },
		{ &pubDevIndexAllKeys, &(indexToImport->pubDevIndexAllKeys) },
		{ &ratingsIndexAllKeys, &(indexToImport->ratingsIndexAllKeys) },
		{ &favoritesIndexAllKeys, &(indexToImport->favoritesIndexAllKeys) },
		{ &yearIndexAllKeys, &(indexToImport->yearIndexAllKeys) },
		{ &kidGameIndexAllKeys, &(indexToImport->kidGameIndexAllKeys) },
		{ &playedIndexAllKeys, &(indexToImport->playedIndexAllKeys) }
	};

	std::vector<IndexImportStructure> indexImportDecl = std::vector<IndexImportStructure>(indexStructDecls, indexStructDecls + sizeof(indexStructDecls) / sizeof(indexStructDecls[0]));

	for (std::vector<IndexImportStructure>::const_iterator indexesIt = indexImportDecl.cbegin(); indexesIt != indexImportDecl.cend(); ++indexesIt )
	{
		for (std::map<std::string, int>::const_iterator sourceIt = (*indexesIt).sourceIndex->cbegin(); sourceIt != (*indexesIt).sourceIndex->cend(); ++sourceIt )
		{
			if ((*indexesIt).destinationIndex->find((*sourceIt).first) == (*indexesIt).destinationIndex->cend())
			{
				// entry doesn't exist
				(*((*indexesIt).destinationIndex))[(*sourceIt).first] = (*sourceIt).second;
			}
			else
			{
				(*((*indexesIt).destinationIndex))[(*sourceIt).first] += (*sourceIt).second;
			}
		}
	}
}

void FileFilterIndex::resetIndex()
{
	mTextFilter = "";
	clearAllFilters();
	clearIndex(genreIndexAllKeys);
	clearIndex(playersIndexAllKeys);
	clearIndex(pubDevIndexAllKeys);
	clearIndex(ratingsIndexAllKeys);
	clearIndex(favoritesIndexAllKeys);
	clearIndex(yearIndexAllKeys);
	clearIndex(kidGameIndexAllKeys);
	clearIndex(playedIndexAllKeys);

	clearIndex(langIndexAllKeys);
	clearIndex(regionIndexAllKeys);

	manageIndexEntry(&favoritesIndexAllKeys, "FALSE", false);
	manageIndexEntry(&favoritesIndexAllKeys, "TRUE", false);

	manageIndexEntry(&kidGameIndexAllKeys, "FALSE", false);
	manageIndexEntry(&kidGameIndexAllKeys, "TRUE", false);

	manageIndexEntry(&playedIndexAllKeys, "FALSE", false);
	manageIndexEntry(&playedIndexAllKeys, "TRUE", false);
}

std::string FileFilterIndex::getIndexableKey(FileData* game, FilterIndexType type, bool getSecondary)
{
	std::string key;
	switch(type)
	{
		case LANG_FILTER:
		{
			if (getSecondary)
				break;

			key = game->getMetadata(MetaDataId::Language);
			break;
		}

		case REGION_FILTER:
		{
			if (getSecondary)
				break;

			key = game->getMetadata(MetaDataId::Region);
			break;
		}

		case GENRE_FILTER:
		{
			key = Utils::String::toUpper(game->getMetadata(MetaDataId::Genre));
			
			if (getSecondary && !key.empty()) 
			{
				auto idx = key.find('/');
				if (idx != std::string::npos)
					key = Utils::String::trim(key.substr(0, idx));
				else
					key = "";
			}
			break;
		}
		case PLAYER_FILTER:
		{
			if (getSecondary)
				break;

			key = game->getMetadata(MetaDataId::Players);
			break;
		}
		case PUBDEV_FILTER:
		{
			key = Utils::String::toUpper(game->getMetadata(MetaDataId::Publisher));

			if ((getSecondary && !key.empty()) || (!getSecondary && key.empty()))
				key = Utils::String::toUpper(game->getMetadata(MetaDataId::Developer));
		
			break;
		}
		case RATINGS_FILTER:
		{
			int ratingNumber = 0;
			if (!getSecondary)
			{
				std::string ratingString = game->getMetadata(MetaDataId::Rating);
				if (!ratingString.empty()) 
				{
					try 
					{
						ratingNumber = (int)((std::stod(ratingString)*5)+0.5);
						if (ratingNumber < 0)
							ratingNumber = 0;

						key = std::to_string(ratingNumber) + " STARS";
					}
					catch (int e)
					{
						LOG(LogError) << "Error parsing Rating (invalid value, exception nr.): " << ratingString << ", " << e;
					}
				}
			}
			break;
		}
		case FAVORITES_FILTER:
		{
			if (game->getType() != GAME)
				return "FALSE";
			key = Utils::String::toUpper(game->getMetadata(MetaDataId::Favorite));
			break;
		}
		case KIDGAME_FILTER:
		{
			if (game->getType() != GAME)
				return "FALSE";
			key = Utils::String::toUpper(game->getMetadata(MetaDataId::KidGame));
			break;
		}
		case PLAYED_FILTER:
		{
			key = atoi(game->getMetadata(MetaDataId::PlayCount).c_str()) == 0 ? "FALSE" : "TRUE";
			break;
		}
		case YEAR_FILTER:
		{
			key = game->getMetadata(MetaDataId::ReleaseDate);
			key = (key.length() >= 4 && key[0] >= '1' && key[0] <= '2') ? key.substr(0, 4) : "";
			break;
		}
	}
		
	if (key.empty() || (type == RATINGS_FILTER && key == "0 STARS")) 
		return UNKNOWN_LABEL;
	
	return key;
}

void FileFilterIndex::addToIndex(FileData* game)
{
	game->detectLanguageAndRegion(false);

	manageGenreEntryInIndex(game);
	managePlayerEntryInIndex(game);
	managePubDevEntryInIndex(game);
	manageRatingsEntryInIndex(game);
	manageYearEntryInIndex(game);
	manageLangEntryInIndex(game);
	manageRegionEntryInIndex(game);	
}

void FileFilterIndex::removeFromIndex(FileData* game)
{
	manageGenreEntryInIndex(game, true);
	managePlayerEntryInIndex(game, true);
	managePubDevEntryInIndex(game, true);
	manageRatingsEntryInIndex(game, true);
	manageYearEntryInIndex(game, true);
	manageLangEntryInIndex(game, true);
	manageRegionEntryInIndex(game, true);
}

void FileFilterIndex::setFilter(FilterIndexType type, std::vector<std::string>* values)
{
	// test if it exists before setting
	if(type == NONE)
	{
		clearAllFilters();
		return;
	}

	auto it = mFilterDecl.find(type);
	if (it == mFilterDecl.cend())
		return;
	
	FilterDataDecl& filterData = it->second;
	*(filterData.filteredByRef) = values != nullptr && values->size() > 0;
	filterData.currentFilteredKeys->clear();

	if (values == nullptr)
		return;

	for(auto value : *values)
		if (filterData.allIndexKeys->find(value) != filterData.allIndexKeys->cend()) // check if exists
			filterData.currentFilteredKeys->insert(value);	
}

void FileFilterIndex::clearAllFilters()
{
	for (auto& it : mFilterDecl)
	{
		FilterDataDecl& filterData = it.second;
		*(filterData.filteredByRef) = false;
		filterData.currentFilteredKeys->clear();
	}
	return;
}

void FileFilterIndex::resetFilters()
{
	clearAllFilters();
	setUIModeFilters();
}

void FileFilterIndex::setUIModeFilters()
{
	if (Settings::getInstance()->getBool("ForceDisableFilters"))
		return;
	
	if (UIModeController::getInstance()->isUIModeKid())
	{
		filterByKidGame = true;
		std::vector<std::string> val = { "TRUE" };
		setFilter(KIDGAME_FILTER, &val);
	}	
}

void FileFilterIndex::setTextFilter(const std::string text) 
{ 
	mTextFilter = Utils::String::toUpper(text);
}

bool FileFilterIndex::showFile(FileData* game)
{
	// this shouldn't happen, but just in case let's get it out of the way
	if (!isFiltered())
		return true;

	// if folder, needs further inspection - i.e. see if folder contains at least one element
	// that should be shown
	if (game->getType() == FOLDER) 
	{
		std::vector<FileData*> children = ((FolderData*)game)->getChildren();
		// iterate through all of the children, until there's a match

		for (std::vector<FileData*>::const_iterator it = children.cbegin(); it != children.cend(); ++it )
			if (showFile(*it))
				return true;

		return false;
	}

	bool keepGoing = false;

	if (!mTextFilter.empty() && Utils::String::toUpper(game->getName()).find(mTextFilter) != std::string::npos)
		keepGoing = true;

	bool hasFilter = false;

	for (auto& it : mFilterDecl)
	{
		FilterDataDecl& filterData = it.second;
		if (!(*(filterData.filteredByRef)))
			continue;
		
		hasFilter = true;

		// try to find a match
		std::string key = getIndexableKey(game, filterData.type, false);
		if (filterData.type == LANG_FILTER || filterData.type == REGION_FILTER)
		{
			for (auto val : Utils::String::split(key, ','))
				if (isKeyBeingFilteredBy(val, filterData.type))
					keepGoing = true;
		}
		else
			keepGoing = isKeyBeingFilteredBy(key, filterData.type);

		// if we didn't find a match, try for secondary keys - i.e. publisher and dev, or first genre
		if (!keepGoing)
		{
			if (!filterData.hasSecondaryKey)
				return false;

			std::string secKey = getIndexableKey(game, filterData.type, true);
			if (secKey != UNKNOWN_LABEL)
				keepGoing = isKeyBeingFilteredBy(secKey, filterData.type);
		}

		// if still nothing, then it's not a match
		if (!keepGoing)
			return false;		
	}

	if (mTextFilter.empty() && !hasFilter)
		return true;

	return keepGoing;
}

bool FileFilterIndex::isKeyBeingFilteredBy(std::string key, FilterIndexType type)
{
	auto it = mFilterDecl.find(type);
	if (it == mFilterDecl.cend())
		return false;

	auto keys = it->second.currentFilteredKeys;
	return keys->find(key) != keys->cend();
}

void FileFilterIndex::manageLangEntryInIndex(FileData* game, bool remove)
{
	std::string key = getIndexableKey(game, LANG_FILTER, false);
	if (key.empty() || key == UNKNOWN_LABEL)
		manageIndexEntry(&langIndexAllKeys, UNKNOWN_LABEL, remove, true);
	else
		for(auto val : Utils::String::split(key, ','))
			manageIndexEntry(&langIndexAllKeys, val, remove);
}

void FileFilterIndex::manageRegionEntryInIndex(FileData* game, bool remove)
{
	std::string key = getIndexableKey(game, REGION_FILTER, false);
	if (key.empty() || key == UNKNOWN_LABEL)
		manageIndexEntry(&regionIndexAllKeys, UNKNOWN_LABEL, remove, true);
	else
		for (auto val : Utils::String::split(key, ','))
			manageIndexEntry(&regionIndexAllKeys, val, remove);
}

void FileFilterIndex::manageGenreEntryInIndex(FileData* game, bool remove)
{
	std::string key = getIndexableKey(game, GENRE_FILTER, false);

	// flag for including unknowns
	bool includeUnknown = INCLUDE_UNKNOWN;

	// only add unknown in pubdev IF both dev and pub are empty
	if (!includeUnknown && (key == UNKNOWN_LABEL || key == "BIOS"))		
		return;	 // no valid genre info found

	manageIndexEntry(&genreIndexAllKeys, key, remove);

	key = getIndexableKey(game, GENRE_FILTER, true);
	if (!includeUnknown && key == UNKNOWN_LABEL)
		manageIndexEntry(&genreIndexAllKeys, key, remove);
}

void FileFilterIndex::managePlayerEntryInIndex(FileData* game, bool remove)
{
	// flag for including unknowns
	bool includeUnknown = INCLUDE_UNKNOWN;
	std::string key = getIndexableKey(game, PLAYER_FILTER, false);

	// only add unknown in pubdev IF both dev and pub are empty
	if (!includeUnknown && key == UNKNOWN_LABEL) {
		// no valid player info found
		return;
	}

	manageIndexEntry(&playersIndexAllKeys, key, remove);
}

void FileFilterIndex::managePubDevEntryInIndex(FileData* game, bool remove)
{
	std::string pub = getIndexableKey(game, PUBDEV_FILTER, false);
	std::string dev = getIndexableKey(game, PUBDEV_FILTER, true);

	// flag for including unknowns
	bool includeUnknown = INCLUDE_UNKNOWN;
	bool unknownPub = false;
	bool unknownDev = false;

	if (pub == UNKNOWN_LABEL)
		unknownPub = true;

	if (dev == UNKNOWN_LABEL)
		unknownDev = true;

	if (!includeUnknown && unknownDev && unknownPub) {
		// no valid rating info found
		return;
	}

	if (unknownDev && unknownPub) {
		// if no info at all
		manageIndexEntry(&pubDevIndexAllKeys, pub, remove);
	}
	else
	{
		if (!unknownDev) {
			// if no info at all
			manageIndexEntry(&pubDevIndexAllKeys, dev, remove);
		}
		if (!unknownPub) {
			// if no info at all
			manageIndexEntry(&pubDevIndexAllKeys, pub, remove);
		}
	}
}

void FileFilterIndex::manageRatingsEntryInIndex(FileData* game, bool remove)
{
	std::string key = getIndexableKey(game, RATINGS_FILTER, false);

	// flag for including unknowns
	bool includeUnknown = INCLUDE_UNKNOWN;

	if (!includeUnknown && key == UNKNOWN_LABEL) {
		// no valid rating info found
		return;
	}

	manageIndexEntry(&ratingsIndexAllKeys, key, remove);
}

void FileFilterIndex::manageYearEntryInIndex(FileData* game, bool remove)
{
	std::string key = getIndexableKey(game, YEAR_FILTER, false);

	// flag for including unknowns
	bool includeUnknown = INCLUDE_UNKNOWN;

	if (!includeUnknown && key == UNKNOWN_LABEL)
		return;

	manageIndexEntry(&yearIndexAllKeys, key, remove);
}

void FileFilterIndex::manageIndexEntry(std::map<std::string, int>* index, std::string key, bool remove, bool forceUnknown)
{
	bool includeUnknown = INCLUDE_UNKNOWN;
	if (!includeUnknown && key == UNKNOWN_LABEL && !forceUnknown)
		return;

	if (remove) 
	{
		// removing entry
		if (index->find(key) == index->cend())
		{
			// this shouldn't happen
			LOG(LogInfo) << "Couldn't find entry in index! " << key;
		}
		else
		{
			(index->at(key))--;
			if(index->at(key) <= 0)
				index->erase(key);			
		}

		return;
	}

	// adding entry
	if (index->find(key) == index->cend())
		(*index)[key] = 1;
	else
		(index->at(key))++;
}

void FileFilterIndex::clearIndex(std::map<std::string, int> indexMap)
{
	indexMap.clear();
}

bool CollectionFilter::create(const std::string name)
{
	mName = name;
	mPath = getCollectionsFolder() + "/" + mName + ".xcc";
	return save();
}

bool CollectionFilter::createFromSystem(const std::string name, SystemData* system)
{
	FileFilterIndex* filter = system->getFilterIndex();
	if (filter == nullptr)
		return false;

	mSystemFilter.clear();

	if (system != nullptr && !system->isCollection() && !system->isGroupSystem() && system->getName() != "all")
		mSystemFilter.insert(system->getName());

	resetIndex();

	mTextFilter = filter->mTextFilter;	
	importIndex(filter);

	for (auto& it : mFilterDecl)
	{
		FilterDataDecl& filterData = it.second;
		*(filterData.filteredByRef) = (filterData.currentFilteredKeys->size() > 0);
	}

	mName = name;
	mPath = getCollectionsFolder() + "/" + mName + ".xcc";
	
	return save();
}

bool CollectionFilter::load(const std::string file)
{
	if (!Utils::FileSystem::exists(file))
		return false;

	clearAllFilters();

	mPath = file;

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(file.c_str());

	if (!res)
	{
		LOG(LogError) << "Could not parse filter collection !";
		LOG(LogError) << res.description();
		return false;
	}

	//actually read the file
	pugi::xml_node root = doc.child("filter");
	if (!root)
	{
		LOG(LogError) << "filter collection is missing the <filter> tag!";
		return false;
	}

	if (root.attribute("name"))
		mName = root.attribute("name").value();
	else
		mName = Utils::FileSystem::getStem(file);
	
	for (pugi::xml_node node = root.first_child(); node; node = node.next_sibling())
	{
		std::string name = node.name();

		if (name == "system")
			mSystemFilter.insert(node.text().as_string());
		else if (name == "text")
			mTextFilter = node.text().as_string();
		else if (name == "genre")
			genreIndexFilteredKeys.insert(node.text().as_string());
		else if (name == "ratings")
			ratingsIndexFilteredKeys.insert(node.text().as_string());
		else if (name == "year")
			yearIndexFilteredKeys.insert(node.text().as_string());
		else if (name == "players")
			playersIndexFilteredKeys.insert(node.text().as_string());
		else if (name == "pubDev")
			pubDevIndexFilteredKeys.insert(node.text().as_string());
		else if (name == "favorites")
			favoritesIndexFilteredKeys.insert(node.text().as_string());
		else if (name == "kidGame")
			kidGameIndexFilteredKeys.insert(node.text().as_string());
		else if (name == "played")
			playedIndexFilteredKeys.insert(node.text().as_string());
		else if (name == "lang")
			langIndexFilteredKeys.insert(node.text().as_string());
		else if (name == "region")
			regionIndexFilteredKeys.insert(node.text().as_string());
	}

	for (auto& it : mFilterDecl)
	{
		FilterDataDecl& filterData = it.second;
		*(filterData.filteredByRef) = (filterData.currentFilteredKeys->size() > 0);
	}

	return true;
}

bool CollectionFilter::save()
{
	pugi::xml_document doc;
	pugi::xml_node root = doc.append_child("filter");
	root.append_attribute("name").set_value(mName.c_str());

	for (auto key : mSystemFilter)
		root.append_child("system").text().set(key.c_str());

	if (!mTextFilter.empty())
		root.append_child("text").text().set(mTextFilter.c_str());

	for (auto key : genreIndexFilteredKeys)
		root.append_child("genre").text().set(key.c_str());

	for (auto key : playersIndexFilteredKeys)
		root.append_child("players").text().set(key.c_str());

	for (auto key : pubDevIndexFilteredKeys)
		root.append_child("pubDev").text().set(key.c_str());

	for (auto key : ratingsIndexFilteredKeys)
		root.append_child("ratings").text().set(key.c_str());

	for (auto key : yearIndexFilteredKeys)
		root.append_child("year").text().set(key.c_str());

	for (auto key : favoritesIndexFilteredKeys)
		root.append_child("favorites").text().set(key.c_str());

	for (auto key : kidGameIndexFilteredKeys)
		root.append_child("kidGame").text().set(key.c_str());

	for (auto key : playedIndexFilteredKeys)
		root.append_child("played").text().set(key.c_str());

	for (auto key : langIndexFilteredKeys)
		root.append_child("lang").text().set(key.c_str());

	for (auto key : regionIndexFilteredKeys)
		root.append_child("region").text().set(key.c_str());

	if (!doc.save_file(mPath.c_str()))
	{
		LOG(LogError) << "Error saving CollectionFilter to \"" << mPath << "\" !";
		return false;
	}

	return true;
}

bool CollectionFilter::isFiltered()
{
	return mSystemFilter.size() > 0 || FileFilterIndex::isFiltered();
}

bool CollectionFilter::showFile(FileData* game)
{
	if (game == nullptr)
		return false;

	if (mSystemFilter.size() > 0 && game->getSourceFileData() != nullptr && game->getSourceFileData()->getSystem() != nullptr)
	{
		std::string system = game->getSourceFileData()->getSystem()->getName();
		if (!isSystemSelected(system))
			return false;
	}
	
	return FileFilterIndex::showFile(game);
}

bool CollectionFilter::isSystemSelected(const std::string name)
{
	if (mSystemFilter.size() == 0)
		return true;

	return mSystemFilter.find(name) != mSystemFilter.cend();
}

void CollectionFilter::setSystemSelected(const std::string name, bool value)
{
	auto sys = mSystemFilter.find(name);
	if (sys == mSystemFilter.cend())
	{
		if (value)
			mSystemFilter.insert(name);
	}
	else if (!value)			
		mSystemFilter.erase(sys);	
}

void CollectionFilter::resetSystemFilter()
{
	mSystemFilter.clear();
}