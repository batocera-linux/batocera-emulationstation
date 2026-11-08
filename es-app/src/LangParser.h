#pragma once

#include <string>
#include <unordered_set>

class SystemData;

class LangInfo
{
public:
	LangInfo() { mHardRegion = false; }

	static LangInfo parse(const std::string& rom, SystemData* system);
	static std::string getFlag(const std::string& lang, const std::string& region);

	std::string region;
	std::unordered_set<std::string> languages;

	std::string getLanguageString();
	bool empty() { return region.empty() && languages.size() == 0; }

private:
	void extractLang(const std::string& val);

	bool mHardRegion;
};

