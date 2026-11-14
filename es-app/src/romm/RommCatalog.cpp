#include "romm/RommCatalog.h"
#include "romm/RommConfig.h"
#include "romm/RommCollections.h"
#include "romm/RommMediaCache.h"

#include "FileData.h"
#include "SystemData.h"
#include "Gamelist.h"
#include "Genres.h"
#include "Settings.h"
#include "Log.h"
#include "Paths.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <pugixml.hpp>
#include <cstdlib>

// Defined in Gamelist.cpp (not exposed in Gamelist.h)
FileData* findOrCreateFile(SystemData* system, const std::string& path, FileType type, std::unordered_map<std::string, FileData*>& fileMap);

namespace Romm
{
	bool RommCatalog::sActive = false;
	std::string RommCatalog::sBaseUrl;
	std::string RommCatalog::sMediaDir[(size_t)Media::Count];
	std::unordered_set<std::string> RommCatalog::sSystemsWithGames;
	std::unordered_set<std::string> RommCatalog::sCachedMedia[(size_t)Media::Count];
	int RommCatalog::sImageMedia = -1;
	std::map<std::string, std::string> RommCatalog::sSystemPaths;
	std::map<std::string, std::set<std::string>> RommCatalog::sSystemExtensions;
	std::unordered_set<std::string> RommCatalog::sFavoriteKeys;
	std::mutex RommCatalog::sMutex;
	std::unordered_map<std::string, RemoteGame> RommCatalog::sRemoteGames;
	std::unordered_map<std::string, int> RommCatalog::sRomIds;
	std::unordered_map<std::string, RemoteGame> RommCatalog::sKnownGames;
	std::set<std::string> RommCatalog::sDefinedSystems;
	std::map<std::string, std::string> RommCatalog::sSystemFullNames;

	std::string getLibraryDir(const std::string& baseUrl)
	{
		return getServerCacheDir(baseUrl) + "/library";
	}

	// Snapshot: one XML file per system, a <rom> per game. RomM-only data are attributes of <rom>,
	// never copied into gamelist.xml.

	static void addText(pugi::xml_node& node, const char* name, const std::string& value)
	{
		if (!value.empty())
			node.append_child(name).text().set(value.c_str());
	}

	bool writeSnapshotFile(const std::string& path, const std::string& system, const std::vector<CatalogRecord>& records)
	{
		pugi::xml_document doc;
		pugi::xml_node root = doc.append_child("rommLibrary");
		root.append_attribute("version").set_value(1);
		root.append_attribute("system").set_value(system.c_str());

		for (auto& r : records)
		{
			pugi::xml_node rom = root.append_child("rom");
			rom.append_attribute("id").set_value(r.romId);
			rom.append_attribute("platform").set_value(r.platformId);
			rom.append_attribute("fileId").set_value(r.fileId);
			rom.append_attribute("file").set_value(r.fileName.c_str());
			rom.append_attribute("size").set_value(std::to_string(r.size).c_str());
			rom.append_attribute("updated").set_value(r.updatedAt.c_str());

			if (r.inPlaylist > 0)
				rom.append_attribute("inPlaylist").set_value(r.inPlaylist);

			if (!r.kind.empty())
			{
				rom.append_attribute("kind").set_value(r.kind.c_str());
				rom.append_attribute("folder").set_value(r.folderName.c_str());

				for (auto& p : r.parts)
				{
					pugi::xml_node part = rom.append_child("part");
					part.append_attribute("id").set_value(p.id);
					part.append_attribute("path").set_value(p.path.c_str());
					part.append_attribute("size").set_value(std::to_string(p.size).c_str());
					if (p.romId > 0)
						part.append_attribute("rom").set_value(p.romId);
				}

				for (auto& d : r.discs)
					rom.append_child("disc").append_attribute("path").set_value(d.c_str());
			}

			// <key>="server path" <key>File="cache file", per media type
			for (auto& m : allMedia())
			{
				size_t i = mediaIndex(m.type);
				if (!r.mediaUrl[i].empty())
					rom.append_attribute(m.key).set_value(r.mediaUrl[i].c_str());

				if (!r.mediaFile[i].empty())
					rom.append_attribute((std::string(m.key) + "File").c_str()).set_value(r.mediaFile[i].c_str());
			}

			if (r.cheevosId > 0)
				rom.append_attribute("cheevosId").set_value(r.cheevosId);

			if (!r.cheevosHash.empty())
				rom.append_attribute("cheevosHash").set_value(r.cheevosHash.c_str());

			addText(rom, "name", r.name);
			addText(rom, "desc", r.desc);
			addText(rom, "releasedate", r.releaseDate);
			addText(rom, "developer", r.developer);
			addText(rom, "publisher", r.publisher);
			addText(rom, "genre", r.genre);
			addText(rom, "players", r.players);
			addText(rom, "rating", r.rating);
		}

		std::string tmp = path + ".tmp";
		if (!doc.save_file(tmp.c_str(), "\t", pugi::format_default, pugi::encoding_utf8))
		{
			LOG(LogError) << "RomM: unable to write " << tmp;
			Utils::FileSystem::removeFile(tmp);
			return false;
		}

		if (!Utils::FileSystem::renameFile(tmp, path, true))
		{
			LOG(LogError) << "RomM: unable to rename " << tmp;
			Utils::FileSystem::removeFile(tmp);
			return false;
		}

		return true;
	}

