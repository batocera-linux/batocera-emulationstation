#include "KeyboardMapping.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include <rapidjson/rapidjson.h>
#include <rapidjson/pointer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <algorithm>

std::vector<KeyMappingFile::KeyName> KeyMappingFile::keyMap =
{
	{ "KEY_ESC" },
	{ "KEY_1" },
	{ "KEY_2" },
	{ "KEY_3" },
	{ "KEY_4" },
	{ "KEY_5" },
	{ "KEY_6" },
	{ "KEY_7" },
	{ "KEY_8" },
	{ "KEY_9" },
	{ "KEY_0" },
	{ "KEY_MINUS", "s02", "-" },
	{ "KEY_EQUAL", "s03", "=" },
	{ "KEY_BACKSPACE" },
	{ "KEY_TAB" },
	{ "KEY_Q" },
	{ "KEY_W" },
	{ "KEY_E" },
	{ "KEY_R" },
	{ "KEY_T" },
	{ "KEY_Y" },
	{ "KEY_U" },
	{ "KEY_I" },
	{ "KEY_O" },
	{ "KEY_P" },
	{ "KEY_LEFTBRACE", "s04", "[" },
	{ "KEY_RIGHTBRACE", "s05", "]" },
	{ "KEY_ENTER" },
	{ "KEY_LEFTCTRL" },
	{ "KEY_A" },
	{ "KEY_S" },
	{ "KEY_D" },
	{ "KEY_F" },
	{ "KEY_G" },
	{ "KEY_H" },
	{ "KEY_J" },
	{ "KEY_K" },
	{ "KEY_L" },
	{ "KEY_SEMICOLON", "s06", ":" },
	{ "KEY_APOSTROPHE", "s07", "\"" },
	{ "KEY_GRAVE", "s01", "~" },
	{ "KEY_LEFTSHIFT" },
	{ "KEY_BACKSLASH", "s08", "\\" },
	{ "KEY_Z" },
	{ "KEY_X" },
	{ "KEY_C" },
	{ "KEY_V" },
	{ "KEY_B" },
	{ "KEY_N" },
	{ "KEY_M" },
	{ "KEY_COMMA", "s09", "," },
	{ "KEY_DOT", "s10", "." },
	{ "KEY_SLASH", "s11", "/" },
	{ "KEY_RIGHTSHIFT" },
	{ "KEY_KPASTERISK" },
	{ "KEY_LEFTALT" },
	{ "KEY_SPACE" },
	{ "KEY_CAPSLOCK" },
	{ "KEY_F1" },
	{ "KEY_F2" },
	{ "KEY_F3" },
	{ "KEY_F4" },
	{ "KEY_F5" },
	{ "KEY_F6" },
	{ "KEY_F7" },
	{ "KEY_F8" },
	{ "KEY_F9" },
	{ "KEY_F10" },
	{ "KEY_NUMLOCK" },
	{ "KEY_SCROLLLOCK" },
	{ "KEY_KP7" },
	{ "KEY_KP8" },
	{ "KEY_KP9" },
	{ "KEY_KPMINUS" },
	{ "KEY_KP4" },
	{ "KEY_KP5" },
	{ "KEY_KP6" },
	{ "KEY_KPPLUS" },
	{ "KEY_KP1" },
	{ "KEY_KP2" },
	{ "KEY_KP3" },
	{ "KEY_KP0" },
	{ "KEY_KPDOT" },
	{ "KEY_F11" },
	{ "KEY_F12" },
	{ "KEY_KPENTER" },
	{ "KEY_RIGHTCTRL" },
	{ "KEY_KPSLASH" },
	{ "KEY_RIGHTALT" },
	{ "KEY_HOME" },
	{ "KEY_UP" },
	{ "KEY_PAGEUP" },
	{ "KEY_LEFT" },
	{ "KEY_RIGHT" },
	{ "KEY_END" },
	{ "KEY_DOWN" },
	{ "KEY_PAGEDOWN" },
	{ "KEY_INSERT" },
	{ "KEY_DELETE" },
	{ "KEY_PAUSE" },
	{ "KEY_PRINT" },
	{ "KEY_MENU" },

	{ "KEY_LEFTMETA" },
	{ "KEY_RIGHTMETA" },

	{ "BTN_LEFT" },
	{ "BTN_RIGHT" },
	{ "BTN_MIDDLE" },
};




