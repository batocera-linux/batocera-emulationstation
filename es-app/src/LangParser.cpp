#include "LangParser.h"
#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"
#include "SystemData.h"
#include "FileData.h"
#include "SystemConf.h"
#include <algorithm>

struct LangData
{
	std::vector<std::string> value;
	std::string lang;
	std::string region;
};

std::string LangInfo::getLanguageString()
{
	std::string data;

	for (auto line : languages)
	{
		if (!data.empty())
			data += ",";

		data += line;
	}

	return data;
}

void LangInfo::extractLang(std::string val)
{
	static std::vector<LangData> langDatas =
	{
		{ { "usa", "us", "u" }, "en", "us" },

		{ { "europe", "eu", "e", "ue" }, "", "eu" },

		{ { "w", "wor", "world" }, "en", "wr" },
		{ { "uk" }, "en", "eu" },
		{ { "es", "spain", "s" }, "es", "eu" },
		{ { "fr", "france", "fre", "french", "f" }, "fr", "eu" },
		{ { "de", "germany", "d" }, "de", "eu" },
		{ { "it", "italy", "i" }, "it", "eu" },
		{ { "nl", "netherlands" }, "nl", "eu" },
		{ { "gr", "greece" }, "gr", "eu" },
		{ { "no" }, "no", "eu" },
		{ { "sw", "sweden", "se" }, "sw", "eu" },
		{ { "pt", "portugal" }, "pt", "eu" },

		{ { "en" }, "en", "" },

		{ { "canada", "ca", "c", "fc" }, "fr", "wr" },

		{ { "jp", "japan", "ja", "j" }, "jp", "jp" },

		{ { "br", "brazil", "b" }, "br", "br" },
		{ { "ru", "r" }, "ru", "ru" },
		{ { "kr", "korea", "k" }, "kr", "kr" },
		{ { "cn", "china", "hong", "kong", "ch", "hk", "as", "tw" }, "cn", "cn" },

		{ { "in", "ìndia" }, "in", "in" },
	};

	for (auto s : Utils::String::splitAny(val, "_, "))
	{
		bool clearLang = s.find("t-") != std::string::npos;

		s = Utils::String::replace(s, "_", "");
		s = Utils::String::replace(s, "t+", "");
		s = Utils::String::replace(s, "t-", "");

		for (auto langData : langDatas)
		{
			if (std::find(langData.value.cbegin(), langData.value.cend(), s) != langData.value.cend())
			{
				if (!langData.lang.empty())
				{
					if (clearLang)
						languages.clear();

					languages.insert(langData.lang);
				}

				if (!langData.region.empty())
					region = langData.region;
			}
		}
	}
}

LangInfo LangInfo::parse(std::string rom, SystemData* system)
{
	LangInfo info;
	if (rom.empty() || (system && system->hasPlatformId(PlatformIds::IMAGEVIEWER)))
		return info;

	std::string fileName = Utils::String::toLower(Utils::FileSystem::getFileName(rom));

	for (auto s : Utils::String::extractStrings(fileName, "(", ")"))
		info.extractLang(s);

	for (auto s : Utils::String::extractStrings(fileName, "[", "]"))
		info.extractLang(s);

	for (auto s : Utils::String::extractStrings(fileName, "_", "_"))
		info.extractLang(s);

	if (system != nullptr)
	{
		static std::string japanDefaults = "pc88|pc98|pcenginecd|pcfx|satellaview|sg1000|sufami|wswan|wswanc|x68000";

		if (info.region.empty())
		{
			if (japanDefaults.find(system->getName()) != std::string::npos)
				info.region = "jp";
			else if (system->hasPlatformId(PlatformIds::ARCADE))
			{
				if (fileName.find("j.zip") != std::string::npos)
					info.region = "jp";
				else
					info.region = "us";
			}
			else if (system->getName() == "thomson")
				info.region = "eu";
		}

		if (info.languages.size() == 0)
		{
			if (japanDefaults.find(system->getName()) != std::string::npos)
				info.languages.insert("jp");
			else if (system->getName() == "thomson")
				info.languages.insert("fr");
			else if (system->hasPlatformId(PlatformIds::ARCADE) && fileName.find("j.zip") != std::string::npos)
				info.languages.insert("jp");
			else
				info.languages.insert("en");
		}
	}

	return info;
}

std::string LangInfo::getFlag(const std::string lang, const std::string region)
{	
	if (lang.empty() && region.empty())
		return "us";

	std::string locale = SystemConf::getInstance()->get("system.language");
	if (locale.empty())
		locale = "en";
	else
	{
		auto shortNameDivider = locale.find("_");
		if (shortNameDivider != std::string::npos)
			locale = Utils::String::toLower(locale.substr(0, shortNameDivider));
	}

	auto langs = Utils::String::split(lang, ',');
	if (locale != "en" && std::find(langs.cbegin(), langs.cend(), locale) != langs.cend())
		return locale;

	if (langs.size() >= 1 && region.empty() && langs[0] == "en")
		return "us";
	else if (langs.size() == 1 && langs[0] != "en")
		return langs[0];
		
	if (region == "eu" && langs.size() == 1 && std::find(langs.cbegin(), langs.cend(), "en") != langs.cend())
		return "uk";
	else if (!region.empty())
		return region;

	return "eu";
}