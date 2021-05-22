#include "components/VideoComponent.h"

#include "resources/ResourceManager.h"
#include "utils/FileSystemUtil.h"
#include "PowerSaver.h"
#include "ThemeData.h"
#include "Window.h"
#include <SDL_timer.h>
#include "LocaleES.h"

#define FADE_TIME_MS	800

std::string getTitlePath() 
{
	std::string titleFolder = getTitleFolder();
	return titleFolder + "subtitle.srt";
}

std::string getTitleFolder() 
{
#if WIN32
	return Utils::FileSystem::getGenericPath(Utils::FileSystem::getEsConfigPath() + "/tmp/");
#endif

	return "/tmp/";
}

void writeSubtitle(const char* gameName, const char* systemName, bool always)
{
	FILE* file = fopen(getTitlePath().c_str(), "w");
	if (file)
	{
		int end = (int)(Settings::getInstance()->getInt("ScreenSaverSwapVideoTimeout") / (1000));
		if (always)
			fprintf(file, "1\n00:00:01,000 --> 00:00:%d,000\n", end);
		else
			fprintf(file, "1\n00:00:01,000 --> 00:00:08,000\n");

		fprintf(file, "%s\n", gameName);
		fprintf(file, "<i>%s</i>\n\n", systemName);

		if (!always && end > 12)
			fprintf(file, "2\n00:00:%d,000 --> 00:00:%d,000\n%s\n<i>%s</i>\n", end - 4, end, gameName, systemName);

		fflush(file);
		fclose(file);
		file = NULL;
	}
}

void VideoComponent::setScreensaverMode(bool isScreensaver)
{
	mScreensaverMode = isScreensaver;
}

VideoComponent::VideoComponent(Window* window) :
	GuiComponent(window),
	mStaticImage(window, true),
	mVideoHeight(0),
	mVideoWidth(0),
	mStartDelayed(false),
	mIsPlaying(false),	
	mScreensaverActive(false),
	mDisable(false),
	mScreensaverMode(false),
	mTargetIsMax(false),
	mTargetIsMin(false),
	mTargetSize(0, 0),
	mPlayAudio(true)
{
	mScaleOrigin = Vector2f::Zero();

	setTag("video");
	mVideoEnded = nullptr;
	mRoundCorners = 0.0f;
	mFadeIn = 0.0f;
	mIsWaitingForVideoToStart = false;

	mStaticImage.setAllowFading(false);

	// Setup the default configuration
	mConfig.showSnapshotDelay 		= false;
	mConfig.showSnapshotNoVideo		= false;
	mConfig.snapshotSource = IMAGE;
	mConfig.startDelay				= 0;

	if (mWindow->getGuiStackSize() > 1)
		topWindow(false);
}

VideoComponent::~VideoComponent()
{
	// Stop any currently running video
	stopVideo();
	// Delete subtitle file, if existing
	remove(getTitlePath().c_str());
}

void VideoComponent::onOriginChanged()
{
	// Update the embeded static image
	mStaticImage.setOrigin(mOrigin);
}

void VideoComponent::onSizeChanged()
{
	// Update the embeded static image
	mStaticImage.onSizeChanged();
}

bool VideoComponent::setVideo(std::string path)
{
	if (path == mVideoPath)
		return !path.empty();

	// Convert the path into a generic format
	std::string fullPath = Utils::FileSystem::getCanonicalPath(path);

	// Check that it's changed
	if (fullPath == mVideoPath)
		return !path.empty();

	// Store the path
	mVideoPath = fullPath;
	mStartDelayed = false;

	// If the file exists then set the new video
	if (!fullPath.empty() && ResourceManager::getInstance()->fileExists(fullPath))
	{
		// Return true to show that we are going to attempt to play a video
		return true;
	}
	// Return false to show that no video will be displayed
	return false;
}

void VideoComponent::setImage(std::string path, bool tile, MaxSizeInfo maxSize)
{
	// Check that the image has changed
	if (!path.empty() && path == mStaticImagePath)
		return;

	mStaticImage.setImage(path, tile, maxSize);
	mFadeIn = 0.0f;
	mStaticImagePath = path;
}

void VideoComponent::setDefaultVideo()
{
	setVideo(mConfig.defaultVideoPath);
}

