#pragma once
#ifndef ES_APP_EMULATION_STATION_H
#define ES_APP_EMULATION_STATION_H

// These numbers and strings need to be manually updated for a new version.
// Do this version number update as the very last commit for the new release version.
#define PROGRAM_VERSION_MAJOR        39
#define PROGRAM_VERSION_MINOR        0
#define PROGRAM_VERSION_MAINTENANCE  0
#define PROGRAM_VERSION_STRING		"39"

#define PROGRAM_BUILT_STRING __DATE__ " - " __TIME__

#define RESOURCE_VERSION_STRING "39,0,0\0"
#define RESOURCE_VERSION PROGRAM_VERSION_MAJOR,PROGRAM_VERSION_MINOR,PROGRAM_VERSION_MAINTENANCE

#ifndef SCREENSCRAPER_SOFTNAME

#if BATOCERA
#define SCREENSCRAPER_SOFTNAME			"Batocera-Emulationstation"
#elif RETROBAT
#define SCREENSCRAPER_SOFTNAME			"Retrobat-Emulationstation"
#else
#define SCREENSCRAPER_SOFTNAME			"Emulationstation"
#endif

#endif

#endif // ES_APP_EMULATION_STATION_H
