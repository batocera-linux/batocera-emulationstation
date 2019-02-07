#include "views/SystemView.h"
#include "SystemData.h"
#include "Renderer.h"
#include "Log.h"
#include "Window.h"
#include "views/ViewController.h"
#include "animations/LambdaAnimation.h"
#include "SystemData.h"
#include "Settings.h"
#include "Util.h"
#include <guis/GuiMsgBox.h>
#include <RecalboxSystem.h>
#include <components/ComponentList.h>
#include <guis/GuiSettings.h>
#include <RecalboxConf.h>
#include "ThemeData.h"
#include "AudioManager.h"
#include "LocaleES.h"
#include <stdlib.h>

// buffer values for scrolling velocity (left, stopped, right)
const int logoBuffersLeft[] = { -5, -2, -1 };
const int logoBuffersRight[] = { 1, 2, 5 };

#define SELECTED_SCALE 1.5f
#define LOGO_PADDING ((logoSize().x() * (SELECTED_SCALE - 1)/2) + (mSize.x() * 0.06f))
#define BAND_HEIGHT (logoSize().y() * SELECTED_SCALE)

SystemView::SystemView(Window* window) : IList<SystemViewData, SystemData*>(window, LIST_SCROLL_STYLE_SLOW, LIST_ALWAYS_LOOP),
	mViewNeedsReload(true),mSystemInfo(window, "SYSTEM INFO", Font::get(FONT_SIZE_SMALL), 0x33333300, ALIGN_CENTER)
{
	mCamOffset = 0;
	mExtrasCamOffset = 0;
	mExtrasFadeOpacity = 0.0f;

	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	mSystemInfo.setSize(mSize.x(), mSystemInfo.getSize().y() * 1.333f);
	mSystemInfo.setPosition(0, (mSize.y() + BAND_HEIGHT) / 2);

	populate();
}

