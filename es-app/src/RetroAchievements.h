#pragma once


#include <string>
#include <vector>
#include <map>

#include "ApiSystem.h"

class SystemData;

// API_GetGameInfoAndUserProgress

struct Achievement
{
	std::string ID;
	std::string NumAwarded;
	std::string NumAwardedHardcore;
	std::string Title;
	std::string Description;
	std::string Points;
	std::string TrueRatio;
	std::string Author;
	std::string DateModified;
	std::string DateCreated;
	std::string BadgeName;
	int DisplayOrder;
	std::string MemAddr;
	std::string DateEarned;

	std::string getBadgeUrl();
};

struct GameInfoAndUserProgress
{
	int ID;
	std::string Title;
	int ConsoleID;
	int ForumTopicID;
	int Flags;
	std::string ImageIcon;
	std::string ImageTitle;
	std::string ImageIngame;
	std::string ImageBoxArt;
	std::string Publisher;
	std::string Developer;
	std::string Genre;
	std::string Released;
	bool IsFinal;
	std::string ConsoleName;
	std::string RichPresencePatch;
	int NumAchievements;
	std::string NumDistinctPlayersCasual;
	std::string NumDistinctPlayersHardcore;
	std::vector<Achievement> Achievements;
	int NumAwardedToUser;
	int NumAwardedToUserHardcore;
	std::string UserCompletion;
	std::string UserCompletionHardcore;

	std::string getImageUrl(const std::string image = "");
};


// API_GetUserSummary
/*

struct LastActivity
{
	std::string ID;
	std::string timestamp;
	std::string lastupdate;
	std::string activitytype;
	std::string User;
	std::string data;
	std::string data2;
};

struct LastGame
{
	int ID;
	std::string Title;
	int ConsoleID;
	int ForumTopicID;
	int Flags;
	std::string ImageIcon;
	std::string ImageTitle;
	std::string ImageIngame;
	std::string ImageBoxArt;
	std::string Publisher;
	std::string Developer;
	std::string Genre;
	std::string Released;
	bool IsFinal;
	std::string ConsoleName;
	std::string RichPresencePatch;
};
*/
struct Award
{
	int NumPossibleAchievements;
	int PossibleScore;
	int NumAchieved;
	int ScoreAchieved;
	int NumAchievedHardcore;
	int ScoreAchievedHardcore;
};

struct RecentGame
{
	std::string GameID;
	std::string ConsoleID;
	std::string ConsoleName;
	std::string Title;
	std::string ImageIcon;
	std::string LastPlayed;
	std::string MyVote;
};

struct RecentAchievement
{
	std::string ID;
	std::string GameID;
	std::string GameTitle;
	std::string Title;
	std::string Description;
	std::string Points;
	std::string BadgeName;
	std::string IsAwarded;
	std::string DateAwarded;
	std::string HardcoreAchieved;
};

struct UserSummary
{
	std::string Username;

	int RecentlyPlayedCount;
	std::vector<RecentGame> RecentlyPlayed;
	std::string MemberSince;
//	LastActivity LastActivity;
	std::string RichPresenceMsg;
	std::string LastGameID;
//	LastGame LastGame;
	std::string ContribCount;
	std::string ContribYield;
	std::string TotalPoints;
	std::string TotalTruePoints;
	std::string Permissions;
	std::string Untracked;
	std::string ID;
	std::string UserWallActive;
	std::string Motto;
	std::string Rank;
	std::string TotalRanked;	
	std::map<std::string, Award> Awarded;
	std::map<std::string, std::vector<RecentAchievement>> RecentAchievements;
	std::string Points;
	std::string UserPic;
	std::string Status;
};

// toRetroAchivementInfo
struct RetroAchievementGame
{
	std::string id;
	std::string name;
	std::string achievements;
	std::string points;
	std::string totalTruePoints;
	std::string lastplayed;
	std::string badge;

	int wonAchievements;
	int totalAchievements;
};

struct RetroAchievementInfo
{
	std::string username;
	std::string points;
	std::string totalpoints;
	std::string rank;
	std::string userpic;
	std::string registered;
	std::string error;
	std::vector<RetroAchievementGame> games;
};

class RetroAchievements
{
public:
	static std::string				getApiUrl(const std::string method, const std::string parameters);
	static UserSummary				getUserSummary(const std::string userName = "", int gameCount = 500);
	static GameInfoAndUserProgress	getGameInfoAndUserProgress(int gameId, const std::string userName = "");
	static RetroAchievementInfo		toRetroAchivementInfo(UserSummary& ret);

	static std::map<std::string, std::string>	getCheevosHashes();

	static std::string				getCheevosHash(SystemData* pSystem, const std::string fileName);
	static bool						testAccount(const std::string& username, const std::string& password, std::string& error);

private:
	static std::string				getCheevosHashFromFile(int consoleId, const std::string fileName);
};