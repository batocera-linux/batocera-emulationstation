#include "controlcenter/ControlCenterApp.h"

#include "controlcenter/CcShell.h"
#include "resources/TextureData.h"
#include "ApiSystem.h"
#include "guis/GuiControlCenter.h"
#include "renderers/Renderer.h"
#include "resources/Font.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "InputConfig.h"
#include "InputManager.h"
#include "PowerSaver.h"
#include "Log.h"
#include "Paths.h"
#include "Settings.h"
#include "SystemConf.h"
#include "ThemeData.h"
#include "Window.h"

#include <SDL.h>
#include <pugixml/src/pugixml.hpp>
#include <algorithm>
#include <cstring>
#include <thread>
#include <sys/stat.h>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <dirent.h>
#include <set>
#include <unistd.h>
#include <linux/input.h>
#include <sys/ioctl.h>

static std::atomic<int> sToggleRequests(0);
static std::atomic<bool> sQuitRequested(false);

static void onToggleSignal(int)
{
	sToggleRequests++;
}

static void onQuitSignal(int)
{
	sQuitRequested = true;
}

static bool anyKeyDown(int fd)
{
	unsigned char keys[KEY_MAX / 8 + 1] = { 0 };
	if (ioctl(fd, EVIOCGKEY(sizeof(keys)), keys) < 0)
		return false;

	for (auto k : keys)
		if (k != 0)
			return true;

	return false;
}

// EVIOCGRAB is per fd: grabbing SDL's own fds hides the pads from ES and the emulator.
// A held button delays the grab, else hotkeygen never sees the hotkey released.
static bool grabJoysticks(std::set<int>& grabbed)
{
	std::set<std::string> paths;
	for (int i = 0; i < SDL_NumJoysticks(); i++)
	{
		const char* path = SDL_JoystickPathForIndex(i);
		if (path != nullptr)
			paths.insert(path);
	}

	DIR* dir = opendir("/proc/self/fd");
	if (dir == nullptr)
		return true;

	bool done = true;

	struct dirent* entry;
	while ((entry = readdir(dir)) != nullptr)
	{
		int fd = atoi(entry->d_name);
		if (fd <= 2 || grabbed.count(fd))
			continue;

		char target[256];
		ssize_t len = readlink((std::string("/proc/self/fd/") + entry->d_name).c_str(), target, sizeof(target) - 1);
		if (len <= 0)
			continue;

		target[len] = 0;
		if (paths.count(target) == 0)
			continue;

		if (anyKeyDown(fd))
		{
			done = false;
			continue;
		}

		if (ioctl(fd, EVIOCGRAB, (void*)1) == 0 || errno == EBUSY)
		{
			grabbed.insert(fd);
			LOG(LogInfo) << "ControlCenter: grabbed " << target;
		}
		else
			LOG(LogWarning) << "ControlCenter: could not grab " << target;
	}

	closedir(dir);
	return done;
}

static std::string findXml(const std::string& arg)
{
	return arg.empty() ? GuiControlCenter::defaultXmlPath() : arg;
}

static void ungrabJoysticks(std::set<int>& grabbed)
{
	for (int fd : grabbed)
		ioctl(fd, EVIOCGRAB, (void*)0);

	grabbed.clear();
}

static bool isGameRunning()
{
	std::string state = CcShell::capture("curl -s --max-time 1 http://127.0.0.1:1234/runningGame", 2000);
	return !state.empty() && state.find("NO GAME RUNNING") == std::string::npos;
}

static time_t fileTime(const std::string& path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0 ? st.st_mtime : 0;
}

// The es_settings.cfg entries the theme is built from
static std::string themeSettings()
{
	pugi::xml_document doc;
	if (!doc.load_file((Paths::getUserEmulationStationPath() + "/es_settings.cfg").c_str()))
		return "";

	pugi::xml_node root = doc.child("config") ? doc.child("config") : doc;

	std::string values;
	for (pugi::xml_node node : root.children())
	{
		std::string name = node.attribute("name").as_string();
		if (Utils::String::startsWith(name, "Theme") || Utils::String::startsWith(name, "subset."))
			values += name + "=" + node.attribute("value").as_string() + "\n";
	}
	return values;
}