	bool readSnapshotFile(const std::string& path, std::vector<CatalogRecord>& records)
	{
		pugi::xml_document doc;
		pugi::xml_parse_result res = doc.load_file(path.c_str());
		if (!res)
		{
			LOG(LogError) << "RomM: unable to parse " << path << " : " << res.description();
			return false;
		}

		pugi::xml_node root = doc.child("rommLibrary");
		if (!root)
			return false;

		std::string system = root.attribute("system").as_string();

		for (pugi::xml_node rom = root.child("rom"); rom; rom = rom.next_sibling("rom"))
		{
			CatalogRecord r;
			r.romId = rom.attribute("id").as_int();
			r.platformId = rom.attribute("platform").as_int();
			r.fileId = rom.attribute("fileId").as_int();
			r.size = rom.attribute("size").as_llong();
			r.fileName = rom.attribute("file").as_string();
			r.updatedAt = rom.attribute("updated").as_string();
			r.kind = rom.attribute("kind").as_string();
			r.inPlaylist = rom.attribute("inPlaylist").as_int();
			r.folderName = rom.attribute("folder").as_string();

			for (pugi::xml_node part = rom.child("part"); part; part = part.next_sibling("part"))
			{
				CatalogRecord::Part p;
				p.id = part.attribute("id").as_int();
				p.path = part.attribute("path").as_string();
				p.size = part.attribute("size").as_llong();
				p.romId = part.attribute("rom").as_int();
				r.parts.push_back(p);
			}

			for (pugi::xml_node disc = rom.child("disc"); disc; disc = disc.next_sibling("disc"))
				r.discs.push_back(disc.attribute("path").as_string());

			for (auto& m : allMedia())
			{
				size_t i = mediaIndex(m.type);
				r.mediaUrl[i] = rom.attribute(m.key).as_string();
				r.mediaFile[i] = rom.attribute((std::string(m.key) + "File").c_str()).as_string();
			}
			r.cheevosId = rom.attribute("cheevosId").as_int();
			r.cheevosHash = rom.attribute("cheevosHash").as_string();
			r.system = system;

			r.name = rom.child("name").text().as_string();
			r.desc = rom.child("desc").text().as_string();
			r.releaseDate = rom.child("releasedate").text().as_string();
			r.developer = rom.child("developer").text().as_string();
			r.publisher = rom.child("publisher").text().as_string();
			r.genre = rom.child("genre").text().as_string();
			r.players = rom.child("players").text().as_string();
			r.rating = rom.child("rating").text().as_string();

			if (r.romId > 0 && !r.fileName.empty())
				records.push_back(r);
		}

		return true;
	}

	// ---------------------------------------------------------------------------

