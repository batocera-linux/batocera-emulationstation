#include "GuiProfilesSettings.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "LocaleES.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static std::vector<std::string> readProfileList()
{
	std::vector<std::string> profiles;
	FILE* pipe = popen("batocera-profiles list 2>/dev/null", "r");
	if (!pipe)
		return profiles;
	char line[256];
	while (fgets(line, sizeof(line), pipe))
	{
		std::string s(line);
		while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
			s.pop_back();
		if (!s.empty())
			profiles.push_back(s);
	}
	pclose(pipe);
	return profiles;
}

static std::string readCurrentProfile()
{
	FILE* pipe = popen("batocera-profiles current 2>/dev/null", "r");
	if (!pipe)
		return "";
	char line[256] = "";
	fgets(line, sizeof(line), pipe);
	pclose(pipe);
	std::string s(line);
	while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
		s.pop_back();
	return s;
}

// Read a named field from a profile conf file (key=value format).
static std::string readConfField(const std::string& confPath, const std::string& key)
{
	FILE* f = fopen(confPath.c_str(), "r");
	if (!f)
		return "";
	std::string prefix = key + "=";
	char line[256];
	while (fgets(line, sizeof(line), f))
	{
		std::string s(line);
		while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
			s.pop_back();
		if (s.size() > prefix.size() && s.substr(0, prefix.size()) == prefix)
		{
			fclose(f);
			return s.substr(prefix.size());
		}
	}
	fclose(f);
	return "";
}

// Resolve display label for a profile: display_name > RA username > fallback.
static std::string profileDisplayName(const std::string& confPath, const std::string& fallback)
{
	std::string dn = readConfField(confPath, "display_name");
	if (!dn.empty())
		return dn;
	std::string ra = readConfField(confPath, "username");
	if (!ra.empty())
		return ra;
	return fallback;
}

static bool isValidProfileName(const std::string& name)
{
	if (name.empty())
		return false;
	for (char c : name)
		if (!isalnum((unsigned char)c) && c != ' ' && c != '_' && c != '-')
			return false;
	return true;
}

GuiProfilesSettings::GuiProfilesSettings(Window* window)
	: GuiSettings(window, _("PROFILES").c_str())
{
	std::string current = readCurrentProfile();
	std::vector<std::string> profiles = readProfileList();

	addGroup(_("SWITCH PROFILE"));

	// DEFAULT entry: always present
	std::string defaultConf = "/userdata/profiles/.default-retroachievements.conf";
	std::string defaultName = profileDisplayName(defaultConf, _("DEFAULT"));
	std::string defaultLabel = current.empty()
		? (defaultName + " (" + _("ACTIVE") + ")")
		: defaultName;
	addEntry(defaultLabel, false,
		[window]
		{
			system("batocera-profiles switch");
			window->pushGui(new GuiMsgBox(window,
				_("SWITCHED TO DEFAULT PROFILE.\nTAKES EFFECT ON NEXT GAME LAUNCH."),
				_("OK"), nullptr));
		});

	for (auto p : profiles)
	{
		bool isActive = (p == current);
		std::string conf = "/userdata/profiles/" + p + "/retroachievements.conf";
		std::string displayName = profileDisplayName(conf, p);
		std::string label = isActive
			? (displayName + " (" + _("ACTIVE") + ")")
			: displayName;
		addEntry(label, false,
			[window, p]
			{
				system(("batocera-profiles switch \"" + p + "\"").c_str());
				window->pushGui(new GuiMsgBox(window,
					_("SWITCHED TO PROFILE:") + "\n" + p + "\n\n" +
					_("TAKES EFFECT ON NEXT GAME LAUNCH."),
					_("OK"), nullptr));
			});
	}

	addGroup(_("MANAGE"));

	addEntry(_("CREATE NEW PROFILE"), true,
		[window]
		{
			window->pushGui(new GuiTextEditPopupKeyboard(window,
				_("NEW PROFILE NAME"), "",
				[window](const std::string& name)
				{
					if (!isValidProfileName(name))
					{
						window->pushGui(new GuiMsgBox(window,
							_("INVALID PROFILE NAME.\nUSE LETTERS, NUMBERS, SPACES, - OR _ ONLY."),
							_("OK"), nullptr));
						return;
					}
					system(("batocera-profiles create \"" + name + "\"").c_str());
					window->pushGui(new GuiMsgBox(window,
						_("PROFILE CREATED:") + "\n" + name,
						_("OK"), nullptr));
				}, false));
		});

	// RENAME: sets display_name for any profile (default or named)
	addEntry(_("RENAME PROFILE"), true,
		[window, current, profiles]
		{
			// Build label for the profile being renamed
			std::string confPath = current.empty()
				? "/userdata/profiles/.default-retroachievements.conf"
				: "/userdata/profiles/" + current + "/retroachievements.conf";
			std::string existing = readConfField(confPath, "display_name");

			window->pushGui(new GuiTextEditPopupKeyboard(window,
				_("DISPLAY NAME"), existing,
				[window, current, confPath](const std::string& newName)
				{
					// Write display_name= into the profile's conf file via shell
					// (avoids rewriting the entire file in C++)
					std::string cmd = "grep -v '^display_name=' \"" + confPath + "\" > /tmp/ra_conf.tmp 2>/dev/null;"
						" echo 'display_name=" + newName + "' >> /tmp/ra_conf.tmp;"
						" mv /tmp/ra_conf.tmp \"" + confPath + "\"";
					system(cmd.c_str());
					window->pushGui(new GuiMsgBox(window,
						_("DISPLAY NAME SET TO:") + "\n" + newName,
						_("OK"), nullptr));
				}, false));
		});

	if (!current.empty())
	{
		addEntry(_("DELETE CURRENT PROFILE"), true,
			[window, current]
			{
				window->pushGui(new GuiMsgBox(window,
					_("DELETE PROFILE") + " \"" + current + "\"?\n" +
					_("ALL SAVES IN THIS PROFILE WILL ALSO BE DELETED."),
					_("YES"),
					[window, current]
					{
						system("batocera-profiles switch");
						system(("batocera-profiles delete \"" + current + "\"").c_str());
						window->pushGui(new GuiMsgBox(window,
							_("PROFILE DELETED. SWITCHED TO DEFAULT."),
							_("OK"), nullptr));
					},
					_("NO"), nullptr,
					GuiMsgBoxIcon::ICON_WARNING));
			});
	}
}
