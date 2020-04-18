#include "views/gamelist/DetailedGameListView.h"

#include "animations/LambdaAnimation.h"
#include "views/ViewController.h"
#include "FileData.h"
#include "SystemData.h"
#include "LocaleES.h"

#ifdef _RPI_
#include "Settings.h"
#include "components/VideoPlayerComponent.h"
#endif
#include "components/VideoVlcComponent.h"

DetailedGameListView::DetailedGameListView(Window* window, FolderData* root) : 
	BasicGameListView(window, root), 
	mDescContainer(window), mDescription(window), 
	mImage(nullptr), mMarquee(nullptr), mVideo(nullptr), mThumbnail(nullptr),

	mLblRating(window), mLblReleaseDate(window), mLblDeveloper(window), mLblPublisher(window), 
	mLblGenre(window), mLblPlayers(window), mLblLastPlayed(window), mLblPlayCount(window), mLblGameTime(window),

	mRating(window), mReleaseDate(window), mDeveloper(window), mPublisher(window), 
	mGenre(window), mPlayers(window), mLastPlayed(window), mPlayCount(window),
	mName(window), mGameTime(window)
{
	const float padding = 0.01f;

	mLblGameTime.setVisible(false);
	mGameTime.setVisible(false);

	mList.setPosition(mSize.x() * (0.50f + padding), mList.getPosition().y());
	mList.setSize(mSize.x() * (0.50f - padding), mList.getSize().y());
	mList.setAlignment(TextListComponent<FileData*>::ALIGN_LEFT);
	mList.setCursorChangedCallback([&](const CursorState& /*state*/) { updateInfoPanel(); });

	// image
	createImage();

	// metadata labels + values
	mLblRating.setText(_("Rating") + ": "); // batocera
	addChild(&mLblRating);
	addChild(&mRating);
	mLblReleaseDate.setText(_("Released") + ": "); // batocera
	addChild(&mLblReleaseDate);
	addChild(&mReleaseDate);
	mLblDeveloper.setText(_("Developer") + ": "); // batocera
	addChild(&mLblDeveloper);
	addChild(&mDeveloper);
	mLblPublisher.setText(_("Publisher") + ": "); // batocera
	addChild(&mLblPublisher);
	addChild(&mPublisher);
	mLblGenre.setText(_("Genre") + ": "); // batocera
	addChild(&mLblGenre);
	addChild(&mGenre);
	mLblPlayers.setText(_("Players") + ": "); // batocera
	addChild(&mLblPlayers);
	addChild(&mPlayers);
	mLblLastPlayed.setText(_("Last played") + ": "); // batocera
	addChild(&mLblLastPlayed);
	mLastPlayed.setDisplayRelative(true);
	addChild(&mLastPlayed);
	mLblPlayCount.setText(_("Times played") + ": "); // batocera
	addChild(&mLblPlayCount);
	addChild(&mPlayCount);
	mLblGameTime.setText(_("Game time") + ": "); // batocera
	addChild(&mLblGameTime);
	addChild(&mGameTime);

	mName.setPosition(mSize.x(), mSize.y());
	mName.setDefaultZIndex(40);
	mName.setColor(0xAAAAAAFF);
	mName.setFont(Font::get(FONT_SIZE_MEDIUM));
	mName.setHorizontalAlignment(ALIGN_CENTER);
	addChild(&mName);

	mDescContainer.setPosition(mSize.x() * padding, mSize.y() * 0.65f);
	mDescContainer.setSize(mSize.x() * (0.50f - 2*padding), mSize.y() - mDescContainer.getPosition().y());
	mDescContainer.setAutoScroll(true);
	mDescContainer.setDefaultZIndex(40);
	addChild(&mDescContainer);

	mDescription.setFont(Font::get(FONT_SIZE_SMALL));
	mDescription.setSize(mDescContainer.getSize().x(), 0);
	mDescContainer.addChild(&mDescription);


	initMDLabels();
	initMDValues();
	updateInfoPanel();
}

DetailedGameListView::~DetailedGameListView()
{
	if (mThumbnail != nullptr)
		delete mThumbnail;
	
	if (mImage != nullptr)
		delete mImage;

	if (mMarquee != nullptr)
		delete mMarquee;

	if (mVideo != nullptr)
		delete mVideo;
}

