#pragma once

#include <thread>
#include "components/AsyncNotificationComponent.h"

class ThreadedBluetooth
{
public:
	static void start(Window* window);
	static bool isRunning() { return mInstance != nullptr; }

private:
	void run();
	void updateNotificationComponentContent(const std::string info);

	ThreadedBluetooth(Window* window);
	~ThreadedBluetooth();

	Window*						mWindow;
	AsyncNotificationComponent* mWndNotification;

	std::thread*				mHandle;
	static ThreadedBluetooth*	mInstance;
};

