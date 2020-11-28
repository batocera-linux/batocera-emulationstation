#include "RetroAchievements.h"
#include "HttpReq.h"
#include "utils/StringUtil.h"
#include "ApiSystem.h"

#include <rapidjson/rapidjson.h>
#include <rapidjson/pointer.h>

const std::string API_DEV_L = { 42, 88, 35, 2, 36, 10, 2, 6, 23, 65, 45, 7, 10, 85, 26, 67, 89, 74, 28, 90, 41, 113, 41, 47, 16, 76, 82, 86, 22, 71, 12, 22, 54, 61, 45, 51, 16, 99, 3, 55, 54, 122, 4, 46, 69, 33, 2, 59, 5, 115 };
const std::string API_DEV_KEY = { 80, 101, 97, 99, 80, 101, 97, 99, 101, 32, 97, 110, 100, 32, 98, 101, 32, 119, 105, 108, 101, 32, 97, 110, 100, 32, 98, 101, 32, 119, 105, 108, 80, 101, 97, 99, 101, 32, 97, 110, 100, 32, 98, 101, 32, 119, 105, 108, 100 };

std::string RetroAchievements::getApiUrl(const std::string method, const std::string parameters)
{
	auto options = Utils::String::scramble(API_DEV_L, API_DEV_KEY);
	return "https://retroachievements.org/API/"+ method +".php?"+ options +"&" + parameters;
}

int jsonInt(const rapidjson::Value& val, const std::string name)
{
	if (!val.HasMember(name.c_str()))
		return 0;

	const rapidjson::Value& value = val[name.c_str()];
	
	if (value.IsInt())
		return value.GetInt();
	
	if (value.IsString())
		return Utils::String::toInteger(value.GetString());

	return 0;
}

std::string jsonString(const rapidjson::Value& val, const std::string name)
{
	if (!val.HasMember(name.c_str()))
		return "";

	const rapidjson::Value& value = val[name.c_str()];

	if (value.IsInt())
		return std::to_string(value.GetInt());

	if (value.IsString())
		return value.GetString();

	return "";
}


GameInfoAndUserProgress RetroAchievements::getGameInfoAndUserProgress(const std::string userName, int gameId)
{
	GameInfoAndUserProgress ret;
	ret.ID = 0;

	HttpReq httpreq(getApiUrl("API_GetGameInfoAndUserProgress", "u=" + userName + "&g=" + std::to_string(gameId)));
	httpreq.wait();

	rapidjson::Document doc;
	doc.Parse(httpreq.getContent().c_str());
	if (doc.HasParseError())
		return ret;

	ret.ID = jsonInt(doc, "ID");
	ret.Title = jsonString(doc, "Title");
	ret.ConsoleID = jsonInt(doc, "ConsoleID");
	ret.ForumTopicID = jsonInt(doc, "ForumTopicID");
	ret.Flags = jsonInt(doc, "Flags");
	ret.ImageIcon = jsonString(doc, "ImageIcon");
	ret.ImageTitle = jsonString(doc, "ImageTitle");
	ret.ImageIngame = jsonString(doc, "ImageIngame");
	ret.ImageBoxArt = jsonString(doc, "ImageBoxArt");
	ret.Publisher = jsonString(doc, "Publisher");
	ret.Developer = jsonString(doc, "Developer");
	ret.Genre = jsonString(doc, "Genre");
	ret.Released = jsonString(doc, "Released");
	ret.ConsoleName = jsonString(doc, "ConsoleName");
	ret.NumDistinctPlayersCasual = jsonString(doc, "NumDistinctPlayersCasual");
	ret.NumDistinctPlayersHardcore = jsonString(doc, "NumDistinctPlayersHardcore");
	ret.NumAchievements = jsonInt(doc, "NumAchievements");
	ret.NumAwardedToUser = jsonInt(doc, "NumAwardedToUser");
	ret.NumAwardedToUserHardcore = jsonInt(doc, "NumAwardedToUserHardcore");
	ret.UserCompletion = jsonString(doc, "UserCompletion");
	ret.UserCompletionHardcore = jsonString(doc, "UserCompletionHardcore");

	if (doc.HasMember("Achievements"))
	{
		const rapidjson::Value& ra = doc["Achievements"];
		for (auto achivId = ra.MemberBegin(); achivId != ra.MemberEnd(); ++achivId)
		{
			auto& recent = achivId->value;
						
			Achievement item;
			item.ID = jsonString(recent, "ID");
			item.NumAwarded = jsonString(recent, "NumAwarded");
			item.NumAwardedHardcore = jsonString(recent, "NumAwardedHardcore");
			item.Title = jsonString(recent, "Title");
			item.Description = jsonString(recent, "Description");
			item.Points = jsonString(recent, "Points");
			item.TrueRatio = jsonString(recent, "TrueRatio");
			item.Author = jsonString(recent, "Author");
			item.DateModified = jsonString(recent, "DateModified");
			item.DateCreated = jsonString(recent, "DateCreated");
			item.BadgeName = jsonString(recent, "BadgeName");
			item.DisplayOrder = jsonString(recent, "DisplayOrder");
			item.DateEarned = jsonString(recent, "DateEarned");
			ret.Achievements.push_back(item);
		}
	}

	return ret;
}

