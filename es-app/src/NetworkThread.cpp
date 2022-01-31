#include "NetworkThread.h"
#include "ApiSystem.h"
#include "SystemConf.h"
#include "guis/GuiMsgBox.h"
#include "LocaleES.h"
#include "Log.h"
#include <chrono>

#define CHECKUPDATE_MINUTES 60

NetworkThread::NetworkThread(Window* window) : mWindow(window)
{
	if (!ApiSystem::getInstance()->isScriptingSupported(ApiSystem::UPGRADE) && !ApiSystem::getInstance()->isScriptingSupported(ApiSystem::PADSINFO))
		return;

	LOG(LogDebug) << "NetworkThread : Starting";

	// creer le thread
	mCheckUpdateTimer = CHECKUPDATE_MINUTES;
	mFirstRun = true;
	mRunning = true;
	mThread = new std::thread(&NetworkThread::run, this);

	InputManager::joystickChanged += this;
}

void NetworkThread::onJoystickChanged()
{
	mEvent.notify_one();
}

NetworkThread::~NetworkThread()
{
	if (mThread == nullptr)
		return;
	
	LOG(LogDebug) << "NetworkThread : Exit";

	mEvent.notify_all();

	mRunning = false;
	mThread->join();
	delete mThread;
	mThread = nullptr;	
}

void NetworkThread::checkPadsBatteryLevel()
{
	if (!ApiSystem::getInstance()->isScriptingSupported(ApiSystem::PADSINFO))
		return;

	auto padsInfo = ApiSystem::getInstance()->getPadsInfo();
	for (auto pad : padsInfo)
	{
		if (pad.status == "Charging")
			pad.battery = -1;

		InputManager::getInstance()->updateBatteryLevel(pad.id, pad.device, pad.battery);
	}
}

void NetworkThread::run()
{
	while (mRunning)
	{
		if (mFirstRun)
		{
			mFirstRun = false;
			checkPadsBatteryLevel();

			std::unique_lock<std::mutex> lock(mLock);
			mEvent.wait_for(lock, std::chrono::seconds(15));
		}
		else
		{			
			std::unique_lock<std::mutex> lock(mLock);
			mEvent.wait_for(lock, std::chrono::minutes(5));
			mCheckUpdateTimer += 1;
		}
		
		checkPadsBatteryLevel();

		if (mCheckUpdateTimer >= CHECKUPDATE_MINUTES && SystemConf::getInstance()->getBool("updates.enabled") && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::UPGRADE))
		{
			mCheckUpdateTimer = 0;

			LOG(LogDebug) << "NetworkThread : Checking for updates";

			std::vector<std::string> msgtbl;
			if (ApiSystem::getInstance()->canUpdate(msgtbl))
			{
				std::string msg = "";
				for (int i = 0; i < msgtbl.size(); i++)
				{
					if (i != 0) msg += "\n";
					msg += msgtbl[i];
				}

				LOG(LogDebug) << "NetworkThread : Update available " << msg.c_str();
				mWindow->displayNotificationMessage(_U("\uF019  ") + _("UPDATE AVAILABLE") + std::string(": ") + msg);
				mRunning = false;
			}
			else
				LOG(LogDebug) << "NetworkThread : No update found";
		}
	}
}