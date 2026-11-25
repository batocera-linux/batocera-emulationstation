#include <Settings.h>
#include "FileSorts.h"

#include "utils/StringUtil.h"
#include "LocaleES.h"
#include <memory>
#include <unordered_map>
#include <utility>

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

		mSortTypes.push_back(SortType(SYSTEM_RELEASEDATE_ASCENDING, &compareSystemReleaseYear, true, _("SYSTEM, RELEASE YEAR, ASCENDING"), _U("\uF160 ")));
		mSortTypes.push_back(SortType(SYSTEM_RELEASEDATE_DESCENDING, &compareSystemReleaseYear, false, _("SYSTEM, RELEASE YEAR, DESCENDING"), _U("\uF161 ")));
		mSortTypes.push_back(SortType(RELEASEDATE_SYSTEM_ASCENDING, &compareReleaseYearSystem, true, _("RELEASE YEAR, SYSTEM, ASCENDING"), _U("\uF160 ")));
		mSortTypes.push_back(SortType(RELEASEDATE_SYSTEM_DESCENDING, &compareReleaseYearSystem, false, _("RELEASE YEAR, SYSTEM, DESCENDING"), _U("\uF161 ")));
	}

	namespace
	{
		int getGameSortValue(const FileData* file, MetaDataId field)
		{
			const auto& metadata = file->getMetadata();
			return metadata.getType() == GAME_METADATA
				? metadata.getInt(field) : 0;
		}

		template<typename GetKey, typename Less>
		std::function<bool(const FileData*, const FileData*)>
		prepareKeys(const std::vector<FileData*>& files,
			GetKey getKey, Less less)
		{
			using Key = decltype(getKey(files.front()));
			using Keys = std::unordered_map<const FileData*, Key>;

			auto keys = std::make_shared<Keys>();
			keys->reserve(files.size());

			for (auto file : files)
			{
				if (keys->find(file) == keys->end())
					keys->emplace(file, getKey(file));
			}

			return [keys, less](const FileData* first, const FileData* second)
			{
				return less(keys->at(first), keys->at(second));
			};
		}

		struct SystemYearKey
		{
			std::string system;
			std::string year;
			std::string name;
		};
	}

	std::function<bool(const FileData*, const FileData*)> prepareComparison(
		const std::vector<FileData*>& files, const SortType& sort)
	{
		auto comparison = sort.comparisonFunction;

		auto textLess = [](const std::string& first, const std::string& second)
		{
			return Utils::String::compareIgnoreCase(first, second) < 0;
		};

		if (comparison == &compareName)
		{
			const bool ignoreArticles = Settings::IgnoreLeadingArticles();
			const auto articles = Utils::String::commaStringToVector(_("A,AN,THE"));

			return prepareKeys(files,
				[ignoreArticles, articles](const FileData* file)
				{
					auto name = file->getSortName();
					return ignoreArticles
						? stripLeadingArticle(name, articles) : name;
				}, textLess);
		}

		if (comparison == &compareRating)
		{
			return prepareKeys(files,
				[](const FileData* file)
				{
					return file->getMetadata().getFloat(MetaDataId::Rating);
				}, std::less<float>());
		}

		if (comparison == &compareTimesPlayed || comparison == &compareGameTime || comparison == &compareNumPlayers)
		{
			const auto field = comparison == &compareTimesPlayed
				? MetaDataId::PlayCount
				: comparison == &compareGameTime
					? MetaDataId::GameTime : MetaDataId::Players;

			const bool gamesOnly = comparison != &compareNumPlayers;

			return prepareKeys(files,
				[field, gamesOnly](const FileData* file)
				{
					return gamesOnly
						? getGameSortValue(file, field)
						: file->getMetadata().getInt(field);
				}, std::less<int>());
		}

		if (comparison == &compareLastPlayed || comparison == &compareReleaseDate)
		{
			const auto field = comparison == &compareLastPlayed
				? MetaDataId::LastPlayed : MetaDataId::ReleaseDate;

			return prepareKeys(files,
				[field](const FileData* file)
				{
					return file->getMetadata().get(field);
				}, std::less<std::string>());
		}

		if (comparison == &compareGenre || comparison == &compareDeveloper || comparison == &comparePublisher)
		{
			const auto field = comparison == &compareGenre
				? MetaDataId::Genre
				: comparison == &compareDeveloper
					? MetaDataId::Developer : MetaDataId::Publisher;

			return prepareKeys(files,
				[field](const FileData* file)
				{
					return file->getMetadata().get(field);
				}, textLess);
		}

		if (comparison == &compareSystem)
		{
			return prepareKeys(files,
				[](const FileData* file)
				{
					return const_cast<FileData*>(file)
						->getSourceFileData()->getSystemName();
				}, textLess);
		}

		if (comparison == &compareFileCreationDate)
		{
			return prepareKeys(files,
				[](const FileData* file)
				{
					return Utils::FileSystem::getFileCreationDate(
						file->getPath()).getIsoString();
				}, std::less<std::string>());
		}

		if (comparison == &compareSystemReleaseYear || comparison == &compareReleaseYearSystem)
		{
			const bool systemFirst = comparison == &compareSystemReleaseYear;

			return prepareKeys(files,
				[](const FileData* file)
				{
					SystemYearKey key;
					key.system = const_cast<FileData*>(file)
						->getSourceFileData()->getSystemName();
					key.year = file->getMetadata()
						.get(MetaDataId::ReleaseDate).substr(0, 4);
					key.name = const_cast<FileData*>(file)->getName();
					return key;
				},
				[systemFirst, textLess](
					const SystemYearKey& first, const SystemYearKey& second)
				{
					if (systemFirst)
					{
						if (first.system != second.system)
							return textLess(first.system, second.system);

						if (first.year != second.year)
							return first.year < second.year;
					}
					else
					{
						if (first.year != second.year)
							return first.year < second.year;

						if (first.system != second.system)
							return textLess(first.system, second.system);
					}

					return textLess(first.name, second.name);
				});
		}

		return comparison;
	}

	//returns if file1 should come before file2
	bool compareName(const FileData* file1, const FileData* file2)
	{
		// we compare the actual metadata name, as collection files have the system appended which messes up the order
		const std::string& name1 = file1->getSortName();
		const std::string& name2 = file2->getSortName();

		if (Settings::IgnoreLeadingArticles())
		{
			static auto articles = Utils::String::commaStringToVector(_("A,AN,THE"));
			auto name1a = stripLeadingArticle(name1, articles);
			auto name2a = stripLeadingArticle(name2, articles);

			return Utils::String::compareIgnoreCase(name1a, name2a) < 0;
		}

		return Utils::String::compareIgnoreCase(name1, name2) < 0;
	}

	std::string stripLeadingArticle(const std::string &string, const std::vector<std::string> &articles)
	{
		const auto candidate = Utils::String::trim(string);
		const auto index = candidate.find_first_of(" \t\r\n");
		if (index == std::string::npos)
		{
			return string;
		}
		const auto maybeArticle = candidate.substr(0, index);
		for (auto &article: articles)
		{
			if (Utils::String::compareIgnoreCase(article, maybeArticle) == 0)
			{
				return Utils::String::trim(candidate.substr(article.length()));
			}
		}
		return string;
	}

	bool compareRating(const FileData* file1, const FileData* file2)
	{
		return file1->getMetadata().getFloat(MetaDataId::Rating) < file2->getMetadata().getFloat(MetaDataId::Rating);
	}

	bool compareTimesPlayed(const FileData* file1, const FileData* file2)
	{
		return getGameSortValue(file1, MetaDataId::PlayCount) < getGameSortValue(file2, MetaDataId::PlayCount);
	}

	bool compareGameTime(const FileData* file1, const FileData* file2)
	{
		return getGameSortValue(file1, MetaDataId::GameTime) < getGameSortValue(file2, MetaDataId::GameTime);
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

	bool compareSystemReleaseYear(const FileData* file1, const FileData* file2)
	{
		std::string system1 = ((FileData*)file1)->getSourceFileData()->getSystemName();
		std::string system2 = ((FileData*)file2)->getSourceFileData()->getSystemName();

		if (system1 == system2)
		{
			std::string year1 = file1->getMetadata().get(MetaDataId::ReleaseDate).substr(0, 4);
			std::string year2 = file2->getMetadata().get(MetaDataId::ReleaseDate).substr(0, 4);

			if (year1 == year2)
				return Utils::String::compareIgnoreCase(((FileData*)file1)->getName(), ((FileData*)file2)->getName()) < 0;

			return year1 < year2;
		}
		return Utils::String::compareIgnoreCase(system1, system2) < 0;
	}

	bool compareReleaseYearSystem(const FileData* file1, const FileData* file2)
	{
		std::string year1 = file1->getMetadata().get(MetaDataId::ReleaseDate).substr(0, 4);
		std::string year2 = file2->getMetadata().get(MetaDataId::ReleaseDate).substr(0, 4);

		if (year1 == year2)
		{
			std::string system1 = ((FileData*)file1)->getSourceFileData()->getSystemName();
			std::string system2 = ((FileData*)file2)->getSourceFileData()->getSystemName();

			if (system1 == system2)
				return Utils::String::compareIgnoreCase(((FileData*)file1)->getName(), ((FileData*)file2)->getName()) < 0;

			return Utils::String::compareIgnoreCase(system1, system2) < 0;
		}

		return year1 < year2;
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
		std::string system1 = ((FileData*)file1)->getSourceFileData()->getSystemName();
		std::string system2 = ((FileData*)file2)->getSourceFileData()->getSystemName();
		return Utils::String::compareIgnoreCase(system1, system2) < 0;		
	}
};
