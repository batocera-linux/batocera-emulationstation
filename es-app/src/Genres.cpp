#include "Genres.h"

#include "resources/ResourceManager.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include <pugixml/src/pugixml.hpp>
#include <string.h>

#include "SystemConf.h"
#include "FileData.h"

#include <set>

std::vector<GameGenre*> Genres::mGameGenres;
std::map<int, GameGenre*> Genres::mGenres;
std::map<std::string, int> Genres::mAllGenresNames;

void Genres::init()
{
	std::string xmlpath = ResourceManager::getInstance()->getResourcePath(":/genres.xml");
	if (!Utils::FileSystem::exists(xmlpath))
		return;

	LOG(LogInfo) << "Parsing XML file \"" << xmlpath << "\"...";

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(xmlpath.c_str());

	if (!result)
	{
		LOG(LogError) << "Error parsing XML file \"" << xmlpath << "\"!\n	" << result.description();
		return;
	}

	pugi::xml_node root = doc.child("genres");
	if (!root)
	{
		LOG(LogError) << "Could not find <genres> node in \"" << xmlpath << "\"!";
		return;
	}

	std::vector<GameGenre*> genres;

	for (pugi::xml_node genre = root.child("genre"); genre; genre = genre.next_sibling("genre"))
	{
		GameGenre* g = new GameGenre();
		
		for (pugi::xml_node node = genre.first_child(); node; node = node.next_sibling())
		{
			std::string name = node.name();
			std::string value = node.text().as_string();

			if (name == "id")
				g->id = Utils::String::toInteger(value);
			else if (name == "parent")
				g->parentId = Utils::String::toInteger(value);
			else if (name == "nom_fr")
				g->nom_fr = value;
			else if (name == "nom_de")
				g->nom_de = value;
			else if (name == "nom_en")
				g->nom_en = value;
			else if (name == "nom_es")
				g->nom_es = value;
			else if (name == "nom_pt")
				g->nom_pt = value;
			else if (name == "altname")
				g->altNames.push_back(value);
		}

		if (g->id == 0)
		{
			delete g;
			continue;
		}

		genres.push_back(g);
	}

	for (auto genre : genres)
	{
		if (genre->parentId == 0)
		{
			for (auto child : genres)
				if (child->parentId == genre->id)
					genre->children.push_back(child);
		}
		else
		{
			for (auto parent : genres)
			{
				if (parent->id == genre->parentId)
				{
					genre->parent = parent;
					break;
				}
			}
		}

		mGenres[genre->id] = genre;
		mGameGenres.push_back(genre);
	}

	// Pass 1 : crate genre mapping indexes
	for (auto genre : genres)
	{
		if (genre->parent != nullptr)
		{
			if (!genre->nom_en.empty())
				mAllGenresNames[Utils::String::toUpper(genre->parent->nom_en) + " / " + Utils::String::toUpper(genre->nom_en)] = genre->id;

			if (!genre->nom_fr.empty())
				mAllGenresNames[Utils::String::toUpper(genre->parent->nom_fr) + " / " + Utils::String::toUpper(genre->nom_fr)] = genre->id;

			if (!genre->nom_de.empty())
				mAllGenresNames[Utils::String::toUpper(genre->parent->nom_de) + " / " + Utils::String::toUpper(genre->nom_de)] = genre->id;

			if (!genre->nom_es.empty())
				mAllGenresNames[Utils::String::toUpper(genre->parent->nom_es) + " / " + Utils::String::toUpper(genre->nom_es)] = genre->id;

			if (!genre->nom_pt.empty())
				mAllGenresNames[Utils::String::toUpper(genre->parent->nom_pt) + " / " + Utils::String::toUpper(genre->nom_pt)] = genre->id;
		

			for (auto altName : genre->altNames)
				mAllGenresNames[Utils::String::toUpper(altName)] = genre->id;
		}
		else
		{
			if (!genre->nom_fr.empty())
				mAllGenresNames[Utils::String::toUpper(genre->nom_fr)] = genre->id;

			if (!genre->nom_de.empty())
				mAllGenresNames[Utils::String::toUpper(genre->nom_de)] = genre->id;

			if (!genre->nom_en.empty())
				mAllGenresNames[Utils::String::toUpper(genre->nom_en)] = genre->id;

			if (!genre->nom_es.empty())
				mAllGenresNames[Utils::String::toUpper(genre->nom_es)] = genre->id;

			if (!genre->nom_pt.empty())
				mAllGenresNames[Utils::String::toUpper(genre->nom_pt)] = genre->id;

			for (auto altName : genre->altNames)
				mAllGenresNames[Utils::String::toUpper(altName)] = genre->id;
		}
	}

	// Pass-2 : add subgenre without parent label, but never overwrite
	for (auto genre : genres)
	{
		if (genre->parent != nullptr)
		{
			if (!genre->nom_fr.empty() && mAllGenresNames.find(Utils::String::toUpper(genre->nom_fr)) == mAllGenresNames.cend())
				mAllGenresNames[Utils::String::toUpper(genre->nom_fr)] = genre->id;

			if (!genre->nom_de.empty() && mAllGenresNames.find(Utils::String::toUpper(genre->nom_de)) == mAllGenresNames.cend())
				mAllGenresNames[Utils::String::toUpper(genre->nom_de)] = genre->id;

			if (!genre->nom_en.empty() && mAllGenresNames.find(Utils::String::toUpper(genre->nom_en)) == mAllGenresNames.cend())
				mAllGenresNames[Utils::String::toUpper(genre->nom_en)] = genre->id;

			if (!genre->nom_es.empty() && mAllGenresNames.find(Utils::String::toUpper(genre->nom_es)) == mAllGenresNames.cend())
				mAllGenresNames[Utils::String::toUpper(genre->nom_es)] = genre->id;

			if (!genre->nom_pt.empty() && mAllGenresNames.find(Utils::String::toUpper(genre->nom_pt)) == mAllGenresNames.cend())
				mAllGenresNames[Utils::String::toUpper(genre->nom_pt)] = genre->id;
		}
	}
}

