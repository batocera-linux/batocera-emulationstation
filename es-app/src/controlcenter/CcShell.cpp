#include "controlcenter/CcShell.h"

#include "Window.h"
#include "Log.h"
#include "utils/StringUtil.h"

#include <SDL_timer.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

CcShell::CcShell(Window* window, int threads) : mWindow(window), mStop(false)
{
	mAlive = std::make_shared<bool>(true);
	mActions = std::make_shared<ActionQueue>();

	for (int i = 0; i < threads; i++)
		mThreads.push_back(std::thread(&CcShell::worker, this));
}

CcShell::~CcShell()
{
	*mAlive = false;
	mWindow->unregisterPostedFunctions(this);

	{
		std::unique_lock<std::mutex> lock(mLock);
		mStop = true;
		mJobs.clear();
	}
	mCond.notify_all();

	for (auto& t : mThreads)
		t.join();
}

void CcShell::worker()
{
	while (true)
	{
		std::function<void()> job;

		{
			std::unique_lock<std::mutex> lock(mLock);
			mCond.wait(lock, [this] { return mStop || !mJobs.empty(); });
			if (mStop)
				return;

			job = mJobs.front();
			mJobs.pop_front();
		}

		job();
	}
}

void CcShell::enqueue(const std::function<void()>& job)
{
	{
		std::unique_lock<std::mutex> lock(mLock);
		mJobs.push_back(job);
	}
	mCond.notify_one();
}

void CcShell::post(const std::function<void()>& func)
{
	auto alive = mAlive;
	mWindow->postToUiThread([alive, func]() { if (*alive) func(); }, this);
}

bool CcShell::hasCommands(const std::string& tmpl)
{
	return tmpl.find("${") != std::string::npos;
}

// brace depth is tracked: commands hold awk programs and shell groups
template<typename F> static std::string expandWith(const std::string& s, F resolve)
{
	if (s.find("${") == std::string::npos)
		return s;

	std::string out;
	size_t i = 0, n = s.size();
	while (i < n)
	{
		if (i + 1 < n && s[i] == '$' && s[i + 1] == '{')
		{
			size_t start = i;
			i += 2;
			int depth = 1;
			size_t cmdStart = i;
			while (i < n && depth > 0)
			{
				if (s[i] == '{') depth++;
				else if (s[i] == '}') depth--;
				i++;
			}

			if (depth != 0)
			{
				out += s.substr(start);
				break;
			}

			out += resolve(Utils::String::trim(s.substr(cmdStart, i - 1 - cmdStart)));
		}
		else
			out += s[i++];
	}
	return out;
}

std::vector<std::string> CcShell::extractCommands(const std::string& tmpl)
{
	std::vector<std::string> ret;
	expandWith(tmpl, [&ret](const std::string& c) { ret.push_back(c); return std::string(); });
	return ret;
}

std::string CcShell::capture(const std::string& cmd, int timeoutMs)
{
	int fds[2];
	if (pipe(fds) != 0)
		return "";

	pid_t pid = fork();
	if (pid < 0)
	{
		close(fds[0]);
		close(fds[1]);
		return "";
	}

	if (pid == 0)
	{
		setpgid(0, 0);
		dup2(fds[1], STDOUT_FILENO);
		int devnull = open("/dev/null", O_RDWR);
		if (devnull >= 0)
		{
			dup2(devnull, STDIN_FILENO);
			dup2(devnull, STDERR_FILENO);
		}
		close(fds[0]);
		close(fds[1]);
		execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)NULL);
		_exit(127);
	}

	close(fds[1]);

	std::string out;
	char buf[4096];
	unsigned int deadline = SDL_GetTicks() + timeoutMs;
	while (true)
	{
		int left = (int)(deadline - SDL_GetTicks());
		if (left <= 0)
		{
			LOG(LogWarning) << "ControlCenter: command timed out: " << cmd;
			kill(-pid, SIGKILL);
			break;
		}

		struct pollfd pfd = { fds[0], POLLIN, 0 };
		if (poll(&pfd, 1, left) <= 0)
			continue;

		ssize_t r = read(fds[0], buf, sizeof(buf));
		if (r <= 0)
			break;

		out.append(buf, r);
	}

	close(fds[0]);
	waitpid(pid, nullptr, 0);

	while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' '))
		out.pop_back();

	if (Utils::String::toLower(out) == "null")
		return "";

	return out;
}

