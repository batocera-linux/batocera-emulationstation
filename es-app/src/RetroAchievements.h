#pragma once


#include <string>
#include <vector>
#include <map>

#include "ApiSystem.h"

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
	std::string DisplayOrder;
	std::string MemAddr;
	std::string DateEarned;
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

struct RecentlyPlayed
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
	std::vector<RecentlyPlayed> RecentlyPlayed;
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

class RetroAchievements
{
public:
	static std::string				getApiUrl(const std::string method, const std::string parameters);
	static UserSummary				getUserSummary(const std::string userName, int gameCount = 50);
	static GameInfoAndUserProgress	getGameInfoAndUserProgress(const std::string userName, int gameId);
	static RetroAchievementInfo		toRetroAchivementInfo(UserSummary& ret);
};