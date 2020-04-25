#pragma once

#include "components/DateTimeComponent.h"
#include "components/RatingComponent.h"
#include "components/ScrollableContainer.h"
#include "views/gamelist/BasicGameListView.h"

class VideoComponent;

class DetailedContainer
{
public:	
	enum DetailedContainerType
	{
		DetailedView,
		VideoView,
		GridView
	};

	DetailedContainer(ISimpleGameListView* parent, GuiComponent* list, Window* window, DetailedContainerType viewType);
	~DetailedContainer();

	void onThemeChanged(const std::shared_ptr<ThemeData>& theme);
	Vector3f getLaunchTarget();

	void updateControls(FileData* file, bool isClearing);

	std::vector<std::pair<std::string, TextComponent*>> getMDLabels();
	std::vector<std::pair<std::string, GuiComponent*>>  getMDValues();

	void update(int deltaTime);

protected:
	void	initMDLabels();
	void	initMDValues();

	const char* getName() { return mParent->getName(); }
	void addChild(GuiComponent* cmp) { mParent->addChild(cmp); }
	void removeChild(GuiComponent* cmp) { mParent->removeChild(cmp); }
	bool isChild(GuiComponent* cmp) { return mParent->isChild(cmp); }

	ISimpleGameListView* mParent;
	GuiComponent* mList;
	Window* mWindow;

	DetailedContainerType mViewType;

	ScrollableContainer mDescContainer;
	TextComponent mDescription;

	void createVideo();
	void createImageComponent(ImageComponent** pImage);
	void loadIfThemed(ImageComponent** pImage, const std::shared_ptr<ThemeData>& theme, const std::string& element, bool forceLoad = false, bool loadPath = false);

	ImageComponent* mImage;
	ImageComponent* mThumbnail;
	ImageComponent* mMarquee;
	VideoComponent* mVideo;

	ImageComponent* mFlag;

	ImageComponent* mKidGame;
	ImageComponent* mFavorite;
	ImageComponent* mHidden;

	TextComponent mLblRating, mLblReleaseDate, mLblDeveloper, mLblPublisher, mLblGenre, mLblPlayers, mLblLastPlayed, mLblPlayCount, mLblGameTime, mLblFavorite;
	TextComponent mDeveloper, mPublisher, mGenre, mPlayers, mPlayCount, mName, mGameTime, mTextFavorite;

	RatingComponent mRating;
	DateTimeComponent mReleaseDate, mLastPlayed;
};