void VideoComponent::setOpacity(unsigned char opacity)
{
	if (mOpacity == opacity)
		return;

	mOpacity = opacity;	

	//if (!hasStoryBoard() && !mStaticImage.hasStoryBoard("snapshot"))
	//	mStaticImage.setOpacity(opacity);
}

void VideoComponent::setScale(float scale)
{
	GuiComponent::setScale(scale);

	if (!hasStoryBoard() && !mStaticImage.hasStoryBoard("snapshot"))
		mStaticImage.setScale(scale);
}

void VideoComponent::render(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	Transform4x4f trans = parentTrans * getTransform();
/*
	if (!Renderer::isVisibleOnScreen(trans.translation().x(), trans.translation().y(), mSize.x(), mSize.y()))
		return;
		*/

	beginCustomClipRect();

	GuiComponent::renderChildren(trans);
	VideoComponent::renderSnapshot(parentTrans);

	endCustomClipRect();

	Renderer::setMatrix(trans);

	if (Settings::DebugImage)
	{
		Vector2f targetSizePos = (mTargetSize - mSize) * mOrigin * -1;
		Renderer::drawRect(targetSizePos.x(), targetSizePos.y(), mTargetSize.x(), mTargetSize.y(), 0xFF000033);
		Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), 0x00000033);
	}

	// Handle the case where the video is delayed
	if (!GuiComponent::isLaunchTransitionRunning)
		handleStartDelay();

	// Handle looping of the video
	handleLooping();
}

void VideoComponent::renderSnapshot(const Transform4x4f& parentTrans)
{
	// This is the case where the video is not currently being displayed. Work out
	// if we need to display a static image
	if ((mConfig.showSnapshotNoVideo && mVideoPath.empty()) || ((mStartDelayed || mFadeIn < 1.0) && mConfig.showSnapshotDelay))
	{
		float t = 1.0 - mFadeIn;
		t -= 1; // cubic ease out
		t = Math::lerp(0, 1, t*t*t + 1);

		if (hasStoryBoard())
			t = (t * 255.0f);
		else
			t = (t * (float)mOpacity);

		if (t == 0.0)
			return;

		if (!mStaticImage.isStoryBoardRunning("snapshot"))
		{
			t = ((getOpacity() / 255.0) * (t / 255.0)) * 255.0;
			mStaticImage.setOpacity((unsigned char)t);
		}

		mStaticImage.render(parentTrans);
	}
}

void VideoComponent::onPositionChanged()
{
	mStaticImage.setPosition(mPosition);
}

void VideoComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	using namespace ThemeFlags;

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "video");
	if(!elem)
		return;

	Vector2f screenScale = Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	Vector2f scale = getParent() ? getParent()->getSize() : screenScale;

	if ((properties & POSITION) && elem->has("pos"))
	{
		Vector2f denormalized = elem->get<Vector2f>("pos") * scale;
		setPosition(Vector3f(denormalized.x(), denormalized.y(), 0));
	}

	if (properties & POSITION && elem->has("x"))
	{
		float denormalized = elem->get<float>("x") * scale.x();
		setPosition(Vector3f(denormalized, mPosition.y(), 0));
	}

	if (properties & POSITION && elem->has("y"))
	{
		float denormalized = elem->get<float>("y") * scale.y();
		setPosition(Vector3f(mPosition.x(), denormalized, 0));
	}

	if(properties & ThemeFlags::SIZE)
	{
		if (elem->has("size"))
		{
			mSize = elem->get<Vector2f>("size") * scale;
			setResize(mSize);
		}
		else if (elem->has("maxSize"))
		{
			mSize = elem->get<Vector2f>("maxSize") * scale;
			setMaxSize(mSize);
		}
		else if (elem->has("minSize"))
		{
			mSize = elem->get<Vector2f>("minSize") * scale;
			setMinSize(mSize);
		}
	}

	// position + size also implies origin
	if (((properties & ORIGIN) || ((properties & POSITION) && (properties & ThemeFlags::SIZE))) && elem->has("origin"))
		setOrigin(elem->get<Vector2f>("origin"));

	if(elem->has("default"))
		mConfig.defaultVideoPath = elem->get<std::string>("default");

	if((properties & ThemeFlags::DELAY) && elem->has("delay"))
		mConfig.startDelay = (unsigned)(elem->get<float>("delay") * 1000.0f);

	if (elem->has("showSnapshotNoVideo"))
		mConfig.showSnapshotNoVideo = elem->get<bool>("showSnapshotNoVideo");

	if (elem->has("showSnapshotDelay"))
		mConfig.showSnapshotDelay = elem->get<bool>("showSnapshotDelay");

	if (elem->has("snapshotSource"))
	{
		auto direction = elem->get<std::string>("snapshotSource");
		if (direction == "image")
			mConfig.snapshotSource = IMAGE;
		else if (direction == "marquee")
			mConfig.snapshotSource = MARQUEE;
		else
			mConfig.snapshotSource = THUMBNAIL;
	}

	if(properties & ThemeFlags::ROTATION) 
	{
		if(elem->has("rotation"))
			setRotationDegrees(elem->get<float>("rotation"));
		if(elem->has("rotationOrigin"))
			setRotationOrigin(elem->get<Vector2f>("rotationOrigin"));
	}

	if(properties & ThemeFlags::Z_INDEX && elem->has("zIndex"))
		setZIndex(elem->get<float>("zIndex"));
	else
		setZIndex(getDefaultZIndex());

	if(properties & ThemeFlags::VISIBLE && elem->has("visible"))
		setVisible(elem->get<bool>("visible"));
	else
		setVisible(true);

	if (properties & ThemeFlags::VISIBLE && elem->has("audio"))
		setPlayAudio(elem->get<bool>("audio"));
	else
		setPlayAudio(true);

	if (elem->has("path"))
	{
		if (Utils::FileSystem::exists(elem->get<std::string>("path")))
			mVideoPath = elem->get<std::string>("path");
		else
			mVideoPath = mConfig.defaultVideoPath;
	}

	if (properties & POSITION && elem->has("offset"))
	{
		Vector2f denormalized = elem->get<Vector2f>("offset") * screenScale;
		mScreenOffset = denormalized;
	}

	if (properties & POSITION && elem->has("offsetX"))
	{
		float denormalized = elem->get<float>("offsetX") * screenScale.x();
		mScreenOffset = Vector2f(denormalized, mScreenOffset.y());
	}

	if (properties & POSITION && elem->has("offsetY"))
	{
		float denormalized = elem->get<float>("offsetY") * scale.y();
		mScreenOffset = Vector2f(mScreenOffset.x(), denormalized);
	}

	if (properties & POSITION && elem->has("clipRect"))
	{
		Vector4f val = elem->get<Vector4f>("clipRect") * Vector4f(screenScale.x(), screenScale.y(), screenScale.x(), screenScale.y());
		setClipRect(val);
	}
	else
		setClipRect(Vector4f());

	if (elem->has("defaultSnapshot"))
		mStaticImage.setDefaultImage(elem->get<std::string>("defaultSnapshot"));
}

std::vector<HelpPrompt> VideoComponent::getHelpPrompts()
{
	std::vector<HelpPrompt> ret;
	ret.push_back(HelpPrompt(BUTTON_BACK, _("SELECT"))); // batocera
	return ret;
}

void VideoComponent::handleStartDelay()
{
	// Only play if any delay has timed out
	if (!mStartDelayed || mIsWaitingForVideoToStart)
		return;

	// Timeout not yet completed
	if (mStartTime > SDL_GetTicks())
		return;

	// Completed
	mStartDelayed = false;
	// Clear the playing flag so startVideo works
	mIsPlaying = false;

	mIsWaitingForVideoToStart = true;

	startVideo();

	if (mIsPlaying)
		mIsWaitingForVideoToStart = false;
}

void VideoComponent::onVideoStarted()
{
	mIsWaitingForVideoToStart = false;

	if (mConfig.startDelay == 0 || PowerSaver::getMode() == PowerSaver::INSTANT)
	{
		mFadeIn = 1.0f;
		mIsPlaying = true;
	}
	else
	{
		mFadeIn = 0.0f;
		mIsPlaying = true;
	}
}

void VideoComponent::handleLooping()
{
}

void VideoComponent::startVideoWithDelay()
{
	// If not playing then either start the video or initiate the delay
	if (mIsPlaying || mStartDelayed || mIsWaitingForVideoToStart)
		return;

	// Set the video that we are going to be playing so we don't attempt to restart it
	mPlayingVideoPath = mVideoPath;

	if (mConfig.startDelay == 0 || PowerSaver::getMode() == PowerSaver::INSTANT)
	{
		// No delay. Just start the video
		mStartDelayed = false;
		mIsPlaying = false;

		mIsWaitingForVideoToStart = true;

		startVideo();

		if (mIsPlaying)
			mIsWaitingForVideoToStart = false;
	}
	else
	{
		// Configure the start delay
		mStartDelayed = true;
		mFadeIn = 0.0f;
		mStartTime = SDL_GetTicks() + mConfig.startDelay;
	}
}

