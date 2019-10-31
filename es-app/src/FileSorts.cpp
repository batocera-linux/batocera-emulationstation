#include "FileSorts.h"

#include "utils/StringUtil.h"
#include "LocaleES.h"

namespace FileSorts
{
	static Singleton* sInstance = nullptr;

	Singleton* getInstance()
	{
		if (sInstance == nullptr)
			sInstance = new Singleton();

		return sInstance;
	}

	void reset()
	{
		if (sInstance != nullptr)
			delete sInstance;

		sInstance = nullptr;
	}

	const std::vector<SortType>& getSortTypes()
	{
		return getInstance()->mSortTypes;
	}

	SortType getSortType(int sortId)
	{
		for (auto sort : getSortTypes())
			if (sort.id == sortId)
				return sort;

		return getSortTypes().at(0);
	}

	Singleton::Singleton()
	{
		mSortTypes.push_back(SortType(FILENAME_ASCENDING, &compareName, true, _("FILENAME, ASCENDING"), _U("\uF15d ")));
		mSortTypes.push_back(SortType(FILENAME_DESCENDING, &compareName, false, _("FILENAME, DESCENDING"), _U("\uF15e ")));
		mSortTypes.push_back(SortType(RATING_ASCENDING, &compareRating, true, _("RATING, ASCENDING"), _U("\uF165 ")));
		mSortTypes.push_back(SortType(RATING_DESCENDING, &compareRating, false, _("RATING, DESCENDING"), _U("\uF164 ")));
		mSortTypes.push_back(SortType(TIMESPLAYED_ASCENDING, &compareTimesPlayed, true, _("TIMES PLAYED, ASCENDING"), _U("\uF160 ")));
		mSortTypes.push_back(SortType(TIMESPLAYED_DESCENDING, &compareTimesPlayed, false, _("TIMES PLAYED, DESCENDING"), _U("\uF161 ")));
		mSortTypes.push_back(SortType(LASTPLAYED_ASCENDING, &compareLastPlayed, true, _("LAST PLAYED, ASCENDING"), _U("\uF160 ")));
		mSortTypes.push_back(SortType(LASTPLAYED_DESCENDING, &compareLastPlayed, false, _("LAST PLAYED, DESCENDING"), _U("\uF161 ")));
		mSortTypes.push_back(SortType(NUMBERPLAYERS_ASCENDING, &compareNumPlayers, true, _("NUMBER PLAYERS, ASCENDING"), _U("\uF162 ")));
		mSortTypes.push_back(SortType(NUMBERPLAYERS_DESCENDING, &compareNumPlayers, false, _("NUMBER PLAYERS, DESCENDING"), _U("\uF163 ")));
		mSortTypes.push_back(SortType(RELEASEDATE_ASCENDING, &compareReleaseDate, true, _("RELEASE DATE, ASCENDING"), _U("\uF160 ")));
		mSortTypes.push_back(SortType(RELEASEDATE_DESCENDING, &compareReleaseDate, false, _("RELEASE DATE, DESCENDING"), _U("\uF161 ")));
		mSortTypes.push_back(SortType(GENRE_ASCENDING, &compareGenre, true, _("GENRE, ASCENDING"), _U("\uF15d ")));		
		mSortTypes.push_back(SortType(GENRE_DESCENDING, &compareGenre, false, _("GENRE, DESCENDING"), _U("\uF15e ")));
		mSortTypes.push_back(SortType(DEVELOPER_ASCENDING, &compareDeveloper, true, _("DEVELOPER, ASCENDING"), _U("\uF15d ")));
		mSortTypes.push_back(SortType(DEVELOPER_DESCENDING, &compareDeveloper, false, _("DEVELOPER, DESCENDING"), _U("\uF15e ")));
		mSortTypes.push_back(SortType(PUBLISHER_ASCENDING, &comparePublisher, true, _("PUBLISHER, ASCENDING"), _U("\uF15d ")));
		mSortTypes.push_back(SortType(PUBLISHER_DESCENDING, &comparePublisher, false, _("PUBLISHER, DESCENDING"), _U("\uF15e ")));
		mSortTypes.push_back(SortType(SYSTEM_ASCENDING, &compareSystem, true, _("SYSTEM, ASCENDING"), _U("\uF15d ")));
		mSortTypes.push_back(SortType(SYSTEM_DESCENDING, &compareSystem, false, _("SYSTEM, DESCENDING"), _U("\uF15e ")));
	}

