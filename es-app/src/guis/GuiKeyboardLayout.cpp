#include "GuiKeyboardLayout.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "LocaleES.h"

#define KB_WIDTH 0.92f

std::vector<std::vector<std::string>> kbLayout =
{
	{ "KEY_ESC", "",	"KEY_F1", "KEY_F2", "KEY_F3", "KEY_F4", "KEY_F5", "KEY_F6", "KEY_F7", "KEY_F8", "KEY_F9", "KEY_F10", "KEY_F11", "KEY_F12", "KEY_PRINT", "KEY_SCROLLLOCK", "KEY_PAUSE", "KEY_NONE" },
	{ "KEY_GRAVE",		"KEY_1",			"KEY_2",		"KEY_3",	"KEY_4",	"KEY_5",	"KEY_6",	"KEY_7",	"KEY_8",		"KEY_9",		"KEY_0",			"KEY_MINUS",		"KEY_EQUAL",		"KEY_BACKSPACE",	"KEY_INSERT",	"KEY_HOME",		"KEY_PAGEUP",	"KEY_NUMLOCK",	"KEY_KPSLASH",	"KEY_KPASTERISK",	"KEY_KPMINUS" },
	{ "KEY_TAB",		"KEY_Q",			"KEY_W",		"KEY_E",	"KEY_R",	"KEY_T",	"KEY_Y",	"KEY_U",	"KEY_I",		"KEY_O",		"KEY_P",			"KEY_LEFTBRACE",	"KEY_RIGHTBRACE",	"KEY_BACKSLASH",	"KEY_DELETE",	"KEY_END",		"KEY_PAGEDOWN",	"KEY_KP7",		"KEY_KP8",		"KEY_KP9",			"KEY_KPPLUS" },
	{ "KEY_CAPSLOCK",	"KEY_A",			"KEY_S",		"KEY_D",	"KEY_F",	"KEY_G",	"KEY_H",	"KEY_J",	"KEY_K",		"KEY_L",		"KEY_SEMICOLON",	"KEY_APOSTROPHE",	"KEY_ENTER",		"KEY_ENTER",		"",				"",				"",				"KEY_KP4",		"KEY_KP5",		"KEY_KP6",			"KEY_KPPLUS",	"BTN_LEFT", "BTN_MIDDLE", "BTN_RIGHT" },
	{ "KEY_LEFTSHIFT",	"KEY_Z",			"KEY_X",		"KEY_C",	"KEY_V",	"KEY_B",	"KEY_N",	"KEY_M",	"KEY_COMMA",	"KEY_DOT",		"KEY_SLASH",		"KEY_RIGHTSHIFT",	"KEY_RIGHTSHIFT",	"KEY_RIGHTSHIFT",	"",				"KEY_UP",		"",				"KEY_KP1",		"KEY_KP2",		"KEY_KP3",			"KEY_KPENTER",	"BTN_LEFT", "BTN_MIDDLE", "BTN_RIGHT" },
	{ "KEY_LEFTCTRL",	"KEY_LEFTMETA",		"KEY_LEFTALT",	"KEY_SPACE", "KEY_SPACE","KEY_SPACE","KEY_SPACE","KEY_SPACE","KEY_SPACE",	"KEY_RIGHTALT", "KEY_RIGHTMETA",	"KEY_MENU",			"KEY_RIGHTCTRL",	"KEY_RIGHTCTRL",	"KEY_LEFT",		"KEY_DOWN",		"KEY_RIGHT",	"KEY_KP0",		"KEY_KP0",		"KEY_KPDOT",		"KEY_KPENTER",		"BTN_LEFT", "BTN_MIDDLE", "BTN_RIGHT" }
};

