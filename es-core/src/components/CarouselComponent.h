#pragma once
#ifndef ES_APP_VIEWS_CCCSYSTEM_VIEW_H
#define ES_APP_VIEWS_CCCSYSTEM_VIEW_H

#include "components/IList.h"
#include "components/TextComponent.h"
#include "resources/Font.h"
#include "GuiComponent.h"
#include <memory>
#include <functional>

class ThemeData;
class IBindable;

struct CarouselComponentData
{
	std::shared_ptr<GuiComponent> logo;
};

enum class CarouselImageSource
{
	TEXT,
	THUMBNAIL,
	IMAGE,
	MARQUEE,
	FANART,
	TITLESHOT,
	BOXART,
	CARTRIDGE,
	BOXBACK,
	MIX
};

enum class CarouselType : unsigned int
{
	HORIZONTAL = 0,
	VERTICAL = 1,
	VERTICAL_WHEEL = 2,
	HORIZONTAL_WHEEL = 3
};


class CarouselItemTemplate : public GuiComponent
{
public:
	CarouselItemTemplate(const std::string& name, Window* window);

	void loadFromString(const std::string& xml, std::map<std::string, std::string>* map = nullptr);
	void loadTemplatedChildren(const ThemeData::ThemeElement* elem);

	bool selectStoryboard(const std::string& name = "") override;
	void deselectStoryboard(bool restoreinitialProperties = true) override;
	void startStoryboard() override;
	bool storyBoardExists(const std::string& name = "", const std::string& propertyName = "");

	void playDefaultActivationStoryboard(bool activation);

	void updateBindings(IBindable* bindable) override;

private:
	std::string mName;
};

class CarouselComponent : public IList<CarouselComponentData, IBindable*>
{
public:
	CarouselComponent(Window* window);
	~CarouselComponent();

	void setThemedContext(const std::string& logoName, const std::string& logoTextName, const std::string& elementName, const std::string& className, CarouselType type, CarouselImageSource source)
	{
		mThemeLogoName = logoName;
		mThemeLogoTextName = logoTextName;
		mThemeElementName = elementName;
		mThemeClass = className;
		mType = type;
		mImageSource = source;
	}

	void setDefaultBackground(unsigned int color, unsigned int colorEnd, bool gradientHorizontal);

	std::string getThemeTypeName() override { return mThemeClass; }

	virtual void onShow() override;
	virtual void onHide() override;

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;

	std::vector<HelpPrompt> getHelpPrompts() override;

	void		add(const std::string& name, IBindable* obj, bool preloadLogo = false);
	IBindable*	getActiveObject();

	inline void setCursorChangedCallback(const std::function<void(CursorState state)>& func) { mCursorChangedCallback = func; }
	void	applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties);

	int getLastCursor() { return mLastCursor; }
	void resetLastCursor() { mLastCursor = -1; }

	virtual bool onMouseClick(int button, bool pressed, int x, int y) override;
	virtual void onMouseMove(int x, int y) override;
	virtual bool onMouseWheel(int delta) override;

	std::string getDefaultTransition() { return mDefaultTransition; }
	float getTransitionSpeed() { return mTransitionSpeed; }

	std::shared_ptr<GuiComponent> getLogo(int index);

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
	void ensureLogo(IList<CarouselComponentData, IBindable*>::Entry& entry);

	// unit is list index
	float mCamOffset;

	bool mDisable;
	bool mScreensaverActive;

	bool mWasRendered;

	std::function<void(CursorState state)> mCursorChangedCallback;

	std::shared_ptr<ThemeData>	mTheme;

	std::string					mThemeViewName;
	std::string					mThemeLogoName;
	std::string					mThemeLogoTextName;
	std::string					mThemeElementName;
	std::string					mThemeClass;

	int mLastCursor;	

	CarouselType			mType;
	CarouselImageSource		mImageSource;

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

	bool			mAnyLogoHasScaleStoryboard;
	bool			mAnyLogoHasOpacityStoryboard;

	float			mScaledSpacing;

	// Mouse support
	int				mPressedCursor;
	Vector2i		mPressedPoint;	

	unsigned int	mColor;
	unsigned int    mColorEnd;
	bool			mColorGradientHorizontal;

public:
	bool isHorizontalCarousel() { return mType == CarouselType::HORIZONTAL || mType == CarouselType::HORIZONTAL_WHEEL; }
};

#endif // ES_APP_VIEWS_SYSTEM_VIEW_H
