#pragma once

#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <set>

class Window;
class SwitchComponent;

class GuiFavoriteMusicSelector
{
public:

    static void openSelectFavoriteSongs(Window *window, bool browseMusicMode, bool animate, 
                                      const std::string& customPath = "");
    static void openFavoritesSongs(Window *window, bool animate);
    
private:

    GuiFavoriteMusicSelector() {}

};