void DetailedGameListView::createImage()
{
	if (mImage != nullptr)
		return;

	const float padding = 0.01f;

	// Image
	mImage = new ImageComponent(mWindow);
	mImage->setAllowFading(false);
	mImage->setOrigin(0.5f, 0.5f);
	mImage->setPosition(mSize.x() * 0.25f, mList.getPosition().y() + mSize.y() * 0.2125f);
	mImage->setMaxSize(mSize.x() * (0.50f - 2 * padding), mSize.y() * 0.4f);
	mImage->setDefaultZIndex(30);
	addChild(mImage);
}

void DetailedGameListView::createThumbnail()
{
	if (mThumbnail != nullptr)
		return;

	const float padding = 0.01f;

	// Image
	mThumbnail = new ImageComponent(mWindow);
	mThumbnail->setAllowFading(false);
	mThumbnail->setOrigin(0.5f, 0.5f);
	mThumbnail->setPosition(mSize.x() * 0.25f, mList.getPosition().y() + mSize.y() * 0.2125f);
	mThumbnail->setMaxSize(mSize.x() * (0.50f - 2 * padding), mSize.y() * 0.4f);
	mThumbnail->setDefaultZIndex(30);
	addChild(mThumbnail);
}

void DetailedGameListView::createVideo()
{
	if (mVideo != nullptr)
		return;

	const float padding = 0.01f;

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
	addChild(mVideo);
}

void DetailedGameListView::createMarquee()
{
	const float padding = 0.01f;

	// Marquee
	mMarquee = new ImageComponent(mWindow);
	mMarquee->setAllowFading(false);
	mMarquee->setOrigin(0.5f, 0.5f);
	mMarquee->setPosition(mSize.x() * 0.25f, mSize.y() * 0.10f);
	mMarquee->setMaxSize(mSize.x() * (0.5f - 2 * padding), mSize.y() * 0.18f);
	mMarquee->setDefaultZIndex(35);
	addChild(mMarquee);

}

void DetailedGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	BasicGameListView::onThemeChanged(theme);

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
	
	if (mVideo == nullptr || theme->getElement(getName(), "md_image", "image"))
	{
		createImage();
		mImage->applyTheme(theme, getName(), "md_image", ALL ^ (PATH));
	}
	else if (mImage != nullptr)
	{
		removeChild(mImage);
		delete mImage;
		mImage = nullptr;
	}

	if (theme->getElement(getName(), "md_thumbnail", "image"))
	{
		createThumbnail();
		mThumbnail->applyTheme(theme, getName(), "md_thumbnail", ALL ^ (PATH));
	}
	else if (mThumbnail != nullptr)
	{
		removeChild(mThumbnail);
		delete mThumbnail;
		mThumbnail = nullptr;
	}	

	if (theme->getElement(getName(), "md_marquee", "image"))
	{
		createMarquee();
		mMarquee->applyTheme(theme, getName(), "md_marquee", ALL ^ (PATH));
	}
	else if (mMarquee != nullptr)
	{
		removeChild(mMarquee);
		delete mMarquee;
		mMarquee = nullptr;
	}

	initMDLabels();
	std::vector<TextComponent*> labels = getMDLabels();
	assert(labels.size() == 9);
	const char* lblElements[9] = {
		"md_lbl_rating", "md_lbl_releasedate", "md_lbl_developer", "md_lbl_publisher", 
		"md_lbl_genre", "md_lbl_players", "md_lbl_lastplayed", "md_lbl_playcount", "md_lbl_gametime"
	};

	for(unsigned int i = 0; i < labels.size(); i++)
		labels[i]->applyTheme(theme, getName(), lblElements[i], ALL);

	initMDValues();
	std::vector<GuiComponent*> values = getMDValues();
	assert(values.size() == 9);
	const char* valElements[9] = {
		"md_rating", "md_releasedate", "md_developer", "md_publisher", 
		"md_genre", "md_players", "md_lastplayed", "md_playcount", "md_gametime"
	};

	for(unsigned int i = 0; i < values.size(); i++)
		values[i]->applyTheme(theme, getName(), valElements[i], ALL ^ ThemeFlags::TEXT);

	mDescContainer.applyTheme(theme, getName(), "md_description", POSITION | ThemeFlags::SIZE | Z_INDEX | VISIBLE);
	mDescription.setSize(mDescContainer.getSize().x(), 0);
	mDescription.applyTheme(theme, getName(), "md_description", ALL ^ (POSITION | ThemeFlags::SIZE | ThemeFlags::ORIGIN | TEXT | ROTATION));

	sortChildren();
	updateInfoPanel();
}

