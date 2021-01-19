#include "CarouselComponent.h"
#include "FileData.h"
#include "Settings.h"
#include "Log.h"
#include "animations/LambdaAnimation.h"
#include "Sound.h"
#include "LocaleES.h"

// buffer values for scrolling velocity (left, stopped, right)
const int logoBuffersLeft[] = { -5, -2, -1 };
const int logoBuffersRight[] = { 1, 2, 5 };

CarouselComponent::CarouselComponent(Window* window) :
	IList<CarouselComponentData, FileData*>(window, LIST_SCROLL_STYLE_SLOW, LIST_ALWAYS_LOOP)
{
	mCamOffset = 0;
	mScreensaverActive = false;
	mDisable = false;		
	mLastCursor = 0;
		
	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	mType = VERTICAL;
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
}

CarouselComponent::~CarouselComponent()
{
	clearEntries();
}

void CarouselComponent::clearEntries()
{
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
		case VERTICAL:
		case VERTICAL_WHEEL:
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
		case HORIZONTAL:
		case HORIZONTAL_WHEEL:
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

	std::string transition_style = Settings::getInstance()->getString("TransitionStyle");
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
	mLastCursor = mCursor;

	// TODO

	if (oldCursor >= 0 && oldCursor < mEntries.size())
	{
		auto logo = mEntries.at(oldCursor).data.logo;				
		if (logo)
		{
			if (logo->selectStoryboard("deactivate"))
				logo->startStoryboard();
			else if (!logo->selectStoryboard())
				logo->deselectStoryboard();
		}
	}

	if (mCursor >= 0 && mCursor < mEntries.size())
	{
		auto logo = mEntries.at(mCursor).data.logo;
		if (logo)
		{
			if (logo->selectStoryboard("activate"))
				logo->startStoryboard();
			else if (!logo->selectStoryboard())
				logo->deselectStoryboard();
		}
	}

	Animation* anim;
	bool move_carousel = Settings::getInstance()->getBool("MoveCarousel");
	if (Settings::getInstance()->getString("PowerSaverMode") == "instant")
		move_carousel = false;

	if (transition_style == "fade" || transition_style == "fade & slide")
	{
		anim = new LambdaAnimation([this, startPos, endPos, posMax, move_carousel, oldCursor, transition_style](float t)
		{
			float f = Math::lerp(startPos, endPos, Math::easeOutQuint(t));

			if (f < 0) f += posMax;
			if (f >= posMax) f -= posMax;

			this->mCamOffset = move_carousel ? f : endPos;
		}, 500);
	} 
	else if (transition_style == "slide") 
	{
		anim = new LambdaAnimation([this, startPos, endPos, posMax, move_carousel](float t)
		{			
			float f = Math::lerp(startPos, endPos, Math::easeOutQuint(t));
			if (f < 0) f += posMax;
			if (f >= posMax) f -= posMax;

			this->mCamOffset = move_carousel ? f : endPos;			

		}, 500);
	} 
	else // instant
	{		
		anim = new LambdaAnimation([this, startPos, endPos, posMax, move_carousel ](float t)
		{
			float f = Math::lerp(startPos, endPos, Math::easeOutQuint(t));
			if (f < 0) f += posMax; 
			if (f >= posMax) f -= posMax;

			this->mCamOffset = move_carousel ? f : endPos;			

		}, move_carousel ? 500 : 1);
	}

	if (mCursorChangedCallback)
		mCursorChangedCallback(state);

	setAnimation(anim, 0, [this, state]
	{
		if (mCursorChangedCallback)
			mCursorChangedCallback(state);
	}, false, 0);
}

void CarouselComponent::render(const Transform4x4f& parentTrans)
{
	if (size() == 0 || !mVisible)
		return;  // nothing to render

	Transform4x4f trans = getTransform() * parentTrans;

	if (!Renderer::isVisibleOnScreen(trans.translation().x(), trans.translation().y(), mSize.x(), mSize.y()))
		return;

	Renderer::pushClipRect(Vector2i(trans.translation().x(), trans.translation().y()), Vector2i(mSize.x(), mSize.y()));
	renderCarousel(trans);
	Renderer::popClipRect();
}

