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
#include "Sound.h"
#include "utils/StringUtil.h"
#include "BindingManager.h"

bool GuiComponent::isLaunchTransitionRunning = false;

GuiComponent::GuiComponent(Window* window) : mWindow(window), mParent(NULL), mOpacity(255), mAmbientOpacity(255),
	mPosition(Vector3f::Zero()), mOrigin(Vector2f::Zero()), mRotationOrigin(0.5, 0.5), mScaleOrigin(0.5f, 0.5f), mSourceBounds(Vector4f::Zero()),
	mSize(Vector2f::Zero()), mTransform(Transform4x4f::Identity()), mVisible(true), mShowing(false), mPadding(Vector4f(0, 0, 0, 0)), mClipChildren(false),
	mExtraType(ExtraType::BUILTIN), mStoryboardAnimator(nullptr), mScreenOffset(0.0f), mTransformDirty(true), mIsMouseOver(false), mMousePressed(false), mChildZIndexDirty(false)
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

	if (mParent)
		mParent->removeChild(this);

	for (int i = (int)getChildCount() - 1; i >= 0; i--)
	{
		auto child = getChild(i);

		if (child->mExtraType == ExtraType::EXTRACHILDREN)
			delete child;
		else
			child->setParent(NULL);
	}
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
	if (mChildZIndexDirty)
	{
		mChildZIndexDirty = false;
		sortChildren();
	}

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

	auto rect = Renderer::getScreenRect(trans, mSize);

	// Don't use soft clip if rotation applied : let renderer do the work
	if (mRotation == 0 && trans.r0().y() == 0 && !Renderer::isVisibleOnScreen(rect))	
		return;

	if (mClipChildren)
		Renderer::pushClipRect(rect);
	else if (!mClipRect.empty() && !GuiComponent::isLaunchTransitionRunning)
		Renderer::pushClipRect(mClipRect.x(), mClipRect.y(), mClipRect.z(), mClipRect.w());
	
	renderChildren(trans);

	if (mClipChildren)
		Renderer::popClipRect();
	else if (!mClipRect.empty() && !GuiComponent::isLaunchTransitionRunning)
		Renderer::popClipRect();
}

void GuiComponent::renderChildren(const Transform4x4f& transform) const
{
	for (auto child : mChildren)
		if (child->mVisible)
			TRYCATCH("GuiComponent::renderChildren", child->render(transform));
}

Vector3f GuiComponent::getPosition() const
{
	return mPosition;
}

void GuiComponent::setPosition(float x, float y, float z)
{
	auto position = Vector3f(x, y, z);
	if (position == mPosition)
		return;
	
	mPosition = position;
	onPositionChanged();	
}

Vector2f GuiComponent::getOrigin() const
{
	return mOrigin;
}

void GuiComponent::setOrigin(float x, float y)
{
	auto origin = Vector2f(x, y);
	if (origin == mOrigin)
		return;

	mOrigin = origin;
	onOriginChanged();
}

Vector2f GuiComponent::getRotationOrigin() const
{
	return mRotationOrigin;
}

void GuiComponent::setRotationOrigin(float x, float y)
{
	auto origin = Vector2f(x, y);
	if (origin == mRotationOrigin)
		return;

	mRotationOrigin = origin;
	onRotationOriginChanged();
}

Vector2f GuiComponent::getSize() const
{
	return mSize;
}

void GuiComponent::setSize(float w, float h)
{
	auto oldClientSize = getClientRect();
	auto size = Vector2f(w, h);
	//if (size == mSize)
	//	return;

	mSize = size;
    onSizeChanged();

	auto clientSize = getClientRect();

	if (mChildren.size() && oldClientSize != clientSize)
		recalcChildrenLayout();
}

void GuiComponent::setPadding(const Vector4f padding)
{
	if (mPadding == padding)
		return;

	auto oldClientSize = getClientRect();

	mPadding = padding;
	onPaddingChanged();

	auto clientSize = getClientRect();

	if (mChildren.size() && oldClientSize != clientSize)
		recalcChildrenLayout();
}

float GuiComponent::getRotation() const
{
	return mRotation;
}

float GuiComponent::getRotationDegrees() const
{
	return ES_RAD_TO_DEG(mRotation);
}