void SystemView::addSystem(SystemData * it){
	if((it)->getRootFolder()->getChildren().size() == 0){
		return;
	}
	const std::shared_ptr<ThemeData>& theme = (it)->getTheme();

        if(mViewNeedsReload)
                getViewElements(theme);
        
	Entry e;
	e.name = (it)->getName();
	e.object = it;

	// make logo
	const ThemeData::ThemeElement* logoElem = theme->getElement("system", "logo", "image");
	if(logoElem)
	{
                LOG(LogInfo) << " LOGO HAS ELEMENT IMAGE ";
		ImageComponent* logo = new ImageComponent(mWindow, false, false);
		logo->setMaxSize(Vector2f(logoSize().x(), logoSize().y()));
		logo->applyTheme((it)->getTheme(), "system", "logo", ThemeFlags::PATH | ThemeFlags::COLOR);
		logo->setPosition((logoSize().x() - logo->getSize().x()) / 2, (logoSize().y() - logo->getSize().y()) / 2); // center
		e.data.logo = std::shared_ptr<GuiComponent>(logo);

		ImageComponent* logoSelected = new ImageComponent(mWindow, false, false);
		logoSelected->setMaxSize(Vector2f(logoSize().x() * SELECTED_SCALE, logoSize().y() * SELECTED_SCALE * 0.70f));
		logoSelected->applyTheme((it)->getTheme(), "system", "logo", ThemeFlags::PATH | ThemeFlags::COLOR);
		logoSelected->setPosition((logoSize().x() - logoSelected->getSize().x()) / 2,
								  (logoSize().y() - logoSelected->getSize().y()) / 2); // center
                logoSelected->setZIndex(50);
		e.data.logoSelected = std::shared_ptr<GuiComponent>(logoSelected);
	}
	if (!e.data.logo)
	{
                LOG(LogInfo) << " LOGO HAS NO ELEMENT IMAGE " << (it)->getName();
		// no logo in theme; use text
		TextComponent* text = new TextComponent(mWindow,
                                                        (it)->getName(),
                                                        Font::get(FONT_SIZE_LARGE),
                                                        0x000000FF,
                                                        ALIGN_CENTER);
		text->setSize(mCarousel.logoSize );
		text->applyTheme((it)->getTheme(), "system", "logoText", ThemeFlags::FONT_PATH | ThemeFlags::FONT_SIZE | ThemeFlags::COLOR | ThemeFlags::FORCE_UPPERCASE);
		e.data.logo = std::shared_ptr<GuiComponent>(text);

                if (mCarousel.type == VERTICAL || mCarousel.type == VERTICAL_WHEEL)
                        text->setHorizontalAlignment(mCarousel.logoAlignment);
                else
                        text->setVerticalAlignment(mCarousel.logoAlignment);
                
		TextComponent* textSelected = new TextComponent(mWindow,
                                                                (it)->getName(),
                                                                Font::get((int)(FONT_SIZE_LARGE * SELECTED_SCALE)),
                                                                0x000000FF,
                                                                ALIGN_CENTER);
                text->setSize(mCarousel.logoSize );
		textSelected->applyTheme((it)->getTheme(), "system", "logoText", ThemeFlags::FONT_PATH | ThemeFlags::FONT_SIZE | ThemeFlags::COLOR | ThemeFlags::FORCE_UPPERCASE);
		e.data.logoSelected = std::shared_ptr<GuiComponent>(textSelected);

                if (mCarousel.type == VERTICAL || mCarousel.type == VERTICAL_WHEEL)
                        textSelected->setHorizontalAlignment(mCarousel.logoAlignment);
                else
                        textSelected->setVerticalAlignment(mCarousel.logoAlignment);
                
	}
        
        if (mCarousel.type == VERTICAL || mCarousel.type == VERTICAL_WHEEL)
        {
                if (mCarousel.logoAlignment == ALIGN_LEFT)
                        e.data.logo->setOrigin(0, 0.5);
                else if (mCarousel.logoAlignment == ALIGN_RIGHT)
                        e.data.logo->setOrigin(1.0, 0.5);
                else
                        e.data.logo->setOrigin(0.5, 0.5);
        } else {
                if (mCarousel.logoAlignment == ALIGN_TOP)
                        e.data.logo->setOrigin(0.5, 0);
                else if (mCarousel.logoAlignment == ALIGN_BOTTOM)
                        e.data.logo->setOrigin(0.5, 1);
                else
                        e.data.logo->setOrigin(0.5, 0.5);
        }
        Vector2f denormalized = mCarousel.logoSize * e.data.logo->getOrigin();
        e.data.logo->setPosition(denormalized.x(), denormalized.y(), 0.0);
        //e.data.logo->setPosition(0.0, 0.0, 0.0);

	// make background extras
	e.data.backgroundExtras = std::shared_ptr<ThemeExtras>(new ThemeExtras(mWindow));
	e.data.backgroundExtras->setExtras(ThemeData::makeExtras((it)->getTheme(), "system", mWindow));

	this->add(e);
}
void SystemView::populate()
{
	mEntries.clear();

	for(auto it = SystemData::sSystemVector.begin(); it != SystemData::sSystemVector.end(); it++)
	{
		addSystem((*it));
	}
}

void SystemView::goToSystem(SystemData* system, bool animate)
{


	setCursor(system);

	if(!animate)
		finishAnimation(0);
}