bool Genres::genreExists(int id)
{
	return id > 0 && mGenres.find(id) != mGenres.cend();
}

std::string& GameGenre::getLocalizedName()
{
	static int locale = -1;

	if (locale < 0)
	{
		std::string lang = SystemConf::getInstance()->get("system.language");

		auto shortNameDivider = lang.find("_");
		if (shortNameDivider != std::string::npos)
			lang = Utils::String::toLower(lang.substr(0, shortNameDivider));

		if (lang == "fr")
			locale = 1;
		else if (lang == "pt" || lang == "br")
			locale = 2;
		else if (lang == "de")
			locale = 3;
		else if (lang == "es")
			locale = 4;
		else
			locale = 0;
	}

	if (locale == 1 && !nom_fr.empty())
		return nom_fr;

	if (locale == 2 && !nom_pt.empty())
		return nom_pt;

	if (locale == 3 && !nom_de.empty())
		return nom_de;

	if (locale == 4 && nom_es.empty())
		return nom_es;

	return nom_en;
}

std::vector<std::string> Genres::getGenreFiltersNames(MetaDataList* game)
{
	std::vector<std::string> ret;

	auto ids = Utils::String::split(game->get(MetaDataId::GenreIds), ',', true);
	for (auto id : ids)
	{
		auto g = mGenres.find(Utils::String::toInteger(id));
		if (g != mGenres.cend())
		{
			if (g->second->parent != nullptr)
			{
				ret.push_back(std::to_string(g->second->parent->id));
				ret.push_back(std::to_string(g->second->id));
			}
			else
				ret.push_back(std::to_string(g->second->id));
		}
	}

	return ret;
}

std::string Genres::genreStringFromIds(const std::vector<std::string>& ids, bool localized)
{
	std::vector<std::string> ret;

	for (auto id : ids)
	{
		auto g = mGenres.find(Utils::String::toInteger(id));
		if (g != mGenres.cend())
		{
			if (g->second->parent != nullptr)
			{
				if (localized)
					ret.push_back(g->second->parent->getLocalizedName() + " / " + g->second->getLocalizedName());
				else 
					ret.push_back(g->second->parent->nom_en + " / " + g->second->nom_en);

				continue;
			}

			ret.push_back(localized ? g->second->getLocalizedName() : g->second->nom_en);
		}
	}

	return Utils::String::join(ret, ", ");
}


GameGenre* Genres::getGameGenre(const std::string& id)
{
	auto genre = mGenres.find(Utils::String::toInteger(id));
	if (genre != mGenres.cend())
		return genre->second;

	return nullptr;
}

GameGenre* Genres::fromGenreName(const std::string& name)
{
	auto g = mAllGenresNames.find(Utils::String::toUpper(name));
	if (g != mAllGenresNames.cend())
		return mGenres[g->second];
	
	if (name.find("/") >= 0)
	{
		for (auto subgenre : Utils::String::splitAny(Utils::String::toUpper(name), "/", true))
		{
			auto sg = mAllGenresNames.find(Utils::String::trim(subgenre));
			if (sg != mAllGenresNames.cend())
				return mGenres[g->second];
		}
	}

	return nullptr;
}

bool Genres::genreExists(MetaDataList* file, int id)
{
	auto idToFind = std::to_string(id);
	auto ids = Utils::String::split(file->get(MetaDataId::GenreIds), ',', true);
	for (auto id : ids)
		if (id == idToFind)
			return true;

	return false;
}

void Genres::convertGenreToGenreIds(MetaDataList* file)
{
	auto genre = Utils::String::toUpper(file->get(MetaDataId::Genre));
	if (genre.empty())
		return;

	std::set<int> ids;

	auto g = mAllGenresNames.find(genre);
	if (g != mAllGenresNames.cend())
		ids.insert(g->second);
	else if (genre.find(",") >= 0 || genre.find("/") >= 0)
	{
		for (auto subgenre : Utils::String::splitAny(genre, ",/", true))
		{
			auto sg = mAllGenresNames.find(Utils::String::trim(subgenre));
			if (sg != mAllGenresNames.cend())
				ids.insert(sg->second);
		}
	}

	if (ids.size() > 0)
	{
		std::vector<std::string> list;
		for (auto id : ids)
			list.push_back(std::to_string(id));
		
		file->set(MetaDataId::GenreIds, Utils::String::join(list, ","));
	}
	else
	{
		LOG(LogDebug) << "UNKNOWN GENRE : " << genre;
		//file->set(MetaDataId::GenreIds, "");
	}
}
