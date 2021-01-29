#pragma once
#ifndef ES_CORE_MAMENAMES_H
#define ES_CORE_MAMENAMES_H

#include <string>
#include <vector>
#include <unordered_set>

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

}; // MameNames

#endif // ES_CORE_MAMENAMES_H
