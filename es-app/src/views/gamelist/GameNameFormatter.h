#pragma once

#include <string>

class SystemData;
class FileData;

class GameNameFormatter
{
public:
	GameNameFormatter(SystemData* system);

	std::string getDisplayName(FileData* fd, bool showFolderIcon = true);

private:
	unsigned int mSortId;
	bool mShowCheevosIcon;
	bool mShowFavoriteIcon;

	bool mShowSystemName;
	bool mShowYear;
	bool mShowSystemFirst;
	bool mShowSystemAfterYear;

	bool mShowManualIcon;		
	bool mShowSaveStates;

	int mShowFlags;
};