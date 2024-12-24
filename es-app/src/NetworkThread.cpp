#include "NetworkThread.h"
#include "ApiSystem.h"
#include "SystemConf.h"
#include "guis/GuiMsgBox.h"
#include "LocaleES.h"
#include "Log.h"
#include <chrono>
#include <SDL.h>

#include "watchers/BatteryLevelWatcher.h"
#include "watchers/NetworkStateWatcher.h"
#include "RetroAchievements.h"

NetworkThread::NetworkThread(Window* window) : mWindow(window)
{
	WatchersManager* mgr = WatchersManager::getInstance();

	mgr->RegisterComponent(&mCheckCheevosTokenComponent);
	mgr->RegisterComponent(new BatteryLevelWatcher());
	mgr->RegisterComponent(new NetworkStateWatcher());

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::UPGRADE))
		mgr->RegisterComponent(&mCheckUpdatesComponent);

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::PADSINFO))
		mgr->RegisterComponent(&mCheckPadsBatteryLevelComponent);

	mgr->RegisterNotify(this);

	LOG(LogDebug) << "NetworkThread : Starting";
	InputManager::joystickChanged += this;
}

CheckPadsBatteryLevelComponent::CheckPadsBatteryLevelComponent()
{
#if WIN32		
	mEnabled = false; // Windows uses SDL_JOYBATTERYUPDATED event instead
#else
	mEnabled = ApiSystem::getInstance()->isScriptingSupported(ApiSystem::PADSINFO);
#endif
}

bool CheckPadsBatteryLevelComponent::check()
{
	bool changed = false;

	auto padsInfo = ApiSystem::getInstance()->getPadsInfo();

	if (mPadsInfo.size() != padsInfo.size())
		changed = true;
	else
	{
		for (int i = 0; i < padsInfo.size(); i++)
		{
			if (padsInfo[i].status == "Charging")
				padsInfo[i].battery = -1;

			if (mPadsInfo[i].battery != padsInfo[i].battery || mPadsInfo[i].status != padsInfo[i].status || mPadsInfo[i].id != padsInfo[i].id || mPadsInfo[i].name != padsInfo[i].name || mPadsInfo[i].path != padsInfo[i].path || mPadsInfo[i].device != padsInfo[i].device)
			{
				changed = true;
				break;
			}
		}
	}

	mPadsInfo = padsInfo;
	return changed;
}

bool CheckUpdatesComponent::enabled()
{
	return mEnabled && SystemConf::getInstance()->getBool("updates.enabled") && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::UPGRADE);
}

bool CheckUpdatesComponent::check()
{
	LOG(LogDebug) << "CheckUpdatesComponent : Checking for updates";

	std::vector<std::string> msgtbl;
	if (ApiSystem::getInstance()->canUpdate(msgtbl))
	{
		std::string msg = "";
		for (int i = 0; i < msgtbl.size(); i++)
		{
			if (i != 0) msg += "\n";
			msg += msgtbl[i];
		}

		LOG(LogDebug) << "CheckUpdatesComponent : Update available " << msg.c_str();
		mLastUpdateMessage = msg;
		mEnabled = false;
		return true;
	}
	else
		LOG(LogDebug) << "NetworkThread : No update found";

	return false;
}

bool CheckCheevosTokenComponent::enabled()
{
	return SystemConf::getInstance()->getBool("global.retroachievements");
};

bool CheckCheevosTokenComponent::check()
{
	if (!enabled())
		return false;

	std::string cheevosUsername = SystemConf::getInstance()->get("global.retroachievements.username");
	std::string cheevosPassword = SystemConf::getInstance()->get("global.retroachievements.password");
	
	if (cheevosUsername.empty() || cheevosPassword.empty())
		return false;

	std::string tokenOrError;
	if (RetroAchievements::testAccount(cheevosUsername, cheevosPassword, tokenOrError))
	{
		if (tokenOrError == SystemConf::getInstance()->get("global.retroachievements.token"))
		{
			LOG(LogInfo) << "[CheckCheevosTokenComponent] Cheevos token is unchanged.";
		}
		else
		{
			LOG(LogInfo) << "[CheckCheevosTokenComponent] Generated a new cheevos token, saving.";
			mLastToken = tokenOrError;
			return true;
		}
	}
	else
	{
		LOG(LogError) << "[CheckCheevosTokenComponent] Failed to generate a new cheevos token: " << tokenOrError;		
	}

	return false;
}


void NetworkThread::onJoystickChanged()
{
	WatchersManager::getInstance()->ResetComponent(&mCheckPadsBatteryLevelComponent);
}

void NetworkThread::OnWatcherChanged(IWatcher* component)
{
	if (component == &mCheckUpdatesComponent)
	{
		mWindow->displayNotificationMessage(_U("\uF019  ") + _("UPDATE AVAILABLE") + std::string(": ") + mCheckUpdatesComponent.getLastUpdateMessage());
		return;
	}

	if (component == &mCheckPadsBatteryLevelComponent)
	{
		auto pads = mCheckPadsBatteryLevelComponent.getPadsInfo();

		mWindow->postToUiThread([pads]() { for (auto pad : pads) InputManager::getInstance()->updateBatteryLevel(pad.id, pad.device, pad.path, pad.battery); });
		return;
	}

	if (component == &mCheckCheevosTokenComponent)
	{
		auto token = mCheckCheevosTokenComponent.getLastToken();

		mWindow->postToUiThread([token]() 
			{ 
				SystemConf::getInstance()->set("global.retroachievements.token", token);
				SystemConf::getInstance()->saveSystemConf();
			});

		return;
	}
}