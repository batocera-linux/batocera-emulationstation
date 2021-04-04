#include "InputManager.h"

#include "utils/FileSystemUtil.h"
#include "CECInput.h"
#include "Log.h"
#include "platform.h"
#include "Scripting.h"
#include "Window.h"
#include <pugixml/src/pugixml.hpp>
#include <SDL.h>
#include <iostream>
#include <assert.h>
#include "Settings.h"
#include <algorithm>
#include <mutex>
#include "utils/StringUtil.h"
#include "LocaleES.h"

#define KEYBOARD_GUID_STRING "-1"
#define CEC_GUID_STRING      "-2"

// SO HEY POTENTIAL POOR SAP WHO IS TRYING TO MAKE SENSE OF ALL THIS (by which I mean my future self)
// There are like four distinct IDs used for joysticks (crazy, right?)
// 1. Device index - this is the "lowest level" identifier, and is just the Nth joystick plugged in to the system (like /dev/js#).
//    It can change even if the device is the same, and is only used to open joysticks (required to receive SDL events).
// 2. SDL_JoystickID - this is an ID for each joystick that is supposed to remain consistent between plugging and unplugging.
//    ES doesn't care if it does, though.
// 3. "Device ID" - this is something I made up and is what InputConfig's getDeviceID() returns.  
//    This is actually just an SDL_JoystickID (also called instance ID), but -1 means "keyboard" instead of "error."
// 4. Joystick GUID - this is some squashed version of joystick vendor, version, and a bunch of other device-specific things.
//    It should remain the same across runs of the program/system restarts/device reordering and is what I use to identify which joystick to load.

// hack for cec support
int SDL_USER_CECBUTTONDOWN = -1;
int SDL_USER_CECBUTTONUP   = -1;

static std::mutex mJoysticksLock;

InputManager* InputManager::mInstance = NULL;
Delegate<IJoystickChangedEvent> InputManager::joystickChanged;

InputManager::InputManager() : mKeyboardInputConfig(NULL)
{
}

InputManager::~InputManager()
{
	deinit();
}

InputManager* InputManager::getInstance()
{
	if(!mInstance)
		mInstance = new InputManager();

	return mInstance;
}

void InputManager::init()
{
	if (initialized())
		deinit();

	rebuildAllJoysticks(false);

	mKeyboardInputConfig = new InputConfig(DEVICE_KEYBOARD, -1, "Keyboard", KEYBOARD_GUID_STRING, 0, 0, 0); // batocera
	loadInputConfig(mKeyboardInputConfig);

	SDL_USER_CECBUTTONDOWN = SDL_RegisterEvents(2);
	SDL_USER_CECBUTTONUP   = SDL_USER_CECBUTTONDOWN + 1;
	CECInput::init();
	mCECInputConfig = new InputConfig(DEVICE_CEC, -1, "CEC", CEC_GUID_STRING, 0, 0, 0); // batocera
	loadInputConfig(mCECInputConfig);
}

