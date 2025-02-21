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
	void update(int deltaTime) override;

	void setOpacity(unsigned char opacity) override;

protected:
	ImageComponent*		mMarquee;
	TextComponent*		mLabelGame;
	TextComponent*		mLabelSystem;
	TextComponent*		mLabelDate;
	TextComponent*		mLabelTime;

	ImageComponent*		mDecoration;

	Renderer::Rect		mViewport;

	int 				mDateTimeUpdateAccumulator;
	time_t				mDateTimeLastUpdate;
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

class SystemScreenSaver;

class VideoScreenSaver : public GameScreenSaverBase
{
public:
	VideoScreenSaver(Window* window, SystemScreenSaver* systemScreenSaver);
	~VideoScreenSaver();

	void setVideo(const std::string path);
	void render(const Transform4x4f& transform) override;
	void update(int deltaTime) override;

private:
	VideoComponent*		mVideo;
	SystemScreenSaver*  mSystemScreenSaver;

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
	inline virtual void resetCounts() { mGamesWithVideosLoaded = false; mGamesWithImagesLoaded = false; mGamesWithImages.clear(); mGamesWithVideos.clear(); };

private:
	unsigned long countGameListNodes(bool video = false);

	std::string pickRandomGameMedia(bool video = false);
	std::string pickRandomCustomImage(bool video = false);
	
	std::string	selectGameMedia(FileData* game, bool video = false);
	
	enum STATE {
		STATE_INACTIVE,
		STATE_FADE_OUT_WINDOW,
		STATE_FADE_IN_VIDEO,
		STATE_SCREENSAVER_ACTIVE
	};

private:
	std::shared_ptr<VideoScreenSaver>		mVideoScreensaver;
	std::shared_ptr<ImageScreenSaver>		mFadingImageScreensaver;
	std::shared_ptr<ImageScreenSaver>		mImageScreensaver;

	std::vector<FileData*> mGamesWithImages;
	std::vector<FileData*> mGamesWithVideos;

	bool			mGamesWithVideosLoaded;
	bool			mGamesWithImagesLoaded;
	Window*			mWindow;
	STATE			mState;
	float			mOpacity;
	int				mTimer;
	FileData*		mCurrentGame;
	std::string		mGameName;
	std::string		mSystemName;
	int 			mVideoChangeTime;
	bool			mLoadingNext;

};

#endif // ES_APP_SYSTEM_SCREEN_SAVER_H
