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
		{ PLAYED_FILTER, 	&playedIndexAllKeys,    &filterByPlayed,	&playedIndexFilteredKeys, 	"played",		false,				"",				_("ALREADY PLAYED") },
		{ CHEEVOS_FILTER, 	&cheevosIndexAllKeys,   &filterByCheevos,	&cheevosIndexFilteredKeys, 	"cheevos",		false,				"",				_("HAS ACHIEVEMENTS") },
		{ VERTICAL_FILTER, 	&verticalIndexAllKeys,  &filterByVertical,	&verticalIndexFilteredKeys, "vertical",		false,				"",				_("VERTICAL") }
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

void FileFilterIndex::copyFrom(FileFilterIndex* indexToImport)
{
	resetFilters();
	clearAllFilters();

	if (indexToImport == nullptr)
		return;

	mTextFilter = indexToImport->mTextFilter;
	mUseRelevency = indexToImport->mUseRelevency;

	for (auto decl : indexToImport->mFilterDecl)
	{
		auto src = mFilterDecl.find(decl.first);
		if (src == mFilterDecl.cend())
			continue;

		for (auto all : *decl.second.allIndexKeys)
			(*src->second.allIndexKeys)[all.first] = all.second;

		for (auto all : *decl.second.currentFilteredKeys)
			(*src->second.currentFilteredKeys).insert(all);

		*src->second.filteredByRef = *decl.second.filteredByRef;
	}
}

