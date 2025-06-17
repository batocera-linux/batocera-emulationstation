#pragma once

#include <string>
#include <vector>
#include <utility>

class Window;


class FavoriteMusicManager
{
public:

    static FavoriteMusicManager& getInstance();
    static std::string getFavoriteMusicFilePath();
    static std::string getFavoriteMusicPath();
    static std::vector<std::pair<std::string, std::string>> loadFavoriteSongs(const std::string& favoritesFile);
    bool saveSongToFavorites(const std::string& path, const std::string& name, Window* window);
    bool removeSongFromFavorites(const std::string& path, const std::string& name, Window* window);

private:

    FavoriteMusicManager() = default;
    FavoriteMusicManager(const FavoriteMusicManager&) = delete;
    FavoriteMusicManager& operator=(const FavoriteMusicManager&) = delete;

};
