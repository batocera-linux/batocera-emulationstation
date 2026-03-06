#include "GameDatabase.h"

#include "SystemData.h"
#include "FileData.h"
#include "Log.h"
#include "MetaData.h"
#include "Genres.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <sqlite3/sqlite3.h>

GameDatabase* GameDatabase::sInstance = nullptr;

GameDatabase::GameDatabase() : mDb(nullptr)
{
}

GameDatabase::~GameDatabase()
{
	deinit();
}

GameDatabase* GameDatabase::getInstance()
{
	if (sInstance == nullptr)
		sInstance = new GameDatabase();

	return sInstance;
}

bool GameDatabase::init(const std::string& dbPath)
{
	if (mDb != nullptr)
		return true;

	mDbPath = dbPath;

	int rc = sqlite3_open(dbPath.c_str(), &mDb);
	if (rc != SQLITE_OK)
	{
		LOG(LogError) << "GameDatabase: Failed to open database at " << dbPath << ": " << sqlite3_errmsg(mDb);
		sqlite3_close(mDb);
		mDb = nullptr;
		return false;
	}

	// Enable WAL mode for better concurrent read performance
	exec("PRAGMA journal_mode=WAL");
	exec("PRAGMA synchronous=NORMAL");

	if (!createTables())
	{
		LOG(LogError) << "GameDatabase: Failed to create tables";
		deinit();
		return false;
	}

	LOG(LogInfo) << "GameDatabase: Opened database at " << dbPath;
	return true;
}

void GameDatabase::deinit()
{
	if (mDb != nullptr)
	{
		sqlite3_close(mDb);
		mDb = nullptr;
		LOG(LogInfo) << "GameDatabase: Closed database";
	}
}

bool GameDatabase::createTables()
{
	return exec(R"(
		CREATE TABLE IF NOT EXISTS games (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			system TEXT NOT NULL,
			path TEXT NOT NULL,
			type INTEGER NOT NULL DEFAULT 0,
			name TEXT,
			sortname TEXT,
			desc TEXT,
			emulator TEXT,
			core TEXT,
			image TEXT,
			video TEXT,
			marquee TEXT,
			thumbnail TEXT,
			fanart TEXT,
			titleshot TEXT,
			cartridge TEXT,
			boxart TEXT,
			boxback TEXT,
			wheel TEXT,
			mix TEXT,
			manual TEXT,
			magazine TEXT,
			map TEXT,
			bezel TEXT,
			rating TEXT,
			releasedate TEXT,
			developer TEXT,
			publisher TEXT,
			genre TEXT,
			genre_ids TEXT,
			arcade_system TEXT,
			players TEXT,
			favorite INTEGER DEFAULT 0,
			hidden INTEGER DEFAULT 0,
			kidgame INTEGER DEFAULT 0,
			playcount INTEGER DEFAULT 0,
			lastplayed TEXT,
			crc32 TEXT,
			md5 TEXT,
			gametime INTEGER DEFAULT 0,
			lang TEXT,
			region TEXT,
			cheevos_hash TEXT,
			cheevos_id TEXT,
			scraper_id TEXT,
			family TEXT,
			tags TEXT,
			last_synced TEXT DEFAULT CURRENT_TIMESTAMP,
			UNIQUE(system, path)
		);

		CREATE INDEX IF NOT EXISTS idx_games_system ON games(system);
		CREATE INDEX IF NOT EXISTS idx_games_system_path ON games(system, path);

		CREATE TABLE IF NOT EXISTS db_meta (
			key TEXT PRIMARY KEY,
			value TEXT
		);

		INSERT OR IGNORE INTO db_meta (key, value)
			VALUES ('schema_version', '1');
	)");
}

bool GameDatabase::exec(const std::string& sql)
{
	if (mDb == nullptr)
		return false;

	char* errMsg = nullptr;
	int rc = sqlite3_exec(mDb, sql.c_str(), nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK)
	{
		LOG(LogError) << "GameDatabase: SQL error: " << (errMsg ? errMsg : "unknown");
		sqlite3_free(errMsg);
		return false;
	}
	return true;
}

