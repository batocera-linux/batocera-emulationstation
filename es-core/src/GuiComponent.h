#pragma once
#ifndef ES_CORE_GUI_COMPONENT_H
#define ES_CORE_GUI_COMPONENT_H

#include "math/Misc.h"
#include "math/Transform4x4f.h"
#include "HelpPrompt.h"
#include "HelpStyle.h"
#include "InputConfig.h"
#include <functional>
#include "ThemeData.h"
#include <memory>
#include "anim/ThemeStoryboard.h"

class Animation;
class AnimationController;
class Font;
class InputConfig;
class ThemeData;
class Window;
class StoryboardAnimator;
class IBindable;

namespace AnimateFlags
{
	enum Flags : unsigned int
	{
		POSITION = 1,
		SCALE = 2,
		OPACITY = 4,
		ALL = 0xFFFFFFFF
	};
}

enum class ExtraType : unsigned int
{
	BUILTIN = 0,
	EXTRA = 1,
	STATIC = 2,
	EXTRACHILDREN = 3
};

#define GetComponentScreenRect(tx, sz) Renderer::getScreenRect(tx, sz)

class GuiComponent
{
	friend class BindingManager;

public:
	GuiComponent(Window* window);
	virtual ~GuiComponent();

	template<typename T>
	bool isKindOf() { return (dynamic_cast<T*>(this) != nullptr); }

	virtual std::string getThemeTypeName() { return "container"; }

	virtual void	textInput(const char* text);
	virtual bool	input(InputConfig* config, Input input);
	virtual void	update(int deltaTime);
	virtual void	render(const Transform4x4f& parentTrans);

	Vector3f		getPosition() const;
	void			setPosition(float x, float y, float z = 0.0f);
	inline void		setPosition(const Vector3f& offset) { setPosition(offset.x(), offset.y(), offset.z()); }
	
	Vector2f		getOrigin() const; // origin as a percentage of this image (e.g. (0, 0) is top left, (0.5, 0.5) is the center)
	void			setOrigin(float originX, float originY);
	inline void		setOrigin(Vector2f origin) { setOrigin(origin.x(), origin.y()); }
	
	Vector2f		getRotationOrigin() const; // rotation origin as a percentage of this image (e.g. (0, 0) is top left, (0.5, 0.5) is the center)
	void			setRotationOrigin(float originX, float originY);
	inline void		setRotationOrigin(Vector2f origin) { setRotationOrigin(origin.x(), origin.y()); }

	virtual Vector2f getSize() const;
	virtual void	setSize(const Vector2f& size) { setSize(size.x(), size.y()); }
	virtual void	setSize(float w, float h);

	float			getRotation() const;
	void			setRotation(float rotation);
	inline void		setRotationDegrees(float rotation) { setRotation((float)ES_DEG_TO_RAD(rotation)); }
	float			getRotationDegrees() const;

	virtual Vector2f getRotationSize() const { return getSize(); };

	float			getScale() const;
	virtual void	setScale(float scale);

	Vector2f		getScaleOrigin() const;
	void			setScaleOrigin(const Vector2f& scaleOrigin);

	Vector2f		getScreenOffset() const;
	void			setScreenOffset(const Vector2f& screenOffset);

	float			getZIndex() const;
	void			setZIndex(float zIndex);

	float			getDefaultZIndex() const;
	void			setDefaultZIndex(float zIndex);

	bool			isVisible() const;
	void			setVisible(bool visible);

	std::string		getTag() const { return mTag; };
	void			setTag(const std::string& value) { mTag = value; };

	virtual unsigned char getOpacity() const;
	virtual void	setOpacity(unsigned char opacity);

	virtual std::string getValue() const;
	virtual void	setValue(const std::string& value);

	virtual Vector4f getPadding() { return mPadding; }
	virtual void setPadding(const Vector4f padding);

	virtual void	setColor(unsigned int color) {};

	Vector2f		getCenter() const;  // Returns the center point of the image (takes origin into account).

	virtual const Transform4x4f& getTransform();

	virtual void	topWindow(bool isTop);

	ExtraType		getExtraType() { return mExtraType; }
	bool			isStaticExtra() const { return mExtraType == ExtraType::STATIC; }
	void			setExtraType(ExtraType value) { mExtraType = value; }

	void			setParent(GuiComponent* parent);
	GuiComponent*	getParent() const;

	void			addChild(GuiComponent* cmp);
	void			removeChild(GuiComponent* cmp);
	void			clearChildren();
	void			sortChildren();
	unsigned int	getChildCount() const;
	GuiComponent*	getChild(unsigned int i) const;
	bool			isChild(GuiComponent* cmp);