bool SystemView::input(InputConfig* config, Input input)
{
	if(input.value != 0)
	{
		if(config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_r && SDL_GetModState() & KMOD_LCTRL && Settings::getInstance()->getBool("Debug"))
		{
			LOG(LogInfo) << " Reloading SystemList view";

			// reload themes
			for(auto it = mEntries.begin(); it != mEntries.end(); it++)
				it->object->loadTheme();

			populate();
			updateHelpPrompts();
			return true;
		}
		if(config->isMappedTo("left", input))
		{
			listInput(-1);
			return true;
		}
		if(config->isMappedTo("right", input))
		{
			listInput(1);
			return true;
		}
		if(config->isMappedTo("b", input))
		{
			stopScrolling();
			ViewController::get()->goToGameList(getSelected());
			return true;
		}
		if(config->isMappedTo("select", input) && RecalboxConf::getInstance()->get("system.es.menu") != "none")
		{
		  auto s = new GuiSettings(mWindow, _("QUIT").c_str());

			Window *window = mWindow;
			ComponentListRow row;
			row.makeAcceptInputHandler([window] {
			    window->pushGui(new GuiMsgBox(window, _("REALLY RESTART?"), _("YES"),
											  [] {
												  if (RecalboxSystem::getInstance()->reboot() != 0)  {
													  LOG(LogWarning) << "Restart terminated with non-zero result!";
												  }
							  }, _("NO"), nullptr));
			});
			row.addElement(std::make_shared<TextComponent>(window, _("RESTART SYSTEM"), Font::get(FONT_SIZE_MEDIUM),
														   0x777777FF), true);
			s->addRow(row);

			row.elements.clear();
			row.makeAcceptInputHandler([window] {
			    window->pushGui(new GuiMsgBox(window, _("REALLY SHUTDOWN?"), _("YES"),
											  [] {
												  if (RecalboxSystem::getInstance()->shutdown() != 0)  {
													  LOG(LogWarning) <<
																	  "Shutdown terminated with non-zero result!";
												  }
							  }, _("NO"), nullptr));
			});
			row.addElement(std::make_shared<TextComponent>(window, _("SHUTDOWN SYSTEM"), Font::get(FONT_SIZE_MEDIUM),
														   0x777777FF), true);
			s->addRow(row);
			row.elements.clear();
			row.makeAcceptInputHandler([window] {
				window->pushGui(new GuiMsgBox(window, _("REALLY SHUTDOWN WITHOUT SAVING METADATAS?"), _("YES"),
											  [] {
												  if (RecalboxSystem::getInstance()->fastShutdown() != 0)  {
													  LOG(LogWarning) <<
																	  "Shutdown terminated with non-zero result!";
												  }
											  }, _("NO"), nullptr));
			});
			row.addElement(std::make_shared<TextComponent>(window, _("FAST SHUTDOWN SYSTEM"), Font::get(FONT_SIZE_MEDIUM),
														   0x777777FF), true);
			s->addRow(row);
			mWindow->pushGui(s);
		}

	}else{
		if(config->isMappedTo("left", input) || config->isMappedTo("right", input))
			listInput(0);
	}

	return GuiComponent::input(config, input);
}

void SystemView::update(int deltaTime)
{
	listUpdate(deltaTime);
	GuiComponent::update(deltaTime);
}

