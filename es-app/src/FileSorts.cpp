#include "FileSorts.h"
#include "Locale.h"

namespace FileSorts
{
  std::vector<FileData::SortType> SortTypes;
  
  void init() {
	SortTypes.push_back(FileData::SortType(&compareFileName, true, "\uF15d " + _("FILENAME")));
	SortTypes.push_back(FileData::SortType(&compareFileName, false, "\uF15e " + _("FILENAME")));
	SortTypes.push_back(FileData::SortType(&compareRating, true, "\uF165 " + _("RATING")));
	SortTypes.push_back(FileData::SortType(&compareRating, false, "\uF164 " + _("RATING")));
	SortTypes.push_back(FileData::SortType(&compareTimesPlayed, true, "\uF160 " + _("TIMES PLAYED")));
	SortTypes.push_back(FileData::SortType(&compareTimesPlayed, false, "\uF161 " + _("TIMES PLAYED")));
	SortTypes.push_back(FileData::SortType(&compareLastPlayed, true, "\uF160 " + _("LAST PLAYED")));
	SortTypes.push_back(FileData::SortType(&compareLastPlayed, false, "\uF161 " + _("LAST PLAYED")));
	SortTypes.push_back(FileData::SortType(&compareNumberPlayers, true, "\uF162 " + _("NUMBER OF PLAYERS")));
	SortTypes.push_back(FileData::SortType(&compareNumberPlayers, false, "\uF163 " + _("NUMBER OF PLAYERS")));
	SortTypes.push_back(FileData::SortType(&compareDevelopper, true, "\uF15d " + _("DEVELOPER")));
	SortTypes.push_back(FileData::SortType(&compareDevelopper, false, "\uF15e " + _("DEVELOPER")));
	SortTypes.push_back(FileData::SortType(&compareGenre, true, "\uF15d " + _("GENRE")));
	SortTypes.push_back(FileData::SortType(&compareGenre, false, "\uF15e " + _("GENRE")));
  }

	//returns if file1 should come before file2
	bool compareFileName(const FileData* file1, const FileData* file2)
	{
		std::string name1 = file1->getName();
		std::string name2 = file2->getName();

		//min of name1/name2 .length()s
		unsigned int count = name1.length() > name2.length() ? name2.length() : name1.length();
		for(unsigned int i = 0; i < count; i++)
		{
			if(toupper(name1[i]) != toupper(name2[i]))
			{
				return toupper(name1[i]) < toupper(name2[i]);
			}
		}

		return name1.length() < name2.length();
	}

	bool compareRating(const FileData* file1, const FileData* file2)
	{
		//only games have rating metadata
		if(file1->metadata.getType() == GAME_METADATA && file2->metadata.getType() == GAME_METADATA)
		{
			return file1->metadata.getFloat("rating") < file2->metadata.getFloat("rating");
		}

		return false;
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
		//only games have lastplayed metadata
		if(file1->metadata.getType() == GAME_METADATA && file2->metadata.getType() == GAME_METADATA)
		{
			return (file1)->metadata.getTime("lastplayed") < (file2)->metadata.getTime("lastplayed");
		}

		return false;
	}

	bool compareNumberPlayers(const FileData* file1, const FileData* file2)
	{
		//only games have lastplayed metadata
		if(file1->metadata.getType() == GAME_METADATA && file2->metadata.getType() == GAME_METADATA)
		{
			return (file1)->metadata.getInt("players") < (file2)->metadata.getInt("players");
		}

		return false;
	}

	bool compareDevelopper(const FileData* file1, const FileData* file2)
	{
		//only games have developper metadata
		if(file1->metadata.getType() == GAME_METADATA && file2->metadata.getType() == GAME_METADATA)
		{
			std::string dev1 = file1->metadata.get("developer");
			std::string dev2 = file2->metadata.get("developer");

		//min of dev1/dev2 .length()s
		unsigned int count = dev1.length() > dev2.length() ? dev2.length() : dev1.length();
		for(unsigned int i = 0; i < count; i++)
		{
			if(toupper(dev1[i]) != toupper(dev2[i]))
			{
				return toupper(dev1[i]) < toupper(dev2[i]);
			}
		}
		}

		return false;
	}

	bool compareGenre(const FileData* file1, const FileData* file2)
	{
		//only games have genre metadata
		if(file1->metadata.getType() == GAME_METADATA && file2->metadata.getType() == GAME_METADATA)
		{
			std::string genre1 = file1->metadata.get("genre");
			std::string genre2 = file2->metadata.get("genre");

		//min of genre1/genre2 .length()s
		unsigned int count = genre1.length() > genre2.length() ? genre2.length() : genre1.length();
		for(unsigned int i = 0; i < count; i++)
		{
			if(toupper(genre1[i]) != toupper(genre2[i]))
			{
				return toupper(genre1[i]) < toupper(genre2[i]);
			}
		}
		}

		return false;
	}

};
