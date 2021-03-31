#pragma once
#ifndef ES_APP_FILE_SORTS_H
#define ES_APP_FILE_SORTS_H

#include "FileData.h"
#include <vector>

namespace FileSorts
{
	enum SortId : unsigned int
	{
		FILENAME_ASCENDING = 0,
		FILENAME_DESCENDING = 1,
		RATING_ASCENDING = 2,
		RATING_DESCENDING = 3,
		TIMESPLAYED_ASCENDING = 4,
		TIMESPLAYED_DESCENDING = 5,
		LASTPLAYED_ASCENDING = 6,
		LASTPLAYED_DESCENDING = 7,
		NUMBERPLAYERS_ASCENDING = 8,
		NUMBERPLAYERS_DESCENDING = 9,
		RELEASEDATE_ASCENDING = 10,
		RELEASEDATE_DESCENDING = 11,
		GENRE_ASCENDING = 12,
		GENRE_DESCENDING = 13,
		DEVELOPER_ASCENDING = 14,
		DEVELOPER_DESCENDING = 15,
		PUBLISHER_ASCENDING = 16,
		PUBLISHER_DESCENDING = 17,
		SYSTEM_ASCENDING = 18,
		SYSTEM_DESCENDING = 19,
		FILECREATION_DATE_ASCENDING = 20,
		FILECREATION_DATE_DESCENDING = 21,
		GAMETIME_ASCENDING = 22,
		GAMETIME_DESCENDING = 23,

		SYSTEM_RELEASEDATE_ASCENDING = 24,
		SYSTEM_RELEASEDATE_DESCENDING = 25,
		RELEASEDATE_SYSTEM_ASCENDING = 26,
		RELEASEDATE_SYSTEM_DESCENDING = 27
	};

	typedef bool ComparisonFunction(const FileData* a, const FileData* b);

	struct SortType
	{
		int id;
		ComparisonFunction* comparisonFunction;
		bool ascending;
		std::string description;
		std::string icon;

		SortType(int sortId, ComparisonFunction* sortFunction, bool sortAscending, const std::string & sortDescription, const std::string & iconId = "")
			: id(sortId), comparisonFunction(sortFunction), ascending(sortAscending), description(sortDescription), icon(iconId) {}
	};

	class Singleton
	{
	public:
		Singleton();
	
		std::vector<SortType> mSortTypes;
	};

	void reset();
	SortType getSortType(int sortId);
	const std::vector<SortType>& getSortTypes();

	bool compareName(const FileData* file1, const FileData* file2);
	bool compareRating(const FileData* file1, const FileData* file2);
	bool compareTimesPlayed(const FileData* file1, const FileData* fil2);
	bool compareLastPlayed(const FileData* file1, const FileData* file2);
	bool compareNumPlayers(const FileData* file1, const FileData* file2);
	bool compareReleaseDate(const FileData* file1, const FileData* file2);
	bool compareGenre(const FileData* file1, const FileData* file2);
	bool compareDeveloper(const FileData* file1, const FileData* file2);
	bool comparePublisher(const FileData* file1, const FileData* file2);
	bool compareSystem(const FileData* file1, const FileData* file2);
	bool compareFileCreationDate(const FileData* file1, const FileData* file2);
	bool compareGameTime(const FileData* file1, const FileData* file2);	

	bool compareSystemReleaseYear(const FileData* file1, const FileData* file2);
	bool compareReleaseYearSystem(const FileData* file1, const FileData* file2);
};
#endif // ES_APP_FILE_SORTS_H
