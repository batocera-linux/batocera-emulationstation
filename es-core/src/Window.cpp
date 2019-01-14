#include "Window.h"
#include <iostream>
#include "Renderer.h"
#include "AudioManager.h"
#include "Log.h"
#include "Settings.h"
#include <algorithm>
#include <iomanip>
#include "components/HelpComponent.h"
#include "components/ImageComponent.h"
#include "guis/GuiMsgBox.h"
#include "RecalboxSystem.h"
#include "RecalboxConf.h"
#include "LocaleES.h"

#define PLAYER_PAD_TIME_MS 200

Window::Window() : mNormalizeNextUpdate(false), mFrameTimeElapsed(0), mFrameCountElapsed(0), mAverageDeltaTime(10), 
  mAllowSleep(true), mSleeping(false), mTimeSinceLastInput(0), launchKodi(false), mClockElapsed(0)
{
	mHelp = new HelpComponent(this);
	mBackgroundOverlay = new ImageComponent(this);
	mBackgroundOverlay->setImage(":/scroll_gradient.png");

	// pads
	for(int i=0; i<MAX_PLAYERS; i++) {
	  mplayerPads[i] = 0;
	}
	mplayerPadsIsHotkey = false;
}

Window::~Window()
{
	delete mBackgroundOverlay;

	// delete all our GUIs
	while(peekGui())
		delete peekGui();
	
	delete mHelp;
}

void Window::pushGui(GuiComponent* gui)
{
	if (mGuiStack.size() > 0)
	{
		auto& top = mGuiStack.back();
		top->topWindow(false);
	}
	mGuiStack.push_back(gui);
	gui->updateHelpPrompts();
}

void Window::displayMessage(std::string message)
{
    mMessages.push_back(message);
}

void Window::removeGui(GuiComponent* gui)
{
	for(auto i = mGuiStack.cbegin(); i != mGuiStack.cend(); i++)
	{
		if(*i == gui)
		{
			i = mGuiStack.erase(i);

			if(i == mGuiStack.cend() && mGuiStack.size()) // we just popped the stack and the stack is not empty
			{
				mGuiStack.back()->updateHelpPrompts();
				mGuiStack.back()->topWindow(true);
			}

			return;
		}
	}
}

GuiComponent* Window::peekGui()
{
	if(mGuiStack.size() == 0)
		return NULL;

	return mGuiStack.back();
}

bool Window::init(unsigned int width, unsigned int height, bool initRenderer)
{
    if (initRenderer) {
        if(!Renderer::init(width, height))
        {
            LOG(LogError) << "Renderer failed to initialize!";
            return false;
        }
    }

	InputManager::getInstance()->init();

	ResourceManager::getInstance()->reloadAll();

	//keep a reference to the default fonts, so they don't keep getting destroyed/recreated
	if(mDefaultFonts.empty())
	{
		mDefaultFonts.push_back(Font::get(FONT_SIZE_SMALL));
		mDefaultFonts.push_back(Font::get(FONT_SIZE_MEDIUM));
		mDefaultFonts.push_back(Font::get(FONT_SIZE_LARGE));
	}

	mBackgroundOverlay->setResize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	// update our help because font sizes probably changed
	if(peekGui())
		peekGui()->updateHelpPrompts();

	return true;
}

void Window::deinit()
{
	// Hide all GUI elements on uninitialisation - this disable
	for(auto i = mGuiStack.cbegin(); i != mGuiStack.cend(); i++)
	{
		(*i)->onHide();
	}
	InputManager::getInstance()->deinit();
	ResourceManager::getInstance()->unloadAll();
	Renderer::deinit();
}

void Window::textInput(const char* text)
{
	if(peekGui())
		peekGui()->textInput(text);
}