std::vector<KeyMappingFile::KeyName> KeyMappingFile::triggerNames =
{
	{ "up" },
	{ "down" },
	{ "left" },
	{ "right" },
	{ "start" },
	{ "select" },
	{ "a" },
	{ "b" },
	{ "x" },
	{ "y" },
	{ "joystick1up", "j1up" },
	{ "joystick1left", "j1left" },
	{ "joystick2up", "j2up" },
	{ "joystick2left", "j2left" },
	{ "pageup", "l1" },
	{ "pagedown", "r1" },
	{ "l2" },
	{ "r2" },
	{ "l3" },
	{ "r3" },
	{ "hotkey" },	
	{ "joystick1down", "j1down" },
	{ "joystick1right", "j1right" },
	{ "joystick2down", "j2down" },
	{ "joystick2right", "j2right" }
};


std::vector<std::string> targetOrders =
{
	"KEY_LEFTCTRL", "KEY_LEFTMETA", "KEY_LEFTSHIFT", "KEY_LEFTALT",
	"KEY_RIGHTCTRL", "KEY_RIGHTMETA", "KEY_RIGHTSHIFT", "KEY_RIGHTALT", 
};

KeyMappingFile::KeyName::KeyName(const std::string& k, const std::string& p2k, const std::string& p2kAlt)
{
	key = k;
	p2kname = p2k;
	p2knameAlt = p2kAlt;

	if (p2kname.empty())
	{
		if (k.find("_") != std::string::npos)
			p2kname = Utils::String::toLower(key.substr(k.find("_") + 1));
		else
			p2kname = Utils::String::toLower(k);		
	}
}

bool KeyMappingFile::checkTriggerExists(const std::string& target, const std::string& type)
{
	if (type == "mouse")
		return (target == "joystick1" || target == "joystick2");

	for (auto map : triggerNames)
		if (map.key == target)
			return true;

	return false;
}

bool KeyMappingFile::checkTargetExists(const std::string& target)
{
	for (auto map : keyMap)
		if (map.key == target)
			return true;

	return false;
}

std::string KeyMappingFile::getTriggerFromP2k(const std::string& target)
{
	for (auto map : triggerNames)
		if (map.p2kname == target || map.p2knameAlt == target)
			return map.key;

	return "";
}

std::string KeyMappingFile::getTargetFromP2k(const std::string& target)
{
	for (auto map : keyMap)
		if (map.p2kname == target || map.p2knameAlt == target)
			return map.key;

	return "";
}

KeyMappingFile KeyMappingFile::load(const std::string& fileName)
{
	KeyMappingFile ret;
	ret.path = fileName;
	
	if (!Utils::FileSystem::exists(fileName))
		return ret;

	std::string json = Utils::FileSystem::readAllText(fileName);

	rapidjson::Document doc;
	doc.Parse(json.c_str());

	if (doc.HasParseError())
	{
		LOG(LogError) << "KeyMappingFile - Error parsing JSON.";
		return ret;
	}

	std::string rootName;

	for (int i = 0; i < 4; i++)
	{
		std::string playerID;
		playerID = "actions_player" + std::to_string(i + 1);
		PlayerMapping pm;

		if (doc.HasMember(playerID.c_str()) && doc[playerID.c_str()].IsArray())
		{		
			for (auto& action : doc[playerID.c_str()].GetArray())
			{
				KeyMapping map;

				if (action.HasMember("type"))
					map.type = action["type"].GetString();

				if (action.HasMember("mode"))
					map.mode = action["mode"].GetString();

				if (action.HasMember("description"))
					map.description = action["description"].GetString();

				if (action.HasMember("trigger"))
				{
					if (action["trigger"].IsArray())
					{
						for (auto& target : action["trigger"].GetArray())
						{
							if (checkTriggerExists(target.GetString(), map.type))
								map.triggers.insert(target.GetString());
						}
					}
					else
					{
						if (checkTriggerExists(action["trigger"].GetString(), map.type))
							map.triggers.insert(action["trigger"].GetString());
					}
				}

				if (action.HasMember("target"))
				{
					if (action["target"].IsArray())
					{
						for (auto& target : action["target"].GetArray())
						{
							if (map.type != "key" || checkTargetExists(target.GetString()))
								map.targets.insert(target.GetString());
						}
					}
					else
					{
						if (map.type != "key" || checkTargetExists(action["target"].GetString()))
							map.targets.insert(action["target"].GetString());
					}				
				}

				if ((map.targets.size() > 0 || map.type == "mouse") && map.triggers.size() > 0)
					pm.mappings.push_back(map);
			}

			ret.players.push_back(pm);
		}
		else 
			ret.players.push_back(pm);
	}

	return ret;
}

