#include "MameNames.h"

#include "resources/ResourceManager.h"
#include "utils/FileSystemUtil.h"
#include "Log.h"
#include <pugixml/src/pugixml.hpp>
#include "utils/StringUtil.h"
#include <string.h>

MameNames* MameNames::sInstance = nullptr;

void MameNames::init()
{
	if(!sInstance)
		sInstance = new MameNames();

} // init

void MameNames::deinit()
{
	if(sInstance)
	{
		delete sInstance;
		sInstance = nullptr;
	}

} // deinit

MameNames* MameNames::getInstance()
{
	if(!sInstance)
		sInstance = new MameNames();

	return sInstance;

} // getInstance

static ArcadeRomType& operator |=(ArcadeRomType& a, ArcadeRomType b)
{
	unsigned ai = static_cast<unsigned>(a);
	unsigned bi = static_cast<unsigned>(b);
	ai |= bi;
	return a = static_cast<ArcadeRomType>(ai);
}

static bool hasFlag(ArcadeRomType& a, ArcadeRomType b)
{
	unsigned ai = static_cast<unsigned>(a);
	unsigned bi = static_cast<unsigned>(b);
	return (ai & bi) == bi;
}

MameNames::MameNames()
{
	std::string xmlpath;

	pugi::xml_document doc;
	pugi::xml_parse_result result;

	// Read mame games information
	xmlpath = ResourceManager::getInstance()->getResourcePath(":/arcaderoms.xml");
	if (Utils::FileSystem::exists(xmlpath))
	{		
		result = doc.load_file(WINSTRINGW(xmlpath).c_str());
		if (result)
		{
			LOG(LogInfo) << "Parsing XML file \"" << xmlpath << "\"...";

			pugi::xml_node games = doc.child("roms");
			if (games)
			{
				std::string sTrue = "true";
				for (pugi::xml_node gameNode = games.child("rom"); gameNode; gameNode = gameNode.next_sibling("rom"))
				{
					if (!gameNode.attribute("id"))
						continue;

					std::string name = gameNode.attribute("id").value();

					ArcadeRom rom;

					if (gameNode.attribute("device") && gameNode.attribute("device").value() == sTrue)
					{
						rom.type |= ArcadeRomType::DEVICE;
						mArcadeRoms[name] = rom;
						continue;
					}

					if (gameNode.attribute("bios") && gameNode.attribute("bios").value() == sTrue)
					{
						rom.type |= ArcadeRomType::BIOS;
						mArcadeRoms[name] = rom;
						continue;
					}

					if (!gameNode.attribute("name"))
						continue;

					rom.displayName = gameNode.attribute("name").value();

					if (gameNode.attribute("vert") && gameNode.attribute("vert").value() == sTrue)
						rom.type |= ArcadeRomType::VERTICAL;

					//if (gameNode.attribute("gun") && gameNode.attribute("gun").value() == sTrue)
					//	rom.type |= ArcadeRomType::LIGHTGUN;

					//if (gameNode.attribute("wheel") && gameNode.attribute("wheel").value() == sTrue)
					//	rom.type |= ArcadeRomType::WHEEL;

					//if (gameNode.attribute("trackball") && gameNode.attribute("trackball").value() == sTrue)
					//	rom.type |= ArcadeRomType::TRACKBALL;

					//if (gameNode.attribute("spinner") && gameNode.attribute("spinner").value() == sTrue)
					//	rom.type |= ArcadeRomType::SPINNER;

					mArcadeRoms[name] = rom;
				}
			}
			else
				LOG(LogError) << "Error parsing XML file \"" << xmlpath << "\" <roms> root is missing !";
		}
		else
			LOG(LogError) << "Error parsing XML file \"" << xmlpath << "\"!\n	" << result.description();
	}
	
	// Read gun games for non arcade systems
	xmlpath = ResourceManager::getInstance()->getResourcePath(":/gamesdb.xml");
	if (Utils::FileSystem::exists(xmlpath))
	{
		result = doc.load_file(WINSTRINGW(xmlpath).c_str());
		if (result)
		{
			pugi::xml_node systems = doc.child("systems");
			if (systems)
			{
				LOG(LogInfo) << "Parsing XML file \"" << xmlpath;

				for (pugi::xml_node systemNode = systems.child("system"); systemNode; systemNode = systemNode.next_sibling("system"))
				{
					if (!systemNode.attribute("name"))
						continue;

					std::string systemNames = systemNode.attribute("name").value();
					for (auto systemName : Utils::String::split(systemNames, ','))
					{
						std::unordered_set<std::string> gunGames;
						std::unordered_set<std::string> wheelGames;
						std::unordered_set<std::string> trackballGames;
						std::unordered_set<std::string> spinnerGames;

						for (pugi::xml_node gameNode = systemNode.child("game"); gameNode; gameNode = gameNode.next_sibling("game"))
						{
							if (!gameNode.attribute("name"))
								continue;

							std::string gameName = gameNode.attribute("name").value();
							if (gameName.empty() || gameName == "default")
								continue;
								
							if (gameNode.child("gun"))
								gunGames.insert(gameName);

							if (gameNode.child("wheel"))
								wheelGames.insert(gameName);

							if (gameNode.child("trackball"))
								trackballGames.insert(gameName);

							if (gameNode.child("spinner"))
								spinnerGames.insert(gameName);
						}

						if (gunGames.size())
						{
							if (systemNames == "arcade")
							{
								for (auto game : gunGames)
								{
									auto it = mArcadeRoms.find(game);
									if (it == mArcadeRoms.cend())
									{
										ArcadeRom rom;
										rom.type |= ArcadeRomType::LIGHTGUN;
										mArcadeRoms[game] = rom;
									}
									else 
										it->second.type |= ArcadeRomType::LIGHTGUN;
								}
							}
							else
								mNonArcadeGunGames[Utils::String::trim(systemName)] = gunGames;
						}

						if (wheelGames.size())
						{
							if (systemNames == "arcade")
							{
								for (auto game : wheelGames)
								{
									auto it = mArcadeRoms.find(game);
									if (it == mArcadeRoms.cend())
									{
										ArcadeRom rom;
										rom.type |= ArcadeRomType::WHEEL;
										mArcadeRoms[game] = rom;
									}
									else
										it->second.type |= ArcadeRomType::WHEEL;
								}
							}
							else
								mNonArcadeWheelGames[Utils::String::trim(systemName)] = wheelGames;
						}

						if (trackballGames.size())
						{
							if (systemNames == "arcade")
							{
								for (auto game : trackballGames)
								{
									auto it = mArcadeRoms.find(game);
									if (it == mArcadeRoms.cend())
									{
										ArcadeRom rom;
										rom.type |= ArcadeRomType::TRACKBALL;
										mArcadeRoms[game] = rom;
									}
									else
										it->second.type |= ArcadeRomType::TRACKBALL;
								}
							}
							else
								mNonArcadeTrackballGames[Utils::String::trim(systemName)] = trackballGames;
						}

						if (spinnerGames.size())
						{
							if (systemNames == "arcade")
							{
								for (auto game : spinnerGames)
								{
									auto it = mArcadeRoms.find(game);
									if (it == mArcadeRoms.cend())
									{
										ArcadeRom rom;
										rom.type |= ArcadeRomType::SPINNER;
										mArcadeRoms[game] = rom;
									}
									else
										it->second.type |= ArcadeRomType::SPINNER;
								}
							}
							else
								mNonArcadeSpinnerGames[Utils::String::trim(systemName)] = spinnerGames;
						}
					}	
				}
			}
			else 
				LOG(LogError) << "Error parsing XML file \"" << xmlpath << "\" <systems> root is missing !";
		}
		else
			LOG(LogError) << "Error parsing XML file \"" << xmlpath << "\"!\n	" << result.description();
	}
	
} // MameNames

