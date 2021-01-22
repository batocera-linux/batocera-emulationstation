#include <chrono>
#include <fstream>
#include <memory>
#include <thread>

#include "Log.h"

#include "scrapers/ArcadeDBJSONScraperResources.h"
#include "utils/FileSystemUtil.h"


#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

using namespace rapidjson;


namespace
{
constexpr char ArcadeDBAPIKey[] = "TODO"; // batocera


constexpr int MAX_WAIT_MS = 90000;
constexpr int POLL_TIME_MS = 500;
constexpr int MAX_WAIT_ITER = MAX_WAIT_MS / POLL_TIME_MS;

constexpr char SCRAPER_RESOURCES_DIR[] = "scrapers";
constexpr char DEVELOPERS_JSON_FILE[] = "gamesdb_developers.json";
constexpr char PUBLISHERS_JSON_FILE[] = "gamesdb_publishers.json";
constexpr char GENRES_JSON_FILE[] = "gamesdb_genres.json";
constexpr char DEVELOPERS_ENDPOINT[] = "/Developers";
constexpr char PUBLISHERS_ENDPOINT[] = "/Publishers";
constexpr char GENRES_ENDPOINT[] = "/Genres";

std::string genFilePath(const std::string& file_name)
{
	return Utils::FileSystem::getGenericPath(getScrapersResourceDir() + "/" + file_name);
}

void ensureScrapersResourcesDir()
{
	std::string path = getScrapersResourceDir();
	if (!Utils::FileSystem::exists(path))
		Utils::FileSystem::createDirectory(path);
}

} // namespace


std::string getScrapersResourceDir()
{
	return Utils::FileSystem::getGenericPath(Utils::FileSystem::getEsConfigPath() + std::string("/") + SCRAPER_RESOURCES_DIR); // batocera
}

std::string ArcadeDBJSONRequestResources::getApiKey() const { return ArcadeDBAPIKey; }


void ArcadeDBJSONRequestResources::prepare()
{
	if (checkLoaded())
	{
		return;
	}
}

void ArcadeDBJSONRequestResources::ensureResources()
{

	if (checkLoaded())
	{
		return;
	}


	/*for (int i = 0; i < MAX_WAIT_ITER; ++i)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(POLL_TIME_MS));
	}
	LOG(LogError) << "Timed out while waiting for resources\n";*/
}

bool ArcadeDBJSONRequestResources::checkLoaded()
{
	return true;
}

bool ArcadeDBJSONRequestResources::saveResource(HttpReq* req, std::unordered_map<int, std::string>& resource,
	const std::string& resource_name, const std::string& file_name)
{

	if (req == nullptr)
	{
		LOG(LogError) << "Http request pointer was null\n";
		return true;
	}
	if (req->status() == HttpReq::REQ_IN_PROGRESS)
	{
		return false; // Not ready: wait some more
	}
	if (req->status() != HttpReq::REQ_SUCCESS)
	{
		LOG(LogError) << "Resource request for " << file_name << " failed:\n\t" << req->getErrorMsg();
		return true; // Request failed, resetting request.
	}

	ensureScrapersResourcesDir();

	std::ofstream fout(file_name);
	fout << req->getContent();
	fout.close();
	loadResource(resource, resource_name, file_name);
	return true;
}

std::unique_ptr<HttpReq> ArcadeDBJSONRequestResources::fetchResource(const std::string& endpoint)
{
	std::string path = "https://api.ArcadeDB.net/v1";
	path += endpoint;
	path += "?apikey=" + getApiKey();

	return std::unique_ptr<HttpReq>(new HttpReq(path));
}


int ArcadeDBJSONRequestResources::loadResource(
	std::unordered_map<int, std::string>& resource, const std::string& resource_name, const std::string& file_name)
{


	std::ifstream fin(file_name);
	if (!fin.good())
	{
		return 1;
	}
	std::stringstream buffer;
	buffer << fin.rdbuf();
	Document doc;
	doc.Parse(buffer.str().c_str());

	if (doc.HasParseError())
	{
		std::string err = std::string("ArcadeDBJSONRequest - Error parsing JSON for resource file ") + file_name +
						  ":\n\t" + GetParseError_En(doc.GetParseError());
		LOG(LogError) << err;
		return 1;
	}

	if (!doc.HasMember("data") || !doc["data"].HasMember(resource_name.c_str()) ||
		!doc["data"][resource_name.c_str()].IsObject())
	{
		std::string err = "ArcadeDBJSONRequest - Response had no resource data.\n";
		LOG(LogError) << err;
		return 1;
	}
	auto& data = doc["data"][resource_name.c_str()];

	for (Value::ConstMemberIterator itr = data.MemberBegin(); itr != data.MemberEnd(); ++itr)
	{
		auto& entry = itr->value;
		if (!entry.IsObject() || !entry.HasMember("id") || !entry["id"].IsInt() || !entry.HasMember("name") ||
			!entry["name"].IsString())
		{
			continue;
		}
		resource[entry["id"].GetInt()] = entry["name"].GetString();
	}
	return resource.empty();
}
