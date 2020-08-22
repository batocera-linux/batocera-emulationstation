#pragma once
#ifndef ES_APP_SYSTEM_SCREEN_SAVER_H
#define ES_APP_SYSTEM_SCREEN_SAVER_H

#include "Window.h"
#include "GuiComponent.h"
#include "renderers/Renderer.h"

class ImageComponent;
class Sound;
class VideoComponent;
class TextComponent;

class GameScreenSaverBase : public GuiComponent
{
public:
	GameScreenSaverBase(Window* window);
	~GameScreenSaverBase();

	virtual void setGame(FileData* mCurrentGame);

	void render(const Transform4x4f& transform) override;

	void setOpacity(unsigned char opacity) override;

protected:
	ImageComponent*		mMarquee;
	TextComponent*		mLabelGame;
	TextComponent*		mLabelSystem;

	ImageComponent*		mDecoration;

	Renderer::Rect		mViewport;
};

class ImageScreenSaver : public GameScreenSaverBase
{
public:
	ImageScreenSaver(Window* window);
	~ImageScreenSaver();

	void setImage(const std::string path);
	bool hasImage();

	void render(const Transform4x4f& transform) override;	

private:
	ImageComponent*		mImage;	
};

class VideoScreenSaver : public GameScreenSaverBase
{
public:
	VideoScreenSaver(Window* window);
	~VideoScreenSaver();

	void setVideo(const std::string path);
	void render(const Transform4x4f& transform) override;
	void update(int deltaTime) override;

private:
	VideoComponent*		mVideo;

	int mTime;
	float mFade;
};

// Screensaver implementation for main window
class SystemScreenSaver : public Window::ScreenSaver
{
public:
	SystemScreenSaver(Window* window);
	virtual ~SystemScreenSaver();

	virtual void startScreenSaver();
	virtual void stopScreenSaver();
	virtual void nextVideo();
	virtual void renderScreenSaver();
	virtual bool allowSleep();
	virtual void update(int deltaTime);
	virtual bool isScreenSaverActive();

	virtual FileData* getCurrentGame();
	virtual void launchGame();
	inline virtual void resetCounts() { mVideosCounted = false; mImagesCounted = false; };

private:
	unsigned long countGameListNodes(const char *nodeName);
	void countVideos();
	void countImages();

	std::string pickGameListNode(unsigned long index, const char *nodeName);
	std::string pickRandomVideo();
	std::string pickRandomGameListImage();
	std::string pickRandomCustomImage(bool video = false);

	enum STATE {
		STATE_INACTIVE,
		STATE_FADE_OUT_WINDOW,
		STATE_FADE_IN_VIDEO,
		STATE_SCREENSAVER_ACTIVE
	};

private:
	bool			mVideosCounted;
	unsigned long		mVideoCount;	
	bool			mImagesCounted;
	unsigned long		mImageCount;

	//VideoComponent*		mVideoScreensaver;
	std::shared_ptr<VideoScreenSaver>		mVideoScreensaver;

	std::shared_ptr<ImageScreenSaver>		mFadingImageScreensaver;
	std::shared_ptr<ImageScreenSaver>		mImageScreensaver;

	Window*			mWindow;
	STATE			mState;
	float			mOpacity;
	int				mTimer;
	FileData*		mCurrentGame;
	std::string		mGameName;
	std::string		mSystemName;
	int 			mVideoChangeTime;
	
	//std::shared_ptr<Sound>	mBackgroundAudio;
	bool			mLoadingNext;

};

#endif // ES_APP_SYSTEM_SCREEN_SAVER_H
