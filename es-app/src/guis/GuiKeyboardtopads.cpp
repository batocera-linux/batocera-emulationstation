#include "guis/GuiKeyboardtopads.h"
#include "components/OptionListComponent.h"
#include "guis/GuiLoading.h"

struct KeyboardtopadsInputStructure
{
	std::string code;
	std::string dispName;
	std::string icon;
};

#ifdef BATOCERA
struct hotkeyTargetDefinition
{
	std::string code;
	std::string name;
};
#endif

GuiKeyboardtopads::GuiKeyboardtopads(Window* window, Keyboardtopad ktp)
	: GuiSettings(window, _("KEYBOARDTOPADS"), true)
{
  	m_ktp = ktp;
	m_devices = ApiSystem::getInstance()->getKeyboardtopadDevices(m_ktp.config);
	m_need_save = false;

	// force 4 devices minimum and 1 hotkey
	int n_joystick = countJoystickDevice("joystick");
	for(unsigned int i=n_joystick; i<4; i++) {
	  KeyboardtopadDevice dev;
	  dev.type = "joystick";
	  m_devices.push_back(dev);
	}
	int n_hotkey = countJoystickDevice("hotkeys");
	if(n_hotkey == 0) {
	  KeyboardtopadDevice dev;
	  dev.type = "hotkeys";
	  m_devices.push_back(dev);
	}
	//

	addTab(_("PLAYER 1"));
	addTab(_("PLAYER 2"));
	addTab(_("PLAYER 3"));
	addTab(_("PLAYER 4"));
	addTab(_("HOTKEYS"));
	setOnTabIndexChanged([this, ktp] { loadActivePage(ktp.device_path); });

	loadActivePage(m_ktp.device_path);

	if (Renderer::ScreenSettings::fullScreenMenus())
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
	else
		mMenu.setPosition((mSize.x() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.15f);

	onFinalize([this, window] {  if(this->m_need_save) {
	      for(unsigned int d=0; d<this->m_devices.size(); d++) {
		if(this->m_devices[d].type == "joystick" && this->m_devices[d].name == "" && this->m_devices[d].keys.size() > 0) {
		  window->displayNotificationMessage(_U("\uF013  ") + _("KEYS DEFINED BUT JOYSTICK NOT NAMED"));
		}

		if (this->m_devices[d].type == "joystick" && this->m_devices[d].name != "" && this->m_devices[d].keys.size() == 0) {
		  window->displayNotificationMessage(_U("\uF013  ") + _("JOYSTICK NAMED BUT NO KEYS DEFINED"));
		}
	      }
	      ApiSystem::getInstance()->saveKeyboardtopads(this->m_ktp, this->m_devices); }
	});
}

void GuiKeyboardtopads::loadActivePage(const std::string& device_path)
{
	mMenu.clear();
	mMenu.clearButtons();

	switch (this->getTabIndex())
	{
	case 0:
	case 1:
	case 2:
	case 3:
	  loadPlayerPage(this->getTabIndex()+1, device_path);
	  break;
	case 4:
	  loadHotkeysPage(device_path);
	  break;
	}

	mMenu.addButton(_("BACK"), _("go back"), [this] { close(); });
}

KeyboardtopadDevice* GuiKeyboardtopads::getJoystickDevice(int n) {
  KeyboardtopadDevice* res;

  unsigned int x = 0;
  for(unsigned int i=0; i<m_devices.size(); i++) {
    if(m_devices[i].type == "joystick") {
      if(x == n) return &(m_devices[i]);
      x++;
    }
  }
  return NULL;
}

KeyboardtopadDevice* GuiKeyboardtopads::getHotkeyDevice() {
  KeyboardtopadDevice* res;

  for(unsigned int i=0; i<m_devices.size(); i++) {
    if(m_devices[i].type == "hotkeys") {
      return &(m_devices[i]);
    }
  }
  return NULL;
}

int GuiKeyboardtopads::countJoystickDevice(const std::string& type) {
  int n = 0;
  for(unsigned int i=0; i<m_devices.size(); i++) {
    if(m_devices[i].type == type) {
      n++;
    }
  }
  return n;
}

void GuiKeyboardtopads::loadPlayerPage(int n, const std::string& device_path) 
{
  std::vector<KeyboardtopadsInputStructure> GUI_INPUT_CONFIG_LIST = 
        {
                { "btn:south",           "SOUTH",              ":/help/buttons_south.svg" },
                { "btn:east",            "EAST",               ":/help/buttons_east.svg" },
                { "btn:north",           "NORTH",              ":/help/buttons_north.svg" },
                { "btn:west",            "WEST",               ":/help/buttons_west.svg" },

                { "btn:start",           "START",              ":/help/button_start.svg" },
                { "btn:select",          "SELECT",             ":/help/button_select.svg" },

                { "abs:hat0y:-1",        "D-PAD UP",           ":/help/dpad_up.svg" },
                { "abs:hat0y:1",         "D-PAD DOWN",         ":/help/dpad_down.svg" },
                { "abs:hat0x:-1",        "D-PAD LEFT",         ":/help/dpad_left.svg" },
                { "abs:hat0x:1",         "D-PAD RIGHT",        ":/help/dpad_right.svg" },

                { "btn:tl",              "LEFT SHOULDER",      ":/help/button_l.svg" },
                { "btn:tr",              "RIGHT SHOULDER",     ":/help/button_r.svg" },

                { "btn:tl2",             "LEFT TRIGGER",       ":/help/button_lt.svg" },
                { "btn:tr2",             "RIGHT TRIGGER",      ":/help/button_rt.svg" },
                { "btn:thumbl",          "LEFT STICK PRESS",   ":/help/analog_thumb.svg" },
                { "btn:thumbr",          "RIGHT STICK PRESS",  ":/help/analog_thumb.svg" },

                { "btn:mode",            "HOTKEY",             ":/help/button_hotkey.svg" }
        };
        auto theme = ThemeData::getMenuTheme();
	Window* window = mWindow;

	KeyboardtopadDevice* device = getJoystickDevice(n-1);

	if(device == NULL) return;
	
	addInputTextRow(_("JOYSTICK NAME"), device->name, false, nullptr, [this, device](std::string new_value) { this->m_need_save = true; device->name = new_value; } );

	for(int i = 0; i < GUI_INPUT_CONFIG_LIST.size(); i++)
	{
		ComponentListRow row;
		
		// icon
		auto icon = std::make_shared<ImageComponent>(mWindow);
		icon->setIsLinear(true);
		icon->setImage(GUI_INPUT_CONFIG_LIST[i].icon);
		icon->setColorShift(theme->Text.color);
		icon->setResize(0, Font::get(FONT_SIZE_MEDIUM)->getLetterHeight() * 1.25f);
		row.addElement(icon, false);

		// spacer between icon and text
		auto spacer = std::make_shared<GuiComponent>(mWindow);
		spacer->setSize(16, 0);
		row.addElement(spacer, false);

		auto text = std::make_shared<TextComponent>(mWindow, pgettext("joystick", GUI_INPUT_CONFIG_LIST[i].dispName.c_str()), theme->Text.font, theme->Text.color);
		row.addElement(text, true);

		auto mapping = std::make_shared<TextComponent>(mWindow, getNameForInput(device, GUI_INPUT_CONFIG_LIST[i].code), theme->Text.font, theme->Text.color);
		row.addElement(mapping, true);

		std::string code = GUI_INPUT_CONFIG_LIST[i].code;
		row.makeAcceptInputHandler([this, window, device_path, device, code, mapping] { declareEvKey(window, device_path, [this, device, code, mapping](std::string key) { mapping->setValue(key); setDeviceValue(device, key, code); }); });
		addRow(row);
	}

	// adding keys that are not in the defined list
	for(unsigned int i=0; i<device->keys.size(); i++) {
	  bool found = false;
	  for(int x = 0; x < GUI_INPUT_CONFIG_LIST.size(); x++) {
	    if(GUI_INPUT_CONFIG_LIST[x].code == device->keys[i].value) {
	      found = true;
	    }
	  }
	  if(found == false) {
	    auto text = std::make_shared<TextComponent>(mWindow, device->keys[i].name, theme->Text.font, theme->Text.color);
	    std::string code = device->keys[i].value;
	    addWithLabel(device->keys[i].value, text, false, [this, window, device_path, device, text, code] { declareEvKey(window, device_path, [this, device, text, code](std::string key) { text->setValue(key); setDeviceValue(device, key, code); }); });
	  }
	}
}

void GuiKeyboardtopads::declareEvKey(Window* window, const std::string& device_path, const std::function<void(std::string)>& func)
{
  window->pushGui(new GuiLoading<std::string>(window, _("PRESS A KEY OR WAIT 4 SECONDS TO CLEAR"), [this, window, device_path, func](auto gui)
  {
    m_need_save = true;
    return ApiSystem::getInstance()->detectEvKey(device_path);
  }, func));
}

std::string GuiKeyboardtopads::getNameForInput(KeyboardtopadDevice* device, std::string value) {
  for(unsigned int i=0; i<device->keys.size(); i++) {
    if(device->keys[i].value == value) return device->keys[i].name;
  }
  return "";
}

void GuiKeyboardtopads::loadHotkeysPage(const std::string& device_path) 
{
  std::vector<hotkeyTargetDefinition> targets_labels = {
    { "bezels",           _("OVERLAYS") },
    { "brightness-cycle", _("BRIGHTNESS CYCLE") },
    { "coin",             _("COIN") },
    { "exit",             _("EXIT") },
    { "fastforward",      _("FAST FORWARD") },
    { "menu",             _("MENU") },
    { "files",            _("FILES") },
    { "next_disk",        _("NEXT DISK") },
    { "next_slot",        _("NEXT SLOT") },
    { "pause",            _("PAUSE") },
    { "previous_slot",    _("PREVIOUS SLOT") },
    { "reset",            _("RESET") },
    { "restore_state",    _("RESTORE STATE") },
    { "rewind",           _("REWIND") },
    { "save_state",       _("SAVE STATE") },
    { "screen_layout",    _("SCREEN LAYOUT") },
    { "screenshot",       _("SCREENSHOT") },
    { "swap_screen",      _("SWAP SCREEN") },
    { "translation",      _("TRANSLATION") },
    { "volumedown",       _("VOLUME DOWN") },
    { "volumemute",       _("VOLUME MUTE") },
    { "volumeup",         _("VOLUME UP") },
    { "controlcenter",    _("CONTROL CENTER") }
  };

  KeyboardtopadDevice* device = getHotkeyDevice();
  if(device == NULL) return;

  Window* window = mWindow;

  std::vector<KeyboardtopadKey> key_values = ApiSystem::getInstance()->getKeyboardtopadKeyValues();
  auto theme = ThemeData::getMenuTheme();

  for(unsigned int i = 0; i < key_values.size(); i++) {
    std::string xlabel = key_values[i].value;
    for(unsigned int x = 0; x < targets_labels.size(); x++) {
      if(targets_labels[x].code == key_values[i].name) {
	xlabel = targets_labels[x].name;
	break;
      }
    }

    auto text = std::make_shared<TextComponent>(mWindow, getNameForInput(device, key_values[i].value), theme->Text.font, theme->Text.color);
    std::string code = key_values[i].value;
    addWithLabel(xlabel, text, false, [this, window, device_path, device, text, code] { declareEvKey(window, device_path, [this, device, text, code](std::string key) { text->setValue(key); this->updateSize(); setDeviceValue(device, key, code); }); });
  }
}

std::vector<HelpPrompt> GuiKeyboardtopads::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("start", _("CLOSE")));
	return prompts;
}

void GuiKeyboardtopads::setDeviceValue(KeyboardtopadDevice* ktp, std::string key, std::string value) {
  if(key == "") {
    int nerase  =-1;
    for(unsigned int i=0; i<ktp->keys.size(); i++) {
      if(ktp->keys[i].value == value) {
	nerase = i;
      }
    }
    if(nerase != -1) {
      ktp->keys.erase(ktp->keys.begin()+nerase);
    }
  } else {
    for(unsigned int i=0; i<ktp->keys.size(); i++) {
      if(ktp->keys[i].name == key) {
	ktp->keys[i].value = value;
	return;
      }
    }
    // not found, create a new one
    KeyboardtopadKey ktpk;
    ktpk.name = key;
    ktpk.value = value;
    ktp->keys.push_back(ktpk);
  }
}
