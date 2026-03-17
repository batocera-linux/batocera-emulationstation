#pragma once
#ifndef ES_APP_GAME_DATABASE_H
#define ES_APP_GAME_DATABASE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

struct sqlite3;
struct sqlite3_stmt;

class SystemData;
class FileData;
class FolderData;

class GameDatabase
{
public:
	static GameDatabase* getInstance();

	bool init(const std::string& dbPath);
	void deinit();
	bool isInitialized() const { return mDb != nullptr; }

	// Core operations
	bool hasSystemCache(const std::string& systemName);
	bool loadSystem(SystemData* system, std::unordered_map<std::string, FileData*>& fileMap);
	bool syncSystem(SystemData* system);

	// Single game operations (for metadata edits, scraper updates)
	bool upsertGame(const std::string& systemName, FileData* game);
	bool removeGame(const std::string& systemName, const std::string& path);

	// Maintenance
	int removeStaleGames(const std::string& systemName, const std::vector<std::string>& validPaths);
	void vacuum();

private:
	GameDatabase();
	~GameDatabase();

	bool createTables();
	bool exec(const std::string& sql);
	sqlite3_stmt* prepare(const std::string& sql);

	FileData* loadGameFromRow(int64_t gameId, const std::string& path, int type,
		const std::unordered_map<int64_t, std::vector<std::pair<std::string, std::string>>>& metadataMap,
		SystemData* system, FolderData* root,
		std::unordered_map<std::string, FileData*>& fileMap);

	sqlite3* mDb;
	std::string mDbPath;
	std::mutex mMutex;

	static GameDatabase* sInstance;
};

#endif // ES_APP_GAME_DATABASE_H
