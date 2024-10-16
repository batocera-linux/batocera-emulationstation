#pragma once
#ifndef ES_CORE_MAMENAMES_H
#define ES_CORE_MAMENAMES_H

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <map>

class SystemData;

enum class ArcadeRomType
{
	NONE = 0,
	VERTICAL = 1,
	LIGHTGUN = 2,
	WHEEL = 4,
	BIOS = 8,
	DEVICE = 16,
	TRACKBALL = 32,
	SPINNER = 64,
};

struct ArcadeRom
{	
	ArcadeRom() { type = ArcadeRomType::NONE; }

	std::string displayName;
	ArcadeRomType type;
};

class MameNames
{
public:
	static void       init       ();
	static void       deinit     ();
	static MameNames* getInstance();
	std::string       getRealName(const std::string& _mameName);
	const bool        isBiosOrDevice(const std::string& _biosName);	
	const bool        isVertical(const std::string& _nameName);
	const bool		  isLightgun(const std::string& _nameName, const std::string& systemName, bool isArcade);
  	const bool		  isWheel(const std::string& _nameName, const std::string& systemName, bool isArcade);
    	const bool		  isTrackball(const std::string& _nameName, const std::string& systemName, bool isArcade);
      	const bool		  isSpinner(const std::string& _nameName, const std::string& systemName, bool isArcade);

private:
	 MameNames();
	~MameNames();

	static MameNames* sInstance;

	std::unordered_map<std::string, ArcadeRom> mArcadeRoms;

	std::unordered_map<std::string, std::unordered_set<std::string>> mNonArcadeGunGames;
  	std::unordered_map<std::string, std::unordered_set<std::string>> mNonArcadeWheelGames;
    	std::unordered_map<std::string, std::unordered_set<std::string>> mNonArcadeTrackballGames;
      	std::unordered_map<std::string, std::unordered_set<std::string>> mNonArcadeSpinnerGames;

}; // MameNames

#endif // ES_CORE_MAMENAMES_H
