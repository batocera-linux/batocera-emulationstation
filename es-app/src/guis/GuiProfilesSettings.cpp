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

	// DEFAULT entry — always present
	std::string defaultLabel = current.empty()
		? (_("DEFAULT") + " (" + _("ACTIVE") + ")")
		: _("DEFAULT");
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
		std::string label = isActive
			? (p + " (" + _("ACTIVE") + ")")
			: p;
		addEntry(label, false,
			[window, p]
			{
				system(
					("batocera-profiles switch \"" + p + "\"").c_str());
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
					system(
						("batocera-profiles create \"" + name + "\"").c_str());
					window->pushGui(new GuiMsgBox(window,
						_("PROFILE CREATED:") + "\n" + name,
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
						system(
							("batocera-profiles delete \"" + current + "\"").c_str());
						window->pushGui(new GuiMsgBox(window,
							_("PROFILE DELETED. SWITCHED TO DEFAULT."),
							_("OK"), nullptr));
					},
					_("NO"), nullptr,
					GuiMsgBoxIcon::ICON_WARNING));
			});
	}
}
