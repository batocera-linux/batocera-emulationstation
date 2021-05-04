#include "HttpApi.h"

#ifdef WIN32
#include <Windows.h>
#endif

#include "platform.h"
#include "Gamelist.h"
#include "SystemData.h"
#include "FileData.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "utils/md5.h"
#include "scrapers/Scraper.h"
#include <unordered_map>

void HttpApi::getSystemDataJson(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, SystemData* sys)
{
	writer.StartObject();
	writer.Key("name"); writer.String(sys->getName().c_str());
	writer.Key("fullname"); writer.String(sys->getFullName().c_str());

	writer.Key("hardwareType"); writer.String(sys->getSystemMetadata().hardwareType.c_str());
	writer.Key("manufacturer"); writer.String(sys->getSystemMetadata().manufacturer.c_str());

	if (sys->getSystemMetadata().releaseYear != 0)
	{
		writer.Key("releaseYear"); writer.Int(sys->getSystemMetadata().releaseYear);
	}

	writer.Key("theme"); writer.String(sys->getSystemMetadata().themeFolder.c_str());

	// writer.Key("startpath"); writer.String(sys->getStartPath().c_str());
	writer.Key("theme"); writer.String(sys->getThemeFolder().c_str());

	auto exts = sys->getExtensions();
	if (exts.size() > 0)
	{
		writer.Key("extensions");
		writer.StartArray();

		for (auto ext : exts)
			writer.String(ext.c_str());

		writer.EndArray();
	}

	writer.Key("visible"); writer.String(sys->isVisible() ? "true" : "false");

	if (!sys->getSystemEnvData()->mGroup.empty())
	{
		writer.Key("group"); writer.String(sys->getSystemEnvData()->mGroup.c_str());
	}

	writer.Key("collection"); writer.String(sys->isCollection() ? "true" : "false");
	writer.Key("gamesystem"); writer.String(sys->isGameSystem() ? "true" : "false");
	writer.Key("groupsystem"); writer.String(sys->isGroupSystem() ? "true" : "false");

	GameCountInfo* info = sys->getGameCountInfo();

	writer.Key("totalGames"); writer.Int(info->totalGames);
	writer.Key("visibleGames"); writer.Int(info->visibleGames);
	writer.Key("favoriteGames"); writer.Int(info->favoriteCount);
	writer.Key("playedGames"); writer.Int(info->gamesPlayed);
	writer.Key("hiddenGames"); writer.Int(info->hiddenCount);
	writer.Key("mostPlayedGame"); writer.String(info->mostPlayed.c_str());

	auto theme = sys->getTheme();
	if (theme != nullptr)
	{
		const ThemeData::ThemeElement* elem = theme->getElement("system", "logo", "image");
		if (elem && elem->has("path"))
			writer.Key("logo"); writer.String(("/systems/" + sys->getName() + "/logo").c_str());
	}

	writer.EndObject();
}

std::string HttpApi::getSystemList()
{
	rapidjson::StringBuffer s;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(s);

	writer.StartArray();

	for (auto sys : SystemData::sSystemVector)
		getSystemDataJson(writer, sys);

	writer.EndArray();

	return s.GetString();
}

std::string HttpApi::getFileDataId(FileData* game)
{
	MD5 md5;
	md5.update(game->getPath().c_str(), game->getPath().size());
	md5.finalize();
	return md5.hexdigest();
}

FileData* HttpApi::findFileData(SystemData* system, const std::string& id)
{
	std::stack<FolderData*> stack;
	stack.push(system->getRootFolder());

	while (stack.size())
	{
		FolderData* current = stack.top();
		stack.pop();

		for (auto it : current->getChildren())
		{			
			if (it->getType() == FOLDER)
				stack.push((FolderData*)it);
			else if (getFileDataId(it) == id)
				return it;
		}
	}

	return nullptr;
}