static void restartShown(char* argv[])
{
	std::vector<char*> args;
	for (int i = 0; argv[i] != nullptr; i++)
		if (strcmp(argv[i], "--controlcenter-hidden") != 0)
			args.push_back(argv[i]);
	args.push_back(nullptr);

	LOG(LogInfo) << "ControlCenter: restarting on the selected screen";
	Log::flush();
	execv("/proc/self/exe", args.data());
}

// The connector for "primary" or "secondary", picked as emulationstation-standalone does
static std::string outputFor(const std::string& role)
{
	std::vector<std::string> outputs;
	for (auto& o : Utils::String::split(CcShell::capture("batocera-resolution listOutputs"), '\n'))
		if (!Utils::String::trim(o).empty())
			outputs.push_back(Utils::String::trim(o));

	auto valid = [&outputs](const std::string& o) { return std::find(outputs.cbegin(), outputs.cend(), o) != outputs.cend(); };

	std::string primary = Utils::String::trim(CcShell::capture("batocera-settings-get-master global.videooutput"));
	if (!valid(primary))
		primary = Utils::String::trim(CcShell::capture("batocera-resolution currentOutput"));
	if (!valid(primary) && !outputs.empty())
		primary = outputs[0];

	if (role == "primary")
		return primary;

	std::string secondary = Utils::String::trim(CcShell::capture("batocera-settings-get-master global.videooutput2"));
	if (valid(secondary) && secondary != primary)
		return secondary;

	for (auto& o : outputs)
		if (o != primary)
			return o;

	return "";
}

// --screen, then controlcenter.screen: primary, secondary (the default) or a connector
static int selectDisplay(const std::string& arg)
{
	int count = SDL_GetNumVideoDisplays();
	if (count <= 1)
		return 0;

	std::string want = arg;
	if (want.empty())
		want = Utils::String::trim(Utils::String::replace(SystemConf::getInstance()->get("controlcenter.screen"), "\"", ""));

	if (want.empty() || want == "primary" || want == "secondary")
		want = outputFor(want == "primary" ? "primary" : "secondary");

	if (!want.empty())
	{
		if (want.find_first_not_of("0123456789") == std::string::npos)
		{
			int idx = atoi(want.c_str());
			if (idx < count)
				return idx;
		}

		// SDL on Wayland names displays "make model (connector)"
		for (int i = 0; i < count; i++)
		{
			const char* name = SDL_GetDisplayName(i);
			if (name != nullptr && (want == name || Utils::String::endsWith(name, "(" + want + ")")))
				return i;
		}

		// X11 names displays after the monitor
		auto outputs = Utils::String::split(CcShell::capture("batocera-resolution listOutputs"), '\n');
		for (int i = 0; i < (int)outputs.size() && i < count; i++)
			if (Utils::String::trim(outputs[i]) == want)
				return i;

		LOG(LogWarning) << "ControlCenter: screen " << want << " not found, using default";
	}

	return 1;
}

static bool loadTheme()
{
	std::string path = ThemeData::getThemeFromCurrentSet("controlcenter");
	if (path.empty())
		return false;

	if (!Utils::FileSystem::exists(path))
		path = Utils::FileSystem::getParent(Utils::FileSystem::getParent(path)) + "/theme.xml";

	if (!Utils::FileSystem::exists(path))
		return false;

	std::map<std::string, std::string> sysData;
	sysData["screen.width"] = std::to_string(Renderer::getScreenWidth());
	sysData["screen.height"] = std::to_string(Renderer::getScreenHeight());
	sysData["screen.ratio"] = Renderer::getAspectRatio();
	sysData["screen.vertical"] = Renderer::isVerticalScreen() ? "true" : "false";
	sysData["global.help"] = "false";
	sysData["global.clock"] = "false";
	sysData["global.cheevos"] = "false";
	sysData["global.netplay"] = "false";

	sysData["system.name"] = "controlcenter";
	sysData["system.theme"] = "controlcenter";
	sysData["system.fullName"] = "";
	sysData["system.collection"] = "false";
	sysData["system.cheevos"] = "false";
	sysData["system.netplay"] = "false";
	sysData["system.savestates"] = "false";
	sysData["system.showSystemName"] = "false";
	sysData["system.sortedBy"] = "";

	// leaked: the menu theme points at it for the whole run
	ThemeData* theme = new ThemeData();
	try
	{
		theme->loadFile("controlcenter", sysData, path);
	}
	catch (std::exception& e)
	{
		LOG(LogError) << "ControlCenter: theme " << path << ": " << e.what();
		delete theme;
		return false;
	}

	Font::OnThemeChanged();
	ThemeData::setDefaultTheme(theme);
	return true;
}