std::string KeyMappingFile::joinStrings(const std::set<std::string>& items)
{
	std::string data;

	for (auto line : items)
	{
		if (!data.empty())
			data += ", ";

		data += "\"" + line + "\"";
	}

	return data;
};


void KeyMappingFile::deleteFile()
{
	if (!path.empty() && Utils::FileSystem::exists(path))
		Utils::FileSystem::removeFile(path);
}

void KeyMappingFile::save(const std::string& fileName)
{
	rapidjson::StringBuffer s;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(s);

	writer.StartObject();

	int idx = 1;
	for (auto player : players)
	{
		if (player.mappings.size() == 0)
		{
			idx++;
			continue;
		}
		
		writer.Key(("actions_player" + std::to_string(idx)).c_str());
		writer.StartArray();

		for (int m = 0 ; m < player.mappings.size() ; m++)		
		{
			auto mapping = player.mappings[m];

			if (mapping.triggers.size() == 0 || (mapping.targets.size() == 0 && mapping.type != "mouse"))
				continue;

			writer.StartObject();
			
			writer.Key("trigger");

			if (mapping.triggers.size() == 1)
				writer.String((*mapping.triggers.begin()).c_str());
			else
			{
				writer.StartArray();
				for (auto v : mapping.triggers)
					writer.String(v.c_str());

				writer.EndArray();
			}

			if (!mapping.mode.empty())
			{
				writer.Key("mode");
				writer.String(mapping.mode.c_str());
			}

			if (!mapping.type.empty())
			{
				writer.Key("type");
				writer.String(mapping.type.c_str());
			}

			if (mapping.targets.size() > 0)
			{
				writer.Key("target");

				if (mapping.targets.size() == 1)
					writer.String((*mapping.targets.begin()).c_str());
				else
				{
					writer.StartArray();

					for (auto modifier : targetOrders)
						if (mapping.targets.find(modifier) != mapping.targets.cend())
							writer.String(modifier.c_str());

					for (auto v : mapping.targets)
						if (std::find(targetOrders.cbegin(), targetOrders.cend(), v) == targetOrders.cend())
							writer.String(v.c_str());

					writer.EndArray();
				}
			}

			if (!mapping.description.empty())
			{
				writer.Key("description");
				writer.String(mapping.description.c_str());
			}

			writer.EndObject();
		}

		writer.EndArray();
		idx++;
	}

	writer.EndObject();
	
	std::string data = s.GetString();

	if (!Utils::FileSystem::isDirectory(Utils::FileSystem::getParent(path)))
		Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(path));

	if (fileName.empty())
	{
		if (!path.empty())
			Utils::FileSystem::writeAllText(path, data);
	}
	else
		Utils::FileSystem::writeAllText(fileName, data);
}