sqlite3_stmt* GameDatabase::prepare(const std::string& sql)
{
	if (mDb == nullptr)
		return nullptr;

	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(mDb, sql.c_str(), -1, &stmt, nullptr);
	if (rc != SQLITE_OK)
	{
		LOG(LogError) << "GameDatabase: Failed to prepare statement: " << sqlite3_errmsg(mDb);
		return nullptr;
	}
	return stmt;
}

bool GameDatabase::hasSystemCache(const std::string& systemName)
{
	if (mDb == nullptr)
		return false;

	sqlite3_stmt* stmt = prepare("SELECT COUNT(*) FROM games WHERE system = ?");
	if (!stmt)
		return false;

	sqlite3_bind_text(stmt, 1, systemName.c_str(), -1, SQLITE_STATIC);

	bool hasCache = false;
	if (sqlite3_step(stmt) == SQLITE_ROW)
		hasCache = sqlite3_column_int(stmt, 0) > 0;

	sqlite3_finalize(stmt);
	return hasCache;
}

// Helper to safely get text from a column (returns empty string for NULL)
static std::string getColumnText(sqlite3_stmt* stmt, int col)
{
	const unsigned char* text = sqlite3_column_text(stmt, col);
	return text ? std::string(reinterpret_cast<const char*>(text)) : std::string();
}

bool GameDatabase::loadSystem(SystemData* system, std::unordered_map<std::string, FileData*>& fileMap)
{
	if (mDb == nullptr)
		return false;

	FolderData* root = system->getRootFolder();
	const std::string& systemName = system->getName();

	sqlite3_stmt* stmt = prepare(
		"SELECT path, type, name, sortname, desc, emulator, core, "
		"image, video, marquee, thumbnail, fanart, titleshot, cartridge, "
		"boxart, boxback, wheel, mix, manual, magazine, map, bezel, "
		"rating, releasedate, developer, publisher, genre, genre_ids, "
		"arcade_system, players, favorite, hidden, kidgame, playcount, "
		"lastplayed, crc32, md5, gametime, lang, region, "
		"cheevos_hash, cheevos_id, scraper_id, family, tags "
		"FROM games WHERE system = ? ORDER BY path");
	if (!stmt)
		return false;

	sqlite3_bind_text(stmt, 1, systemName.c_str(), -1, SQLITE_STATIC);

	int count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		FileData* game = loadGameFromRow(stmt, system, root, fileMap);
		if (game)
			count++;
	}

	sqlite3_finalize(stmt);

	LOG(LogInfo) << "GameDatabase: Loaded " << count << " games for " << systemName << " from cache";
	return count > 0;
}

