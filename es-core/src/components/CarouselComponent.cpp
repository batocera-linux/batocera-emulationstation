#include "CarouselComponent.h"
#include "Settings.h"
#include "Log.h"
#include "animations/LambdaAnimation.h"
#include "Sound.h"
#include "LocaleES.h"
#include "InputManager.h"
#include "Window.h"
#include "Log.h"
#include "BindingManager.h"

// buffer values for scrolling velocity (left, stopped, right)
const int logoBuffersLeft[] = { -5, -2, -1 };
const int logoBuffersRight[] = { 1, 2, 5 };

CarouselComponent::CarouselComponent(Window* window) :
	IList<CarouselComponentData, IBindable*>(window, LIST_SCROLL_STYLE_SLOW, LIST_ALWAYS_LOOP)
{
	mColor = mColorEnd = 0;
	mColorGradientHorizontal = false;

	mThemeViewName = "gamecarousel";
	mThemeLogoName = "gamecarouselLogo";
	mThemeLogoTextName = "gamecarouselLogoText";
	mThemeElementName = "gamecarousel";
	mThemeClass = "gamecarousel";

	// setThemedContext("logo", "logoText", "systemcarousel", "carousel") // For a future system view !?

	mWasRendered = false;
	mCamOffset = 0;
	mScreensaverActive = false;
	mDisable = false;		
	mLastCursor = 0;
		
	mPressedPoint = Vector2i(-1, -1);
	mPressedCursor = -1;
	
	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	mType = CarouselType::VERTICAL;
	mLogoAlignment = ALIGN_CENTER;
	mLogoScale = 1.2f;
	mLogoRotation = 7.5;
	mLogoRotationOrigin.x() = -5;
	mLogoRotationOrigin.y() = 0.5;
	mLogoSize.x() = 0.25f * mSize.x();
	mLogoSize.y() = 0.155f * mSize.y();
	mLogoPos = Vector2f(-1, -1);
	mMaxLogoCount = 3;	
	mScrollSound = "";
	mDefaultTransition = "";
	mTransitionSpeed = 500;
	mMinLogoOpacity = 0.5f;
	mScaledSpacing = 0.0f;	
	mImageSource = CarouselImageSource::THUMBNAIL;

	mAnyLogoHasScaleStoryboard = false;
	mAnyLogoHasOpacityStoryboard = false;
}

CarouselComponent::~CarouselComponent()
{
	clearEntries();
}

void CarouselComponent::clearEntries()
{
	mWasRendered = false;
	mEntries.clear();
}

int CarouselComponent::moveCursorFast(bool forward)
{
	int value = mCursor;
	int count = forward ? 10 : -10;
	int sz = mEntries.size();

	if (count >= sz)
		return value;

	value += count;
	if (value < 0) 
		value += sz;
	if (value >= sz) 
		value -= sz;

	return value;
}

bool CarouselComponent::input(InputConfig* config, Input input)
{
	if(input.value != 0)
	{	
		switch (mType)
		{
		case CarouselType::VERTICAL:
		case CarouselType::VERTICAL_WHEEL:
			if (config->isMappedLike("up", input) || config->isMappedLike("l2", input))
			{
				listInput(-1);
				return true;
			}
			if (config->isMappedLike("down", input) || config->isMappedLike("r2", input))
			{
				listInput(1);
				return true;
			}
			if (config->isMappedTo("pagedown", input))
			{
				int cursor = moveCursorFast(true);
				listInput(cursor - mCursor);				
				return true;
			}
			if (config->isMappedTo("pageup", input))
			{
				int cursor = moveCursorFast(false);
				listInput(cursor - mCursor);
				return true;
			}

			break;
		case CarouselType::HORIZONTAL:
		case CarouselType::HORIZONTAL_WHEEL:
		default:
			if (config->isMappedLike("left", input) || config->isMappedLike("l2", input))
			{
				listInput(-1);
				return true;
			}
			if (config->isMappedLike("right", input) || config->isMappedLike("r2", input))
			{
				listInput(1);
				return true;
			}
			if (config->isMappedTo("pagedown", input))
			{
				int cursor = moveCursorFast(true);
				listInput(cursor - mCursor);
				return true;
			}
			if (config->isMappedTo("pageup", input))
			{
				int cursor = moveCursorFast(false);
				listInput(cursor - mCursor);
				return true;
			}

			break;
		}		
	}
	else
	{
		if(config->isMappedLike("left", input) ||
			config->isMappedLike("right", input) ||
			config->isMappedLike("up", input) ||
			config->isMappedLike("down", input) ||
			config->isMappedLike("pagedown", input) ||
			config->isMappedLike("pageup", input) ||
			config->isMappedLike("l2", input) ||
			config->isMappedLike("r2", input))
			listInput(0);
	}

	return GuiComponent::input(config, input);
}

