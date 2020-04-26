#pragma once

#include <string>
#include <unordered_set>

class SystemData;

class LangInfo
{
public:
	static LangInfo parse(std::string rom, SystemData* system);
	static std::string getFlag(const std::string lang, const std::string region);

	std::string region;
	std::unordered_set<std::string> languages;

	std::string getLanguageString();
	bool empty() { return region.empty() && languages.size() == 0; }

private:
	void extractLang(std::string val);
};

