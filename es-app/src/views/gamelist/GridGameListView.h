#pragma once
#ifndef ES_APP_VIEWS_GAME_LIST_GRID_GAME_LIST_VIEW_H
#define ES_APP_VIEWS_GAME_LIST_GRID_GAME_LIST_VIEW_H

#include "components/DateTimeComponent.h"
#include "components/RatingComponent.h"
#include "components/ImageGridComponent.h"
#include "views/gamelist/ISimpleGameListView.h"
#include "views/gamelist/BasicGameListView.h"
#include "DetailedContainer.h"

class VideoComponent;

class GridGameListView : public ISimpleGameListView
{
public:
	GridGameListView(Window* window, FolderData* root, const std::shared_ptr<ThemeData>& theme, std::string customThemeName = "", Vector2f gridSize = Vector2f(0,0));

	virtual void onThemeChanged(const std::shared_ptr<ThemeData>& theme) override;

	virtual FileData* getCursor() override;
	virtual void setCursor(FileData*) override;
	virtual int getCursorIndex() override; // batocera
	virtual void setCursorIndex(int index) override; // batocera

	virtual bool input(InputConfig* config, Input input) override;

	virtual const char* getName() const override
	{
		if (!mCustomThemeName.empty())
			return mCustomThemeName.c_str();

		return "grid";
	}

	virtual void launch(FileData* game) override;
	virtual void onFileChanged(FileData* file, FileChangeType change);

	virtual void setThemeName(std::string name);
	virtual void onShow();
	virtual std::vector<FileData*> getFileDataEntries() override;
	virtual void update(int deltaTime) override;

protected:
	virtual std::string getQuickSystemSelectRightButton() override;
	virtual std::string getQuickSystemSelectLeftButton() override;
	virtual void populateList(const std::vector<FileData*>& files) override;
	virtual void remove(FileData* game) override;
	virtual void addPlaceholder();

	ImageGridComponent<FileData*> mGrid;

private:
	DetailedContainerHost mDetails;

	void updateInfoPanel();
	const std::string getImagePath(FileData* file);
	const bool isVirtualFolder(FileData* file);
};

#endif // ES_APP_VIEWS_GAME_LIST_GRID_GAME_LIST_VIEW_H