void VideoComponent::update(int deltaTime)
{
	manageState();

	if (mIsPlaying)
	{
		// If the video start is delayed and there is less than the fade time then set the image fade
		// accordingly

		if (mStartDelayed)
		{
			Uint32 ticks = SDL_GetTicks();
			if (mStartTime > ticks)
			{
				Uint32 diff = mStartTime - ticks;
				if (diff < FADE_TIME_MS)
				{
					mFadeIn = (float)diff / (float)FADE_TIME_MS;
					return;
				}
			}
		}

		// If the fade in is less than 1 then increment it
		if (mFadeIn < 1.0f)
		{
			mFadeIn += deltaTime / (float)FADE_TIME_MS;
			if (mFadeIn > 1.0f)
				mFadeIn = 1.0f;
		}
	}

	GuiComponent::update(deltaTime);
}

void VideoComponent::manageState()
{
	if (mIsWaitingForVideoToStart && mIsPlaying)
		mIsWaitingForVideoToStart = false;

	// We will only show if the component is on display and the screensaver
	// is not active
	bool show = isShowing() && !mScreensaverActive && !mDisable && mVisible;
	if (!show)
		mStartDelayed = false;

	// See if we're already playing
	if (mIsPlaying || mIsWaitingForVideoToStart)
	{
		// If we are not on display then stop the video from playing
		if (!show)
		{
			mIsWaitingForVideoToStart = false;
			mStartDelayed = false;
			stopVideo();
		}
		else
		{
			if (mVideoPath != mPlayingVideoPath)
			{
				// Path changed. Stop the video. We will start it again below because
				// mIsPlaying will be modified by stopVideo to be false
				mStartDelayed = false;
				mIsWaitingForVideoToStart = false;
				stopVideo();
			}
		}
	}
	// Need to recheck variable rather than 'else' because it may be modified above
	if (!mIsPlaying)
	{
		// If we are on display then see if we should start the video
		if (show && !mVideoPath.empty())
		{
			startVideoWithDelay();
		}
	}
}

void VideoComponent::onShow()
{
	if (!isShowing() && mPlaylist != nullptr && !mVideoPath.empty() && mPlaylist->getRotateOnShow())
	{
		auto video = mPlaylist->getNextItem();
		if (!video.empty())
			mVideoPath = video;
	}

	GuiComponent::onShow();
	manageState();
}

void VideoComponent::onHide()
{
	GuiComponent::onHide();	
	manageState();
}

void VideoComponent::onScreenSaverActivate()
{
	mScreensaverActive = true;
	manageState();
}

void VideoComponent::onScreenSaverDeactivate()
{
	mScreensaverActive = false;
	manageState();
}

void VideoComponent::topWindow(bool isTop)
{
	mDisable = !isTop;
	manageState();
}


void VideoComponent::setPlaylist(std::shared_ptr<IPlaylist> playList)
{
	mPlaylist = playList;
	if (mPlaylist == nullptr)
		return;

	auto video = mPlaylist->getNextItem();
	if (!video.empty())
		setVideo(video);
}

void VideoComponent::setRoundCorners(float value) 
{ 
	mRoundCorners = value; 
	mStaticImage.setRoundCorners(value);
}

void VideoComponent::setProperty(const std::string name, const ThemeData::ThemeElement::Property& value)
{
	GuiComponent::setProperty(name, value);

	if (hasStoryBoard() && !mStaticImage.hasStoryBoard("snapshot"))
	{
		if (name == "offset" || name == "offsetX" || name == "offsetY" || name == "scale")
			mStaticImage.setProperty(name, value);
	}
}

void VideoComponent::setClipRect(const Vector4f& vec)
{
	GuiComponent::setClipRect(vec);
	mStaticImage.setClipRect(vec);
}

bool VideoComponent::showSnapshots()
{
	return mConfig.showSnapshotNoVideo || (mConfig.showSnapshotDelay && mConfig.startDelay);
}