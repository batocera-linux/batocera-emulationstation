#include "Log.h"

#include "utils/FileSystemUtil.h"
#include "utils/Platform.h"
#include "Settings.h"
#include "Paths.h"

#include <iostream>
#include <mutex>
#include <SDL_timer.h>
#include <iomanip> 
#include <fstream>
#include <iomanip>
#include <csignal>

#include <thread>
#include <condition_variable>

#if WIN32
#include <Windows.h>
#endif

LogLevel Log::mReportingLevel = (LogLevel) -1;
bool     Log::mEnabled = false;

static thread_local std::ostringstream tl_stream;

class AsyncLogger
{
public:
    static AsyncLogger& instance()
    {
        static AsyncLogger inst;
        return inst;
    }

    struct Entry
    {
        LogLevel    level;
        std::string msg;  
    };

    void open(const std::string& path, LogLevel level, bool debugToStderr)
    {
        mFile.open(path, std::ios::out | std::ios::trunc);
        if (!mFile.is_open()) 
        {
            std::cerr << "[Log] Cannot open: " << path << "\n";
            return;
        }

        mBufA.reserve(256);
        mBufB.reserve(256);

        mLevel.store(level, std::memory_order_relaxed);
        mDebugToStderr = debugToStderr;
        mShutdown = false;
        mEnabled.store(true, std::memory_order_release);
        mThread = std::thread(&AsyncLogger::workerLoop, this);
    }

    void close()
    {
        if (!mEnabled.load(std::memory_order_acquire)) 
            return;

        mEnabled.store(false, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lk(mMutex);
            mShutdown = true;
        }

        mCV.notify_one();

        if (mThread.joinable()) 
            mThread.join();

        drainBuffer();
        mFile.flush();
        mFile.close();
    }

    void emergencyFlush()
    {
        if (!mEnabled.load(std::memory_order_acquire))
            return;

        if (!mFile.is_open())
            return;

        mEmergency.store(true, std::memory_order_release);
        mCV.notify_one();

        for (int i = 0; i < 200 && mEmergency.load(std::memory_order_acquire); ++i)
            std::this_thread::sleep_for(std::chrono::microseconds(50));

        mFile.flush();
    }

    void enqueue(LogLevel level, std::string&& msg)
    {
        {
            std::lock_guard<std::mutex> lk(mMutex);
            mWriteBuf->push_back({ level, std::move(msg) });
        }
        mCV.notify_one();
    }

    void     setLevel(LogLevel l) { mLevel.store(l, std::memory_order_relaxed); }
    LogLevel getLevel()     const { return mLevel.load(std::memory_order_relaxed); }
    bool     isEnabled()    const { return mEnabled.load(std::memory_order_relaxed); }

private:
    AsyncLogger() : mWriteBuf(&mBufA), mDrainBuf(&mBufB) {}

    void workerLoop()
    {
        while (true)
        {
            {
                std::unique_lock<std::mutex> lk(mMutex);
                mCV.wait(lk, [this] 
                    {
                    return !mWriteBuf->empty()
                        || mShutdown
                        || mEmergency.load(std::memory_order_acquire);
                    });
                
                std::swap(mWriteBuf, mDrainBuf);
            }

            drainBuffer();

            if (mEmergency.load(std::memory_order_acquire)) 
            {
                mFile.flush();
                mEmergency.store(false, std::memory_order_release);
            }
                        
            if (++mBatchCount % kFsyncEveryNBatches == 0)
                mFile.flush();

            if (mShutdown) break;
        }
    }

    void drainBuffer()
    {
        for (const auto& e : *mDrainBuf)
            writeLine(e);

        mDrainBuf->clear();
    }

    void writeLine(const Entry& e)
    {
        mFile << e.msg << '\n';

        if (e.level == LogError || mDebugToStderr) 
        {
#if WIN32
            OutputDebugStringA(e.msg.c_str());
            OutputDebugStringA("\n");
#else
            std::cerr << e.msg << '\n';
#endif
        }
    }

    static const char* levelTag(LogLevel l) 
    {
        switch (l) 
        {
        case LogError:   
            return "ERROR";
        case LogWarning: 
            return "WARNING";
        case LogDebug:   
            return "DEBUG";
        default:         
            return "INFO";
        }
    }

    std::ofstream  mFile;
    std::thread    mThread;

