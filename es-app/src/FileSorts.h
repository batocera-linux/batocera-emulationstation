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
		SYSTEM_DESCENDING = 19
	};

	class Singleton
	{
	public:
		Singleton();
	
		std::vector<FolderData::SortType> mSortTypes;
	};

	void reset();
	FolderData::SortType getSortType(int sortId);
	const std::vector<FolderData::SortType>& getSortTypes();

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
};
#endif // ES_APP_FILE_SORTS_H
