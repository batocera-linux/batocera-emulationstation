#include "GuiComponent.h"

#include "animations/Animation.h"
#include "animations/AnimationController.h"
#include "renderers/Renderer.h"
#include "Log.h"
#include "ThemeData.h"
#include "Window.h"
#include <algorithm>
#include "animations/LambdaAnimation.h"
#include "anim/StoryboardAnimator.h"
#include "components/ScrollableContainer.h"
#include "math/Vector2i.h"

bool GuiComponent::isLaunchTransitionRunning = false;

GuiComponent::GuiComponent(Window* window) : mWindow(window), mParent(NULL), mOpacity(255),
	mPosition(Vector3f::Zero()), mOrigin(Vector2f::Zero()), mRotationOrigin(0.5, 0.5), mScaleOrigin(0.5f, 0.5f),
	mSize(Vector2f::Zero()), mTransform(Transform4x4f::Identity()), mIsProcessing(false), mVisible(true), mShowing(false),
	mStaticExtra(false), mStoryboardAnimator(nullptr), mScreenOffset(0.0f)
{
	mClipRect = Vector4f();
}

GuiComponent::~GuiComponent()
{
	mWindow->removeGui(this);

	cancelAllAnimations();

	if (mStoryboardAnimator != nullptr)
	{
		delete mStoryboardAnimator;
		mStoryboardAnimator = nullptr;
	}

	if(mParent)
		mParent->removeChild(this);

	for(unsigned int i = 0; i < getChildCount(); i++)
		getChild(i)->setParent(NULL);
}

bool GuiComponent::input(InputConfig* config, Input input)
{	
	for (auto it = mChildren.cbegin(), next_it = it; it != mChildren.cend(); it = next_it)
	{
		++next_it;
		TRYCATCH("GuiComponent::input", if ((*it)->input(config, input)) return true)
	}	

	return false;
}

void GuiComponent::updateSelf(int deltaTime)
{
	if (mAnimationMap.size())
	{
		for (auto it = mAnimationMap.cbegin(), next_it = it; it != mAnimationMap.cend(); it = next_it)
		{
			++next_it;
			advanceAnimation(it->first, deltaTime);
		}
	}

	if (mStoryboardAnimator != nullptr)
		mStoryboardAnimator->update(deltaTime);
}

void GuiComponent::updateChildren(int deltaTime)
{
	for (auto it = mChildren.cbegin(), next_it = it; it != mChildren.cend(); it = next_it)
	{
		++next_it;
		TRYCATCH("GuiComponent::updateChildren", (*it)->update(deltaTime))
	}
}

void GuiComponent::update(int deltaTime)
{
	updateSelf(deltaTime);
	updateChildren(deltaTime);
}

void GuiComponent::render(const Transform4x4f& parentTrans)
{
	if (mChildren.empty() || !mVisible)
		return;

	Transform4x4f trans = parentTrans * getTransform();

	if (!Renderer::isVisibleOnScreen(trans.translation().x(), trans.translation().y(), mSize.x(), mSize.y()))
		return;
	
	if (!mClipRect.empty() && !GuiComponent::isLaunchTransitionRunning)
		Renderer::pushClipRect(Vector2i(mClipRect.x(), mClipRect.y()), Vector2i(mClipRect.z(), mClipRect.w()));

	renderChildren(trans);

	if (!mClipRect.empty() && !GuiComponent::isLaunchTransitionRunning)
		Renderer::popClipRect();
}

void GuiComponent::renderChildren(const Transform4x4f& transform) const
{
	for (auto child : mChildren)
		TRYCATCH("GuiComponent::renderChildren", child->render(transform));
}

Vector3f GuiComponent::getPosition() const
{
	return mPosition;
}

void GuiComponent::setPosition(float x, float y, float z)
{
	mPosition = Vector3f(x, y, z);
	onPositionChanged();
}

Vector2f GuiComponent::getOrigin() const
{
	return mOrigin;
}

void GuiComponent::setOrigin(float x, float y)
{
	mOrigin = Vector2f(x, y);
	onOriginChanged();
}

Vector2f GuiComponent::getRotationOrigin() const
{
	return mRotationOrigin;
}

