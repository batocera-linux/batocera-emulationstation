#include "Window.h"

#include "components/HelpComponent.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "resources/Font.h"
#include "resources/TextureResource.h"
#include "InputManager.h"
#include "Log.h"
#include "Scripting.h"
#include <algorithm>
#include <iomanip>
#include "guis/GuiInfoPopup.h"
#include "SystemConf.h"
#include "LocaleES.h"
#include "AudioManager.h"
#include <SDL_events.h>
#include "ThemeData.h"
#include <mutex>
#include "components/AsyncNotificationComponent.h"
#include "components/ControllerActivityComponent.h"
#include "components/BatteryIndicatorComponent.h"
#include "guis/GuiMsgBox.h"
#include "components/VolumeInfoComponent.h"
#include "Splash.h"
#include "PowerSaver.h"
#ifdef _ENABLEEMUELEC
#include "utils/FileSystemUtil.h"
#endif

Window::Window() : mNormalizeNextUpdate(false), mFrameTimeElapsed(0), mFrameCountElapsed(0), mAverageDeltaTime(10),
  mAllowSleep(true), mSleeping(false), mTimeSinceLastInput(0), mScreenSaver(NULL), mRenderScreenSaver(false), mClockElapsed(0) // batocera
{		
	mTransitionOffset = 0;

	mHelp = new HelpComponent(this);
	mBackgroundOverlay = new ImageComponent(this);
	mBackgroundOverlay->setImage(":/scroll_gradient.png"); // batocera

	mSplash = nullptr;
}

Window::~Window()
{
	for (auto extra : mScreenExtras)
		delete extra;

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

	gui->onShow();
	mGuiStack.push_back(gui);
	gui->updateHelpPrompts();
}