MameNames::~MameNames()
{

} // ~MameNames

std::string MameNames::getRealName(const std::string& _mameName)
{
	auto it = mArcadeRoms.find(_mameName);
	if (it != mArcadeRoms.cend() && !it->second.displayName.empty())
		return it->second.displayName;

	return _mameName;

} // getRealName

const bool MameNames::isBiosOrDevice(const std::string& _biosName)
{
	auto it = mArcadeRoms.find(_biosName);
	if (it != mArcadeRoms.cend())
		return hasFlag(it->second.type, ArcadeRomType::BIOS) || hasFlag(it->second.type, ArcadeRomType::DEVICE);

	return false;	
}

const bool MameNames::isVertical(const std::string& _nameName)
{
	auto it = mArcadeRoms.find(_nameName);
	if (it != mArcadeRoms.cend())
		return hasFlag(it->second.type, ArcadeRomType::VERTICAL);

	return false;
}

static std::string getIndexedName(const std::string& name)
{
	std::string result;

	bool inpar = false;
	bool inblock = false;

	for (auto c : Utils::String::toLower(name))
	{
		if (!inpar && !inblock && (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
			result += c;
		else if (c == '(') inpar = true;
		else if (c == ')') inpar = false;
		else if (c == '[') inblock = true;
		else if (c == ']') inblock = false;
	}

	return result;
}

const bool MameNames::isLightgun(const std::string& _nameName, const std::string& systemName, bool isArcade)
{
	if (isArcade)
	{
		auto it = mArcadeRoms.find(_nameName);
		if (it != mArcadeRoms.cend())
			return hasFlag(it->second.type, ArcadeRomType::LIGHTGUN);

		return false;
	}

	auto it = mNonArcadeGunGames.find(systemName);
	if (it == mNonArcadeGunGames.cend())
		return false;

	std::string indexedName = getIndexedName(_nameName);

	// Exact match ?
	if (it->second.find(indexedName) != it->second.cend())
		return true;

	// name contains ?
	for (auto gameName : it->second)
		if (indexedName.find(gameName) != std::string::npos)
			return true;

	return false;
}

const bool MameNames::isWheel(const std::string& _nameName, const std::string& systemName, bool isArcade)
{
	if (isArcade)
	{
		auto it = mArcadeRoms.find(_nameName);
		if (it != mArcadeRoms.cend())
			return hasFlag(it->second.type, ArcadeRomType::WHEEL);

		return false;
	}

	auto it = mNonArcadeWheelGames.find(systemName);
	if (it == mNonArcadeWheelGames.cend())
		return false;

	std::string indexedName = getIndexedName(_nameName);

	// Exact match ?
	if (it->second.find(indexedName) != it->second.cend())
		return true;

	// name contains ?
	for (auto gameName : it->second)
		if (indexedName.find(gameName) != std::string::npos)
			return true;

	return false;
}

const bool MameNames::isTrackball(const std::string& _nameName, const std::string& systemName, bool isArcade)
{
	if (isArcade)
	{
		auto it = mArcadeRoms.find(_nameName);
		if (it != mArcadeRoms.cend())
			return hasFlag(it->second.type, ArcadeRomType::TRACKBALL);

		return false;
	}

	auto it = mNonArcadeTrackballGames.find(systemName);
	if (it == mNonArcadeTrackballGames.cend())
		return false;

	std::string indexedName = getIndexedName(_nameName);

	// Exact match ?
	if (it->second.find(indexedName) != it->second.cend())
		return true;

	// name contains ?
	for (auto gameName : it->second)
		if (indexedName.find(gameName) != std::string::npos)
			return true;

	return false;
}

const bool MameNames::isSpinner(const std::string& _nameName, const std::string& systemName, bool isArcade)
{
	if (isArcade)
	{
		auto it = mArcadeRoms.find(_nameName);
		if (it != mArcadeRoms.cend())
			return hasFlag(it->second.type, ArcadeRomType::SPINNER);

		return false;
	}

	auto it = mNonArcadeSpinnerGames.find(systemName);
	if (it == mNonArcadeSpinnerGames.cend())
		return false;

	std::string indexedName = getIndexedName(_nameName);

	// Exact match ?
	if (it->second.find(indexedName) != it->second.cend())
		return true;

	// name contains ?
	for (auto gameName : it->second)
		if (indexedName.find(gameName) != std::string::npos)
			return true;

	return false;
}