std::vector<HelpPrompt> CarouselComponent::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;

	if (mType == VERTICAL || mType == VERTICAL_WHEEL)
		prompts.push_back(HelpPrompt("up/down", _("CHOOSE")));
	else
		prompts.push_back(HelpPrompt("left/right", _("CHOOSE")));

	return prompts;
}

//  Render system carousel
void CarouselComponent::renderCarousel(const Transform4x4f& trans)
{
	Transform4x4f carouselTrans = trans;
	
	Renderer::setMatrix(carouselTrans);

	// draw logos
	Vector2f logoSpacing(0.0, 0.0); // NB: logoSpacing will include the size of the logo itself as well!
	float xOff = 0.0;
	float yOff = 0.0;

	switch (mType)
	{
		case VERTICAL_WHEEL:
			yOff = (mSize.y() - mLogoSize.y()) / 2.f - (mCamOffset * logoSpacing[1]);
			if (mLogoAlignment == ALIGN_LEFT)
				xOff = mLogoSize.x() / 10.f;
			else if (mLogoAlignment == ALIGN_RIGHT)
				xOff = mSize.x() - (mLogoSize.x() * 1.1f);
			else
				xOff = (mSize.x() - mLogoSize.x()) / 2.f;
			break;
		case VERTICAL:
			logoSpacing[1] = ((mSize.y() - (mLogoSize.y() * mMaxLogoCount)) / (mMaxLogoCount)) + mLogoSize.y();
			yOff = (mSize.y() - mLogoSize.y()) / 2.f - (mCamOffset * logoSpacing[1]);

			if (mLogoAlignment == ALIGN_LEFT)
				xOff = mLogoSize.x() / 10.f;
			else if (mLogoAlignment == ALIGN_RIGHT)
				xOff = mSize.x() - (mLogoSize.x() * 1.1f);
			else
				xOff = (mSize.x() - mLogoSize.x()) / 2;
			break;
		case HORIZONTAL_WHEEL:
			xOff = (mSize.x() - mLogoSize.x()) / 2 - (mCamOffset * logoSpacing[1]);
			if (mLogoAlignment == ALIGN_TOP)
				yOff = mLogoSize.y() / 10;
			else if (mLogoAlignment == ALIGN_BOTTOM)
				yOff = mSize.y() - (mLogoSize.y() * 1.1f);
			else
				yOff = (mSize.y() - mLogoSize.y()) / 2;
			break;
		case HORIZONTAL:
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
		xOff = mLogoPos.x() - (mType == HORIZONTAL ? (mCamOffset * logoSpacing[0]) : 0);

	if (mLogoPos.y() >= 0)
		yOff = mLogoPos.y() - (mType == VERTICAL ? (mCamOffset * logoSpacing[1]) : 0);

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
		logoTrans.translate(Vector3f(i * logoSpacing[0] + xOff, i * logoSpacing[1] + yOff, 0));

		float distance = i - mCamOffset;

		float scale = 1.0f + ((mLogoScale - 1.0f) * (1.0f - fabs(distance)));
		scale = Math::min(mLogoScale, Math::max(1.0f, scale));
		scale /= mLogoScale;

		int opacity = (int)Math::round(0x80 + ((0xFF - 0x80) * (1.0f - fabs(distance))));
		opacity = Math::max((int)0x80, opacity);

		ensureLogo(mEntries.at(index));

		const std::shared_ptr<GuiComponent> &comp = mEntries.at(index).data.logo;
		if (mType == VERTICAL_WHEEL || mType == HORIZONTAL_WHEEL) 
		{
			comp->setRotationDegrees(mLogoRotation * distance);
			comp->setRotationOrigin(mLogoRotationOrigin);
		}
		
		if (!comp->hasStoryBoard())
		{
			comp->setScale(scale);
			comp->setOpacity((unsigned char)opacity);
		}
		
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
	if (elem->has("type"))
	{
		if (!(elem->get<std::string>("type").compare("vertical")))
			mType = VERTICAL;
		else if (!(elem->get<std::string>("type").compare("vertical_wheel")))
			mType = VERTICAL_WHEEL;
		else if (!(elem->get<std::string>("type").compare("horizontal_wheel")))
			mType = HORIZONTAL_WHEEL;
		else
			mType = HORIZONTAL;
	}
	if (elem->has("logoScale"))
		mLogoScale = elem->get<float>("logoScale");
	if (elem->has("logoSize"))
		mLogoSize = elem->get<Vector2f>("logoSize") * mSize;
	if (elem->has("logoPos"))
		mLogoPos = elem->get<Vector2f>("logoPos") * mSize;
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

	if (elem->has("scrollSound"))
		mScrollSound = elem->get<std::string>("scrollSound");

	if (elem->has("defaultTransition"))
		mDefaultTransition = elem->get<std::string>("defaultTransition");

	if (elem->has("imageSource"))
	{
		auto direction = elem->get<std::string>("imageSource");
		if (direction == "text")
			mImageSource = TEXT;
		else if (direction == "image")
			mImageSource = IMAGE;
		else if (direction == "marquee")
			mImageSource = MARQUEE;
		else
			mImageSource = THUMBNAIL;
	}
	else
		mImageSource = THUMBNAIL;

}

void CarouselComponent::onShow()
{
	GuiComponent::onShow();		

	if (mCursor >= 0 && mCursor < mEntries.size())
	{
		auto logo = mEntries.at(mCursor).data.logo;
		if (logo)
		{
			if (logo->selectStoryboard("activate"))
				logo->startStoryboard();
			else if (!logo->selectStoryboard())
				logo->deselectStoryboard();
		}
	}
}

void CarouselComponent::onHide()
{
	GuiComponent::onHide();	
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

FileData* CarouselComponent::getActiveFileData()
{
	if (mCursor < 0 || mCursor >= mEntries.size())
		return nullptr;

	return mEntries[mCursor].object;
}

void CarouselComponent::ensureLogo(IList<CarouselComponentData, FileData*>::Entry& entry)
{
	if (entry.data.logo != nullptr)
		return;

	std::string marqueePath;

	if (mImageSource == IMAGE)
		marqueePath = entry.object->getImagePath();
	else if (mImageSource == THUMBNAIL)
		marqueePath = entry.object->getThumbnailPath();
	else
		marqueePath = entry.object->getMarqueePath();

	if (mImageSource != TEXT && Utils::FileSystem::exists(marqueePath))
	{
		ImageComponent* logo = new ImageComponent(mWindow, false, true);
		logo->setMaxSize(mLogoSize * mLogoScale);
		logo->setIsLinear(true);
		logo->applyTheme(mTheme, "gamecarousel", "gamecarouselLogo", ThemeFlags::COLOR | ThemeFlags::ALIGNMENT | ThemeFlags::VISIBLE); //  ThemeFlags::PATH | 
		logo->setImage(marqueePath, false, MaxSizeInfo(mLogoSize * mLogoScale));

		entry.data.logo = std::shared_ptr<GuiComponent>(logo);
	}
	else // no logo in theme; use text 
	{
		TextComponent* text = new TextComponent(mWindow, entry.name, Renderer::isSmallScreen() ? Font::get(FONT_SIZE_MEDIUM) : Font::get(FONT_SIZE_LARGE), 0x000000FF, ALIGN_CENTER);
		text->setScaleOrigin(0.0f);
		text->setSize(mLogoSize * mLogoScale);
		text->applyTheme(mTheme, "gamecarousel", "gamecarouselLogoText", ThemeFlags::FONT_PATH | ThemeFlags::FONT_SIZE | ThemeFlags::COLOR | ThemeFlags::FORCE_UPPERCASE | ThemeFlags::LINE_SPACING | ThemeFlags::TEXT);

		if (mType == VERTICAL || mType == VERTICAL_WHEEL)
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

	if (mType == VERTICAL || mType == VERTICAL_WHEEL)
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
}

void CarouselComponent::add(const std::string& name, FileData* obj)
{
	typename IList<CarouselComponentData, FileData*>::Entry entry;
	entry.name = name;
	entry.object = obj;	
	entry.data.logo = nullptr;

	static_cast<IList<CarouselComponentData, FileData*>*>(this)->add(entry);
}

void CarouselComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	mTheme = theme;

	IList<CarouselComponentData, FileData*>::applyTheme(theme, view, element, properties);

	const ThemeData::ThemeElement* carouselElem = theme->getElement(view, "gamecarousel", "gamecarousel");
	if (carouselElem)
		getCarouselFromTheme(carouselElem);
}