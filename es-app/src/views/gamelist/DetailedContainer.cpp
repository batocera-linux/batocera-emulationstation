#include "DetailedContainer.h"

#include "animations/LambdaAnimation.h"
#include "views/ViewController.h"
#include "FileData.h"
#include "SystemData.h"
#include "LocaleES.h"
#include "LangParser.h"
#include "SaveStateRepository.h"
#include "SystemConf.h"
#include "Window.h"
#include "components/ComponentGrid.h"
#include <set>
#include "BindingManager.h"

#ifdef _RPI_
#include "Settings.h"
#include "components/VideoPlayerComponent.h"
#endif
#include "components/VideoVlcComponent.h"

DetailedContainer::DetailedContainer(ISimpleGameListView* parent, GuiComponent* list, Window* window, DetailedContainerType viewType) :
	mParent(parent), mList(list), mWindow(window), mViewType(viewType),
	mDescription(window),
	mImage(nullptr), mVideo(nullptr), mThumbnail(nullptr), mFlag(nullptr),
	mKidGame(nullptr), mNotKidGame(nullptr), mHidden(nullptr), 
	mGunGame(nullptr), mNotGunGame(nullptr),
	mWheelGame(nullptr), mNotWheelGame(nullptr),
	mTrackballGame(nullptr), mNotTrackballGame(nullptr),
	mSpinnerGame(nullptr), mNotSpinnerGame(nullptr),
	mFavorite(nullptr), mNotFavorite(nullptr),
	mManual(nullptr), mNoManual(nullptr), 
	mMap(nullptr), mNoMap(nullptr),
	mCheevos(nullptr), mNotCheevos(nullptr),
	mNetplay(nullptr), mNotNetplay(nullptr),
	mSaveState(nullptr), mNoSaveState(nullptr),
	mState(true), mFolderView(nullptr),

	mLblRating(window), mLblReleaseDate(window), mLblDeveloper(window), mLblPublisher(window),
	mLblGenre(window), mLblPlayers(window), mLblLastPlayed(window), mLblPlayCount(window), mLblGameTime(window), mLblFavorite(window),

	mRating(window), mReleaseDate(window), mDeveloper(window), mPublisher(window),
	mGenre(window), mPlayers(window), mLastPlayed(window), mPlayCount(window),
	mName(window), mGameTime(window), mTextFavorite(window)	
{
	std::vector<MdImage> mdl = 
	{ 
		// Metadata that can be substituted if not found
		{ "md_marquee", { MetaDataId::Marquee, MetaDataId::Wheel } },
		{ "md_fanart", { MetaDataId::FanArt, MetaDataId::TitleShot, MetaDataId::Image } },
		{ "md_titleshot", { MetaDataId::TitleShot, MetaDataId::Image } },
		{ "md_boxart", { MetaDataId::BoxArt, MetaDataId::Thumbnail } },		
		{ "md_wheel",{ MetaDataId::Wheel, MetaDataId::Marquee } },
		{ "md_cartridge",{ MetaDataId::Cartridge } },
		{ "md_boxback",{ MetaDataId::BoxBack } },		
		{ "md_mix",{ MetaDataId::Mix, MetaDataId::Image, MetaDataId::Thumbnail } },

		// Medias  that can't be substituted even if not found
		{ "md_image_only", { MetaDataId::Image } },
		{ "md_thumbnail_only", { MetaDataId::Thumbnail } },
		{ "md_marquee_only", { MetaDataId::Marquee } },
		{ "md_wheel_only", { MetaDataId::Wheel } },
		{ "md_fanart_only", { MetaDataId::FanArt } },
		{ "md_titleshot_only", { MetaDataId::TitleShot } },
		{ "md_boxart_only", { MetaDataId::BoxArt } },
		{ "md_boxback_only", { MetaDataId::BoxBack } },		
		{ "md_cartridge_only",{ MetaDataId::Cartridge } },
		{ "md_boxback_only",{ MetaDataId::BoxBack } },
		{ "md_mix_only", { MetaDataId::Mix } }
	};

	for (auto md : mdl)
		mdImages.push_back(md);

	const float padding = 0.01f;
	auto mSize = mParent->getSize();

	if (mViewType == DetailedContainerType::GridView)
	{
		mLblRating.setVisible(false);
		mLblReleaseDate.setVisible(false);
		mLblDeveloper.setVisible(false);
		mLblPublisher.setVisible(false);
		mLblGenre.setVisible(false);
		mLblPlayers.setVisible(false);
		mLblLastPlayed.setVisible(false);
		mLblPlayCount.setVisible(false);
		mName.setVisible(false);
		mPlayCount.setVisible(false);
		mLastPlayed.setVisible(false);
		mPlayers.setVisible(false);
		mGenre.setVisible(false);
		mPublisher.setVisible(false);
		mDeveloper.setVisible(false);
		mReleaseDate.setVisible(false);
		mRating.setVisible(false);
		mDescription.setVisible(false);
	}

	// metadata labels + values
	mLblGameTime.setVisible(false);
	mGameTime.setVisible(false);
	mLblFavorite.setVisible(false);
	mTextFavorite.setVisible(false);

	// image
	if (mViewType == DetailedContainerType::DetailedView)
		createImageComponent(&mImage);

	mLblRating.setText(_("Rating") + ": ");
	addChild(&mLblRating);
	addChild(&mRating);
	mLblReleaseDate.setText(_("Released") + ": ");
	addChild(&mLblReleaseDate);
	addChild(&mReleaseDate);
	mLblDeveloper.setText(_("Developer") + ": ");
	addChild(&mLblDeveloper);
	addChild(&mDeveloper);
	mLblPublisher.setText(_("Publisher") + ": ");
	addChild(&mLblPublisher);
	addChild(&mPublisher);
	mLblGenre.setText(_("Genre") + ": ");
	addChild(&mLblGenre);
	addChild(&mGenre);
	mLblPlayers.setText(_("Players") + ": ");
	addChild(&mLblPlayers);
	addChild(&mPlayers);
	mLblLastPlayed.setText(_("Last played") + ": ");
	addChild(&mLblLastPlayed);
	mLastPlayed.setDisplayRelative(true);
	addChild(&mLastPlayed);
	mLblPlayCount.setText(_("Times played") + ": ");
	addChild(&mLblPlayCount);
	addChild(&mPlayCount);
	mLblGameTime.setText(_("Game time") + ": ");
	addChild(&mLblGameTime);
	addChild(&mGameTime);
	mLblFavorite.setText(_("Favorite") + ": ");
	addChild(&mLblFavorite);
	addChild(&mTextFavorite);

	mName.setPosition(mSize.x(), mSize.y());
	mName.setDefaultZIndex(40);
	mName.setColor(0xAAAAAAFF);
	mName.setFont(Font::get(FONT_SIZE_MEDIUM));
	mName.setHorizontalAlignment(ALIGN_CENTER);
	addChild(&mName);

	mDescription.setPosition(mSize.x() * padding, mSize.y() * 0.65f);
	mDescription.setSize(mSize.x() * (0.50f - 2 * padding), mSize.y() - mDescription.getPosition().y());
	mDescription.setDefaultZIndex(40);
	mDescription.setFont(Font::get(FONT_SIZE_SMALL));
	mDescription.setVerticalAlignment(ALIGN_TOP);
	mDescription.setAutoScroll(TextComponent::AutoScrollType::VERTICAL);
	addChild(&mDescription);

	initMDLabels();
	initMDValues();
}