	// Theming
	virtual void	applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties);

	// Help
	virtual std::vector<HelpPrompt> getHelpPrompts() { return std::vector<HelpPrompt>(); };
	void updateHelpPrompts();
	virtual HelpStyle getHelpStyle();


	virtual ThemeData::ThemeElement::Property getProperty(const std::string name);
	virtual void	setProperty(const std::string name, const ThemeData::ThemeElement::Property& value);

	bool&			isShowing() { return mShowing; }

	// AnimateTo methods
	void			animateTo(Vector2f from, Vector2f to, unsigned int flags = 0xFFFFFFFF, int delay = 350);
	void			animateTo(Vector2f from, unsigned int flags = AnimateFlags::OPACITY | AnimateFlags::SCALE, int delay = 350) { animateTo(from, from, flags, delay); }

	// hard-coded animations
	bool			isAnimationPlaying(unsigned char slot) const;
	bool			isAnimationReversed(unsigned char slot) const;
	int				getAnimationTime(unsigned char slot) const;
	void			setAnimation(Animation* animation, int delay = 0, std::function<void()> finishedCallback = nullptr, bool reverse = false, unsigned char slot = 0);
	bool			stopAnimation(unsigned char slot);
	bool			cancelAnimation(unsigned char slot); // Like stopAnimation, but doesn't call finishedCallback - only removes the animation, leaving things in their current state.  Returns true if successful (an animation was in this slot).
	bool			finishAnimation(unsigned char slot); // Calls update(1.f) and finishedCallback, then deletes the animation - basically skips to the end.  Returns true if successful (an animation was in this slot).
	bool			advanceAnimation(unsigned char slot, unsigned int time); // Returns true if successful (an animation was in this slot).
	void			stopAllAnimations();
	void			cancelAllAnimations();

	// Storyboards
	bool			hasStoryBoard(const std::string& name = "", bool compareEmptyName = false);
	bool			applyStoryboard(const ThemeData::ThemeElement* elem, const std::string name = "");
	virtual bool	selectStoryboard(const std::string& name = "");
	virtual void	deselectStoryboard(bool restoreinitialProperties = true);
	virtual void	startStoryboard();
	void			pauseStoryboard();
	void			stopStoryboard();
	void			enableStoryboardProperty(const std::string& name, bool enable);
	bool			currentStoryBoardHasProperty(const std::string& propertyName);
	virtual bool	storyBoardExists(const std::string& name = "", const std::string& propertyName = "");
	bool			isStoryBoardRunning(const std::string& name = "");

	// Clipping
	Vector4f&		getClipRect() { return mClipRect; }
	virtual void	setClipRect(const Vector4f& vec);

	// Mouse
	bool			isMouseOver() { return mIsMouseOver; }
	virtual bool	hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult = nullptr);	
	void			setClickAction(const std::string& action) { mClickAction = action; }

	// Bindings
	std::map<std::string, std::string> getBindingExpressions() { return mBindingExpressions; }

	// Events
	virtual void	onPositionChanged();
	virtual void	onOriginChanged();
	virtual void	onRotationOriginChanged();
	virtual void	onSizeChanged();
	virtual void	onRotationChanged();
	virtual void	onScaleChanged();
	virtual void	onScaleOriginChanged();
	virtual void	onScreenOffsetChanged();
	virtual void	onOpacityChanged() { };
	virtual void	onFocusGained() { };
	virtual void	onFocusLost() { };
	virtual void	onShow();
	virtual void	onHide();
	virtual void	onScreenSaverActivate();
	virtual void	onScreenSaverDeactivate();
	virtual void	onMouseLeave();
	virtual void	onMouseEnter();
	virtual void	onPaddingChanged();
	virtual void	onMouseMove(int x, int y);
	virtual bool	onMouseWheel(int delta);
	virtual bool	onMouseClick(int button, bool pressed, int x, int y);
	virtual bool	onAction(const std::string& action);

	virtual void	updateBindings(IBindable* bindable);
	std::vector<GuiComponent*> enumerateExtraChildrens();

	Vector4f		getClientRect();

	void			setAmbientOpacity(unsigned char opacity);

	std::map<std::string, ThemeStoryboard*>& getStoryBoards() { return mStoryBoards; };

protected:
	void			beginCustomClipRect();
	void			endCustomClipRect();

	void			renderChildren(const Transform4x4f& transform) const;
	void			updateSelf(int deltaTime); // updates animations
	void			updateChildren(int deltaTime); // updates animations

	void			loadThemedChildren(const ThemeData::ThemeElement* elem);

	virtual void	recalcChildrenLayout();
	virtual void	recalcLayout();

	Window* mWindow;

	GuiComponent* mParent;
	std::vector<GuiComponent*> mChildren;

	Vector3f mPosition;
	Vector2f mOrigin;
	Vector2f mRotationOrigin;
	Vector2f mSize;
	Vector2f mScaleOrigin;
	Vector2f mScreenOffset;

	Vector4f mSourceBounds;

	std::string mStoryBoardSound;

	float mRotation = 0.0;
	float mScale = 1.0;
	float mDefaultZIndex = 0;
	float mZIndex = 0;

	bool mShowing;
	bool mVisible;
	bool mTransformDirty;
	bool mChildZIndexDirty;
	bool mIsMouseOver;

	bool mClipChildren;

	unsigned char mOpacity;

	Vector4f	mPadding;

	std::map<std::string, std::string> mBindingExpressions;
	ExtraType mExtraType;

public:
	const static unsigned char MAX_ANIMATIONS = 4;
	static bool isLaunchTransitionRunning;

private:
	std::string		mTag;
	std::string		mClickAction;
	bool			mMousePressed;
	

	unsigned char	mAmbientOpacity; // Don't access this directly! Use setAmbientOpacity()!

	Transform4x4f   mTransform; // Don't access this directly! Use getTransform()!
	Vector4f		mClipRect;

	std::map<unsigned char, AnimationController*> mAnimationMap;

	StoryboardAnimator* mStoryboardAnimator;
	std::map<std::string, ThemeStoryboard*> mStoryBoards;
};

#endif // ES_CORE_GUI_COMPONENT_H
