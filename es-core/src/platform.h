#pragma once
#ifndef ES_CORE_PLATFORM_H
#define ES_CORE_PLATFORM_H

#include <string>

#ifdef WIN32
#include <Windows.h>
#include <intrin.h>

#define sleep Sleep
#endif

class Window;

int runShutdownCommand(); // shut down the system (returns 0 if successful)
int runRestartCommand(); // restart the system (returns 0 if successful)
int runSystemCommand(const std::string& cmd_utf8, const std::string& name, Window* window); // run a utf-8 encoded in the shell (requires wstring conversion on Windows)
int quitES(const std::string& filename);
void touch(const std::string& filename);

#ifdef WIN32
void splitCommand(std::string cmd, std::string* executable, std::string* parameters);
#endif

#ifdef _ENABLEEMUELEC
std::string getShOutput(const std::string& mStr); /* < emuelec */
#endif
#endif // ES_CORE_PLATFORM_H