    // Double buffer
    std::vector<Entry>  mBufA, mBufB;
    std::vector<Entry>* mWriteBuf;
    std::vector<Entry>* mDrainBuf;

    std::mutex              mMutex;
    std::condition_variable mCV;
    bool                    mShutdown{ false };
    bool                    mDebugToStderr{ false };

    std::atomic<LogLevel>   mLevel{ LogInfo };
    std::atomic<bool>       mEnabled{ false };
    std::atomic<bool>       mEmergency{ false };

    uint32_t mBatchCount{ 0 };
 
    static constexpr uint32_t kFsyncEveryNBatches = 8;
};

static inline uint64_t getOsThreadId()
{
#if defined(_WIN32)
    return static_cast<uint64_t>(GetCurrentThreadId());
#elif defined(__linux__)
    return static_cast<uint64_t>(::gettid());
#elif defined(__APPLE__)
    uint64_t tid;
    pthread_threadid_np(nullptr, &tid);
    return tid;
#else
    return static_cast<uint64_t>(::pthread_self());
#endif
}

std::ostringstream& Log::stream()
{
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf {};
#if WIN32
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif
    tl_stream << std::put_time(&tm_buf, "%F %T") << '\t';

    tl_stream << "[" << std::to_string(getOsThreadId()) << "]\t";

    switch (mLevel) 
    {
    case LogError:   tl_stream << "ERROR\t";   break;
    case LogWarning: tl_stream << "WARNING\t"; break;
    case LogDebug:   tl_stream << "DEBUG\t";   break;
    default:         tl_stream << "INFO\t";    break;
    }
    return tl_stream;
}

Log::Log(LogLevel level) : mLevel(level)
{
    tl_stream.str("");
    tl_stream.clear();    
}

static void crashHandler(int sig)
{
    AsyncLogger::instance().emergencyFlush();
    signal(sig, SIG_DFL);
    raise(sig);
}

void Log::init()
{
    AsyncLogger::instance().close();

    LogLevel lvl = LogInfo;
    if (Settings::getInstance()->getBool("Debug"))
    {
        lvl = LogDebug;
    }
    else
    {
        auto level = Settings::getInstance()->getString("LogLevel");
        if (level == "debug")       lvl = LogDebug;
        else if (level == "information") lvl = LogInfo;
        else if (level == "warning")     lvl = LogWarning;
        else if (level == "error")       lvl = LogError;
        else if (level.empty())          lvl = LogError;
        else                             lvl = (LogLevel)-1;  // disabled
    }

    auto base = Paths::getUserEmulationStationPath() + "/es_log";
    auto logPath = base + ".txt";

    if ((int)lvl < 0)
    {
        Utils::FileSystem::removeFile(logPath);
        mEnabled = false;
        return;
    }

    static constexpr int kMaxArchives = 4; // Keep 5 logs ( current + 4 latest )

    Utils::FileSystem::removeFile(base + "." + std::to_string(kMaxArchives - 1) + ".txt");

    for (int i = kMaxArchives - 2; i >= 0; --i)
    {
        Utils::FileSystem::renameFile(
            base + "." + std::to_string(i) + ".txt",
            base + "." + std::to_string(i + 1) + ".txt", true);
    }

    Utils::FileSystem::renameFile(logPath, base + ".0.txt", true);

    // signal(SIGSEGV, crashHandler); // It's already managed by main()
    signal(SIGABRT, crashHandler);

#ifndef WIN32
    signal(SIGTERM, crashHandler);
#endif

    bool debugToStderr = (lvl >= LogDebug);
    AsyncLogger::instance().open(logPath, lvl, debugToStderr);

    mReportingLevel = lvl;
    mEnabled = true;
}

void Log::flush()
{
    AsyncLogger::instance().emergencyFlush();
}

void Log::close()
{
    mEnabled = false;
    AsyncLogger::instance().close();
}

Log::~Log()
{
    AsyncLogger::instance().enqueue(mLevel, tl_stream.str());
}

StopWatch::StopWatch(const std::string& elapsedMillisecondsMessage, LogLevel level)
{ 
	mMessage = elapsedMillisecondsMessage; 
	mLevel = level;
	mStartTicks = SDL_GetTicks();
}

StopWatch::~StopWatch()
{
	int elapsed = SDL_GetTicks() - mStartTicks;
	LOG(mLevel) << mMessage << " " << elapsed << "ms";
}