void SystemView::onCursorChanged(const CursorState& state)
{

	if(lastSystem != getSelected()){
		lastSystem = getSelected();
		AudioManager::getInstance()->themeChanged(getSelected()->getTheme());
	}
	// update help style
	updateHelpPrompts();

	float startPos = mCamOffset;

	float posMax = (float)mEntries.size();
	float target = (float)mCursor;

	// what's the shortest way to get to our target?
	// it's one of these...

	float endPos = target; // directly
    float dist = std::abs(endPos - startPos);

    if(std::abs(target + posMax - startPos) < dist)
		endPos = target + posMax; // loop around the end (0 -> max)
    if(std::abs(target - posMax - startPos) < dist)
		endPos = target - posMax; // loop around the start (max - 1 -> -1)


	// animate mSystemInfo's opacity (fade out, wait, fade back in)

	cancelAnimation(1);
	cancelAnimation(2);

	const float infoStartOpacity = mSystemInfo.getOpacity() / 255.f;

	Animation* infoFadeOut = new LambdaAnimation(
		[infoStartOpacity, this] (float t)
	{
		mSystemInfo.setOpacity((unsigned char)(lerp<float>(infoStartOpacity, 0.f, t) * 255));
	}, (int)(infoStartOpacity * 150));

	unsigned int gameCount = getSelected()->getGameCount();
	unsigned int favoritesCount = getSelected()->getFavoritesCount();
	unsigned int hiddenCount = getSelected()->getHiddenCount();
	unsigned int gameNoHiddenCount = gameCount - hiddenCount;

	// also change the text after we've fully faded out
	setAnimation(infoFadeOut, 0, [this, gameCount, favoritesCount, gameNoHiddenCount, hiddenCount] {
		char strbuf[256];
		if(favoritesCount == 0 && hiddenCount == 0) {
			snprintf(strbuf, 256, ngettext("%i GAME AVAILABLE", "%i GAMES AVAILABLE", gameNoHiddenCount).c_str(), gameNoHiddenCount);
		}else if (favoritesCount != 0 && hiddenCount == 0) {
			snprintf(strbuf, 256,
				(ngettext("%i GAME AVAILABLE", "%i GAMES AVAILABLE", gameNoHiddenCount) + ", " +
				 ngettext("%i FAVORITE", "%i FAVORITES", favoritesCount)).c_str(), gameNoHiddenCount, favoritesCount);
		}else if (favoritesCount == 0 && hiddenCount != 0) {
			snprintf(strbuf, 256,
				(ngettext("%i GAME AVAILABLE", "%i GAMES AVAILABLE", gameNoHiddenCount) + ", " +
				 ngettext("%i GAME HIDDEN", "%i GAMES HIDDEN", hiddenCount)).c_str(), gameNoHiddenCount, hiddenCount);
		}else {
			snprintf(strbuf, 256,
				(ngettext("%i GAME AVAILABLE", "%i GAMES AVAILABLE", gameNoHiddenCount) + ", " +
				 ngettext("%i GAME HIDDEN", "%i GAMES HIDDEN", hiddenCount) + ", " +
				 ngettext("%i FAVORITE", "%i FAVORITES", favoritesCount)).c_str(), gameNoHiddenCount, hiddenCount, favoritesCount);
		}
		mSystemInfo.setText(strbuf);
	}, false, 1);

	Animation* infoFadeIn = new LambdaAnimation(
		[this](float t)
	{
		mSystemInfo.setOpacity((unsigned char)(lerp<float>(0.f, 1.f, t) * 255));
	}, 300);

	// wait ms to fade in
	setAnimation(infoFadeIn, 800, nullptr, false, 2);

	// no need to animate transition, we're not going anywhere (probably mEntries.size() == 1)
	if(endPos == mCamOffset && endPos == mExtrasCamOffset)
		return;

	Animation* anim;
	if(Settings::getInstance()->getString("TransitionStyle") == "fade")
	{
		float startExtrasFade = mExtrasFadeOpacity;
		anim = new LambdaAnimation(
			[startExtrasFade, startPos, endPos, posMax, this](float t)
		{
			t -= 1;
			float f = lerp<float>(startPos, endPos, t*t*t + 1);
			if(f < 0)
				f += posMax;
			if(f >= posMax)
				f -= posMax;

			this->mCamOffset = f;

			t += 1;
			if(t < 0.3f)
				this->mExtrasFadeOpacity = lerp<float>(0.0f, 1.0f, t / 0.3f + startExtrasFade);
			else if(t < 0.7f)
				this->mExtrasFadeOpacity = 1.0f;
			else
				this->mExtrasFadeOpacity = lerp<float>(1.0f, 0.0f, (t - 0.7f) / 0.3f);

			if(t > 0.5f)
				this->mExtrasCamOffset = endPos;

		}, 500);
	} else if (Settings::getInstance()->getString("TransitionStyle") == "slide")
	{
		anim = new LambdaAnimation(
			[startPos, endPos, posMax, this](float t)
		{
			t -= 1;
			float f = lerp<float>(startPos, endPos, t*t*t + 1);
			if(f < 0)
				f += posMax;
			if(f >= posMax)
				f -= posMax;

			this->mCamOffset = f;
			this->mExtrasCamOffset = f;
		}, 500);
	} else {
      anim = new LambdaAnimation(
        [startPos, endPos, posMax, this](float t)
      {
        t -= 1;
        float f = lerp<float>(startPos, endPos, t*t*t + 1);
        if(f < 0)
          f += posMax;
        if(f >= posMax)
          f -= posMax;

          this->mCamOffset = endPos;
          this->mExtrasCamOffset = endPos;
      }, this ? 500 : 1);
    }

	setAnimation(anim, 0, nullptr, false, 0);
}

void SystemView::render(const Transform4x4f& parentTrans)
{
	if(size() == 0)
		return;

    Transform4x4f trans = getTransform() * parentTrans;
    auto systemInfoZIndex = mSystemInfo.getZIndex();

    
    renderExtras(trans, INT16_MIN, 100);
    
    // fade extras if necessary
    if(mExtrasFadeOpacity)
    {
            Renderer::setMatrix(trans);
            Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), 0x00000000 | (unsigned char)(mExtrasFadeOpacity * 255));
    }
    
    renderCarousel(trans);
    
    renderExtras(trans, 60, 100);
    
    Renderer::setMatrix(trans);
    mSystemInfo.render(trans);
    
}

