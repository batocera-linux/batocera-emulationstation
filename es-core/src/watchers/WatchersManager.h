#pragma once

#include <thread>
#include <condition_variable>
#include <mutex>
#include <list>
#include <chrono>

class WatchersManager;

class IWatcher
{
	friend class WatchersManager;

protected:
	virtual bool enabled() = 0;
	virtual int  updateTime() = 0;
	virtual bool check() = 0;

	virtual int  initialUpdateTime() { return 5 * 1000; } // 15 seconds
};

class IWatcherNotify
{
public:
	virtual void OnWatcherChanged(IWatcher* component) = 0;
};

class WatchersManager
{
public:
	template <typename T>
	static T* GetComponent()
	{
		std::unique_lock<std::mutex> lock(mWatchersLock);

		for (auto cp : mWatchers)
		{
			T* sgview = dynamic_cast<T*>(cp->component);
			if (sgview != nullptr)
				return sgview;
		}

		return nullptr;
	}

	static void RegisterComponent(IWatcher* instance);
	static void UnregisterComponent(IWatcher* instance);

	static void RegisterNotify(IWatcherNotify* instance);
	static void UnregisterNotify(IWatcherNotify* instance);

	static void ResetComponent(IWatcher* instance);

	static WatchersManager* getInstance();
	static void             stop();

	virtual ~WatchersManager();

private:
	WatchersManager();

	static WatchersManager* mInstance;

	struct WatcherInfo
	{
		WatcherInfo() { component = nullptr; nextCheckTime = std::chrono::steady_clock::now(); }

		IWatcher*	component;
		std::chrono::time_point<std::chrono::steady_clock>	nextCheckTime;
	};

	void NotifyComponentChanged(IWatcher* component);

	static std::mutex								mWatchersLock;
	static std::list<WatcherInfo*>					mWatchers;

	static std::mutex								mNotificationLock;
	static std::list<IWatcherNotify*>				mNotification;

	std::mutex										mThreadLock;
	std::condition_variable							mEvent;
	bool											mRunning;
	
	std::thread*									mThread;

	void run();	
};