KeyMappingFile KeyMappingFile::fromP2k(const std::string& fileName)
{
	KeyMappingFile ret;
	ret.path = fileName;

	auto lines = Utils::String::split(Utils::FileSystem::readAllText(fileName), '\n', true);

	for (int i = 0; i < 4; i++)
	{
		PlayerMapping pm;

		auto pidx = std::to_string(i) + ":";

		for (auto ll : lines)
		{
			auto line = Utils::String::trim(ll);
			if (!Utils::String::startsWith(line, pidx))
				continue;

			std::string description;

			line = line.substr(2);
			auto comment = line.find(";");
			if (comment != std::string::npos)
			{
				description = line.substr(comment + 1);
				if (Utils::String::startsWith(description, ";")) // Descriptions need two ;;
					description = Utils::String::trim(description.substr(1));
				else
					description = description = "";

				line = Utils::String::trim(line.substr(0, comment));
			}
			
			auto values = Utils::String::split(line, '=', true);
			if (values.size() == 2)
			{
				KeyMapping map;
				map.type = "key";
				map.description = description;

				std::string p2k = getTargetFromP2k(Utils::String::trim(Utils::String::toLower(values[1])));
				if (p2k.empty())
					continue;

				map.targets.insert(p2k);

				std::string trigger = getTriggerFromP2k(Utils::String::trim(Utils::String::toLower(values[0])));
				if (trigger.empty())
					continue;

				map.triggers.insert(trigger);

				if (map.targets.size() > 0 && map.triggers.size() > 0)
					pm.mappings.push_back(map);
			}
		}

		if (pm.mappings.size() > 0)
			ret.players.push_back(pm);
	}

	return ret;
}



bool KeyMappingFile::isValid()
{
	if (players.size() == 0)
		return false;

	for (auto p : players)
		for (auto m : p.mappings)
			if (m.targets.size() > 0 && m.triggers.size() > 0)
				return true;

	return false;
}

bool KeyMappingFile::KeyMapping::triggerEquals(const std::string& names)
{
	std::set<std::string> test;

	for (auto s : Utils::String::splitAny(names, " +"))
		if (!Utils::String::trim(s).empty())
			test.insert(Utils::String::trim(s));

	if (test.size() == 0)
		return false;

	if (test.size() != triggers.size())
		return false;

	for (auto t : test)
	{
		if (triggers.find(t) == triggers.cend())
			return false;
	}

	return true;
}

std::string KeyMappingFile::KeyMapping::toTargetString()
{
	std::string data;
	
	for (auto modifier : targetOrders)
	{
		if (targets.find(modifier) == targets.cend())
			continue;

		if (!data.empty())
			data += "+";

		auto pos = modifier.find("KEY_LEFT");
		if (pos != std::string::npos)
		{
			data += modifier.substr(pos + 8);
			continue;
		}

		pos = modifier.find("KEY_");
		if (pos != std::string::npos)
			data += modifier.substr(pos + 4);
	}
	
	for (auto target : targets)
	{
		if (std::find(targetOrders.cbegin(), targetOrders.cend(), target) != targetOrders.cend())
			continue;

		if (!data.empty())
			data += "+";

		auto pos = target.find("KEY_");
		if (pos != std::string::npos)
			data += target.substr(pos + 4);
		else
		{
			pos = target.find("BTN_");
			if (pos != std::string::npos)
				data += "MOUSE " + target.substr(pos + 4);
			else
				data += target;
		}
	}

	if (type != "key" && !type.empty())
		data = "(" + type + ") " + data;

	return data;
}

bool KeyMappingFile::updateMapping(int player, const std::string& trigger, const std::set<std::string>& targets)
{
	if (trigger.empty())
		return false;

	while (player >= players.size())
		players.push_back(PlayerMapping());

	PlayerMapping& pm = players[player];

	for (auto it = pm.mappings.begin(); it != pm.mappings.end(); ++it)
	{
		if (it->triggerEquals(trigger))
		{
			if (targets.empty())
			{
				pm.mappings.erase(it);
				return true;
			}

			if (joinStrings(it->targets) != joinStrings(targets))
			{
				it->targets = targets;
				return true;
			}

			return false;
		}
	}
			
	if (targets.size() == 0)
		return false;

	std::set<std::string> triggers;

	for (auto s : Utils::String::splitAny(trigger, " +"))
		if (!Utils::String::trim(s).empty())
			triggers.insert(Utils::String::trim(s));

	if (triggers.size() == 0)
		return false;

	KeyMapping km;
	km.triggers = triggers;
	km.type = "key";
	km.targets = targets;
	pm.mappings.push_back(km);
	return true;
}

