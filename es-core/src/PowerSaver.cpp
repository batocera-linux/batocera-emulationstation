#include "PowerSaver.h"

#include "AudioManager.h"
#include "Settings.h"
#include <SDL.h>
#include "math/Misc.h"

bool PowerSaver::mState = false;
bool PowerSaver::mRunningScreenSaver = false;

int PowerSaver::mWakeupTimeout = -1;
int PowerSaver::mScreenSaverTimeout = -1;
PowerSaver::mode PowerSaver::mMode = PowerSaver::DISABLED;

bool PowerSaver::mHasPushedEvent = false;
int PowerSaver::mPushEventID = -1;
int PowerSaver::mPauseCounter = 0;

void PowerSaver::pushRefreshEvent()
{
	if (mHasPushedEvent || !mState)
		return;

	if (mPushEventID == -1)
		mPushEventID = SDL_RegisterEvents(1);

	SDL_Event ev;
	ev.type = mPushEventID;
	SDL_PushEvent(&ev);
}

void PowerSaver::resetRefreshEvent()
{
	mHasPushedEvent = false;
}

void PowerSaver::init()
{
	setState(true);
	updateMode();
}

int PowerSaver::getTimeout()
{
	 if (mMode == INSTANT || mMode == ENHANCED)
		 return mRunningScreenSaver ? mWakeupTimeout : mScreenSaverTimeout;

	 return 40; // 1000;
}

void PowerSaver::loadWakeupTime()
{
	// TODO : Move this to Screensaver Class
	std::string behaviour = Settings::getInstance()->getString("ScreenSaverBehavior");
	if (behaviour == "random video")
		mWakeupTimeout = Settings::getInstance()->getInt("ScreenSaverSwapVideoTimeout") - getMode();
	else if (behaviour == "slideshow")
		mWakeupTimeout = Settings::getInstance()->getInt("ScreenSaverSwapImageTimeout") - getMode();
	else // Dim and Blank
		mWakeupTimeout = -1;
}

void PowerSaver::updateTimeouts()
{
	mScreenSaverTimeout = (unsigned int) Settings::ScreenSaverTime();
	mScreenSaverTimeout = mScreenSaverTimeout > 0 ? mScreenSaverTimeout - getMode() : -1;
	loadWakeupTime();
}

PowerSaver::mode PowerSaver::getMode()
{
	return mMode;
}

void PowerSaver::updateMode()
{
	std::string mode = Settings::PowerSaverMode();

	if (mode == "disabled") {
		mMode = DISABLED;
	} else if (mode == "instant") {
		mMode = INSTANT;
	} else if (mode == "enhanced") {
		mMode = ENHANCED;
	} else {
		mMode = DEFAULT;
	}
	updateTimeouts();
}

bool PowerSaver::getState()
{
	return mState;
}

void PowerSaver::setState(bool state)
{
	bool ps_enabled = Settings::PowerSaverMode() != "disabled";

	if (mState == ps_enabled && state)
		return;
	
	mState = ps_enabled && state;
	if (state)
		pushRefreshEvent(); // Exit SDL_WaitEventTimeout loop by sending an event
}

void PowerSaver::runningScreenSaver(bool state)
{
	mRunningScreenSaver = state;
	if (mWakeupTimeout < mMode)
	{
		// Disable PS if wake up time is less than mode as PS will never trigger
		setState(!state);
	}
}

bool PowerSaver::isScreenSaverActive()
{
	return mRunningScreenSaver;
}