DetailedContainer::~DetailedContainer()
{
	for (auto extra : mThemeExtras)
		delete extra;

	mThemeExtras.clear();

	if (mFolderView != nullptr)
		delete mFolderView;

	if (mThumbnail != nullptr)
		delete mThumbnail;

	if (mImage != nullptr)
		delete mImage;

	for (auto& md : mdImages)
		if (md.component != nullptr)
			delete md.component;

	mdImages.clear();

	if (mVideo != nullptr)
		delete mVideo;

	if (mFlag != nullptr)
		delete mFlag;

	if (mManual != nullptr)
		delete mManual;

	if (mNoManual != nullptr)
		delete mNoManual;

	if (mMap != nullptr)
		delete mMap;

	if (mNoMap != nullptr)
		delete mNoMap;

	if (mSaveState != nullptr)
		delete mSaveState;

	if (mNoSaveState != nullptr)
		delete mNoSaveState;
	
	if (mKidGame != nullptr)
		delete mKidGame;

	if (mNotKidGame != nullptr)
		delete mNotKidGame;

	if (mGunGame != nullptr)
		delete mGunGame;

	if (mNotGunGame != nullptr)
		delete mNotGunGame;

	if (mWheelGame != nullptr)
		delete mWheelGame;

	if (mNotWheelGame != nullptr)
		delete mNotWheelGame;

	if (mTrackballGame != nullptr)
		delete mTrackballGame;

	if (mNotTrackballGame != nullptr)
		delete mNotTrackballGame;

	if (mSpinnerGame != nullptr)
		delete mSpinnerGame;

	if (mNotSpinnerGame != nullptr)
		delete mNotSpinnerGame;

	if (mCheevos != nullptr)
		delete mCheevos;

	if (mNotCheevos != nullptr)
		delete mNotCheevos;

	if (mNetplay != nullptr)
		delete mNetplay;

	if (mNotNetplay != nullptr)
		delete mNotNetplay;	

	if (mFavorite != nullptr)
		delete mFavorite;

	if (mNotFavorite != nullptr)
		delete mNotFavorite;

	if (mHidden != nullptr)
		delete mHidden;
}

std::vector<MdComponent> DetailedContainer::getMetaComponents()
{
	std::vector<MdComponent> mdl =
	{
		{ "rating",   "md_rating",      &mRating,       "md_lbl_rating",      &mLblRating },
		{ "datetime", "md_releasedate", &mReleaseDate,  "md_lbl_releasedate", &mLblReleaseDate },
		{ "text",     "md_developer",   &mDeveloper,    "md_lbl_developer",   &mLblDeveloper },
		{ "text",     "md_publisher",   &mPublisher,    "md_lbl_publisher",   &mLblPublisher },
		{ "text",     "md_genre",       &mGenre,        "md_lbl_genre",       &mLblGenre },
		{ "text",     "md_players",     &mPlayers,      "md_lbl_players",     &mLblPlayers },
		{ "datetime", "md_lastplayed",  &mLastPlayed,   "md_lbl_lastplayed",  &mLblLastPlayed },
		{ "text",     "md_playcount",   &mPlayCount,    "md_lbl_playcount",   &mLblPlayCount },
		{ "text",     "md_gametime",    &mGameTime,     "md_lbl_gametime",    &mLblGameTime },
		{ "text",     "md_favorite",    &mTextFavorite, "md_lbl_favorite",    &mLblFavorite }
	};
	return mdl;	
}


void DetailedContainer::createImageComponent(ImageComponent** pImage, bool forceLoad, bool allowFading)
{
	if (*pImage != nullptr)
		return;

	const float padding = 0.01f;
	auto mSize = mParent->getSize();

	// Image	
	auto image = new ImageComponent(mWindow, !Settings::AllImagesAsync() && (forceLoad || mViewType == DetailedContainerType::VideoView));
	image->setAllowFading(allowFading);
	image->setOrigin(0.5f, 0.5f);
	image->setPosition(mSize.x() * 0.25f, mList->getPosition().y() + mSize.y() * 0.2125f);
	image->setMaxSize(mSize.x() * (0.50f - 2 * padding), mSize.y() * 0.4f);
	image->setDefaultZIndex(30);
	mParent->addChild(image);

	*pImage = image;
}

void DetailedContainer::createVideo()
{
	if (mVideo != nullptr)
		return;

	const float padding = 0.01f;
	auto mSize = mParent->getSize();

	// video
	// Create the correct type of video window
#ifdef _RPI_
	if (Settings::getInstance()->getBool("VideoOmxPlayer"))
		mVideo = new VideoPlayerComponent(mWindow, "");
	else
#endif
		mVideo = new VideoVlcComponent(mWindow);

	// Default is IMAGE in Recalbox themes -> video view does not exist
	mVideo->setSnapshotSource(IMAGE);

	mVideo->setOrigin(0.5f, 0.5f);
	mVideo->setPosition(mSize.x() * 0.25f, mSize.y() * 0.4f);
	mVideo->setSize(mSize.x() * (0.5f - 2 * padding), mSize.y() * 0.4f);
	mVideo->setStartDelay(2000);
	mVideo->setDefaultZIndex(31);
	mParent->addChild(mVideo);
}

