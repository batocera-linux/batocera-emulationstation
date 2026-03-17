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
	exec("PRAGMA foreign_keys=ON");

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

static const char* CURRENT_SCHEMA_VERSION = "1";

bool GameDatabase::createTables()
{
	// Create db_meta table first (if it doesn't exist)
	exec(R"(
		CREATE TABLE IF NOT EXISTS db_meta (
			key TEXT PRIMARY KEY,
			value TEXT
		);
	)");

	// Check schema version — if outdated, drop and recreate
	sqlite3_stmt* stmt = prepare("SELECT value FROM db_meta WHERE key = 'schema_version'");
	if (stmt)
	{
		if (sqlite3_step(stmt) == SQLITE_ROW)
		{
			const unsigned char* text = sqlite3_column_text(stmt, 0);
			std::string version = text ? std::string(reinterpret_cast<const char*>(text)) : std::string();
			if (version != CURRENT_SCHEMA_VERSION)
			{
				LOG(LogInfo) << "GameDatabase: Schema version changed (" << version << " -> " << CURRENT_SCHEMA_VERSION << "), rebuilding cache";
				sqlite3_finalize(stmt);
				exec("DROP TABLE IF EXISTS game_metadata");
				exec("DROP TABLE IF EXISTS games");
				exec("DELETE FROM db_meta");
			}
			else
			{
				sqlite3_finalize(stmt);
				return true; // schema is current, tables exist
			}
		}
		else
		{
			sqlite3_finalize(stmt);
		}
	}

	// Create tables
	bool ok = exec(R"(
		CREATE TABLE IF NOT EXISTS games (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			system TEXT NOT NULL,
			path TEXT NOT NULL,
			type INTEGER NOT NULL DEFAULT 0,
			last_synced TEXT DEFAULT CURRENT_TIMESTAMP,
			UNIQUE(system, path)
		);

		CREATE INDEX IF NOT EXISTS idx_games_system ON games(system);

		CREATE TABLE IF NOT EXISTS game_metadata (
			game_id INTEGER NOT NULL,
			key TEXT NOT NULL,
			value TEXT NOT NULL,
			PRIMARY KEY(game_id, key),
			FOREIGN KEY(game_id) REFERENCES games(id) ON DELETE CASCADE
		);

		CREATE INDEX IF NOT EXISTS idx_gm_game_id ON game_metadata(game_id);
	)");

	if (ok)
		exec("INSERT OR REPLACE INTO db_meta (key, value) VALUES ('schema_version', '" + std::string(CURRENT_SCHEMA_VERSION) + "')");

	return ok;
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

	// Step 1: Load all games for this system
	sqlite3_stmt* gameStmt = prepare(
		"SELECT id, path, type FROM games WHERE system = ? ORDER BY path");
	if (!gameStmt)
		return false;

	sqlite3_bind_text(gameStmt, 1, systemName.c_str(), -1, SQLITE_STATIC);

	struct GameRow { int64_t id; std::string path; int type; };
	std::vector<GameRow> gameRows;

	while (sqlite3_step(gameStmt) == SQLITE_ROW)
	{
		GameRow row;
		row.id = sqlite3_column_int64(gameStmt, 0);
		row.path = getColumnText(gameStmt, 1);
		row.type = sqlite3_column_int(gameStmt, 2);
		gameRows.push_back(std::move(row));
	}
	sqlite3_finalize(gameStmt);

	if (gameRows.empty())
		return false;

	// Step 2: Load all metadata for these games in one query
	// Build a map of game_id -> vector of key/value pairs
	std::unordered_map<int64_t, std::vector<std::pair<std::string, std::string>>> metadataMap;

	// Load metadata for all games of this system via a join
	sqlite3_stmt* metaStmt = prepare(
		"SELECT gm.game_id, gm.key, gm.value FROM game_metadata gm "
		"INNER JOIN games g ON gm.game_id = g.id "
		"WHERE g.system = ? ORDER BY gm.game_id");
	if (metaStmt)
	{
		sqlite3_bind_text(metaStmt, 1, systemName.c_str(), -1, SQLITE_STATIC);

		while (sqlite3_step(metaStmt) == SQLITE_ROW)
		{
			int64_t gameId = sqlite3_column_int64(metaStmt, 0);
			std::string key = getColumnText(metaStmt, 1);
			std::string value = getColumnText(metaStmt, 2);
			metadataMap[gameId].emplace_back(std::move(key), std::move(value));
		}
		sqlite3_finalize(metaStmt);
	}

	// Step 3: Create FileData objects
	int count = 0;
	for (const auto& row : gameRows)
	{
		FileData* game = loadGameFromRow(row.id, row.path, row.type, metadataMap, system, root, fileMap);
		if (game)
			count++;
	}

	LOG(LogInfo) << "GameDatabase: Loaded " << count << " games for " << systemName << " from cache";
	return count > 0;
}