void Window::input(InputConfig* config, Input input)
{
	if(mSleeping)
	{
		// wake up
		mTimeSinceLastInput = 0;
		mSleeping = false;
		onWake();
		return;
	}

	mTimeSinceLastInput = 0;

	if(config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_g && SDL_GetModState() & KMOD_LCTRL && Settings::getInstance()->getBool("Debug"))
	{
		// toggle debug grid with Ctrl-G
		Settings::getInstance()->setBool("DebugGrid", !Settings::getInstance()->getBool("DebugGrid"));
	}
	else if(config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_t && SDL_GetModState() & KMOD_LCTRL && Settings::getInstance()->getBool("Debug"))
	{
		// toggle TextComponent debug view with Ctrl-T
		Settings::getInstance()->setBool("DebugText", !Settings::getInstance()->getBool("DebugText"));
	}
#if ENABLE_FILEMANAGER == 1
	else if(config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_F1)
	{
	  RecalboxSystem::getInstance()->launchFileManager(this);
	}
#endif
	else
	{
	  // show the pad button
	  if(config->getDeviceIndex() != -1 && (input.type == TYPE_BUTTON || input.type == TYPE_HAT)) {
	    mplayerPads[config->getDeviceIndex()] = PLAYER_PAD_TIME_MS;
	    mplayerPadsIsHotkey = config->isMappedTo("hotkey", input);
	  }

#if ENABLE_KODI == 1
            if(config->isMappedTo("x", input) && input.value && !launchKodi && RecalboxConf::getInstance()->get("kodi.enabled") == "1" && RecalboxConf::getInstance()->get("kodi.xbutton") == "1"){
                launchKodi = true;
                Window * window = this;
                this->pushGui(new GuiMsgBox(this, _("DO YOU WANT TO START KODI MEDIA CENTER ?"), _("YES"),
				[window, this] { 
                                    if( ! RecalboxSystem::getInstance()->launchKodi(window)) {
                                        LOG(LogWarning) << "Shutdown terminated with non-zero result!";
                                    }
                                    launchKodi = false;
					    }, _("NO"), [this] {
                                    launchKodi = false;
                                }));
            } else {
		if(peekGui())
			this->peekGui()->input(config, input);
            }
#else
	    if(peekGui())
	      this->peekGui()->input(config, input);
#endif
	}
}

void Window::update(int deltaTime)
{
    
        if(!mMessages.empty()){
		std::string message = mMessages.back();
		mMessages.pop_back();
                pushGui(new GuiMsgBox(this, message));
	}
	if(mNormalizeNextUpdate)
	{
		mNormalizeNextUpdate = false;
		if(deltaTime > mAverageDeltaTime)
			deltaTime = mAverageDeltaTime;
	}

	mFrameTimeElapsed += deltaTime;
	mFrameCountElapsed++;
	if(mFrameTimeElapsed > 500)
	{
		mAverageDeltaTime = mFrameTimeElapsed / mFrameCountElapsed;
		
		if(Settings::getInstance()->getBool("DrawFramerate"))
		{
			std::stringstream ss;
			
			// fps
			ss << std::fixed << std::setprecision(1) << (1000.0f * (float)mFrameCountElapsed / (float)mFrameTimeElapsed) << "fps, ";
			ss << std::fixed << std::setprecision(2) << ((float)mFrameTimeElapsed / (float)mFrameCountElapsed) << "ms";

			// vram
			float textureVramUsageMb = TextureResource::getTotalMemUsage() / 1000.0f / 1000.0f;
			float textureTotalUsageMb = TextureResource::getTotalTextureSize() / 1000.0f / 1000.0f;
			float fontVramUsageMb = Font::getTotalMemUsage() / 1000.0f / 1000.0f;

			ss << "\nFont VRAM: " << fontVramUsageMb << " Tex VRAM: " << textureVramUsageMb <<
				  " Tex Max: " << textureTotalUsageMb;
			mFrameDataText = std::unique_ptr<TextCache>(mDefaultFonts.at(1)->buildTextCache(ss.str(), 50.f, 50.f, 0xFF00FFFF));
		}

		mFrameTimeElapsed = 0;
		mFrameCountElapsed = 0;
	}

	/* draw the clock */
	if(Settings::getInstance()->getBool("DrawClock")) {
	  mClockElapsed -= deltaTime;
	  if(mClockElapsed <= 0)
	    {
	      time_t     clockNow;
	      struct tm  clockTstruct;
	      char       clockBuf[32];

	      clockNow = time(0);
	      clockTstruct = *localtime(&clockNow);

	      if(clockTstruct.tm_year > 100) { /* display the clock only if year is more than 1900+100 ; rpi have no internal clock and out of the networks, the date time information has no value */
		// Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
		// for more information about date/time format
		strftime(clockBuf, sizeof(clockBuf), "%H:%M", &clockTstruct);
		mClockText = std::unique_ptr<TextCache>(mDefaultFonts.at(0)->buildTextCache(clockBuf, Renderer::getScreenWidth()-80.0f, Renderer::getScreenHeight()-50.0f, 0x33333366));
	      }
	      mClockElapsed = 1000; // next update in 1000ms
	    }
	}

	// hide pads
	for(int i=0; i<MAX_PLAYERS; i++) {
	  if(mplayerPads[i] > 0) {
	    mplayerPads[i] -= deltaTime;
	    if(mplayerPads[i] < 0) {
	      mplayerPads[i] = 0;
	    }
	  }
	}

	mTimeSinceLastInput += deltaTime;

	if(peekGui())
		peekGui()->update(deltaTime);
}

