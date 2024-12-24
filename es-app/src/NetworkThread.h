#pragma once

#include "Window.h"
#include <thread>
#include <condition_variable>
#include <mutex>
#include <list>

#include "InputManager.h"
#include "ApiSystem.h"
#include "watchers/WatchersManager.h"

class CheckPadsBatteryLevelComponent : public IWatcher
{
public:
	CheckPadsBatteryLevelComponent();

	std::vector<PadInfo>& getPadsInfo() { return mPadsInfo; }

protected:
	bool enabled() override { return mEnabled; };
	int  updateTime() override { return 15 * 1000; } // 15 seconds

	bool check() override;

private:
	std::vector<PadInfo> mPadsInfo;
	bool mEnabled;
};

class CheckUpdatesComponent : public IWatcher
{
public:
	CheckUpdatesComponent() { mEnabled = true; }

	std::string& getLastUpdateMessage() { return mLastUpdateMessage; }

protected:
	bool enabled() override;
	int  updateTime() override { return 60 * 60 * 1000; } // 60 minutes
	bool check() override;

private:
	std::string mLastUpdateMessage;
	bool mEnabled;
};

class CheckCheevosTokenComponent : public IWatcher
{
public:
	std::string& getLastToken() { return mLastToken; }

protected:
	bool enabled() override;
	int  updateTime() override { return 120 * 60 * 1000; } // 120 minutes
	int  initialUpdateTime() override { return 100; } // 100ms
	bool check() override;

private:
	std::string mLastToken;
};

class NetworkThread : public IJoystickChangedEvent, public IWatcherNotify
{
public:
	NetworkThread(Window * window);

public:
	void onJoystickChanged() override;

private:
	void OnWatcherChanged(IWatcher* component) override;

	CheckPadsBatteryLevelComponent					mCheckPadsBatteryLevelComponent;
	CheckUpdatesComponent							mCheckUpdatesComponent;
	CheckCheevosTokenComponent						mCheckCheevosTokenComponent;
	Window* mWindow;
};