FileData* GameDatabase::loadGameFromRow(sqlite3_stmt* stmt, SystemData* system, FolderData* root,
										std::unordered_map<std::string, FileData*>& fileMap)
{
	std::string path = getColumnText(stmt, 0);    // path
	int type = sqlite3_column_int(stmt, 1);        // type

	if (path.empty())
		return nullptr;

	// Check if this file already exists in the map
	auto existing = fileMap.find(path);
	if (existing != fileMap.end())
		return existing->second;

	// Skip if the extension is unknown by the system
	if (type == GAME && !system->getSystemEnvData()->isValidExtension(
			Utils::String::toLower(Utils::FileSystem::getExtension(path))))
		return nullptr;

	// Build folder hierarchy (same approach as findOrCreateFile in Gamelist.cpp)
	bool contains = false;
	std::string relative = Utils::FileSystem::removeCommonPath(path, root->getPath(), contains);
	if (!contains)
		return nullptr;

	auto pathList = Utils::FileSystem::getPathList(relative);
	auto path_it = pathList.begin();
	FolderData* treeNode = root;

	while (path_it != pathList.end())
	{
		std::string key = Utils::FileSystem::combine(treeNode->getPath(), *path_it);

		// Check if this path component already exists
		FileData* item = (fileMap.find(key) != fileMap.end()) ? fileMap[key] : nullptr;
		if (item != nullptr)
		{
			if (item->getType() == FOLDER)
				treeNode = (FolderData*)item;
			else
				return item; // Already exists as a game
		}

		// Last component = the game itself
		if (path_it == --pathList.end())
		{
			FileData* game = new FileData(GAME, path, system);

			if (game->isArcadeAsset())
			{
				delete game;
				return nullptr;
			}

			fileMap[key] = game;
			treeNode->addChild(game);

			// Set all metadata from DB columns
			MetaDataList& md = game->getMetadata();

			std::string name = getColumnText(stmt, 2);
			if (!name.empty())
				md.set(MetaDataId::Name, name);

			auto setIfNotEmpty = [&](MetaDataId id, int col) {
				std::string val = getColumnText(stmt, col);
				if (!val.empty())
					md.set(id, val);
			};

			setIfNotEmpty(MetaDataId::SortName,         3);
			setIfNotEmpty(MetaDataId::Desc,             4);
			setIfNotEmpty(MetaDataId::Emulator,         5);
			setIfNotEmpty(MetaDataId::Core,             6);
			setIfNotEmpty(MetaDataId::Image,            7);
			setIfNotEmpty(MetaDataId::Video,            8);
			setIfNotEmpty(MetaDataId::Marquee,          9);
			setIfNotEmpty(MetaDataId::Thumbnail,        10);
			setIfNotEmpty(MetaDataId::FanArt,           11);
			setIfNotEmpty(MetaDataId::TitleShot,        12);
			setIfNotEmpty(MetaDataId::Cartridge,        13);
			setIfNotEmpty(MetaDataId::BoxArt,           14);
			setIfNotEmpty(MetaDataId::BoxBack,          15);
			setIfNotEmpty(MetaDataId::Wheel,            16);
			setIfNotEmpty(MetaDataId::Mix,              17);
			setIfNotEmpty(MetaDataId::Manual,           18);
			setIfNotEmpty(MetaDataId::Magazine,         19);
			setIfNotEmpty(MetaDataId::Map,              20);
			setIfNotEmpty(MetaDataId::Bezel,            21);
			setIfNotEmpty(MetaDataId::Rating,           22);
			setIfNotEmpty(MetaDataId::ReleaseDate,      23);
			setIfNotEmpty(MetaDataId::Developer,        24);
			setIfNotEmpty(MetaDataId::Publisher,        25);
			setIfNotEmpty(MetaDataId::Genre,            26);
			setIfNotEmpty(MetaDataId::GenreIds,         27);
			setIfNotEmpty(MetaDataId::ArcadeSystemName, 28);
			setIfNotEmpty(MetaDataId::Players,          29);

			// Bool/int fields
			if (sqlite3_column_int(stmt, 30))
				md.set(MetaDataId::Favorite, "true");
			if (sqlite3_column_int(stmt, 31))
				md.set(MetaDataId::Hidden, "true");
			if (sqlite3_column_int(stmt, 32))
				md.set(MetaDataId::KidGame, "true");

			int playcount = sqlite3_column_int(stmt, 33);
			if (playcount > 0)
				md.set(MetaDataId::PlayCount, std::to_string(playcount));

			setIfNotEmpty(MetaDataId::LastPlayed,  34);
			setIfNotEmpty(MetaDataId::Crc32,       35);
			setIfNotEmpty(MetaDataId::Md5,         36);

			int gametime = sqlite3_column_int(stmt, 37);
			if (gametime > 0)
				md.set(MetaDataId::GameTime, std::to_string(gametime));

			setIfNotEmpty(MetaDataId::Language,    38);
			setIfNotEmpty(MetaDataId::Region,      39);
			setIfNotEmpty(MetaDataId::CheevosHash, 40);
			setIfNotEmpty(MetaDataId::CheevosId,   41);
			setIfNotEmpty(MetaDataId::ScraperId,   42);
			setIfNotEmpty(MetaDataId::Family,      43);
			setIfNotEmpty(MetaDataId::Tags,        44);

			Genres::convertGenreToGenreIds(&md);
			md.resetChangedFlag();

			return game;
		}

		// Intermediate path component — create folder if needed
		if (item == nullptr)
		{
			FolderData* folder = new FolderData(key, system);
			fileMap[key] = folder;
			treeNode->addChild(folder);
			treeNode = folder;
		}

		path_it++;
	}

	return nullptr;
}