void CarouselComponent::update(int deltaTime)
{		
	for (int i = 0; i < mEntries.size(); i++)
	{
		const std::shared_ptr<GuiComponent> &comp = mEntries.at(i).data.logo;
		if (comp != nullptr)
			comp->update(deltaTime);
	}
	
	listUpdate(deltaTime);	

	GuiComponent::update(deltaTime);
}

void CarouselComponent::onCursorChanged(const CursorState& state)
{
	// update help style
	updateHelpPrompts();

	float startPos = mCamOffset;

	float posMax = (float)mEntries.size();
	float target = (float)mCursor;

	// what's the shortest way to get to our target?
	// it's one of these...

	float endPos = target; // directly
	float dist = abs(endPos - startPos);

	if(abs(target + posMax - startPos) < dist)
		endPos = target + posMax; // loop around the end (0 -> max)
	if(abs(target - posMax - startPos) < dist)
		endPos = target - posMax; // loop around the start (max - 1 -> -1)

	cancelAnimation(1);
	cancelAnimation(2);

	std::string transition_style = Settings::TransitionStyle();
	if (transition_style == "auto")
	{
		if (mDefaultTransition == "instant" || mDefaultTransition == "fade" || mDefaultTransition == "slide" || mDefaultTransition == "fade & slide")
			transition_style = mDefaultTransition;
		else
			transition_style = "slide";
	}

	// no need to animate transition, we're not going anywhere (probably mEntries.size() == 1)
	//if(endPos == mCamOffset)
		//return;

	if (mLastCursor == mCursor)
		return;

	if (!mScrollSound.empty())
		Sound::get(mScrollSound)->play();

	int oldCursor = mLastCursor;
	
	bool oldCursorHasStoryboard = false;

	if (oldCursor >= 0 && oldCursor < mEntries.size())
	{
		auto logo = mEntries.at(oldCursor).data.logo;
		if (logo)
		{
			if (logo->selectStoryboard("deactivate"))
			{
				logo->startStoryboard();
				oldCursorHasStoryboard = true;
			}
			else
				logo->deselectStoryboard();
		}
	}

	bool cursorHasStoryboard = false;

	if (mCursor >= 0 && mCursor < mEntries.size())
	{
		auto logo = mEntries.at(mCursor).data.logo;
		if (logo)
		{
			if (logo->selectStoryboard("activate"))
			{
				logo->startStoryboard();
				cursorHasStoryboard = true;
			}
			else
				logo->deselectStoryboard();
		}
	}

	for (int i = 0; i < mEntries.size(); i++)
	{
		if ((cursorHasStoryboard && i == mCursor) || (oldCursorHasStoryboard && i == oldCursor))
			continue;

		auto logo = mEntries.at(i).data.logo;
		if (logo && logo->selectStoryboard("scroll"))
			logo->startStoryboard();
	}

	Animation* anim;
	bool move_carousel = Settings::getInstance()->getBool("MoveCarousel");
	if (Settings::PowerSaverMode() == "instant")
		move_carousel = false;

	if (transition_style == "fade" || transition_style == "fade & slide")
	{
		anim = new LambdaAnimation([this, startPos, endPos, posMax, move_carousel, oldCursor, transition_style](float t)
		{
			float f = Math::lerp(startPos, endPos, Math::easeOutQuint(t));

			if (f < 0) f += posMax;
			if (f >= posMax) f -= posMax;

			this->mCamOffset = move_carousel ? f : endPos;
		}, mTransitionSpeed);
	} 
	else if (transition_style == "slide") 
	{
		anim = new LambdaAnimation([this, startPos, endPos, posMax, move_carousel](float t)
		{			
			float f = Math::lerp(startPos, endPos, Math::easeOutQuint(t));
			if (f < 0) f += posMax;
			if (f >= posMax) f -= posMax;

			this->mCamOffset = move_carousel ? f : endPos;			

		}, mTransitionSpeed);
	} 
	else // instant
	{		
		anim = new LambdaAnimation([this, startPos, endPos, posMax, move_carousel ](float t)
		{
			float f = Math::lerp(startPos, endPos, Math::easeOutQuint(t));
			if (f < 0) f += posMax; 
			if (f >= posMax) f -= posMax;

			this->mCamOffset = move_carousel ? f : endPos;			

		}, move_carousel ? mTransitionSpeed : 1);
	}

	if (mCursorChangedCallback)
		mCursorChangedCallback(state);

	mLastCursor = mCursor;

	auto curState = state;
	setAnimation(anim, 0, [this, curState]
	{
		if (curState == CURSOR_SCROLLING && mCursorChangedCallback)
			mCursorChangedCallback(curState);
	}, false, 0);
}

