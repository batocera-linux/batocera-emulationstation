#pragma once
#ifndef ES_APP_VIEWS_GAME_LIST_DETAILED_GAME_LIST_VIEW_H
#define ES_APP_VIEWS_GAME_LIST_DETAILED_GAME_LIST_VIEW_H

#include "components/DateTimeComponent.h"
#include "components/RatingComponent.h"
#include "components/ScrollableContainer.h"
#include "views/gamelist/BasicGameListView.h"
#include "DetailedContainer.h"

class DetailedGameListView : public BasicGameListView
{
public:
	DetailedGameListView(Window* window, FolderData* root);

	virtual void onThemeChanged(const std::shared_ptr<ThemeData>& theme) override;
	virtual void onShow() override;

	virtual const char* getName() const override
	{
		if (!mCustomThemeName.empty())
			return mCustomThemeName.c_str();

		return "detailed";
	}

	virtual void launch(FileData* game) override;

protected:
	virtual void update(int deltaTime) override;

private:
	void updateInfoPanel();

	DetailedContainerHost mDetails;
};

#endif // ES_APP_VIEWS_GAME_LIST_DETAILED_GAME_LIST_VIEW_H