bool GameDatabase::syncSystem(SystemData* system)
{
	if (mDb == nullptr)
		return false;

	const std::string& systemName = system->getName();
	FolderData* root = system->getRootFolder();
	if (root == nullptr)
		return false;

	auto games = root->getFilesRecursive(GAME, false, nullptr, true);

	exec("BEGIN TRANSACTION");

	std::vector<std::string> validPaths;
	validPaths.reserve(games.size());

	sqlite3_stmt* stmt = prepare(R"(
		INSERT INTO games (system, path, type, name, sortname, desc,
			emulator, core, image, video, marquee, thumbnail,
			fanart, titleshot, cartridge, boxart, boxback, wheel, mix,
			manual, magazine, map, bezel,
			rating, releasedate, developer, publisher, genre,
			genre_ids, arcade_system, players,
			favorite, hidden, kidgame, playcount, lastplayed,
			crc32, md5, gametime, lang, region,
			cheevos_hash, cheevos_id, scraper_id, family, tags,
			last_synced)
		VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,
				?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,
				CURRENT_TIMESTAMP)
		ON CONFLICT(system, path) DO UPDATE SET
			type=excluded.type,
			name=excluded.name, sortname=excluded.sortname,
			desc=excluded.desc, emulator=excluded.emulator, core=excluded.core,
			image=excluded.image, video=excluded.video, marquee=excluded.marquee,
			thumbnail=excluded.thumbnail, fanart=excluded.fanart,
			titleshot=excluded.titleshot, cartridge=excluded.cartridge,
			boxart=excluded.boxart, boxback=excluded.boxback,
			wheel=excluded.wheel, mix=excluded.mix,
			manual=excluded.manual, magazine=excluded.magazine,
			map=excluded.map, bezel=excluded.bezel,
			rating=excluded.rating, releasedate=excluded.releasedate,
			developer=excluded.developer, publisher=excluded.publisher,
			genre=excluded.genre, genre_ids=excluded.genre_ids,
			arcade_system=excluded.arcade_system, players=excluded.players,
			favorite=excluded.favorite, hidden=excluded.hidden,
			kidgame=excluded.kidgame, playcount=excluded.playcount,
			lastplayed=excluded.lastplayed,
			crc32=excluded.crc32, md5=excluded.md5,
			gametime=excluded.gametime, lang=excluded.lang, region=excluded.region,
			cheevos_hash=excluded.cheevos_hash, cheevos_id=excluded.cheevos_id,
			scraper_id=excluded.scraper_id, family=excluded.family,
			tags=excluded.tags,
			last_synced=CURRENT_TIMESTAMP
	)");

	if (!stmt)
	{
		exec("ROLLBACK");
		return false;
	}

	for (auto* game : games)
	{
		bindGameData(stmt, systemName, game);
		sqlite3_step(stmt);
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
		validPaths.push_back(game->getPath());
	}

	sqlite3_finalize(stmt);

	// Remove games no longer on disk
	removeStaleGames(systemName, validPaths);

	exec("COMMIT");

	LOG(LogInfo) << "GameDatabase: Synced " << games.size() << " games for " << systemName;
	return true;
}