void DetailedContainer::initMDLabels()
{
	auto mSize = mParent->getSize();
	std::shared_ptr<Font> defaultFont = Font::get(FONT_SIZE_SMALL);

	auto components = getMetaComponents();

	const unsigned int colCount = 2;
	const unsigned int rowCount = (int)(components.size() / 2);

	Vector3f start(mSize.x() * 0.01f, mSize.y() * 0.625f, 0.0f);

	const float colSize = (mSize.x() * 0.48f) / colCount;
	const float rowPadding = 0.01f * mSize.y();

	int i = 0;	
	for (unsigned int i = 0; i < components.size(); i++)
	{
		if (components[i].labelid.empty() || components[i].label == nullptr)
			continue;

		const unsigned int row = i % rowCount;
		Vector3f pos(0.0f, 0.0f, 0.0f);
		if (row == 0)
			pos = start + Vector3f(colSize * (i / rowCount), 0, 0);
		else 
		{
			// work from the last component
			GuiComponent* lc = components[i - 1].label;
			pos = lc->getPosition() + Vector3f(0, lc->getSize().y() + rowPadding, 0);
		}

		components[i].label->setFont(defaultFont);
		components[i].label->setPosition(pos);
		components[i].label->setDefaultZIndex(40);
	}
}

void DetailedContainer::initMDValues()
{
	auto mSize = mParent->getSize();

	std::shared_ptr<Font> defaultFont = Font::get(FONT_SIZE_SMALL);
	mRating.setSize(defaultFont->getHeight() * 5.0f, (float)defaultFont->getHeight());

	float bottom = 0.0f;

	const float colSize = (mSize.x() * 0.48f) / 2;

	auto components = getMetaComponents();
	for (auto mdc : components)
	{
		TextComponent* text = dynamic_cast<TextComponent*>(mdc.component);
		if (text != nullptr)
			text->setFont(defaultFont);
		
		if (mdc.labelid.empty() || mdc.label == nullptr)
			continue;

		const float heightDiff = (mdc.label->getSize().y() - mdc.label->getSize().y()) / 2;
		mdc.component->setPosition(mdc.label->getPosition() + Vector3f(mdc.label->getSize().x(), heightDiff, 0));
		mdc.component->setSize(colSize - mdc.label->getSize().x(), mdc.component->getSize().y());
		mdc.component->setDefaultZIndex(40);

		float testBot = mdc.component->getPosition().y() + mdc.component->getSize().y();
		if (testBot > bottom)
			bottom = testBot;
	}

	mDescription.setPosition(mDescription.getPosition().x(), bottom + mSize.y() * 0.01f);
	mDescription.setSize(mDescription.getSize().x(), mSize.y() - mDescription.getPosition().y());
}

void DetailedContainer::loadIfThemed(ImageComponent** pImage, const std::shared_ptr<ThemeData>& theme, const std::string& element, bool forceLoad, bool loadPath)
{
	using namespace ThemeFlags;

	auto elem = theme->getElement(getName(), element, "image");

	if (forceLoad || (elem && elem->properties.size() > 0 && (!elem->has("visible") || elem->get<bool>("visible"))))
	{
		createImageComponent(pImage, element == "md_fanart", element == "md_fanart" && Settings::AllImagesAsync());			
		(*pImage)->applyTheme(theme, getName(), element, loadPath ? ALL : ALL ^ (PATH));	
	}
	else if ((*pImage) != nullptr)
	{
		removeChild(*pImage);
		delete (*pImage);
		(*pImage) = nullptr;
	}
}

void DetailedContainer::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	using namespace ThemeFlags;
	
	std::string viewName = getName();

	if (theme != mCustomTheme)
	{
		mTheme = theme;

		const ThemeData::ThemeElement* elem = theme->getElement(viewName, "gameextras", "gameextras");
		if (elem && elem->has("path"))
			mPerGameExtrasPath = elem->get<std::string>("path");
		else
			mPerGameExtrasPath = "";

		initMDLabels();
		initMDValues();
	}
	
	mName.applyTheme(theme, viewName, "md_name", ALL);

	auto velem = theme->getElement(viewName, "md_video", "video");
	if (velem && (!velem->has("visible") || velem->get<bool>("visible")))
	{
		createVideo();
		mVideo->applyTheme(theme, viewName, "md_video", ALL ^ (PATH));
	}
	else if (mVideo != nullptr)
	{
		removeChild(mVideo);
		delete mVideo;
		mVideo = nullptr;
	}

	loadIfThemed(&mImage, theme, "md_image", (mVideo == nullptr && mViewType == DetailedContainerType::DetailedView));
	loadIfThemed(&mThumbnail, theme, "md_thumbnail");
	loadIfThemed(&mFlag, theme, "md_flag");

	for (auto& md : mdImages)
		loadIfThemed(&md.component, theme, md.id);

	loadIfThemed(&mKidGame, theme, "md_kidgame", false, true);
	loadIfThemed(&mNotKidGame, theme, "md_notkidgame", false, true);

	loadIfThemed(&mGunGame, theme, "md_gungame", false, true);
	loadIfThemed(&mNotGunGame, theme, "md_notgungame", false, true);

	loadIfThemed(&mWheelGame, theme, "md_wheelgame", false, true);
	loadIfThemed(&mNotWheelGame, theme, "md_notwheelgame", false, true);

	loadIfThemed(&mTrackballGame, theme, "md_trackballgame", false, true);
	loadIfThemed(&mNotTrackballGame, theme, "md_nottrackballgame", false, true);

	loadIfThemed(&mSpinnerGame, theme, "md_spinnergame", false, true);
	loadIfThemed(&mNotSpinnerGame, theme, "md_notspinnergame", false, true);

	loadIfThemed(&mCheevos, theme, "md_cheevos", false, true);
	loadIfThemed(&mNotCheevos, theme, "md_notcheevos", false, true);

	loadIfThemed(&mNetplay, theme, "md_netplay", false, true);
	loadIfThemed(&mNotNetplay, theme, "md_notnetplay", false, true);
	
	loadIfThemed(&mFavorite, theme, "md_favorite", false, true);
	loadIfThemed(&mNotFavorite, theme, "md_notfavorite", false, true);
	
	loadIfThemed(&mHidden, theme, "md_hidden", false, true);
	loadIfThemed(&mManual, theme, "md_manual", false, true);
	loadIfThemed(&mNoManual, theme, "md_nomanual", false, true);	
	loadIfThemed(&mMap, theme, "md_map", false, true);
	loadIfThemed(&mNoMap, theme, "md_nomap", false, true);
	loadIfThemed(&mSaveState, theme, "md_savestate", false, true);
	loadIfThemed(&mNoSaveState, theme, "md_nosavestate", false, true);		
	
	for (auto comp : getComponents())
		disableComponent(comp);

	for (auto ctrl : getMetaComponents())
	{
		if (ctrl.label != nullptr && theme->getElement(viewName, ctrl.labelid, "text"))
			ctrl.label->applyTheme(theme, viewName, ctrl.labelid, ALL);

		if (ctrl.component != nullptr && theme->getElement(viewName, ctrl.id, ctrl.expectedType))
			ctrl.component->applyTheme(theme, viewName, ctrl.id, ALL);
	}

	if (theme->getElement(viewName, "md_description", "text"))
	{
		mDescription.applyTheme(theme, viewName, "md_description", ALL);
		mDescription.setAutoScroll(TextComponent::AutoScrollType::VERTICAL);

		if (!isChild(&mDescription))
			addChild(&mDescription);
	}
	else if (mViewType == DetailedContainerType::GridView)
		removeChild(&mDescription);

	// Add new theme extras
	for (auto extra : mThemeExtras)
	{
		removeChild(extra);
		delete extra;
	}

	mThemeExtras.clear();
	mThemeExtras = ThemeData::makeExtras(theme, viewName, mWindow, false, (ThemeData::ExtraImportType) (ThemeData::ExtraImportType::WITH_ACTIVATESTORYBOARD | ThemeData::ExtraImportType::PERGAMEEXTRAS));
	for (auto extra : mThemeExtras)
		addChild(extra);

	mParent->sortChildren();
}

