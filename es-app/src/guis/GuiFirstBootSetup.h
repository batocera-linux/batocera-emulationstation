#pragma once
#ifndef ES_APP_GUIS_GUI_FIRST_BOOT_SETUP_H
#define ES_APP_GUIS_GUI_FIRST_BOOT_SETUP_H

#include "GuiComponent.h"

// Shown on first boot only (system.firstboot != "done").
// Walks the user through 3 steps:
//   1. Enter their name
//   2. Select / configure WiFi
//   3. Optionally download AI model files (~5 GB)
// On completion writes system.firstboot=done and launches GuiAiGraphics.
class GuiFirstBootSetup : public GuiComponent
{
public:
    GuiFirstBootSetup(Window* window);

    // GuiComponent overrides - this container itself is invisible;
    // each step pushes its own GUI on the window stack.
    bool input(InputConfig* config, Input input) override { return false; }
    std::vector<HelpPrompt> getHelpPrompts() override { return {}; }

    void showStepName();

private:
    void showStepWifi(const std::string& error = "");
    void showStepModels();
    void finalize();
};

#endif // ES_APP_GUIS_GUI_FIRST_BOOT_SETUP_H