void GameDatabase::bindGameData(sqlite3_stmt* stmt, const std::string& systemName, FileData* game)
{
	const MetaDataList& md = game->getMetadata();

	int col = 1;

	// system, path, type
	sqlite3_bind_text(stmt, col++, systemName.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, col++, game->getPath().c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, col++, game->getType());

	// Helper to bind text (use false for resolveRelativePaths to store relative paths)
	auto bindText = [&](MetaDataId id) {
		std::string val = md.get(id, false);
		if (val.empty())
			sqlite3_bind_null(stmt, col++);
		else
			sqlite3_bind_text(stmt, col++, val.c_str(), -1, SQLITE_TRANSIENT);
	};

	auto bindName = [&]() {
		std::string val = md.getName();
		if (val.empty())
			sqlite3_bind_null(stmt, col++);
		else
			sqlite3_bind_text(stmt, col++, val.c_str(), -1, SQLITE_TRANSIENT);
	};

	// name
	bindName();

	// sortname, desc, emulator, core
	bindText(MetaDataId::SortName);
	bindText(MetaDataId::Desc);
	bindText(MetaDataId::Emulator);
	bindText(MetaDataId::Core);

	// Media paths (store as relative)
	bindText(MetaDataId::Image);
	bindText(MetaDataId::Video);
	bindText(MetaDataId::Marquee);
	bindText(MetaDataId::Thumbnail);
	bindText(MetaDataId::FanArt);
	bindText(MetaDataId::TitleShot);
	bindText(MetaDataId::Cartridge);
	bindText(MetaDataId::BoxArt);
	bindText(MetaDataId::BoxBack);
	bindText(MetaDataId::Wheel);
	bindText(MetaDataId::Mix);
	bindText(MetaDataId::Manual);
	bindText(MetaDataId::Magazine);
	bindText(MetaDataId::Map);
	bindText(MetaDataId::Bezel);

	// rating, releasedate, developer, publisher, genre, genre_ids, arcade_system, players
	bindText(MetaDataId::Rating);
	bindText(MetaDataId::ReleaseDate);
	bindText(MetaDataId::Developer);
	bindText(MetaDataId::Publisher);
	bindText(MetaDataId::Genre);
	bindText(MetaDataId::GenreIds);
	bindText(MetaDataId::ArcadeSystemName);
	bindText(MetaDataId::Players);

	// favorite, hidden, kidgame (as int 0/1)
	sqlite3_bind_int(stmt, col++, md.get(MetaDataId::Favorite) == "true" ? 1 : 0);
	sqlite3_bind_int(stmt, col++, md.get(MetaDataId::Hidden) == "true" ? 1 : 0);
	sqlite3_bind_int(stmt, col++, md.get(MetaDataId::KidGame) == "true" ? 1 : 0);

	// playcount
	sqlite3_bind_int(stmt, col++, md.getInt(MetaDataId::PlayCount));

	// lastplayed
	bindText(MetaDataId::LastPlayed);

	// crc32, md5
	bindText(MetaDataId::Crc32);
	bindText(MetaDataId::Md5);

	// gametime
	sqlite3_bind_int(stmt, col++, md.getInt(MetaDataId::GameTime));

	// lang, region
	bindText(MetaDataId::Language);
	bindText(MetaDataId::Region);

	// cheevos_hash, cheevos_id, scraper_id
	bindText(MetaDataId::CheevosHash);
	bindText(MetaDataId::CheevosId);
	bindText(MetaDataId::ScraperId);

	// family, tags
	bindText(MetaDataId::Family);
	bindText(MetaDataId::Tags);
}

