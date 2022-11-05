#include "ThreadedBluetooth.h"
#include "Window.h"
#include "components/AsyncNotificationComponent.h"
#include "guis/GuiMsgBox.h"
#include "ApiSystem.h"
#include "LocaleES.h"

#define ICONINDEX _U("\uF085 ")

ThreadedBluetooth* ThreadedBluetooth::mInstance = nullptr;

ThreadedBluetooth::ThreadedBluetooth(Window* window)
	: mWindow(window)
{
	mWndNotification = mWindow->createAsyncNotificationComponent();
	mWndNotification->updateTitle(ICONINDEX + _("SCANNING BLUETOOTH"));	
	mWndNotification->updateText(_("Searching for devices..."));

	mHandle = new std::thread(&ThreadedBluetooth::run, this);
}

ThreadedBluetooth::~ThreadedBluetooth()
{
	mWndNotification->close();
	mWndNotification = nullptr;

	ThreadedBluetooth::mInstance = nullptr;
}

void ThreadedBluetooth::updateNotificationComponentContent(const std::string info)
{
	if (info.empty())
		return;

	mWndNotification->updatePercent(-1);
	mWndNotification->updateText(info);
}

void ThreadedBluetooth::run()
{
	ApiSystem::getInstance()->scanNewBluetooth([this](const std::string info)
	{
		updateNotificationComponentContent(info);
	});

	delete this;
	ThreadedBluetooth::mInstance = nullptr;
}

void ThreadedBluetooth::start(Window* window)
{
	if (ThreadedBluetooth::mInstance != nullptr)
	{
		window->pushGui(new GuiMsgBox(window, _("BLUETOOTH SCAN IS ALREADY RUNNING.")));
		return;
	}
	
	ThreadedBluetooth::mInstance = new ThreadedBluetooth(window);
}


// Formatter

ThreadedFormatter* ThreadedFormatter::mInstance = nullptr;

ThreadedFormatter::ThreadedFormatter(Window* window, const std::string disk, const std::string fileSystem)
	: mWindow(window)
{
	mDisk = disk;
	mFileSystem = fileSystem;

	mWndNotification = mWindow->createAsyncNotificationComponent();
	mWndNotification->updateTitle(ICONINDEX + _("FORMATTING DEVICE"));
	mWndNotification->updateText(_("Formatting") + " " + disk);

	mHandle = new std::thread(&ThreadedFormatter::run, this);
}

ThreadedFormatter::~ThreadedFormatter()
{
	mWndNotification->close();
	mWndNotification = nullptr;

	ThreadedFormatter::mInstance = nullptr;
}

void ThreadedFormatter::updateNotificationComponentContent(const std::string info)
{
	if (info.empty())
		return;

	mWndNotification->updatePercent(-1);
	mWndNotification->updateText(info);
}

void ThreadedFormatter::run()
{
#if WIN32
	std::this_thread::sleep_for(std::chrono::milliseconds(10000));
#endif

	int ret = ApiSystem::getInstance()->formatDisk(mDisk, mFileSystem, [this](const std::string info)
	{
		updateNotificationComponentContent(info);
	});

	if (ret == 69)
		mWindow->displayNotificationMessage(_("A REBOOT IS REQUIRED TO COMPLETE THE OPERATION"));

	delete this;
	ThreadedFormatter::mInstance = nullptr;
}

void ThreadedFormatter::start(Window* window, const std::string disk, const std::string fileSystem)
{
	if (ThreadedFormatter::mInstance != nullptr)
	{
		window->pushGui(new GuiMsgBox(window, _("A DRIVE IS ALREADY BEING FORMATTED.")));
		return;
	}

	ThreadedFormatter::mInstance = new ThreadedFormatter(window, disk, fileSystem);
}
