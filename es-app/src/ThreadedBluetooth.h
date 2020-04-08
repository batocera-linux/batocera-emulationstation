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

class ThreadedFormatter
{
public:
	static void start(Window* window, const std::string disk, const std::string fileSystem);
	static bool isRunning() { return mInstance != nullptr; }

private:
	std::string mDisk;
	std::string mFileSystem;

	void run();
	void updateNotificationComponentContent(const std::string info);

	ThreadedFormatter(Window* window, const std::string disk, const std::string fileSystem);
	~ThreadedFormatter();

	Window*						mWindow;
	AsyncNotificationComponent* mWndNotification;

	std::thread*				mHandle;
	static ThreadedFormatter*	mInstance;
};
