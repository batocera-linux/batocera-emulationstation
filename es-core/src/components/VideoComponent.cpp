#include "components/VideoComponent.h"

#include "resources/ResourceManager.h"
#include "utils/FileSystemUtil.h"
#include "PowerSaver.h"
#include "ThemeData.h"
#include "Window.h"
#include <SDL_timer.h>
#include "LocaleES.h"
#include "Paths.h"

#define FADE_TIME_MS	400

std::string getTitlePath() 
{
	std::string titleFolder = getTitleFolder();
	return titleFolder + "subtitle.srt";
}

std::string getTitleFolder() 
{
#if WIN32
	return Utils::FileSystem::getGenericPath(Paths::getUserEmulationStationPath() + "/tmp/");
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
	mStaticImage(window, !Settings::AllImagesAsync()),
	mVideoHeight(0),
	mVideoWidth(0),
	mStartDelayed(false),
	mIsPlaying(false),
	mScreensaverActive(false),
	mIsTopWindow(true),
	mEnabled(true),
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
	mConfig.scaleSnapshot = true;

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
	GuiComponent::onOriginChanged();
	mStaticImage.setOrigin(mOrigin);
}

void VideoComponent::onPositionChanged()
{
	GuiComponent::onPositionChanged();
	mStaticImage.setPosition(mPosition);
}

void VideoComponent::onSizeChanged()
{
	GuiComponent::onSizeChanged();
	mStaticImage.onSizeChanged();
}

bool VideoComponent::setVideo(std::string path, bool checkFileExists)
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
	if (!fullPath.empty() && (!checkFileExists || ResourceManager::getInstance()->fileExists(fullPath)))
	{
		// Return true to show that we are going to attempt to play a video
		return true;
	}
	// Return false to show that no video will be displayed
	return false;
}

void VideoComponent::setImage(std::string path, bool tile, const MaxSizeInfo& maxSize)
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

//  Scale the opacity
void VideoComponent::onOpacityChanged()
{
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

	if (Settings::DebugImage())
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
			t = (t * (float)getOpacity());

		if (t == 0.0)
			return;

		if (!mStaticImage.isStoryBoardRunning("snapshot"))
		{
			t = ((getOpacity() / 255.0) * (t / 255.0)) * 255.0;
			mStaticImage.setOpacity((unsigned char)t);

			if (!currentStoryBoardHasProperty("scale") && getScale() == 1.0f)
			{
				if (mIsPlaying && mConfig.scaleSnapshot)
					mStaticImage.setScale(t / 255.0);
				else
					mStaticImage.setScale(getScale());
			}
		}

		mStaticImage.render(parentTrans);
	}
}

void VideoComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	using namespace ThemeFlags;

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "video");
	if (!elem)
		return;

	if (properties & ThemeFlags::SIZE)
	{
		Vector4f clientRectangle = getParent() ? getParent()->getClientRect() : Vector4f(0, 0, (float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
		Vector2f scale = Vector2f(clientRectangle.z(), clientRectangle.w());

		if (elem->has("size"))
		{
			auto sz = mSourceBounds.zw() = elem->get<Vector2f>("size");
			setResize(sz * scale);
		}
		else if (elem->has("maxSize"))
		{
			auto sz = mSourceBounds.zw() = elem->get<Vector2f>("maxSize");
			setMaxSize(sz * scale);
		}
		else if (elem->has("minSize"))
		{
			auto sz = mSourceBounds.zw() = elem->get<Vector2f>("minSize");
			setMinSize(sz * scale);
		}
	}

	if (elem->has("enabled"))
		mEnabled = elem->get<bool>("enabled");

	if ((properties & ThemeFlags::DELAY) && elem->has("delay"))
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
		else if (direction == "fanart")
			mConfig.snapshotSource = FANART;
		else if (direction == "titleshot")
			mConfig.snapshotSource = TITLESHOT;
		else if (direction == "boxart")
			mConfig.snapshotSource = BOXART;
		else if (direction == "cartridge")
			mConfig.snapshotSource = CARTRIDGE;
		else if (direction == "boxback")
			mConfig.snapshotSource = BOXBACK;
		else if (direction == "mix")
			mConfig.snapshotSource = MIX;
		else
			mConfig.snapshotSource = THUMBNAIL;
	}

	if (properties & ThemeFlags::VISIBLE && elem->has("audio"))
		setPlayAudio(elem->get<bool>("audio"));

	GuiComponent::applyTheme(theme, view, element, properties & ~ThemeFlags::SIZE);

	if (properties & PATH)
	{
		if (elem->has("default"))
			mConfig.defaultVideoPath = elem->get<std::string>("default");

		if (elem->has("path"))
		{
			auto path = elem->get<std::string>("path");

			if (path[0] == '{' || Utils::FileSystem::exists(path))
				mVideoPath = path;
			else
				mVideoPath = mConfig.defaultVideoPath;

			mThemedPath = mVideoPath;
		}
		else if (!mConfig.defaultVideoPath.empty())
		{
			mVideoPath = mConfig.defaultVideoPath;
			mThemedPath = mVideoPath;
		}
	}

	if (elem->has("defaultSnapshot"))
		mStaticImage.setDefaultImage(elem->get<std::string>("defaultSnapshot"));

	mStaticImage.applyStoryboard(elem, "snapshot");
}