void HttpApi::getFileDataJson(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, FileData* game)
{
	if (game->getType() != GAME)
		return;

	std::string id = getFileDataId(game);

	writer.StartObject();
	writer.Key("id"); writer.String(id.c_str());
	writer.Key("path"); writer.String(game->getPath().c_str());
	writer.Key("name"); writer.String(game->getName().c_str());

	auto meta = game->getMetadata();
	for (auto mdd : MetaDataList::getMDD())
	{
		if (mdd.id == MetaDataId::Name)
			continue;

		std::string value = game->getMetadata(mdd.id);
		if (!value.empty())
		{
			if (meta.getType(mdd.id) == MD_PATH)
				value = "/systems/" + game->getSourceFileData()->getSystemName() + "/games/" + id + "/media/" + mdd.key;

			if (mdd.id == MetaDataId::ScraperId)
				writer.Key("scraperId");
			else 
				writer.Key(mdd.key.c_str());

			writer.String(value.c_str());
		}
	}

	writer.EndObject();
}

bool HttpApi::ImportFromJson(FileData* file, const std::string& json)
{
	rapidjson::Document doc;
	doc.Parse(json.c_str());
	if (doc.HasParseError())
		return false;

	bool changed = false;

	MetaDataList& meta = file->getMetadata();

	for (auto mdd : MetaDataList::getMDD())
	{
		if (mdd.type == MD_PATH)
			continue;

		std::string key = mdd.key == "id" ? "scraperId" : mdd.key;

		if (!doc.HasMember(key.c_str()))
			continue;

		const rapidjson::Value& value = doc[key.c_str()];
		if (!value.IsString())
			continue;

		std::string newValue = value.GetString();
		std::string currentValue = meta.get(mdd.id);
		if (newValue != currentValue)
		{
			meta.set(mdd.id, newValue);
			changed = true;
		}
	}

	if (changed)
		saveToGamelistRecovery(file);

	return changed;
}

bool HttpApi::ImportMedia(FileData* file, const std::string& mediaType, const std::string& contentType, const std::string& mediaBytes)
{
	std::string extension;
	if (Utils::String::startsWith(contentType, "image/"))
	{
		extension = "." + contentType.substr(6);
		if (extension == ".jpeg")
			extension = ".jpg";
		else if (extension == ".svg+xml")
			extension = ".svg";

	}
	else if (Utils::String::startsWith(contentType, "video/"))
	{
		extension = "." + contentType.substr(6);
		if (extension == ".quicktime")
			extension = ".mov";
	}
	else if (Utils::String::startsWith(contentType, "application/"))
		extension = "." + contentType.substr(12);
	else
		return false;

	for (auto mdd : MetaDataList::getMDD())
	{
		if (mdd.key != mediaType && mdd.type != MD_PATH)
			continue;

		if (mdd.id == MetaDataId::Video)
		{
			if (extension != ".mp4" && extension != ".avi" && extension != ".mkv")
				return false;
		}
		else if (mdd.id == MetaDataId::Manual || mdd.id == MetaDataId::Magazine)
		{
			if (extension != ".pdf" && extension != ".cbz")
				return false;
		}
		else if (mdd.id == MetaDataId::Map)
		{
			if (extension != ".jpg" && extension != ".png" && extension != ".gif" && extension != ".pdf" && extension != ".cbz")
				return false;
		}
		else if (extension != ".jpg" && extension != ".png" && extension != ".gif")
			return false;

		std::string path = Scraper::getSaveAsPath(file, mdd.id, extension);

		Utils::FileSystem::writeAllText(path, mediaBytes);
		file->setMetadata(mdd.id, path);
		saveToGamelistRecovery(file);
		return true;
	}

	return false;
}

std::string HttpApi::ToJson(FileData* file)
{
	rapidjson::StringBuffer s;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(s);
	getFileDataJson(writer, file);
	return s.GetString();
}

std::string HttpApi::ToJson(SystemData* system)
{
	rapidjson::StringBuffer s;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(s);
	getSystemDataJson(writer, system);
	return s.GetString();
}

std::string HttpApi::getSystemGames(SystemData* system)
{
	rapidjson::StringBuffer s;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(s);

	writer.StartArray();

	std::vector<FileData*> files;

	std::stack<FolderData*> stack;
	stack.push(system->getRootFolder());

	while (stack.size())
	{
		FolderData* current = stack.top();
		stack.pop();

		for (auto it : current->getChildren())
		{
			files.push_back(it);
			if (it->getType() == FOLDER)
				stack.push((FolderData*)it);
		}
	}

	for (auto game : files)
		getFileDataJson(writer, game);

	writer.EndArray();

	return s.GetString();
}



