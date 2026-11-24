#include "romm/RommCollections.h"
#include "romm/RommConfig.h"

#include "Log.h"
#include "Paths.h"
#include "Settings.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <pugixml.hpp>
#include <algorithm>

namespace Romm
{
	std::string getCollectionsSnapshotPath(const std::string& baseUrl)
	{
		return getServerCacheDir(baseUrl) + "/collections.xml";
	}

	bool writeCollectionsSnapshot(const std::string& path, const std::vector<RommCollection>& collections)
	{
		pugi::xml_document doc;
		pugi::xml_node root = doc.append_child("rommCollections");
		root.append_attribute("version").set_value(1);

		for (auto& c : collections)
		{
			pugi::xml_node node = root.append_child("collection");
			node.append_attribute("key").set_value(c.key.c_str());
			node.append_attribute("smart").set_value(c.smart);
			if (c.favorite)
				node.append_attribute("favorite").set_value(true);

			if (!c.mine)
				node.append_attribute("mine").set_value(false);

			if (!c.owner.empty())
				node.append_attribute("owner").set_value(c.owner.c_str());
			node.append_attribute("name").set_value(c.name.c_str());
			node.append_attribute("updated").set_value(c.updatedAt.c_str());

			for (auto& g : c.games)
			{
				pugi::xml_node game = node.append_child("game");
				game.append_attribute("system").set_value(g.first.c_str());
				game.append_attribute("file").set_value(g.second.c_str());
			}
		}

		std::string tmp = path + ".tmp";
		if (!doc.save_file(tmp.c_str(), "\t", pugi::format_default, pugi::encoding_utf8))
		{
			Utils::FileSystem::removeFile(tmp);
			return false;
		}

		if (!Utils::FileSystem::renameFile(tmp, path, true))
		{
			Utils::FileSystem::removeFile(tmp);
			return false;
		}

		return true;
	}

	bool readCollectionsSnapshot(const std::string& path, std::vector<RommCollection>& collections)
	{
		if (!Utils::FileSystem::exists(path, false))
			return false;

		pugi::xml_document doc;
		if (!doc.load_file(path.c_str()))
		{
			LOG(LogError) << "RomM: unable to parse " << path;
			return false;
		}

		for (pugi::xml_node node = doc.child("rommCollections").child("collection"); node; node = node.next_sibling("collection"))
		{
			RommCollection c;
			c.key = node.attribute("key").as_string();
			c.smart = node.attribute("smart").as_bool();
			c.favorite = node.attribute("favorite").as_bool();
			c.mine = node.attribute("mine").as_bool(true);
			c.owner = node.attribute("owner").as_string();
			c.name = node.attribute("name").as_string();
			c.updatedAt = node.attribute("updated").as_string();

			for (pugi::xml_node game = node.child("game"); game; game = game.next_sibling("game"))
				c.games.push_back(std::make_pair(std::string(game.attribute("system").as_string()), std::string(game.attribute("file").as_string())));

			if (!c.key.empty())
				collections.push_back(c);
		}

		return true;
	}

	std::string collectionDisplayName(const RommCollection& collection)
	{
		if (collection.mine)
			return collection.name;

		return collection.name + " (" + (collection.owner.empty() ? std::string("shared") : collection.owner) + ")";
	}

	std::string sanitizeCollectionName(const std::string& name)
	{
		static const std::string forbidden = "/\\:*?\"<>|,"; // file name, and the comma separated setting

		std::string ret;
		for (unsigned char c : name)
		{
			if (c < 0x20 || forbidden.find((char)c) != std::string::npos)
				ret += '-';
			else
				ret += (char)c;
		}

		ret = Utils::String::trim(ret);
		while (!ret.empty() && ret[0] == '.')
			ret = ret.substr(1);

		return ret;
	}

	static std::string collectionsFolder()
	{
		// same as getCollectionsFolder() in CollectionSystemManager.cpp
		return Utils::FileSystem::getGenericPath(Paths::getUserEmulationStationPath() + "/collections");
	}

	static std::string collectionFile(const std::string& esName)
	{
		return collectionsFolder() + "/custom-" + esName + ".cfg";
	}

	static std::string ownedManifestPath()
	{
		return getConfigDirectory() + "/collections-owned.txt";
	}