Vector3f DetailedContainer::getLaunchTarget()
{
	Vector3f target(Renderer::getScreenWidth() / 2.0f, Renderer::getScreenHeight() / 2.0f, 0);

	if (mVideo != nullptr)
		target = Vector3f(mVideo->getCenter().x(), mVideo->getCenter().y(), 0);
	else if (mImage != nullptr && mImage->hasImage())
		target = Vector3f(mImage->getCenter().x(), mImage->getCenter().y(), 0);
	else if (mThumbnail != nullptr && mThumbnail->hasImage())
		target = Vector3f(mThumbnail->getCenter().x(), mThumbnail->getCenter().y(), 0);

	return target;
}

#define GRIDPADDING Renderer::getScreenHeight() * 0.004f

void DetailedContainer::createFolderGrid(Vector2f targetSize, std::vector<std::string> thumbs)
{
	if (thumbs.size() == 0)
		return;

	auto gridSize = Vector2i(3, 2);
	mFolderView = new ComponentGrid(mWindow, gridSize);

	auto sz = Vector2f(targetSize.x() / (float)gridSize.x(), targetSize.y() / (float)gridSize.y());

	int idx = 0;
	for (int x = 0; x < gridSize.x(); x++)
	{
		for (int y = 0; y < gridSize.y(); y++)
		{
			if (idx >= thumbs.size())
				break;

			auto image = std::make_shared<ImageComponent>(mWindow);
			image->setMaxSize(sz);
			image->setIsLinear(true);
			image->setImage(thumbs[idx]);
			image->setPadding(Vector4f(GRIDPADDING, GRIDPADDING, GRIDPADDING, GRIDPADDING));
			mFolderView->setEntry(image, Vector2i(x, y), false, false);
			idx++;
		}
	}

	mFolderView->setSize(targetSize);
}

void DetailedContainer::updateFolderViewAmbiantProperties()
{
	if (mFolderView != nullptr)
	{
		auto parent = mFolderView->getParent();
		if (parent != nullptr)
		{
			auto opacity = parent->getOpacity();

			for (int i = 0; i < mFolderView->getChildCount(); i++)
				mFolderView->setOpacity(opacity);
		}
	}
}


void DetailedContainer::updateDetailsForFolder(FolderData* folder)
{
	auto games = folder->getChildrenListToDisplay();
	if (games.size() == 0)
		return;
	
	auto desc = folder->getMetadata(MetaDataId::Desc);
	if (desc.empty())
	{
		std::vector<std::string> gamesList;

		for (auto child : games)
		{
			gamesList.push_back("- " + child->getName());
			if (gamesList.size() == 6)
				break;
		}

		desc = "\n" + Utils::String::join(gamesList, "\n");

		int count = games.size();

		char trstring[2048];
		snprintf(trstring, 2048, ngettext(
			"This folder contains %i game, including :%s",
			"This folder contains %i games, including :%s", count), count, desc.c_str());

		mDescription.setText(trstring);
	}	

	FileData* firstGameWithImage = nullptr;

	std::vector<std::string> thumbs;
	for (auto child : games)
	{
		if (!child->getThumbnailPath().empty())
			thumbs.push_back(child->getThumbnailPath());

		if (firstGameWithImage == nullptr && !child->getImagePath().empty())
			firstGameWithImage = child;		
	}



	if (firstGameWithImage != nullptr)
	{
		std::string imagePath = firstGameWithImage->getImagePath().empty() ? firstGameWithImage->getThumbnailPath() : firstGameWithImage->getImagePath();
		
		if (mVideo != nullptr && mVideo->showSnapshots())
		{
			mVideo->setImage(":/blank.png");
			mVideo->setVideo("");

			createFolderGrid(mVideo->getTargetSize(), thumbs);
			if (mFolderView)
				mVideo->addChild(mFolderView);
			else
			{
				std::string snapShot = imagePath;

				auto src = mVideo->getSnapshotSource();


				if (src == TITLESHOT && Utils::FileSystem::exists(firstGameWithImage->getMetadata(MetaDataId::TitleShot)))
					snapShot = firstGameWithImage->getMetadata(MetaDataId::TitleShot);
				else if (src == BOXART && Utils::FileSystem::exists(firstGameWithImage->getMetadata(MetaDataId::BoxArt)))
					snapShot = firstGameWithImage->getMetadata(MetaDataId::BoxArt);
				else if (src == MARQUEE && !firstGameWithImage->getMarqueePath().empty())
					snapShot = firstGameWithImage->getMarqueePath();
				else if ((src == THUMBNAIL || src == BOXART) && !firstGameWithImage->getThumbnailPath().empty())
					snapShot = firstGameWithImage->getThumbnailPath();
				else if ((src == IMAGE || src == TITLESHOT) && !firstGameWithImage->getImagePath().empty())
					snapShot = firstGameWithImage->getImagePath();
				else if (src == FANART && Utils::FileSystem::exists(firstGameWithImage->getMetadata(MetaDataId::FanArt)))
					snapShot = firstGameWithImage->getMetadata(MetaDataId::FanArt);
				else if (src == CARTRIDGE && Utils::FileSystem::exists(firstGameWithImage->getMetadata(MetaDataId::Cartridge)))
					snapShot = firstGameWithImage->getMetadata(MetaDataId::Cartridge);
				else if (src == MIX && Utils::FileSystem::exists(firstGameWithImage->getMetadata(MetaDataId::Mix)))
					snapShot = firstGameWithImage->getMetadata(MetaDataId::Mix);

				mVideo->setImage(snapShot);
			}
		}
		
		if (mThumbnail != nullptr)
		{
			if (mViewType == DetailedContainerType::VideoView && mImage != nullptr && !mImage->hasImage())
				mImage->setImage(firstGameWithImage->getImagePath(), false, mImage->getMaxSizeInfo());

			if (!mThumbnail->hasImage())
				mThumbnail->setImage(firstGameWithImage->getThumbnailPath(), false, mThumbnail->getMaxSizeInfo());
		}

		if (mImage != nullptr && !mImage->hasImage())
		{
			if (mVideo == nullptr || !mVideo->showSnapshots())
			{
				mImage->setImage(":/blank.png");

				createFolderGrid(mImage->getSize(), thumbs);
				if (mFolderView)
					mImage->addChild(mFolderView);
			}
			else
			{
				if (mViewType == DetailedContainerType::VideoView && mThumbnail == nullptr)
					mImage->setImage(firstGameWithImage->getThumbnailPath(), false, mImage->getMaxSizeInfo());
				else if (mViewType != DetailedContainerType::VideoView)
					mImage->setImage(imagePath, false, mImage->getMaxSizeInfo());
			}
		}
	}

	mReleaseDate.setValue(folder->getMetadata(MetaDataId::ReleaseDate));
	mDeveloper.setValue(folder->getMetadata(MetaDataId::Developer));
	mPublisher.setValue(folder->getMetadata(MetaDataId::Publisher));
	mGenre.setValue(folder->getGenre());
	mLastPlayed.setValue("");
	mPlayCount.setValue("");
	mGameTime.setValue("");
}