void CarouselComponent::render(const Transform4x4f& parentTrans)
{
	if (size() == 0 || !mVisible)
		return;  // nothing to render

	Transform4x4f trans = getTransform() * parentTrans;

	auto rect = Renderer::getScreenRect(trans, mSize);
	if (!Renderer::isVisibleOnScreen(rect))
		return;

	Renderer::pushClipRect(rect);
	renderCarousel(trans);
	Renderer::popClipRect();

	if (!mWasRendered && mShowing)
	{
		mWasRendered = true;
		onShow();
	}
}

std::vector<HelpPrompt> CarouselComponent::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;

	if (mType == CarouselType::VERTICAL || mType == CarouselType::VERTICAL_WHEEL)
		prompts.push_back(HelpPrompt("up/down", _("CHOOSE")));
	else
		prompts.push_back(HelpPrompt("left/right", _("CHOOSE")));

	return prompts;
}

void CarouselComponent::setDefaultBackground(unsigned int color, unsigned int colorEnd, bool gradientHorizontal)
{
	mColor = color;
	mColorEnd = colorEnd;
	mColorGradientHorizontal = gradientHorizontal;
}

//  Render system carousel
void CarouselComponent::renderCarousel(const Transform4x4f& trans)
{
	Transform4x4f carouselTrans = trans;
	
	Renderer::setMatrix(carouselTrans);

	if (mColor != 0 || mColorEnd != 0)
		Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), mColor, mColorEnd, mColorGradientHorizontal);

	// draw logos
	Vector2f logoSpacing(0.0, 0.0); // NB: logoSpacing will include the size of the logo itself as well!
	float xOff = 0.0;
	float yOff = 0.0;

	switch (mType)
	{
		case CarouselType::VERTICAL_WHEEL:
			yOff = (mSize.y() - mLogoSize.y()) / 2.f - (mCamOffset * logoSpacing[1]);
			if (mLogoAlignment == ALIGN_LEFT)
				xOff = mLogoSize.x() / 10.f;
			else if (mLogoAlignment == ALIGN_RIGHT)
				xOff = mSize.x() - (mLogoSize.x() * 1.1f);
			else
				xOff = (mSize.x() - mLogoSize.x()) / 2.f;
			break;
		case CarouselType::VERTICAL:
			logoSpacing[1] = ((mSize.y() - (mLogoSize.y() * mMaxLogoCount)) / (mMaxLogoCount)) + mLogoSize.y();
			yOff = (mSize.y() - mLogoSize.y()) / 2.f - (mCamOffset * logoSpacing[1]);

			if (mLogoAlignment == ALIGN_LEFT)
				xOff = mLogoSize.x() / 10.f;
			else if (mLogoAlignment == ALIGN_RIGHT)
				xOff = mSize.x() - (mLogoSize.x() * 1.1f);
			else
				xOff = (mSize.x() - mLogoSize.x()) / 2;
			break;
		case CarouselType::HORIZONTAL_WHEEL:
			xOff = (mSize.x() - mLogoSize.x()) / 2 - (mCamOffset * logoSpacing[1]);
			if (mLogoAlignment == ALIGN_TOP)
				yOff = mLogoSize.y() / 10;
			else if (mLogoAlignment == ALIGN_BOTTOM)
				yOff = mSize.y() - (mLogoSize.y() * 1.1f);
			else
				yOff = (mSize.y() - mLogoSize.y()) / 2;
			break;
		case CarouselType::HORIZONTAL:
		default:
			logoSpacing[0] = ((mSize.x() - (mLogoSize.x() * mMaxLogoCount)) / (mMaxLogoCount)) + mLogoSize.x();
			xOff = (mSize.x() - mLogoSize.x()) / 2.f - (mCamOffset * logoSpacing[0]);

			if (mLogoAlignment == ALIGN_TOP)
				yOff = mLogoSize.y() / 10.f;
			else if (mLogoAlignment == ALIGN_BOTTOM)
				yOff = mSize.y() - (mLogoSize.y() * 1.1f);
			else
				yOff = (mSize.y() - mLogoSize.y()) / 2.f;
			break;
	}

	if (mLogoPos.x() >= 0)
		xOff = mLogoPos.x() - (mType == CarouselType::HORIZONTAL ? (mCamOffset * logoSpacing[0]) : 0);

	if (mLogoPos.y() >= 0)
		yOff = mLogoPos.y() - (mType == CarouselType::VERTICAL ? (mCamOffset * logoSpacing[1]) : 0);

	int center = (int)(mCamOffset);
	int logoCount = Math::min(mMaxLogoCount, (int)mEntries.size());

	// Adding texture loading buffers depending on scrolling speed and status
	int bufferIndex = Math::max(0, Math::min(2, getScrollingVelocity() + 1));
	int bufferLeft = logoBuffersLeft[bufferIndex];
	int bufferRight = logoBuffersRight[bufferIndex];

	if (logoCount == 1 && mCamOffset == 0)
	{
		bufferLeft = 0;
		bufferRight = 0;
	}

	auto renderLogo = [this, carouselTrans, logoSpacing, xOff, yOff](int i)
	{
		int index = i % (int)mEntries.size();
		if (index < 0)
			index += (int)mEntries.size();

		Transform4x4f logoTrans = carouselTrans;

		if (mType == CarouselType::HORIZONTAL && mLogoScale != 1.0f && mScaledSpacing != 0.0f)
		{
			auto logoDiffX = ((logoSpacing[0] * mLogoScale) - logoSpacing[0]) / 2.0f * mScaledSpacing;

			if (index == mCursor)
				logoTrans.translate(Vector3f(i * logoSpacing[0] + xOff, i * logoSpacing[1] + yOff, 0));
			else if (i < mCursor || (mCursor == 0 && index > mMaxLogoCount))
				logoTrans.translate(Vector3f(i * logoSpacing[0] + xOff - logoDiffX, i * logoSpacing[1] + yOff, 0));
			else
				logoTrans.translate(Vector3f(i * logoSpacing[0] + xOff + logoDiffX, i * logoSpacing[1] + yOff, 0));
		}
		else
			logoTrans.translate(Vector3f(i * logoSpacing[0] + xOff, i * logoSpacing[1] + yOff, 0));

		float distance = i - mCamOffset;

		float scale = 1.0f + ((mLogoScale - 1.0f) * (1.0f - fabs(distance)));
		scale = Math::min(mLogoScale, Math::max(1.0f, scale));
		scale /= mLogoScale;

		int opref = (Math::clamp(mMinLogoOpacity, 0, 1) * 255);
		
		int opacity = (int)Math::round(opref + ((0xFF - opref) * (1.0f - fabs(distance))));
		opacity = Math::max((int)opref, opacity);

		ensureLogo(mEntries.at(index));

		const std::shared_ptr<GuiComponent> &comp = mEntries.at(index).data.logo;
		if (mType == CarouselType::VERTICAL_WHEEL || mType == CarouselType::HORIZONTAL_WHEEL)
		{
			comp->setRotationDegrees(mLogoRotation * distance);
			comp->setRotationOrigin(mLogoRotationOrigin);
		}
		
		if (!mAnyLogoHasOpacityStoryboard)
			comp->setOpacity((unsigned char)opacity);

		if (!mAnyLogoHasScaleStoryboard)
			comp->setScale(scale);
		
		comp->render(logoTrans);
	};


	std::vector<int> activePositions;
	for (int i = center - logoCount / 2 + bufferLeft; i <= center + logoCount / 2 + bufferRight; i++)
	{
		int index = i % (int)mEntries.size();
		if (index < 0)
			index += (int)mEntries.size();
	
		if (index == mCursor)
			activePositions.push_back(i);
		else
			renderLogo(i);
	}
	
	for (auto activePos : activePositions)
		renderLogo(activePos);
}

