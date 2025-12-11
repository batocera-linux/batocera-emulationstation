#include "FileTag.h"
#include "LocaleES.h"

#define fake_gettext_finished _("FINISHED")
#define fake_gettext_slow _("SLOW")
#define fake_gettext_notworking _("NOT WORKING")
#define fake_gettext_buggy _("BUGGY")
#define fake_gettext_liked _("LIKED")
#define fake_gettext_disliked _("DISLIKED")

static std::vector<FileTag> defaultTags = 
{ 
	{ "FINISHED", _U("\uF11E") },
	{ "SLOW", _U("\uF071") },
	{ "NOT WORKING", _U("\uF057") },
	{ "BUGGY", _U("\uF070") },
	{ "LIKED", _U("\uF087") },
	{ "DISLIKED", _U("\uF088") }
};

std::vector<FileTag> FileTag::Values() { return defaultTags; }

std::vector<std::string> FileTag::getDisplayIcons(const std::string& names)
{
	std::vector<std::string> ret;

	for (auto name : Utils::String::split(names, ','))
	{
		std::string test = Utils::String::trim(name);

		auto it = std::find_if(defaultTags.cbegin(), defaultTags.cend(), [test](const FileTag& ss) { return ss.Name == test; });
		if (it == defaultTags.cend())
			continue;

		ret.push_back(it->displayIcon);
	}

	return ret;
}