FileData* GameDatabase::loadGameFromRow(int64_t gameId, const std::string& path, int type,
	const std::unordered_map<int64_t, std::vector<std::pair<std::string, std::string>>>& metadataMap,
	SystemData* system, FolderData* root,
	std::unordered_map<std::string, FileData*>& fileMap)
{
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

			// Set metadata from key-value pairs
			MetaDataList& md = game->getMetadata();
			md.setRelativeTo(system);

			auto it = metadataMap.find(gameId);
			if (it != metadataMap.end())
			{
				for (const auto& kv : it->second)
				{
					MetaDataId id = md.getId(kv.first);
					if (id != MetaDataId::Name || !kv.second.empty()) // getId returns Name for unknown keys
						md.set(id, kv.second);
				}
			}

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

	std::lock_guard<std::mutex> lock(mMutex);

	const std::string& systemName = system->getName();
	FolderData* root = system->getRootFolder();
	if (root == nullptr)
		return false;

	auto games = root->getFilesRecursive(GAME, false, nullptr, true);
	const auto& mdd = MetaDataList::getMDD();

	exec("BEGIN TRANSACTION");

	std::vector<std::string> validPaths;
	validPaths.reserve(games.size());

	// Prepared statements
	sqlite3_stmt* gameStmt = prepare(R"(
		INSERT INTO games (system, path, type, last_synced)
		VALUES (?, ?, ?, CURRENT_TIMESTAMP)
		ON CONFLICT(system, path) DO UPDATE SET
			type=excluded.type,
			last_synced=CURRENT_TIMESTAMP
		RETURNING id
	)");

	sqlite3_stmt* deleteMetaStmt = prepare("DELETE FROM game_metadata WHERE game_id = ?");

	sqlite3_stmt* metaStmt = prepare(
		"INSERT INTO game_metadata (game_id, key, value) VALUES (?, ?, ?)");

	if (!gameStmt || !deleteMetaStmt || !metaStmt)
	{
		if (gameStmt) sqlite3_finalize(gameStmt);
		if (deleteMetaStmt) sqlite3_finalize(deleteMetaStmt);
		if (metaStmt) sqlite3_finalize(metaStmt);
		exec("ROLLBACK");
		return false;
	}

	for (auto* game : games)
	{
		const MetaDataList& md = game->getMetadata();

		// Upsert game row and get id
		sqlite3_bind_text(gameStmt, 1, systemName.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(gameStmt, 2, game->getPath().c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(gameStmt, 3, game->getType());

		int64_t gameId = -1;
		if (sqlite3_step(gameStmt) == SQLITE_ROW)
			gameId = sqlite3_column_int64(gameStmt, 0);

		sqlite3_reset(gameStmt);
		sqlite3_clear_bindings(gameStmt);

		if (gameId < 0)
			continue;

		// Delete old metadata for this game
		sqlite3_bind_int64(deleteMetaStmt, 1, gameId);
		sqlite3_step(deleteMetaStmt);
		sqlite3_reset(deleteMetaStmt);
		sqlite3_clear_bindings(deleteMetaStmt);

		// Insert metadata key-value pairs
		for (const auto& decl : mdd)
		{
			std::string val = md.get(decl.id, false);
			if (val.empty() || val == decl.defaultValue)
				continue;

			sqlite3_bind_int64(metaStmt, 1, gameId);
			sqlite3_bind_text(metaStmt, 2, decl.key.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_text(metaStmt, 3, val.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_step(metaStmt);
			sqlite3_reset(metaStmt);
			sqlite3_clear_bindings(metaStmt);
		}

		validPaths.push_back(game->getPath());
	}

	sqlite3_finalize(gameStmt);
	sqlite3_finalize(deleteMetaStmt);
	sqlite3_finalize(metaStmt);

	exec("COMMIT");

	// Remove games no longer on disk (outside transaction to avoid temp table conflicts)
	// CASCADE will clean up game_metadata automatically
	removeStaleGames(systemName, validPaths);

	LOG(LogInfo) << "GameDatabase: Synced " << games.size() << " games for " << systemName;
	return true;
}

bool GameDatabase::upsertGame(const std::string& systemName, FileData* game)
{
	std::lock_guard<std::mutex> lock(mMutex);

	if (mDb == nullptr || game == nullptr)
		return false;

	const MetaDataList& md = game->getMetadata();
	const auto& mdd = MetaDataList::getMDD();

	exec("BEGIN TRANSACTION");

	// Upsert game row
	sqlite3_stmt* gameStmt = prepare(R"(
		INSERT INTO games (system, path, type, last_synced)
		VALUES (?, ?, ?, CURRENT_TIMESTAMP)
		ON CONFLICT(system, path) DO UPDATE SET
			type=excluded.type,
			last_synced=CURRENT_TIMESTAMP
		RETURNING id
	)");

	if (!gameStmt)
	{
		exec("ROLLBACK");
		return false;
	}

	sqlite3_bind_text(gameStmt, 1, systemName.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(gameStmt, 2, game->getPath().c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(gameStmt, 3, game->getType());

	int64_t gameId = -1;
	if (sqlite3_step(gameStmt) == SQLITE_ROW)
		gameId = sqlite3_column_int64(gameStmt, 0);

	sqlite3_finalize(gameStmt);

	if (gameId < 0)
	{
		exec("ROLLBACK");
		return false;
	}

	// Delete old metadata
	sqlite3_stmt* deleteStmt = prepare("DELETE FROM game_metadata WHERE game_id = ?");
	if (deleteStmt)
	{
		sqlite3_bind_int64(deleteStmt, 1, gameId);
		sqlite3_step(deleteStmt);
		sqlite3_finalize(deleteStmt);
	}

	// Insert new metadata
	sqlite3_stmt* metaStmt = prepare(
		"INSERT INTO game_metadata (game_id, key, value) VALUES (?, ?, ?)");

	if (metaStmt)
	{
		for (const auto& decl : mdd)
		{
			std::string val = md.get(decl.id, false);
			if (val.empty() || val == decl.defaultValue)
				continue;

			sqlite3_bind_int64(metaStmt, 1, gameId);
			sqlite3_bind_text(metaStmt, 2, decl.key.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_text(metaStmt, 3, val.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_step(metaStmt);
			sqlite3_reset(metaStmt);
			sqlite3_clear_bindings(metaStmt);
		}
		sqlite3_finalize(metaStmt);
	}

	exec("COMMIT");
	return true;
}

bool GameDatabase::removeGame(const std::string& systemName, const std::string& path)
{
	std::lock_guard<std::mutex> lock(mMutex);

	if (mDb == nullptr)
		return false;

	// CASCADE will delete game_metadata rows automatically
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

	// Delete games not in the valid paths set (CASCADE handles game_metadata)
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
