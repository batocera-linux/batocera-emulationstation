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
	FAMILY_FILTER = 2,
	PLAYER_FILTER = 3,
	PUBDEV_FILTER = 4,
	RATINGS_FILTER = 5,
	YEAR_FILTER = 6,
	KIDGAME_FILTER = 7,
	HIDDEN_FILTER = 8,
	PLAYED_FILTER = 9,
	LANG_FILTER = 10,
	REGION_FILTER = 11,
	FAVORITES_FILTER = 12,
	CHEEVOS_FILTER = 13,
	VERTICAL_FILTER = 14
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

private:
	FileFilterIndex(const FileFilterIndex&) { };
	FileFilterIndex& operator=(FileFilterIndex const&) { return *this; }

public:
	FileFilterIndex();
	~FileFilterIndex();

	void addToIndex(FileData* game);
	void removeFromIndex(FileData* game);
	void setFilter(FilterIndexType type, std::vector<std::string>* values);
	std::unordered_set<std::string>* getFilter(FilterIndexType type);

	void clearAllFilters();
	
	virtual int showFile(FileData* game);
	virtual bool isFiltered() { return (!mTextFilter.empty() || filterByGenre || filterByPlayers || filterByPubDev || filterByFamily
		|| filterByRatings || filterByFavorites || filterByKidGame || filterByPlayed || filterByLang || filterByRegion || filterByYear || filterByCheevos || filterByVertical); };

	bool isKeyBeingFilteredBy(std::string key, FilterIndexType type);
	std::vector<FilterDataDecl> getFilterDataDecls();

	void importIndex(FileFilterIndex* indexToImport);
	void copyFrom(FileFilterIndex* indexToImport);

	void resetIndex();
	void resetFilters();
	void setUIModeFilters();

	void setTextFilter(const std::string text, bool useRelevancy = false);
	inline const std::string getTextFilter() { return mTextFilter; }
	inline bool hasRelevency() { return !mTextFilter.empty() && mUseRelevency; }

protected:
	//std::vector<FilterDataDecl> filterDataDecl;
	std::map<int, FilterDataDecl> mFilterDecl;

	std::string getIndexableKey(FileData* game, FilterIndexType type, bool getSecondary);

	void manageGenreEntryInIndex(FileData* game, bool remove = false);
	void manageFamilyEntryInIndex(FileData* game, bool remove = false);
	void managePlayerEntryInIndex(FileData* game, bool remove = false);
	void managePubDevEntryInIndex(FileData* game, bool remove = false);	
	void manageYearEntryInIndex(FileData* game, bool remove = false);
	void manageIndexEntry(std::map<std::string, int>* index, const std::string& key, bool remove, bool forceUnknown = false);

	void manageLangEntryInIndex(FileData* game, bool remove = false);
	void manageRegionEntryInIndex(FileData* game, bool remove = false);

	void clearIndex(std::map<std::string, int> indexMap);

	bool filterByGenre;
	bool filterByFamily;
	bool filterByPlayers;
	bool filterByPubDev;
	bool filterByRatings;
	bool filterByFavorites;
	bool filterByYear;
	bool filterByKidGame;
	bool filterByPlayed;
	bool filterByLang;
	bool filterByRegion;
	bool filterByCheevos;
	bool filterByVertical;

	std::map<std::string, int> genreIndexAllKeys;
	std::map<std::string, int> familyIndexAllKeys;
	std::map<std::string, int> playersIndexAllKeys;
	std::map<std::string, int> pubDevIndexAllKeys;
	std::map<std::string, int> ratingsIndexAllKeys;
	std::map<std::string, int> favoritesIndexAllKeys;
	std::map<std::string, int> yearIndexAllKeys;
	std::map<std::string, int> kidGameIndexAllKeys;
	std::map<std::string, int> playedIndexAllKeys;
	std::map<std::string, int> langIndexAllKeys;
	std::map<std::string, int> regionIndexAllKeys;
	std::map<std::string, int> cheevosIndexAllKeys;

	std::map<std::string, int> verticalIndexAllKeys;

	std::unordered_set<std::string> genreIndexFilteredKeys;
	std::unordered_set<std::string> familyIndexFilteredKeys;
	std::unordered_set<std::string> playersIndexFilteredKeys;
	std::unordered_set<std::string> pubDevIndexFilteredKeys;
	std::unordered_set<std::string> ratingsIndexFilteredKeys;
	std::unordered_set<std::string> favoritesIndexFilteredKeys;
	std::unordered_set<std::string> yearIndexFilteredKeys;
	std::unordered_set<std::string> kidGameIndexFilteredKeys;
	std::unordered_set<std::string> playedIndexFilteredKeys;
	std::unordered_set<std::string> langIndexFilteredKeys;
	std::unordered_set<std::string> regionIndexFilteredKeys;
	std::unordered_set<std::string> cheevosIndexFilteredKeys;
	std::unordered_set<std::string> verticalIndexFilteredKeys;

	std::string mTextFilter;
	bool		mUseRelevency;
};

class CollectionFilter : public FileFilterIndex
{
public:	
	bool create(const std::string name);
	bool createFromSystem(const std::string name, SystemData* system);

	bool load(const std::string file);
	bool save();

	int showFile(FileData* game) override;
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
