#pragma once
#ifndef ES_APP_GUIS_GUI_FAVORITE_MUSIC_SELECTOR_H
#define ES_APP_GUIS_GUI_FAVORITE_MUSIC_SELECTOR_H

#include "GuiComponent.h"
#include <vector>
#include <memory>

class Window;
class SwitchComponent;

class GuiFavoriteMusicSelector : public GuiComponent
{
public:
    GuiFavoriteMusicSelector(Window* window);
    virtual ~GuiFavoriteMusicSelector();

    static void openSelectFavoriteSongs(Window* window, bool browseMusicMode = false, bool animate = false);

private:
    void save();

    std::vector<std::pair<std::string, std::string>> mFiles;
    std::vector<std::shared_ptr<SwitchComponent>> mSwitches;

};

#endif 