void CarouselComponent::getCarouselFromTheme(const ThemeData::ThemeElement* elem)
{
	Vector2f size = mThemeViewName == "gamecarousel" ? 
		mSize :
		Vector2f(Renderer::getScreenWidth(), Renderer::getScreenHeight());

	if (elem->has("type"))
	{
		if (!(elem->get<std::string>("type").compare("vertical")))
			mType = CarouselType::VERTICAL;
		else if (!(elem->get<std::string>("type").compare("vertical_wheel")))
			mType = CarouselType::VERTICAL_WHEEL;
		else if (!(elem->get<std::string>("type").compare("horizontal_wheel")))
			mType = CarouselType::HORIZONTAL_WHEEL;
		else
			mType = CarouselType::HORIZONTAL;
	}
	if (elem->has("logoScale"))
		mLogoScale = elem->get<float>("logoScale");
	if (elem->has("logoSize"))
		mLogoSize = elem->get<Vector2f>("logoSize") * size;
	if (elem->has("logoPos"))
		mLogoPos = elem->get<Vector2f>("logoPos") * size;
	if (elem->has("maxLogoCount"))
		mMaxLogoCount = (int)Math::round(elem->get<float>("maxLogoCount"));
	if (elem->has("logoRotation"))
		mLogoRotation = elem->get<float>("logoRotation");
	if (elem->has("logoRotationOrigin"))
		mLogoRotationOrigin = elem->get<Vector2f>("logoRotationOrigin");
	if (elem->has("logoAlignment"))
	{
		if (!(elem->get<std::string>("logoAlignment").compare("left")))
			mLogoAlignment = ALIGN_LEFT;
		else if (!(elem->get<std::string>("logoAlignment").compare("right")))
			mLogoAlignment = ALIGN_RIGHT;
		else if (!(elem->get<std::string>("logoAlignment").compare("top")))
			mLogoAlignment = ALIGN_TOP;
		else if (!(elem->get<std::string>("logoAlignment").compare("bottom")))
			mLogoAlignment = ALIGN_BOTTOM;
		else
			mLogoAlignment = ALIGN_CENTER;
	}

	if (elem->has("color"))
		mColor = mColorEnd = elem->get<unsigned int>("color");
	if (elem->has("colorEnd"))
		mColorEnd = elem->get<unsigned int>("colorEnd");
	if (elem->has("gradientType"))
		mColorGradientHorizontal = elem->get<std::string>("gradientType").compare("horizontal");

	if (elem->has("scrollSound"))
		mScrollSound = elem->get<std::string>("scrollSound");

	if (elem->has("defaultTransition"))
		mDefaultTransition = elem->get<std::string>("defaultTransition");

	if (elem->has("transitionSpeed"))
		mTransitionSpeed = elem->get<float>("transitionSpeed");

	if (elem->has("minLogoOpacity"))
		mMinLogoOpacity = elem->get<float>("minLogoOpacity");
	
	if (elem->has("scaledLogoSpacing"))
		mScaledSpacing = elem->get<float>("scaledLogoSpacing");	

	if (elem->has("imageSource"))
	{
		auto direction = elem->get<std::string>("imageSource");
		if (direction == "text")
			mImageSource = CarouselImageSource::TEXT;
		else if (direction == "image")
			mImageSource = CarouselImageSource::IMAGE;
		else if (direction == "marquee")
			mImageSource = CarouselImageSource::MARQUEE;
		else if (direction == "fanart")
			mImageSource = CarouselImageSource::FANART;
		else if (direction == "titleshot")
			mImageSource = CarouselImageSource::TITLESHOT;
		else if (direction == "boxart")
			mImageSource = CarouselImageSource::BOXART;
		else if (direction == "cartridge")
			mImageSource = CarouselImageSource::CARTRIDGE;
		else if (direction == "boxback")
			mImageSource = CarouselImageSource::BOXBACK;
		else if (direction == "mix")
			mImageSource = CarouselImageSource::MIX;
		else
			mImageSource = CarouselImageSource::THUMBNAIL;
	}
}

