#include "GameNameFormatter.h"
#include "SystemData.h"
#include "FileData.h"
#include "FileSorts.h"
#include "Settings.h"
#include "utils/StringUtil.h"
#include "LocaleES.h"

#define FOLDERICON _U("\uF07C ")
#define FAVORITEICON _U("\uF006 ")
#define CHEEVOSICON _U("  \uF091")

GameNameFormatter::GameNameFormatter(SystemData* system)
{
	mSortId = system->getSortId();

	mShowCheevosIcon = system->getShowCheevosIcon();
	mShowFavoriteIcon = system->getShowFavoritesIcon();

	mShowYear =
		mSortId == FileSorts::RELEASEDATE_ASCENDING ||
		mSortId == FileSorts::RELEASEDATE_ASCENDING ||
		mSortId == FileSorts::SYSTEM_RELEASEDATE_ASCENDING ||
		mSortId == FileSorts::SYSTEM_RELEASEDATE_DESCENDING ||
		mSortId == FileSorts::RELEASEDATE_SYSTEM_ASCENDING ||
		mSortId == FileSorts::RELEASEDATE_SYSTEM_DESCENDING;

	mShowSystemFirst =
		mSortId == FileSorts::SYSTEM_ASCENDING ||
		mSortId == FileSorts::SYSTEM_DESCENDING ||
		mSortId == FileSorts::SYSTEM_RELEASEDATE_ASCENDING ||
		mSortId == FileSorts::SYSTEM_RELEASEDATE_DESCENDING ||
		mSortId == FileSorts::RELEASEDATE_SYSTEM_ASCENDING ||
		mSortId == FileSorts::RELEASEDATE_SYSTEM_DESCENDING;

	mShowSystemAfterYear =
		mSortId == FileSorts::RELEASEDATE_SYSTEM_ASCENDING ||
		mSortId == FileSorts::RELEASEDATE_SYSTEM_DESCENDING;

	mShowSystemName = (system->isGroupSystem() || system->isCollection()) && Settings::getInstance()->getBool("CollectionShowSystemInfo");
}

std::string valueOrDefault(const std::string value, const std::string defaultValue = _("Unknown"))
{
	if (value.empty())
		return defaultValue;

	return value;
}

#define RATINGSTAR _U("\uF005")

#define SEPARATOR_BEFORE "["
#define SEPARATOR_AFTER "] "

std::string GameNameFormatter::getDisplayName(FileData* fd, bool showFolderIcon)
{
	std::string name = fd->getName();

	if (mSortId == FileSorts::GENRE_ASCENDING || mSortId == FileSorts::GENRE_DESCENDING)
		name = SEPARATOR_BEFORE + valueOrDefault(fd->getSourceFileData()->getMetadata(MetaDataId::Genre)) + SEPARATOR_AFTER + name;
	else if (mSortId == FileSorts::TIMESPLAYED_ASCENDING || mSortId == FileSorts::TIMESPLAYED_DESCENDING)
		name = SEPARATOR_BEFORE + valueOrDefault(fd->getSourceFileData()->getMetadata(MetaDataId::PlayCount), "0") + SEPARATOR_AFTER + name;
	else if (mSortId == FileSorts::DEVELOPER_ASCENDING || mSortId == FileSorts::DEVELOPER_DESCENDING)
		name = SEPARATOR_BEFORE + valueOrDefault(fd->getSourceFileData()->getMetadata(MetaDataId::Developer)) + SEPARATOR_AFTER + name;
	else if (mSortId == FileSorts::PUBLISHER_ASCENDING || mSortId == FileSorts::PUBLISHER_DESCENDING)
		name = SEPARATOR_BEFORE + valueOrDefault(fd->getSourceFileData()->getMetadata(MetaDataId::Publisher)) + SEPARATOR_AFTER + name;
	else if (mSortId == FileSorts::NUMBERPLAYERS_ASCENDING || mSortId == FileSorts::NUMBERPLAYERS_DESCENDING)
		name = SEPARATOR_BEFORE + valueOrDefault(fd->getSourceFileData()->getMetadata(MetaDataId::Players), "1") + SEPARATOR_AFTER + name;
	else if (mSortId == FileSorts::RATING_ASCENDING || mSortId == FileSorts::RATING_DESCENDING)
	{
		int stars = (int) Math::round(Math::clamp(0.0f, 1.0f, Utils::String::toFloat(fd->getSourceFileData()->getMetadata(MetaDataId::Rating)))*5.0);
		std::string str;
		for (int i = 0; i < stars; i++)
			str += RATINGSTAR;

		name = name + " " + str;
	}

	if (mShowYear)
	{
		if (mShowSystemName && mShowSystemAfterYear)
			name = SEPARATOR_BEFORE + fd->getSourceFileData()->getSystemName() + SEPARATOR_AFTER + name;

		auto year = Utils::String::toInteger(fd->getMetadata(MetaDataId::ReleaseDate).substr(0, 4));
		if (year >= 1000 && year < 9999)
			name = SEPARATOR_BEFORE + std::to_string(year) + SEPARATOR_AFTER + name;
		else
			name = SEPARATOR_BEFORE + std::string("????") + SEPARATOR_AFTER + name;
	}

	if (mShowSystemName && !mShowSystemAfterYear)
	{
		if (mShowSystemFirst)
			name = SEPARATOR_BEFORE + fd->getSourceFileData()->getSystemName() + SEPARATOR_AFTER + name;
		else
			name = name + " [" + fd->getSourceFileData()->getSystemName() + "]";
	}

	if (mShowFavoriteIcon && fd->getFavorite())
	{
		if (mShowCheevosIcon && fd->hasCheevos())
			return FAVORITEICON + name + CHEEVOSICON;

		return FAVORITEICON + name;
	}

	if (fd->getType() == FOLDER)
	{
		if (showFolderIcon)
			return FOLDERICON + name;

		return name;
	}

	if (mShowCheevosIcon && fd->hasCheevos())
		return name + CHEEVOSICON;

	return name;
};