void GuiComponent::setRotation(float rotation)
{
	if (rotation == mRotation)
		return;

	mRotation = rotation;
	onRotationChanged();
}

float GuiComponent::getScale() const
{
	return mScale;
}

void GuiComponent::setScale(float scale)
{
	if (scale == mScale)
		return;

	mScale = scale;
	onScaleChanged();
}

Vector2f GuiComponent::getScaleOrigin() const
{
	return mScaleOrigin;
}

void GuiComponent::setScaleOrigin(const Vector2f& scaleOrigin)
{
	if (scaleOrigin == mScaleOrigin)
		return;

	mScaleOrigin = scaleOrigin;
	onScaleOriginChanged();
}

Vector2f GuiComponent::getScreenOffset() const
{
	return mScreenOffset;
}

void GuiComponent::setScreenOffset(const Vector2f& screenOffset)
{
	if (screenOffset == mScreenOffset)
		return;

	mScreenOffset = screenOffset;
	onScreenOffsetChanged();
}

float GuiComponent::getZIndex() const
{
	return mZIndex;
}

void GuiComponent::setZIndex(float z)
{
	if (mZIndex == z)
		return;

	mZIndex = z;

	if (mParent != nullptr)
		mParent->mChildZIndexDirty = true;
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
	cmp->mShowing = mShowing;
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

	if (mParent != nullptr)
	{
		mShowing = mParent->mShowing;
		
		setAmbientOpacity(mParent->getOpacity());
	}
}

GuiComponent* GuiComponent::getParent() const
{
	return mParent;
}

unsigned char GuiComponent::getOpacity() const
{
	if (mAmbientOpacity == 255)
		return mOpacity;

	return (unsigned char) (((int)mOpacity * (int)mAmbientOpacity) / 255);
}

void GuiComponent::setOpacity(unsigned char opacity)
{
	if (mOpacity == opacity)
		return;

	mOpacity = opacity;
	onOpacityChanged();

	auto ambientOpacity = getOpacity();
	for (auto it : mChildren)
		it->setAmbientOpacity(ambientOpacity);
}

void GuiComponent::setAmbientOpacity(unsigned char opacity)
{
	if (mAmbientOpacity == opacity)
		return;

	mAmbientOpacity = opacity;
	onOpacityChanged();

	auto ambientOpacity = getOpacity();
	for (auto it : mChildren)
		it->setAmbientOpacity(ambientOpacity);
}

