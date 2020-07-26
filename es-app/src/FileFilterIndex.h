#pragma once
#ifndef ES_APP_FILE_FILTER_INDEX_H
#define ES_APP_FILE_FILTER_INDEX_H

#include <map>
#include <vector>
#include <unordered_set>
#include <string>

class FileData;
class SystemData;

enum FilterIndexType
{
	NONE = 0,
	GENRE_FILTER = 1,
	PLAYER_FILTER = 2,
	PUBDEV_FILTER = 3,
	RATINGS_FILTER = 4,
	FAVORITES_FILTER = 5,
	KIDGAME_FILTER = 6,
	HIDDEN_FILTER = 7,
	PLAYED_FILTER = 8,

	LANG_FILTER = 9,
	REGION_FILTER = 10
};

struct FilterDataDecl
{
	FilterIndexType type; // type of filter
	std::map<std::string, int>* allIndexKeys; // all possible filters for this type
	bool* filteredByRef; // is it filtered by this type
	std::unordered_set<std::string>* currentFilteredKeys; // current keys being filtered for
	std::string primaryKey; // primary key in metadata
	bool hasSecondaryKey; // has secondary key for comparison
	std::string secondaryKey; // what's the secondary key
	std::string menuLabel; // text to show in menu
};

class FileFilterIndex
{
	friend class CollectionFilter;

public:
	FileFilterIndex();
	~FileFilterIndex();

	void addToIndex(FileData* game);
	void removeFromIndex(FileData* game);
	void setFilter(FilterIndexType type, std::vector<std::string>* values);
	void clearAllFilters();
	
	virtual bool showFile(FileData* game);
	virtual bool isFiltered() { return (!mTextFilter.empty() || filterByGenre || filterByPlayers || filterByPubDev 
		|| filterByRatings || filterByFavorites || filterByKidGame || filterByPlayed || filterByLang || filterByRegion); };

	bool isKeyBeingFilteredBy(std::string key, FilterIndexType type);
	std::vector<FilterDataDecl> getFilterDataDecls();

	void importIndex(FileFilterIndex* indexToImport);
	void resetIndex();
	void resetFilters();
	void setUIModeFilters();

	void setTextFilter(const std::string text);
	inline const std::string getTextFilter() { return mTextFilter; }

protected:
	//std::vector<FilterDataDecl> filterDataDecl;
	std::map<int, FilterDataDecl> mFilterDecl;

	std::string getIndexableKey(FileData* game, FilterIndexType type, bool getSecondary);

	void manageGenreEntryInIndex(FileData* game, bool remove = false);
	void managePlayerEntryInIndex(FileData* game, bool remove = false);
	void managePubDevEntryInIndex(FileData* game, bool remove = false);
	void manageRatingsEntryInIndex(FileData* game, bool remove = false);	
	void manageIndexEntry(std::map<std::string, int>* index, std::string key, bool remove, bool forceUnknown = false);

	void manageLangEntryInIndex(FileData* game, bool remove = false);
	void manageRegionEntryInIndex(FileData* game, bool remove = false);

	void clearIndex(std::map<std::string, int> indexMap);

	bool filterByGenre;
	bool filterByPlayers;
	bool filterByPubDev;
	bool filterByRatings;
	bool filterByFavorites;
//	bool filterByHidden;
	bool filterByKidGame;
	bool filterByPlayed;
	bool filterByLang;
	bool filterByRegion;

	std::map<std::string, int> genreIndexAllKeys;
	std::map<std::string, int> playersIndexAllKeys;
	std::map<std::string, int> pubDevIndexAllKeys;
	std::map<std::string, int> ratingsIndexAllKeys;
	std::map<std::string, int> favoritesIndexAllKeys;
	//std::map<std::string, int> hiddenIndexAllKeys;
	std::map<std::string, int> kidGameIndexAllKeys;
	std::map<std::string, int> playedIndexAllKeys;
	std::map<std::string, int> langIndexAllKeys;
	std::map<std::string, int> regionIndexAllKeys;

	std::unordered_set<std::string> genreIndexFilteredKeys;
	std::unordered_set<std::string> playersIndexFilteredKeys;
	std::unordered_set<std::string> pubDevIndexFilteredKeys;
	std::unordered_set<std::string> ratingsIndexFilteredKeys;
	std::unordered_set<std::string> favoritesIndexFilteredKeys;
	//std::unordered_set<std::string> hiddenIndexFilteredKeys;
	std::unordered_set<std::string> kidGameIndexFilteredKeys;
	std::unordered_set<std::string> playedIndexFilteredKeys;
	std::unordered_set<std::string> langIndexFilteredKeys;
	std::unordered_set<std::string> regionIndexFilteredKeys;

	FileData* mRootFolder;

	std::string mTextFilter;
};

class CollectionFilter : public FileFilterIndex
{
public:	
	bool create(const std::string name);
	bool createFromSystem(const std::string name, SystemData* system);

	bool load(const std::string file);
	bool save();

	bool showFile(FileData* game) override;
	bool isFiltered() override;

	bool isSystemSelected(const std::string name);
	void setSystemSelected(const std::string name, bool value);
	void resetSystemFilter();

protected:
	std::string mName;
	std::string mPath;
	std::unordered_set<std::string> mSystemFilter;
};

#endif // ES_APP_FILE_FILTER_INDEX_H