GuiKeyboardLayout::GuiKeyboardLayout(Window* window, const std::function<void(const std::set<std::string>&)>& okCallback, std::set<std::string>* activeKeys) : GuiComponent(window), mKeyboard(window, true, false), mImageToggle(false)
{
	setTag("popup");

	mOkCallback = okCallback;

	mX = 0;
	mY = 0;

	std::string sel = "KEY_ENTER";

	if (activeKeys != nullptr)
	{
		mActiveKeys = *activeKeys;
		if (mActiveKeys.size() > 0)
			sel = *mActiveKeys.begin();
	}

	for (int y = 0; y < kbLayout.size(); y++)
	{
		for (int x = 0; x < kbLayout[y].size(); x++)
		{
			if (kbLayout[y][x] == sel)
			{
				mX = x;
				mY = y;
				break;
			}
		}
	}

	setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());

	mKeyboard.setOrigin(0.5f, 0.5f);
	mKeyboard.setPosition(Renderer::getScreenWidth() / 2.0f, Renderer::getScreenHeight() / 2.0f);
	mKeyboard.setMaxSize(Renderer::getScreenWidth() * KB_WIDTH, Renderer::getScreenHeight() * KB_WIDTH);
	addChild(&mKeyboard);

	auto path = ResourceManager::getInstance()->getResourcePath(":/kblayout.svg");
	mKeyboardSvg = Utils::FileSystem::readAllText(path);
		
	selectKey(kbLayout[mY][mX]);
}

GuiKeyboardLayout::~GuiKeyboardLayout()
{
#if WIN32
	Utils::FileSystem::removeFile(Utils::FileSystem::getGenericPath(Utils::FileSystem::getTempPath() + "/kblayout-1.svg"));
	Utils::FileSystem::removeFile(Utils::FileSystem::getGenericPath(Utils::FileSystem::getTempPath() + "/kblayout-2.svg"));
#else
	Utils::FileSystem::removeFile("/tmp/kblayout-1.svg");
	Utils::FileSystem::removeFile("/tmp/kblayout-2.svg");
#endif
}

bool GuiKeyboardLayout::isEnabled()
{
	return ResourceManager::getInstance()->fileExists(":/kblayout.svg");
}

std::string colorToHex(long unsigned int hex)
{
	char hash_cstr[10];
	sprintf(hash_cstr, "#%08lX", hex);
	std::string ret = hash_cstr;
	return Utils::String::toLower(ret.substr(0, 7));
}

void GuiKeyboardLayout::selectKey(const std::string& keyName)
{
	auto theme = ThemeData::getMenuTheme();

	mImageToggle = !mImageToggle;

	auto svg = mKeyboardSvg;
	auto rects = Utils::String::extractStrings(mKeyboardSvg, "<rect", "/>");

	if (mSelectedKeys.size() > 0)
	{
		// Set selected key red
		for (auto activeKey : mSelectedKeys)
		{
			for (auto rect : rects)
			{
				if (rect.find("\"" + activeKey + "\"") != std::string::npos)
				{
					auto resp = Utils::String::replace(rect, "fill:#808080", "fill:#395DAE");
					svg = Utils::String::replace(svg, rect, resp);
				}
			}
		}
	}
	else
	{
		// Set active key red
		for (auto activeKey : mActiveKeys)
		{
			for (auto rect : rects)
			{
				if (rect.find("\"" + activeKey + "\"") != std::string::npos)
				{
					auto resp = Utils::String::replace(rect, "fill:#808080", "fill:#AB4F6F");
					svg = Utils::String::replace(svg, rect, resp);
				}
			}
		}
	}

	// Set selected key blue
	rects = Utils::String::extractStrings(svg, "<rect", "/>");
	for (auto rect : rects)
	{
		if (rect.find("\""+ keyName +"\"") != std::string::npos)
		{
			auto resp = Utils::String::replace(rect, "fill:#808080", "fill:#597DCE");
			resp = Utils::String::replace(resp, "fill:#395DAE", "fill:#698DDE");
			resp = Utils::String::replace(resp, "fill:#AB4F6F", "fill:#698DDE");
			svg = Utils::String::replace(svg, rect, resp);
		}
	}

	/*
    svg = Utils::String::replace(svg, "fill:#333333", "fill:" + colorToHex(theme->Background.color));
	svg = Utils::String::replace(svg, "fill:#ffffff", "fill:"+ colorToHex(theme->Text.selectedColor));
	svg = Utils::String::replace(svg, "fill:#808080", "fill:" + colorToHex(Renderer::mixColors(theme->Background.color, theme->Text.color, 0.33f)));
	*/

	svg = Utils::String::replace(svg, "fill:#333333", "fill:#242424");
	svg = Utils::String::replace(svg, "fill:#808080", "fill:#404040");

	// Optimize required size
	svg = Utils::String::replace(svg, ";fill-opacity:1;fill-rule:nonzero;stroke:none", "");
	svg = Utils::String::replace(svg, ";stroke-linejoin:round;stroke-miterlimit:4;stroke-dasharray:none;stroke-dashoffset:0;stroke-opacity:1;paint-order:stroke markers fill", "");
	svg = Utils::String::replace(svg, ";stroke-width:3.6312573", "");
	svg = Utils::String::replace(svg, ";stroke-width:4.4454608", "");
	svg = Utils::String::replace(svg, ";stroke-width:5.34725142", "");
	svg = Utils::String::replace(svg, ";stroke-width:5.31482887", "");
	svg = Utils::String::replace(svg, ";stroke-width:5.14240026", "");
	svg = Utils::String::replace(svg, "inkscape:connector-curvature=\"0\"", "");

#if WIN32
	auto newPath = Utils::FileSystem::getGenericPath(Utils::FileSystem::getTempPath() + "/kblayout"+ std::string(mImageToggle ? "-1" : "-2") +".svg");
#else
	auto newPath = "/tmp/kblayout" + std::string(mImageToggle ? "-1" : "-2") + ".svg";
#endif

	Utils::FileSystem::writeAllText(newPath, svg);
	mKeyboard.setImage(newPath);	
}

