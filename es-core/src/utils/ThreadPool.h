#ifndef __THREADPOOL
#define __THREADPOOL

#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <functional>

namespace Utils
{
	class ThreadPool
	{
	public:
		typedef std::function<void(void)> work_function;

		ThreadPool(int threadByCore = 2);
		~ThreadPool();

		void start();
		void queueWorkItem(work_function work);
		void wait();
		void wait(work_function work, int delay = 50);
		void cancel() { mRunning = false; }
		void stop();

		bool isRunning() { return mRunning; }

	private:
		bool mRunning;
		bool mWaiting;
		std::queue<work_function> mWorkQueue;
		std::atomic<size_t> mNumWork;
		std::mutex _mutex;
		std::vector<std::thread> mThreads;
		int mThreadByCore;

	};
}

#endif