	void RommCatalog::prepare(pugi::xml_node& systemList)
	{
		sActive = false;
		sBaseUrl.clear();
		sSystemsWithGames.clear();
		for (size_t i = 0; i < (size_t)Media::Count; i++)
		{
			sMediaDir[i].clear();
			sCachedMedia[i].clear();
		}
		sFavoriteKeys.clear();

		std::map<std::string, std::string> systemPaths; // rom folder of every system, for the collections
		std::map<std::string, std::set<std::string>> systemExtensions;

		{
			std::unique_lock<std::mutex> lock(sMutex);
			sRemoteGames.clear();
			sRomIds.clear();
			sKnownGames.clear();
			sDefinedSystems.clear();
			sSystemFullNames.clear();

			for (pugi::xml_node system = systemList.child("system"); system; system = system.next_sibling("system"))
			{
				std::string name = system.child("name").text().get();
				if (name.empty())
					continue;

				sDefinedSystems.insert(name);

				std::string fullName = system.child("fullname").text().get();
				sSystemFullNames[name] = fullName.empty() ? name : fullName;

				// Normalized exactly like SystemData::loadSystem, so collection lines match the games' paths
				std::string path = Utils::FileSystem::getGenericPath(system.child("path").text().get());
				if (!path.empty() && path[0] == '~')
				{
					path.erase(0, 1);
					path.insert(0, Paths::getHomePath());
					path = Utils::FileSystem::getCanonicalPath(path);
				}

				if (!path.empty())
					systemPaths[name] = path;

				// ".cue .CUE .m3u" -> { ".cue", ".m3u" }
				for (auto& ext : Utils::String::splitAny(system.child("extension").text().get(), " \t\r\n,", true))
					systemExtensions[name].insert(Utils::String::toLower(ext));
			}
		}

		{
			std::unique_lock<std::mutex> lock(sMutex);
			sSystemPaths = systemPaths;
			sSystemExtensions = systemExtensions;
		}

		RommConfig config = RommConfig::load();

		// RomM collections become ES collections, on every path out of here (removed when RomM is not usable).
		// This runs before ES loads its collections (loadConfig calls prepare first).
		bool libraryPresent = false;
		struct CollectionsGuard
		{
			const RommConfig& config; const bool& present; const std::map<std::string, std::string>& paths;
			~CollectionsGuard() { applyEsCollections(config, present, paths); }
		} collectionsGuard{ config, libraryPresent, systemPaths };

		if (!config.isUsable())
			return;

		sBaseUrl = config.baseUrl();

		std::string libraryDir = getLibraryDir(sBaseUrl);
		if (!Utils::FileSystem::isDirectory(libraryDir))
			return;

		libraryPresent = true;

		for (auto file : Utils::FileSystem::getDirContent(libraryDir, false, false))
		{
			if (Utils::String::toLower(Utils::FileSystem::getExtension(file)) != ".xml")
				continue;

			std::string system = Utils::FileSystem::getStem(file);
			if (config.hiddenSystems.find(system) == config.hiddenSystems.cend()) // hidden in the RomM menu
				sSystemsWithGames.insert(system);
		}

		{
			Media image;
			sImageMedia = config.imageMedia(image) ? (int)mediaIndex(image) : -1;
		}

		configureOnDemand(config); // media of a selected RomM game fetched when missing

		// Media already in the cache (shown for RomM games not on the device)
		for (auto& m : allMedia())
		{
			size_t i = mediaIndex(m.type);
			sMediaDir[i] = getMediaCacheDir(sBaseUrl, m.type);

			if (Utils::FileSystem::isDirectory(sMediaDir[i]))
				for (auto file : Utils::FileSystem::getDirContent(sMediaDir[i], false, false))
					sCachedMedia[i].insert(Utils::FileSystem::getFileName(file));
		}

		// RomM favorites (the user's own favorites collection) become ES favorites
		if (config.syncFavorites)
		{
			std::vector<RommCollection> collections;
			readCollectionsSnapshot(getCollectionsSnapshotPath(sBaseUrl), collections);

			for (auto& c : collections)
				if (c.favorite)
					for (auto& g : c.games)
						sFavoriteKeys.insert(g.first + "/" + g.second);
		}

		sActive = sSystemsWithGames.size() > 0;

		LOG(LogInfo) << "RomM: library snapshot for " << sSystemsWithGames.size() << " systems, " << sCachedMedia[mediaIndex(Media::Cover)].size() << " cached covers";
	}

