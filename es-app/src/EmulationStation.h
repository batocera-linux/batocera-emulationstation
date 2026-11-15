#pragma once
#ifndef ES_APP_EMULATION_STATION_H
#define ES_APP_EMULATION_STATION_H

// API keys set at build time (empty when the service is not configured).
#include "ApiKeys.h"

// The version comes from the release tag (CMake sets it on BuildInfo.cpp only, see ES_VERSION),
// and the build date from that same file: a new version or a new day then rebuilds one small
// file, and every other file stays the same for ccache.
const char* esProgramVersionString();
const char* esProgramBuiltString();

#define PROGRAM_VERSION_STRING		esProgramVersionString()
#define PROGRAM_BUILT_STRING		esProgramBuiltString()

#ifndef SCREENSCRAPER_SOFTNAME

#define SCREENSCRAPER_SOFTNAME			"Emulationstation"

#endif

#endif // ES_APP_EMULATION_STATION_H