int runControlCenter(int argc, char* argv[], const std::string& xmlArg, const std::string& screenArg, bool startHidden)
{
	std::string xmlPath = findXml(xmlArg);
	if (xmlPath.empty() || !Utils::FileSystem::exists(xmlPath))
	{
		LOG(LogError) << "ControlCenter: no controlcenter.xml found";
		return 1;
	}

	// for labwc window rules
	setenv("SDL_VIDEO_WAYLAND_WMCLASS", "es-controlcenter", 1);
	setenv("SDL_VIDEO_X11_WMCLASS", "es-controlcenter", 1);

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
	{
		LOG(LogError) << "ControlCenter: SDL video init failed: " << SDL_GetError();
		return 1;
	}

	int display = selectDisplay(screenArg);
	SDL_Rect bounds;
	if (SDL_GetDisplayBounds(display, &bounds) != 0)
	{
		display = 0;
		SDL_GetDisplayBounds(0, &bounds);
	}

	// controlcenter.window: auto, overlay or card
	std::string mode = SystemConf::getInstance()->get("controlcenter.window");
	const char* driver = SDL_GetCurrentVideoDriver();
	bool wayland = driver != nullptr && std::string(driver) == "wayland";
	bool transparent = mode == "overlay" || (mode != "card" && wayland);

	LOG(LogInfo) << "ControlCenter: display " << display << " (" << (SDL_GetDisplayName(display) ? SDL_GetDisplayName(display) : "?") << ") "
		<< bounds.w << "x" << bounds.h << "+" << bounds.x << "+" << bounds.y << (transparent ? " overlay" : " card");

	auto settings = Settings::getInstance();
	settings->setBool("OverlayWindow", true);
	settings->setBool("OverlayTransparent", transparent);
	settings->setBool("Windowed", false);
	settings->setBool("ShowHelpPrompts", true);
	settings->setBool("DrawClock", false);
	settings->setBool("ShowControllerActivity", false);
	settings->setString("ShowBattery", "");
	settings->setInt("ScreenSaverTime", 0);
	settings->setInt("ScreenWidth", 0);
	settings->setInt("ScreenHeight", 0);
	settings->setInt("ScreenOffsetX", 0);
	settings->setInt("ScreenOffsetY", 0);
	settings->setInt("MonitorID", -1);

	// labwc can keep focus on another output while the card holds the pads
	settings->setBool("BackgroundJoystickInput", true);

	if (transparent)
	{
		settings->setInt("WindowWidth", bounds.w);
		settings->setInt("WindowHeight", bounds.h);
		Renderer::setInitialWindowPosition(SDL_WINDOWPOS_UNDEFINED_DISPLAY(display), SDL_WINDOWPOS_UNDEFINED_DISPLAY(display));
	}
	else
	{
		int w = (int)std::min((float)bounds.h, bounds.w * 0.90f);
		int h = (int)(bounds.h * 0.82f);

		settings->setInt("WindowWidth", w);
		settings->setInt("WindowHeight", h);
		Renderer::setInitialWindowPosition(bounds.x + (bounds.w - w) / 2, bounds.y);
	}

	signal(SIGUSR1, onToggleSignal);
	signal(SIGTERM, onQuitSignal);
	signal(SIGINT, onQuitSignal);

	unsigned int startTime = SDL_GetTicks();

	Window window;
	if (!window.init(true, false))
	{
		LOG(LogError) << "ControlCenter: window failed to initialize";
		return 1;
	}

	// a/b must be settled before the input configs load
	InputConfig::AssignActionButtons();
	InputManager::getInstance()->init();

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::PDFEXTRACTION))
		TextureData::PdfHandler = ApiSystem::getInstance();

	SDL_Window* sdlWindow = Renderer::getSDLWindow();
	auto showWindow = [sdlWindow, transparent, display]()
	{
		SDL_ShowWindow(sdlWindow);
		if (transparent)
		{
			SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED_DISPLAY(display), SDL_WINDOWPOS_CENTERED_DISPLAY(display));
			SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
		}
		SDL_PumpEvents();
	};

	if (startHidden)
		SDL_HideWindow(sdlWindow);
	else
		showWindow();

	if (!loadTheme())
	{
		LOG(LogWarning) << "ControlCenter: no theme, using built-in menu colors";
	}

	bool rescreen = false;
	GuiControlCenter* gui = nullptr;
	time_t xmlTime = fileTime(xmlPath);
	std::string themeValues = themeSettings();

	auto fitGui = [&gui, &bounds, transparent]()
	{
		float w = (float)Renderer::getScreenWidth();
		float h = (float)Renderer::getScreenHeight();
		if (transparent)
			gui->setSize(std::min(w, (float)bounds.w), std::min(h, (float)bounds.h));
		else
			gui->setSize(w, h);
	};

	auto createGui = [&]()
	{
		gui = new GuiControlCenter(&window, xmlPath, (float)bounds.h, transparent);
		gui->setSavesHistory(true);
		fitGui();
		gui->setOnRescreen([&rescreen, &gui]() { rescreen = true; gui->close(); });
		window.pushGui(gui);
	};

	createGui();
	if (!startHidden)
		gui->open();

	LOG(LogInfo) << "ControlCenter: ready in " << (SDL_GetTicks() - startTime) << "ms" << (startHidden ? ", hidden" : "");

	const int grabDelay = 100;
	unsigned int grabTime = startHidden ? 0 : SDL_GetTicks() + grabDelay;
	std::set<int> grabbed;

	bool hidden = startHidden;
	bool pausedGame = false;
	unsigned int lastToggle = 0;
	unsigned int lastRender = 0;

	auto show = [&]()
	{
		// a new theme needs fonts and textures loaded from scratch
		if (selectDisplay(screenArg) != display || themeSettings() != themeValues)
			restartShown(argv);

		if (fileTime(xmlPath) != xmlTime)
		{
			xmlTime = fileTime(xmlPath);
			delete gui;
			createGui();
		}

		showWindow();
		gui->open();
		hidden = false;
		grabTime = SDL_GetTicks() + grabDelay;

		// batocera.conf may have changed since this process started
		if (CcShell::capture("batocera-settings-get controlcenter.pause", 2000) == "1" && isGameRunning())
		{
			// a toggle, sent again on hide
			CcShell::capture("hotkeygen --send pause", 2000);
			pausedGame = true;
		}
	};

	auto hide = [&]()
	{
		ungrabJoysticks(grabbed);
		grabTime = 0;
		SDL_HideWindow(sdlWindow);
		hidden = true;

		// keys sent now would reach this window, so wait for the game to get the focus back
		std::string cmds = gui->takeCloseActions();
		if (pausedGame)
		{
			cmds = cmds.empty() ? "hotkeygen --send pause" : "hotkeygen --send pause; " + cmds;
			pausedGame = false;
		}
		if (!cmds.empty())
			std::thread([cmds]() { CcShell::capture("sleep 0.3; " + cmds, 60000); }).detach();
	};

	int lastTime = SDL_GetTicks();
	while (!sQuitRequested)
	{
		SDL_Event event;
		if (SDL_WaitEventTimeout(&event, hidden ? 250 : 16))
		{
			do
			{
				if (event.type == SDL_QUIT)
					sQuitRequested = true;
				else if (event.type == SDL_JOYDEVICEADDED || event.type == SDL_JOYDEVICEREMOVED)
				{
					InputManager::getInstance()->parseEvent(event, &window);
					// fd numbers get reused
					grabbed.clear();
					if (!hidden)
						grabTime = SDL_GetTicks() + grabDelay;
				}
				else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
				{
					// a window opened over the card: the pads and hotkeys go back to it
					ungrabJoysticks(grabbed);
					grabTime = 0;
				}
				else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
				{
					if (!hidden && grabbed.empty())
						grabTime = SDL_GetTicks() + grabDelay;
				}
				else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
				{
					// labwc can size the surface for another output: trust the chosen output's bounds
					LOG(LogDebug) << "ControlCenter: window size " << event.window.data1 << "x" << event.window.data2;
					if (Renderer::onScreenSizeChanged(event.window.data1, event.window.data2))
						fitGui();
				}
				else if (!hidden)
					InputManager::getInstance()->parseEvent(event, &window);
			}
			while (SDL_PollEvent(&event));
		}

		// one press can arrive twice, through hotkeygen and ES
		if (sToggleRequests > 0)
		{
			sToggleRequests = 0;
			unsigned int now = SDL_GetTicks();
			if (lastToggle == 0 || now - lastToggle > 1000)
			{
				lastToggle = now;
				if (hidden)
					show();
				else if (!(SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_INPUT_FOCUS))
				{
					// covered by a game started after it: map it again on top
					SDL_HideWindow(sdlWindow);
					showWindow();
					SDL_RaiseWindow(sdlWindow);
					grabTime = SDL_GetTicks() + grabDelay;
					lastRender = 0;
				}
				else
					gui->close();
			}
		}

		if (!hidden && gui->isClosed())
		{
			hide();
			if (rescreen)
				restartShown(argv);
		}

		int curTime = SDL_GetTicks();
		int deltaTime = std::max(0, curTime - lastTime);
		lastTime = curTime;

		// the monitor history keeps sampling while hidden
		if (hidden)
		{
			window.update(deltaTime);
			continue;
		}

		if (grabTime != 0 && SDL_GetTicks() >= grabTime)
			grabTime = grabJoysticks(grabbed) ? 0 : SDL_GetTicks() + 30;

		window.update(deltaTime);

		// a dialog on top draws itself, the periodic frame catches images loading
		if (gui->consumeDirty() || window.peekGui() != gui || curTime - lastRender >= 500)
		{
			window.render();
			Renderer::swapBuffers();
			lastRender = curTime;
		}
	}

	ungrabJoysticks(grabbed);
	if (pausedGame)
		CcShell::capture("hotkeygen --send pause", 2000);

	while (window.peekGui() != nullptr)
		delete window.peekGui();

	window.deinit();
	return 0;
}

