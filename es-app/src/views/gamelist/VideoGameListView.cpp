#include "views/gamelist/VideoGameListView.h"

#include "animations/LambdaAnimation.h"
#ifdef _RPI_
#include "components/VideoPlayerComponent.h"
#endif
#include "components/VideoVlcComponent.h"
#include "utils/FileSystemUtil.h"
#include "FileData.h"
#include "SystemData.h"
#include "views/ViewController.h"
#ifdef _RPI_
#include "Settings.h"
#endif

VideoGameListView::VideoGameListView(Window* window, FolderData* root) :
	BasicGameListView(window, root),
	mDescContainer(window), mDescription(window),
	mMarquee(window),
	mImage(window),
	mVideo(nullptr),
	mVideoPlaying(false),
	mThumbnail(nullptr),

	mLblRating(window), mLblReleaseDate(window), mLblDeveloper(window), mLblPublisher(window),
	mLblGenre(window), mLblPlayers(window), mLblLastPlayed(window), mLblPlayCount(window), mLblGameTime(window),

	mRating(window), mReleaseDate(window), mDeveloper(window), mPublisher(window),
	mGenre(window), mPlayers(window), mLastPlayed(window), mPlayCount(window),
	mName(window), mGameTime(window)
{
	const float padding = 0.01f;

	mLblGameTime.setVisible(false);
	mGameTime.setVisible(false);

	// Create the correct type of video window
#ifdef _RPI_
	if (Settings::getInstance()->getBool("VideoOmxPlayer"))
		mVideo = new VideoPlayerComponent(window, "");
	else
		mVideo = new VideoVlcComponent(window, getTitlePath());
#else
	mVideo = new VideoVlcComponent(window, getTitlePath());
#endif
	
	mVideo->setSnapshotSource(IMAGE);

	mList.setPosition(mSize.x() * (0.50f + padding), mList.getPosition().y());
	mList.setSize(mSize.x() * (0.50f - padding), mList.getSize().y());
	mList.setAlignment(TextListComponent<FileData*>::ALIGN_LEFT);
	mList.setCursorChangedCallback([&](const CursorState& /*state*/) { updateInfoPanel(); });

	// Marquee
	mMarquee.setAllowFading(false);
	mMarquee.setOrigin(0.5f, 0.5f);
	mMarquee.setPosition(mSize.x() * 0.25f, mSize.y() * 0.10f);
	mMarquee.setMaxSize(mSize.x() * (0.5f - 2*padding), mSize.y() * 0.18f);
	mMarquee.setDefaultZIndex(35);
	addChild(&mMarquee);

	// Image
	mImage.setAllowFading(false);
	mImage.setOrigin(0.5f, 0.5f);
	// Default to off the screen
	mImage.setPosition(2.0f, 2.0f);
	mImage.setMaxSize(1.0f, 1.0f);
	mImage.setDefaultZIndex(30);
	addChild(&mImage);

	// video
	mVideo->setOrigin(0.5f, 0.5f);
	mVideo->setPosition(mSize.x() * 0.25f, mSize.y() * 0.4f);
	mVideo->setSize(mSize.x() * (0.5f - 2*padding), mSize.y() * 0.4f);
	mVideo->setDefaultZIndex(30);
	addChild(mVideo);

	// metadata labels + values
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
}

VideoGameListView::~VideoGameListView()
{
	if (mThumbnail != nullptr)
		delete mThumbnail;

	if (mVideo != nullptr)
		delete mVideo;
}

void VideoGameListView::createThumbnail()
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

void VideoGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	BasicGameListView::onThemeChanged(theme);

	using namespace ThemeFlags;
	mMarquee.applyTheme(theme, getName(), "md_marquee", ALL ^ (PATH));
	mImage.applyTheme(theme, getName(), "md_image", ALL ^ (PATH));
	mVideo->applyTheme(theme, getName(), "md_video", ALL ^ (PATH));
	mName.applyTheme(theme, getName(), "md_name", ALL);

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
}

void VideoGameListView::initMDLabels()
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

void VideoGameListView::initMDValues()
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



