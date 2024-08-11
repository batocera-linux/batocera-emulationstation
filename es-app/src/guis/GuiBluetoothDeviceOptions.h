#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/BusyComponent.h"

#include <functional>

class GuiBluetoothDeviceOptions : public GuiComponent
{
public:
    GuiBluetoothDeviceOptions(Window* window, const std::string& id, const std::string& name, bool isConnected, std::function<void()> onComplete);

    bool input(InputConfig* config, Input input) override;
    std::vector<HelpPrompt> getHelpPrompts() override;

private:
    void onForgetDevice();
    void onConnectDevice();
    void onDisconnectDevice();

    MenuComponent mMenu;
    std::string mId;
    std::string mName;
    bool mIsConnected;
    bool mWaitingLoad;
    std::function<void()> mOnComplete; // Callback to be called after action
};