	bool RommCatalog::hasGamesFor(const std::string& systemName)
	{
		return sActive && sSystemsWithGames.find(systemName) != sSystemsWithGames.cend();
	}

	std::set<std::string> RommCatalog::getDefinedSystems()
	{
		std::unique_lock<std::mutex> lock(sMutex);
		return sDefinedSystems;
	}

	std::string RommCatalog::getSystemFullName(const std::string& systemName)
	{
		std::unique_lock<std::mutex> lock(sMutex);

		auto it = sSystemFullNames.find(systemName);
		return it == sSystemFullNames.cend() ? systemName : it->second;
	}

	bool RommCatalog::getKnownGame(const std::string& path, RemoteGame& game)
	{
		std::unique_lock<std::mutex> lock(sMutex);

		auto it = sKnownGames.find(path);
		if (it == sKnownGames.cend())
			return false;

		game = it->second;
		return true;
	}

	void RommCatalog::registerRemote(const std::string& path, const RemoteGame& game)
	{
		std::unique_lock<std::mutex> lock(sMutex);
		sRemoteGames[path] = game;
	}

	int RommCatalog::getRomId(const std::string& path)
	{
		std::unique_lock<std::mutex> lock(sMutex);

		auto it = sRomIds.find(path);
		return it == sRomIds.cend() ? 0 : it->second;
	}

	std::string RommCatalog::getSystemPath(const std::string& systemName)
	{
		std::unique_lock<std::mutex> lock(sMutex);

		auto it = sSystemPaths.find(systemName);
		return it == sSystemPaths.cend() ? "" : it->second;
	}

	std::set<std::string> RommCatalog::getSystemExtensions(const std::string& systemName)
	{
		std::unique_lock<std::mutex> lock(sMutex);

		auto it = sSystemExtensions.find(systemName);
		return it == sSystemExtensions.cend() ? std::set<std::string>() : it->second;
	}

	size_t RommCatalog::getInjectedCount()
	{
		std::unique_lock<std::mutex> lock(sMutex);
		return sRemoteGames.size();
	}

	bool RommCatalog::getRemoteGame(const std::string& path, RemoteGame& game)
	{
		std::unique_lock<std::mutex> lock(sMutex);

		auto it = sRemoteGames.find(path);
		if (it == sRemoteGames.cend())
			return false;

		game = it->second;
		return true;
	}