void VideoGameListView::updateInfoPanel()
{
	if (mRoot->getSystem()->isCollection())
		updateHelpPrompts();

	FileData* file = (mList.size() == 0 || mList.isScrolling()) ? NULL : mList.getSelected();

	Utils::FileSystem::removeFile(getTitlePath());

	bool fadingOut;
	if(file == NULL)
	{
		mVideo->setVideo("");
		mVideo->setImage("");
		mVideoPlaying = false;
		//mMarquee.setImage("");
		//mDescription.setText("");
		fadingOut = true;

	}else{
		if (!mVideo->setVideo(file->getVideoPath()))
		{
			mVideo->setDefaultVideo();
		}
		mVideoPlaying = true;
		
		std::string snapShot = file->getThumbnailPath();

		auto src = mVideo->getSnapshotSource();
		if (src == MARQUEE && !file->getMarqueePath().empty())
			snapShot = file->getMarqueePath();
		if (src == IMAGE && !file->getImagePath().empty())
			snapShot = file->getImagePath();

		mVideo->setImage(snapShot);

		mMarquee.setImage(file->getMarqueePath()/*, false, mMarquee.getMaxSizeInfo()*/); // Too slow on pi

		if (mThumbnail != nullptr)
		{
			mImage.setImage(file->getImagePath(), false, mImage.getMaxSizeInfo());
			mThumbnail->setImage(file->getThumbnailPath(), false, mThumbnail->getMaxSizeInfo());
		}
		else
			mImage.setImage(file->getThumbnailPath(), false, mImage.getMaxSizeInfo());

		mDescription.setText(file->getMetadata().get("desc"));
		mDescContainer.reset();

		mRating.setValue(file->getMetadata().get("rating"));
		mReleaseDate.setValue(file->getMetadata().get("releasedate"));
		mDeveloper.setValue(file->getMetadata().get("developer"));
		mPublisher.setValue(file->getMetadata().get("publisher"));
		mGenre.setValue(file->getMetadata().get("genre"));
		mPlayers.setValue(file->getMetadata().get("players"));
		mName.setValue(file->getMetadata().get("name"));

		if(file->getType() == GAME)
		{
			mLastPlayed.setValue(file->getMetadata().get("lastplayed"));
			mPlayCount.setValue(file->getMetadata().get("playcount"));
			mGameTime.setValue(Utils::Time::secondsToString(atol(file->getMetadata("gametime").c_str())));
		}

		fadingOut = false;
	}

	// We're clearing / populating : don't setup fade animations
	if (file == nullptr && mList.getObjects().size() == 0 && mList.getCursorIndex() == 0 && mList.getScrollingVelocity() == 0)
		return;

	std::vector<GuiComponent*> comps = getMDValues();
	comps.push_back(&mMarquee);
	comps.push_back(mVideo);
	comps.push_back(&mDescription);
	comps.push_back(&mImage);

	if (mThumbnail != nullptr)
		comps.push_back(mThumbnail);

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
			comp->setAnimation(new LambdaAnimation(func, 150), 0, [this, isFadeOut]
			{
				if (isFadeOut)
				{
					if (mVideo != nullptr) mVideo->setImage("");
					if (mThumbnail != nullptr) mThumbnail->setImage("");

					mImage.setImage("");
					mMarquee.setImage("");
				}
			}, fadingOut);
		}
	}
}

void VideoGameListView::launch(FileData* game)
{
	float screenWidth = (float) Renderer::getScreenWidth();
	float screenHeight = (float) Renderer::getScreenHeight();

	Vector3f target(screenWidth / 2.0f, screenHeight / 2.0f, 0);

	if(mMarquee.hasImage() &&
		(mMarquee.getPosition().x() < screenWidth && mMarquee.getPosition().x() > 0.0f &&
		 mMarquee.getPosition().y() < screenHeight && mMarquee.getPosition().y() > 0.0f))
	{
		target = Vector3f(mMarquee.getCenter().x(), mMarquee.getCenter().y(), 0);
	}
	else if(mImage.hasImage() &&
		(mImage.getPosition().x() < screenWidth && mImage.getPosition().x() > 2.0f &&
		 mImage.getPosition().y() < screenHeight && mImage.getPosition().y() > 2.0f))
	{
		target = Vector3f(mImage.getCenter().x(), mImage.getCenter().y(), 0);
	}
	else if(mHeaderImage.hasImage() &&
		(mHeaderImage.getPosition().x() < screenWidth && mHeaderImage.getPosition().x() > 0.0f &&
		 mHeaderImage.getPosition().y() < screenHeight && mHeaderImage.getPosition().y() > 0.0f))
	{
		target = Vector3f(mHeaderImage.getCenter().x(), mHeaderImage.getCenter().y(), 0);
	}
	else if(mVideo != nullptr && mVideo->getPosition().x() < screenWidth && mVideo->getPosition().x() > 0.0f &&
		 mVideo->getPosition().y() < screenHeight && mVideo->getPosition().y() > 0.0f)
	{
		target = Vector3f(mVideo->getCenter().x(), mVideo->getCenter().y(), 0);
	}
	else if (mThumbnail != nullptr && mThumbnail->getPosition().x() < screenWidth && mThumbnail->getPosition().x() > 0.0f &&
		mThumbnail->getPosition().y() < screenHeight && mThumbnail->getPosition().y() > 0.0f)
	{
		target = Vector3f(mThumbnail->getCenter().x(), mThumbnail->getCenter().y(), 0);
	}

	ViewController::get()->launch(game, target);
}

std::vector<TextComponent*> VideoGameListView::getMDLabels()
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

std::vector<GuiComponent*> VideoGameListView::getMDValues()
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

void VideoGameListView::update(int deltaTime)
{
	BasicGameListView::update(deltaTime);
	mVideo->update(deltaTime);
}

void VideoGameListView::onShow()
{
	GuiComponent::onShow();
	updateInfoPanel();
}