void GuiComponent::setRotationOrigin(float x, float y)
{
	mRotationOrigin = Vector2f(x, y);
}

Vector2f GuiComponent::getSize() const
{
	return mSize;
}

void GuiComponent::setSize(float w, float h)
{
	mSize = Vector2f(w, h);
    onSizeChanged();
}

float GuiComponent::getRotation() const
{
	return mRotation;
}

void GuiComponent::setRotation(float rotation)
{
	mRotation = rotation;
}

float GuiComponent::getScale() const
{
	return mScale;
}

void GuiComponent::setScale(float scale)
{
	mScale = scale;
}

Vector2f GuiComponent::getScaleOrigin() const
{
	return mScaleOrigin;
}

void GuiComponent::setScaleOrigin(const Vector2f& scaleOrigin)
{
	mScaleOrigin = scaleOrigin;
}

float GuiComponent::getZIndex() const
{
	return mZIndex;
}

void GuiComponent::setZIndex(float z)
{
	mZIndex = z;
}

float GuiComponent::getDefaultZIndex() const
{
	return mDefaultZIndex;
}

void GuiComponent::setDefaultZIndex(float z)
{
	mDefaultZIndex = z;
}

bool GuiComponent::isVisible() const
{
	return mVisible;
}
void GuiComponent::setVisible(bool visible)
{
	mVisible = visible;
}

Vector2f GuiComponent::getCenter() const
{
	return Vector2f(mPosition.x() - (getSize().x() * mOrigin.x()) + getSize().x() / 2,
	                mPosition.y() - (getSize().y() * mOrigin.y()) + getSize().y() / 2);
}

//Children stuff.
void GuiComponent::addChild(GuiComponent* cmp)
{
	mChildren.push_back(cmp);

	if(cmp->getParent())
		cmp->getParent()->removeChild(cmp);

	cmp->setParent(this);
}

void GuiComponent::removeChild(GuiComponent* cmp)
{
	if(!cmp->getParent())
		return;

	if(cmp->getParent() != this)
	{
		LOG(LogError) << "Tried to remove child from incorrect parent!";
	}

	cmp->setParent(NULL);

	for(auto i = mChildren.cbegin(); i != mChildren.cend(); i++)
	{
		if(*i == cmp)
		{
			mChildren.erase(i);
			return;
		}
	}
}

void GuiComponent::clearChildren()
{
	mChildren.clear();
}

void GuiComponent::sortChildren()
{
	std::stable_sort(mChildren.begin(), mChildren.end(),  [](GuiComponent* a, GuiComponent* b) {
		return b->getZIndex() > a->getZIndex();
	});
}

unsigned int GuiComponent::getChildCount() const
{
	return (int)mChildren.size();
}

GuiComponent* GuiComponent::getChild(unsigned int i) const
{
	return mChildren.at(i);
}

void GuiComponent::setParent(GuiComponent* parent)
{
	mParent = parent;
}

GuiComponent* GuiComponent::getParent() const
{
	return mParent;
}

unsigned char GuiComponent::getOpacity() const
{
	return mOpacity;
}

void GuiComponent::setOpacity(unsigned char opacity)
{
	if (mOpacity == opacity)
		return;

	mOpacity = opacity;
	for(auto it = mChildren.cbegin(); it != mChildren.cend(); it++)
	{
		(*it)->setOpacity(opacity);
	}
}

const Transform4x4f& GuiComponent::getTransform()
{
	mTransform = Transform4x4f::Identity();
	mTransform.translate(mPosition);

	if (!mScreenOffset.empty())
		mTransform.translate(Vector3f(mScreenOffset, 0.0f));

	if (mScale != 1.0)
	{
		float xOff = mSize.x() * mScaleOrigin.x();
		float yOff = mSize.y() * mScaleOrigin.y();

		if (mScaleOrigin != Vector2f::Zero())
			mTransform.translate(Vector3f(xOff, yOff, 0.0f));

		mTransform.scale(mScale);
		
		if (mScaleOrigin != Vector2f::Zero())
			mTransform.translate(Vector3f(-xOff, -yOff, 0.0f));
	}

	if (mRotation != 0.0)
	{
		// Calculate offset as difference between origin and rotation origin
		Vector2f rotationSize = getRotationSize();
		float xOff = (mOrigin.x() - mRotationOrigin.x()) * rotationSize.x();
		float yOff = (mOrigin.y() - mRotationOrigin.y()) * rotationSize.y();

		// transform to offset point
		if (xOff != 0.0 || yOff != 0.0)
			mTransform.translate(Vector3f(xOff * -1, yOff * -1, 0.0f));

		// apply rotation transform
		mTransform.rotateZ(mRotation);

		// Tranform back to original point
		if (xOff != 0.0 || yOff != 0.0)
			mTransform.translate(Vector3f(xOff, yOff, 0.0f));
	}

	mTransform.translate(Vector3f(mOrigin.x() * mSize.x() * -1, mOrigin.y() * mSize.y() * -1, 0.0f));


	return mTransform;
}