void InputManager::deinit()
{
	if(!initialized())
		return;

	clearJoysticks();

	if(mKeyboardInputConfig != NULL)
	{
		delete mKeyboardInputConfig;
		mKeyboardInputConfig = NULL;
	}

	if(mCECInputConfig != NULL)
	{
		delete mCECInputConfig;
		mCECInputConfig = NULL;
	}

	CECInput::deinit();

	SDL_JoystickEventState(SDL_DISABLE);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

int InputManager::getNumJoysticks() 
{ 
	std::unique_lock<std::mutex> lock(mJoysticksLock);
	return (int)mJoysticks.size(); 
}

InputConfig* InputManager::getInputConfigByDevice(int device)
{
	if(device == DEVICE_KEYBOARD)
		return mKeyboardInputConfig;
	else if(device == DEVICE_CEC)
		return mCECInputConfig;
	else
		return mInputConfigs[device];
}

void InputManager::clearJoysticks()
{
	mJoysticksLock.lock();

	for (auto iter = mJoysticks.begin(); iter != mJoysticks.end(); iter++)
		SDL_JoystickClose(iter->second);

	mJoysticks.clear();

	for (auto iter = mInputConfigs.begin(); iter != mInputConfigs.end(); iter++)
		delete iter->second;

	mInputConfigs.clear();

	for (auto iter = mPrevAxisValues.begin(); iter != mPrevAxisValues.end(); iter++)
		delete[] iter->second;

	mPrevAxisValues.clear();

	mJoysticksLock.unlock();
}

void InputManager::rebuildAllJoysticks(bool deinit)
{
	SDL_JoystickEventState(SDL_DISABLE);

	clearJoysticks();

	if (deinit)
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK);

	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, Settings::getInstance()->getBool("BackgroundJoystickInput") ? "1" : "0");
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);	

	mJoysticksLock.lock();

	int numJoysticks = SDL_NumJoysticks();
	for (int idx = 0; idx < numJoysticks; idx++)
	{
		// open joystick & add to our list
		SDL_Joystick* joy = SDL_JoystickOpen(idx);
		if (joy == nullptr)
			return;

		// add it to our list so we can close it again later
		SDL_JoystickID joyId = SDL_JoystickInstanceID(joy);

		mJoysticks[joyId] = joy;

		char guid[40];
		SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joy), guid, 40);

		// create the InputConfig
		mInputConfigs[joyId] = new InputConfig(joyId, idx, SDL_JoystickName(joy), guid, SDL_JoystickNumButtons(joy), SDL_JoystickNumHats(joy), SDL_JoystickNumAxes(joy)); // batocera
		if (!loadInputConfig(mInputConfigs[joyId]))
			LOG(LogInfo) << "Added unconfigured joystick " << SDL_JoystickName(joy) << " (GUID: " << guid << ", instance ID: " << joyId << ", device index: " << idx << ").";
		else
			LOG(LogInfo) << "Added known joystick " << SDL_JoystickName(joy) << " (instance ID: " << joyId << ", device index: " << idx << ")";

		// set up the prevAxisValues
		int numAxes = SDL_JoystickNumAxes(joy);
		mPrevAxisValues[joyId] = new int[numAxes];
		std::fill(mPrevAxisValues[joyId], mPrevAxisValues[joyId] + numAxes, 0); //initialize array to 0
	}

	mJoysticksLock.unlock();

	computeLastKnownPlayersDeviceIndexes();

	joystickChanged.invoke([](IJoystickChangedEvent* c) { c->onJoystickChanged(); });

	SDL_JoystickEventState(SDL_ENABLE);
}