void CarouselComponent::onShow()
{
	GuiComponent::onShow();		

	if (!mWasRendered)
	{
		// If it's not rendered yet, every logo is null. Do defer storyboarding to the 1st render()
		return;
	}

	bool cursorStoryboardSet = false;

	for (int i = 0; i < mEntries.size(); i++)
	{
		auto logo = mEntries.at(i).data.logo;
		if (logo)
			logo->isShowing() = true;
	}

	if (mCursor >= 0 && mCursor < mEntries.size())
	{
		auto logo = mEntries.at(mCursor).data.logo;
		if (logo)
		{
			if (logo->selectStoryboard("activate"))
			{
				logo->startStoryboard();
				cursorStoryboardSet = true;
			}
			else if (logo->selectStoryboard())
			{
				logo->startStoryboard();
				cursorStoryboardSet = true;
			}
			else
				logo->deselectStoryboard();
		}
	}

	for (int i = 0; i < mEntries.size(); i++)
	{
		if (cursorStoryboardSet && mCursor == i)
			continue;

		auto logo = mEntries.at(i).data.logo;
		if (logo && (logo->selectStoryboard("scroll") || logo->selectStoryboard()))
			logo->startStoryboard();
	}
}

void CarouselComponent::onHide()
{
	GuiComponent::onHide();	

	for (int i = 0; i < mEntries.size(); i++)
	{
		auto logo = mEntries.at(i).data.logo;
		if (logo)
			logo->isShowing() = false;
	}
}

void CarouselComponent::onScreenSaverActivate()
{
	mScreensaverActive = true;
}

void CarouselComponent::onScreenSaverDeactivate()
{
	mScreensaverActive = false;
}

void CarouselComponent::topWindow(bool isTop)
{
	mDisable = !isTop;
}

IBindable* CarouselComponent::getActiveObject()
{
	if (mCursor < 0 || mCursor >= mEntries.size())
		return nullptr;

	return mEntries[mCursor].object;
}

