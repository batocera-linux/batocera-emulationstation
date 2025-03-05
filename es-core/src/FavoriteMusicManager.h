#pragma once

#include <string>

class Window;

class FavoriteMusicManager
{
public:
    static FavoriteMusicManager& getInstance();

    bool saveSongToFavorites(const std::string& path, const std::string& name, Window* window);
    bool removeSongFromFavorites(const std::string& path, const std::string& name, Window* window);

private:
    FavoriteMusicManager() = default;
    std::string getFavoritesFilePath() const;
};