void SystemView::renderCarousel(const Transform4x4f& trans)
{
	// background box behind logos
	Transform4x4f carouselTrans = trans;
	carouselTrans.translate(Vector3f(mCarousel.pos.x(), mCarousel.pos.y(), 0.0));
	carouselTrans.translate(Vector3f(mCarousel.origin.x() * mCarousel.size.x() * -1, mCarousel.origin.y() * mCarousel.size.y() * -1, 0.0f));

	Vector2f clipPos(carouselTrans.translation().x(), carouselTrans.translation().y());
	Renderer::pushClipRect(Vector2i((int)clipPos.x(), (int)clipPos.y()), Vector2i((int)mCarousel.size.x(), (int)mCarousel.size.y()));

	Renderer::setMatrix(carouselTrans);
	Renderer::drawRect(0.0, 0.0, mCarousel.size.x(), mCarousel.size.y(), mCarousel.color);

	// draw logos
	Vector2f logoSpacing(0.0, 0.0); // NB: logoSpacing will include the size of the logo itself as well!
	float xOff = 0.0;
	float yOff = 0.0;

	switch (mCarousel.type)
	{
		case VERTICAL_WHEEL:
			yOff = (mCarousel.size.y() - mCarousel.logoSize.y()) / 2.f - (mCamOffset * logoSpacing[1]);
			if (mCarousel.logoAlignment == ALIGN_LEFT)
				xOff = mCarousel.logoSize.x() / 10.f;
			else if (mCarousel.logoAlignment == ALIGN_RIGHT)
				xOff = mCarousel.size.x() - (mCarousel.logoSize.x() * 1.1f);
			else
				xOff = (mCarousel.size.x() - mCarousel.logoSize.x()) / 2.f;
			break;
		case VERTICAL:
			logoSpacing[1] = ((mCarousel.size.y() - (mCarousel.logoSize.y() * mCarousel.maxLogoCount)) / (mCarousel.maxLogoCount)) + mCarousel.logoSize.y();
			yOff = (mCarousel.size.y() - mCarousel.logoSize.y()) / 2.f - (mCamOffset * logoSpacing[1]);

			if (mCarousel.logoAlignment == ALIGN_LEFT)
				xOff = mCarousel.logoSize.x() / 10.f;
			else if (mCarousel.logoAlignment == ALIGN_RIGHT)
				xOff = mCarousel.size.x() - (mCarousel.logoSize.x() * 1.1f);
			else
				xOff = (mCarousel.size.x() - mCarousel.logoSize.x()) / 2;
			break;
		case HORIZONTAL:
		default:
			logoSpacing[0] = ((mCarousel.size.x() - (mCarousel.logoSize.x() * mCarousel.maxLogoCount)) / (mCarousel.maxLogoCount)) + mCarousel.logoSize.x();
			xOff = (mCarousel.size.x() - mCarousel.logoSize.x()) / 2.f - (mCamOffset * logoSpacing[0]);

			if (mCarousel.logoAlignment == ALIGN_TOP)
				yOff = mCarousel.logoSize.y() / 10.f;
			else if (mCarousel.logoAlignment == ALIGN_BOTTOM)
				yOff = mCarousel.size.y() - (mCarousel.logoSize.y() * 1.1f);
			else
				yOff = (mCarousel.size.y() - mCarousel.logoSize.y()) / 2.f;
			break;
	}

	int center = (int)(mCamOffset);
	int logoCount = Math::min(mCarousel.maxLogoCount, (int)mEntries.size());


	for(int i = center - logoCount/2; i < center + logoCount/2 + 1; i++)
	{
		int index = i;
		while (index < 0)
			index += (int)mEntries.size();
		while (index >= (int)mEntries.size())
			index -= (int)mEntries.size();

		Transform4x4f logoTrans = carouselTrans;
		logoTrans.translate(Vector3f(i * logoSpacing[0] + xOff, i * logoSpacing[1] + yOff, 0));

		float distance = i - mCamOffset;

		float scale = 1.0f + ((mCarousel.logoScale - 1.0f) * (1.0f - fabs(distance)));
		scale = Math::min(mCarousel.logoScale, Math::max(1.0f, scale));
		scale /= mCarousel.logoScale;

		int opacity = (int)Math::round(0x80 + ((0xFF - 0x80) * (1.0f - fabs(distance))));
		opacity = Math::max((int) 0x80, opacity);

		const std::shared_ptr<GuiComponent> &comp = mEntries.at(index).data.logo;
		if (mCarousel.type == VERTICAL_WHEEL) {
			comp->setRotationDegrees(mCarousel.logoRotation * distance);
			comp->setRotationOrigin(mCarousel.logoRotationOrigin);
		}
		comp->setScale(scale);
		comp->setOpacity((unsigned char)opacity);
		comp->render(logoTrans);
	}
	Renderer::popClipRect();
}

