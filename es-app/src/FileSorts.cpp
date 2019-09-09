#include "FileSorts.h"

#include "utils/StringUtil.h"

namespace FileSorts
{
	const FolderData::SortType typesArr[] = {
		FolderData::SortType(&compareName, true, "filename, ascending"),
		FolderData::SortType(&compareName, false, "filename, descending"),

		FolderData::SortType(&compareRating, true, "rating, ascending"),
		FolderData::SortType(&compareRating, false, "rating, descending"),

		FolderData::SortType(&compareTimesPlayed, true, "times played, ascending"),
		FolderData::SortType(&compareTimesPlayed, false, "times played, descending"),

		FolderData::SortType(&compareLastPlayed, true, "last played, ascending"),
		FolderData::SortType(&compareLastPlayed, false, "last played, descending"),

		FolderData::SortType(&compareNumPlayers, true, "number players, ascending"),
		FolderData::SortType(&compareNumPlayers, false, "number players, descending"),

		FolderData::SortType(&compareReleaseDate, true, "release date, ascending"),
		FolderData::SortType(&compareReleaseDate, false, "release date, descending"),

		FolderData::SortType(&compareGenre, true, "genre, ascending"),
		FolderData::SortType(&compareGenre, false, "genre, descending"),

		FolderData::SortType(&compareDeveloper, true, "developer, ascending"),
		FolderData::SortType(&compareDeveloper, false, "developer, descending"),

		FolderData::SortType(&comparePublisher, true, "publisher, ascending"),
		FolderData::SortType(&comparePublisher, false, "publisher, descending"),

		FolderData::SortType(&compareSystem, true, "system, ascending"),
		FolderData::SortType(&compareSystem, false, "system, descending")
	};

	const std::vector<FolderData::SortType> SortTypes(typesArr, typesArr + sizeof(typesArr)/sizeof(typesArr[0]));

	//returns if file1 should come before file2
	bool compareName(const FileData* file1, const FileData* file2)
	{
		// we compare the actual metadata name, as collection files have the system appended which messes up the order
		std::string name1 = Utils::String::toUpper(file1->metadata.getName());
		std::string name2 = Utils::String::toUpper(file2->metadata.getName());
		return name1.compare(name2) < 0;
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