void CarouselComponent::ensureLogo(IList<CarouselComponentData, IBindable*>::Entry& entry)
{
	if (entry.data.logo != nullptr)
		return;

	// itemTemplate
	const ThemeData::ThemeElement* carouselElem = mTheme->getElement(mThemeViewName, mThemeElementName, mThemeClass);
	if (carouselElem)
	{
		auto itemTemplate = std::find_if(carouselElem->children.cbegin(), carouselElem->children.cend(), [](const std::pair<std::string, ThemeData::ThemeElement> ss) { return ss.first == "itemTemplate"; });
		if (itemTemplate != carouselElem->children.cend())
		{
			CarouselItemTemplate* templ = new CarouselItemTemplate(entry.name, mWindow);
			templ->setScaleOrigin(0.0f);
			templ->setSize(mLogoSize * mLogoScale);
			templ->loadTemplatedChildren(&itemTemplate->second);

			entry.data.logo = std::shared_ptr<GuiComponent>(templ);
		}
	}

	if (!entry.data.logo)
	{
		std::string mediaName = "marquee";

		switch (mImageSource)
		{
		case CarouselImageSource::IMAGE:     mediaName = "image";     break;
		case CarouselImageSource::THUMBNAIL: mediaName = "thumbnail"; break;
		case CarouselImageSource::TITLESHOT: mediaName = "titleshot"; break;
		case CarouselImageSource::BOXART:    mediaName = "boxart";    break;
		case CarouselImageSource::BOXBACK:   mediaName = "boxback";   break;
		case CarouselImageSource::MARQUEE:   mediaName = "marquee";   break;
		case CarouselImageSource::FANART:    mediaName = "fanart";    break;
		case CarouselImageSource::CARTRIDGE: mediaName = "cartridge"; break;
		case CarouselImageSource::MIX:       mediaName = "mix";       break;
		}

		std::string marqueePath = entry.object->getProperty(mediaName).toString();

		if (mImageSource != CarouselImageSource::TEXT && !marqueePath.empty() && Utils::FileSystem::exists(marqueePath))
		{
			ImageComponent* logo = new ImageComponent(mWindow, false, true);
			logo->setMaxSize(mLogoSize * mLogoScale);

			if (mImageSource == CarouselImageSource::IMAGE) // If it's of type image, it's probably the systemview
				logo->setIsLinear(true);

			logo->applyTheme(mTheme, mThemeViewName, mThemeLogoName, ThemeFlags::COLOR | ThemeFlags::ALIGNMENT | ThemeFlags::VISIBLE); //  ThemeFlags::PATH | 
			logo->setImage(marqueePath, false, MaxSizeInfo(mLogoSize * mLogoScale), false);

			if (mImageSource == CarouselImageSource::IMAGE && mSize.x() != mLogoSize.x() && mSize.y() != mLogoSize.y())
				logo->setRotateByTargetSize(true);

			entry.data.logo = std::shared_ptr<GuiComponent>(logo);
		}
		else // no logo in theme; use text 
		{
			TextComponent* text = new TextComponent(mWindow, entry.name, Renderer::isSmallScreen() ? Font::get(FONT_SIZE_MEDIUM) : Font::get(FONT_SIZE_LARGE), 0x000000FF, ALIGN_CENTER);
			text->setScaleOrigin(0.0f);
			text->setSize(mLogoSize * mLogoScale);
			text->applyTheme(mTheme, mThemeViewName, mThemeLogoTextName, ThemeFlags::FONT_PATH | ThemeFlags::FONT_SIZE | ThemeFlags::COLOR | ThemeFlags::FORCE_UPPERCASE | ThemeFlags::LINE_SPACING | ThemeFlags::TEXT);

			if (mType == CarouselType::VERTICAL || mType == CarouselType::VERTICAL_WHEEL)
			{
				text->setHorizontalAlignment(mLogoAlignment);
				text->setVerticalAlignment(ALIGN_CENTER);
			}
			else
			{
				text->setHorizontalAlignment(ALIGN_CENTER);
				text->setVerticalAlignment(mLogoAlignment);
			}

			entry.data.logo = std::shared_ptr<GuiComponent>(text);
		}
	}

	entry.data.logo->updateBindings(entry.object);

	if (mType == CarouselType::VERTICAL || mType == CarouselType::VERTICAL_WHEEL)
	{
		if (mLogoAlignment == ALIGN_LEFT)
			entry.data.logo->setOrigin(0, 0.5);
		else if (mLogoAlignment == ALIGN_RIGHT)
			entry.data.logo->setOrigin(1.0, 0.5);
		else
			entry.data.logo->setOrigin(0.5, 0.5);
	}
	else
	{
		if (mLogoAlignment == ALIGN_TOP)
			entry.data.logo->setOrigin(0.5, 0);
		else if (mLogoAlignment == ALIGN_BOTTOM)
			entry.data.logo->setOrigin(0.5, 1);
		else
			entry.data.logo->setOrigin(0.5, 0.5);
	}

	Vector2f denormalized = mLogoSize * entry.data.logo->getOrigin();
	entry.data.logo->setPosition(denormalized.x(), denormalized.y(), 0.0);

	mAnyLogoHasScaleStoryboard = entry.data.logo->storyBoardExists("deactivate", "scale") ||
		entry.data.logo->storyBoardExists("activate", "scale") ||
		entry.data.logo->storyBoardExists("scroll", "scale") ||
		entry.data.logo->storyBoardExists("", "scale");

	mAnyLogoHasOpacityStoryboard =
		entry.data.logo->storyBoardExists("deactivate", "opacity") ||
		entry.data.logo->storyBoardExists("activate", "opacity") ||
		entry.data.logo->storyBoardExists("scroll", "opacity") ||
		entry.data.logo->storyBoardExists("", "opacity");

	if (!entry.data.logo->selectStoryboard("deactivate") && !entry.data.logo->selectStoryboard())
		entry.data.logo->deselectStoryboard();
}