void Window::removeGui(GuiComponent* gui)
{
	for(auto i = mGuiStack.cbegin(); i != mGuiStack.cend(); i++)
	{
		if(*i == gui)
		{						
			gui->onHide();
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

bool Window::init(bool initRenderer)
{
	LOG(LogInfo) << "Window::init";

	if (initRenderer)
	{
		if (!Renderer::init())
		{
			LOG(LogError) << "Renderer failed to initialize!";
			return false;
		}
	}
	else 
		Renderer::activateWindow();

	InputManager::getInstance()->init();

	ResourceManager::getInstance()->reloadAll();

	//keep a reference to the default fonts, so they don't keep getting destroyed/recreated
	if(mDefaultFonts.empty())
	{
		mDefaultFonts.push_back(Font::get(FONT_SIZE_SMALL));
		mDefaultFonts.push_back(Font::get(FONT_SIZE_MEDIUM));
		mDefaultFonts.push_back(Font::get(FONT_SIZE_LARGE));
	}

	mBackgroundOverlay->setImage(":/scroll_gradient.png");
	mBackgroundOverlay->setResize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	if (mClock == nullptr)
	{
		mClock = std::make_shared<TextComponent>(this);
		mClock->setFont(Font::get(FONT_SIZE_SMALL));
		mClock->setHorizontalAlignment(ALIGN_RIGHT);
		mClock->setVerticalAlignment(ALIGN_TOP);
		mClock->setPosition(Renderer::getScreenWidth()*0.94, Renderer::getScreenHeight()*0.9965 - Font::get(FONT_SIZE_SMALL)->getHeight());
		mClock->setSize(Renderer::getScreenWidth()*0.05, 0);
		mClock->setColor(0x777777FF);
	}

	if (mControllerActivity == nullptr)
		mControllerActivity = std::make_shared<ControllerActivityComponent>(this);

	if (mBatteryIndicator == nullptr)
		mBatteryIndicator = std::make_shared<BatteryIndicatorComponent>(this);
	
	if (mVolumeInfo == nullptr)
		mVolumeInfo = std::make_shared<VolumeInfoComponent>(this);
	else
		mVolumeInfo->reset();

	// update our help because font sizes probably changed
	if (peekGui())
#ifdef _ENABLEEMUELEC	
		// emuelec
      if(Utils::FileSystem::exists("/emuelec/bin/fbfix")) {
      system("/emuelec/bin/fbfix");      
  } else { 
	  if(Utils::FileSystem::exists("/storage/.kodi/addons/script.emuelec.Amlogic-ng.launcher/bin/fbfix")) {
	   system("/storage/.kodi/addons/script.emuelec.Amlogic-ng.launcher/bin/fbfix");
	  }
  }
#endif
	peekGui()->updateHelpPrompts();
	return true;
}

void Window::reactivateGui()
{
	for (auto extra : mScreenExtras)
		extra->onShow();

	for (auto i = mGuiStack.cbegin(); i != mGuiStack.cend(); i++)
		(*i)->onShow();

	if (peekGui())
		peekGui()->updateHelpPrompts();
}

void Window::deinit(bool deinitRenderer)
{
	for (auto extra : mScreenExtras)
		extra->onHide();

	// Hide all GUI elements on uninitialisation - this disable
	for(auto i = mGuiStack.cbegin(); i != mGuiStack.cend(); i++)
		(*i)->onHide();

	if (deinitRenderer)
		InputManager::getInstance()->deinit();

	TextureResource::clearQueue();
	ResourceManager::getInstance()->unloadAll();

	if (deinitRenderer)
		Renderer::deinit();
}

void Window::textInput(const char* text)
{
	if(peekGui())
		peekGui()->textInput(text);
}

void Window::input(InputConfig* config, Input input)
{
	if (mScreenSaver) 
	{
		if (mScreenSaver->isScreenSaverActive() && Settings::getInstance()->getBool("ScreenSaverControls") &&
			((Settings::getInstance()->getString("ScreenSaverBehavior") == "slideshow") || 			
			(Settings::getInstance()->getString("ScreenSaverBehavior") == "random video")))
		{
			if (config->isMappedLike("right", input) || config->isMappedTo("select", input))
			{
				if (input.value != 0) // handle screensaver control
					mScreenSaver->nextVideo();
					
				mTimeSinceLastInput = 0;
				return;
			}
			else if (config->isMappedTo("start", input) && input.value != 0 && mScreenSaver->getCurrentGame() != nullptr)
			{
				// launch game!
				cancelScreenSaver();
				mScreenSaver->launchGame();
				// to force handling the wake up process
				mSleeping = true;
			}
		}
	}

	if (mSleeping)
	{
		// wake up
		mTimeSinceLastInput = 0;
		cancelScreenSaver();
		mSleeping = false;
		onWake();
		return;
	}

	mTimeSinceLastInput = 0;
	if (cancelScreenSaver())
		return;

	if (config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_g && SDL_GetModState() & KMOD_LCTRL) // && Settings::getInstance()->getBool("Debug"))
	{
		// toggle debug grid with Ctrl-G
		Settings::getInstance()->setBool("DebugGrid", !Settings::getInstance()->getBool("DebugGrid"));
	}
	else if (config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_t && SDL_GetModState() & KMOD_LCTRL) // && Settings::getInstance()->getBool("Debug"))
	{
		// toggle TextComponent debug view with Ctrl-T
		Settings::getInstance()->setBool("DebugText", !Settings::getInstance()->getBool("DebugText"));
	}
	else if (config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_i && SDL_GetModState() & KMOD_LCTRL) // && Settings::getInstance()->getBool("Debug"))
	{
		// toggle TextComponent debug view with Ctrl-I
		Settings::getInstance()->setBool("DebugImage", !Settings::getInstance()->getBool("DebugImage"));
	}
	else
	{
		if (mControllerActivity != nullptr)
			mControllerActivity->input(config, input);

		if (peekGui())
			peekGui()->input(config, input); // this is where the majority of inputs will be consumed: the GuiComponent Stack
	}
}

// batocera Notification messages
static std::mutex mNotificationMessagesLock;

void Window::displayNotificationMessage(std::string message, int duration)
{
	std::unique_lock<std::mutex> lock(mNotificationMessagesLock);

	if (duration <= 0)
	{
		duration = Settings::getInstance()->getInt("audio.display_titles_time");
		if (duration <= 2 || duration > 120)
			duration = 10;

		duration *= 1000;
	}

	NotificationMessage msg;
	msg.first = message;
	msg.second = duration;
	mNotificationMessages.push_back(msg);
}


void Window::processNotificationMessages()
{
	std::unique_lock<std::mutex> lock(mNotificationMessagesLock);

	if (mNotificationMessages.empty())
		return;
	
	NotificationMessage msg = mNotificationMessages.back();
	mNotificationMessages.pop_back();

	LOG(LogDebug) << "Notification message :" << msg.first.c_str();
	
	if (mNotificationPopups.size() == 0)
		PowerSaver::pause();

	auto infoPopup = new GuiInfoPopup(this, msg.first, msg.second);
	mNotificationPopups.push_back(infoPopup);

	layoutNotificationPopups();
}

void Window::stopNotificationPopups()
{
	if (mNotificationPopups.size() == 0)
		return;

	for (auto ip : mNotificationPopups)
		delete ip;

	mNotificationPopups.clear();
	PowerSaver::resume();
}

void Window::updateNotificationPopups(int deltaTime)
{
	bool changed = false;

	for (int i = mNotificationPopups.size() - 1 ; i >= 0 ; i--)
	{
		mNotificationPopups[i]->update(deltaTime);

		if (!mNotificationPopups[i]->isRunning())
		{
			delete mNotificationPopups[i];

			auto it = mNotificationPopups.begin();
			std::advance(it, i);
			mNotificationPopups.erase(it);

			changed = true;
		}
		else if (mNotificationPopups[i]->getFadeOut() != 0)
			changed = true;
	}

	if (changed)
	{
		if (mNotificationPopups.size() == 0)
			PowerSaver::resume();
		else
			layoutNotificationPopups();
	}
}

void Window::layoutNotificationPopups()
{
	float posY = Renderer::getScreenHeight() * 0.02f;
	int paddingY = (int)posY;

	for (auto popup : mNotificationPopups)
	{
		float fadingOut = popup->getFadeOut();
		if (fadingOut != 0)
		{
			// cubic ease in
			fadingOut = fadingOut - 1;
			fadingOut = Math::lerp(0, 1, fadingOut*fadingOut*fadingOut + 1);

			posY -= (popup->getSize().y() + paddingY) * fadingOut;
		}

		popup->setPosition(popup->getPosition().x(), posY, 0);
		posY += popup->getSize().y() + paddingY;
	}
}

void Window::processSongTitleNotifications()
{
	if (!Settings::getInstance()->getBool("audio.display_titles"))
		return;

	if (AudioManager::getInstance()->songNameChanged())
	{
		std::string songName = AudioManager::getInstance()->getSongName();
		if (!songName.empty())
		{
			std::unique_lock<std::mutex> lock(mNotificationMessagesLock);

			for (int i = mNotificationPopups.size() - 1; i >= 0; i--)
			{
				if (mNotificationPopups[i]->getMessage().find(_U("\uF028")) != std::string::npos)
				{
					delete mNotificationPopups[i];

					auto it = mNotificationPopups.begin();
					std::advance(it, i);
					mNotificationPopups.erase(it);
				}
			}

			lock.unlock();
			displayNotificationMessage(_U("\uF028  ") + songName); // _("Now playing: ") + 
		}

		AudioManager::getInstance()->resetSongNameChangedFlag();
	}	
}

void Window::update(int deltaTime)
{
	processPostedFunctions();
	processSongTitleNotifications();
	processNotificationMessages();

	if (mNormalizeNextUpdate)
	{
		mNormalizeNextUpdate = false;
		if (deltaTime > mAverageDeltaTime)
			deltaTime = mAverageDeltaTime;
	}

	if (mVolumeInfo)
		mVolumeInfo->update(deltaTime);

	mFrameTimeElapsed += deltaTime;
	mFrameCountElapsed++;
	if (mFrameTimeElapsed > 500)
	{
		mAverageDeltaTime = mFrameTimeElapsed / mFrameCountElapsed;

		if (Settings::getInstance()->getBool("DrawFramerate"))
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

	/* draw the clock */ // batocera
	if (Settings::getInstance()->getBool("DrawClock") && mClock) 
	{
		mClockElapsed -= deltaTime;
		if (mClockElapsed <= 0)
		{
			time_t     clockNow = time(0);
			struct tm  clockTstruct = *localtime(&clockNow);

			if (clockTstruct.tm_year > 100) 
			{ 
				// Display the clock only if year is more than 1900+100 ; rpi have no internal clock and out of the networks, the date time information has no value */
				// Visit http://en.cppreference.com/w/cpp/chrono/c/strftime for more information about date/time format
				
				char       clockBuf[32];
				strftime(clockBuf, sizeof(clockBuf), "%H:%M", &clockTstruct);
				mClock->setText(clockBuf);
			}

			mClockElapsed = 1000; // next update in 1000ms
		}
	}

	mTimeSinceLastInput += deltaTime;

	if (peekGui())
		peekGui()->update(deltaTime);

	// Update the screensaver
	if (mScreenSaver)
		mScreenSaver->update(deltaTime);

	// update pads // batocera
	if (mControllerActivity)
		mControllerActivity->update(deltaTime);

	if (mBatteryIndicator)
		mBatteryIndicator->update(deltaTime);

	updateAsyncNotifications(deltaTime);
	updateNotificationPopups(deltaTime);

	AudioManager::update(deltaTime);
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
			if ((top->getTag() == "GuiLoading") && mGuiStack.size() > 2)
			{
				mBackgroundOverlay->render(transform);

				auto& middle = mGuiStack.at(mGuiStack.size() - 2);
				if (middle != bottom)
					middle->render(transform);

				top->render(transform);
			}
			else
			{
				if ((top->isKindOf<GuiMsgBox>() || top->getTag() == "popup") && mGuiStack.size() > 2)
				{
					auto& middle = mGuiStack.at(mGuiStack.size() - 2);
					if (middle != bottom)
						middle->render(transform);
				}

				mBackgroundOverlay->render(transform);
				top->render(transform);
			}
		}
	}
	
	if (mGuiStack.size() < 2 || !Renderer::isSmallScreen())
		if(!mRenderedHelpPrompts)
			mHelp->render(transform);

	if(Settings::getInstance()->getBool("DrawFramerate") && mFrameDataText)
	{
		Renderer::setMatrix(Transform4x4f::Identity());
		mDefaultFonts.at(1)->renderTextCache(mFrameDataText.get());
	}

    // clock // batocera
	if (Settings::getInstance()->getBool("DrawClock") && mClock && (mGuiStack.size() < 2 || !Renderer::isSmallScreen()))
		mClock->render(transform);
	
	if (Settings::getInstance()->getBool("ShowControllerActivity") && mControllerActivity != nullptr && (mGuiStack.size() < 2 || !Renderer::isSmallScreen()))
		mControllerActivity->render(transform);

	if (mBatteryIndicator != nullptr && (mGuiStack.size() < 2 || !Renderer::isSmallScreen()))
		mBatteryIndicator->render(transform);

	Renderer::setMatrix(Transform4x4f::Identity());

	unsigned int screensaverTime = (unsigned int)Settings::getInstance()->getInt("ScreenSaverTime");
	if (mTimeSinceLastInput >= screensaverTime && screensaverTime != 0)
		startScreenSaver();

	// Render notifications
	if (!mRenderScreenSaver)
	{
		for (auto popup : mNotificationPopups)
			popup->render(transform);

		renderAsyncNotifications(transform);
	}
	
	// Always call the screensaver render function regardless of whether the screensaver is active
	// or not because it may perform a fade on transition
	renderScreenSaver();

	for (auto extra : mScreenExtras)
		extra->render(transform);

	if (mVolumeInfo && Settings::getInstance()->getBool("VolumePopup"))
		mVolumeInfo->render(transform);

	if(mTimeSinceLastInput >= screensaverTime && screensaverTime != 0)
	{
		if (!isProcessing() && mAllowSleep && (!mScreenSaver || mScreenSaver->allowSleep()))
		{
			// go to sleep
			if (mSleeping == false) {
				mSleeping = true;
				onSleep();
			}
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

void Window::setCustomSplashScreen(std::string imagePath, std::string customText)
{
	if (Settings::getInstance()->getBool("HideWindow"))
		return;
		
	if (!Utils::FileSystem::exists(imagePath))
		mSplash = std::make_shared<Splash>(this, DEFAULT_SPLASH_IMAGE, false);
	else
		mSplash = std::make_shared<Splash>(this, imagePath, false);

	mSplash->update(customText);
}

void Window::renderSplashScreen(std::string text, float percent, float opacity)
{
	if (mSplash == NULL)
		mSplash = std::make_shared<Splash>(this);

	mSplash->update(text, percent);
	mSplash->render(opacity);	
}

void Window::renderSplashScreen(float opacity, bool swapBuffers)
{
	if (mSplash != nullptr)
		mSplash->render(opacity, swapBuffers);
}

void Window::closeSplashScreen()
{
	mSplash = nullptr;
}

void Window::renderHelpPromptsEarly()
{
	mHelp->render(Transform4x4f::Identity());
	mRenderedHelpPrompts = true;
}

void Window::setHelpPrompts(const std::vector<HelpPrompt>& prompts, const HelpStyle& style)
{
	// Keep a temporary reference to the previous grid.
	// It avoids unloading/reloading images if they are the same, and avoids flickerings
	auto oldGrid = mHelp->getGrid();

	mHelp->clearPrompts();
	mHelp->setStyle(style);

	mClockElapsed = -1;

	std::vector<HelpPrompt> addPrompts;

	std::map<std::string, bool> inputSeenMap;
	std::map<std::string, int> mappedToSeenMap;
	for(auto it = prompts.cbegin(); it != prompts.cend(); it++)
	{
		// only add it if the same icon hasn't already been added
		if(inputSeenMap.emplace(it->first, true).second)
		{
			// this symbol hasn't been seen yet, what about the action name?
			auto mappedTo = mappedToSeenMap.find(it->second);
			if(mappedTo != mappedToSeenMap.cend())
			{
				// yes, it has!

				// can we combine? (dpad only)
				if((it->first == "up/down" && addPrompts.at(mappedTo->second).first != "left/right") ||
					(it->first == "left/right" && addPrompts.at(mappedTo->second).first != "up/down"))
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
				mappedToSeenMap.emplace(it->second, (int)addPrompts.size());
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
	Scripting::fireEvent("sleep");
}

void Window::onWake()
{
	Scripting::fireEvent("wake");
}

bool Window::isProcessing()
{
	return count_if(mGuiStack.cbegin(), mGuiStack.cend(), [](GuiComponent* c) { return c->isProcessing(); }) > 0;
}

void Window::startScreenSaver()
{
	if (mScreenSaver && !mRenderScreenSaver)
	{
		for (auto extra : mScreenExtras)
			extra->onScreenSaverActivate();

		// Tell the GUI components the screensaver is starting
		for(auto i = mGuiStack.cbegin(); i != mGuiStack.cend(); i++)
			(*i)->onScreenSaverActivate();

		mScreenSaver->startScreenSaver();
		mRenderScreenSaver = true;
	}
}

bool Window::cancelScreenSaver()
{
	if (mScreenSaver && mRenderScreenSaver)
	{		
		mScreenSaver->stopScreenSaver();
		mRenderScreenSaver = false;
		mScreenSaver->resetCounts();

		// Tell the GUI components the screensaver has stopped
		for(auto i = mGuiStack.cbegin(); i != mGuiStack.cend(); i++)
			(*i)->onScreenSaverDeactivate();

		for (auto extra : mScreenExtras)
			extra->onScreenSaverDeactivate();

		return true;
	}

	return false;
}

void Window::renderScreenSaver()
{
	if (mScreenSaver)
		mScreenSaver->renderScreenSaver();
}

AsyncNotificationComponent* Window::createAsyncNotificationComponent(bool actionLine)
{
	std::unique_lock<std::mutex> lock(mNotificationMessagesLock);

	AsyncNotificationComponent* pc = new AsyncNotificationComponent(this, actionLine);
	mAsyncNotificationComponent.push_back(pc);
	
	if (mAsyncNotificationComponent.size() == 1)
		PowerSaver::pause();

	return pc;
}

void Window::renderAsyncNotifications(const Transform4x4f& trans)
{
	std::unique_lock<std::mutex> lock(mNotificationMessagesLock);

#define PADDING_H  (Renderer::getScreenWidth()*0.01)

	float posY = Renderer::getScreenHeight() * 0.02f;

	bool first = true;
	for (auto child : mAsyncNotificationComponent)
	{		
		float posX = Renderer::getScreenWidth()*0.99f - child->getSize().x();

		float offset = child->getSize().y() + PADDING_H;

		float fadingOut = child->getFading();
		if (fadingOut != 0)
		{
			// cubic ease in
			fadingOut = fadingOut - 1;
			fadingOut = Math::lerp(0, 1, fadingOut*fadingOut*fadingOut + 1);
						
			if (child->isClosing())
			{
				child->setPosition(posX, posY - (offset * fadingOut), 0);
				offset = offset - (offset * fadingOut);

				auto sz = child->getSize();
				Renderer::pushClipRect(Vector2i(
					(int)trans.translation()[0] + posX - PADDING_H, 
					(int)trans.translation()[1] + (first ? 0 : posY)), 
					Vector2i(
					(int)sz.x() + 2 * PADDING_H, 
					(int)sz.y() + (first ? posY : 0)));
			}
			else 
				child->setPosition(posX + (child->getSize().x() * (1.0 - fadingOut)), posY, 0);
		}
		else 
			child->setPosition(posX, posY, 0);

		child->render(trans);

		if (fadingOut != 0 && child->isClosing())
			Renderer::popClipRect();

		posY += offset;
		first = false;
	}
}

void Window::updateAsyncNotifications(int deltaTime)
{
	std::unique_lock<std::mutex> lock(mNotificationMessagesLock);

	bool changed = false;

	for (int i = mAsyncNotificationComponent.size() - 1; i >= 0; i--)
	{
		mAsyncNotificationComponent[i]->update(deltaTime);

		if (!mAsyncNotificationComponent[i]->isRunning())
		{
			delete mAsyncNotificationComponent[i];

			auto it = mAsyncNotificationComponent.begin();
			std::advance(it, i);
			mAsyncNotificationComponent.erase(it);

			changed = true;
		}
	}

	if (changed && mAsyncNotificationComponent.size() == 0)
		PowerSaver::resume();
}

void Window::postToUiThread(const std::function<void(Window*)>& func)
{	
	std::unique_lock<std::mutex> lock(mNotificationMessagesLock);

	mFunctions.push_back(func);	
}

void Window::processPostedFunctions()
{
	std::unique_lock<std::mutex> lock(mNotificationMessagesLock);

	for (auto func : mFunctions)
		func(this);	

	mFunctions.clear();
}

void Window::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	for (auto extra : mScreenExtras)
		delete extra;

	mScreenExtras.clear();
	mScreenExtras = ThemeData::makeExtras(theme, "screen", this);

	std::stable_sort(mScreenExtras.begin(), mScreenExtras.end(), [](GuiComponent* a, GuiComponent* b) { return b->getZIndex() > a->getZIndex(); });

	if (mBackgroundOverlay)
		mBackgroundOverlay->setImage(ThemeData::getMenuTheme()->Background.fadePath);

	if (mClock)
	{
		mClock->setFont(Font::get(FONT_SIZE_SMALL));
		mClock->setColor(0x777777FF);		
		mClock->setHorizontalAlignment(ALIGN_RIGHT);
		mClock->setVerticalAlignment(ALIGN_TOP);
		
		// if clock element does not exist in screen view -> <view name="screen"><text name="clock"> 
		// skin it from system.helpsystem -> <view name="system"><helpsystem name="help"> )
		if (!theme->getElement("screen", "clock", "text"))
		{
			auto elem = theme->getElement("system", "help", "helpsystem");
			if (elem && elem->has("textColor"))
				mClock->setColor(elem->get<unsigned int>("textColor"));

			if (elem && (elem->has("fontPath") || elem->has("fontSize")))
				mClock->setFont(Font::getFromTheme(elem, ThemeFlags::ALL, Font::get(FONT_SIZE_MEDIUM)));
		}
		
		mClock->setPosition(Renderer::getScreenWidth()*0.94, Renderer::getScreenHeight()*0.9965 - mClock->getFont()->getHeight());
		mClock->setSize(Renderer::getScreenWidth()*0.05, 0);

		mClock->applyTheme(theme, "screen", "clock", ThemeFlags::ALL ^ (ThemeFlags::TEXT));
	}

	if (mControllerActivity)
		mControllerActivity->applyTheme(theme, "screen", "controllerActivity", ThemeFlags::ALL ^ (ThemeFlags::TEXT));

	if (mBatteryIndicator)
		mBatteryIndicator->applyTheme(theme, "screen", "batteryIndicator", ThemeFlags::ALL);
	
	mVolumeInfo = std::make_shared<VolumeInfoComponent>(this);
}
