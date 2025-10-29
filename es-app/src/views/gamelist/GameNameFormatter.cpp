#include "GameNameFormatter.h"
#include "SystemData.h"
#include "FileData.h"
#include "FileSorts.h"
#include "Settings.h"
#include "utils/StringUtil.h"

#include "LangParser.h"
#include "LocaleES.h"
#include "SaveStateRepository.h"
#include "CollectionSystemManager.h"

#define FOLDERICON	 _U("\uF07C ")
#define FAVORITEICON _U("\uF006 ")
#define FINISHEDICON _U("\uF11E ")

#define CHEEVOSICON _U("\uF091")
#define SAVESTATE	_U("\uF0C7")
#define MANUAL		_U("\uF02D")

#define GUN			_U("\uF05B")
#define WHEEL			_U("\uF1B9")
#define TRACKBALL		_U("\uF111")
#define SPINNER 		_U("\uF10C")

#define RATINGSTAR _U("\uF005")
#define SEPARATOR_BEFORE "["
#define SEPARATOR_AFTER "] "

std::map<std::string, std::string> langFlag =
{
	{ "au", _U("\uF300") },
	{ "br", _U("\uF301") },
	{ "ca", _U("\uF302") },
	{ "ch", _U("\uF303") },
	{ "de", _U("\uF304") },
	{ "es", _U("\uF305") },
	{ "eu", _U("\uF306") },
	{ "fr", _U("\uF307") },
	{ "gr", _U("\uF308") },
	{ "in", _U("\uF309") },
	{ "it", _U("\uF30A") },
	{ "jp", _U("\uF30B") },
	{ "kr", _U("\uF30C") },
	{ "nl", _U("\uF30D") },
	{ "no", _U("\uF30E") },
	{ "pt", _U("\uF30F") },
	{ "ru", _U("\uF310") },
	{ "sw", _U("\uF311") },
	{ "uk", _U("\uF312") },
	{ "us", _U("\uF313") },
	{ "wr", _U("\uF314") },
	{ "pl", _U("\uF315") }
};

std::string getLangFlag(const std::string lang)
{
	auto it = langFlag.find(lang);
	if (it == langFlag.cend())
		return "";

	return it->second;
};

GameNameFormatter::GameNameFormatter(SystemData* system)
{
	mSortId = system->getSortId();

	mShowCheevosIcon = system->getShowCheevosIcon() && system->getBoolSetting("ShowCheevosIcon");
	mShowFavoriteIcon = system->getShowFavoritesIcon();
	mShowFinishedIcon = system->getBoolSetting("ShowFinished");

	mShowManualIcon = system->getBoolSetting("ShowManualIcon");
	mShowSaveStates = system->getBoolSetting("ShowSaveStates");

	mShowGunIcon = system->getName() != "lightgun" && system->getBoolSetting("ShowGunIconOnGames");
	mShowWheelIcon = system->getName() != "wheel" && system->getBoolSetting("ShowWheelIconOnGames");
	mShowTrackballIcon = system->getName() != "trackball" && system->getBoolSetting("ShowTrackballIconOnGames");
	mShowSpinnerIcon = system->getName() != "spinner" && system->getBoolSetting("ShowSpinnerIconOnGames");

	mShowFlags = system->getShowFlags();

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

	mShowGameTime =
		mSortId == FileSorts::GAMETIME_ASCENDING ||
		mSortId == FileSorts::GAMETIME_DESCENDING;

	mShowSystemName = (!system->isGameSystem() || system->isCollection()) && Settings::getInstance()->getBool("CollectionShowSystemInfo");

	if (mShowSystemName && !system->isGameSystem() && system->getFolderViewMode() != "never")
		mShowSystemName = false;
}

std::string valueOrDefault(const std::string value, const std::string defaultValue = _("Unknown"))
{
	if (value.empty())
		return defaultValue;

	return value;
}

