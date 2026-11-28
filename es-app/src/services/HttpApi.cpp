#include "HttpApi.h"

#ifdef WIN32
#include <Windows.h>
#endif

#include "utils/Platform.h"
#include "ApiSystem.h"
#include "Gamelist.h"
#include "SystemData.h"
#include "FileData.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "utils/md5.h"
#include "scrapers/Scraper.h"
#include "SaveStateRepository.h"
#include "LocaleES.h"
#include "RetroAchievements.h"
#include "SystemConf.h"
#include <algorithm>
#include <unordered_map>
#include <mutex>
#include <chrono>

void HttpApi::getSystemDataJson(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, SystemData* sys, bool localpaths)
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
		if (elem && elem->has("path")) {
		  if(localpaths) {
		    writer.Key("logo"); writer.String(elem->get<std::string>("path").c_str());
		  } else {
		    writer.Key("logo"); writer.String(("/systems/" + sys->getName() + "/logo").c_str());
		  }
		}
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

void HttpApi::getFileDataJson(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, FileData* game, bool localpaths)
{
	if (game->getType() != GAME)
		return;

	std::string id = getFileDataId(game);

	writer.StartObject();
	writer.Key("id"); writer.String(id.c_str());
	writer.Key("path"); writer.String(game->getPath().c_str());
	writer.Key("name"); writer.String(game->getName().c_str());

	writer.Key("systemName"); writer.String(game->getSystemName().c_str());

	auto meta = game->getMetadata();
	for (auto mdd : MetaDataList::getMDD())
	{
		if (mdd.id == MetaDataId::Name)
			continue;

		std::string value = game->getMetadata(mdd.id);
		if (!value.empty())
		{
			if (meta.getType(mdd.id) == MD_PATH && localpaths == false)
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
		if (mdd.key != mediaType || mdd.type != MD_PATH)
			continue;

		if (mdd.id == MetaDataId::Video)
		{
			if (extension != ".mp4" && extension != ".avi" && extension != ".mkv" && extension != ".webm")
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

std::string HttpApi::ToJson(FileData* file, bool localpaths)
{
	rapidjson::StringBuffer s;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(s);
	getFileDataJson(writer, file, localpaths);
	return s.GetString();
}

std::string HttpApi::ToJson(SystemData* system, bool localpaths)
{
	rapidjson::StringBuffer s;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(s);
	getSystemDataJson(writer, system, localpaths);
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

std::string HttpApi::getRunnningGameInfo()
{
	auto file = FileData::GetRunningGame();
	if (file == nullptr)
		return "";

	return ToJson(file);
}

#ifdef BATOCERA
static bool isCheevosActive(FileData* game)
{
	if (SystemConf::getInstance()->get("global.retroachievements.username").empty() || game->getCurrentGameSetting("retroachievements") != "1")
		return false;

	if (Utils::String::toInteger(game->getMetadata(MetaDataId::CheevosId)) <= 0)
		return false;

	auto system = game->getSourceFileData()->getSystem();
	return system->isFeatureSupported(game->getEmulator(), game->getCore(), EmulatorFeatures::cheevos);
}

// hardcore mode refuses state loading
static bool isHardcoreActive(FileData* game)
{
	return isCheevosActive(game) && game->getCurrentGameSetting("retroachievements.hardcore") == "1";
}

// the control center polls, so keep the RA web API to one request a minute per game
static bool getRunningGameProgress(FileData*& game, GameInfoAndUserProgress& progress)
{
	static std::mutex lock;
	static int cachedId = 0;
	static GameInfoAndUserProgress cached;
	static std::chrono::steady_clock::time_point cachedAt;

	game = FileData::GetRunningGame();
	if (game == nullptr || !isCheevosActive(game))
		return false;

	int id = Utils::String::toInteger(game->getMetadata(MetaDataId::CheevosId));

	std::unique_lock<std::mutex> guard(lock);
	auto now = std::chrono::steady_clock::now();
	// offline, or a failed refresh, keeps the last list
	if ((id != cachedId || now - cachedAt > std::chrono::seconds(60)) && ApiSystem::getInstance()->getIpAddress() != "NOT CONNECTED")
	{
		auto info = RetroAchievements::getGameInfoAndUserProgress(id);
		if (info.ID == id || id != cachedId)
			cached = info;

		cachedId = id;
		cachedAt = now;
	}

	if (cached.ID != id || cached.Achievements.empty())
		return false;

	progress = cached;
	return true;
}

// for the control center: id, title, subtitle and image of each state, newest first
std::string HttpApi::getRunningGameSaveStates()
{
	rapidjson::StringBuffer s;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(s);
	writer.StartArray();

	auto game = FileData::GetRunningGame();
	if (game != nullptr && SaveStateRepository::isEnabled(game) && !isHardcoreActive(game))
	{
		// read now, the system's own list is only refreshed when a game ends
		SaveStateRepository repository(game->getSourceFileData()->getSystem());
		auto states = repository.getSaveStates(game);
		std::sort(states.begin(), states.end(), [](SaveState* a, SaveState* b) { return a->creationDate > b->creationDate; });

		for (auto state : states)
		{
			if (!state->isSlotValid() || state->slot < -1)
				continue;

			// another emulator's states can't be loaded into the running one
			if (state->config != nullptr && !state->config->emulator.empty() && !state->config->isActiveConfig(game))
				continue;

			std::string title = state->slot == -1 ? _("AUTO SAVE") : _("SLOT") + std::string(" ") + std::to_string(state->slot);

			writer.StartObject();
			writer.Key("id"); writer.Int(state->slot);
			writer.Key("title"); writer.String(title.c_str());
			writer.Key("subtitle"); writer.String(state->creationDate.toLocalTimeString().c_str());
			writer.Key("image"); writer.String(state->getScreenShot().c_str());
			writer.EndObject();
		}
	}

	writer.EndArray();
	return s.GetString();
}

// for the control center: the running game's achievements, unlocked first
std::string HttpApi::getRunningGameAchievements()
{
	rapidjson::StringBuffer s;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(s);
	writer.StartArray();

	FileData* game = nullptr;
	GameInfoAndUserProgress progress;
	if (getRunningGameProgress(game, progress))
	{
		// softcore unlocks don't count in hardcore mode
		bool hardcore = isHardcoreActive(game);
		auto earned = [hardcore](const Achievement& a) { return !a.DateEarnedHardcore.empty() || (!hardcore && !a.DateEarned.empty()); };
		std::stable_partition(progress.Achievements.begin(), progress.Achievements.end(), earned);

		for (auto& achievement : progress.Achievements)
		{
			std::string title = Utils::String::trim(achievement.Title);
			std::string subtitle = achievement.Points + " " + _("Points") + _U(" - ") + Utils::String::trim(achievement.Description);
			std::string image = "http://i.retroachievements.org/Badge/" + achievement.BadgeName + (earned(achievement) ? ".png" : "_lock.png");

			writer.StartObject();
			writer.Key("id"); writer.String(achievement.ID.c_str());
			writer.Key("title"); writer.String(title.c_str());
			writer.Key("subtitle"); writer.String(subtitle.c_str());
			writer.Key("image"); writer.String(image.c_str());
			writer.EndObject();
		}
	}

	writer.EndArray();
	return s.GetString();
}

std::string HttpApi::getRunningGameAchievementsSummary()
{
	FileData* game = nullptr;
	GameInfoAndUserProgress progress;
	if (!getRunningGameProgress(game, progress) || progress.NumAchievements <= 0)
		return "";

	bool hardcore = isHardcoreActive(game);
	int won = hardcore ? progress.NumAwardedToUserHardcore : progress.NumAwardedToUser;
	int percent = won * 100 / progress.NumAchievements;

	std::string ret = Utils::String::format(_("%d%% (%d of %d)").c_str(), percent, won, progress.NumAchievements);
	if (hardcore)
		ret += _U(" - ") + _("HARDCORE MODE");

	return ret;
}
#endif

std::string HttpApi::getCaps()
{
	rapidjson::StringBuffer s;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(s);

	writer.StartObject();

	writer.Key("Version"); writer.String(ApiSystem::getInstance()->getVersion().c_str());

	// MetaDataId::SortName is disabled in this EmulationStation 
	writer.Key("SortName"); writer.Bool(false);

	//	:
	// add capability info for web client 
	//	:

	writer.EndObject();

	return s.GetString();
}