void CarouselComponent::add(const std::string& name, IBindable* obj, bool preloadLogo)
{
	typename IList<CarouselComponentData, IBindable*>::Entry entry;
	entry.name = name;
	entry.object = obj;	
	entry.data.logo = nullptr;

	static_cast<IList<CarouselComponentData, IBindable*>*>(this)->add(entry);

	if (preloadLogo)
		ensureLogo(mEntries.at(mEntries.size() - 1));
}

void CarouselComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	mTheme = theme;
	mThemeViewName = view;

	IList<CarouselComponentData, IBindable*>::applyTheme(theme, view, element, properties);

	const ThemeData::ThemeElement* carouselElem = theme->getElement(view, element, getThemeTypeName());
	if (carouselElem)
		getCarouselFromTheme(carouselElem);
}

bool CarouselComponent::onMouseClick(int button, bool pressed, int x, int y)
{
	if (button != 1)
		return false;
	
	if (pressed)
	{
		mPressedPoint = Vector2i(x, y);
		mPressedCursor = mCursor;
		mWindow->setMouseCapture(this);
	}
	else if (mWindow->hasMouseCapture(this))
	{
		mWindow->releaseMouseCapture();

		mPressedPoint = Vector2i(-1, -1);

		if (mCamOffset != mCursor)
		{
			mLastCursor = -1;
			onCursorChanged(CursorState::CURSOR_STOPPED);
		}

		if (mPressedCursor == mCursor)
			InputManager::getInstance()->sendMouseClick(mWindow, 1);
	}

	return true;
}

#define CAROUSEL_MOUSE_SPEED 100.0f

void CarouselComponent::onMouseMove(int x, int y)
{
	if (mPressedPoint.x() != -1 && mPressedPoint.y() != -1 && mWindow->hasMouseCapture(this))
	{
		mPressedCursor = -1;

		if (isHorizontalCarousel())
		{
			float speed = CAROUSEL_MOUSE_SPEED;
			if (mMaxLogoCount > 1)
				speed = mSize.x() / mMaxLogoCount;

			if (mType == CarouselType::HORIZONTAL_WHEEL)
				speed *= 2;

			mCamOffset += (mPressedPoint.x() - x) / speed;
		}
		else
		{
			float speed = CAROUSEL_MOUSE_SPEED;
			if (mMaxLogoCount > 1)
				speed = mSize.y() / mMaxLogoCount;

			if (mType == CarouselType::VERTICAL_WHEEL)
				speed *= 2;

			mCamOffset += (mPressedPoint.y() - y) / speed;
		}

		int itemCount = mEntries.size();

		if (mCamOffset < 0)
			mCamOffset += itemCount;
		else if (mCamOffset >= itemCount)
			mCamOffset = mCamOffset - (float) itemCount;

		int offset = (int) Math::round(mCamOffset);
		if (offset < 0)
			offset += (int)itemCount;
		else if (offset >= (int)itemCount)
			offset -= (int)itemCount;

		if (mCursor != offset)
		{
			float camOffset = mCamOffset;

			if (mLastCursor == offset)
				mLastCursor = -1;

			mCursor = offset;

			onCursorChanged(CursorState::CURSOR_STOPPED);
			stopAllAnimations();

			mCursor = offset;
			mCamOffset = camOffset;
		}

		mPressedPoint = Vector2i(x, y);
	}
}

bool CarouselComponent::onMouseWheel(int delta)
{
	listInput(-delta);
	mScrollVelocity = 0;
	return true;
}

std::shared_ptr<GuiComponent> CarouselComponent::getLogo(int index)
{
	if (index >= 0 && index < mEntries.size())
		return mEntries.at(index).data.logo;

	return nullptr;
}


CarouselItemTemplate::CarouselItemTemplate(const std::string& name, Window* window) : GuiComponent(window)
{
	mName = name;
}

