#pragma once
#ifndef ES_APP_GAME_LIST_H
#define ES_APP_GAME_LIST_H

#include <unordered_map>
#include <vector>
#include <string>

class SystemData;
class FileData;

// Loads gamelist.xml data into a SystemData.
void parseGamelist(SystemData* system, std::unordered_map<std::string, FileData*>& fileMap);

// Writes currently loaded metadata for a SystemData to gamelist.xml.
void updateGamelist(SystemData* system);
void cleanupGamelist(SystemData* system);

bool saveToGamelistRecovery(FileData* file);
bool removeFromGamelistRecovery(FileData* file);

bool saveToXml(FileData* file, const std::string& fileName, bool fullPaths = false);

bool hasDirtyFile(SystemData* system);

std::vector<FileData*> loadGamelistFile(const std::string xmlpath, SystemData* system, std::unordered_map<std::string, FileData*>& fileMap, size_t checkSize = SIZE_MAX, bool fromFile = true);

#endif // ES_APP_GAME_LIST_H
