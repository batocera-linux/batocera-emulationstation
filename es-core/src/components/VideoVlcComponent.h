#pragma once
#ifndef ES_CORE_COMPONENTS_VIDEO_VLC_COMPONENT_H
#define ES_CORE_COMPONENTS_VIDEO_VLC_COMPONENT_H

#include "VideoComponent.h"
#include "ThemeData.h"
#include <mutex>

struct libvlc_instance_t;
struct libvlc_media_t;
struct libvlc_media_player_t;

struct VideoContext 
{
	VideoContext()
	{
		surfaces[0] = nullptr;
		surfaces[1] = nullptr;
		component = nullptr;
		valid = false;
		hasFrame[0] = false;
		hasFrame[1] = false;
		surfaceId = 0;
	}

	int					surfaceId;
	unsigned char*		surfaces[2];	
	std::mutex			mutexes[2];
	bool				hasFrame[2];

	VideoComponent*		component;
	bool				valid;	
};


namespace VideoVlcFlags
{
	enum VideoVlcEffect
	{
		NONE,
		BUMP,
		SIZE,
		SLIDERIGHT
	};
}

class VideoVlcComponent : public VideoComponent
{
	// Structure that groups together the configuration of the video component
	struct Configuration
	{
		unsigned						startDelay;
		bool							showSnapshotNoVideo;
		bool							showSnapshotDelay;
		std::string						defaultVideoPath;
	};

public:
	static void setupVLC(std::string subtitles);

	VideoVlcComponent(Window* window, std::string subtitles="");
	virtual ~VideoVlcComponent();

	void render(const Transform4x4f& parentTrans) override;

	// Resize the video to fit this size. If one axis is zero, scale that axis to maintain aspect ratio.
	// If both are non-zero, potentially break the aspect ratio.  If both are zero, no resizing.
	// Can be set before or after a video is loaded.
	// setMaxSize() and setResize() are mutually exclusive.
	void setResize(float width, float height);

	// Resize the video to be as large as possible but fit within a box of this size.
	// Can be set before or after a video is loaded.
	// Never breaks the aspect ratio. setMaxSize() and setResize() are mutually exclusive.
	void setMaxSize(float width, float height);
	void setMinSize(float width, float height);

	virtual void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties);
	virtual void update(int deltaTime);

	void	setColorShift(unsigned int color);

	virtual void onShow() override;

	ThemeData::ThemeElement::Property getProperty(const std::string name) override;
	void setProperty(const std::string name, const ThemeData::ThemeElement::Property& value) override;

private:
	// Calculates the correct mSize from our resizing information (set by setResize/setMaxSize).
	// Used internally whenever the resizing parameters or texture change.
	void resize();
	// Start the video Immediately
	virtual void startVideo();
	// Stop the video
	virtual void stopVideo();
	// Handle looping the video. Must be called periodically
	virtual void handleLooping();

	virtual void onVideoStarted();

	void setupContext();
	void freeContext();

	void setEffect(VideoVlcFlags::VideoVlcEffect effect) { mEffect = effect; }

private:
	static libvlc_instance_t*		mVLC;
	libvlc_media_t*					mMedia;
	libvlc_media_player_t*			mMediaPlayer;
	VideoContext					mContext;
	std::shared_ptr<TextureResource> mTexture;

	std::string					    mSubtitlePath;
	std::string					    mSubtitleTmpFile;

	VideoVlcFlags::VideoVlcEffect	mEffect;

	unsigned int					mColorShift;
	int								mElapsed;

	int								mCurrentLoop;
	int								mLoops;
};

#endif // ES_CORE_COMPONENTS_VIDEO_VLC_COMPONENT_H