void Window::render()
{
	Transform4x4f transform = Transform4x4f::Identity();

	mRenderedHelpPrompts = false;

	// draw only bottom and top of GuiStack (if they are different)
	if(mGuiStack.size())
	{
		auto& bottom = mGuiStack.front();
		auto& top = mGuiStack.back();

		bottom->render(transform);
		if(bottom != top)
		{
			mBackgroundOverlay->render(transform);
			top->render(transform);
		}
	}

	if(!mRenderedHelpPrompts)
		mHelp->render(transform);

	if(Settings::getInstance()->getBool("DrawFramerate") && mFrameDataText)
	{
		Renderer::setMatrix(Transform4x4f::Identity());
		mDefaultFonts.at(1)->renderTextCache(mFrameDataText.get());
	}

	if(Settings::getInstance()->getBool("DrawClock") && mClockText)
	  {
	    Renderer::setMatrix(Transform4x4f::Identity());
	    mDefaultFonts.at(1)->renderTextCache(mClockText.get());
	  }

	// pads
	Renderer::setMatrix(Transform4x4f::Identity());
	std::map<int, int> playerJoysticks = InputManager::getInstance()->lastKnownPlayersDeviceIndexes();
	for (int player = 0; player < MAX_PLAYERS; player++) {
	  if(playerJoysticks.count(player) == 1) {
	    unsigned int padcolor = 0xFFFFFF99;

	    if(mplayerPads[playerJoysticks[player]] > 0) {
	      if(mplayerPadsIsHotkey) {
		padcolor = 0x0000FF66;
	      } else {
		padcolor = 0xFF000066;
	      }
	    }

	    Renderer::drawRect((player*(10+4))+2, Renderer::getScreenHeight()-10-2, 10, 10, padcolor);
	  }
	}

	unsigned int screensaverTime = (unsigned int)Settings::getInstance()->getInt("ScreenSaverTime");
	if(mTimeSinceLastInput >= screensaverTime && screensaverTime != 0)
	{
		renderScreenSaver();

		if (!isProcessing() && mAllowSleep)
		{
			// go to sleep
			mSleeping = true;
			onSleep();
		}
	}
}

void Window::normalizeNextUpdate()
{
	mNormalizeNextUpdate = true;
}

bool Window::getAllowSleep()
{
	return mAllowSleep;
}

void Window::setAllowSleep(bool sleep)
{
	mAllowSleep = sleep;
}

