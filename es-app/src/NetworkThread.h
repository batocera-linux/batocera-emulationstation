#pragma once

#include "Window.h"
#include <thread>
#include <condition_variable>
#include <mutex>

#include "InputManager.h"

class NetworkThread : public IJoystickChangedEvent
{
public:
	NetworkThread(Window * window);
	virtual ~NetworkThread();

	void onJoystickChanged() override;

private:
	void	checkPadsBatteryLevel();

	Window*			mWindow;
	bool			mRunning;
	bool			mFirstRun;
	std::thread*	mThread;

	void run();

	std::mutex					mLock;
	std::condition_variable		mEvent;

	int				mCheckUpdateTimer;
};


