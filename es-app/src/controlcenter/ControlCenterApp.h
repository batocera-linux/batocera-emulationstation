#pragma once
#ifndef ES_APP_CONTROLCENTER_CONTROLCENTERAPP_H
#define ES_APP_CONTROLCENTER_CONTROLCENTERAPP_H

#include <string>

// emulationstation --controlcenter: the card in its own window, kept running once closed.
// SIGUSR1 shows or hides it, --controlcenter-hidden starts it hidden.
int runControlCenter(int argc, char* argv[], const std::string& xmlPath, const std::string& screen, bool startHidden);

class Window;

// The card inside ES when it is in front; false with several screens.
bool canShowControlCenterInProcess();
bool toggleControlCenterInProcess(Window* window);

#endif