void DetailedGameListView::initMDLabels()
{
	std::vector<TextComponent*> components = getMDLabels();

	const unsigned int colCount = 2;
	const unsigned int rowCount = (int)(components.size() / 2);

	Vector3f start(mSize.x() * 0.01f, mSize.y() * 0.625f, 0.0f);
	
	const float colSize = (mSize.x() * 0.48f) / colCount;
	const float rowPadding = 0.01f * mSize.y();

	for(unsigned int i = 0; i < components.size(); i++)
	{
		const unsigned int row = i % rowCount;
		Vector3f pos(0.0f, 0.0f, 0.0f);
		if(row == 0)
		{
			pos = start + Vector3f(colSize * (i / rowCount), 0, 0);
		}else{
			// work from the last component
			GuiComponent* lc = components[i-1];
			pos = lc->getPosition() + Vector3f(0, lc->getSize().y() + rowPadding, 0);
		}

		components[i]->setFont(Font::get(FONT_SIZE_SMALL));
		components[i]->setPosition(pos);
		components[i]->setDefaultZIndex(40);
	}
}

void DetailedGameListView::initMDValues()
{
	std::vector<TextComponent*> labels = getMDLabels();
	std::vector<GuiComponent*> values = getMDValues();

	std::shared_ptr<Font> defaultFont = Font::get(FONT_SIZE_SMALL);
	mRating.setSize(defaultFont->getHeight() * 5.0f, (float)defaultFont->getHeight());
	mReleaseDate.setFont(defaultFont);
	mDeveloper.setFont(defaultFont);
	mPublisher.setFont(defaultFont);
	mGenre.setFont(defaultFont);
	mPlayers.setFont(defaultFont);
	mLastPlayed.setFont(defaultFont);
	mPlayCount.setFont(defaultFont);
	mGameTime.setFont(defaultFont);

	float bottom = 0.0f;

	const float colSize = (mSize.x() * 0.48f) / 2;
	for(unsigned int i = 0; i < labels.size(); i++)
	{
		const float heightDiff = (labels[i]->getSize().y() - values[i]->getSize().y()) / 2;
		values[i]->setPosition(labels[i]->getPosition() + Vector3f(labels[i]->getSize().x(), heightDiff, 0));
		values[i]->setSize(colSize - labels[i]->getSize().x(), values[i]->getSize().y());
		values[i]->setDefaultZIndex(40);

		float testBot = values[i]->getPosition().y() + values[i]->getSize().y();
		if(testBot > bottom)
			bottom = testBot;
	}

	mDescContainer.setPosition(mDescContainer.getPosition().x(), bottom + mSize.y() * 0.01f);
	mDescContainer.setSize(mDescContainer.getSize().x(), mSize.y() - mDescContainer.getPosition().y());
}