std::string CcShell::resolve(const std::string& cmd, int ttlMs)
{
	std::shared_ptr<Pending> pending;
	bool owner = false;

	{
		std::unique_lock<std::mutex> lock(mCacheLock);
		auto it = mCache.find(cmd);
		if (it != mCache.cend() && ttlMs > 0 && SDL_GetTicks() - it->second.time < (unsigned int)ttlMs)
			return it->second.value;

		auto running = mInflight.find(cmd);
		if (running != mInflight.cend())
			pending = running->second;
		else
		{
			pending = std::make_shared<Pending>();
			mInflight[cmd] = pending;
			owner = true;
		}
	}

	if (!owner)
	{
		std::unique_lock<std::mutex> lock(pending->lock);
		pending->cond.wait(lock, [pending] { return pending->done; });
		return pending->value;
	}

	std::string value = capture(cmd);

	{
		std::unique_lock<std::mutex> lock(mCacheLock);
		mCache[cmd] = { value, SDL_GetTicks() };
		mInflight.erase(cmd);
	}

	{
		std::unique_lock<std::mutex> lock(pending->lock);
		pending->value = value;
		pending->done = true;
	}
	pending->cond.notify_all();
	return value;
}

std::string CcShell::expandNow(const std::string& tmpl, int ttlMs)
{
	return expandWith(tmpl, [this, ttlMs](const std::string& c) { return resolve(c, ttlMs); });
}

void CcShell::expand(const std::string& tmpl, int ttlMs, const std::function<void(const std::string&)>& onResult)
{
	if (!hasCommands(tmpl))
	{
		onResult(tmpl);
		return;
	}

	enqueue([this, tmpl, ttlMs, onResult]()
	{
		std::string value = expandNow(tmpl, ttlMs);
		post([onResult, value]() { onResult(value); });
	});
}

// In order, without an output pipe: a backgrounded job must not hold a worker or hit the timeout.
struct CcShell::ActionQueue
{
	std::mutex lock;
	std::condition_variable cond;
	std::deque<std::function<void()>> jobs;
	bool running = false;
};

void CcShell::run(const std::string& cmd, const std::function<void()>& onDone)
{
	LOG(LogInfo) << "ControlCenter: run " << cmd;

	auto alive = mAlive;
	auto window = mWindow;
	void* owner = this;

	auto job = [alive, window, owner, cmd, onDone]()
	{
		pid_t pid = fork();
		if (pid == 0)
		{
			int devnull = open("/dev/null", O_RDWR);
			if (devnull >= 0)
			{
				dup2(devnull, STDIN_FILENO);
				dup2(devnull, STDOUT_FILENO);
				dup2(devnull, STDERR_FILENO);
			}
			execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)NULL);
			_exit(127);
		}

		if (pid > 0)
			waitpid(pid, nullptr, 0);

		if (onDone && *alive)
			window->postToUiThread([alive, onDone]() { if (*alive) onDone(); }, owner);
	};

	auto queue = mActions;
	std::unique_lock<std::mutex> lock(queue->lock);
	queue->jobs.push_back(job);
	if (!queue->running)
	{
		queue->running = true;
		std::thread([queue]()
		{
			while (true)
			{
				std::function<void()> next;
				{
					std::unique_lock<std::mutex> lock(queue->lock);
					if (queue->jobs.empty())
					{
						queue->running = false;
						return;
					}
					next = queue->jobs.front();
					queue->jobs.pop_front();
				}
				next();
			}
		}).detach();
	}
}

void CcShell::invalidate(const std::string& tmpl)
{
	std::unique_lock<std::mutex> lock(mCacheLock);
	for (auto& cmd : extractCommands(tmpl))
		mCache.erase(cmd);
}