bool GuiKeyboardLayout::input(InputConfig* config, Input input)
{
	if (GuiComponent::input(config, input))
		return true;

	if (input.value != 0 && (config->isMappedTo(BUTTON_OK, input) || config->isMappedTo("start", input)))
	{
		if (mOkCallback)
		{
			std::set<std::string> set = mSelectedKeys;
			set.insert(kbLayout[mY][mX]);
			if (kbLayout[mY][mX] == "KEY_NONE")
				set.clear();

			mOkCallback(set);
		}

		delete this;
		return true;
	}

	if (input.value != 0 && (config->isMappedTo("x", input)))
	{
		auto currCell = kbLayout[mY][mX];

		if (mSelectedKeys.find(currCell) == mSelectedKeys.cend())
			mSelectedKeys.insert(currCell);
		else
			mSelectedKeys.erase(currCell);
		
		selectKey(currCell);
		return true;
	}

	if (input.value != 0 && (config->isMappedTo("y", input)))
	{
		if (mOkCallback)
			mOkCallback(std::set<std::string>());

		delete this;
		return true;
	}

	if (input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		delete this;
		return true;
	}

	int x = mX;
	int y = mY;
	auto current = kbLayout[mY][mX];

	if (config->isMappedLike("left", input) && input.value != 0)
	{
		if (x == 0)
			x = kbLayout[mY].size() - 1;
		else
			x--;

		while (x > 0 && (kbLayout[y][x] == "" || kbLayout[y][x] == current))
			x--;
	}
	else if (config->isMappedLike("right", input) && input.value != 0)
	{
		if (x + 1 >= kbLayout[mY].size())
			x = 0;
		else
			x++;

		while (x + 1 < kbLayout[mY].size() && (kbLayout[y][x] == "" || kbLayout[y][x] == current))
			x++;

		if (x + 1 == kbLayout[mY].size() && (kbLayout[y][x] == "" || kbLayout[y][x] == current))
			x = 0;
	}
	else if (config->isMappedLike("down", input) && input.value != 0)
	{
		y++;

		while (y < kbLayout.size() && (kbLayout[y][x] == current || kbLayout[y][x] == ""))
			y++;

		if (y >= kbLayout.size())
			y = 0;
		
		if (x >= kbLayout[y].size())
			x = kbLayout[y].size() - 1;

		while (x > 0 && kbLayout[y][x] == "")
			x--;
	}
	else if (config->isMappedLike("up", input) && input.value != 0)
	{
		y--;

		while (y > 0 && (kbLayout[y][x] == current || kbLayout[y][x] == ""))
			y--;

		if (y < 0)
			y = kbLayout.size() - 1;

		if (x >= kbLayout[y].size())
			x = kbLayout[y].size() - 1;

		while (x > 0 && kbLayout[y][x] == "")
			x--;
	}

	if (x != mX || y != mY)
	{
		mX = x;
		mY = y;
		selectKey(kbLayout[mY][mX]);
	}

	return false;
}

std::vector<HelpPrompt> GuiKeyboardLayout::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt(BUTTON_OK, _("OK")));
	prompts.push_back(HelpPrompt("y", _("RESET")));
	prompts.push_back(HelpPrompt("x", _("ADD COMBINATION KEY")));
	return prompts;
}