void DetailedGameListView::updateInfoPanel()
{
	if (mRoot->getSystem()->isCollection())
		updateHelpPrompts();

	FileData* file = (mList.size() == 0 || mList.isScrolling()) ? NULL : mList.getSelected();

	bool fadingOut;
	if(file == NULL)
	{
		if (mVideo != nullptr)
			mVideo->setVideo("");

		//mImage.setImage("");
		//mDescription.setText("");
		fadingOut = true;
	}
	else
	{
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

			mVideo->setImage(snapShot);
		}

		if (mThumbnail != nullptr)
			mThumbnail->setImage(file->getThumbnailPath());

		if (mImage != nullptr)
			mImage->setImage(imagePath);

		if (mMarquee != nullptr)
			mMarquee->setImage(file->getMarqueePath(), false, mMarquee->getMaxSizeInfo());
		
		mDescription.setText(file->getMetadata("desc"));
		mDescContainer.reset();

		mRating.setValue(file->getMetadata("rating"));
		mReleaseDate.setValue(file->getMetadata("releasedate"));
		mDeveloper.setValue(file->getMetadata("developer"));
		mPublisher.setValue(file->getMetadata("publisher"));
		mGenre.setValue(file->getMetadata("genre"));
		mPlayers.setValue(file->getMetadata("players"));
		mName.setValue(file->getMetadata("name"));

		if(file->getType() == GAME)
		{
			mLastPlayed.setValue(file->getMetadata("lastplayed"));
			mPlayCount.setValue(file->getMetadata("playcount"));
			mGameTime.setValue(Utils::Time::secondsToString(atol(file->getMetadata("gametime").c_str())));
		}
		
		fadingOut = false;
	}
	
	// We're clearing / populating : don't setup fade animations
	if (file == nullptr && mList.getObjects().size() == 0 && mList.getCursorIndex() == 0 && mList.getScrollingVelocity() == 0)
		return;

	std::vector<GuiComponent*> comps = getMDValues();

	if (mVideo != nullptr)
		comps.push_back(mVideo);

	if (mImage != nullptr)
		comps.push_back(mImage);

	if (mThumbnail != nullptr)
		comps.push_back(mThumbnail);
		
	if (mMarquee != nullptr)
		comps.push_back(mMarquee);

	comps.push_back(&mDescription);
	comps.push_back(&mName);
	std::vector<TextComponent*> labels = getMDLabels();
	comps.insert(comps.cend(), labels.cbegin(), labels.cend());

	for(auto it = comps.cbegin(); it != comps.cend(); it++)
	{
		GuiComponent* comp = *it;
		// an animation is playing
		//   then animate if reverse != fadingOut
		// an animation is not playing
		//   then animate if opacity != our target opacity
		if((comp->isAnimationPlaying(0) && comp->isAnimationReversed(0) != fadingOut) || 
			(!comp->isAnimationPlaying(0) && comp->getOpacity() != (fadingOut ? 0 : 255)))
		{
			auto func = [comp](float t)
			{
				comp->setOpacity((unsigned char)(Math::lerp(0.0f, 1.0f, t)*255));
			};

			bool isFadeOut = fadingOut;
			comp->setAnimation(new LambdaAnimation(func, 150), 0, [this, isFadeOut, file]
			{
				if (isFadeOut)
				{
					if (mVideo != nullptr) mVideo->setImage("");
					if (mImage != nullptr) mImage->setImage("");
					if (mThumbnail != nullptr) mThumbnail->setImage("");
					if (mMarquee != nullptr) mMarquee->setImage("");
				}
			}, fadingOut);
		}
	}
}

void DetailedGameListView::launch(FileData* game)
{
	Vector3f target(Renderer::getScreenWidth() / 2.0f, Renderer::getScreenHeight() / 2.0f, 0);
	
	if (mVideo != nullptr)
		target = Vector3f(mVideo->getCenter().x(), mVideo->getCenter().y(), 0);
	else if (mImage != nullptr && mImage->hasImage())
		target = Vector3f(mImage->getCenter().x(), mImage->getCenter().y(), 0);
	else if (mThumbnail != nullptr && mThumbnail->hasImage())
		target = Vector3f(mThumbnail->getCenter().x(), mThumbnail->getCenter().y(), 0);

	ViewController::get()->launch(game, target);
}

std::vector<TextComponent*> DetailedGameListView::getMDLabels()
{
	std::vector<TextComponent*> ret;
	ret.push_back(&mLblRating);
	ret.push_back(&mLblReleaseDate);
	ret.push_back(&mLblDeveloper);
	ret.push_back(&mLblPublisher);
	ret.push_back(&mLblGenre);
	ret.push_back(&mLblPlayers);
	ret.push_back(&mLblLastPlayed);
	ret.push_back(&mLblPlayCount);
	ret.push_back(&mLblGameTime);
	return ret;
}

std::vector<GuiComponent*> DetailedGameListView::getMDValues()
{
	std::vector<GuiComponent*> ret;
	ret.push_back(&mRating);
	ret.push_back(&mReleaseDate);
	ret.push_back(&mDeveloper);
	ret.push_back(&mPublisher);
	ret.push_back(&mGenre);
	ret.push_back(&mPlayers);
	ret.push_back(&mLastPlayed);
	ret.push_back(&mPlayCount);
	ret.push_back(&mGameTime);
	return ret;
}

void DetailedGameListView::onShow()
{
	BasicGameListView::onShow();
	updateInfoPanel();
}