void DetailedContainer::resetThemedExtras()
{
	if (mCustomTheme != nullptr)
	{
		mCustomTheme = nullptr;

		if (mTheme != nullptr)
			onThemeChanged(mTheme);
	}
}

void DetailedContainer::loadThemedExtras(FileData* file)
{
	if (mPerGameExtrasPath.empty())
	{
		resetThemedExtras();			
		return;
	}

	auto path = Utils::FileSystem::combine(mPerGameExtrasPath, Utils::FileSystem::getStem(file->getPath()) + ".xml");
	
	if (!Utils::FileSystem::exists(path) && !file->getMetadata(MetaDataId::Crc32).empty())
		path = Utils::FileSystem::combine(mPerGameExtrasPath, file->getMetadata(MetaDataId::Crc32) + ".xml");

	if (!Utils::FileSystem::exists(path))
		path = Utils::FileSystem::combine(mPerGameExtrasPath, Utils::FileSystem::getStem(file->getPath()) + "/theme.xml");

	if (!Utils::FileSystem::exists(path) && !file->getMetadata(MetaDataId::Crc32).empty())
		path = Utils::FileSystem::combine(mPerGameExtrasPath, file->getMetadata(MetaDataId::Crc32) + "/theme.xml");

	if (!Utils::FileSystem::exists(path))
	{
		resetThemedExtras();
		return;
	}

	auto customTheme = mTheme->clone(getName());
	if (customTheme->appendFile(path, true))
	{
		auto currentTheme = mTheme;
		mCustomTheme = customTheme;
		onThemeChanged(mCustomTheme);
		mTheme = currentTheme;
	}
	else
		resetThemedExtras();
}

