#pragma once
#ifndef ES_APP_VIEWS_CCCSYSTEM_VIEW_H
#define ES_APP_VIEWS_CCCSYSTEM_VIEW_H

#include "components/IList.h"
#include "components/TextComponent.h"
#include "resources/Font.h"
#include "GuiComponent.h"
#include <memory>
#include <functional>

class FileData;
class FolderData;
class ThemeData;

struct CarouselComponentData
{
	std::shared_ptr<GuiComponent> logo;
	// std::vector<GuiComponent*> backgroundExtras;
};

class CarouselComponent : public IList<CarouselComponentData, FileData*>
{
public:
	CarouselComponent(Window* window);
	~CarouselComponent();

	virtual void onShow() override;
	virtual void onHide() override;

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;

	std::vector<HelpPrompt> getHelpPrompts() override;

	void		add(const std::string& name, FileData* obj);
	FileData*	getActiveFileData();

	inline void setCursorChangedCallback(const std::function<void(CursorState state)>& func) { mCursorChangedCallback = func; }
	void	applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties);

	int getLastCursor() { return mLastCursor; }

protected:
	void onCursorChanged(const CursorState& state) override;

private:
	void	 clearEntries();

	int		 moveCursorFast(bool forward = true);

	virtual void onScreenSaverActivate() override;
	virtual void onScreenSaverDeactivate() override;
	virtual void topWindow(bool isTop) override;

	void getCarouselFromTheme(const ThemeData::ThemeElement* elem);

	void renderCarousel(const Transform4x4f& parentTrans);	
	void ensureLogo(IList<CarouselComponentData, FileData*>::Entry& entry);

	// unit is list index
	float mCamOffset;

	bool mDisable;
	bool mScreensaverActive;

	std::function<void(CursorState state)> mCursorChangedCallback;

	std::shared_ptr<ThemeData> mTheme;
	int mLastCursor;	

	enum ImageSource
	{
		TEXT,
		THUMBNAIL,
		IMAGE,
		MARQUEE
	};

	enum CarouselType : unsigned int
	{
		HORIZONTAL = 0,
		VERTICAL = 1,
		VERTICAL_WHEEL = 2,
		HORIZONTAL_WHEEL = 3
	};

	CarouselType mType;
	ImageSource mImageSource;

	float			mLogoScale;
	float			mLogoRotation;
	Vector2f		mLogoRotationOrigin;
	Alignment		mLogoAlignment;
	int				mMaxLogoCount;
	Vector2f		mLogoSize;
	Vector2f		mLogoPos;
	std::string		mDefaultTransition;
	std::string		mScrollSound;
	float			mTransitionSpeed;
	float			mMinLogoOpacity;

public:
	bool isHorizontalCarousel() { return mType == HORIZONTAL || mType == HORIZONTAL_WHEEL; }
};

#endif // ES_APP_VIEWS_SYSTEM_VIEW_H
