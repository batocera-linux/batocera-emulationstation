#include "guis/GuiFavoriteMusicSelector.h"
#include "guis/GuiSettings.h"
#include "components/SwitchComponent.h"
#include "Window.h"
#include "Paths.h"
#include "FavoriteMusicManager.h"
#include "utils/FileSystemUtil.h"
#include "LocaleES.h"
#include "renderers/Renderer.h"
#include <set>
#include <algorithm>

GuiFavoriteMusicSelector::GuiFavoriteMusicSelector(Window* window) : GuiComponent(window)
{
    auto s = new GuiSettings(window, _("FAVORITE SONGS"));
    s->setCloseButton("select"); 
    
    std::function<void(const std::string&)> scan = [&](const std::string& path) {
        if (!Utils::FileSystem::exists(path)) return;
        for (auto& file : Utils::FileSystem::getDirContent(path, false)) {
            if (Utils::FileSystem::isDirectory(file)) scan(file);
            else if (Utils::FileSystem::isAudio(file)) {
                std::string fileName = Utils::FileSystem::getFileName(file);
                size_t lastDot = fileName.find_last_of('.');
                if (lastDot != std::string::npos) {
                    fileName = fileName.substr(0, lastDot);
                }
                mFiles.emplace_back(file, fileName);
            }
        }
    };
    if (!Paths::getUserMusicPath().empty()) scan(Paths::getUserMusicPath());
    if (!Paths::getMusicPath().empty()) scan(Paths::getMusicPath());
    std::sort(mFiles.begin(), mFiles.end(), [](auto& a, auto& b) { return a.second < b.second; });
    
    std::set<std::string> fav; 
    auto favorites = FavoriteMusicManager::loadFavoriteSongs(FavoriteMusicManager::getFavoriteMusicFilePath());
    for (auto& f : favorites) fav.insert(f.first);
    
    // s->addGroup(_("SELECT FAVORITE SONGS"));             //sub menu "SELECT FAVORITE SONGS".
    
    for (auto& file : mFiles) {
        auto sw = std::make_shared<SwitchComponent>(window);
        sw->setState(fav.count(file.first) > 0);
    //    s->addWithDescription(file.second,file.first, sw, nullptr, "iconSound");  //   Description with sound icone
    //    s->addWithDescription(file.second,"", sw, nullptr, "iconSound");          //no Description with sound icone
        s->addWithDescription(file.second,"", sw, nullptr);                         //no Description no sound icone

        mSwitches.push_back(sw);
    }
    
    s->addSaveFunc([this]() {
        save();
    });
    
    s->getMenu().setPosition((Renderer::getScreenWidth() - s->getMenu().getSize().x()) / 2, 
                            (Renderer::getScreenHeight() - s->getMenu().getSize().y()) / 2);
    
    mWindow->pushGui(s);
    
}

GuiFavoriteMusicSelector::~GuiFavoriteMusicSelector() { 

}

void GuiFavoriteMusicSelector::save()
{
    auto currentFavorites = FavoriteMusicManager::loadFavoriteSongs(FavoriteMusicManager::getFavoriteMusicFilePath());
    std::set<std::string> currentFavPaths;
    for (auto& fav : currentFavorites) currentFavPaths.insert(fav.first);
    
    for (size_t i = 0; i < mFiles.size() && i < mSwitches.size(); i++)
    {
        std::string filePath = mFiles[i].first;
        std::string fileName = mFiles[i].second;
        bool isChecked = mSwitches[i]->getState();
        bool isInFavorites = currentFavPaths.count(filePath) > 0;
        
        if (isChecked && !isInFavorites) {
            FavoriteMusicManager::getInstance().saveSongToFavorites(filePath, fileName, mWindow);
        }
        else if (!isChecked && isInFavorites) {
            FavoriteMusicManager::getInstance().removeSongFromFavorites(filePath, fileName, mWindow);
        }
    }
}

void GuiFavoriteMusicSelector::openSelectFavoriteSongs(Window* window, bool, bool) { 
    new GuiFavoriteMusicSelector(window);
}