void DetailedContainer::updateControls(FileData* file, bool isClearing, int moveBy, bool isDeactivating)
{
	bool state = (file != NULL);
	if (state)
	{
		if (!isClearing && !isDeactivating)
			loadThemedExtras(file);

		if (mFolderView)
		{
			delete mFolderView;
			mFolderView = nullptr;
		}

		std::string imagePath = file->getImagePath().empty() ? file->getThumbnailPath() : file->getImagePath();

		if (mVideo != nullptr)
		{
			if (!mVideo->setVideo(file->getVideoPath()))
				mVideo->setDefaultVideo();

			std::string snapShot = imagePath;

			auto src = mVideo->getSnapshotSource();

			if (src == TITLESHOT && Utils::FileSystem::exists(file->getMetadata(MetaDataId::TitleShot)))
				snapShot = file->getMetadata(MetaDataId::TitleShot);
			else if (src == BOXART && Utils::FileSystem::exists(file->getMetadata(MetaDataId::BoxArt)))
				snapShot = file->getMetadata(MetaDataId::BoxArt);
			else if (src == MARQUEE && !file->getMarqueePath().empty())
				snapShot = file->getMarqueePath();
			else if ((src == THUMBNAIL || src == BOXART) && !file->getThumbnailPath().empty())
				snapShot = file->getThumbnailPath();
			else if ((src == IMAGE || src == TITLESHOT) && !file->getImagePath().empty())
				snapShot = file->getImagePath();
			else if (src == FANART && Utils::FileSystem::exists(file->getMetadata(MetaDataId::FanArt)))
				snapShot = file->getMetadata(MetaDataId::FanArt);
			else if (src == CARTRIDGE && Utils::FileSystem::exists(file->getMetadata(MetaDataId::Cartridge)))
				snapShot = file->getMetadata(MetaDataId::Cartridge);
			else if (src == MIX && Utils::FileSystem::exists(file->getMetadata(MetaDataId::Mix)))
				snapShot = file->getMetadata(MetaDataId::Mix);

			mVideo->setImage(snapShot, false, mVideo->getMaxSizeInfo());
		}

		if (mThumbnail != nullptr)
		{
			if (mViewType == DetailedContainerType::VideoView && mImage != nullptr)
				mImage->setImage(file->getImagePath(), false, mImage->getMaxSizeInfo());

			mThumbnail->setImage(file->getThumbnailPath(), false, mThumbnail->getMaxSizeInfo());
		}

		if (mImage != nullptr)
		{
			if (mViewType == DetailedContainerType::VideoView && mThumbnail == nullptr)
				mImage->setImage(file->getThumbnailPath(), false, mImage->getMaxSizeInfo());
			else if (mViewType != DetailedContainerType::VideoView)
				mImage->setImage(imagePath, false, mImage->getMaxSizeInfo());
		}

		for (auto& md : mdImages)
		{
			if (md.component != nullptr)
			{
				std::string image;

				for (auto& id : md.metaDataIds)
				{
					if (id == MetaDataId::Marquee)
					{
						if (Utils::FileSystem::exists(file->getMarqueePath()))
						{
							image = file->getMarqueePath();
							break;
						}

						continue;
					}

					std::string path = file->getMetadata(id);
					if (Utils::FileSystem::exists(path))
					{
						image = path;
						break;
					}
				}

				if (!image.empty())
					md.component->setImage(image, false, md.component->getMaxSizeInfo());
				else
					md.component->setImage("");
			}
		}

		if (mFlag != nullptr)
		{
			if (file->getType() == GAME)
			{
				file->detectLanguageAndRegion(false);
				mFlag->setImage(":/flags/" + LangInfo::getFlag(file->getMetadata(MetaDataId::Language), file->getMetadata(MetaDataId::Region)) + ".png"
					, false, mFlag->getMaxSizeInfo());
			}
			else
				mFlag->setImage(":/folder.svg");
		}

		if (mManual != nullptr || mNoManual != nullptr)
		{
			bool hasManualOrMagazine = Settings::getInstance()->getBool("PreloadMedias") ?
				!file->getMetadata(MetaDataId::Manual).empty() || !file->getMetadata(MetaDataId::Magazine).empty() :
				Utils::FileSystem::exists(file->getMetadata(MetaDataId::Manual)) || Utils::FileSystem::exists(file->getMetadata(MetaDataId::Magazine));

			if (mManual != nullptr)
				mManual->setVisible(hasManualOrMagazine);

			if (mNoManual != nullptr)
				mNoManual->setVisible(!hasManualOrMagazine);
		}

		if (mMap != nullptr || mNoMap != nullptr)
		{
			bool hasMap = Settings::getInstance()->getBool("PreloadMedias") ?
				!file->getMetadata(MetaDataId::Map).empty() :
				Utils::FileSystem::exists(file->getMetadata(MetaDataId::Map));

			if (mMap != nullptr)
				mMap->setVisible(hasMap);

			if (mNoMap != nullptr)
				mNoMap->setVisible(!hasMap);
		}

		// Save states
		bool hasSaveState = false;
		bool systemSupportsSaveStates = SaveStateRepository::isEnabled(file);

		if (systemSupportsSaveStates && (mSaveState != nullptr || mNoSaveState != nullptr))
			hasSaveState = file->getSourceFileData()->getSystem()->getSaveStateRepository()->hasSaveStates(file);

		if (mSaveState != nullptr)
			mSaveState->setVisible(systemSupportsSaveStates && hasSaveState);

		if (mNoSaveState != nullptr)
			mNoSaveState->setVisible(systemSupportsSaveStates && !hasSaveState);

		// Kid game
		if (mKidGame != nullptr)
			mKidGame->setVisible(file->getKidGame());

		if (mNotKidGame != nullptr)
			mNotKidGame->setVisible(!file->getKidGame());

		// Gun game
		if (mGunGame != nullptr)
			mGunGame->setVisible(file->isLightGunGame());

		if (mNotGunGame != nullptr)
			mNotGunGame->setVisible(!file->isLightGunGame());

		// Wheel game
		if (mWheelGame != nullptr)
			mWheelGame->setVisible(file->isWheelGame());

		if (mNotWheelGame != nullptr)
			mNotWheelGame->setVisible(!file->isWheelGame());

		// Trackball game
		if (mTrackballGame != nullptr)
			mTrackballGame->setVisible(file->isTrackballGame());

		if (mNotTrackballGame != nullptr)
			mNotTrackballGame->setVisible(!file->isTrackballGame());

		// Spinner game
		if (mSpinnerGame != nullptr)
			mSpinnerGame->setVisible(file->isSpinnerGame());

		if (mNotSpinnerGame != nullptr)
			mNotSpinnerGame->setVisible(!file->isSpinnerGame());

		bool systemHasCheevos = 
			SystemConf::getInstance()->getBool("global.retroachievements") && (
			file->getSourceFileData()->getSystem()->isCheevosSupported() || 
			file->getSystem()->isCollection() || 
			!file->getSystem()->isGameSystem()); // Fake cheevos supported if the game is in a collection cuz there are lot of games from different systems

		// Cheevos
		if (mCheevos != nullptr)
			mCheevos->setVisible(systemHasCheevos && file->hasCheevos());

		if (mNotCheevos != nullptr)
			mNotCheevos->setVisible(systemHasCheevos && !file->hasCheevos());

		// Netplay
		bool systemHasNetplay =
			SystemConf::getInstance()->getBool("global.netplay") && (
				file->getSourceFileData()->getSystem()->isNetplaySupported() ||
				file->getSystem()->isCollection() ||
				!file->getSystem()->isGameSystem());

		if (mNetplay != nullptr)
			mNetplay->setVisible(systemHasNetplay && file->isNetplaySupported());

		if (mNotNetplay != nullptr)
			mNotNetplay->setVisible(systemHasNetplay && !file->isNetplaySupported());

		if (mFavorite != nullptr)
			mFavorite->setVisible(file->getFavorite());

		if (mNotFavorite != nullptr)
			mNotFavorite->setVisible(!file->getFavorite());
		
		if (mHidden != nullptr)
			mHidden->setVisible(file->getHidden());
	
		mDescription.setText(file->getMetadata(MetaDataId::Desc));

		auto valueOrUnknown = [](const std::string value) { return value.empty() ? _("Unknown") : value; };
		auto valueOrOne = [](const std::string value) 
		{ 
			auto split = value.rfind("+");
			if (split != std::string::npos)
				return value.substr(0, split);

			split = value.rfind("-");
			if (split != std::string::npos)
				return value.substr(split + 1);

			std::string ret = value;

			int count = Utils::String::toInteger(value);

			if (count >= 10) ret = "9";
			else if (count == 0) ret = "1";

			return ret;
		};

		mRating.setValue(file->getMetadata(MetaDataId::Rating));
		mReleaseDate.setValue(file->getMetadata(MetaDataId::ReleaseDate));
		mDeveloper.setValue(valueOrUnknown(file->getMetadata(MetaDataId::Developer)));
		mPublisher.setValue(valueOrUnknown(file->getMetadata(MetaDataId::Publisher)));
		mGenre.setValue(valueOrUnknown(file->getGenre()));

		if (mPlayers.getOriginalThemeText() == "1")
			mPlayers.setValue(valueOrOne(file->getMetadata(MetaDataId::Players)));
		else 
			mPlayers.setValue(valueOrUnknown(file->getMetadata(MetaDataId::Players)));

		mName.setValue(file->getMetadata(MetaDataId::Name));
		mTextFavorite.setText(file->getFavorite()?_("YES"):_("NO"));

		if (file->getType() == GAME)
		{
			mLastPlayed.setValue(file->getMetadata(MetaDataId::LastPlayed));
			mPlayCount.setValue(file->getMetadata(MetaDataId::PlayCount));
			mGameTime.setValue(Utils::Time::secondsToString(atol(file->getMetadata(MetaDataId::GameTime).c_str())));
		}
		else if (file->getType() == FOLDER)
			updateDetailsForFolder((FolderData*)file);

		for (auto extra : mThemeExtras)
			BindingManager::updateBindings(extra, file);
	}

	std::vector<GuiComponent*> comps = getComponents();

	if (state && file != nullptr && !isClearing)
	{
		for (auto comp : getComponents())
			handleStoryBoard(comp, true, moveBy);
	}
	
	// We're clearing / populating : don't setup fade animations
	if (file == nullptr && isClearing)
	{
		resetThemedExtras();

		for (auto comp : comps)
		{
			comp->cancelAnimation(0);
			disableComponent(comp);
		}

		return;
	}

	if (state == mState)
		return;

	mState = state;

	bool fadeOut = !state;

	for (auto comp : comps)
	{
		if (fadeOut && isDeactivating && hasActivationStoryboard(comp, false, true))
		{
			handleStoryBoard(comp, false, moveBy);
		}
		else if (fadeOut && isDeactivating && !hasActivationStoryboard(comp))
		{
			comp->deselectStoryboard();
			comp->setOpacity(0);
			disableComponent(comp);
		}
		else
		{
			if (fadeOut)
				comp->enableStoryboardProperty("opacity", false);
			else
				comp->deselectStoryboard(false);

			comp->cancelAnimation(0);

			unsigned char initialOpacity = fadeOut ? comp->getOpacity() : 255;

			auto func = [comp, initialOpacity](float t)
			{
				comp->setOpacity((unsigned char)(Math::lerp(0.0f, 1.0f, t) * initialOpacity)); // 255
			};
			
			comp->setAnimation(new LambdaAnimation(func, fadeOut && isDeactivating ? 500 : 250), 0, [this, comp, fadeOut, file]
			{
				if (fadeOut)
				{
					comp->enableStoryboardProperty("opacity", true);
					comp->deselectStoryboard(true);
					comp->setOpacity(0);
					disableComponent(comp);
				}

			}, fadeOut);
		}
	}

#ifdef _RPI_
	if (Settings::getInstance()->getBool("ScreenSaverOmxPlayer"))
		Utils::FileSystem::removeFile(getTitlePath());
#endif
}