void FileFilterIndex::importIndex(FileFilterIndex* indexToImport)
{
	struct IndexImportStructure
	{
		std::map<std::string, int>* destinationIndex;
		std::map<std::string, int>* sourceIndex;
	};

	IndexImportStructure indexStructDecls[] = {
		{ &favoritesIndexAllKeys, &(indexToImport->favoritesIndexAllKeys) },
		{ &genreIndexAllKeys, &(indexToImport->genreIndexAllKeys) },
		{ &playersIndexAllKeys, &(indexToImport->playersIndexAllKeys) },
		{ &pubDevIndexAllKeys, &(indexToImport->pubDevIndexAllKeys) },
		{ &ratingsIndexAllKeys, &(indexToImport->ratingsIndexAllKeys) },
		{ &yearIndexAllKeys, &(indexToImport->yearIndexAllKeys) },
		{ &langIndexAllKeys, &(indexToImport->langIndexAllKeys) },
		{ &regionIndexAllKeys, &(indexToImport->regionIndexAllKeys) },
		{ &kidGameIndexAllKeys, &(indexToImport->kidGameIndexAllKeys) },
		{ &cheevosIndexAllKeys, &(indexToImport->cheevosIndexAllKeys) },
		{ &verticalIndexAllKeys, &(indexToImport->verticalIndexAllKeys) },
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
	mUseRelevency = false;
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
	clearIndex(cheevosIndexAllKeys);
	clearIndex(verticalIndexAllKeys);

	manageIndexEntry(&favoritesIndexAllKeys, "FALSE", false);
	manageIndexEntry(&favoritesIndexAllKeys, "TRUE", false);

	manageIndexEntry(&kidGameIndexAllKeys, "FALSE", false);
	manageIndexEntry(&kidGameIndexAllKeys, "TRUE", false);

	manageIndexEntry(&playedIndexAllKeys, "FALSE", false);
	manageIndexEntry(&playedIndexAllKeys, "TRUE", false);

	manageIndexEntry(&cheevosIndexAllKeys, "FALSE", false);
	manageIndexEntry(&cheevosIndexAllKeys, "TRUE", false);

	manageIndexEntry(&verticalIndexAllKeys, "FALSE", false);
	manageIndexEntry(&verticalIndexAllKeys, "TRUE", false);

	manageIndexEntry(&ratingsIndexAllKeys, "1 STAR", false);
	manageIndexEntry(&ratingsIndexAllKeys, "2 STARS", false);
	manageIndexEntry(&ratingsIndexAllKeys, "3 STARS", false);
	manageIndexEntry(&ratingsIndexAllKeys, "4 STARS", false);
	manageIndexEntry(&ratingsIndexAllKeys, "5 STARS", false);
}

std::string FileFilterIndex::getIndexableKey(FileData* game, FilterIndexType type, bool getSecondary)
{
	std::string key;
	switch (type)
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
		key = game->getMetadata(MetaDataId::Genre);
		if (!key.empty())
		{
			auto idx = key.find('/');

			if (!getSecondary && idx != std::string::npos)
				key = Utils::String::trim(key.substr(0, idx));
			else if (getSecondary && idx == std::string::npos)
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
		key = game->getMetadata(MetaDataId::Publisher);

		if ((getSecondary && !key.empty()) || (!getSecondary && key.empty()))
			key = game->getMetadata(MetaDataId::Developer);

		break;
	}

	case RATINGS_FILTER:
	{
		int ratingNumber = 0;
		if (getSecondary)
			return UNKNOWN_LABEL;
		
		std::string ratingString = game->getMetadata(MetaDataId::Rating);
		if (ratingString.empty())
			return UNKNOWN_LABEL;

		float rating = Utils::String::toFloat(ratingString);
		if (rating <= 0.0f)
			return UNKNOWN_LABEL;
			
		if (rating > 1.0f)
			rating = 1.0f;

		ratingNumber = (int)Math::round(rating * 5);
		return std::to_string(ratingNumber) + " STARS";					
	}

	case FAVORITES_FILTER:
	{
		if (game->getType() != GAME)
			return "FALSE";

		key = game->getMetadata(MetaDataId::Favorite);
		break;
	}

	case KIDGAME_FILTER:
	{
		if (game->getType() != GAME)
			return "FALSE";

		key = game->getMetadata(MetaDataId::KidGame);
		break;
	}

	case PLAYED_FILTER:
		return Utils::String::toInteger(game->getMetadata(MetaDataId::PlayCount)) == 0 ? "FALSE" : "TRUE";		

	case YEAR_FILTER:
		key = game->getMetadata(MetaDataId::ReleaseDate);
		key = (key.length() >= 4 && key[0] >= '1' && key[0] <= '2') ? key.substr(0, 4) : "";
		return key;

	case CHEEVOS_FILTER:
	{
		if (getSecondary)
			break;

		if (game->getType() != GAME)
			return "FALSE";

		return game->hasCheevos() ? "TRUE" : "FALSE";		
	}

	case VERTICAL_FILTER:
	{
		if (getSecondary)
			break;

		if (game->getType() != GAME)
			return "FALSE";

		return game->isVerticalArcadeGame() ? "TRUE" : "FALSE";		
	}
	}

	if (key.empty() || (type == RATINGS_FILTER && key == "0 STARS"))
		return UNKNOWN_LABEL;

	return Utils::String::toUpper(key);
}

void FileFilterIndex::addToIndex(FileData* game)
{
	game->detectLanguageAndRegion(false);

	manageGenreEntryInIndex(game);
	managePlayerEntryInIndex(game);
	managePubDevEntryInIndex(game);	
	manageYearEntryInIndex(game);
	manageLangEntryInIndex(game);
	manageRegionEntryInIndex(game);		
}

