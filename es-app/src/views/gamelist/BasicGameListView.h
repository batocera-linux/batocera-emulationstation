#pragma once
#ifndef ES_APP_VIEWS_GAME_LIST_BASIC_GAME_LIST_VIEW_H
#define ES_APP_VIEWS_GAME_LIST_BASIC_GAME_LIST_VIEW_H

#include "components/TextListComponent.h"
#include "views/gamelist/ISimpleGameListView.h"

class BasicGameListView : public ISimpleGameListView, public ILongMouseClickEvent
{
public:
	BasicGameListView(Window* window, FolderData* root);

	// Called when a FileData* is added, has its metadata changed, or is removed
	virtual void onFileChanged(FileData* file, FileChangeType change);

	virtual void onThemeChanged(const std::shared_ptr<ThemeData>& theme);

	virtual FileData* getCursor() override;
	virtual void setCursor(FileData* file) override;
	virtual int getCursorIndex() override;
	virtual void setCursorIndex(int index) override;
	virtual void resetLastCursor() override;
	virtual bool onMouseWheel(int delta) override;
	virtual void onShow() override;

	virtual const char* getName() const override
	{
		if (!mCustomThemeName.empty())
			return mCustomThemeName.c_str();

		return "basic";
	}

	virtual void launch(FileData* game) override;
	virtual std::vector<FileData*> getFileDataEntries() override;

	virtual void onLongMouseClick(GuiComponent* component) override;

protected:
	virtual std::string getQuickSystemSelectRightButton() override;
	virtual std::string getQuickSystemSelectLeftButton() override;
	virtual void populateList(const std::vector<FileData*>& files) override;
	virtual void remove(FileData* game) override;
	virtual void addPlaceholder();

	TextListComponent<FileData*> mList;
};

#endif // ES_APP_VIEWS_GAME_LIST_BASIC_GAME_LIST_VIEW_H
