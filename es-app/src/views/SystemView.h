#pragma once

#include "GuiComponent.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "components/ScrollableContainer.h"
#include "components/IList.h"
#include "resources/TextureResource.h"

class SystemData;
class AnimatedImageComponent;
class SystemData;

enum CarouselType : unsigned int
{
	HORIZONTAL = 0,
	VERTICAL = 1,
	VERTICAL_WHEEL = 2
};

struct SystemViewData
{
	std::shared_ptr<GuiComponent> logo;
	std::shared_ptr<GuiComponent> logoSelected;
	std::shared_ptr<ThemeExtras> backgroundExtras;
};

struct SystemViewCarousel
{
	CarouselType type;
	Vector2f pos;
	Vector2f size;
	Vector2f origin;
	float logoScale;
	float logoRotation;
	Vector2f logoRotationOrigin;
	Alignment logoAlignment;
	unsigned int color;
	int maxLogoCount; // number of logos shown on the carousel
	Vector2f logoSize;
	float zIndex;
};


class SystemView : public IList<SystemViewData, SystemData*>
{
public:
	SystemView(Window* window);

	virtual void onShow() override;
	virtual void onHide() override;

	void goToSystem(SystemData* system, bool animate);

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;

	void onThemeChanged();

	std::vector<HelpPrompt> getHelpPrompts() override;
	virtual HelpStyle getHelpStyle() override;
	void populate();
	void removeFavoriteSystem();
	void manageFavorite();
	void addSystem(SystemData * it);

protected:
	void onCursorChanged(const CursorState& state) override;

private:
        void renderCarousel(const Transform4x4f& parentTrans);	
        void renderExtras(const Transform4x4f& parentTrans, float lower, float upper);
        inline Vector2f logoSize() const { return Vector2f(mSize.x() * 0.25f, mSize.y() * 0.155f); }

	void renderFade(const Transform4x4f& trans);


	SystemViewCarousel mCarousel;
	TextComponent mSystemInfo;
        
        void getViewElements(const std::shared_ptr<ThemeData>& theme);
        void getDefaultElements(void);
        void getCarouselFromTheme(const ThemeData::ThemeElement* elem);
	// unit is list index
	float mCamOffset;
	float mExtrasCamOffset;
	float mExtrasFadeOpacity;

	bool mViewNeedsReload;
	bool mShowing;
	SystemData * lastSystem;
};
