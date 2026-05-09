#include "FavoriteMusicManager.h"
#include "Paths.h"
#include "LocaleES.h"
#include "utils/FileSystemUtil.h"
#include "Settings.h"
#include "Window.h"
#include "guis/GuiMsgBox.h"

#include <fstream>

FavoriteMusicManager& FavoriteMusicManager::getInstance()
{
    static FavoriteMusicManager instance;
    return instance;
}

std::string FavoriteMusicManager::getFavoriteMusicFilePath()
{
    return Utils::FileSystem::combine(getFavoriteMusicPath(), "favorites.m3u");
}

std::string FavoriteMusicManager::getFavoriteMusicPath()
{
    if (!Paths::getUserMusicPath().empty())
        return Paths::getUserMusicPath();

    return Paths::getMusicPath();
}


std::vector<std::pair<std::string, std::string>> FavoriteMusicManager::loadFavoriteSongs(const std::string& favoritesFile)
{
    std::vector<std::pair<std::string, std::string>> favorites;
    std::list<std::string> lines = Utils::FileSystem::readAllLines(favoritesFile);
    for (const auto& line : lines)
    {
        if (line.empty())
            continue;
        size_t pos = line.find(';');
        if (pos != std::string::npos)
        {
            std::string path = line.substr(0, pos);
            std::string name = line.substr(pos + 1);
            favorites.emplace_back(path, name);
        }
    }
    return favorites;
}

bool FavoriteMusicManager::saveSongToFavorites(const std::string& path, const std::string& name, Window* window)
{
    std::string favoritesFile = getFavoriteMusicFilePath();
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
    std::string favoritesFile = getFavoriteMusicFilePath();
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
        if (Settings::getInstance()->getBool("audio.useFavoriteMusic"))
        {
            Settings::getInstance()->setBool("audio.useFavoriteMusic", false);
            Settings::getInstance()->saveFile();
        }
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