void Window::renderWaitingScreen(const std::string& text)
{
	Transform4x4f trans = Transform4x4f::Identity();
	Renderer::setMatrix(trans);
	Renderer::drawRect(0, 0, Renderer::getScreenWidth(), Renderer::getScreenHeight(), 0xFFFFFFFF);

	ImageComponent splash(this, true);
	splash.setResize(Renderer::getScreenWidth() * 0.6f, 0.0f);
	splash.setImage(":/splash.svg");
	splash.setPosition((Renderer::getScreenWidth() - splash.getSize().x()) / 2, (Renderer::getScreenHeight() - splash.getSize().y()) / 2 * 0.6f);
	splash.render(trans);

	auto& font = mDefaultFonts.at(1);
	TextCache* cache = font->buildTextCache(text, 0, 0, 0x656565FF);
	trans = trans.translate(Vector3f(Math::round((Renderer::getScreenWidth() - cache->metrics.size.x()) / 2.0f),
		Math::round(Renderer::getScreenHeight() * 0.835f), 0.0f));
	Renderer::setMatrix(trans);
	font->renderTextCache(cache);
	delete cache;

	Renderer::swapBuffers();
}
void Window::renderLoadingScreen()
{
  renderWaitingScreen(_("LOADING..."));
}

void Window::renderHelpPromptsEarly()
{
	mHelp->render(Transform4x4f::Identity());
	mRenderedHelpPrompts = true;
}

void Window::setHelpPrompts(const std::vector<HelpPrompt>& prompts, const HelpStyle& style)
{
	mHelp->clearPrompts();
	mHelp->setStyle(style);

	std::vector<HelpPrompt> addPrompts;

	std::map<std::string, bool> inputSeenMap;
	std::map<std::string, int> mappedToSeenMap;
	for(auto it = prompts.cbegin(); it != prompts.cend(); it++)
	{
		// only add it if the same icon hasn't already been added
	  if(inputSeenMap.insert(std::make_pair<std::string, bool>(it->first.c_str(), true)).second)
		{
			// this symbol hasn't been seen yet, what about the action name?
			auto mappedTo = mappedToSeenMap.find(it->second);
			if(mappedTo != mappedToSeenMap.cend())
			{
				// yes, it has!

				// can we combine? (dpad only)
			  if((strcmp(it->first.c_str(), "up/down") == 0 && strcmp(addPrompts.at(mappedTo->second).first.c_str(), "left/right") == 0) ||
			     (strcmp(it->first.c_str(), "left/right") == 0 && strcmp(addPrompts.at(mappedTo->second).first.c_str(), "up/down") == 0))
				{
					// yes!
				  addPrompts.at(mappedTo->second).first = "up/down/left/right";
					// don't need to add this to addPrompts since we just merged
				}else{
					// no, we can't combine!
					addPrompts.push_back(*it);
				}
			}else{
				// no, it hasn't!
				mappedToSeenMap.insert(std::pair<std::string, int>(it->second, addPrompts.size()));
				addPrompts.push_back(*it);
			}
		}
	}

	// sort prompts so it goes [dpad_all] [dpad_u/d] [dpad_l/r] [a/b/x/y/l/r] [start/select]
	std::sort(addPrompts.begin(), addPrompts.end(), [](const HelpPrompt& a, const HelpPrompt& b) -> bool {
		
		static const char* map[] = {
			"up/down/left/right",
			"up/down",
			"left/right",
			"a", "b", "x", "y", "l", "r", 
			"start", "select", 
			NULL
		};
		
		int i = 0;
		int aVal = 0;
		int bVal = 0;
		while(map[i] != NULL)
		{
			if(a.first == map[i])
				aVal = i;
			if(b.first == map[i])
				bVal = i;
			i++;
		}

		return aVal > bVal;
	});

	mHelp->setPrompts(addPrompts);
}


void Window::onSleep()
{

}

void Window::onWake()
{

}

void Window::renderShutdownScreen() {
  renderWaitingScreen(_("PLEASE WAIT..."));

}

bool Window::isProcessing()
{
	return count_if(mGuiStack.begin(), mGuiStack.end(), [](GuiComponent* c) { return c->isProcessing(); }) > 0;
}

void Window::renderScreenSaver()
{
	Renderer::setMatrix(Transform4x4f::Identity());
	unsigned char opacity = Settings::getInstance()->getString("ScreenSaverBehavior") == "dim" ? 0xA0 : 0xFF;
	Renderer::drawRect(0, 0, Renderer::getScreenWidth(), Renderer::getScreenHeight(), 0x00000000 | opacity);
}
