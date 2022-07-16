#pragma once
#ifndef ES_CORE_LOG_H
#define ES_CORE_LOG_H

#include <sstream>
#include <exception>
	
#define LOG(level) if(!Log::enabled() || level > Log::getReportingLevel()) ; else Log().get(level)

#define TRYCATCH(m, x) { try { x; } \
catch (const std::exception& e) { LOG(LogError) << m << " Exception " << e.what(); Log::flush(); throw e; } \
catch (...) { LOG(LogError) << m << " Unknown Exception occured"; Log::flush(); throw; } }

enum LogLevel { LogError, LogWarning, LogInfo, LogDebug };

class Log
{
public:
	~Log();
	std::ostringstream& get(LogLevel level = LogInfo);

	static inline LogLevel& getReportingLevel() { return mReportingLevel; }
	static inline bool enabled() { return mFile != NULL; }

	static void init();
	static void flush();
	static void close();
	
private:
	static LogLevel     mReportingLevel;
	static bool         mDirty;
	static FILE*        mFile;

protected:
	std::ostringstream  mStream;
	LogLevel		    mMessageLevel;
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
