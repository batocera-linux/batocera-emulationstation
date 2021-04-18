#pragma once

#include "components/DateTimeComponent.h"
#include "components/RatingComponent.h"
#include "components/ScrollableContainer.h"
#include "views/gamelist/BasicGameListView.h"

class VideoComponent;
class ComponentGrid;

struct MdComponent
{
	std::string expectedType;

	std::string id;
	GuiComponent* component;

	std::string labelid;
	TextComponent* label;
};

struct MdImage
{
	MdImage(const std::string id, std::vector<MetaDataId> metaids)
	{
		this->id = id;
		metaDataIds = metaids;
		component = nullptr;
	}

	std::string id;
	
	std::vector<MetaDataId> metaDataIds;

	ImageComponent* component;
};

class DetailedContainer
{
	friend class DetailedContainerHost;

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

	void updateControls(FileData* file, bool isClearing, int moveBy = 0, bool isDeactivating = false);

protected:
	void	initMDLabels();
	void	initMDValues();

	std::vector<GuiComponent*>  getComponents();
	std::vector<MdComponent>  getMetaComponents();

	const char* getName() { return mParent->getName(); }
	void addChild(GuiComponent* cmp) { mParent->addChild(cmp); }
	void removeChild(GuiComponent* cmp) { mParent->removeChild(cmp); }
	bool isChild(GuiComponent* cmp) { return mParent->isChild(cmp); }

	void updateDetailsForFolder(FolderData* folder);
	void updateFolderViewAmbiantProperties();

	void disableComponent(GuiComponent* comp);

	bool hasActivationStoryboard(GuiComponent* comp, bool checkActivate = true, bool checkDeactivate = false);

	bool anyComponentHasStoryBoard();
	bool anyComponentHasStoryBoardRunning();

	ISimpleGameListView* mParent;
	GuiComponent* mList;
	Window* mWindow;

	DetailedContainerType mViewType;

	TextComponent mDescription;

	void createVideo();
	void createImageComponent(ImageComponent** pImage, bool forceLoad = false);
	void loadIfThemed(ImageComponent** pImage, const std::shared_ptr<ThemeData>& theme, const std::string& element, bool forceLoad = false, bool loadPath = false);

	ImageComponent* mImage;
	ImageComponent* mThumbnail;
	VideoComponent* mVideo;

	ImageComponent* mFlag;

	ImageComponent* mHidden;
	ImageComponent* mNotHidden;

	ImageComponent* mFavorite;
	ImageComponent* mNotFavorite;

	ImageComponent* mKidGame;
	ImageComponent* mNotKidGame;

	ImageComponent* mCheevos;
	ImageComponent* mNotCheevos;

	ImageComponent* mManual;
	ImageComponent* mNoManual;

	ImageComponent* mMap;
	ImageComponent* mNoMap;

	ImageComponent* mSaveState;
	ImageComponent* mNoSaveState;

	TextComponent mLblRating, mLblReleaseDate, mLblDeveloper, mLblPublisher, mLblGenre, mLblPlayers, mLblLastPlayed, mLblPlayCount, mLblGameTime, mLblFavorite;
	TextComponent mDeveloper, mPublisher, mGenre, mPlayers, mPlayCount, mName, mGameTime, mTextFavorite;

	RatingComponent mRating;
	DateTimeComponent mReleaseDate, mLastPlayed;

	std::vector<MdImage> mdImages;

	std::vector<GuiComponent*> mThemeExtras;

	void createFolderGrid(Vector2f targetSize, std::vector<std::string> thumbs);
	ComponentGrid* mFolderView;

	bool		mState;
};





class DetailedContainerHost
{
public:
	DetailedContainerHost(ISimpleGameListView* parent, GuiComponent* list, Window* window, DetailedContainer::DetailedContainerType viewType);
	~DetailedContainerHost();

	void onThemeChanged(const std::shared_ptr<ThemeData>& theme);
	Vector3f getLaunchTarget();

	void updateControls(FileData* file, bool isClearing, int moveBy = 0);
	void update(int deltaTime);

private:
	FileData* mActiveFile;

	ISimpleGameListView* mParent;
	GuiComponent*		mList;
	Window* mWindow;
	DetailedContainer::DetailedContainerType mViewType;

	DetailedContainer* mContainer;
	std::vector<DetailedContainer*> mContainers;
	std::shared_ptr<ThemeData> mTheme;
};