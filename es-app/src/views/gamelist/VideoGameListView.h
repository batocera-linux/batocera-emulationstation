#pragma once
#ifndef ES_APP_VIEWS_GAME_LIST_VIDEO_GAME_LIST_VIEW_H
#define ES_APP_VIEWS_GAME_LIST_VIDEO_GAME_LIST_VIEW_H

#include "components/DateTimeComponent.h"
#include "components/RatingComponent.h"
#include "views/gamelist/BasicGameListView.h"
#include "DetailedContainer.h"

class VideoComponent;

class VideoGameListView : public BasicGameListView
{
public:
	VideoGameListView(Window* window, FolderData* root);

	virtual void onShow() override;

	virtual void onThemeChanged(const std::shared_ptr<ThemeData>& theme) override;

	virtual const char* getName() const override
	{
		if (!mCustomThemeName.empty())
			return mCustomThemeName.c_str();

		return "video";
	}

	virtual void launch(FileData* game) override;

protected:
	virtual void update(int deltaTime) override;

private:	
	void updateInfoPanel();
	DetailedContainer mDetails;	
};

#endif // ES_APP_VIEWS_GAME_LIST_VIDEO_GAME_LIST_VIEW_H