UserSummary RetroAchievements::getUserSummary(const std::string userName, int gameCount)
{
	UserSummary ret;

	std::string count = std::to_string(gameCount);

	HttpReq httpreq(getApiUrl("API_GetUserSummary", "u="+ userName +"&g="+ count +"&a="+ count));
	httpreq.wait();

	rapidjson::Document doc;
	doc.Parse(httpreq.getContent().c_str());
	if (doc.HasParseError())
		return ret;

	ret.Username = userName;
	ret.RecentlyPlayedCount = jsonInt(doc, "RecentlyPlayedCount");
	ret.MemberSince = jsonString(doc, "MemberSince");
	ret.RichPresenceMsg = jsonString(doc, "RichPresenceMsg");
	ret.LastGameID = jsonString(doc, "LastGameID");
	ret.ContribCount = jsonString(doc, "ContribCount");
	ret.ContribYield = jsonString(doc, "ContribYield");
	ret.TotalTruePoints = jsonString(doc, "TotalTruePoints");
	ret.TotalPoints = jsonString(doc, "TotalPoints");
	ret.Permissions = jsonString(doc, "Permissions");
	ret.Untracked = jsonString(doc, "Untracked");
	ret.ID = jsonString(doc, "ID");
	ret.UserWallActive = jsonString(doc, "UserWallActive");
	ret.Motto = jsonString(doc, "Motto");
	ret.Rank = jsonString(doc, "Rank");
	ret.TotalRanked = jsonString(doc, "TotalRanked");
	ret.Points = jsonString(doc, "Points");
	ret.UserPic = jsonString(doc, "UserPic");
	ret.Status = jsonString(doc, "Status");

	if (doc.HasMember("RecentlyPlayed"))
	{
		for (auto& recent : doc["RecentlyPlayed"].GetArray())
		{
			RecentlyPlayed item;
			item.GameID = jsonString(recent, "GameID");
			item.ConsoleID = jsonString(recent, "ConsoleID");
			item.ConsoleName = jsonString(recent, "ConsoleName");
			item.Title = jsonString(recent, "Title");
			item.ImageIcon = jsonString(recent, "ImageIcon");
			item.LastPlayed = jsonString(recent, "LastPlayed");
			item.MyVote = jsonString(recent, "MyVote");

			ret.RecentlyPlayed.push_back(item);
		}
	}

	if (doc.HasMember("Awarded"))
	{
		const rapidjson::Value& ra = doc["Awarded"];
		for (auto achivId = ra.MemberBegin(); achivId != ra.MemberEnd(); ++achivId)
		{
			std::string gameID = achivId->name.GetString();
			auto& recent = achivId->value;

			Award item;
			item.NumPossibleAchievements = jsonInt(recent, "NumPossibleAchievements");
			item.PossibleScore = jsonInt(recent, "PossibleScore");
			item.NumAchieved = jsonInt(recent, "NumAchieved");
			item.ScoreAchieved = jsonInt(recent, "ScoreAchieved");
			item.NumAchievedHardcore = jsonInt(recent, "NumAchievedHardcore");
			item.ScoreAchievedHardcore = jsonInt(recent, "ScoreAchievedHardcore");

			ret.Awarded[gameID] = item;
		}
	}

	if (doc.HasMember("RecentAchievements"))
	{
		const rapidjson::Value& ra = doc["RecentAchievements"];		
		for (auto achivId = ra.MemberBegin(); achivId != ra.MemberEnd(); ++achivId)
		{			
			std::string gameID = achivId->name.GetString();

			for (auto itrc = achivId->value.MemberBegin(); itrc != achivId->value.MemberEnd(); ++itrc)
			{
				auto& recent = itrc->value;
				RecentAchievement item;

				item.ID = jsonString(recent, "ID");
				item.GameID = jsonString(recent, "GameID");
				item.GameTitle = jsonString(recent, "GameTitle");
				item.Description = jsonString(recent, "Description");
				item.Points = jsonString(recent, "Points");
				item.BadgeName = jsonString(recent, "BadgeName");
				item.IsAwarded = jsonString(recent, "IsAwarded");
				item.DateAwarded = jsonString(recent, "DateAwarded");
				item.HardcoreAchieved = jsonString(recent, "HardcoreAchieved");
				ret.RecentAchievements[gameID].push_back(item);
			}			
		}
	}

	return ret;
}


RetroAchievementInfo RetroAchievements::toRetroAchivementInfo(UserSummary& ret)
{
	RetroAchievementInfo info;

	info.userpic = "https://retroachievements.org" + ret.UserPic;
	info.rank = ret.Rank;
	info.totalpoints = ret.TotalTruePoints;
	info.username = ret.Username;
	info.registered = ret.MemberSince;

	for (auto played : ret.RecentlyPlayed)
	{
		RetroAchievementGame rg;
		rg.id = played.GameID;		

		if (!played.ImageIcon.empty())
			rg.badge = "http://i.retroachievements.org" + played.ImageIcon;

		rg.name = played.Title + " [" + played.ConsoleName + "]";
		rg.lastplayed = played.LastPlayed;

		auto aw = ret.Awarded.find(played.GameID);
		if (aw != ret.Awarded.cend())
		{
			if (aw->second.NumAchieved == 0 && aw->second.ScoreAchieved == 0)
				continue;

			rg.achievements = std::to_string(aw->second.NumAchieved) + " of " + std::to_string(aw->second.NumPossibleAchievements);
			rg.points = std::to_string(aw->second.ScoreAchieved) + "/" + std::to_string(aw->second.PossibleScore);
		}

		info.games.push_back(rg);
	}

	return info;
}
