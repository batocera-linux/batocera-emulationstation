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

enum QuitMode
{
	QUIT = 0,
	RESTART = 1,
	SHUTDOWN = 2,
	REBOOT = 3,
	FAST_SHUTDOWN = 4,
	FAST_REBOOT = 5
};

int runSystemCommand(const std::string& cmd_utf8, const std::string& name, Window* window); // run a utf-8 encoded in the shell (requires wstring conversion on Windows)
int quitES(QuitMode mode = QuitMode::QUIT);
bool isFastShutdown();
void processQuitMode();

struct BatteryInformation
{
	BatteryInformation()
	{
		hasBattery = false;
		level = 0;
		isCharging = false;
	}

	bool hasBattery;
	int  level;
	bool isCharging;
};

BatteryInformation queryBatteryInformation();
std::string queryIPAdress();

#ifdef _ENABLEEMUELEC
std::string getShOutput(const std::string& mStr); /* < emuelec */
#endif
#endif // ES_CORE_PLATFORM_H