void DetailedContainer::handleStoryBoard(GuiComponent* comp, bool activate, int moveBy, bool recursive)
{
	if (moveBy > 0 && comp->storyBoardExists(activate ? "activateNext" : "deactivateNext"))
	{
		comp->deselectStoryboard(activate);
		comp->selectStoryboard(activate ? "activateNext" : "deactivateNext");

		if (comp->isShowing())
			comp->startStoryboard();
	}
	else if (moveBy < 0 && comp->storyBoardExists(activate ? "activatePrev" : "deactivatePrev"))
	{
		comp->deselectStoryboard(activate);
		comp->selectStoryboard(activate ? "activatePrev" : "deactivatePrev");

		if (comp->isShowing())
			comp->startStoryboard();
	}
	else if (moveBy != 0 && comp->storyBoardExists(activate ? "activate" : "deactivate"))
	{
		comp->deselectStoryboard(activate);
		comp->selectStoryboard(activate ? "activate" : "deactivate");

		if (comp->isShowing())
			comp->startStoryboard();
	}
	else if (activate && moveBy == 0 && comp->storyBoardExists("open"))
	{
		comp->deselectStoryboard();
		comp->selectStoryboard("open");

		if (comp->isShowing())
			comp->startStoryboard();
	}
	else if (activate && moveBy == 0 && comp->storyBoardExists("") && !comp->hasStoryBoard("", true))
	{
		comp->deselectStoryboard();
		comp->selectStoryboard();

		if (comp->isShowing())
			comp->onShow();
	}
	else if (activate && moveBy == 0 && comp->storyBoardExists("activate"))
	{
		comp->deselectStoryboard(activate);
		comp->selectStoryboard("activate");

		if (comp->isShowing())
			comp->startStoryboard();
	}

	if (!recursive)
		return;
	
	for (auto child : comp->enumerateExtraChildrens())
		handleStoryBoard(child, activate, moveBy, false);
}

bool DetailedContainer::hasActivationStoryboard(GuiComponent* comp, bool checkActivate, bool checkDeactivate)
{
	if (checkDeactivate && (comp->storyBoardExists("deactivate") || comp->storyBoardExists("deactivateNext") || comp->storyBoardExists("deactivatePrev")))	
		return true;

	if (checkActivate && (comp->storyBoardExists("activate") || comp->storyBoardExists("activateNext") || comp->storyBoardExists("activatePrev")))
		return true;

	return false;
}

bool DetailedContainer::anyComponentHasStoryBoardRunning()
{

	for (auto comp : getComponents())
		if (comp->isAnimationPlaying(0) || 
			comp->isStoryBoardRunning("activate") || 
			comp->isStoryBoardRunning("activateNext") || 
			comp->isStoryBoardRunning("activatePrev") || 
			comp->isStoryBoardRunning("deactivate") ||
			comp->isStoryBoardRunning("deactivateNext") ||
			comp->isStoryBoardRunning("deactivatePrev"))
			return true;

	return false;
}

bool DetailedContainer::anyComponentHasStoryBoard()
{
	for (auto comp : getComponents())
		if (hasActivationStoryboard(comp))
			return true;

	return false;
}


void DetailedContainer::disableComponent(GuiComponent* comp)
{
	if (comp == nullptr)
		return;

	if (mVideo == comp) { mVideo->setImage(""); mVideo->setVideo(""); }
	if (mImage == comp) mImage->setImage("");
	if (mThumbnail == comp) mThumbnail->setImage("");
	if (mFlag == comp) mFlag->setImage("");
	if (mManual == comp) mManual->setVisible(false);
	if (mNoManual == comp) mNoManual->setVisible(false);
	if (mMap == comp) mMap->setVisible(false);
	if (mNoMap == comp) mNoMap->setVisible(false);
	if (mSaveState == comp) mSaveState->setVisible(false);
	if (mNoSaveState == comp) mNoSaveState->setVisible(false);
	if (mKidGame == comp) mKidGame->setVisible(false);
	if (mNotKidGame == comp) mNotKidGame->setVisible(false);
	if (mGunGame == comp) mGunGame->setVisible(false);
	if (mNotGunGame == comp) mNotGunGame->setVisible(false);
	if (mWheelGame == comp) mWheelGame->setVisible(false);
	if (mNotWheelGame == comp) mNotWheelGame->setVisible(false);
	if (mTrackballGame == comp) mTrackballGame->setVisible(false);
	if (mNotTrackballGame == comp) mNotTrackballGame->setVisible(false);
	if (mSpinnerGame == comp) mSpinnerGame->setVisible(false);
	if (mNotSpinnerGame == comp) mNotSpinnerGame->setVisible(false);
	if (mCheevos == comp) mCheevos->setVisible(false);
	if (mNotCheevos == comp) mNotCheevos->setVisible(false);
	if (mNetplay == comp) mNetplay->setVisible(false);
	if (mNotNetplay == comp) mNotNetplay->setVisible(false);	
	if (mFavorite == comp) mFavorite->setVisible(false);
	if (mNotFavorite == comp) mNotFavorite->setVisible(false);
	if (mHidden == comp) mHidden->setVisible(false);
	
	for (auto& md : mdImages)
		if (md.component == comp)
			md.component->setImage("");
}