static GuiControlCenter* sInProcess = nullptr;
static unsigned int sInProcessToggle = 0;

bool canShowControlCenterInProcess()
{
	return SDL_GetNumVideoDisplays() <= 1;
}

bool toggleControlCenterInProcess(Window* window)
{
	if (!canShowControlCenterInProcess())
		return false;

	unsigned int now = SDL_GetTicks();
	if (sInProcessToggle != 0 && now - sInProcessToggle < 1000)
		return true;
	sInProcessToggle = now;

	if (sInProcess != nullptr)
	{
		sInProcess->close();
		return true;
	}

	std::string xmlPath = GuiControlCenter::defaultXmlPath();
	if (xmlPath.empty())
		return false;

	// the card animates by itself, ES must keep drawing
	PowerSaver::pause();

	sInProcess = new GuiControlCenter(window, xmlPath, (float)Renderer::getScreenHeight(), true);
	sInProcess->setOnClosed([window]()
	{
		// not from inside its own update
		window->postToUiThread([]()
		{
			std::string cmds = sInProcess->takeCloseActions();
			if (!cmds.empty())
				std::thread([cmds]() { CcShell::capture(cmds, 60000); }).detach();

			delete sInProcess;
			sInProcess = nullptr;
			PowerSaver::resume();
		});
	});
	window->pushGui(sInProcess);
	sInProcess->open();
	return true;
}
