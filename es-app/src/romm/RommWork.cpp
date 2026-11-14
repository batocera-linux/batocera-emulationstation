#include "romm/RommWork.h"
#include "Log.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#ifdef __linux__
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace Romm
{
	static std::atomic<bool> sGameRunning(false);
	static std::atomic<bool> sPaused(false);
	static std::atomic<bool> sAborted(false);

	static double loadAverage()
	{
#ifdef __linux__
		FILE* f = fopen("/proc/loadavg", "r");
		if (f != nullptr)
		{
			double load = -1;
			if (fscanf(f, "%lf", &load) != 1)
				load = -1;

			fclose(f);
			return load;
		}
#endif
		return -1; // unknown
	}

	int backgroundWorkers()
	{
		int cores = (int)std::thread::hardware_concurrency();
		if (cores <= 0)
			cores = 1;

		// Leave a core to the UI, never more than 4 connections to the server
		int workers = std::max(1, std::min(4, cores - 1));

		double load = loadAverage();
		if (load >= cores * 0.75)
			workers = 1;
		else if (load >= cores * 0.5)
			workers = std::max(1, workers / 2);

		return workers;
	}

	void lowerThreadPriority()
	{
#ifdef __linux__
		// On Linux, the nice value is per thread when given a thread id
		setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), 10);
#endif
	}

	void setGameRunning(bool running)
	{
		sGameRunning = running;
	}

	bool isGameRunning()
	{
		return sGameRunning;
	}

	void setBackgroundPaused(bool paused) { sPaused = paused; }
	bool isBackgroundPaused() { return sPaused; }
	void setBackgroundAborted(bool aborted) { sAborted = aborted; }
	bool isBackgroundAborted() { return sAborted; }

	void waitWhileGameRunning(const std::atomic<bool>& stop)
	{
		while ((sGameRunning || sPaused) && !stop && !sAborted)
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	void runInBackground(size_t count, int workers, const std::function<void(size_t)>& work, const std::atomic<bool>& stop)
	{
		if (count == 0)
			return;

		workers = std::max(1, std::min(workers, (int)count));
		std::atomic<size_t> next(0);

		auto loop = [&]()
		{
			lowerThreadPriority();

			while (!stop && !sAborted)
			{
				waitWhileGameRunning(stop);
				if (stop || sAborted)
					break;

				size_t i = next++;
				if (i >= count)
					break;

				work(i);
			}
		};

		std::vector<std::thread> threads;
		for (int w = 1; w < workers; w++)
			threads.push_back(std::thread(loop));

		loop(); // the calling thread works too

		for (auto& t : threads)
			t.join();
	}
}