void GuiComponent::setValue(const std::string& /*value*/)
{
}

std::string GuiComponent::getValue() const
{
	return "";
}

void GuiComponent::textInput(const char* text)
{
	for(auto iter = mChildren.cbegin(); iter != mChildren.cend(); iter++)
		(*iter)->textInput(text);
}

void GuiComponent::setAnimation(Animation* anim, int delay, std::function<void()> finishedCallback, bool reverse, unsigned char slot)
{
	AnimationController* oldAnim = nullptr;

	auto it = mAnimationMap.find(slot);
	if (it != mAnimationMap.cend() && it->second != nullptr)
		oldAnim = it->second;
	
	if (oldAnim)
		delete oldAnim;

	mAnimationMap[slot] = new AnimationController(anim, delay, finishedCallback, reverse);
}

bool GuiComponent::stopAnimation(unsigned char slot)
{
	auto it = mAnimationMap.find(slot);
	if (it != mAnimationMap.cend() && it->second != nullptr)
	{
		auto anim = it->second;
		mAnimationMap.erase(it);
		delete anim;
		return true;
	}

	return false;
}

bool GuiComponent::cancelAnimation(unsigned char slot)
{
	auto it = mAnimationMap.find(slot);
	if (it != mAnimationMap.cend() && it->second != nullptr)
	{
		auto anim = it->second;
		anim->removeFinishedCallback();

		mAnimationMap.erase(it);
		delete anim;

		return true;
	}

	return false;
}

bool GuiComponent::finishAnimation(unsigned char slot)
{
	auto it = mAnimationMap.find(slot);
	if (it != mAnimationMap.cend() && it->second != nullptr)
	{
		const bool done = mAnimationMap[slot]->update(mAnimationMap[slot]->getAnimation()->getDuration() - mAnimationMap[slot]->getTime());

		auto anim = it->second;
		mAnimationMap.erase(it);
		delete anim;
		return true;
	}

	return false;
}

bool GuiComponent::advanceAnimation(unsigned char slot, unsigned int time)
{
	auto it = mAnimationMap.find(slot);
	if (it != mAnimationMap.cend() && it->second != nullptr)
	{
		if (it->second->update(time))
		{
			auto anim = it->second;
			mAnimationMap.erase(it);
			delete anim;
		}

		return true;
	}

	return false;
}

void GuiComponent::stopAllAnimations()
{
	for (auto it = mAnimationMap.cbegin(), next_it = it; it != mAnimationMap.cend(); it = next_it)
	{
		++next_it;
		stopAnimation(it->first);
	}
}

void GuiComponent::cancelAllAnimations()
{
	for (auto it = mAnimationMap.cbegin(), next_it = it; it != mAnimationMap.cend(); it = next_it)
	{
		++next_it;
		cancelAnimation(it->first);
	}
}

bool GuiComponent::isAnimationPlaying(unsigned char slot) const
{
	return mAnimationMap.find(slot) != mAnimationMap.cend();
}

bool GuiComponent::isAnimationReversed(unsigned char slot) const
{
	auto anim = mAnimationMap.find(slot);
	if (anim != mAnimationMap.cend() && anim->second != nullptr)
		return anim->second->isReversed();

	return false;
}

int GuiComponent::getAnimationTime(unsigned char slot) const
{
	auto anim = mAnimationMap.find(slot);
	if (anim != mAnimationMap.cend() && anim->second != nullptr)
		return anim->second->getTime();

	return 0;
}