bool InputManager::parseEvent(const SDL_Event& ev, Window* window)
{
	bool causedEvent = false;
	switch (ev.type)
	{
	case SDL_JOYAXISMOTION:
	{
		// batocera
	// some axes are "full" : from -32000 to +32000
	// in this case, their unpressed state is not 0
	// SDL provides a function to get this value
	// in es, the trick is to minus this value to the value to do as if it started at 0
		int initialValue = 0;
		Sint16 x;

#if SDL_VERSION_ATLEAST(2, 0, 9)
		// SDL_JoystickGetAxisInitialState doesn't work with 8bitdo start+b
		// required for several pads like xbox and 8bitdo

		auto inputConfig = mInputConfigs.find(ev.jaxis.which);
		if (inputConfig != mInputConfigs.cend())
		{
			std::string guid = std::to_string(ev.jaxis.axis) + "@" + inputConfig->second->getDeviceGUIDString();

			auto it = mJoysticksInitialValues.find(guid);
			if (it != mJoysticksInitialValues.cend())
				initialValue = it->second;
			else if (SDL_JoystickGetAxisInitialState(mJoysticks[ev.jaxis.which], ev.jaxis.axis, &x))
			{
				mJoysticksInitialValues[guid] = x;
				initialValue = x;
			}
		}
#endif

		//if it switched boundaries
		if ((abs(ev.jaxis.value - initialValue) > DEADZONE) != (abs(mPrevAxisValues[ev.jaxis.which][ev.jaxis.axis]) > DEADZONE)) // batocera
		{
			int normValue;
			if (abs(ev.jaxis.value - initialValue) <= DEADZONE) // batocera
				normValue = 0;
			else
				if (ev.jaxis.value - initialValue > 0) // batocera
					normValue = 1;
				else
					normValue = -1;

			window->input(getInputConfigByDevice(ev.jaxis.which), Input(ev.jaxis.which, TYPE_AXIS, ev.jaxis.axis, normValue, false));
			causedEvent = true;
		}

		mPrevAxisValues[ev.jaxis.which][ev.jaxis.axis] = ev.jaxis.value - initialValue; // batocera
		return causedEvent;
	}
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		window->input(getInputConfigByDevice(ev.jbutton.which), Input(ev.jbutton.which, TYPE_BUTTON, ev.jbutton.button, ev.jbutton.state == SDL_PRESSED, false));
		return true;

	case SDL_JOYHATMOTION:
		window->input(getInputConfigByDevice(ev.jhat.which), Input(ev.jhat.which, TYPE_HAT, ev.jhat.hat, ev.jhat.value, false));
		return true;

	case SDL_KEYDOWN:
		if (ev.key.keysym.sym == SDLK_BACKSPACE && SDL_IsTextInputActive())
			window->textInput("\b");

		if (ev.key.repeat)
			return false;

#ifdef _ENABLEEMUELEC
		/* use the POWER KEY to turn off EmuELEC, specially useful for GTKING-PRO and Odroid Go Advance*/
        if(ev.key.keysym.sym == SDLK_POWER) {
			Scripting::fireEvent("quit", "shutdown");
			quitES(QuitMode::SHUTDOWN);
			/*LOG(LogError) << "no quit?";*/
			return false;
		}
#endif
		window->input(getInputConfigByDevice(DEVICE_KEYBOARD), Input(DEVICE_KEYBOARD, TYPE_KEY, ev.key.keysym.sym, 1, false));
		return true;

	case SDL_KEYUP:
		window->input(getInputConfigByDevice(DEVICE_KEYBOARD), Input(DEVICE_KEYBOARD, TYPE_KEY, ev.key.keysym.sym, 0, false));
		return true;

	case SDL_TEXTINPUT:
		window->textInput(ev.text.text);
		break;

	case SDL_JOYDEVICEADDED:
		{
			std::string addedDeviceName;
			auto id = SDL_JoystickGetDeviceInstanceID(ev.jdevice.which);
			auto it = std::find_if(mInputConfigs.cbegin(), mInputConfigs.cend(), [id](const std::pair<SDL_JoystickID, InputConfig*> & t) { return t.second != nullptr && t.second->getDeviceId() == id; });
			if (it == mInputConfigs.cend())
				addedDeviceName = SDL_JoystickNameForIndex(ev.jdevice.which);
			
			rebuildAllJoysticks();

			if (!addedDeviceName.empty())
				window->displayNotificationMessage(_U("\uF11B ") + Utils::String::format(_("%s connected").c_str(), Utils::String::trim(addedDeviceName).c_str()));
		}
		return true;

	case SDL_JOYDEVICEREMOVED:
		{
			auto it = mInputConfigs.find(ev.jdevice.which);
			if (it != mInputConfigs.cend() && it->second != nullptr)
				window->displayNotificationMessage(_U("\uF11B ") + Utils::String::format(_("%s disconnected").c_str(), Utils::String::trim(it->second->getDeviceName()).c_str()));
	
			rebuildAllJoysticks();
		}
		return false;
	}

	if((ev.type == (unsigned int)SDL_USER_CECBUTTONDOWN) || (ev.type == (unsigned int)SDL_USER_CECBUTTONUP))
	{
		window->input(getInputConfigByDevice(DEVICE_CEC), Input(DEVICE_CEC, TYPE_CEC_BUTTON, ev.user.code, ev.type == (unsigned int)SDL_USER_CECBUTTONDOWN, false));
		return true;
	}

	return false;
}

bool InputManager::tryLoadInputConfig(std::string path, InputConfig* config, bool allowApproximate)
{
	pugi::xml_node configNode(NULL);

	if (!Utils::FileSystem::exists(path))
		return false;

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(path.c_str());

	if (!res)
	{
		LOG(LogError) << "Error parsing input config: " << res.description();
		return false;
	}

	pugi::xml_node root = doc.child("inputList");
	if (!root)
		return false;

	// batocera
	// looking for a device having the same guid and name, or if not, one with the same guid or in last chance, one with the same name


	bool found_guid = false;
	bool found_exact = false;
	for (pugi::xml_node item = root.child("inputConfig"); item; item = item.next_sibling("inputConfig")) {
		// check the guid
		if (strcmp(config->getDeviceGUIDString().c_str(), item.attribute("deviceGUID").value()) == 0) {
			// found a correct guid
			found_guid = true; // no more need to check the name only
			configNode = item;
#if WIN32
			found_exact = true;
			break;
#else
			if (strcmp(config->getDeviceName().c_str(), item.attribute("deviceName").value()) == 0) {
				// found the exact device
				found_exact = true;
				configNode = item;
				break;
			}
#endif
		}

#if !WIN32
		// check for a name if no guid is found
		if (found_guid == false) {
			if (strcmp(config->getDeviceName().c_str(), item.attribute("deviceName").value()) == 0) {
				configNode = item;
			}
		}
#endif
	}

	if (!configNode)
		return false;

	// batocera
	if (found_exact == false)
	{
		LOG(LogInfo) << "Approximative device found using guid=" << configNode.attribute("deviceGUID").value() << " name=" << configNode.attribute("deviceName").value() << ")";

		if (!allowApproximate)
			return false;
	}

	config->loadFromXML(configNode);
	return true;
}

