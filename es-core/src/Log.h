#pragma once
#ifndef ES_CORE_LOG_H
#define ES_CORE_LOG_H

#include <sstream>
#include <exception>
#include <atomic>

#define LOG(level) if(!Log::enabled() || level > Log::getReportingLevel()) ; else Log(level).stream()

#define TRYCATCH(m, x) { try { x; } \
catch (const std::exception& e) { LOG(LogError) << m << " Exception " << e.what(); Log::flush(); throw e; } \
catch (...) { LOG(LogError) << m << " Unknown Exception occured"; Log::flush(); throw; } }

enum LogLevel { LogError, LogWarning, LogInfo, LogDebug };

class Log
{
public:
	static LogLevel getReportingLevel() { return mReportingLevel; }
	static bool     enabled() { return mEnabled; }

	static void init();
	static void flush();
	static void close();

private:
	static LogLevel mReportingLevel;
	static bool     mEnabled;

public:
	Log(LogLevel level);
	~Log();

	std::ostringstream& stream();

	Log(const Log&) = delete;
	Log& operator=(const Log&) = delete;

protected:	
	LogLevel		    mLevel;
};

class StopWatch
{
public:
	StopWatch(const std::string& elapsedMillisecondsMessage, LogLevel level = LogDebug);
	~StopWatch();

private:
	std::string mMessage;
	LogLevel    mLevel;
	int         mStartTicks;	
};

#endif // ES_CORE_LOG_H
