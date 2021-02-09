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
		mSortTypes.push_back(SortType(FILECREATION_DATE_ASCENDING, &compareFileCreationDate, true, _("FILE CREATION DATE, ASCENDING"), _U("\uF160 ")));
		mSortTypes.push_back(SortType(FILECREATION_DATE_DESCENDING, &compareFileCreationDate, false, _("FILE CREATION DATE, DESCENDING"), _U("\uF161 ")));
		mSortTypes.push_back(SortType(GAMETIME_ASCENDING, &compareGameTime, true, _("GAME TIME, ASCENDING"), _U("\uF160 ")));
		mSortTypes.push_back(SortType(GAMETIME_DESCENDING, &compareGameTime, false, _("GAME TIME, DESCENDING"), _U("\uF161 ")));
	}

	//returns if file1 should come before file2
	bool compareName(const FileData* file1, const FileData* file2)
	{
		if (file1->getType() != file2->getType())
			return file1->getType() == FOLDER;

		// we compare the actual metadata name, as collection files have the system appended which messes up the order		
		return Utils::String::compareIgnoreCase(((FileData*)file1)->getName(), ((FileData*)file2)->getName()) < 0;
	}

	bool compareRating(const FileData* file1, const FileData* file2)
	{
		return file1->getMetadata().getFloat(MetaDataId::Rating) < file2->getMetadata().getFloat(MetaDataId::Rating);
	}

	bool compareTimesPlayed(const FileData* file1, const FileData* file2)
	{
		//only games have playcount metadata
		if (file1->getMetadata().getType() == GAME_METADATA && file2->getMetadata().getType() == GAME_METADATA)
			return (file1)->getMetadata().getInt(MetaDataId::PlayCount) < (file2)->getMetadata().getInt(MetaDataId::PlayCount);

		return false;
	}

	bool compareGameTime(const FileData* file1, const FileData* file2)
	{
		//only games have playcount metadata
		if (file1->getMetadata().getType() == GAME_METADATA && file2->getMetadata().getType() == GAME_METADATA)
			return (file1)->getMetadata().getInt(MetaDataId::GameTime) < (file2)->getMetadata().getInt(MetaDataId::GameTime);

		return false;
	}

	bool compareLastPlayed(const FileData* file1, const FileData* file2)
	{
		// since it's stored as an ISO string (YYYYMMDDTHHMMSS), we can compare as a string
		// as it's a lot faster than the time casts and then time comparisons
		return (file1)->getMetadata().get(MetaDataId::LastPlayed) < (file2)->getMetadata().get(MetaDataId::LastPlayed);
	}

	bool compareNumPlayers(const FileData* file1, const FileData* file2)
	{
		return (file1)->getMetadata().getInt(MetaDataId::Players) < (file2)->getMetadata().getInt(MetaDataId::Players);
	}

	bool compareReleaseDate(const FileData* file1, const FileData* file2)
	{
		// since it's stored as an ISO string (YYYYMMDDTHHMMSS), we can compare as a string
		// as it's a lot faster than the time casts and then time comparisons
		return (file1)->getMetadata().get(MetaDataId::ReleaseDate) < (file2)->getMetadata().get(MetaDataId::ReleaseDate);
	}

	bool compareFileCreationDate(const FileData* file1, const FileData* file2)
	{
		// As this sort mode is rarely used, don't care about storing date, always ask the file system
		auto dt1 = Utils::FileSystem::getFileCreationDate(file1->getPath()).getIsoString();
		auto dt2 = Utils::FileSystem::getFileCreationDate(file2->getPath()).getIsoString();
		return dt1 < dt2;
	}

	bool compareGenre(const FileData* file1, const FileData* file2)
	{
		std::string genre1 = file1->getMetadata().get(MetaDataId::Genre);
		std::string genre2 = file2->getMetadata().get(MetaDataId::Genre);
		return Utils::String::compareIgnoreCase(genre1, genre2) < 0;
	}

	bool compareDeveloper(const FileData* file1, const FileData* file2)
	{
		std::string developer1 = file1->getMetadata().get(MetaDataId::Developer);
		std::string developer2 = file2->getMetadata().get(MetaDataId::Developer);
		return Utils::String::compareIgnoreCase(developer1, developer2) < 0;
	}

	bool comparePublisher(const FileData* file1, const FileData* file2)
	{
		std::string publisher1 = file1->getMetadata().get(MetaDataId::Publisher);
		std::string publisher2 = file2->getMetadata().get(MetaDataId::Publisher);
		return Utils::String::compareIgnoreCase(publisher1, publisher2) < 0;
	}

	bool compareSystem(const FileData* file1, const FileData* file2)
	{
		std::string system1 = file1->getSystemName();
		std::string system2 = file2->getSystemName();
		return Utils::String::compareIgnoreCase(system1, system2) < 0;		
	}
};