bool InputManager::loadInputConfig(InputConfig* config)
{
	std::string path = getConfigPath();

#if WIN32
	// Find exact device
	if (tryLoadInputConfig(path, config, false))
		return true;
#else
	// Find exact device
	if (tryLoadInputConfig(path, config, false))
		return true;

	// Find system exact device
	std::string sharedPath = Utils::FileSystem::getSharedConfigPath() + "/es_input.cfg";
	if (tryLoadInputConfig(sharedPath, config, false))
		return true;

	// Find user Approximative device
	if (tryLoadInputConfig(path, config, true))
		return true;

	// Find system Approximative device
	if (tryLoadInputConfig(sharedPath, config, true))
		return true;
#endif

	return false;
}

//used in an "emergency" where no keyboard config could be loaded from the inputmanager config file
//allows the user to select to reconfigure in menus if this happens without having to delete es_input.cfg manually
void InputManager::loadDefaultKBConfig()
{
	InputConfig* cfg = getInputConfigByDevice(DEVICE_KEYBOARD);

	cfg->clear();
	cfg->mapInput("up", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_UP, 1, true));
	cfg->mapInput("down", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_DOWN, 1, true));
	cfg->mapInput("left", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_LEFT, 1, true));
	cfg->mapInput("right", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RIGHT, 1, true));

	cfg->mapInput(BUTTON_OK, Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RETURN, 1, true));
	cfg->mapInput(BUTTON_BACK, Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_ESCAPE, 1, true));
	cfg->mapInput("start", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_F1, 1, true));
	cfg->mapInput("select", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_F2, 1, true));
#ifdef _ENABLEEMUELEC
	cfg->mapInput("lefttrigger", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RIGHTBRACKET, 1, true));
	cfg->mapInput("righttrigger", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_LEFTBRACKET, 1, true));
#else
	cfg->mapInput("pageup", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RIGHTBRACKET, 1, true));
	cfg->mapInput("pagedown", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_LEFTBRACKET, 1, true));
#endif
}

void InputManager::writeDeviceConfig(InputConfig* config)
{
	assert(initialized());

	std::string path = getConfigPath();

	pugi::xml_document doc;

	if (Utils::FileSystem::exists(path))
	{
		// merge files
		pugi::xml_parse_result result = doc.load_file(path.c_str());
		if (!result)
		{
			LOG(LogError) << "Error parsing input config: " << result.description();
		}
		else
		{
			// successfully loaded, delete the old entry if it exists
			pugi::xml_node root = doc.child("inputList");
			if (root)
			{
				// batocera
				pugi::xml_node oldEntry(NULL);
				for (pugi::xml_node item = root.child("inputConfig"); item; item = item.next_sibling("inputConfig")) 
				{
					if (strcmp(config->getDeviceGUIDString().c_str(), item.attribute("deviceGUID").value()) == 0
#if !WIN32
						&& strcmp(config->getDeviceName().c_str(), item.attribute("deviceName").value()) == 0
#endif
						) 
					{
						oldEntry = item;
						break;
					}
				}
				if (oldEntry)
					root.remove_child(oldEntry);
			}
		}
	}

	pugi::xml_node root = doc.child("inputList");
	if(!root)
		root = doc.append_child("inputList");

	config->writeToXML(root);
	doc.save_file(path.c_str());

        // batocera
	/* create a es_last_input.cfg so that people can easily share their config */
	pugi::xml_document lastdoc;
	pugi::xml_node lastroot = lastdoc.append_child("inputList");
	config->writeToXML(lastroot);
	std::string lastpath = getTemporaryConfigPath();
	lastdoc.save_file(lastpath.c_str());

	Scripting::fireEvent("config-changed");
	Scripting::fireEvent("controls-changed");
	
	// execute any onFinish commands and re-load the config for changes
	doOnFinish();
	loadInputConfig(config);
}

