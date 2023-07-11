#pragma once

#include <string>
#include <rapidjson/rapidjson.h>
#include <rapidjson/pointer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

class SystemData;
class FileData;

class HttpApi
{
public:
	static std::string getCaps();
	static std::string getSystemList();
	static std::string getSystemGames(SystemData* system);

	static std::string getRunnningGameInfo();

	static std::string ToJson(SystemData* system, bool localpaths = false);
	static std::string ToJson(FileData* file, bool localpaths = false);

	static FileData*   findFileData(SystemData* system, const std::string& id);

	static bool ImportFromJson(FileData* file, const std::string& json);

	static bool ImportMedia(FileData* file, const std::string& mediaType, const std::string& contentType, const std::string& mediaBytes);
	

private:
	static std::string getFileDataId(FileData* game);
	static void getFileDataJson(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, FileData* game, bool localpaths = false);
	static void getSystemDataJson(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, SystemData* sys, bool localpaths = false);
};
