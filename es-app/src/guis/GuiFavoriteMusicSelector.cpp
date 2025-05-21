#include "guis/GuiFavoriteMusicSelector.h"
#include "guis/GuiSettings.h"
#include "guis/GuiMsgBox.h"
#include "components/SwitchComponent.h"
#include "components/TextComponent.h"
#include "components/ButtonComponent.h"
#include "FavoriteMusicManager.h"
#include "Settings.h"
#include "AudioManager.h"
#include "Paths.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "views/ViewController.h"
#include "LocaleES.h"
#include "renderers/Renderer.h"
#include <fstream>
#include <set>
#include <algorithm>

void GuiFavoriteMusicSelector::openFavoritesSongs(Window *window, bool animate)
{
    auto s = new GuiSettings(window, _("FAVORITES PLAYLIST"));
    s->setCloseButton("back"); 

    std::vector<std::pair<std::string, std::string>> favorites;
    std::vector<std::shared_ptr<SwitchComponent>> favoritesSwitches;
    
    bool hasFavorites = false;
    std::string favoritesFile = FavoriteMusicManager::getFavoriteMusicFilePath();
    
    if (Utils::FileSystem::exists(favoritesFile))
    {
        favorites = FavoriteMusicManager::loadFavoriteSongs(favoritesFile);
        hasFavorites = !favorites.empty();
    }
    
    s->addGroup(_("FAVORITE SONGS"));
    
    if (hasFavorites)
    {
        for (auto& fav : favorites)
        {
            std::string name = fav.second;
            std::string path = fav.first;

            auto sw = std::make_shared<SwitchComponent>(window);
            sw->setState(true);  

            s->addWithDescription(name, path, sw, nullptr, "iconSound");

            favoritesSwitches.push_back(sw);
        }
    }
    else
    {
        s->addEntry(_("NO FAVORITES FOUND"), false, nullptr, "iconInfo");
    }
    
    s->addSaveFunc([window, favorites, favoritesSwitches]() {
        bool favoritesChanged = false;
        
        for (size_t i = 0; i < favorites.size(); ++i) {
            if (!favoritesSwitches[i]->getState()) {
                FavoriteMusicManager::getInstance().removeSongFromFavorites(
                    favorites[i].first, favorites[i].second, window);
                favoritesChanged = true;
            }
        }
    });
    
    if (animate)
        s->getMenu().animateTo(Vector2f((Renderer::getScreenWidth() - s->getMenu().getSize().x()) / 2, 
                                      (Renderer::getScreenHeight() - s->getMenu().getSize().y()) / 2));
    else
        s->getMenu().setPosition((Renderer::getScreenWidth() - s->getMenu().getSize().x()) / 2, 
                                (Renderer::getScreenHeight() - s->getMenu().getSize().y()) / 2);

    window->pushGui(s);
}