const Transform4x4f& GuiComponent::getTransform()
{
	if (!mTransformDirty)
		return mTransform;

	mTransformDirty = false;
	mTransform = Transform4x4f::Identity();
	mTransform.translate(mPosition);

	if (!mScreenOffset.empty())
		mTransform.translate(mScreenOffset.x(), mScreenOffset.y());

	if (mScale != 1.0)
	{
		float xOff = mSize.x() * mScaleOrigin.x();
		float yOff = mSize.y() * mScaleOrigin.y();

		if (mScaleOrigin != Vector2f::Zero())
			mTransform.translate(xOff, yOff);

		mTransform.scale(mScale);
		
		if (mScaleOrigin != Vector2f::Zero())
			mTransform.translate(-xOff, -yOff);
	}

	if (mRotation != 0.0)
	{
		// Calculate offset as difference between origin and rotation origin
		Vector2f rotationSize = getRotationSize();
		float xOff = (mOrigin.x() - mRotationOrigin.x()) * rotationSize.x();
		float yOff = (mOrigin.y() - mRotationOrigin.y()) * rotationSize.y();

		// transform to offset point
		if (xOff != 0.0 || yOff != 0.0)
			mTransform.translate(xOff * -1, yOff * -1);

		// apply rotation transform
		mTransform.rotateZ(mRotation);

		// Tranform back to original point
		if (xOff != 0.0 || yOff != 0.0)
			mTransform.translate(xOff, yOff);
	}

	mTransform.translate(mOrigin.x() * mSize.x() * -1, mOrigin.y() * mSize.y() * -1);	
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

bool GuiComponent::currentStoryBoardHasProperty(const std::string& propertyName)
{
	if (mStoryboardAnimator == nullptr)
		return false;
	
	return storyBoardExists(mStoryboardAnimator->getName(), propertyName);
}

bool GuiComponent::isStoryBoardRunning(const std::string& name)
{
	if (!name.empty())
		return mStoryboardAnimator != nullptr && mStoryboardAnimator->getName() == name && mStoryboardAnimator->isRunning();

	return mStoryboardAnimator != nullptr && mStoryboardAnimator->isRunning();
}

bool GuiComponent::storyBoardExists(const std::string& name, const std::string& propertyName)
{
	if (mStoryBoards.size() > 0)
	{
		auto it = mStoryBoards.find(name);
		if (it == mStoryBoards.cend())
			return false;

		if (propertyName.empty())
			return true;
		
		for (auto animation : it->second->animations)
			if (animation->propertyName == propertyName)
				return true;
	}

	return false;
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

void GuiComponent::loadThemedChildren(const ThemeData::ThemeElement* elem)
{
	if (mChildren.size())
	{
		for (int i = (int)mChildren.size() - 1; i >= 0; i--)
		{
			auto child = getChild(i);
			if (child->mExtraType == ExtraType::EXTRACHILDREN)
				delete child;
		}
	}

	if (elem->children.size() == 0)
		return;

	for (const std::pair<std::string, ThemeData::ThemeElement>& child : elem->children)
	{
		if (child.second.type == "shader")
			continue;

		auto comp = ThemeData::createExtraComponent(mWindow, child.second, false);
		if (comp != nullptr)
		{
			comp->setTag(child.first);
			comp->setExtraType(ExtraType::EXTRACHILDREN);

			std::pair<std::string, ThemeData::ThemeElement>* item = (std::pair<std::string, ThemeData::ThemeElement>*) &child;

			// Default pos & size properties
			if (child.second.properties.find("pos") == child.second.properties.cend() &&
				child.second.properties.find("x") == child.second.properties.cend() &&
				child.second.properties.find("y") == child.second.properties.cend())
				item->second.properties["pos"] = Vector2f(0, 0);

			if (child.second.properties.find("size") == child.second.properties.cend() &&
				child.second.properties.find("minSize") == child.second.properties.cend() &&
				child.second.properties.find("maxSize") == child.second.properties.cend() &&
				child.second.properties.find("w") == child.second.properties.cend() &&
				child.second.properties.find("w") == child.second.properties.cend() &&
				child.second.properties.find("h") == child.second.properties.cend())
				item->second.properties["size"] = Vector2f(1, 1);

			addChild(comp);
			ThemeData::applySelfTheme(comp, child.second);			
		}
	}

	sortChildren();	
}

void GuiComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	const ThemeData::ThemeElement* elem = theme->getElement(view, element, getThemeTypeName()); // getThemeTypeName()
	if (!elem)
		return;

	Vector2f screenScale = Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	Vector4f clientRectangle = getParent() ? getParent()->getClientRect() : Vector4f(0, 0, (float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	Vector2f scale = Vector2f(clientRectangle.z(), clientRectangle.w());
	Vector2f offset = Vector2f(clientRectangle.x(), clientRectangle.y());

	using namespace ThemeFlags;

	if (properties & POSITION && elem->has("pos"))
	{		
		auto pos = mSourceBounds.xy() = elem->get<Vector2f>("pos");

		Vector2f denormalized = pos * scale + offset;
		setPosition(Vector3f(denormalized.x(), denormalized.y(), 0));
	}

	if (properties & POSITION && elem->has("x"))
	{
		auto x = mSourceBounds.x() = elem->get<float>("x");
		setPosition(Vector3f(x * scale.x() + offset.x(), mPosition.y(), 0));
	}

	if (properties & POSITION && elem->has("y"))
	{
		auto y = mSourceBounds.y() = elem->get<float>("y");
		setPosition(Vector3f(mPosition.x(), y * scale.y() + offset.y(), 0));
	}

	if (properties & ThemeFlags::SIZE && elem->has("size"))
	{
		auto sz = mSourceBounds.zw() = elem->get<Vector2f>("size");
		setSize(sz * scale);
	}

	if (properties & SIZE && elem->has("w"))
	{
		auto w = mSourceBounds.z() = elem->get<float>("w");
		setSize(Vector2f(w * scale.x(), mSize.y()));
	}

	if (properties & SIZE && elem->has("h"))
	{
		auto h = mSourceBounds.w() = elem->get<float>("h");
		setSize(Vector2f(mSize.x(), h * scale.y()));
	}
	
	if (elem->has("padding"))
	{
		auto padding = elem->get<Vector4f>("padding");
		if (abs(padding.x()) < 1 && abs(padding.y()) < 1 && abs(padding.z()) < 1 && abs(padding.w()) < 1)
			setPadding(padding * Vector4f(scale.x(), scale.y(), scale.x(), scale.y()));
		else
			setPadding(padding); // Pixel size
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

	if (properties & ThemeFlags::VISIBLE)
		setVisible(!elem->has("visible") || elem->get<bool>("visible"));

	if (elem->has("opacity"))
		setOpacity((unsigned char)(elem->get<float>("opacity") * 255.0));

	if (properties & POSITION && elem->has("offset"))
	{
		Vector2f denormalized = elem->get<Vector2f>("offset") * screenScale;
		setScreenOffset(denormalized);
	}

	if (properties & POSITION && elem->has("offsetX"))
	{
		float denormalized = elem->get<float>("offsetX") * screenScale.x();
		setScreenOffset(Vector2f(denormalized, mScreenOffset.y()));
	}

	if (properties & POSITION && elem->has("offsetY"))
	{
		float denormalized = elem->get<float>("offsetY") * scale.y();
		setScreenOffset(Vector2f(mScreenOffset.x(), denormalized));
	}

	if (properties & POSITION && elem->has("clipRect"))
	{
		Vector4f val = elem->get<Vector4f>("clipRect") * Vector4f(screenScale.x(), screenScale.y(), screenScale.x(), screenScale.y());
		setClipRect(val);
	}
	else
		setClipRect(Vector4f());

	if (elem->has("clipChildren"))
		mClipChildren = elem->get<bool>("clipChildren");

	if (elem->has("onclick"))
		setClickAction(elem->get<std::string>("onclick"));
	else
		setClickAction("");

	for (auto prop : elem->properties)
		if (prop.second.type == ThemeData::ThemeElement::Property::PropertyType::String && Utils::String::endsWith(prop.first, "_binding"))
			mBindingExpressions[Utils::String::replace(prop.first, "_binding", "")] = prop.second.s;

	applyStoryboard(elem);
	loadThemedChildren(elem);

	mTransformDirty = true;
}

void GuiComponent::updateHelpPrompts()
{
	if(getParent())
	{
		getParent()->updateHelpPrompts();
		return;
	}

	if (mWindow->peekGui() == this && getTag() != "GuiLoading")
	{
		std::vector<HelpPrompt> prompts = getHelpPrompts();
		mWindow->setHelpPrompts(prompts, getHelpStyle());
	}
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
	setScaleOrigin(Vector2f::Zero());

	if ((flags & AnimateFlags::POSITION)==0)
		from = to;

	float scale = mScale;
	float opacity = mOpacity;

	float x1 = from.x();
	float x2 = to.x();
	float y1 = from.y();
	float y2 = to.y();

	if (Settings::PowerSaverMode() == "instant" || Settings::TransitionStyle() == "instant")
		setPosition(x2, y2);
	else
	{
		setPosition(x1, y1);

		if ((flags & AnimateFlags::OPACITY) == AnimateFlags::OPACITY)
			setOpacity(0);

		if ((flags & AnimateFlags::SCALE) == AnimateFlags::SCALE)
			setScale(0.0f);

		auto fadeFunc = [this, x1, x2, y1, y2, flags, scale, opacity](float t) {

			t -= 1; // cubic ease out
			float pct = Math::lerp(0, 1, t*t*t + 1);
			
			if ((flags & AnimateFlags::OPACITY) == AnimateFlags::OPACITY)
				setOpacity(pct * opacity);

			if ((flags & AnimateFlags::SCALE) == AnimateFlags::SCALE)
				setScale(pct * scale);

			float x = (x1 + mSize.x() / 2 - (mSize.x() / 2 * mScale)) * (1 - pct) + (x2 + mSize.x() / 2 - (mSize.x() / 2 * mScale)) * pct;
			float y = (y1 + mSize.y() / 2 - (mSize.y() / 2 * mScale)) * (1 - pct) + (y2 + mSize.y() / 2 - (mSize.y() / 2 * mScale)) * pct;

			if (mScale != 0.0f)
				setPosition(x, y);
		};

		setAnimation(new LambdaAnimation(fadeFunc, delay), 5, [this, fadeFunc, x2, y2, flags, scale, opacity]
		{			
			if ((flags & AnimateFlags::SCALE) == AnimateFlags::SCALE)
				setScale(scale);

			if ((flags & AnimateFlags::OPACITY) == AnimateFlags::OPACITY)
				setOpacity(opacity);

			float x = x2 + mSize.x() / 2 - (mSize.x() / 2 * mScale);
			float y = y2 + mSize.y() / 2 - (mSize.y() / 2 * mScale);
			
			setPosition(x, y);

			mTransformDirty = true;
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
	Vector4f clientRectangle = getParent() ? getParent()->getClientRect() : Vector4f(0, 0, (float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	Vector2f scale = Vector2f(clientRectangle.z(), clientRectangle.w());
	// Vector2f offset = Vector2f(clientRectangle.x(), clientRectangle.y());

	if (name == "pos")
		return Vector2f(mPosition.x() - clientRectangle.x(), mPosition.y() - clientRectangle.y()) / scale;

	if (name == "x")
		return (mPosition.x() - clientRectangle.x()) / scale.x();

	if (name == "y")
		return (mPosition.y() - clientRectangle.y()) / scale.y();
	
	if (name == "size")
		return mSize / scale;

	if (name == "w")
		return mSize.x() / scale.x();

	if (name == "h")
		return mSize.y() / scale.y();

	if (name == "origin")
		return getOrigin();

	if (name == "rotation")
		return getRotationDegrees();

	if (name == "rotationOrigin")
		return getRotationOrigin();

	if (name == "opacity")
		return mOpacity / 255.0f;

	if (name == "zIndex")
		return getZIndex();

	if (name == "scale")
		return getScale();

	if (name == "padding")
		return mPadding;

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

	if (name == "sound")
		return mStoryBoardSound;

	if (name == "visible")
		return mVisible;

	ThemeData::ThemeElement::Property unk;
	unk.type = ThemeData::ThemeElement::Property::Unknown;
	return unk;
}

void GuiComponent::setProperty(const std::string name, const ThemeData::ThemeElement::Property& value)
{
	Vector2f screenScale = Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	Vector4f clientRectangle = getParent() ? getParent()->getClientRect() : Vector4f(0, 0, (float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	Vector2f scale = Vector2f(clientRectangle.z(), clientRectangle.w());
	Vector2f offset = Vector2f(clientRectangle.x(), clientRectangle.y());

	if (getParent() != nullptr && getParent()->isKindOf<ScrollableContainer>())
	{
		if (name == "pos" || name == "x" || name == "y" || name == "size" || name == "w" || name == "h" || name == "offset" || name == "offsetX" || name == "offsetY")
		{
			getParent()->setProperty(name, value);
			return;
		}
	}

	switch (value.type)
	{
	case ThemeData::ThemeElement::Property::PropertyType::Pair:

		if (name == "pos")
		{
			mSourceBounds.xy() = value.v;
			setPosition(Vector3f(offset.x() + value.v.x() * scale.x(), offset.y() + value.v.y() * scale.y(), 0));
		}
		else if (name == "size")
		{
			mSourceBounds.zw() = value.v;
			setSize(Vector2f(value.v.x() * scale.x(), value.v.y() * scale.y()));
		}
		else if (name == "origin")
			setOrigin(Vector2f(value.v.x(), value.v.y()));
		else if (name == "rotationOrigin")
			setRotationOrigin(Vector2f(value.v.x(), value.v.y()));
		else if (name == "scaleOrigin")
			setScaleOrigin(Vector2f(value.v.x(), value.v.y()));
		else if (name == "offset")
			setScreenOffset(Vector2f(value.v.x() * screenScale.x(), value.v.y() * screenScale.y()));

		break;

	case ThemeData::ThemeElement::Property::PropertyType::Float:

		if (name == "x")
		{
			mSourceBounds.x() = value.f;
			setPosition(Vector3f(offset.x() + value.f * scale.x(), mPosition.y(), 0));
		}
		else if (name == "y")
		{
			mSourceBounds.y() = value.f;
			setPosition(Vector3f(mPosition.x(), offset.y() + value.f * scale.y(), 0));
		}
		else if (name == "w")
		{
			mSourceBounds.z() = value.f;
			setSize(Vector2f(value.f * scale.x(), mSize.y()));
		}
		else if (name == "h")
		{
			mSourceBounds.w() = value.f;
			setSize(Vector2f(mSize.x(), value.f * scale.y()));
		}
		else if (name == "rotation")
			setRotationDegrees(value.f);
		else if (name == "zIndex")
			setZIndex(value.f);
		else if (name == "opacity")
			setOpacity(value.f * 255.0f);
		else if (name == "scale")
			setScale(value.f);
		else if (name == "offsetX")
			setScreenOffset(Vector2f(value.f * screenScale.x(), mScreenOffset.y()));
		else if (name == "offsetY")
			setScreenOffset(Vector2f(mScreenOffset.x(), value.f * screenScale.y()));

		break;

	case ThemeData::ThemeElement::Property::PropertyType::Rect:
		if (name == "clipRect")
			setClipRect(Vector4f(value.r.x() * screenScale.x(), value.r.y() * screenScale.y(), value.r.z() * screenScale.x(), value.r.w() * screenScale.y()));
		else if (name == "padding")
			setPadding(value.r);

		break;

	case ThemeData::ThemeElement::Property::PropertyType::Bool:
		if (name == "visible")
			setVisible(value.b);

		break;

	case ThemeData::ThemeElement::Property::PropertyType::String:
		if (name == "sound" && mStoryBoardSound != value.s)
		{
			mStoryBoardSound = value.s;

			if (!mStoryBoardSound.empty())
				Sound::get(mStoryBoardSound)->play();
		}

		break;
	}
	
	mTransformDirty = true;
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

	Renderer::pushClipRect(mClipRect.x(), mClipRect.y(), mClipRect.z(), mClipRect.w());
}

void GuiComponent::endCustomClipRect()
{
	if (!mClipRect.empty() && !GuiComponent::isLaunchTransitionRunning)
		Renderer::popClipRect();
}

void GuiComponent::onPositionChanged() 
{
	mTransformDirty = true;
}

void GuiComponent::onOriginChanged()
{
	mTransformDirty = true;
}

void GuiComponent::onRotationOriginChanged()
{
	mTransformDirty = true;
}

void GuiComponent::onRotationChanged()
{
	mTransformDirty = true;
}

void GuiComponent::onScaleChanged()
{
	mTransformDirty = true;
}

void GuiComponent::onScaleOriginChanged()
{
	mTransformDirty = true;
}

void GuiComponent::onScreenOffsetChanged()
{
	mTransformDirty = true;
}

void GuiComponent::onSizeChanged()
{
	mTransformDirty = true;
}

void GuiComponent::onPaddingChanged()
{
	mTransformDirty = true;
}

bool GuiComponent::hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult)
{	
	if (!isVisible())
	{
		if (mIsMouseOver)
		{
			onMouseLeave();
			mIsMouseOver = false;
		}

		return false;
	}

	bool ret = false;

	Transform4x4f trans = getTransform() * parentTransform;

	auto rect = Renderer::getScreenRect(trans, getSize(), true);
	if (x != -1 && y != -1 && rect.contains(x, y))
	{
		if (pResult)
			pResult->push_back(this);

		if (!mIsMouseOver)
		{
			mIsMouseOver = true;
			onMouseEnter();
		}

		onMouseMove(x, y);		
		ret = true;
	}
	else if (mIsMouseOver)
	{
		onMouseLeave();
		mIsMouseOver = false;
	}

	for (int i = 0; i < getChildCount(); i++)
		ret |= getChild(i)->hitTest(x, y, trans, pResult);

	return ret;
}


void GuiComponent::onMouseLeave() 
{

}

void GuiComponent::onMouseEnter() 
{

}

void GuiComponent::onMouseMove(int x, int y) 
{

}

bool GuiComponent::onMouseWheel(int delta)
{
	return false;
}

bool GuiComponent::onAction(const std::string& action)
{
	if (action == "none")
		return true;

	if (getParent() != nullptr)
		return getParent()->onAction(action);

	return false;
}

bool GuiComponent::onMouseClick(int button, bool pressed, int x, int y)
{
	if (button == 1 && !mClickAction.empty())
	{
		if (pressed)
		{
			mMousePressed = true;	
			mWindow->setMouseCapture(this);
		}
		else if (mMousePressed)
		{
			mWindow->releaseMouseCapture();
			mMousePressed = false;

			if (mIsMouseOver)
				onAction(mClickAction);			
		}

		return true;
	}

	return false;
}

std::vector<GuiComponent*> GuiComponent::enumerateExtraChildrens()
{
	std::vector<GuiComponent*> recursiveExtraChildrens;

	std::stack<GuiComponent*> stack;
	stack.push(this);

	while (stack.size())
	{
		GuiComponent* current = stack.top();
		stack.pop();

		for (auto it : current->mChildren)
		{
			if (it->mExtraType != ExtraType::EXTRACHILDREN || !it->isVisible())
				continue;

			stack.push(it);
			recursiveExtraChildrens.push_back(it);
		}
	}

	return recursiveExtraChildrens;
}

/// <summary>
/// Sort GuiComponents by binding expressions inter-dependancies
/// </summary>
static void visit(const std::vector<GuiComponent*>& items, const std::string& itemId, std::vector<GuiComponent*>& sortedItems, std::unordered_map<GuiComponent*, bool>& visited)
{
	auto itemIt = std::find_if(items.begin(), items.end(), [&](GuiComponent* item) { return !visited[item] && item->getTag() == itemId; });
	if (itemIt != items.end())
	{
		GuiComponent* comp = *itemIt;
		visited[comp] = true;

		for (auto expression : comp->getBindingExpressions())
		{
			for (auto name : Utils::String::extractStrings(expression.second, "{", ":"))
			{
				if (name == "system" || name == "game" || name == "collection" || name == "grid")
					continue;

				auto referenceIt = std::find_if(items.begin(), items.end(), [&](GuiComponent* item) { return item->getTag() == name; });
				if (referenceIt != items.end())
				{
					visit(items, name, sortedItems, visited);
					break;
				}
			}
		}

		sortedItems.push_back(comp);
	}
}


void GuiComponent::updateBindings(IBindable* bindable)
{
	auto recursiveExtraChildrens = enumerateExtraChildrens();

	// Sort items by inter-dependency -> The ones that references another one are last
	std::vector<GuiComponent*> sortedItems;
	std::unordered_map<GuiComponent*, bool> visited;

	for (auto child : recursiveExtraChildrens)
		visit(recursiveExtraChildrens, child->getTag(), sortedItems, visited);

	bool hasStackPanel = false;

	for (auto child : sortedItems) // recursiveExtraChildrens
	{
		hasStackPanel |= child->getThemeTypeName() == "stackpanel";
		BindingManager::updateBindings(child, bindable, false);
	}

	if (hasStackPanel)
	{
		for (auto child : recursiveExtraChildrens)
			if (child->getThemeTypeName() == "stackpanel")
				child->onSizeChanged();
	}
}

Vector4f GuiComponent::getClientRect()
{
	return Vector4f(
		mPadding.x(),
		mPadding.y(),
		getSize().x() - mPadding.x() - mPadding.z(),
		getSize().y() - mPadding.y() - mPadding.w());
}

void GuiComponent::recalcChildrenLayout()
{
	for (auto child : mChildren)
		child->recalcLayout();
}

void GuiComponent::recalcLayout()
{
	if (mExtraType != ExtraType::EXTRACHILDREN || !getParent())
		return;

	Vector4f clientRectangle = getParent()->getClientRect();
	Vector2f newPos = mSourceBounds.xy() * clientRectangle.zw() + clientRectangle.xy();
	Vector2f newSize = mSourceBounds.zw() * clientRectangle.zw();
	
	if (mPosition.v2() != newPos)
		setPosition(newPos.x(), newPos.y());

	if (mSize != newSize)
		setSize(newSize.x(), newSize.y());
}
