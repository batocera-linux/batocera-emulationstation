#pragma once
#ifndef ES_APP_CONTROLCENTER_CCSHELL_H
#define ES_APP_CONTROLCENTER_CCSHELL_H

#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class Window;

// Runs the control center commands off the UI thread, results are posted back to it.
class CcShell
{
public:
	CcShell(Window* window, int threads = 3);
	~CcShell();

	void expand(const std::string& tmpl, int ttlMs, const std::function<void(const std::string&)>& onResult);

	void run(const std::string& cmd, const std::function<void()>& onDone = nullptr);

	void invalidate(const std::string& tmpl);

	static bool hasCommands(const std::string& tmpl);
	static std::vector<std::string> extractCommands(const std::string& tmpl);
	static std::string capture(const std::string& cmd, int timeoutMs = 3000);

private:
	struct CacheEntry
	{
		std::string value;
		unsigned int time;
	};

	void worker();
	void enqueue(const std::function<void()>& job);
	std::string resolve(const std::string& cmd, int ttlMs);
	std::string expandNow(const std::string& tmpl, int ttlMs);
	void post(const std::function<void()>& func);

	struct ActionQueue;

	Window* mWindow;
	std::shared_ptr<bool> mAlive;
	std::shared_ptr<ActionQueue> mActions;

	std::mutex mLock;
	std::condition_variable mCond;
	std::deque<std::function<void()>> mJobs;
	std::vector<std::thread> mThreads;
	bool mStop;

	struct Pending
	{
		std::mutex lock;
		std::condition_variable cond;
		std::string value;
		bool done = false;
	};

	std::mutex mCacheLock;
	std::map<std::string, CacheEntry> mCache;
	std::map<std::string, std::shared_ptr<Pending>> mInflight;
};

#endif
