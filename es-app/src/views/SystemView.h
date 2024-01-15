#pragma once
#ifndef ES_APP_VIEWS_SYSTEM_VIEW_H
#define ES_APP_VIEWS_SYSTEM_VIEW_H

#include "components/IList.h"
#include "components/TextComponent.h"
#include "resources/Font.h"
#include "GuiComponent.h"
#include "MultiStateInput.h"

#include "components/CarouselComponent.h"
#include "components/TextListComponent.h"
#include "components/ImageGridComponent.h"

#include <memory>
#include <functional>
#include <map>
#include <unordered_set>

#include "SystemViewControlWrapper.h"

class AnimatedImageComponent;
class SystemData;
class VideoVlcComponent;

struct SystemViewData
{
	SystemData* object;
	std::vector<GuiComponent*> backgroundExtras;
};


class SystemView : public GuiComponent
{
public:
	SystemView(Window* window);
	~SystemView();

	void goToSystem(SystemData* system, bool animate);

	void onThemeChanged(const std::shared_ptr<ThemeData>& theme);
	void reloadTheme(SystemData* system);

	SystemData* getActiveSystem();

	// GuiComponent overrides
	virtual void onShow() override;
	virtual void onHide() override;
	virtual void onScreenSaverActivate() override;
	virtual void onScreenSaverDeactivate() override;
	virtual void topWindow(bool isTop) override;

	virtual bool input(InputConfig* config, Input input) override;
	virtual void update(int deltaTime) override;
	virtual void render(const Transform4x4f& parentTrans) override;

	// Help
	std::vector<HelpPrompt> getHelpPrompts() override;
	virtual HelpStyle getHelpStyle() override;

	// Mouse support
	virtual bool hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult = nullptr) override;
	virtual void onMouseMove(int x, int y) override;
	virtual bool onMouseWheel(int delta) override;
	virtual bool onMouseClick(int button, bool pressed, int x, int y) override;

	virtual bool onAction(const std::string& action) override;

	int getCursorIndex();
	std::vector<SystemData*> getObjects();

protected:
	//virtual void onCursorChanged(const CursorState& state) override;
	void onCursorChanged(const CursorState& state);
	SystemData* getSelected();

private:
	void	 loadExtras(SystemData* system);
	void	 ensureTexture(GuiComponent* extra, bool reload);

	void	 updateExtraTextBinding();
	void	 showQuickSearch();

	void	 preloadExtraNeighbours(int cursor);
	void	 setExtraRequired(SystemViewData& data, bool required);

	void	 activateExtras(int cursor, bool activate = true);	
	void	 updateExtras(const std::function<void(GuiComponent*)>& func);
	void	 clearEntries();

	int		 moveCursorFast(bool forward = true);

	void	 showNetplay();
	bool	 showNavigationBar();
	void	 showNavigationBar(const std::string& title, const std::function<std::string(SystemData* system)>& selector);

	void	 populate();
	void	 getViewElements(const std::shared_ptr<ThemeData>& theme);
	void	 getDefaultElements(void);
	void	 getCarouselFromTheme(const ThemeData::ThemeElement* elem);

	void	 renderCarousel(const Transform4x4f& parentTrans);
	void	 renderExtras(const Transform4x4f& parentTrans, float lower, float upper);
	void	 renderInfoBar(const Transform4x4f& trans);
	
	ControlWrapper						mCarousel;

	TextComponent						mSystemInfo;

	std::vector<GuiComponent*>			mStaticBackgrounds;
	std::vector<SystemViewData>			mEntries;

	float			mCamOffset;
	float			mExtrasCamOffset;
	float			mExtrasFadeOpacity;
	float			mExtrasFadeMove;
	int				mExtrasFadeOldCursor;

	bool			mViewNeedsReload;		
	bool			mDisable;
	bool			mScreensaverActive;

	MultiStateInput mYButton;

	int				mLastCursor;

	// Mouse support
	int				mPressedCursor;
	Vector2i		mPressedPoint;
	bool			mLockCamOffsetChanges;
	bool			mLockExtraChanges;


	float			mSystemInfoDelay;
	bool			mSystemInfoCountOnly;

	std::string		mExtraTransitionType;
	float			mExtraTransitionSpeed;
	bool			mExtraTransitionHorizontal;
};

#endif // ES_APP_VIEWS_SYSTEM_VIEW_H