std::string GameNameFormatter::getDisplayName(FileData* fd, bool showFolderIcon)
{
	std::string name = fd->getName();

	bool showSystemNameByFile = (fd->getType() == GAME || fd->getParent() == nullptr || fd->getParent()->getName() != "collections");
	if (showSystemNameByFile)
	{
		/*if (fd->getSystem()->isGroupChildSystem())
			showSystemNameByFile = fd->getSystem()->getRootFolder()->getChildren().size() > 1;
		else */if (!fd->getSystem()->isGameSystem())
			showSystemNameByFile = false;
	}

	if (mSortId != FileSorts::FILENAME_ASCENDING && mSortId != FileSorts::FILENAME_DESCENDING)
	{
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
			int stars = (int)Math::round(Math::clamp(0.0f, 1.0f, Utils::String::toFloat(fd->getSourceFileData()->getMetadata(MetaDataId::Rating)))*5.0);
			std::string str;
			for (int i = 0; i < stars; i++)
				str += RATINGSTAR;

			name = name + " " + str;
		}
	}

	if (mShowYear)
	{
		if (showSystemNameByFile && mShowSystemName && mShowSystemAfterYear)
			name = SEPARATOR_BEFORE + fd->getSourceFileData()->getSystemName() + SEPARATOR_AFTER + name;

		auto year = Utils::String::toInteger(fd->getMetadata(MetaDataId::ReleaseDate).substr(0, 4));
		if (year >= 1000 && year < 9999)
			name = SEPARATOR_BEFORE + std::to_string(year) + SEPARATOR_AFTER + name;
		else
			name = SEPARATOR_BEFORE + std::string("????") + SEPARATOR_AFTER + name;
	}

	if (mShowGameTime)
	{
		int seconds = atol(fd->getMetadata(MetaDataId::GameTime).c_str());
		if (seconds > 0)
		{
			int h = 0, m = 0, s = 0;
			h = (seconds / 3600) % 24;
			m = (seconds / 60) % 60;
			s = seconds % 60;

			std::string timeText;
			if (h > 0)
				timeText = Utils::String::format("%02d:%02d:%02d", h, m, s);
			else 
				timeText = Utils::String::format("%02d:%02d", m, s);

			name = name + " [" + timeText + "]";
		}
	}

	if (showSystemNameByFile && mShowSystemName && !mShowSystemAfterYear)
	{
		if (mShowSystemFirst)
			name = SEPARATOR_BEFORE + fd->getSourceFileData()->getSystemName() + SEPARATOR_AFTER + name;
		else
			name = name + " [" + fd->getSourceFileData()->getSystemName() + "]";
	}

	std::string lang;
	if (mShowFlags == 1)
		lang = getLangFlag(LangInfo::getFlag(fd->getMetadata(MetaDataId::Language), fd->getMetadata(MetaDataId::Region))) + " ";

	std::vector<std::string> after;

	if (mShowGunIcon && fd->getSourceFileData()->isLightGunGame())
		after.push_back(GUN);

	if (mShowWheelIcon && fd->getSourceFileData()->isWheelGame())
		after.push_back(WHEEL);

	if (mShowTrackballIcon && fd->getSourceFileData()->isTrackballGame())
		after.push_back(TRACKBALL);

	if (mShowSpinnerIcon && fd->getSourceFileData()->isSpinnerGame())
		after.push_back(SPINNER);

	if (mShowCheevosIcon && fd->hasCheevos())
		after.push_back(CHEEVOSICON);

	if (mShowFinishedIcon && fd->getFinished())
		after.push_back(FINISHEDICON);	
		
	bool saves = mShowSaveStates && fd->getSourceFileData()->getSystem()->getSaveStateRepository()->hasSaveStates(fd);
	if (saves)
		after.push_back(SAVESTATE);

	bool manual = mShowManualIcon && (!fd->getMetadata(MetaDataId::Manual).empty() || !fd->getMetadata(MetaDataId::Magazine).empty());
	if (manual)
		after.push_back(MANUAL);

	if (mShowFlags == 2)
		after.push_back(getLangFlag(LangInfo::getFlag(fd->getMetadata(MetaDataId::Language), fd->getMetadata(MetaDataId::Region))));

	std::string langAfter = "  " + Utils::String::join(after, " ");

	if (mShowFavoriteIcon && fd->getFavorite())
	{
	//	if (mShowCheevosIcon && fd->hasCheevos())
	//		return lang + FAVORITEICON + name + CHEEVOSICON + langAfter;

		return lang + FAVORITEICON + name + langAfter;
	}

	if (fd->getType() == FOLDER)
	{
		if (showFolderIcon)
			return FOLDERICON + name;

		return name;
	}

	//if (mShowCheevosIcon && fd->hasCheevos())
//		return lang + name + CHEEVOSICON + langAfter;

	return lang + name + langAfter;
};
