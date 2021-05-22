#pragma once

#ifndef ES_CORE_GENRES_H
#define ES_CORE_GENRES_H

#include <string>
#include <vector>
#include <map>

#define GENRE_LIGHTGUN 32
#define GENRE_ADULT 413

class MetaDataList;

struct GameGenre
{
	GameGenre()
	{
		id = 0;
		parentId = 0;
		parent = nullptr;
	}

	int id;
	int parentId;

	std::string nom_en;
	std::string nom_fr;
	std::string nom_de;
	std::string nom_es;
	std::string nom_pt;
	std::vector<std::string> altNames;

	std::string& getLocalizedName();

	GameGenre* parent;
	std::vector<GameGenre*> children;
};

class Genres
{
public:
	static void  init();
	static void convertGenreToGenreIds(MetaDataList* metaData);

	static bool genreExists(int id);
	static bool genreExists(MetaDataList* file, int id);

	static const std::vector<GameGenre*>& getGameGenres() { return mGameGenres; }

	static GameGenre* fromGenreName(const std::string& name);

	static GameGenre* getGameGenre(const std::string& id);

	static std::string					genreStringFromIds(const std::vector<std::string>& ids, bool localized = true);

	static std::vector<std::string>		getGenreFiltersNames(MetaDataList* game);

private:
	static std::vector<GameGenre*> mGameGenres;
	static std::map<int, GameGenre*> mGenres;
	static std::map<std::string, int> mAllGenresNames;
};

#endif // ES_CORE_GENRES_H
