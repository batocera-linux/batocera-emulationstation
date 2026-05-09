#include "ThreadPool.h"

#if WIN32
#include <Windows.h>
#endif

#include <stdlib.h>
#include <algorithm>
#include <chrono>

namespace Utils
{
	ThreadPool::ThreadPool(int threadByCore) : mRunning(false), mWaiting(false), mNumWork(0)
	{
		mThreadByCore = threadByCore;
	}

	void ThreadPool::start()
	{
		mRunning = true;

		size_t num_threads = mThreadByCore < 0 ? abs(mThreadByCore) : std::thread::hardware_concurrency() * mThreadByCore;

		auto doWork = [&](size_t id)
		{
#if WIN32
			SetThreadDescription(GetCurrentThread(), L"ThreadPool::thread");

			auto mask = (static_cast<DWORD_PTR>(1) << id);
			SetThreadAffinityMask(GetCurrentThread(), mask);
#endif

			while (mRunning)
			{
				_mutex.lock();
				if (!mWorkQueue.empty())
				{
					auto entry = mWorkQueue.front();
					mWorkQueue.pop();
					_mutex.unlock();

					try
					{
						entry.fn();
					}
					catch (...) {}

					entry.item->mDone.store(true);
					mNumWork--;
				}
				else
				{
					_mutex.unlock();

					// Extra code : Exit finished threads
					if (mWaiting)
						return;

					std::this_thread::yield();
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
			}
		};

		mThreads.reserve(num_threads);

		for (size_t i = 0; i < num_threads; i++)
			mThreads.push_back(std::thread(doWork, i));
	}

	ThreadPool::~ThreadPool()
	{
		mRunning = false;

		for (std::thread& t : mThreads)
			if (t.joinable())
				t.join();
	}

	WorkItemPtr  ThreadPool::queueWorkItem(work_function work)
	{
		auto item = std::make_shared<WorkItem>();

		_mutex.lock();
		mWorkQueue.push({ work, item });
		mAllItems.push_back(item);
		mNumWork++;
		_mutex.unlock();

		return item;
	}
	
	void ThreadPool::wait()
	{
		if (!mRunning)
			start();

		mWaiting = true;
		while (mNumWork.load() > 0)
			std::this_thread::yield();

		_mutex.lock();
		mAllItems.clear();
		_mutex.unlock();
	}

	void ThreadPool::wait(work_function work, int delay)
	{
		if (!mRunning)
			start();

		mWaiting = true;

		while (mNumWork.load() > 0)
		{
			work();

			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::milliseconds(delay));
		}		

		_mutex.lock();
		mAllItems.clear();
		_mutex.unlock();

	}

	void WorkItem::wait() const
	{
		while (!mDone.load())
		{
			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	void ThreadPool::cleanupDoneItems()
	{
		_mutex.lock();

		mAllItems.erase(
			std::remove_if(mAllItems.begin(), mAllItems.end(), [](const std::shared_ptr<WorkItem>& item) { return item->isDone(); }), 
			mAllItems.end());

		_mutex.unlock();
	}

	void ThreadPool::waitAll(std::initializer_list<WorkItemPtr> items)
	{
		if (!mRunning)
			start();

		for (const auto& item : items)
			item->wait();

		cleanupDoneItems();
	}

	void ThreadPool::waitAllExcept(WorkItemPtr excluded)
	{
		waitAllExcept({ excluded });
	}

	void ThreadPool::waitAllExcept(std::initializer_list<WorkItemPtr> excluded)
	{
		if (!mRunning)
			start();

		std::vector<WorkItemPtr> toWait;

		_mutex.lock();
		for (auto& item : mAllItems)
		{
			bool isExcluded = false;
			for (const auto& ex : excluded)
			{
				if (ex == item)
				{
					isExcluded = true; 
					break;
				}
			}

			if (!isExcluded)
				toWait.push_back(item);
		}
		_mutex.unlock();

		for (const auto& item : toWait)
			item->wait();

		cleanupDoneItems();
	}

	void ThreadPool::stop()
	{
		_mutex.lock();

		while (!mWorkQueue.empty())
		{
			mNumWork--;
			mWorkQueue.pop();
		}
		
		_mutex.unlock();
		mWaiting = true;

		while (mNumWork.load() > 0)
		{
			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
}
