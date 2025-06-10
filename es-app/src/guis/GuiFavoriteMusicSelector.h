#pragma once

#include "GuiComponent.h"
#include <vector>
#include <memory>
#include <string>

class SwitchComponent;
class MenuComponent;
class Window;
class InputConfig;
struct Input;

class GuiFavoriteMusicSelector : public GuiComponent
{
public:
    GuiFavoriteMusicSelector(Window* window);
    ~GuiFavoriteMusicSelector();
    
    static void openSelectFavoriteSongs(Window* window, bool = false, bool = false);
    bool input(InputConfig* config, Input input) override;

private:
    void save();
    
    MenuComponent* mMenu;
    std::vector<std::pair<std::string, std::string>> mFiles;
    std::vector<std::shared_ptr<SwitchComponent>> mSwitches;
};