// Draw background extras
void SystemView::renderExtras(const Transform4x4f& trans, float lower, float upper)
{
	int extrasCenter = (int)mExtrasCamOffset;

	// Adding texture loading buffers depending on scrolling speed and status
	int bufferIndex = getScrollingVelocity() + 1;

	Renderer::pushClipRect(Vector2i::Zero(), Vector2i((int)mSize.x(), (int)mSize.y()));

	for(int i = extrasCenter - 1; i < extrasCenter + 2; i++)
	{
		int index = i;
		while (index < 0)
			index += (int)mEntries.size();
		while (index >= (int)mEntries.size())
			index -= (int)mEntries.size();

		//Only render selected system when not showing
		if (mShowing || index == mCursor)
		{
			Transform4x4f extrasTrans = trans;
			if (mCarousel.type == HORIZONTAL)
				extrasTrans.translate(Vector3f((i - mExtrasCamOffset) * mSize.x(), 0, 0));
			else
				extrasTrans.translate(Vector3f(0, (i - mExtrasCamOffset) * mSize.y(), 0));

			Renderer::pushClipRect(Vector2i((int)extrasTrans.translation()[0], (int)extrasTrans.translation()[1]),
								   Vector2i((int)mSize.x(), (int)mSize.y()));
			SystemViewData data = mEntries.at(index).data;
			for (unsigned int j = 0; j < data.backgroundExtras->getChildCount(); j++) {
				GuiComponent *extra = data.backgroundExtras->getChild(j);
				if (extra->getZIndex() >= lower && extra->getZIndex() < upper) {
					extra->render(extrasTrans);
				}
			}
			Renderer::popClipRect();
		}
	}
	Renderer::popClipRect();
}


std::vector<HelpPrompt> SystemView::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt("left/right", _("CHOOSE")));
	prompts.push_back(HelpPrompt("b", _("SELECT")));
#if ENABLE_FILEMANAGER == 1
	prompts.push_back(HelpPrompt("F1", _("FILES")));
#endif
	return prompts;
}

HelpStyle SystemView::getHelpStyle()
{
	HelpStyle style;
	style.applyTheme(mEntries.at(mCursor).object->getTheme(), "system");
	return style;
}

void SystemView::removeFavoriteSystem(){
	for(auto it = mEntries.begin(); it != mEntries.end(); it++)
		if(it->object->isFavorite()){
			mEntries.erase(it);
			break;
		}
}

void SystemView::manageFavorite(){
	bool hasFavorite = false;
	for(auto it = mEntries.begin(); it != mEntries.end(); it++)
		if(it->object->isFavorite()){
			hasFavorite = true;
		}
	SystemData *favorite = SystemData::getFavoriteSystem();
	if(hasFavorite) {
		if (favorite->getFavoritesCount() == 0) {
			removeFavoriteSystem();
		}
	}else {
		if (favorite->getFavoritesCount() > 0) {
			addSystem(favorite);
		}
	}
}

void  SystemView::onThemeChanged()
{
	LOG(LogDebug) << "SystemView::onThemeChanged()";
	mViewNeedsReload = true;
	populate();
}

// Populate the system carousel with the legacy values
void  SystemView::getDefaultElements(void)
{
        LOG(LogInfo) << " renderSysView yof "  << mSize.y() << " msx " << mSize.x();
        // Carousel
	mCarousel.type = HORIZONTAL;
	mCarousel.logoAlignment = ALIGN_CENTER;
	mCarousel.size.x() = mSize.x();
	mCarousel.size.y() = 0.2325f * mSize.y();
	mCarousel.pos.x() = 0.0f;
	mCarousel.pos.y() = 0.5f * (mSize.y() - mCarousel.size.y());
	mCarousel.origin.x() = 0.0f;
	mCarousel.origin.y() = 0.0f;
	mCarousel.color = 0xFFFFFFD8;
	mCarousel.logoScale = 1.2f;
	mCarousel.logoRotation = 7.5;
	mCarousel.logoRotationOrigin.x() = -5;
	mCarousel.logoRotationOrigin.y() = 0.5;
	mCarousel.logoSize.x() = 0.25f * mSize.x();
	mCarousel.logoSize.y() = 0.155f * mSize.y();
	mCarousel.maxLogoCount = 3;
	mCarousel.zIndex = 40;
        
	// System Info Bar
	mSystemInfo.setSize(mSize.x(), mSystemInfo.getFont()->getLetterHeight()*2.2f);
	mSystemInfo.setPosition(0, (mCarousel.pos.y() + mCarousel.size.y() - 0.2f));
	mSystemInfo.setBackgroundColor(0xDDDDDDD8);
	mSystemInfo.setRenderBackground(true);
	mSystemInfo.setFont(Font::get((int)(0.035f * mSize.y()), Font::getDefaultPath()));
	mSystemInfo.setColor(0x000000FF);
	mSystemInfo.setZIndex(50);
	mSystemInfo.setDefaultZIndex(50);
        
}

