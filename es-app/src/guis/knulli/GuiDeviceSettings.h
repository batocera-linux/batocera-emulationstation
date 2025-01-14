#pragma once
#include "guis/GuiSettings.h"

class GuiDeviceSettings : public GuiSettings
{
public:
        GuiDeviceSettings(Window* window);

private:
        void openPowerManagementSettings();
        void openRgbLedSettings();
        void installPico8();
};