	static std::set<std::string> readOwned()
	{
		std::set<std::string> owned;
		std::string path = ownedManifestPath();
		if (!Utils::FileSystem::exists(path, false))
			return owned;

		for (auto& line : Utils::String::split(Utils::FileSystem::readAllText(path), '\n'))
		{
			std::string name = Utils::String::trim(line);
			if (!name.empty())
				owned.insert(name);
		}

		return owned;
	}

	void applyEsCollections(const RommConfig& config, bool active, const std::map<std::string, std::string>& systemPaths)
	{
		std::set<std::string> owned = readOwned();

		// ES name -> file content
		std::map<std::string, std::string> desired;

		if (active && !config.collections.empty())
		{
			std::vector<RommCollection> collections;
			readCollectionsSnapshot(getCollectionsSnapshotPath(config.baseUrl()), collections);

			std::sort(collections.begin(), collections.end(), [](const RommCollection& a, const RommCollection& b) { return a.key < b.key; });

			for (auto& c : collections)
			{
				if (config.collections.find(c.key) == config.collections.cend())
					continue;

				std::string base = sanitizeCollectionName(collectionDisplayName(c));
				if (base.empty())
					base = "RomM";

				// Never take the name of a collection made by the user (or of another RomM collection)
				std::string esName = base;
				for (int n = 1; ; n++)
				{
					bool usedByUser = owned.find(esName) == owned.cend() && Utils::FileSystem::exists(collectionFile(esName), false);
					if (!usedByUser && desired.find(esName) == desired.cend())
						break;

					esName = base + (n == 1 ? " (RomM)" : " (RomM " + std::to_string(n) + ")");
				}

				std::string content;
				for (auto& g : c.games)
				{
					auto sys = systemPaths.find(g.first);
					if (sys == systemPaths.cend())
						continue;

					std::string path = Utils::FileSystem::getGenericPath(sys->second + "/" + Utils::FileSystem::getFileName(g.second));

					// Games of hidden systems : only the downloaded ones are visible
					if (config.hiddenSystems.find(g.first) != config.hiddenSystems.cend() && !Utils::FileSystem::exists(path, false))
						continue;

					// Same format as ES writes it (CollectionSystemManager::saveCustomCollection)
					content += Utils::FileSystem::createRelativePath(path, Paths::getRootPath(), true) + "\n";
				}

				desired[esName] = content;
			}
		}

		// ES setting listing the custom collections to show
		std::vector<std::string> shown = Utils::String::split(Settings::getInstance()->getString("CollectionSystemsCustom"), ',', true);
		bool settingChanged = false;

		// Remove RomM collections no longer wanted
		for (auto& name : owned)
		{
			if (desired.find(name) != desired.cend())
				continue;

			Utils::FileSystem::removeFile(collectionFile(name));

			auto it = std::find(shown.begin(), shown.end(), name);
			if (it != shown.end())
			{
				shown.erase(it);
				settingChanged = true;
			}

			LOG(LogInfo) << "RomM: collection removed : " << name;
		}

		// Write the wanted ones (only when the content changed)
		if (!desired.empty())
			Utils::FileSystem::createDirectory(collectionsFolder());

		for (auto& item : desired)
		{
			std::string path = collectionFile(item.first);
			if (!Utils::FileSystem::exists(path, false) || Utils::FileSystem::readAllText(path) != item.second)
				writeFileAtomic(path, item.second);

			// Shown when created; afterwards the user's choice in ES collection settings is kept
			if (owned.find(item.first) == owned.cend() && std::find(shown.cbegin(), shown.cend(), item.first) == shown.cend())
			{
				shown.push_back(item.first);
				settingChanged = true;
			}
		}

		// Manifest of the collection files that belong to RomM
		if (desired.empty())
			Utils::FileSystem::removeFile(ownedManifestPath());
		else
		{
			std::string manifest;
			for (auto& item : desired)
				manifest += item.first + "\n";

			Utils::FileSystem::createDirectory(getConfigDirectory());
			writeFileAtomic(ownedManifestPath(), manifest);
		}

		if (settingChanged)
		{
			Settings::getInstance()->setString("CollectionSystemsCustom", Utils::String::vectorToCommaString(shown));
			Settings::getInstance()->saveFile();
		}

		if (!desired.empty() || !owned.empty())
		{
			LOG(LogInfo) << "RomM: " << desired.size() << " collections";
		}
	}
}
