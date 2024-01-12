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

MameNames::MameNames()
{
	std::string xmlpath;

	pugi::xml_document doc;
	pugi::xml_parse_result result;

	// Read mame games information
	xmlpath = ResourceManager::getInstance()->getResourcePath(":/mamenames.xml");
	if (Utils::FileSystem::exists(xmlpath))
	{		
		result = doc.load_file(WINSTRINGW(xmlpath).c_str());
		if (result)
		{
			LOG(LogInfo) << "Parsing XML file \"" << xmlpath << "\"...";

			pugi::xml_node root = doc;

			pugi::xml_node games = doc.child("games");
			if (games)
				root = games;

			std::string sTrue = "true";
			for (pugi::xml_node gameNode = root.child("game"); gameNode; gameNode = gameNode.next_sibling("game"))
			{
				NamePair namePair = { gameNode.child("mamename").text().get(), gameNode.child("realname").text().get() };
				mNamePairs.push_back(namePair);

				if (gameNode.attribute("vert") && gameNode.attribute("vert").value() == sTrue)
					mVerticalGames.insert(namePair.mameName);

				if (gameNode.attribute("gun") && gameNode.attribute("gun").value() == sTrue)
					mArcadeLightGunGames.insert(namePair.mameName);

				if (gameNode.attribute("wheel") && gameNode.attribute("wheel").value() == sTrue)
					mArcadeWheelGames.insert(namePair.mameName);
			}
		}
		else
			LOG(LogError) << "Error parsing XML file \"" << xmlpath << "\"!\n	" << result.description();
	}
	
	// Read bios
	xmlpath = ResourceManager::getInstance()->getResourcePath(":/mamebioses.xml"); 	
	if (Utils::FileSystem::exists(xmlpath))
	{
		result = doc.load_file(WINSTRINGW(xmlpath).c_str());
		if (result)
		{
			LOG(LogInfo) << "Parsing XML file \"" << xmlpath << "\"...";

			pugi::xml_node root = doc;

			pugi::xml_node bioses = doc.child("bioses");
			if (bioses)
				root = bioses;

			for (pugi::xml_node biosNode = root.child("bios"); biosNode; biosNode = biosNode.next_sibling("bios"))
			{
				std::string bios = biosNode.text().get();
				mMameBioses.insert(bios);
			}
		}
		else
			LOG(LogError) << "Error parsing XML file \"" << xmlpath << "\"!\n	" << result.description();

	}
	
	// Read devices
	xmlpath = ResourceManager::getInstance()->getResourcePath(":/mamedevices.xml"); 	
	if (Utils::FileSystem::exists(xmlpath))
	{
		result = doc.load_file(WINSTRINGW(xmlpath).c_str());
		if (result)
		{
			LOG(LogInfo) << "Parsing XML file \"" << xmlpath << "\"...";

			pugi::xml_node root = doc;

			pugi::xml_node devices = doc.child("devices");
			if (devices)
				root = devices;

			for (pugi::xml_node deviceNode = root.child("device"); deviceNode; deviceNode = deviceNode.next_sibling("device"))
			{
				std::string device = deviceNode.text().get();
				mMameDevices.insert(device);
			}
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

						for (pugi::xml_node gameNode = systemNode.child("game"); gameNode; gameNode = gameNode.next_sibling("game"))
						{
							if (!gameNode.attribute("name"))
								continue;

							std::string gameName = gameNode.attribute("name").value();
							if (gameName.empty())
								continue;
								
							if (gameNode.child("gun"))
								gunGames.insert(gameName);

							if (gameNode.child("wheel"))
								wheelGames.insert(gameName);
						}

						if (gunGames.size())
							mNonArcadeGunGames[Utils::String::trim(systemName)] = gunGames;

						if (wheelGames.size())
							mNonArcadeWheelGames[Utils::String::trim(systemName)] = wheelGames;
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
	size_t start = 0;
	size_t end   = mNamePairs.size();

	while(start < end)
	{
		const size_t index   = (start + end) / 2;
		const int    compare = strcmp(mNamePairs[index].mameName.c_str(), _mameName.c_str());

		if(compare < 0)       start = index + 1;
		else if( compare > 0) end   = index;
		else                  return mNamePairs[index].realName;
	}

	return _mameName;

} // getRealName

const bool MameNames::isBios(const std::string& _biosName)
{
	return (mMameBioses.find(_biosName) != mMameBioses.cend());
} // isBios

const bool MameNames::isDevice(const std::string& _deviceName)
{
	return (mMameDevices.find(_deviceName) != mMameDevices.cend());
} // isDevice

const bool MameNames::isVertical(const std::string& _nameName)
{
	return (mVerticalGames.find(_nameName) != mVerticalGames.cend());
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
		return mArcadeLightGunGames.find(_nameName) != mArcadeLightGunGames.cend();

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
		return mArcadeWheelGames.find(_nameName) != mArcadeWheelGames.cend();

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