bool GuiComponent::hasStoryBoard(const std::string& name, bool compareEmptyName) 
{ 
	if (!name.empty() || compareEmptyName)
		return mStoryboardAnimator != nullptr && mStoryboardAnimator->getName() == name;
	
	return mStoryboardAnimator != nullptr; 
}

bool GuiComponent::isStoryBoardRunning(const std::string& name)
{
	if (!name.empty())
		return mStoryboardAnimator != nullptr && mStoryboardAnimator->getName() == name && mStoryboardAnimator->isRunning();

	return mStoryboardAnimator != nullptr && mStoryboardAnimator->isRunning();
}

bool GuiComponent::storyBoardExists(const std::string& name)
{
	return mStoryBoards.size() > 0 && mStoryBoards.find(name) != mStoryBoards.cend();
}

bool GuiComponent::selectStoryboard(const std::string& name)
{
	if (mStoryboardAnimator != nullptr && mStoryboardAnimator->getName() == name)
		return true;

	auto sb = mStoryBoards.find(name);
	if (sb != mStoryBoards.cend())
	{
		if (mStoryboardAnimator != nullptr)
		{
			mStoryboardAnimator->reset();
			delete mStoryboardAnimator;
			mStoryboardAnimator = nullptr;
		}

		mStoryboardAnimator = new StoryboardAnimator(this, sb->second);
		return true;
	}

	return false;
}

bool GuiComponent::applyStoryboard(const ThemeData::ThemeElement* elem, const std::string name)
{
	if (mStoryboardAnimator != nullptr)
	{
		mStoryboardAnimator->reset();
		delete mStoryboardAnimator;
		mStoryboardAnimator = nullptr;
	}

	mStoryBoards = elem->mStoryBoards;
	return selectStoryboard(name);
}

void GuiComponent::stopStoryboard()
{
	if (mStoryboardAnimator)
		mStoryboardAnimator->stop();
}

void GuiComponent::startStoryboard()
{
	if (mStoryboardAnimator)
		mStoryboardAnimator->reset();
}

void GuiComponent::pauseStoryboard() 
{ 
	if (mStoryboardAnimator) 
		mStoryboardAnimator->pause(); 
}

void GuiComponent::deselectStoryboard(bool restoreinitialProperties)
{
	if (mStoryboardAnimator != nullptr)
	{
		if (!restoreinitialProperties)
			mStoryboardAnimator->clearInitialProperties();

		mStoryboardAnimator->stop();
		mStoryboardAnimator->reset();
		delete mStoryboardAnimator;
		mStoryboardAnimator = nullptr;
	}
}

void GuiComponent::enableStoryboardProperty(const std::string& name, bool enable)
{
	if (mStoryboardAnimator != nullptr)
		mStoryboardAnimator->enableProperty(name, enable);
}

void GuiComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	Vector2f screenScale = Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	Vector2f scale = getParent() ? getParent()->getSize() : screenScale;

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "");
	if(!elem)
		return;

	using namespace ThemeFlags;

	if(properties & POSITION && elem->has("pos"))
	{
		Vector2f denormalized = elem->get<Vector2f>("pos") * scale;
		setPosition(Vector3f(denormalized.x(), denormalized.y(), 0));
	}

	if (properties & POSITION && elem->has("x"))
	{
		float denormalized = elem->get<float>("x") * scale.x();
		setPosition(Vector3f(denormalized, mPosition.y(), 0));
	}

	if (properties & POSITION && elem->has("y"))
	{
		float denormalized = elem->get<float>("y") * scale.y();
		setPosition(Vector3f(mPosition.x(), denormalized, 0));
	}

	if(properties & ThemeFlags::SIZE && elem->has("size"))
		setSize(elem->get<Vector2f>("size") * scale);

	if (properties & SIZE && elem->has("w"))
	{
		float denormalized = elem->get<float>("w") * scale.x();
		setSize(Vector2f(denormalized, mSize.y()));
	}

	if (properties & SIZE && elem->has("h"))
	{
		float denormalized = elem->get<float>("h") * scale.y();
		setSize(Vector2f(mSize.x(), denormalized));
	}

	// position + size also implies origin
	if((properties & ORIGIN || (properties & POSITION && properties & ThemeFlags::SIZE)) && elem->has("origin"))
		setOrigin(elem->get<Vector2f>("origin"));

	if(properties & ThemeFlags::ROTATION) 
	{
		if(elem->has("rotation"))
			setRotationDegrees(elem->get<float>("rotation"));
		
		if(elem->has("rotationOrigin"))
			setRotationOrigin(elem->get<Vector2f>("rotationOrigin"));

		if (elem->has("scale"))
			setScale(elem->get<float>("scale"));

		if (elem->has("scaleOrigin"))
			setScaleOrigin(elem->get<Vector2f>("scaleOrigin"));
	}

	if(properties & ThemeFlags::Z_INDEX && elem->has("zIndex"))
		setZIndex(elem->get<float>("zIndex"));
	else
		setZIndex(getDefaultZIndex());

	if(properties & ThemeFlags::VISIBLE && elem->has("visible"))
		setVisible(elem->get<bool>("visible"));
	else
		setVisible(true);

	if (properties & POSITION && elem->has("offset"))
	{
		Vector2f denormalized = elem->get<Vector2f>("offset") * screenScale;
		mScreenOffset = denormalized;
	}

	if (properties & POSITION && elem->has("offsetX"))
	{
		float denormalized = elem->get<float>("offsetX") * screenScale.x();
		mScreenOffset = Vector2f(denormalized, mScreenOffset.y());
	}

	if (properties & POSITION && elem->has("offsetY"))
	{
		float denormalized = elem->get<float>("offsetY") * scale.y();
		mScreenOffset = Vector2f(mScreenOffset.x(), denormalized);
	}

	if (properties & POSITION && elem->has("clipRect"))
	{
		Vector4f val = elem->get<Vector4f>("clipRect") * Vector4f(screenScale.x(), screenScale.y(), screenScale.x(), screenScale.y());
		setClipRect(val);
	}
	else
		setClipRect(Vector4f());

	applyStoryboard(elem);
}

void GuiComponent::updateHelpPrompts()
{
	if(getParent())
	{
		getParent()->updateHelpPrompts();
		return;
	}

	std::vector<HelpPrompt> prompts = getHelpPrompts();

	if(mWindow->peekGui() == this && getTag() != "GuiLoading")
		mWindow->setHelpPrompts(prompts, getHelpStyle());
}

HelpStyle GuiComponent::getHelpStyle()
{
	HelpStyle style = HelpStyle();

	if (ThemeData::getDefaultTheme() != nullptr)
	{
		std::shared_ptr<ThemeData> theme = std::shared_ptr<ThemeData>(ThemeData::getDefaultTheme(), [](ThemeData*) { });
		style.applyTheme(theme, "system");
	}

	return style;
}

bool GuiComponent::isProcessing() const
{
	return mIsProcessing;
}

void GuiComponent::onShow()
{
	mShowing = true;

	if (mStoryboardAnimator != nullptr)
		mStoryboardAnimator->reset();

	for (auto child : mChildren)
		child->onShow();
}

void GuiComponent::onHide()
{
	mShowing = false;

	if (mStoryboardAnimator != nullptr)
		mStoryboardAnimator->pause();

	for (auto child : mChildren)
		child->onHide();
}

void GuiComponent::onScreenSaverActivate()
{
	for (auto child : mChildren)
		child->onScreenSaverActivate();
}

void GuiComponent::onScreenSaverDeactivate()
{
	for (auto child : mChildren)
		child->onScreenSaverDeactivate();
}

void GuiComponent::topWindow(bool isTop)
{
	for (auto child : mChildren)
		child->topWindow(isTop);
}

