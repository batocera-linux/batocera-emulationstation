#include "GameDatabase.h"

#include "SystemData.h"
#include "FileData.h"
#include "Log.h"
#include "MetaData.h"
#include "Genres.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <sqlite3/sqlite3.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

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
			metadata TEXT,
			last_synced TEXT DEFAULT CURRENT_TIMESTAMP,
			UNIQUE(system, path)
		);

		CREATE INDEX IF NOT EXISTS idx_games_system ON games(system);
		CREATE INDEX IF NOT EXISTS idx_games_system_path ON games(system, path);
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

// Serialize metadata to JSON string
static std::string metadataToJson(FileData* game)
{
	const MetaDataList& md = game->getMetadata();
	const auto& mdd = MetaDataList::getMDD();

	rapidjson::StringBuffer sb;
	rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
	writer.StartObject();

	for (const auto& decl : mdd)
	{
		std::string val = md.get(decl.id, false);
		if (val.empty() || val == decl.defaultValue)
			continue;

		writer.Key(decl.key.c_str());
		writer.String(val.c_str());
	}

	writer.EndObject();
	return sb.GetString();
}

// Deserialize JSON string into metadata
static void jsonToMetadata(const std::string& json, MetaDataList& md, SystemData* system)
{
	md.setRelativeTo(system);

	if (json.empty())
		return;

	rapidjson::Document doc;
	doc.Parse(json.c_str());
	if (doc.HasParseError() || !doc.IsObject())
		return;

	const auto& mdd = MetaDataList::getMDD();

	for (const auto& decl : mdd)
	{
		auto it = doc.FindMember(decl.key.c_str());
		if (it != doc.MemberEnd() && it->value.IsString())
		{
			std::string val = it->value.GetString();
			if (!val.empty())
				md.set(decl.id, val);
		}
	}

	Genres::convertGenreToGenreIds(&md);
	md.resetChangedFlag();
}

bool GameDatabase::loadSystem(SystemData* system, std::unordered_map<std::string, FileData*>& fileMap)
{
	if (mDb == nullptr)
		return false;

	FolderData* root = system->getRootFolder();
	const std::string& systemName = system->getName();

	sqlite3_stmt* stmt = prepare(
		"SELECT path, type, metadata FROM games WHERE system = ? ORDER BY path");
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
	std::string metadata = getColumnText(stmt, 2); // metadata JSON

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

			// Deserialize metadata from JSON
			MetaDataList& md = game->getMetadata();
			jsonToMetadata(metadata, md, system);

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

	exec("BEGIN TRANSACTION");

	std::vector<std::string> validPaths;
	validPaths.reserve(games.size());

	sqlite3_stmt* stmt = prepare(R"(
		INSERT INTO games (system, path, type, metadata, last_synced)
		VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)
		ON CONFLICT(system, path) DO UPDATE SET
			type=excluded.type,
			metadata=excluded.metadata,
			last_synced=CURRENT_TIMESTAMP
	)");

	if (!stmt)
	{
		exec("ROLLBACK");
		return false;
	}

	for (auto* game : games)
	{
		std::string json = metadataToJson(game);

		sqlite3_bind_text(stmt, 1, systemName.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, game->getPath().c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 3, game->getType());
		sqlite3_bind_text(stmt, 4, json.c_str(), -1, SQLITE_TRANSIENT);

		sqlite3_step(stmt);
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
		validPaths.push_back(game->getPath());
	}

	sqlite3_finalize(stmt);

	exec("COMMIT");

	// Remove games no longer on disk (outside transaction to avoid temp table conflicts)
	removeStaleGames(systemName, validPaths);

	LOG(LogInfo) << "GameDatabase: Synced " << games.size() << " games for " << systemName;
	return true;
}

bool GameDatabase::upsertGame(const std::string& systemName, FileData* game)
{
	std::lock_guard<std::mutex> lock(mMutex);

	if (mDb == nullptr || game == nullptr)
		return false;

	sqlite3_stmt* stmt = prepare(R"(
		INSERT INTO games (system, path, type, metadata, last_synced)
		VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)
		ON CONFLICT(system, path) DO UPDATE SET
			type=excluded.type,
			metadata=excluded.metadata,
			last_synced=CURRENT_TIMESTAMP
	)");

	if (!stmt)
		return false;

	std::string json = metadataToJson(game);

	sqlite3_bind_text(stmt, 1, systemName.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, game->getPath().c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 3, game->getType());
	sqlite3_bind_text(stmt, 4, json.c_str(), -1, SQLITE_TRANSIENT);

	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}

bool GameDatabase::removeGame(const std::string& systemName, const std::string& path)
{
	std::lock_guard<std::mutex> lock(mMutex);

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