void CarouselItemTemplate::loadTemplatedChildren(const ThemeData::ThemeElement* elem)
{
	loadThemedChildren(elem);
}

bool CarouselItemTemplate::selectStoryboard(const std::string& name)
{
	bool selected = false;

	for (auto child : enumerateExtraChildrens())
		selected |= child->selectStoryboard(name);

	return GuiComponent::selectStoryboard(name) || selected;
}

void CarouselItemTemplate::deselectStoryboard(bool restoreinitialProperties)
{
	for (auto child : enumerateExtraChildrens())
		child->deselectStoryboard(restoreinitialProperties);

	GuiComponent::deselectStoryboard(restoreinitialProperties);
}

void CarouselItemTemplate::startStoryboard()
{
	for (auto child : enumerateExtraChildrens())
		child->startStoryboard();

	GuiComponent::startStoryboard();
}

bool CarouselItemTemplate::storyBoardExists(const std::string& name, const std::string& propertyName)
{
	for (auto child : enumerateExtraChildrens())
		if (child->storyBoardExists(name, propertyName))
			return true;

	return GuiComponent::storyBoardExists(name, propertyName);
}

void CarouselItemTemplate::updateBindings(IBindable* bindable)
{
	if (bindable != nullptr)
	{
		auto childs = enumerateExtraChildrens();

		if (Utils::String::containsIgnoreCase(mName, _U("\uF006 ")))
		{
			bool hasFavorite = false;

			for (auto comp : childs)
			{
				for (auto xp : comp->getBindingExpressions())
				{
					if (xp.second.find("{game:favorite}") != std::string::npos)
					{
						hasFavorite = true;
						break;
					}
				}
			}

			if (hasFavorite)
				mName = Utils::String::replace(mName, _U("\uF006 "), "");
		}

		std::vector<IBindable*> bindables;

		IBindable* currentParent = bindable;

		for (auto comp : childs)
		{
			auto id = comp->getTag();
			if (id.empty() || id == comp->getThemeTypeName())
				continue;

			IBindable* bindable = new ComponentBinding(comp, currentParent);
			bindables.push_back(bindable);

			currentParent = bindable;
		}		
	
		GridTemplateBinding localBinding(this, mName, currentParent);
		GuiComponent::updateBindings(&localBinding);

		for (auto bindable : bindables)
			delete bindable;
	}
	else
	{
		GridTemplateBinding localBinding(this, mName, nullptr);
		GuiComponent::updateBindings(&localBinding);
	}
}

void CarouselItemTemplate::loadFromString(const std::string& xml, std::map<std::string, std::string>* map)
{
	std::map<std::string, std::string> defMap;	
	std::map<std::string, std::string>* mapPtr = map ? map : &defMap;

	std::string dataXML = R"=====(
			<theme>
				<formatVersion>7</formatVersion>
				<view name="basic">
					<container name="default" extra="true">
			)=====" + 
			xml + 
			R"=====(</container>
				</view>
			</theme>
			)=====";

	auto theme = ThemeData::getMenuTheme();
	
	(*mapPtr)["menu.text.color"] = Utils::String::toHexString(theme->Text.color);
	(*mapPtr)["menu.text.selectedcolor"] = Utils::String::toHexString(theme->Text.selectedColor);
	(*mapPtr)["menu.text.font.path"] = theme->Text.font->getPath();
	(*mapPtr)["menu.text.font.size"] = std::to_string(theme->Text.font->getSize() / ((float)Math::min(Renderer::getScreenHeight(), Renderer::getScreenWidth())));
	(*mapPtr)["menu.textsmall.color"] = Utils::String::toHexString(theme->TextSmall.color);
	(*mapPtr)["menu.textsmall.font.path"] = theme->TextSmall.font->getPath();
	(*mapPtr)["menu.textsmall.font.size"] = std::to_string(theme->TextSmall.font->getSize() / ((float)Math::min(Renderer::getScreenHeight(), Renderer::getScreenWidth())));
	(*mapPtr)["menu.textsmall.selectedcolor"] = Utils::String::toHexString(theme->TextSmall.selectedColor);

	try
	{
		ThemeData themeData(true);
		themeData.loadFile("default", *mapPtr, dataXML, false);
		auto elem = themeData.getElement("basic", "default", "container");
		if (elem)
			loadTemplatedChildren(elem);
	}
	catch (...)
	{
		LOG(LogError) << "CarouselItemTemplate::loadFromString failed";

	}
}

void CarouselItemTemplate::playDefaultActivationStoryboard(bool activation)
{
	if (selectStoryboard(activation ? "activate" : "deactivate") || selectStoryboard())
	{
		startStoryboard();
		update(0);
	}
}
