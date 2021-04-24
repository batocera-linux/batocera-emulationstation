#pragma once
#ifndef ES_CORE_COMPONENTS_VIDEO_COMPONENT_H
#define ES_CORE_COMPONENTS_VIDEO_COMPONENT_H

#include "components/ImageComponent.h"
#include "components/ImageGridComponent.h"
#include "GuiComponent.h"
#include "ImageIO.h"
#include <string>

class TextureResource;

std::string	getTitlePath();
std::string	getTitleFolder();
void writeSubtitle(const char* gameName, const char* systemName, bool always);
/*
enum ImageSource
{
	THUMBNAIL,
	IMAGE,
	MARQUEE
};
*/

class VideoComponent : public GuiComponent
{
	// Structure that groups together the configuration of the video component
	struct Configuration
	{
		unsigned						startDelay;
		bool							showSnapshotNoVideo;
		bool							showSnapshotDelay;
		ImageSource						snapshotSource;
		std::string						defaultVideoPath;
	};

public:
	VideoComponent(Window* window);
	virtual ~VideoComponent();

	std::string getValue() const override 
	{ 
		if (mPlayingVideoPath.empty())
			return mPlayingVideoPath;

		return mVideoPath;		
	}

	// Loads the video at the given filepath
	bool setVideo(std::string path);
	// Loads a static image that is displayed if the video cannot be played
	void setImage(std::string path, bool tile = false, MaxSizeInfo maxSize = MaxSizeInfo());

	// Configures the component to show the default video
	void setDefaultVideo();

	// sets whether it's going to render in screensaver mode
	void setScreensaverMode(bool isScreensaver);

	void setStartDelay(int delay) { mConfig.startDelay = delay; }

	virtual void onShow() override;
	virtual void onHide() override;
	virtual void onScreenSaverActivate() override;
	virtual void onScreenSaverDeactivate() override;
	virtual void topWindow(bool isTop) override;

	void onOriginChanged() override;
	void onSizeChanged() override;
	void setOpacity(unsigned char opacity) override;
	void setScale(float scale) override;

	void render(const Transform4x4f& parentTrans) override;
	void renderSnapshot(const Transform4x4f& parentTrans);

	virtual void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

	virtual std::vector<HelpPrompt> getHelpPrompts() override;

	virtual void update(int deltaTime);

	// Resize the video to fit this size. If one axis is zero, scale that axis to maintain aspect ratio.
	// If both are non-zero, potentially break the aspect ratio.  If both are zero, no resizing.
	// Can be set before or after a video is loaded.
	// setMaxSize() and setResize() are mutually exclusive.
	virtual void setResize(float width, float height) = 0;
	inline void setResize(const Vector2f& size) { setResize(size.x(), size.y()); }

	// Resize the video to be as large as possible but fit within a box of this size.
	// Can be set before or after a video is loaded.
	// Never breaks the aspect ratio. setMaxSize() and setResize() are mutually exclusive.
	virtual void setMaxSize(float width, float height) = 0;
	inline void setMaxSize(const Vector2f& size) { setMaxSize(size.x(), size.y()); }

	virtual void setMinSize(float width, float height) = 0;
	inline void setMinSize(const Vector2f& size) { setMinSize(size.x(), size.y()); }

	Vector2f getVideoSize() { return Vector2f(mVideoWidth, mVideoHeight); }
	bool isPlaying() {
		return mIsPlaying;
	}

	bool isWaitingForVideoToStart() {
		return mIsWaitingForVideoToStart;
	}

	virtual void onVideoStarted();

	const MaxSizeInfo getMaxSizeInfo()
	{
		if (mTargetSize == Vector2f(0, 0))
			return MaxSizeInfo(mSize, mTargetIsMax);

		return MaxSizeInfo(mTargetSize, mTargetIsMax);
	};

	ImageSource getSnapshotSource() { return mConfig.snapshotSource; };
	void setSnapshotSource(ImageSource source) { mConfig.snapshotSource = source; };

	inline void setOnVideoEnded(const std::function<bool()>& callback) {
		mVideoEnded = callback;
	}

	float getRoundCorners() { return mRoundCorners; }
	void setRoundCorners(float value);
	
	bool isFading() {
		return mIsPlaying && mFadeIn < 1.0;
	}

	float getFade() 
	{
		if (!mIsPlaying)
			return 0;

		return mFadeIn;
	}

	std::string getVideoPath() 
	{ 
		if (mPlayingVideoPath.empty())
			return mPlayingVideoPath;

		return mVideoPath; 
	}

	void setPlaylist(std::shared_ptr<IPlaylist> playList);
	void onPositionChanged() override;

	bool getPlayAudio() { return mPlayAudio; }
	void setPlayAudio(bool value) { mPlayAudio = value; }

	void setProperty(const std::string name, const ThemeData::ThemeElement::Property& value) override;

	virtual void setClipRect(const Vector4f& vec);

	Vector2f& getTargetSize() { return mTargetSize; }

	bool showSnapshots();

protected:
	std::shared_ptr<IPlaylist> mPlaylist;
	std::function<bool()> mVideoEnded;

private:
	// Start the video Immediately
	virtual void startVideo() = 0;
	// Stop the video
	virtual void stopVideo() { };
	// Handle looping the video. Must be called periodically
	virtual void handleLooping();

	// Start the video after any configured delay
	void startVideoWithDelay();

	// Handle any delay to the start of playing the video clip. Must be called periodically
	void handleStartDelay();

	// Manage the playing state of the component
	void manageState();


protected:
	unsigned						mVideoWidth;
	unsigned						mVideoHeight;
	Vector2f						mTargetSize;
	std::shared_ptr<TextureResource> mTexture;
	float							mFadeIn;
	std::string						mStaticImagePath;
	ImageComponent					mStaticImage;
	bool							mPlayAudio;

	std::string						mVideoPath;
	std::string						mPlayingVideoPath;
	bool							mStartDelayed;
	unsigned						mStartTime;
	bool							mIsPlaying;	
	bool							mDisable;
	bool							mScreensaverActive;
	bool							mScreensaverMode;
	bool							mTargetIsMax;
	bool							mTargetIsMin;

	bool							mIsWaitingForVideoToStart;

	float							mRoundCorners;

	Configuration					mConfig;
};

#endif // ES_CORE_COMPONENTS_VIDEO_COMPONENT_H