void GuiFavoriteMusicSelector::openSelectFavoriteSongs(Window *window, bool browseMusicMode, bool animate, 
                                                    const std::string& customPath)
{

    auto s = new GuiSettings(window, _("SELECTION OF FAVORITE SONGS"));
    s->setCloseButton("select");

    std::vector<std::pair<std::string, std::string>> favorites;
    std::set<std::string> favoriteSet;
    std::vector<std::pair<std::string, std::string>> directoryFiles;
    std::vector<std::shared_ptr<SwitchComponent>> directorySwitches;
    
    if (Utils::FileSystem::exists(FavoriteMusicManager::getFavoriteMusicFilePath()))
    {
        favorites = FavoriteMusicManager::loadFavoriteSongs(
            FavoriteMusicManager::getFavoriteMusicFilePath());

        for (auto& fav : favorites)
        {
            favoriteSet.insert(fav.first);
        }
    }

    std::string userMusicPath = Paths::getUserMusicPath();
    std::string systemMusicPath = Paths::getMusicPath();

    bool isInUserDir = false;
    bool isInSystemDir = false;
    bool isInRootDir = true;

    if (!customPath.empty()) {
        isInRootDir = false;

        if (customPath == userMusicPath) {
            isInUserDir = true;
        } else if (customPath == systemMusicPath) {
            isInSystemDir = true;
        } else if (customPath.find(userMusicPath) == 0) {
            isInUserDir = true;
        } else if (customPath.find(systemMusicPath) == 0) {
            isInSystemDir = true;
        }
    }

    s->addGroup(_("SELECTION OF FAVORITE SONGS"));
 
    if (isInRootDir) {
        s->addEntry(_("USER DIRECTORY") + " (" + userMusicPath + ")", true, [window, animate, userMusicPath]() {
            GuiFavoriteMusicSelector::openSelectFavoriteSongs(window, true, animate, userMusicPath);
        }, "iconFolder");

        s->addEntry(_("DEFAULT DIRECTORY") + " (" + systemMusicPath + ")", true, [window, animate, systemMusicPath]() {
            GuiFavoriteMusicSelector::openSelectFavoriteSongs(window, true, animate, systemMusicPath);
        }, "iconFolder");
        
        bool hasFavorites = !favorites.empty();
        auto favoriteSwitch = std::make_shared<SwitchComponent>(window);
        bool shouldUseFavorites = Settings::getInstance()->getBool("audio.useFavoriteMusic") && hasFavorites;
        if (Settings::getInstance()->getBool("audio.useFavoriteMusic") && !hasFavorites)
        {
            Settings::getInstance()->setBool("audio.useFavoriteMusic", false);
            Settings::getInstance()->saveFile();
        }

        favoriteSwitch->setState(shouldUseFavorites);

        s->addWithDescription(_("PLAY ONLY SONGS FROM YOUR FAVORITES PLAYLIST"), "", favoriteSwitch, nullptr);
        s->addSaveFunc([favoriteSwitch, hasFavorites]() 
        {
            bool useFavorite = favoriteSwitch->getState();
            if (useFavorite && !hasFavorites)
            {
                useFavorite = false;
            }
            Settings::getInstance()->setBool("audio.useFavoriteMusic", useFavorite);
            Settings::getInstance()->saveFile();
            AudioManager::getInstance()->playRandomMusic(false);
        });
    }
    else {
        std::string currentPath = customPath;

        if (Utils::FileSystem::exists(currentPath)) {
            auto dirContent = Utils::FileSystem::getDirContent(currentPath, false);
            std::vector<std::string> directories;
            std::vector<std::string> audioFiles;

            for (auto& entry : dirContent) {
                if (Utils::FileSystem::isDirectory(entry)) {
                    directories.push_back(entry);
                } else if (Utils::FileSystem::isAudio(entry)) {
                    audioFiles.push_back(entry);
                }
            }

            std::sort(directories.begin(), directories.end(), [](const std::string& a, const std::string& b) {
                return Utils::String::toUpper(Utils::FileSystem::getFileName(a)) < 
                       Utils::String::toUpper(Utils::FileSystem::getFileName(b));
            });

            for (auto& dir : directories) {
                std::string name = Utils::FileSystem::getFileName(dir);
                s->addEntry(name, false, [window, animate, dir]() {
                    GuiFavoriteMusicSelector::openSelectFavoriteSongs(window, true, animate, dir);
                }, "iconFolder");
            }

            std::sort(audioFiles.begin(), audioFiles.end(), [](const std::string& a, const std::string& b) {
                return Utils::String::toUpper(Utils::FileSystem::getFileName(a)) < 
                       Utils::String::toUpper(Utils::FileSystem::getFileName(b));
            });

            for (auto& file : audioFiles) {
                std::string fullName = Utils::FileSystem::getFileName(file);
                std::string nameWithoutExt = fullName;
                size_t lastDot = fullName.find_last_of('.');
                if (lastDot != std::string::npos) {
                    nameWithoutExt = fullName.substr(0, lastDot);
                }

                bool isInFavorites = favoriteSet.find(file) != favoriteSet.end();

                auto sw = std::make_shared<SwitchComponent>(window);
                sw->setState(isInFavorites);

                s->addWithDescription(fullName, "", sw, nullptr, "iconSound");

                directoryFiles.emplace_back(file, nameWithoutExt);
                directorySwitches.push_back(sw);
            }
        }
    }

    s->addGroup(_("FAVORITE SONGS FILE"));
    
    s->addEntry(_("FAVORITES.M3U"), true, [window, animate]() {
        GuiFavoriteMusicSelector::openFavoritesSongs(window, animate);
    }, "iconSound");

    s->addSaveFunc([window, favorites, directoryFiles, directorySwitches]() {
        bool favoritesChanged = false;
        auto favoritePath = FavoriteMusicManager::getFavoriteMusicPath();
        if (!Utils::FileSystem::exists(favoritePath))
            Utils::FileSystem::createDirectory(favoritePath);

        for (size_t i = 0; i < directoryFiles.size(); ++i) {
            if (directorySwitches[i]->getState()) {
                std::string filePath = directoryFiles[i].first;
                std::string fileName = directoryFiles[i].second; 

                bool found = false;
                for (auto& fav : favorites) {
                    if (fav.first == filePath) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    FavoriteMusicManager::getInstance().saveSongToFavorites(filePath, fileName, window);
                    favoritesChanged = true;
                }
            }
        }
    });

    if (animate)
        s->getMenu().animateTo(Vector2f((Renderer::getScreenWidth() - s->getMenu().getSize().x()) / 2, 
                                       (Renderer::getScreenHeight() - s->getMenu().getSize().y()) / 2));
    else
        s->getMenu().setPosition((Renderer::getScreenWidth() - s->getMenu().getSize().x()) / 2, 
                                (Renderer::getScreenHeight() - s->getMenu().getSize().y()) / 2);

    window->pushGui(s);
}
