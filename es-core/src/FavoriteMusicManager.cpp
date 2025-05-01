#include "FavoriteMusicManager.h"
#include "utils/FileSystemUtil.h"
#include "Settings.h"
#include "Window.h"
#include "guis/GuiMsgBox.h"
#include "Paths.h"
#include <libintl.h> 
#define _(String) gettext(String) 

#include <fstream>

FavoriteMusicManager& FavoriteMusicManager::getInstance()
{
    static FavoriteMusicManager instance;
    return instance;
}

std::string FavoriteMusicManager::getFavoritesFilePath() const
{
    return Paths::getUserMusicPath() + "/favorites.m3u";
}

bool FavoriteMusicManager::saveSongToFavorites(const std::string& path, const std::string& name, Window* window)
{
    std::string favoritesFile = getFavoritesFilePath();
    bool alreadyExists = false;
    std::ifstream infile(favoritesFile);
    std::string line;
    while (std::getline(infile, line))
    {
        if (line == path + ";" + name)
        {
            alreadyExists = true;
            break;
        }
    }
    infile.close();

    if (alreadyExists)
    {
        return false;
    }

    std::ofstream ofs(favoritesFile, std::ios::app);
    if (!ofs.is_open())
    {
        return false;
    }

    ofs << path << ";" << name << "\n";
    ofs.close();

    return true;
}

bool FavoriteMusicManager::removeSongFromFavorites(const std::string& path, const std::string& name, Window* window)
{
    std::string favoritesFile = getFavoritesFilePath();
    if (!Utils::FileSystem::exists(favoritesFile))
        return false;

    auto lines = Utils::FileSystem::readAllLines(favoritesFile);
    bool found = false;
    for (auto it = lines.begin(); it != lines.end(); ++it)
    {
        if (*it == path + ";" + name)
        {
            lines.erase(it);
            found = true;
            break;
        }
    }

    if (!found)
        return false;

    if (lines.empty())
    {
        Utils::FileSystem::removeFile(favoritesFile);
        Settings::getInstance()->setBool("audio.useFavoriteMusic", false);
        Settings::getInstance()->saveFile();
        }
    else
    {
        std::ofstream ofs(favoritesFile, std::ios::trunc);
        if (!ofs.is_open())
        {
            return false;
        }

        for (auto& l : lines)
            ofs << l << "\n";
        ofs.close();

        }

    return true;
}
