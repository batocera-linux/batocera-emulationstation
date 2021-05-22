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
	mFavorite(nullptr), mNotFavorite(nullptr),
	mManual(nullptr), mNoManual(nullptr), 
	mMap(nullptr), mNoMap(nullptr),
	mCheevos(nullptr), mNotCheevos(nullptr),
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
		{ "md_marquee", { MetaDataId::Marquee, MetaDataId::Wheel } },
		{ "md_fanart", { MetaDataId::FanArt, MetaDataId::TitleShot, MetaDataId::Image } },
		{ "md_titleshot", { MetaDataId::TitleShot, MetaDataId::Image } },
		{ "md_boxart", { MetaDataId::BoxArt, MetaDataId::Thumbnail } },
		{ "md_wheel",{ MetaDataId::Wheel, MetaDataId::Marquee } },
		{ "md_cartridge",{ MetaDataId::Cartridge } },
		{ "md_boxback",{ MetaDataId::BoxBack } },		
		{ "md_mix",{ MetaDataId::Mix, MetaDataId::Image, MetaDataId::Thumbnail } }
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

	if (mCheevos != nullptr)
		delete mCheevos;

	if (mNotCheevos != nullptr)
		delete mNotCheevos;

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


void DetailedContainer::createImageComponent(ImageComponent** pImage, bool forceLoad)
{
	if (*pImage != nullptr)
		return;

	const float padding = 0.01f;
	auto mSize = mParent->getSize();

	// Image	
	auto image = new ImageComponent(mWindow, forceLoad || mViewType == DetailedContainerType::VideoView);
	image->setAllowFading(false);
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
		mVideo = new VideoVlcComponent(mWindow, "");

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

	auto components = getMetaComponents();

	std::shared_ptr<Font> defaultFont = Font::get(FONT_SIZE_SMALL);
	mRating.setSize(defaultFont->getHeight() * 5.0f, (float)defaultFont->getHeight());

	float bottom = 0.0f;

	const float colSize = (mSize.x() * 0.48f) / 2;
	for (unsigned int i = 0; i < components.size(); i++)
	{
		TextComponent* text = dynamic_cast<TextComponent*>(components[i].component);
		if (text != nullptr)
			text->setFont(defaultFont);
		
		if (components[i].labelid.empty() || components[i].label == nullptr)
			continue;

		const float heightDiff = (components[i].label->getSize().y() - components[i].label->getSize().y()) / 2;
		components[i].component->setPosition(components[i].label->getPosition() + Vector3f(components[i].label->getSize().x(), heightDiff, 0));
		components[i].component->setSize(colSize - components[i].label->getSize().x(), components[i].component->getSize().y());
		components[i].component->setDefaultZIndex(40);

		float testBot = components[i].component->getPosition().y() + components[i].component->getSize().y();
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
	if (forceLoad || (elem && elem->properties.size() > 0))
	{
		createImageComponent(pImage, element == "md_fanart");
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

	mName.applyTheme(theme, getName(), "md_name", ALL);	

	if (theme->getElement(getName(), "md_video", "video"))
	{
		createVideo();
		mVideo->applyTheme(theme, getName(), "md_video", ALL ^ (PATH));
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

	loadIfThemed(&mCheevos, theme, "md_cheevos", false, true);
	loadIfThemed(&mNotCheevos, theme, "md_notcheevos", false, true);
	
	loadIfThemed(&mFavorite, theme, "md_favorite", false, true);
	loadIfThemed(&mNotFavorite, theme, "md_notfavorite", false, true);
	
	loadIfThemed(&mHidden, theme, "md_hidden", false, true);
	loadIfThemed(&mManual, theme, "md_manual", false, true);
	loadIfThemed(&mNoManual, theme, "md_nomanual", false, true);	
	loadIfThemed(&mMap, theme, "md_map", false, true);
	loadIfThemed(&mNoMap, theme, "md_nomap", false, true);
	loadIfThemed(&mSaveState, theme, "md_savestate", false, true);
	loadIfThemed(&mNoSaveState, theme, "md_nosavestate", false, true);	

	initMDLabels();

	for (auto ctrl : getMetaComponents())
		if (ctrl.label != nullptr && theme->getElement(getName(), ctrl.labelid, "text"))
			ctrl.label->applyTheme(theme, getName(), ctrl.labelid, ALL);

	initMDValues();

	for (auto ctrl : getMetaComponents())
		if (ctrl.component != nullptr && theme->getElement(getName(), ctrl.id, ctrl.expectedType))
			ctrl.component->applyTheme(theme, getName(), ctrl.id, ALL);

	if (theme->getElement(getName(), "md_description", "text"))
	{
		mDescription.applyTheme(theme, getName(), "md_description", ALL);
		mDescription.setAutoScroll(TextComponent::AutoScrollType::VERTICAL);

		if (!isChild(&mDescription))
			addChild(&mDescription);
	}
	else if (mViewType == DetailedContainerType::GridView)
		removeChild(&mDescription);

	mThemeExtras.clear();

	// Add new theme extras
	mThemeExtras = ThemeData::makeExtras(theme, getName(), mWindow, false, ThemeData::ExtraImportType::WITH_ACTIVATESTORYBOARD);
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
				if (src == MARQUEE && !firstGameWithImage->getMarqueePath().empty())
					snapShot = firstGameWithImage->getMarqueePath();
				if (src == THUMBNAIL && !firstGameWithImage->getThumbnailPath().empty())
					snapShot = firstGameWithImage->getThumbnailPath();
				if (src == IMAGE && !firstGameWithImage->getImagePath().empty())
					snapShot = firstGameWithImage->getImagePath();

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
	mGenre.setValue(folder->getMetadata(MetaDataId::Genre));
	mLastPlayed.setValue("");
	mPlayCount.setValue("");
	mGameTime.setValue("");
}

void DetailedContainer::updateControls(FileData* file, bool isClearing, int moveBy, bool isDeactivating)
{
	bool state = (file != NULL);
	if (state)
	{	
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
			if (src == MARQUEE && !file->getMarqueePath().empty())
				snapShot = file->getMarqueePath();
			if (src == THUMBNAIL && !file->getThumbnailPath().empty())
				snapShot = file->getThumbnailPath();			
			if (src == IMAGE && !file->getImagePath().empty())
				snapShot = file->getImagePath();

			mVideo->setImage(snapShot);
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

		bool hasManualOrMagazine = Utils::FileSystem::exists(file->getMetadata(MetaDataId::Manual)) || Utils::FileSystem::exists(file->getMetadata(MetaDataId::Magazine));

		if (mManual != nullptr)
			mManual->setVisible(hasManualOrMagazine);

		if (mNoManual != nullptr)
			mNoManual->setVisible(!hasManualOrMagazine);

		if (mMap != nullptr)
			mMap->setVisible(Utils::FileSystem::exists(file->getMetadata(MetaDataId::Map)));

		if (mNoMap != nullptr)
			mNoMap->setVisible(!Utils::FileSystem::exists(file->getMetadata(MetaDataId::Map)));

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

		bool systemHasCheevos = 
			SystemConf::getInstance()->getBool("global.retroachievements") && (
			file->getSourceFileData()->getSystem()->isCheevosSupported() || 
			file->getSystem()->isCollection() || 
			file->getSystem()->isGroupSystem()); // Fake cheevos supported if the game is in a collection cuz there are lot of games from different systems

		// Cheevos
		if (mCheevos != nullptr)
			mCheevos->setVisible(systemHasCheevos && file->hasCheevos());

		if (mNotCheevos != nullptr)
			mNotCheevos->setVisible(systemHasCheevos && !file->hasCheevos());

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
		mGenre.setValue(valueOrUnknown(file->getMetadata(MetaDataId::Genre)));

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
	}

	std::vector<GuiComponent*> comps = getComponents();

	/*if (file == nullptr && !isClearing)
	{
		if (isDeactivating)
			for (auto comp : getComponents())
				comp->deselectStoryboard(!isDeactivating); // TODO !?
	}
	else */
	if (state && file != nullptr && !isClearing)
	{
		for (auto comp : getComponents())
		{
			if (moveBy > 0 && comp->storyBoardExists("activateNext"))
			{
				comp->deselectStoryboard();
				comp->selectStoryboard("activateNext");

				if (comp->isShowing())
					comp->startStoryboard();
			}
			else if (moveBy < 0 && comp->storyBoardExists("activatePrev"))
			{
				comp->deselectStoryboard();
				comp->selectStoryboard("activatePrev");

				if (comp->isShowing())
					comp->startStoryboard();
			}
			else if (moveBy != 0 && comp->storyBoardExists("activate"))
			{
				comp->deselectStoryboard();
				comp->selectStoryboard("activate");

				if (comp->isShowing())
					comp->startStoryboard();
			}
			else if (moveBy == 0 && comp->storyBoardExists("open"))
			{
				comp->deselectStoryboard();
				comp->selectStoryboard("open");

				if (comp->isShowing())
					comp->startStoryboard();
			}
			else if (moveBy == 0 && comp->storyBoardExists("") && !comp->hasStoryBoard("", true))
			{
				comp->deselectStoryboard();
				comp->selectStoryboard();

				if (comp->isShowing())
					comp->onShow();
			}
		}
	}
	
	// We're clearing / populating : don't setup fade animations
	if (file == nullptr && isClearing)
	{
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
			if (moveBy > 0 && comp->storyBoardExists("deactivateNext"))
			{
				comp->deselectStoryboard(false);
				comp->selectStoryboard("deactivateNext");

				if (comp->isShowing())
					comp->startStoryboard();
			}
			else if (moveBy < 0 && comp->storyBoardExists("deactivatePrev"))
			{
				comp->deselectStoryboard(false);
				comp->selectStoryboard("deactivatePrev");

				if (comp->isShowing())
					comp->startStoryboard();
			}
			else if (moveBy != 0 && comp->storyBoardExists("deactivate"))
			{
				comp->deselectStoryboard(false);
				comp->selectStoryboard("deactivate");

				if (comp->isShowing())
					comp->startStoryboard();
			}
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

	Utils::FileSystem::removeFile(getTitlePath());
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
	if (mCheevos == comp) mCheevos->setVisible(false);
	if (mNotCheevos == comp) mNotCheevos->setVisible(false);
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
	if (mCheevos != nullptr) comps.push_back(mCheevos);
	if (mNotCheevos != nullptr) comps.push_back(mNotCheevos);
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

	for (auto it = mContainers.begin(); it != mContainers.end(); it++)
	{
		DetailedContainer* dc = *it;

		dc->updateFolderViewAmbiantProperties();

		if (!dc->anyComponentHasStoryBoardRunning())
		{			
			mContainers.erase(it);
			
			for (auto cp : dc->getComponents())
			{
				cp->setVisible(false);
				cp->onHide();
			}
			
			mWindow->postToUiThread([dc]() { delete dc; }, this);
			break;
		}
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