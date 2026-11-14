#pragma once
#ifndef ES_APP_ROMM_ROMM_CATALOG_H
#define ES_APP_ROMM_ROMM_CATALOG_H

#include "romm/RommMedia.h"

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace pugi { class xml_node; }
class SystemData;
class FileData;

namespace Romm
{
	// One downloadable RomM game, as stored in the local snapshot.
	struct CatalogRecord
	{
		int romId = 0;
		int platformId = 0;
		int fileId = 0;
		long long size = 0;
		std::string system;        // ES system name
		std::string fileName;      // final file name in the system's rom folder
		std::string updatedAt;     // RomM updated_at (ISO 8601)
		// Multi-file games ("" = one file, fileId). The ES entry is fileName in the system folder:
		//   "m3u"    : <name>.m3u, its files in the <name>/ folder beside it (ES hides the discs a
		//              playlist lists, and the folder once empty); discs = the playlist, in order
		//   "folder" : <name>.<ext>/ is the game (systems whose games are folders: dos .pc, ps3 .ps3...)
		//   "loose"  : a .m3u RomM lists as a game of its own, beside the games it lists (the discs);
		//              parts = those games' files (romId), discs = the playlist lines
		struct Part { int id = 0; std::string path; long long size = 0; int romId = 0; }; // path relative to the game folder; romId 0: this game
		int inPlaylist = 0; // a disc listed by a loose playlist (its rom id): not shown on its own
		std::string kind;
		std::string folderName;
		std::vector<Part> parts;
		std::vector<std::string> discs;

		// Per media type (RommMedia.h): where it is on the server (a resource path, or the content
		// endpoint for documents uploaded to RomM), and its file name in that media's cache folder
		std::string mediaUrl[(size_t)Media::Count];
		std::string mediaFile[(size_t)Media::Count];
		int cheevosId = 0;         // RetroAchievements game id (RomM ra_id)
		std::string cheevosHash;   // RetroAchievements hash, uppercase like ES stores it (RomM ra_hash)

		std::string name, desc, releaseDate, developer, publisher, genre, players, rating;
	};

	// What the downloader needs for a remote game, looked up by its ES path.
	struct RemoteGame
	{
		int romId = 0;
		int fileId = 0;
		long long size = 0;
		std::string fileName;
		std::string mediaUrl[(size_t)Media::Count];
		std::string mediaFile[(size_t)Media::Count];

		// Multi-file games (CatalogRecord)
		std::string kind;
		std::string folderName;
		std::vector<CatalogRecord::Part> parts;
		std::vector<std::string> discs;
	};

	// Snapshot files: <server cache>/library/<system>.xml
	std::string getLibraryDir(const std::string& baseUrl);
	bool readSnapshotFile(const std::string& path, std::vector<CatalogRecord>& records);
	bool writeSnapshotFile(const std::string& path, const std::string& system, const std::vector<CatalogRecord>& records);

	class RommCatalog
	{
	public:
		// SystemData::loadConfig, single threaded, before any system is built.
		// Reads the configuration and indexes the snapshot. Does nothing when RomM is disabled.
		static void prepare(pugi::xml_node& systemList);

		static bool isActive() { return sActive; }
		static bool hasGamesFor(const std::string& systemName);

		// SystemData constructor (systems are built in parallel threads).
		// afterGamelist = false : called right after the disk scan (normal mode); local files win.
		// afterGamelist = true  : called after gamelist.xml parsing (ParseGamelistOnly mode).
		static void inject(SystemData* system, std::unordered_map<std::string, FileData*>& fileMap, bool afterGamelist);

		static bool getRemoteGame(const std::string& path, RemoteGame& game);

		// Any RomM game ES shows (not downloaded yet, or already on the device)
		static bool getKnownGame(const std::string& path, RemoteGame& game);

		// A RomM game whose files were deleted from the device: offered for download again
		static void registerRemote(const std::string& path, const RemoteGame& game);

		// RomM id of a game ES shows (not downloaded yet, or already on the device), 0 otherwise
		static int getRomId(const std::string& path);

		// Every system defined in es_systems (loaded or not), for the platform mapping
		static std::set<std::string> getDefinedSystems();

		static size_t getInjectedCount();

		// Rom folder of a system (normalized like SystemData), "" if unknown
		static std::string getSystemPath(const std::string& systemName);

		// Extensions a system accepts (lower case, with the dot), from es_systems.cfg
		static std::set<std::string> getSystemExtensions(const std::string& systemName);

		// Full name from es_systems (the system may not be loaded)
		static std::string getSystemFullName(const std::string& systemName);

	private:
		static bool sActive;
		static std::string sBaseUrl;
		static std::string sMediaDir[(size_t)Media::Count];
		static std::unordered_set<std::string> sSystemsWithGames;
		static std::unordered_set<std::string> sCachedMedia[(size_t)Media::Count];
		static int sImageMedia; // IMAGE SHOWN: media index, -1 automatic
		static std::map<std::string, std::string> sSystemPaths; // rom folder of every system, normalized like SystemData
		static std::map<std::string, std::set<std::string>> sSystemExtensions;
		static std::unordered_set<std::string> sFavoriteKeys; // "system/file name" of RomM favorites (when synced)

		static std::mutex sMutex; // guards the members below
		static std::unordered_map<std::string, RemoteGame> sRemoteGames;
		static std::unordered_map<std::string, int> sRomIds;
		static std::unordered_map<std::string, RemoteGame> sKnownGames;
		static std::set<std::string> sDefinedSystems;
		static std::map<std::string, std::string> sSystemFullNames;
	};
}

#endif