std::vector<GuiComponent*>  DetailedContainer::getComponents()
{
	std::vector<GuiComponent*> comps;

	for (auto lbl : getMetaComponents())
		if (lbl.component != nullptr)
			comps.push_back(lbl.component);

	if (mVideo != nullptr) comps.push_back(mVideo);
	if (mImage != nullptr) comps.push_back(mImage);
	if (mThumbnail != nullptr) comps.push_back(mThumbnail);
	if (mFlag != nullptr) comps.push_back(mFlag);
	if (mManual != nullptr) comps.push_back(mManual);
	if (mNoManual != nullptr) comps.push_back(mNoManual);
	if (mMap != nullptr) comps.push_back(mMap);
	if (mNoMap != nullptr) comps.push_back(mNoMap);
	if (mSaveState != nullptr) comps.push_back(mSaveState);
	if (mNoSaveState != nullptr) comps.push_back(mNoSaveState);
	if (mKidGame != nullptr) comps.push_back(mKidGame);
	if (mNotKidGame != nullptr) comps.push_back(mNotKidGame);
	if (mGunGame != nullptr) comps.push_back(mGunGame);
	if (mNotGunGame != nullptr) comps.push_back(mNotGunGame);
	if (mWheelGame != nullptr) comps.push_back(mWheelGame);
	if (mNotWheelGame != nullptr) comps.push_back(mNotWheelGame);
	if (mTrackballGame != nullptr) comps.push_back(mTrackballGame);
	if (mNotTrackballGame != nullptr) comps.push_back(mNotTrackballGame);
	if (mSpinnerGame != nullptr) comps.push_back(mSpinnerGame);
	if (mNotSpinnerGame != nullptr) comps.push_back(mNotSpinnerGame);
	if (mCheevos != nullptr) comps.push_back(mCheevos);
	if (mNotCheevos != nullptr) comps.push_back(mNotCheevos);
	if (mNetplay != nullptr) comps.push_back(mNetplay);
	if (mNotNetplay != nullptr) comps.push_back(mNotNetplay);	
	if (mFavorite != nullptr) comps.push_back(mFavorite);
	if (mNotFavorite != nullptr) comps.push_back(mNotFavorite);
	if (mHidden != nullptr) comps.push_back(mHidden);

	for (auto& md : mdImages)
		if (md.component != nullptr)
			comps.push_back(md.component);

	comps.push_back(&mDescription);
	comps.push_back(&mName);
	comps.push_back(&mRating);

	for (auto lbl : getMetaComponents())
		if (lbl.label != nullptr)
			comps.push_back(lbl.label);

	for (auto extra : mThemeExtras)
		comps.push_back(extra);

	return comps;
}

DetailedContainerHost::DetailedContainerHost(ISimpleGameListView* parent, GuiComponent* list, Window* window, DetailedContainer::DetailedContainerType viewType)
{
	mParent = parent;
	mList = list;
	mWindow = window;
	mViewType = viewType;

	mActiveFile = nullptr;
	mContainer = new DetailedContainer(parent, list, window, viewType);
}

DetailedContainerHost::~DetailedContainerHost()
{
	mWindow->unregisterPostedFunctions(this);

	delete mContainer;
	for (auto container : mContainers)
		delete container;
}

void DetailedContainerHost::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	mTheme = theme;

	mContainer->onThemeChanged(theme);
	for (auto container : mContainers)
		container->onThemeChanged(theme);
}

void DetailedContainerHost::update(int deltaTime)
{
	mContainer->updateFolderViewAmbiantProperties();

	int index = mContainers.size();
	for (auto it = mContainers.begin(); it != mContainers.end(); )
	{
		DetailedContainer* dc = *it;

		dc->updateFolderViewAmbiantProperties();

		if (!dc->anyComponentHasStoryBoardRunning() || index > 4)
		{
			it = mContainers.erase(it);

			for (auto cp : dc->getComponents())
			{
				cp->setVisible(false);
				cp->onHide();
			}

			mWindow->postToUiThread([dc]() { delete dc; }, this);
			// break;
		}
		else
			++it;

		index--;
	}
}

Vector3f DetailedContainerHost::getLaunchTarget()
{
	return mContainer->getLaunchTarget();
}

void DetailedContainerHost::updateControls(FileData* file, bool isClearing, int moveBy)
{
	if (!mContainer->anyComponentHasStoryBoard() || file == nullptr || isClearing || moveBy == 0)
	{
		if (file != nullptr && !isClearing)
			file->setSelectedGame();

		mContainer->updateControls(file, isClearing, moveBy);
		return;
	}
	
	if (mActiveFile == file)
		return;

	if (file == nullptr || isClearing)
	{
		mContainer->updateControls(nullptr, isClearing, moveBy, true);
		return;
	}

	mActiveFile = file;
	mActiveFile->setSelectedGame();
	bool clear = isClearing;
	int by = moveBy;

	mWindow->postToUiThread([this, clear, by]() 
	{ 		
		auto oldContainer = mContainer;

		mContainers.push_back(mContainer);
		mContainer = new DetailedContainer(mParent, mList, mWindow, mViewType);
	
		if (mTheme != nullptr)
			mContainer->onThemeChanged(mTheme);

		mContainer->updateControls(mActiveFile, clear, by);
		oldContainer->updateControls(nullptr, clear, by, true);

		for (auto cp : mContainer->getComponents())
			cp->onShow();
	}, this);
}

/*
snprintf(trstring, 1024, ngettext(
	"This collection contains %i game, including :%s",
	"This collection contains %i games, including :%s", games_counter), games_counter, games_list.c_str());
*/