	//returns if file1 should come before file2
	bool compareName(const FileData* file1, const FileData* file2)
	{
		if (file1->getType() != file2->getType())
			return file1->getType() == FOLDER;

		// we compare the actual metadata name, as collection files have the system appended which messes up the order		
		std::string name1 = ((FileData*)file1)->getName();
		std::string name2 = ((FileData*)file2)->getName();

		for (auto ap = name1.c_str(), bp = name2.c_str(); ; ap++, bp++)
		{			
			if (*ap == 0 & *bp != 0)
				return true;

			if (*ap == 0 || *bp == 0)
				return false;

			auto c1 = toupper(*ap);
			auto c2 = toupper(*bp);
			if (c1 != c2)
				return c1 < c2;			
		}

		return false;
	}

	bool compareRating(const FileData* file1, const FileData* file2)
	{
		return file1->metadata.getFloat("rating") < file2->metadata.getFloat("rating");
	}

	bool compareTimesPlayed(const FileData* file1, const FileData* file2)
	{
		//only games have playcount metadata
		if(file1->metadata.getType() == GAME_METADATA && file2->metadata.getType() == GAME_METADATA)
		{
			return (file1)->metadata.getInt("playcount") < (file2)->metadata.getInt("playcount");
		}

		return false;
	}

	bool compareLastPlayed(const FileData* file1, const FileData* file2)
	{
		// since it's stored as an ISO string (YYYYMMDDTHHMMSS), we can compare as a string
		// as it's a lot faster than the time casts and then time comparisons
		return (file1)->metadata.get("lastplayed") < (file2)->metadata.get("lastplayed");
	}

	bool compareNumPlayers(const FileData* file1, const FileData* file2)
	{
		return (file1)->metadata.getInt("players") < (file2)->metadata.getInt("players");
	}

	bool compareReleaseDate(const FileData* file1, const FileData* file2)
	{
		// since it's stored as an ISO string (YYYYMMDDTHHMMSS), we can compare as a string
		// as it's a lot faster than the time casts and then time comparisons
		return (file1)->metadata.get("releasedate") < (file2)->metadata.get("releasedate");
	}

	bool compareGenre(const FileData* file1, const FileData* file2)
	{
		std::string genre1 = Utils::String::toUpper(file1->metadata.get("genre"));
		std::string genre2 = Utils::String::toUpper(file2->metadata.get("genre"));
		return genre1.compare(genre2) < 0;
	}

	bool compareDeveloper(const FileData* file1, const FileData* file2)
	{
		std::string developer1 = Utils::String::toUpper(file1->metadata.get("developer"));
		std::string developer2 = Utils::String::toUpper(file2->metadata.get("developer"));
		return developer1.compare(developer2) < 0;
	}

	bool comparePublisher(const FileData* file1, const FileData* file2)
	{
		std::string publisher1 = Utils::String::toUpper(file1->metadata.get("publisher"));
		std::string publisher2 = Utils::String::toUpper(file2->metadata.get("publisher"));
		return publisher1.compare(publisher2) < 0;
	}

	bool compareSystem(const FileData* file1, const FileData* file2)
	{
		std::string system1 = Utils::String::toUpper(file1->getSystemName());
		std::string system2 = Utils::String::toUpper(file2->getSystemName());
		return system1.compare(system2) < 0;
	}
};