//  Get the ThemeElements that make up the SystemView.
void  SystemView::getViewElements(const std::shared_ptr<ThemeData>& theme)
{
	LOG(LogDebug) << "SystemView::getViewElements()";

	getDefaultElements();

	if (!theme->hasView("system"))
		return;

	const ThemeData::ThemeElement* carouselElem = theme->getElement("system", "systemcarousel", "carousel");
	if (carouselElem)
		getCarouselFromTheme(carouselElem);

	const ThemeData::ThemeElement* sysInfoElem = theme->getElement("system", "systemInfo", "text");
	if (sysInfoElem)
		mSystemInfo.applyTheme(theme, "system", "systemInfo", ThemeFlags::ALL);

	mViewNeedsReload = false;
        
        LOG(LogInfo) << " renderSysView yof " << mCarousel.logoSize.x() << " xof " << mCarousel.logoSize.y() << " mcay " << mCarousel.size.y() << " mcax " << mCarousel.size.x() << " mtyp " << mCarousel.type  << " mcsx " << mCarousel.size.x()   << " mcsy " << mCarousel.size.y();
        
}


void SystemView::getCarouselFromTheme(const ThemeData::ThemeElement* elem)
{
	if (elem->has("type"))
	{
		if (!(elem->get<std::string>("type").compare("vertical")))
			mCarousel.type = VERTICAL;
		else if (!(elem->get<std::string>("type").compare("vertical_wheel")))
			mCarousel.type = VERTICAL_WHEEL;
		else
			mCarousel.type = HORIZONTAL;
	}
	if (elem->has("size"))
		mCarousel.size = elem->get<Vector2f>("size") * mSize;
	if (elem->has("pos"))
		mCarousel.pos = elem->get<Vector2f>("pos") * mSize;
	if (elem->has("origin"))
		mCarousel.origin = elem->get<Vector2f>("origin");
	if (elem->has("color"))
		mCarousel.color = elem->get<unsigned int>("color");
	if (elem->has("logoScale"))
		mCarousel.logoScale = elem->get<float>("logoScale");
	if (elem->has("logoSize"))
		mCarousel.logoSize = elem->get<Vector2f>("logoSize") * mSize;
	if (elem->has("maxLogoCount"))
		mCarousel.maxLogoCount = (int)Math::round(elem->get<float>("maxLogoCount"));
	if (elem->has("zIndex"))
		mCarousel.zIndex = elem->get<float>("zIndex");
	if (elem->has("logoRotation"))
		mCarousel.logoRotation = elem->get<float>("logoRotation");
    if (elem->has("logoRotationOrigin"))
		mCarousel.logoRotationOrigin = elem->get<Vector2f>("logoRotationOrigin");
	if (elem->has("logoAlignment"))
	{
		if (!(elem->get<std::string>("logoAlignment").compare("left")))
			mCarousel.logoAlignment = ALIGN_LEFT;
		else if (!(elem->get<std::string>("logoAlignment").compare("right")))
			mCarousel.logoAlignment = ALIGN_RIGHT;
		else if (!(elem->get<std::string>("logoAlignment").compare("top")))
			mCarousel.logoAlignment = ALIGN_TOP;
		else if (!(elem->get<std::string>("logoAlignment").compare("bottom")))
			mCarousel.logoAlignment = ALIGN_BOTTOM;
		else
			mCarousel.logoAlignment = ALIGN_CENTER;
	}
}

void SystemView::onShow()
{
	mShowing = true;
}

void SystemView::onHide()
{
	mShowing = false;
}