	void RommCatalog::inject(SystemData* system, std::unordered_map<std::string, FileData*>& fileMap, bool afterGamelist)
	{
		if (!hasGamesFor(system->getName()))
			return;

		std::vector<CatalogRecord> records;
		if (!readSnapshotFile(getLibraryDir(sBaseUrl) + "/" + system->getName() + ".xml", records))
			return;

		const std::string startPath = system->getStartPath();
		const std::string systemName = system->getName();

		// RomM favorite, for RomM games and for the same games already on the device.
		// Never written to gamelist.xml: re-applied at every load (RomM is the source).
		auto applyExtras = [&](FileData* game, const CatalogRecord& r, const std::string& fileName)
		{
			MetaDataList& md = game->getMetadata();
			bool changed = false;

			if (!sFavoriteKeys.empty() && sFavoriteKeys.find(systemName + "/" + fileName) != sFavoriteKeys.cend())
			{
				md.set(MetaDataId::Favorite, "true");
				changed = true;
			}

			if (changed)
				md.resetChangedFlag();
		};
		std::unordered_map<std::string, RemoteGame> registered;
		std::unordered_map<std::string, int> romIds;
		std::unordered_map<std::string, RemoteGame> known;
		int added = 0;

		for (auto& r : records)
		{
			// A disc listed by a loose playlist: the playlist is the game
			if (r.inPlaylist > 0)
				continue;

			// Never let a server-provided name escape the system folder
			std::string fileName = Utils::FileSystem::getFileName(r.fileName);
			if (fileName.empty() || fileName == "." || fileName == "..")
				continue;

			std::string path = Utils::FileSystem::getGenericPath(startPath + "/" + fileName);

			RemoteGame remote;
			remote.romId = r.romId;
			remote.fileId = r.fileId;
			remote.size = r.size;
			remote.fileName = fileName;
			for (size_t i = 0; i < (size_t)Media::Count; i++)
			{
				remote.mediaUrl[i] = r.mediaUrl[i];
				remote.mediaFile[i] = r.mediaFile[i];
			}

			remote.kind = r.kind;
			remote.folderName = r.folderName;
			remote.parts = r.parts;
			remote.discs = r.discs;

			auto existing = fileMap.find(path);
			if (existing != fileMap.cend())
			{
				// Normal mode: this entry comes from the disk scan, the local file wins.
				// ParseGamelistOnly mode: it may come from a gamelist.xml entry (user data saved
				// on a remote game) : keep its metadata, flag it remote if the file is missing.
				if (afterGamelist && existing->second->getType() == GAME && !Utils::FileSystem::exists(path, false))
				{
					existing->second->setRemote(true);
					registered[path] = remote;
				}

				if (existing->second->getType() == GAME)
				{
					applyExtras(existing->second, r, fileName);
					romIds[path] = r.romId; // a RomM game already on the device
					known[path] = remote;
				}

				continue;
			}

			// In ParseGamelistOnly mode the disk is not scanned: a local file absent from gamelist.xml stays local
			if (afterGamelist && Utils::FileSystem::exists(path, false))
				continue;

			FileData* game = findOrCreateFile(system, path, GAME, fileMap);
			if (game == nullptr)
				continue; // unknown extension for this system, or outside its folder

			if (game->getParent() == nullptr)
			{
				// Arcade asset (bios, device...) : findOrCreateFile did not attach it to the tree
				delete game;
				continue;
			}

			game->setRemote(true);

			MetaDataList& md = game->getMetadata();
			if (!r.name.empty())        md.set(MetaDataId::Name, r.name);
			if (!r.desc.empty())        md.set(MetaDataId::Desc, r.desc);
			if (!r.releaseDate.empty()) md.set(MetaDataId::ReleaseDate, r.releaseDate);
			if (!r.developer.empty())   md.set(MetaDataId::Developer, r.developer);
			if (!r.publisher.empty())   md.set(MetaDataId::Publisher, r.publisher);
			if (!r.genre.empty())       md.set(MetaDataId::Genre, r.genre);
			if (!r.players.empty())     md.set(MetaDataId::Players, r.players);
			if (!r.rating.empty())      md.set(MetaDataId::Rating, r.rating);

			// RetroAchievements: trophy icon, "has achievements" filter, saved to gamelist.xml after download
			if (r.cheevosId > 0)        md.set(MetaDataId::CheevosId, std::to_string(r.cheevosId));
			if (!r.cheevosHash.empty()) md.set(MetaDataId::CheevosHash, r.cheevosHash);

			// Media cached from the library
			for (auto& m : allMedia())
			{
				size_t i = mediaIndex(m.type);
				if (m.imageOnly)
					continue; // only as the image shown, below

				if (!r.mediaFile[i].empty() && sCachedMedia[i].find(r.mediaFile[i]) != sCachedMedia[i].cend())
					md.set(m.metadata, sMediaDir[i] + "/" + r.mediaFile[i]);
			}

			// IMAGE SHOWN: the chosen media is the ES image when the game has it (else the cover)
			if (sImageMedia >= 0)
			{
				size_t i = (size_t)sImageMedia;
				if (!r.mediaFile[i].empty() && sCachedMedia[i].find(r.mediaFile[i]) != sCachedMedia[i].cend())
					md.set(MetaDataId::Image, sMediaDir[i] + "/" + r.mediaFile[i]);
			}

			applyExtras(game, r, fileName);

			Genres::convertGenreToGenreIds(&md);

			// Injected data must never be written into the user's gamelist.xml
			md.resetChangedFlag();

			registered[path] = remote;
			romIds[path] = r.romId;
			known[path] = remote;
			added++;
		}

		{
			std::unique_lock<std::mutex> lock(sMutex);
			for (auto& item : registered)
				sRemoteGames[item.first] = item.second;

			for (auto& item : romIds)
				sRomIds[item.first] = item.second;

			for (auto& item : known)
				sKnownGames[item.first] = item.second;
		}

		LOG(LogInfo) << "RomM: " << system->getName() << " : " << added << " remote games added";
	}
}
