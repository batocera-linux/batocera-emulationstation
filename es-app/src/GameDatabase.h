#pragma once
#ifndef ES_APP_GAME_DATABASE_H
#define ES_APP_GAME_DATABASE_H

#include <string>
#include <vector>
#include <unordered_map>

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

	void bindGameData(sqlite3_stmt* stmt, const std::string& systemName, FileData* game);
	FileData* loadGameFromRow(sqlite3_stmt* stmt, SystemData* system, FolderData* root,
							  std::unordered_map<std::string, FileData*>& fileMap);

	sqlite3* mDb;
	std::string mDbPath;

	static GameDatabase* sInstance;
};

#endif // ES_APP_GAME_DATABASE_H
