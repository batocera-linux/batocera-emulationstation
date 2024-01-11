#pragma once

#include "ImageComponent.h"
#include "BusyComponent.h"

class HttpReq;

class WebImageComponent : public ImageComponent
{
public:		
	WebImageComponent(Window* window, double keepInCacheDuration = -1);
	virtual ~WebImageComponent();

	std::string getThemeTypeName() override { return "webimage"; }

	void setImage(const std::string& path, bool tile = false, const MaxSizeInfo& maxSize = MaxSizeInfo::Empty, bool checkFileExists = true, bool allowMultiImagePlaylist = false) override;

	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;

	void setOnImageLoaded(const std::function<void()> onLoaded) { mOnLoaded = onLoaded; }

protected:
	virtual void resize() override;

private:
	HttpReq*	mRequest = nullptr;
	
	std::string mUrlToLoad;
	std::string mLocalFile;

	MaxSizeInfo mMaxSize;
	
	double		mKeepInCacheDuration;

	bool				  mWaitLoaded;
	std::function<void()> mOnLoaded;

	BusyComponent*		  mBusyAnim;
};