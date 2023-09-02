#pragma once
#ifndef ES_CORE_MAMENAMES_H
#define ES_CORE_MAMENAMES_H

#include <string>
#include <vector>
#include <unordered_set>
#include <map>

class SystemData;

class MameNames
{
public:

	static void       init       ();
	static void       deinit     ();
	static MameNames* getInstance();
	std::string       getRealName(const std::string& _mameName);
	const bool        isBios(const std::string& _biosName);
	const bool        isDevice(const std::string& _deviceName);
	const bool        isVertical(const std::string& _nameName);
	const bool		  isLightgun(const std::string& _nameName, const std::string& systemName, bool isArcade);
  	const bool		  isWheel(const std::string& _nameName, const std::string& systemName);

private:

	struct NamePair
	{
		std::string mameName;
		std::string realName;
	};

	typedef std::vector<NamePair> namePairVector;

	 MameNames();
	~MameNames();

	static MameNames* sInstance;

	namePairVector mNamePairs;

	std::unordered_set<std::string> mMameBioses;
	std::unordered_set<std::string> mMameDevices;
	std::unordered_set<std::string> mVerticalGames;
	std::unordered_set<std::string> mArcadeLightGunGames;

	std::map<std::string, std::unordered_set<std::string>> mNonArcadeGunGames;
  	std::map<std::string, std::unordered_set<std::string>> mWheelGames;

}; // MameNames

#endif // ES_CORE_MAMENAMES_H
