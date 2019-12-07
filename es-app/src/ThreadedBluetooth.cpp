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
	mWndNotification = new AsyncNotificationComponent(window, false);
	mWindow->registerNotificationComponent(mWndNotification);
	mWndNotification->updateTitle(ICONINDEX + _("SCANNING BLUETOOTH"));	
	mWndNotification->updateText(_("Searching controllers..."));

	mHandle = new std::thread(&ThreadedBluetooth::run, this);
}

ThreadedBluetooth::~ThreadedBluetooth()
{
	mWindow->unRegisterNotificationComponent(mWndNotification);
	delete mWndNotification;

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
#if WIN32
	std::this_thread::sleep_for(std::chrono::milliseconds(10000));
#endif

	bool success = ApiSystem::getInstance()->scanNewBluetooth([this](const std::string info)
	{
		updateNotificationComponentContent(info);
	});

	if (success)
		mWindow->postToUiThread([](Window* w) { w->pushGui(new GuiMsgBox(w, _("CONTROLLER PAIRED"))); });
	else
		mWindow->postToUiThread([](Window* w) { w->pushGui(new GuiMsgBox(w, _("UNABLE TO PAIR CONTROLLER"), "OK", nullptr, ICON_ERROR)); });

	delete this;
	ThreadedBluetooth::mInstance = nullptr;
}

void ThreadedBluetooth::start(Window* window)
{
	if (ThreadedBluetooth::mInstance != nullptr)
	{
		window->pushGui(new GuiMsgBox(window, _("BLUETOOTH SCANNING IS ALREADY RUNNING.")));
		return;
	}
	
	ThreadedBluetooth::mInstance = new ThreadedBluetooth(window);
}