void FileFilterIndex::removeFromIndex(FileData* game)
{
	manageGenreEntryInIndex(game, true);
	managePlayerEntryInIndex(game, true);
	managePubDevEntryInIndex(game, true);	
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

std::unordered_set<std::string>* FileFilterIndex::getFilter(FilterIndexType type)
{
	std::vector<std::string> ret;
	auto it = mFilterDecl.find(type);
	if (it == mFilterDecl.cend())
		return nullptr;

	FilterDataDecl& filterData = it->second;
	if (!(*(filterData.filteredByRef)))
		return nullptr;

	return filterData.currentFilteredKeys;
}

void FileFilterIndex::clearAllFilters()
{
	mUseRelevency = false;
	mTextFilter = "";

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

void FileFilterIndex::setTextFilter(const std::string text, bool useRelevancy) 
{ 
	mTextFilter = text;
	mUseRelevency = useRelevancy;
}

float jw_distance(std::string s1, std::string s2, bool caseSensitive = true) {
	float m = 0;
	int low, high, range;
	int k = 0, numTrans = 0;

	// Exit early if either are empty
	if (s1.length() == 0 || s2.length() == 0) {
		return 0;
	}

	// Convert to lower if case-sensitive is false
	if (caseSensitive == false) {
		transform(s1.begin(), s1.end(), s1.begin(), ::tolower);
		transform(s2.begin(), s2.end(), s2.begin(), ::tolower);
	}

	// Exit early if they're an exact match.
	if (s1 == s2) {
		return 1;
	}

	range = (std::max(s1.length(), s2.length()) / 2) - 1;
	int s1Matches[65000] = {};
	int s2Matches[65000] = {};

	for (int i = 0; i < s1.length(); i++) {

		// Low Value;
		if (i >= range) {
			low = i - range;
		}
		else {
			low = 0;
		}

		// High Value;
		if (i + range <= (s2.length() - 1)) {
			high = i + range;
		}
		else {
			high = s2.length() - 1;
		}

		for (int j = low; j <= high; j++) {
			if (s1Matches[i] != 1 && s2Matches[j] != 1 && s1[i] == s2[j]) {
				m += 1;
				s1Matches[i] = 1;
				s2Matches[j] = 1;
				break;
			}
		}
	}

	// Exit early if no matches were found
	if (m == 0) {
		return 0;
	}

	// Count the transpositions.
	for (int i = 0; i < s1.length(); i++) {
		if (s1Matches[i] == 1) {
			int j;
			for (j = k; j < s2.length(); j++) {
				if (s2Matches[j] == 1) {
					k = j + 1;
					break;
				}
			}

			if (s1[i] != s2[j]) {
				numTrans += 1;
			}
		}
	}

	float weight = (m / s1.length() + m / s2.length() + (m - (numTrans / 2)) / m) / 3;
	float l = 0;
	float p = 0.1;
	if (weight > 0.7) {
		while (s1[l] == s2[l] && l < 4) {
			l += 1;
		}

		weight += l * p * (1 - weight);
	}
	return weight;
}

int FileFilterIndex::showFile(FileData* game)
{
	// this shouldn't happen, but just in case let's get it out of the way
	if (!isFiltered())
		return 1;

	// if folder, needs further inspection - i.e. see if folder contains at least one element
	// that should be shown
	if (game->getType() == FOLDER) 
	{
		std::vector<FileData*> children = ((FolderData*)game)->getChildren();
		// iterate through all of the children, until there's a match

		for (std::vector<FileData*>::const_iterator it = children.cbegin(); it != children.cend(); ++it )
			if (showFile(*it))
				return 1;

		return 0;
	}

	bool keepGoing = false;
	
	int textScore = 0;

	if (!mTextFilter.empty())
	{
		auto name = game->getSourceFileData()->getName();

		if (!mUseRelevency)
		{
			if (mTextFilter.find(',') == std::string::npos)
			{
				if (Utils::String::containsIgnoreCase(name, mTextFilter))
				{
					textScore = 1;
					keepGoing = true;
				}
			}
			else
			{
				for (auto token : Utils::String::split(mTextFilter, ',', true))
				{
					if (Utils::String::containsIgnoreCase(name, Utils::String::trim(token)))
					{
						textScore = 1;
						keepGoing = true;
						break;
					}
				}
			}
		}
		else
		{
			if (Utils::String::compareIgnoreCase(name, mTextFilter) == 0)
			{
				keepGoing = true;
				textScore = 1;
			}
			else if (Utils::String::startsWithIgnoreCase(name, mTextFilter))
			{
				keepGoing = true;
				textScore = 2;
			}
			else if (mTextFilter.find(' ') == std::string::npos && Utils::String::containsIgnoreCase(name, mTextFilter))
			{
				keepGoing = true;
				textScore = 3;
			}
			else if (mTextFilter.find(' ') != std::string::npos)
			{
				auto simplify = [](const std::string& text)
				{
					auto s = Utils::String::toLower(text);
					s = Utils::String::replace(s, ":", "");
					s = Utils::String::replace(s, ".", "");
					s = Utils::String::replace(s, " - ", " ");
					s = Utils::String::replace(s, "- ", " ");				

					std::vector<std::string> ret;

					for (auto v : Utils::String::split(s, ' '))
					{
						if (v.empty() || v.length() <= 2 || v == "and" || v == "not" || v == "for" || v == "the" || v == "les" || v == "des")
							continue;

						ret.push_back(v);
					}

					return ret;
				};

				auto filters = simplify(mTextFilter);
				auto words = simplify(name);

				int totalWords = 0;
				int commonWords = 0;
				
				for (int i = 0; i < filters.size(); i++)
				{
					auto filter = filters[i];

					for (auto word : words)
					{
						if (word == filter)
						{
							commonWords++;
							break;
						}
					}

					totalWords++;
				}
				
				int continuousWords = 0;
				int maxContinuousWords = 0;
				int wordsAtStart = 0;
				bool countStart = true;

				for (int j = 0 ; j < words.size(); j++)
				{
					auto word = words[j];

					for (int i = 0; i < filters.size(); i++)
					{
						auto filter = filters[i];

						if (word == filter)
						{
							if (countStart && i == j)
								wordsAtStart++;
							else
								countStart = false;

							continuousWords++;

							if (maxContinuousWords < continuousWords)
								maxContinuousWords = continuousWords;

							j++;

							if (j < words.size())
								word = words[j];
							else
								break;

							continue;
						}
						else
							countStart = false;

						continuousWords = 0;
					}					
				}
				
				if (commonWords > 0)
				{
					if (commonWords > 1 || (commonWords > 0 && filters.size() == 1))
					{
						int sc = ((wordsAtStart * 2) + (maxContinuousWords * 3) + commonWords);
						textScore = 1000 - sc;
						keepGoing = true;
					}
					else
					{
						auto dist = jw_distance(mTextFilter, name, false);
						if (dist > 0.66)
						{
							textScore = 1500 - (500 * dist);
							keepGoing = true;
						}
					}
				}
			}
		}
	}

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
				return 0;

			std::string secKey = getIndexableKey(game, filterData.type, true);
			if (secKey != UNKNOWN_LABEL)
				keepGoing = isKeyBeingFilteredBy(secKey, filterData.type);
		}

		// if still nothing, then it's not a match
		if (!keepGoing)
			return 0;		
	}

	if (keepGoing && !mTextFilter.empty())
		return textScore;
	
	if (mTextFilter.empty() && !hasFilter)
		return 0;
	
	return keepGoing ? 1 : 0;
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

	auto subkey = getIndexableKey(game, GENRE_FILTER, true);
	if (!includeUnknown && (subkey == UNKNOWN_LABEL || subkey == "BIOS"))
		return;	 // no valid genre info found

	manageIndexEntry(&genreIndexAllKeys, subkey, remove);
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

void FileFilterIndex::manageYearEntryInIndex(FileData* game, bool remove)
{
	manageIndexEntry(&yearIndexAllKeys, getIndexableKey(game, YEAR_FILTER, false), remove);
}

void FileFilterIndex::manageIndexEntry(std::map<std::string, int>* index, const std::string& key, bool remove, bool forceUnknown)
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
		else if (name == "cheevos")
			cheevosIndexFilteredKeys.insert(node.text().as_string());
		else if (name == "vertical")
			verticalIndexFilteredKeys.insert(node.text().as_string());
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

	for (auto key : cheevosIndexFilteredKeys)
		root.append_child("cheevos").text().set(key.c_str());

	for (auto key : verticalIndexFilteredKeys)
		root.append_child("vertical").text().set(key.c_str());

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

int CollectionFilter::showFile(FileData* game)
{
	if (game == nullptr)
		return 0;

	if (mSystemFilter.size() > 0 && game->getSourceFileData() != nullptr && game->getSourceFileData()->getSystem() != nullptr)
	{
		std::string system = game->getSourceFileData()->getSystem()->getName();
		if (!isSystemSelected(system))
			return 0;
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