bool GameDatabase::upsertGame(const std::string& systemName, FileData* game)
{
	if (mDb == nullptr || game == nullptr)
		return false;

	sqlite3_stmt* stmt = prepare(R"(
		INSERT INTO games (system, path, type, name, sortname, desc,
			emulator, core, image, video, marquee, thumbnail,
			fanart, titleshot, cartridge, boxart, boxback, wheel, mix,
			manual, magazine, map, bezel,
			rating, releasedate, developer, publisher, genre,
			genre_ids, arcade_system, players,
			favorite, hidden, kidgame, playcount, lastplayed,
			crc32, md5, gametime, lang, region,
			cheevos_hash, cheevos_id, scraper_id, family, tags,
			last_synced)
		VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,
				?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,
				CURRENT_TIMESTAMP)
		ON CONFLICT(system, path) DO UPDATE SET
			type=excluded.type,
			name=excluded.name, sortname=excluded.sortname,
			desc=excluded.desc, emulator=excluded.emulator, core=excluded.core,
			image=excluded.image, video=excluded.video, marquee=excluded.marquee,
			thumbnail=excluded.thumbnail, fanart=excluded.fanart,
			titleshot=excluded.titleshot, cartridge=excluded.cartridge,
			boxart=excluded.boxart, boxback=excluded.boxback,
			wheel=excluded.wheel, mix=excluded.mix,
			manual=excluded.manual, magazine=excluded.magazine,
			map=excluded.map, bezel=excluded.bezel,
			rating=excluded.rating, releasedate=excluded.releasedate,
			developer=excluded.developer, publisher=excluded.publisher,
			genre=excluded.genre, genre_ids=excluded.genre_ids,
			arcade_system=excluded.arcade_system, players=excluded.players,
			favorite=excluded.favorite, hidden=excluded.hidden,
			kidgame=excluded.kidgame, playcount=excluded.playcount,
			lastplayed=excluded.lastplayed,
			crc32=excluded.crc32, md5=excluded.md5,
			gametime=excluded.gametime, lang=excluded.lang, region=excluded.region,
			cheevos_hash=excluded.cheevos_hash, cheevos_id=excluded.cheevos_id,
			scraper_id=excluded.scraper_id, family=excluded.family,
			tags=excluded.tags,
			last_synced=CURRENT_TIMESTAMP
	)");

	if (!stmt)
		return false;

	bindGameData(stmt, systemName, game);
	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}

bool GameDatabase::removeGame(const std::string& systemName, const std::string& path)
{
	if (mDb == nullptr)
		return false;

	sqlite3_stmt* stmt = prepare("DELETE FROM games WHERE system = ? AND path = ?");
	if (!stmt)
		return false;

	sqlite3_bind_text(stmt, 1, systemName.c_str(), -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, path.c_str(), -1, SQLITE_STATIC);

	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}

int GameDatabase::removeStaleGames(const std::string& systemName, const std::vector<std::string>& validPaths)
{
	if (mDb == nullptr || validPaths.empty())
		return 0;

	// Build a temporary table of valid paths for efficient comparison
	exec("CREATE TEMP TABLE IF NOT EXISTS valid_paths (path TEXT PRIMARY KEY)");
	exec("DELETE FROM valid_paths");

	sqlite3_stmt* insertStmt = prepare("INSERT INTO valid_paths (path) VALUES (?)");
	if (!insertStmt)
		return 0;

	for (const auto& p : validPaths)
	{
		sqlite3_bind_text(insertStmt, 1, p.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(insertStmt);
		sqlite3_reset(insertStmt);
		sqlite3_clear_bindings(insertStmt);
	}
	sqlite3_finalize(insertStmt);

	// Delete games not in the valid paths set
	sqlite3_stmt* deleteStmt = prepare(
		"DELETE FROM games WHERE system = ? AND path NOT IN (SELECT path FROM valid_paths)");
	if (!deleteStmt)
	{
		exec("DROP TABLE IF EXISTS valid_paths");
		return 0;
	}

	sqlite3_bind_text(deleteStmt, 1, systemName.c_str(), -1, SQLITE_STATIC);
	sqlite3_step(deleteStmt);

	int removed = sqlite3_changes(mDb);
	sqlite3_finalize(deleteStmt);

	exec("DROP TABLE IF EXISTS valid_paths");

	if (removed > 0)
		LOG(LogInfo) << "GameDatabase: Removed " << removed << " stale games for " << systemName;

	return removed;
}

void GameDatabase::vacuum()
{
	exec("VACUUM");
}
