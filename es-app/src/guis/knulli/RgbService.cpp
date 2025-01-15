#include "guis/knulli/RgbService.h"
#include "utils/Platform.h"
#include "Paths.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <stdio.h>
#include <sys/wait.h>
#include <fstream>

const std::string RGB_SERVICE_NAME = "/usr/bin/knulli-rgb-led-daemon";
const std::string RGB_COMMAND_NAME = "/usr/bin/knulli-rgb-led";
const std::string SEPARATOR = " ";
const std::string START = "start clear";
const std::string STOP = "stop";

void RgbService::start()
{
	if (Utils::FileSystem::exists(RGB_SERVICE_NAME)) {
		system((RGB_SERVICE_NAME + SEPARATOR + START).c_str());
	}
}

void RgbService::stop()
{
	if (Utils::FileSystem::exists(RGB_SERVICE_NAME)) {
		system((RGB_SERVICE_NAME + SEPARATOR + STOP).c_str());
	}
}

// TODO: This is a prototype. First improve the RGB bash scripts, then adopt the changes here.
void RgbService::setRgb(int mode, int brightness, int speed, int r, int g, int b) {
	
	std::string modeString = std::to_string(mode);
	std::string brightnessString = std::to_string(brightness);
	std::string speedString = std::to_string(speed);
	std::string rString = std::to_string(r);
	std::string gString = std::to_string(g);
	std::string bString = std::to_string(b);

	if (mode == 0) {
		system((RGB_COMMAND_NAME + SEPARATOR + modeString).c_str());
	}
	else if (mode < 5) {
		system((RGB_COMMAND_NAME
			 + SEPARATOR + modeString
			 + SEPARATOR + brightnessString
			 // Right stick
			 + SEPARATOR + rString
			 + SEPARATOR + gString
			 + SEPARATOR + bString
			 // Left stick (TODO: Make this obsolete!)
			 + SEPARATOR + rString
			 + SEPARATOR + gString
			 + SEPARATOR + bString
			).c_str());
	} else {
		system((RGB_COMMAND_NAME
			+ SEPARATOR + modeString
			+ SEPARATOR + brightnessString
			+ SEPARATOR + speedString
			).c_str());
	}

}
