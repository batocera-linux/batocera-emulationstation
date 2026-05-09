#pragma once

#include "GuiSettings.h"

class Window;

class GuiNetPlaySettings : public GuiSettings
{
public:
	GuiNetPlaySettings(Window* window, int selectItem = -1);

private:
	void loadCommunityRelayServers();
	void addRelayServerOptions(int selectItem);
};