std::vector<HelpPrompt> VideoComponent::getHelpPrompts()
{
	std::vector<HelpPrompt> ret;
	ret.push_back(HelpPrompt(BUTTON_BACK, _("SELECT")));
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
	bool show = isShowing() && !mScreensaverActive && mIsTopWindow && mVisible && mEnabled;
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

			if ((!mIsTopWindow || !mEnabled) && isShowing() && !mScreensaverActive && mIsPlaying && mVisible)
				pauseVideo();
			else
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
			if (isPaused())
				resumeVideo();
			else
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
	mIsTopWindow = isTop;
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
	else if (!mConfig.defaultVideoPath.empty())
		setVideo(mConfig.defaultVideoPath);
}

void VideoComponent::setRoundCorners(float value) 
{ 
	mRoundCorners = value; 
	mStaticImage.setRoundCorners(value);
}

ThemeData::ThemeElement::Property VideoComponent::getProperty(const std::string name)
{
	if (name == "path")
		return mVideoPath;

	if (name == "enabled")
		return mEnabled;

	return GuiComponent::getProperty(name);
}

void VideoComponent::setProperty(const std::string name, const ThemeData::ThemeElement::Property& value)
{
	if (hasStoryBoard() && !mStaticImage.hasStoryBoard("snapshot"))
	{
		if (name == "offset" || name == "offsetX" || name == "offsetY" || name == "scale")
			mStaticImage.setProperty(name, value);
	}

	if (value.type == ThemeData::ThemeElement::Property::PropertyType::String && name == "path")
	{
		if (Utils::FileSystem::exists(value.s))
			setVideo(value.s, false);
		else
			setVideo(mConfig.defaultVideoPath);		

		mThemedPath = mVideoPath;
	}
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Bool && name == "enabled")
	{
		mEnabled = value.b;

		if (!mEnabled)
		{
			mVideoPath = "";
			mPlayingVideoPath = "";
			mStartDelayed = false;
			mIsWaitingForVideoToStart = false;

			stopVideo();
		}		
		else
		{
			mVideoPath = "";
			mPlayingVideoPath = "";
			mStartDelayed = false;
			mIsWaitingForVideoToStart = false;

			setVideo(mThemedPath, false);
		}
	}
	else 
		GuiComponent::setProperty(name, value);
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

void VideoComponent::recalcLayout()
{
	if (mExtraType != ExtraType::EXTRACHILDREN || !getParent())
		return;

	Vector4f clientRectangle = getParent()->getClientRect();
	Vector2f newPos = mSourceBounds.xy() * clientRectangle.zw() + clientRectangle.xy();
	Vector2f newSize = mSourceBounds.zw() * clientRectangle.zw();

	if (mPosition.v2() != newPos)
		setPosition(newPos.x(), newPos.y());

	if (mSize != newSize)
	{
		if (mTargetIsMax)
			setMaxSize(newSize);
		else if (mTargetIsMin)
			setMinSize(newSize);
		else
			setResize(newSize);
	}
}