void InputManager::doOnFinish()
{
	if (!initialized())
		return;

	std::string path = getConfigPath();
	pugi::xml_document doc;

	if(Utils::FileSystem::exists(path))
	{
		pugi::xml_parse_result result = doc.load_file(path.c_str());
		if(!result)
		{
			LOG(LogError) << "Error parsing input config: " << result.description();
		}
		else
		{
			pugi::xml_node root = doc.child("inputList");
			if(root)
			{
				root = root.find_child_by_attribute("inputAction", "type", "onfinish");
				if(root)
				{
					for(pugi::xml_node command = root.child("command"); command;
							command = command.next_sibling("command"))
					{
						std::string tocall = command.text().get();

						LOG(LogInfo) << "	" << tocall;
						std::cout << "==============================================\ninput config finish command:\n";
						int exitCode = runSystemCommand(tocall, "", nullptr);
						std::cout << "==============================================\n";

						if(exitCode != 0)
						{
							LOG(LogWarning) << "...launch terminated with nonzero exit code " << exitCode << "!";
						}
					}
				}
			}
		}
	}
}

std::string InputManager::getConfigPath()
{
	return Utils::FileSystem::getEsConfigPath() + "/es_input.cfg";
}

std::string InputManager::getTemporaryConfigPath()
{
#ifdef _ENABLEEMUELEC
	return Utils::FileSystem::getEsConfigPath() + "/es_temporaryinput.cfg";
#else
	return Utils::FileSystem::getEsConfigPath() + "/es_last_input.cfg";
#endif
}

bool InputManager::initialized() const
{
	return mKeyboardInputConfig != NULL;
}

int InputManager::getNumConfiguredDevices()
{
	int num = 0;
	for(auto it = mInputConfigs.cbegin(); it != mInputConfigs.cend(); it++)
	{
		if(it->second->isConfigured())
			num++;
	}

	if(mKeyboardInputConfig->isConfigured())
		num++;

	if(mCECInputConfig->isConfigured())
		num++;

	return num;
}

void InputManager::computeLastKnownPlayersDeviceIndexes() 
{
	std::map<int, InputConfig*> playerJoysticks = computePlayersConfigs();

	m_lastKnownPlayersDeviceIndexes.clear();
	for (int player = 0; player < MAX_PLAYERS; player++)
	{
		auto playerJoystick = playerJoysticks[player];
		if (playerJoystick != nullptr)
		{
			PlayerDeviceInfo dev;
			dev.index = playerJoystick->getDeviceIndex();
			dev.batteryLevel = playerJoystick->getBatteryLevel();

			m_lastKnownPlayersDeviceIndexes[player] = dev;
		}
	}
}

