#pragma once
#ifndef ES_CORE_LOG_H
#define ES_CORE_LOG_H

#include <sstream>
#include <exception>
	
#define LOG(level) if(!Log::Enabled() || level > Log::getReportingLevel()) ; else Log().get(level)

#define TRYCATCH(m, x) { try { x; } \
catch (const std::exception& e) { LOG(LogError) << m << " Exception " << e.what(); Log::flush(); throw e; } \
catch (...) { LOG(LogError) << m << " Unknown Exception occured"; Log::flush(); throw; } }

enum LogLevel { LogError, LogWarning, LogInfo, LogDebug };

class Log
{
public:
	//Log();
	~Log();
	std::ostringstream& get(LogLevel level = LogInfo);

	static LogLevel getReportingLevel();
	static void setReportingLevel(LogLevel level);
	static void setupReportingLevel();

	static std::string getLogPath();

	static void flush();
	static void init();
	static void close();

	static inline bool Enabled() { return file != NULL; }

protected:
	std::ostringstream os;
	static FILE* file;

private:
	static LogLevel reportingLevel;
	static bool dirty;

	LogLevel messageLevel;
};

class StopWatch
{
public:
	StopWatch(const std::string& elapsedMillisecondsMessage, LogLevel level = LogDebug);
	~StopWatch();

private:
	std::string mMessage;
	LogLevel mLevel;
	int mStartTicks;
};

#endif // ES_CORE_LOG_H
