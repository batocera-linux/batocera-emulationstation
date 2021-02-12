#pragma once
#ifndef ES_APP_VIEWS_GAME_LIST_ISIMPLE_GAME_LIST_VIEW_H
#define ES_APP_VIEWS_GAME_LIST_ISIMPLE_GAME_LIST_VIEW_H

#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "views/gamelist/IGameListView.h"
#include <stack>
#include <set>

#ifdef _ENABLEEMUELEC
class MultiFunctionButton
{
public:
	enum class Action
	{
		none,
		shortPress,
		longPress
	};

	Action input(bool isPressed);
	Action update(int deltaTime);

private:
	int mTimeHoldingButton = 0;
	bool mIsPressed = false;
	bool mLongPressHasFired = false;
};
#endif

class ISimpleGameListView : public IGameListView
{
public:
	ISimpleGameListView(Window* window, FolderData* root, bool temporary = false);
	virtual ~ISimpleGameListView() {}

	// Called when a new file is added, a file is removed, a file's metadata changes, or a file's children are sorted.
	// NOTE: FILE_SORTED is only reported for the topmost FileData, where the sort started.
	//       Since sorts are recursive, that FileData's children probably changed too.
	virtual void onFileChanged(FileData* file, FileChangeType change);
	
	// Called whenever the theme changes.
	virtual void onThemeChanged(const std::shared_ptr<ThemeData>& theme);

	virtual FileData* getCursor() = 0;
	virtual void setCursor(FileData*) = 0;
	virtual int getCursorIndex() =0; // batocera
	virtual void setCursorIndex(int index) =0; // batocera

#ifdef _ENABLEEMUELEC
	virtual void update(int deltaTime) override;
#endif

	virtual bool input(InputConfig* config, Input input) override;
	virtual void launch(FileData* game) = 0;
	
	virtual std::vector<std::string> getEntriesLetters() override;
	virtual std::vector<FileData*> getFileDataEntries() = 0;

	void	moveToFolder(FolderData* folder);
	FolderData*		getCurrentFolder();

	virtual void repopulate() override;

	void setPopupContext(std::shared_ptr<IGameListView> pThis, std::shared_ptr<GuiComponent> parentView, const std::string label, const std::function<void()>& onExitTemporary);
	void closePopupContext();
	
	void moveToRandomGame();

protected:	
	void	  updateFolderPath();

	virtual std::string getQuickSystemSelectRightButton() = 0;
	virtual std::string getQuickSystemSelectLeftButton() = 0;
	virtual void populateList(const std::vector<FileData*>& files) = 0;
	
	TextComponent mHeaderText;
	ImageComponent mHeaderImage;
	ImageComponent mBackground;
	TextComponent mFolderPath;

	std::shared_ptr<IGameListView> mPopupSelfReference;
	std::shared_ptr<GuiComponent>  mPopupParentView;	
	std::function<void()> mOnExitPopup;

	std::vector<GuiComponent*> mThemeExtras;

	std::stack<FileData*> mCursorStack;
#ifdef _ENABLEEMUELEC
	MultiFunctionButton mFavOrRandomButton;
#endif
};

#endif // ES_APP_VIEWS_GAME_LIST_ISIMPLE_GAME_LIST_VIEW_H
