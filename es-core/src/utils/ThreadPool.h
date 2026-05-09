#ifndef __THREADPOOL
#define __THREADPOOL

#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>
#include <initializer_list>

namespace Utils
{
	class WorkItem
	{
	public:
		WorkItem() : mDone(false) {}

		bool isDone() const { return mDone.load(); }
		void wait() const;

	private:
		friend class ThreadPool;
		std::atomic<bool> mDone;
	};

	using WorkItemPtr = std::shared_ptr<WorkItem>;

	class ThreadPool
	{
	public:
		typedef std::function<void(void)> work_function;

		ThreadPool(int threadByCore = 2);
		~ThreadPool();

		void start();
		WorkItemPtr queueWorkItem(work_function work);

		void wait();
		void wait(work_function work, int delay = 50);

		void waitAll(std::initializer_list<WorkItemPtr> items);

		void waitAllExcept(WorkItemPtr excluded);
		void waitAllExcept(std::initializer_list<WorkItemPtr> excluded);

		void cancel() { mRunning = false; }
		void stop();

		bool isRunning() { return mRunning; }

	private:
		bool mRunning;
		bool mWaiting;

		struct WorkEntry
		{
			work_function   fn;
			WorkItemPtr     item;
		};

		std::queue<WorkEntry> mWorkQueue;
		std::atomic<size_t> mNumWork;
		std::mutex _mutex;
		std::vector<std::thread> mThreads;
		int mThreadByCore;

		std::vector<std::shared_ptr<WorkItem>> mAllItems;
		void cleanupDoneItems();
	};
}

#endif