std::map<int, InputConfig*> InputManager::computePlayersConfigs()
{
	std::unique_lock<std::mutex> lock(mJoysticksLock);

	// 1 recuperer les configurated
	std::vector<InputConfig *> availableConfigured;

	for (auto iter = mJoysticks.begin(); iter != mJoysticks.end(); iter++) 
	{
		InputConfig * config = getInputConfigByDevice(iter->first);
		if (config != NULL && config->isConfigured()) 
			availableConfigured.push_back(config);		
	}

	// sort available configs
	std::sort(availableConfigured.begin(), availableConfigured.end(), [](InputConfig * a, InputConfig * b) -> bool { return a->getDeviceIndex() < b->getDeviceIndex(); });

	//2 pour chaque joueur verifier si il y a un configurated
	// associer le input au joueur
	// enlever des disponibles
	std::map<int, InputConfig*> playerJoysticks;

	// First loop, search for GUID + NAME. High Priority
	for (int player = 0; player < MAX_PLAYERS; player++) 
	{
		std::stringstream sstm;
		sstm << "INPUT P" << player + 1;
		std::string confName = sstm.str() + "NAME";
		std::string confGuid = sstm.str() + "GUID";

		std::string playerConfigName = Settings::getInstance()->getString(confName);
		std::string playerConfigGuid = Settings::getInstance()->getString(confGuid);

		for (auto it1 = availableConfigured.begin(); it1 != availableConfigured.end(); ++it1)
		{
			InputConfig * config = *it1;
			bool nameFound = playerConfigName.compare(config->getDeviceName()) == 0;
			bool guidfound = playerConfigGuid.compare(config->getDeviceGUIDString()) == 0;

			if (nameFound && guidfound) {
				availableConfigured.erase(it1);
				playerJoysticks[player] = config;
				break;
			}
		}
	}
	// Second loop, search for NAME. Low Priority
	for (int player = 0; player < MAX_PLAYERS; player++) 
	{
		std::stringstream sstm;
		sstm << "INPUT P" << player + 1;
		std::string confName = sstm.str() + "NAME";

		std::string playerConfigName = Settings::getInstance()->getString(confName);

		for (auto it1 = availableConfigured.begin(); it1 != availableConfigured.end(); ++it1)
		{
			InputConfig * config = *it1;
			bool nameFound = playerConfigName.compare(config->getDeviceName()) == 0;
			if (nameFound) 
			{
				availableConfigured.erase(it1);
				playerJoysticks[player] = config;
				break;
			}
		}
	}

	// Last loop, search for free controllers for remaining players.
	for (int player = 0; player < MAX_PLAYERS; player++) 
	{
		// si aucune config a été trouvé pour le joueur, on essaie de lui filer un libre
		if (playerJoysticks[player] == NULL) 
		{
			for (auto it1 = availableConfigured.begin(); it1 != availableConfigured.end(); ++it1)
			{
				playerJoysticks[player] = *it1;
				availableConfigured.erase(it1);
				break;
			}
		}
	}

	// in case of hole (player 1 missing, but player 4 set, fill the holes with last players joysticks)
	for (int player = 0; player < MAX_PLAYERS; player++) 
	{
		if (playerJoysticks[player] == NULL) 
		{
			for (int repplayer = MAX_PLAYERS; repplayer > player; repplayer--) 
			{
				if (playerJoysticks[player] == NULL && playerJoysticks[repplayer] != NULL) 
				{
					playerJoysticks[player] = playerJoysticks[repplayer];
					playerJoysticks[repplayer] = NULL;
				}
			}
		}
	}

	return playerJoysticks;
}

std::string InputManager::configureEmulators() {
  std::map<int, InputConfig*> playerJoysticks = computePlayersConfigs();
  std::stringstream command;

  for (int player = 0; player < MAX_PLAYERS; player++) {
    InputConfig * playerInputConfig = playerJoysticks[player];
    if(playerInputConfig != NULL){
#ifdef _ENABLEEMUELEC
      command << "-p" << player+1 << "index "      <<  playerInputConfig->getDeviceIndex();
      command << " -p" << player+1 << "guid "       << playerInputConfig->getDeviceGUIDString();
      command << " ";
#else
      command <<  "-p" << player+1 << "index "      <<  playerInputConfig->getDeviceIndex();
      command << " -p" << player+1 << "guid "       << playerInputConfig->getDeviceGUIDString();
      command << " -p" << player+1 << "name \""     <<  playerInputConfig->getDeviceName() << "\"";
      command << " -p" << player+1 << "nbbuttons "  << playerInputConfig->getDeviceNbButtons();
      command << " -p" << player+1 << "nbhats "     << playerInputConfig->getDeviceNbHats();
      command << " -p" << player+1 << "nbaxes "     << playerInputConfig->getDeviceNbAxes();
      command << " ";
#endif
    }
  }
  LOG(LogInfo) << "Configure emulators command : " << command.str().c_str();
  return command.str();
}

void InputManager::updateBatteryLevel(int id, std::string device, int level)
{
	bool changed = false;

	mJoysticksLock.lock();

	for (auto joy : mJoysticks) 
	{
		InputConfig* config = getInputConfigByDevice(joy.first);
		if (config != NULL && config->isConfigured())
		{
			if (Utils::String::compareIgnoreCase(config->getDeviceGUIDString(), device) == 0)
			{
				config->updateBatteryLevel(level);
				changed = true;
			}
		}
	}

	mJoysticksLock.unlock();

	if (changed)
		computeLastKnownPlayersDeviceIndexes();
}

std::vector<InputConfig*> InputManager::getInputConfigs()
{
	std::vector<InputConfig*> ret;

	std::map<int, InputConfig*> playerJoysticks = computePlayersConfigs();

	for (int player = 0; player < MAX_PLAYERS; player++)
	{
		auto playerJoystick = playerJoysticks[player];
		if (playerJoystick != nullptr && playerJoystick->isConfigured())
			ret.push_back(playerJoystick);
	}

	return ret;
}