void GuiComponent::animateTo(Vector2f from, Vector2f to, unsigned int  flags, int delay)
{
	mScaleOrigin = Vector2f::Zero();

	if ((flags & AnimateFlags::POSITION)==0)
		from = to;

	float scale = mScale;
	float opacity = mOpacity;

	float x1 = from.x();
	float x2 = to.x();
	float y1 = from.y();
	float y2 = to.y();

	if (Settings::getInstance()->getString("PowerSaverMode") == "instant" || Settings::getInstance()->getString("TransitionStyle") == "instant")
		setPosition(x2, y2);
	else
	{
		setPosition(x1, y1);

		if ((flags & AnimateFlags::OPACITY) == AnimateFlags::OPACITY)
			setOpacity(0);

		if ((flags & AnimateFlags::SCALE) == AnimateFlags::SCALE)
			mScale = 0.0f;

		auto fadeFunc = [this, x1, x2, y1, y2, flags, scale, opacity](float t) {

			t -= 1; // cubic ease out
			float pct = Math::lerp(0, 1, t*t*t + 1);
			
			if ((flags & AnimateFlags::OPACITY) == AnimateFlags::OPACITY)
				setOpacity(pct * opacity);

			if ((flags & AnimateFlags::SCALE) == AnimateFlags::SCALE)
				mScale = pct * scale;

			float x = (x1 + mSize.x() / 2 - (mSize.x() / 2 * mScale)) * (1 - pct) + (x2 + mSize.x() / 2 - (mSize.x() / 2 * mScale)) * pct;
			float y = (y1 + mSize.y() / 2 - (mSize.y() / 2 * mScale)) * (1 - pct) + (y2 + mSize.y() / 2 - (mSize.y() / 2 * mScale)) * pct;

			if (mScale != 0.0f)
				setPosition(x, y);
		};

		setAnimation(new LambdaAnimation(fadeFunc, delay), 0, [this, fadeFunc, x2, y2, flags, scale, opacity]
		{			
			if ((flags & AnimateFlags::SCALE) == AnimateFlags::SCALE)
				mScale = scale;

			if ((flags & AnimateFlags::OPACITY) == AnimateFlags::OPACITY)
				setOpacity(opacity);

			float x = x2 + mSize.x() / 2 - (mSize.x() / 2 * mScale);
			float y = y2 + mSize.y() / 2 - (mSize.y() / 2 * mScale);
			
			setPosition(x, y);
		});
	}
}

bool GuiComponent::isChild(GuiComponent* cmp)
{
	for (auto i = mChildren.cbegin(); i != mChildren.cend(); i++)
		if (*i == cmp)
			return true;

	return false;
}

ThemeData::ThemeElement::Property GuiComponent::getProperty(const std::string name)
{
	if (getParent() != nullptr && getParent()->isKindOf<ScrollableContainer>())
	{
		if (name == "pos" || name == "x" || name =="y" || name=="size" || name == "w" || name == "h" || name == "offset" || name == "offsetX" || name == "offsetY")
			return getParent()->getProperty(name);
	}

	Vector2f screenScale = Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	Vector2f scale = getParent() ? getParent()->getSize() : screenScale;

	if (name == "pos")
		return Vector2f(mPosition.x(), mPosition.y()) / scale;

	if (name == "x")
		return mPosition.x() / scale.x();

	if (name == "y")
		return mPosition.y() / scale.y();
	
	if (name == "size")
		return mSize / scale;

	if (name == "w")
		return mSize.x() / scale.x();

	if (name == "h")
		return mSize.y() / scale.y();

	if (name == "origin")
		return getOrigin();

	if (name == "rotation")
		return getRotation();

	if (name == "rotationOrigin")
		return getRotationOrigin();

	if (name == "opacity")
		return getOpacity() / 255.0f;

	if (name == "zIndex")
		return getZIndex();

	if (name == "scale")
		return getScale();

	if (name == "scaleOrigin")
		return getScaleOrigin();

	if (name == "offset")
		return mScreenOffset / screenScale;

	if (name == "offsetX")
		return mScreenOffset.x() / screenScale.x();

	if (name == "offsetY")
		return mScreenOffset.y() / screenScale.y();

	if (name == "clipRect")
		return Vector4f(mClipRect.x() / screenScale.x(), mClipRect.y() / screenScale.y(), mClipRect.z() /screenScale.x(), mClipRect.w() / screenScale.y());

	return "";
}