std::string KeyMappingFile::getMouseMapping(int player)
{
	if (player >= players.size())
		return "";

	PlayerMapping& pm = players[player];
	for (auto it = pm.mappings.begin(); it != pm.mappings.end(); ++it)
		if (it->type == "mouse" && it->triggers.size() > 0)
			return *(it->triggers.begin());

	return "";
}

void KeyMappingFile::clearAnalogJoysticksMappings(int player, const std::string& trigger)
{
	if (player >= players.size())
		return;

	PlayerMapping& pm = players[player];

	for (auto it = pm.mappings.begin(); it != pm.mappings.end(); )
	{
		if (it->type == "key")
		{
			if (trigger == "joystick2")
			{
				it->triggers.erase("joystick2up");
				it->triggers.erase("joystick2left");
				it->triggers.erase("joystick2down");
				it->triggers.erase("joystick2right");
			}
			else
			{
				it->triggers.erase("joystick1up");
				it->triggers.erase("joystick1left");
				it->triggers.erase("joystick1down");
				it->triggers.erase("joystick1right");
			}

			if (it->triggers.size() == 0)
			{
				pm.mappings.erase(it);
				continue;
			}
		}

		it++;
	}
}

bool KeyMappingFile::setMouseMapping(int player, const std::string& trigger)
{
	while (player >= players.size())
		players.push_back(PlayerMapping());

	PlayerMapping& pm = players[player];

	for (auto it = pm.mappings.begin(); it != pm.mappings.end(); ++it)
	{
		if (it->type == "mouse")
		{
			if (trigger.empty())
			{
				pm.mappings.erase(it);
				return true;
			}

			it->triggers.clear();
			it->triggers.insert(trigger);
			it->targets.clear();

			clearAnalogJoysticksMappings(player, trigger);

			return true;
		}
	}

	if (trigger.empty())
		return false;

	KeyMapping km;
	km.triggers.insert(trigger);	
	km.type = "mouse";
	pm.mappings.push_back(km);

	clearAnalogJoysticksMappings(player, trigger);

	return true;
}

bool KeyMappingFile::removeMapping(int player, const std::string& trigger)
{
	if (trigger.empty())
		return false;

	if (player >= players.size())
		return false;

	PlayerMapping& pm = players[player];

	for (auto it = pm.mappings.begin(); it != pm.mappings.end(); ++it)
	{
		if (it->triggerEquals(trigger))
		{
			pm.mappings.erase(it);
			return true;
		}
	}

	return false;
}

KeyMappingFile::KeyMapping KeyMappingFile::getKeyMapping(int player, const std::string& trigger)
{
	KeyMappingFile::KeyMapping empty;
	if (trigger.empty())
		return empty;

	if (player >= players.size())
		return empty;

	PlayerMapping& pm = players[player];

	for (auto it = pm.mappings.begin(); it != pm.mappings.end(); ++it)
		if (it->triggerEquals(trigger))
			return (*it);

	return empty;
}



std::string KeyMappingFile::getMappingDescription(int player, const std::string& trigger)
{
	if (trigger.empty())
		return "";

	if (player >= players.size())
		return "";

	PlayerMapping& pm = players[player];

	for (auto it = pm.mappings.begin(); it != pm.mappings.end(); ++it)
		if (it->triggerEquals(trigger))
			return it->description;

	return "";
}

bool KeyMappingFile::updateMappingDescription(int player, const std::string& trigger, const std::string& description)
{
	if (trigger.empty())
		return false;

	if (player >= players.size())
		return false;

	PlayerMapping& pm = players[player];

	for (auto it = pm.mappings.begin(); it != pm.mappings.end(); ++it)
	{
		if (it->triggerEquals(trigger))
		{
			if (it->description != description)
			{
				it->description = description;
				return true;
			}
			
		}
	}

	return false;
}