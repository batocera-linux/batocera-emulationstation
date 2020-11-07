#pragma once

#include <string>
#include <vector>
#include <set>

class KeyMappingFile
{
public:
	static KeyMappingFile load(const std::string& fileName);
	static KeyMappingFile fromP2k(const std::string& fileName);

	static bool checkTriggerExists(const std::string& target, const std::string& type);
	static bool checkTargetExists(const std::string& target);

	static std::string getTriggerFromP2k(const std::string& target);
	static std::string getTargetFromP2k(const std::string& target);

	bool updateMapping(int player, const std::string& trigger, const std::set<std::string>& targets);

	bool removeMapping(int player, const std::string& trigger);
	bool updateMappingDescription(int player, const std::string& trigger, const std::string& description);
	std::string getMappingDescription(int player, const std::string& trigger);

	std::string getMouseMapping(int player);
	bool setMouseMapping(int player, const std::string& trigger);

	bool isValid();
	void save(const std::string& fileName = "");
	void deleteFile();

	struct KeyMapping
	{
		bool triggerEquals(const std::string& names);
		std::string toTargetString();

		std::set<std::string> triggers;
		std::string			  type;
		std::string			  mode;
		std::set<std::string> targets;		

		std::string			  description;
	};

	struct PlayerMapping
	{
		std::vector<KeyMapping> mappings;
	};

	std::vector<PlayerMapping> players;
	std::string path;


	class KeyName
	{
	public:
		KeyName(const std::string& k, const std::string& p2k = "", const std::string& p2kAlt = "");

		std::string key;
		std::string p2kname;
		std::string p2knameAlt;
	};

	KeyMapping getKeyMapping(int player, const std::string& trigger);

	static std::vector<KeyName> keyMap;
	static std::vector<KeyName> triggerNames;

private:
	static std::string joinStrings(const std::set<std::string>& items);
	void clearAnalogJoysticksMappings(int player, const std::string& trigger);
};

class IKeyboardMapContainer
{
public:
	virtual bool hasKeyboardMapping() = 0;
	virtual KeyMappingFile getKeyboardMapping() = 0;
};