void GuiComponent::setProperty(const std::string name, const ThemeData::ThemeElement::Property& value)
{
	Vector2f screenScale = Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	Vector2f scale = getParent() ? getParent()->getSize() : screenScale;

	if (getParent() != nullptr && getParent()->isKindOf<ScrollableContainer>())
	{
		if (name == "pos" || name == "x" || name == "y" || name == "size" || name == "w" || name == "h" || name == "offset" || name == "offsetX" || name == "offsetY")
		{
			getParent()->setProperty(name, value);
			return;
		}
	}

	if (name == "pos" && value.type == ThemeData::ThemeElement::Property::PropertyType::Pair)
		setPosition(Vector3f(value.v.x() * scale.x(), value.v.y() * scale.y(), 0));

	if (name == "x" && value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
		setPosition(Vector3f(value.f * scale.x(), mPosition.y(), 0));

	if (name == "y" && value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
		setPosition(Vector3f(mPosition.x(), value.f * scale.y(), 0));

	if (name == "size" && value.type == ThemeData::ThemeElement::Property::PropertyType::Pair)
		setSize(Vector2f(value.v.x() * scale.x(), value.v.y() * scale.y()));

	if (name == "w" && value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
		setSize(Vector2f(value.f * scale.x(), mSize.y()));

	if (name == "h" && value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
		setSize(Vector2f(mSize.x(), value.f * scale.y()));

	if (name == "origin" && value.type == ThemeData::ThemeElement::Property::PropertyType::Pair)
		setOrigin(Vector2f(value.v.x(), value.v.y()));

	if (name == "rotation" && value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
		setRotationDegrees(value.f);

	if (name == "rotationOrigin" && value.type == ThemeData::ThemeElement::Property::PropertyType::Pair)
		setRotationOrigin(Vector2f(value.v.x(), value.v.y()));

	if (name == "zIndex" && value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
		setZIndex(value.f);

	if (name == "opacity" && value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
		setOpacity(value.f * 255.0f);

	if (name == "scale" && value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
		setScale(value.f);

	if (name == "scaleOrigin" && value.type == ThemeData::ThemeElement::Property::PropertyType::Pair)
		setScaleOrigin(Vector2f(value.v.x(), value.v.y()));

	if (name == "offset" && value.type == ThemeData::ThemeElement::Property::PropertyType::Pair)
		mScreenOffset = Vector2f(value.v.x() * screenScale.x(), value.v.y() * screenScale.y());
	
	if (name == "offsetX" && value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
		mScreenOffset = Vector2f(value.f * screenScale.x(), mScreenOffset.y());

	if (name == "offsetY" && value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
		mScreenOffset = Vector2f(mScreenOffset.x(), value.f * screenScale.y());

	if (name == "clipRect" && value.type == ThemeData::ThemeElement::Property::PropertyType::Rect)
		setClipRect(Vector4f(value.r.x() * screenScale.x(), value.r.y() * screenScale.y(), value.r.z() * screenScale.x(), value.r.w() * screenScale.y()));
}

void GuiComponent::setClipRect(const Vector4f& vec)
{
	mClipRect = vec;
}

void GuiComponent::beginCustomClipRect()
{
	if (mClipRect.empty() || GuiComponent::isLaunchTransitionRunning)
		return;
	/*
	if (mScale != 1.0)
	{
		mTransform = Transform4x4f::Identity();
		mTransform.translate(mPosition);

		if (!mScreenOffset.empty())
			mTransform.translate(Vector3f(mScreenOffset, 0.0f));

		if (mScale != 1.0)
		{
			float xOff = mSize.x() * mScaleOrigin.x();
			float yOff = mSize.y() * mScaleOrigin.y();

			if (mScaleOrigin != Vector2f::Zero())
				mTransform.translate(Vector3f(xOff, yOff, 0.0f));

			mTransform.scale(mScale);

			if (mScaleOrigin != Vector2f::Zero())
				mTransform.translate(Vector3f(-xOff, -yOff, 0.0f));
		}

		mTransform.translate(Vector3f(mOrigin.x() * mSize.x() * -1, mOrigin.y() * mSize.y() * -1, 0.0f));
	}*/

	Renderer::pushClipRect(Vector2i(mClipRect.x(), mClipRect.y()), Vector2i(mClipRect.z(), mClipRect.w()));
}

void GuiComponent::endCustomClipRect()
{
	if (!mClipRect.empty() && !GuiComponent::isLaunchTransitionRunning)
		Renderer::popClipRect();
}