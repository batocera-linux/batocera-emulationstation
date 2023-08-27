#include "GunManager.h"
#include "Log.h"
#include "utils/Platform.h"
#include "Window.h"
#include <SDL.h>
#include <iostream>
#include <assert.h>
#include <algorithm>
#include "utils/StringUtil.h"
#include "LocaleES.h"
#include "renderers/Renderer.h"
#include "InputManager.h"

#ifdef HAVE_UDEV
#include <poll.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <unistd.h>
#define test_bit(array, bit)    (array[bit/8] & (1<<(bit%8)))
#else
// Uncomment for testing purpose to fake guns using the mouse
// #define FAKE_GUNS 1
#endif

GunManager::GunManager()
{
#ifdef HAVE_UDEV
	udev = udev_new();
	if (udev != NULL)
	{
		udev_monitor = udev_monitor_new_from_netlink(udev, "udev");
		if (udev_monitor)
		{
			udev_monitor_filter_add_match_subsystem_devtype(udev_monitor, "input", NULL);
			udev_monitor_enable_receiving(udev_monitor);
		}
	}
	udev_initial_gunsList();
#endif
}

GunManager::~GunManager()
{
#ifdef HAVE_UDEV
	if (udev != NULL)
	{
		// close guns
		for (auto gun : mGuns) 
		{
		  udev_closeGun(gun);
		  delete gun;
		}

		mGuns.clear();

		if (udev_monitor)
			udev_monitor_unref(udev_monitor);
		
		udev_unref(udev);
	}
#endif
}

int GunManager::readGunEvents(Gun* gun)
{
#ifdef HAVE_UDEV
	int len;
	struct input_event input_events[64];

	while ((len = read(gun->fd, input_events, sizeof(input_events))) > 0) {
	  for (unsigned i = 0; i<len/sizeof(input_event); i++) {
			if (input_events[i].type == EV_KEY) {
				//printf("key, code=%i, value=%i\n", input_events[i].code, input_events[i].value);
				switch (input_events[i].code) {
				case KEY_CONFIG:
					if (input_events[i].value == 1) {
						// starting calibration
						return 1;
					}
					else {
						// stopping calibration
						return 2;
					}
					break;
				case BTN_LEFT:
					gun->mLButtonDown = (input_events[i].value != 0);
					break;
				case BTN_RIGHT:
					gun->mRButtonDown = (input_events[i].value != 0);
					break;
				case BTN_MIDDLE:
					gun->mStartButtonDown = (input_events[i].value != 0);
					break;
				case BTN_1:
					gun->mSelectButtonDown = (input_events[i].value != 0);
					break;
				case BTN_5:
					gun->mDPadUpButtonDown = (input_events[i].value != 0);
					break;
				case BTN_6:
					gun->mDPadDownButtonDown = (input_events[i].value != 0);
					break;
				case BTN_7:
					gun->mDPadLeftButtonDown = (input_events[i].value != 0);
					break;
				case BTN_8:
					gun->mDPadRightButtonDown = (input_events[i].value != 0);
					break;
				}
			}
		}
	}
#endif

	return 0;
}

void GunManager::updateGuns(Window* window)
{
#ifdef HAVE_UDEV
	const char* val_gun;
	const char* val_gunborder;
	bool bGunborder;
	const char* action;

	while (udev_monitor && udev_input_poll_hotplug_available(udev_monitor)) 
	{
		struct udev_device *dev = udev_monitor_receive_device(udev_monitor);
		bool dev_handled = false;

		if (dev != NULL) 
		{
			val_gun = udev_device_get_property_value(dev, "ID_INPUT_GUN"); // ID_INPUT_GUN and ID_INPUT_MOUSE for some tests
			val_gunborder = udev_device_get_property_value(dev, "ID_INPUT_GUN_NEED_BORDERS");
			bGunborder = false;

			if (val_gunborder != NULL && strncmp(val_gunborder, "1", 1) == 0)
			  bGunborder = true;

			action = udev_device_get_action(dev);

			if (val_gun != NULL && strncmp(val_gun, "1", 1) == 0)
			{
				if (strncmp(action, "add", 3) == 0)
				{
				  if (udev_addGun(dev, window, bGunborder))
						dev_handled = true;
				}
				else if (strncmp(action, "remove", 6) == 0)
				{
					if (udev_removeGun(dev, window)) 
						dev_handled = true;
				}
			}

			if (!dev_handled) 
				udev_device_unref(dev); // not handled, clean it
		}
	}
#elif FAKE_GUNS
	if (mGuns.size() == 0)
	{
		Gun* newgun = new Gun();
		newgun->mIndex = mGuns.size();
		newgun->mName = "Mouse";
		newgun->mNeedBorders = false;
		mGuns.push_back(newgun);
	}
#elif WIN32
	bool hasGun = false;

	HWND hWndWiimoteGun = FindWindow("WiimoteGun", NULL);
	if (hWndWiimoteGun)
	{
		int mode = (int) GetProp(hWndWiimoteGun, "mode");
		hasGun = (mode == 1);
	}

	if (hasGun && mGuns.size() == 0)
	{
		Gun* newgun = new Gun();
		newgun->mIndex = mGuns.size();
		newgun->mName = "Wiimote Gun";
		mGuns.push_back(newgun);
	}
	else if (!hasGun && mGuns.size())
	{
		for (auto gun : mGuns)
			delete gun;

		mGuns.clear();
	}
#endif

	float gunMoveTolerence = Settings::getInstance()->getFloat("GunMoveTolerence")/100.0;

	int gunEvent = 0;
	for (Gun* gun : mGuns)
	{
		Gun oldGun = *gun;

		int evt = readGunEvents(gun);
		if (evt != 0)
			gunEvent = evt;

		updateGunPosition(gun);

		if(fabsf(oldGun.mX - gun->mX) > gunMoveTolerence || fabsf(oldGun.mY - gun->mY) > gunMoveTolerence) {
		  window->processMouseMove(gun->x(), gun->y(), true);
		}

#ifdef HAVE_UDEV
		// gun pads
		if(gun->isDPadUpButtonDown() != oldGun.isDPadUpButtonDown())
		  window->input(InputManager::getInstance()->getInputConfigByDevice(DEVICE_GUN), Input(DEVICE_GUN, TYPE_BUTTON, BTN_5, gun->isDPadUpButtonDown() ? 1 : 0, true));

		if(gun->isDPadDownButtonDown() != oldGun.isDPadDownButtonDown())
		  window->input(InputManager::getInstance()->getInputConfigByDevice(DEVICE_GUN), Input(DEVICE_GUN, TYPE_BUTTON, BTN_6, gun->isDPadDownButtonDown() ? 1 : 0, true));

		if(gun->isDPadLeftButtonDown() != oldGun.isDPadLeftButtonDown())
		  window->input(InputManager::getInstance()->getInputConfigByDevice(DEVICE_GUN), Input(DEVICE_GUN, TYPE_BUTTON, BTN_7, gun->isDPadLeftButtonDown() ? 1 : 0, true));

		if(gun->isDPadRightButtonDown() != oldGun.isDPadRightButtonDown())
		  window->input(InputManager::getInstance()->getInputConfigByDevice(DEVICE_GUN), Input(DEVICE_GUN, TYPE_BUTTON, BTN_8, gun->isDPadRightButtonDown() ? 1 : 0, true));

		// start / select
		if(gun->isStartButtonDown() != oldGun.isStartButtonDown())
		  window->input(InputManager::getInstance()->getInputConfigByDevice(DEVICE_GUN), Input(DEVICE_GUN, TYPE_BUTTON, BTN_MIDDLE, gun->isStartButtonDown() ? 1 : 0, true));

		if(gun->isSelectButtonDown() != oldGun.isSelectButtonDown())
		  window->input(InputManager::getInstance()->getInputConfigByDevice(DEVICE_GUN), Input(DEVICE_GUN, TYPE_BUTTON, BTN_1, gun->isSelectButtonDown() ? 1 : 0, true));
#endif
	}

	if (gunEvent == 1 || gunEvent == 2)
		window->setGunCalibrationState(gunEvent == 1);
}

bool GunManager::updateGunPosition(Gun* gun) 
{
#ifdef HAVE_UDEV
	struct input_absinfo absinfo;

	if (ioctl(gun->fd, EVIOCGABS(ABS_X), &absinfo) == -1) 
		return false;

	gun->mX = ((float)(absinfo.value - absinfo.minimum)) / ((float)(absinfo.maximum - absinfo.minimum));

	if (ioctl(gun->fd, EVIOCGABS(ABS_Y), &absinfo) == -1) 
		return false;

	gun->mY = ((float)(absinfo.value - absinfo.minimum)) / ((float)(absinfo.maximum - absinfo.minimum));

	return true;
#else
	int x, y;
	auto buttons = SDL_GetMouseState(&x, &y);
	gun->mX = x;
	gun->mY = y;
	gun->mLButtonDown = (buttons & SDL_BUTTON_LMASK) != 0;
	gun->mRButtonDown = (buttons & SDL_BUTTON_RMASK) != 0;
	return true;	
#endif	
	return false;
}


#ifdef HAVE_UDEV
// function from retroarch
bool GunManager::udev_input_poll_hotplug_available(struct udev_monitor *dev)
{
	struct pollfd fds;

	fds.fd = udev_monitor_get_fd(dev);
	fds.events = POLLIN;
	fds.revents = 0;

	return (poll(&fds, 1, 0) == 1) && (fds.revents & POLLIN);
}

void GunManager::udev_initial_gunsList()
{
	struct udev_list_entry *devs = NULL;
	struct udev_list_entry *item = NULL;
	const char* val_gunborder;
	bool bGunborder;

	struct udev_enumerate *enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_property(enumerate, "ID_INPUT_GUN", "1");
	udev_enumerate_add_match_subsystem(enumerate, "input");
	udev_enumerate_scan_devices(enumerate);
	devs = udev_enumerate_get_list_entry(enumerate);

	for (item = devs; item; item = udev_list_entry_get_next(item)) 
	{
		const char         *name = udev_list_entry_get_name(item);
		struct udev_device *dev = udev_device_new_from_syspath(udev, name);

		val_gunborder = udev_device_get_property_value(dev, "ID_INPUT_GUN_NEED_BORDERS"); // ID_INPUT_GUN and ID_INPUT_MOUSE for some tests
		bGunborder = false;

		if (val_gunborder != NULL && strncmp(val_gunborder, "1", 1) == 0)
		  bGunborder = true;

		if (udev_addGun(dev, NULL, bGunborder) == false) udev_device_unref(dev); // unhandled device
	}
}

bool GunManager::udev_addGun(struct udev_device *dev, Window* window, bool needGunBorder)
{
	char ident[256];
	const char* devnode;
	unsigned char abscaps[(ABS_MAX / 8) + 1] = { '\0' };

	// check that devnode is of form /dev/input/eventXX (to not include devices aliases)
	devnode = udev_device_get_devnode(dev);
	if (devnode == NULL) return false;
	if (strncmp(devnode, "/dev/input/event", 16) != 0) return false;

	int fd = open(devnode, O_RDONLY | O_NONBLOCK);
	if (fd < 0) return false;

	// check absolute capabilities
	if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abscaps)), abscaps) == -1) {
		close(fd);
		return false;
	}

	// check absolute capabilities x and y
	if (((test_bit(abscaps, ABS_X)) && (test_bit(abscaps, ABS_Y))) == false) {
		close(fd);
		return false;
	}

	LOG(LogInfo) << "Gun found at " << devnode;

	if (ioctl(fd, EVIOCGNAME(sizeof(ident)), ident) < 0)
		ident[0] = '\0';

	Gun* newgun = new Gun();
	newgun->mIndex = mGuns.size();
	newgun->mName = std::string(ident);
	newgun->devpath = devnode;
	newgun->dev = dev;
	newgun->fd = fd;
	newgun->mNeedBorders = needGunBorder;

	if (!newgun->mName.empty() && window != NULL)
		window->displayNotificationMessage(_U("\uF05B ") + Utils::String::format(_("%s connected").c_str(), Utils::String::trim(newgun->mName).c_str()));

	mGuns.push_back(newgun);

	return true;
}

void GunManager::udev_closeGun(Gun* gun) {
  close(gun->fd);
  udev_device_unref(gun->dev);
}

bool GunManager::udev_removeGun(struct udev_device *dev, Window* window)
{
	const char* devnode;

	devnode = udev_device_get_devnode(dev);
	LOG(LogInfo) << "Gun removed at " << devnode;

	if (devnode == NULL) return false;

	for (auto iter = mGuns.begin(); iter != mGuns.end(); iter++)
	{
		if ((*iter)->devpath == devnode)
		{
			LOG(LogInfo) << "Gun removed found at " << devnode;
			if (window != NULL)
				window->displayNotificationMessage(_U("\uF05B ") + Utils::String::format(_("%s disconnected").c_str(), Utils::String::trim((*iter)->mName).c_str()));

			udev_closeGun(*iter);
			delete *iter;
			mGuns.erase(iter);

			// Renumber guns
			int idx = 0;
			for (Gun* gun : mGuns) gun->mIndex = idx++;

			return true;
		}
	}
	return false;
}
float Gun::x() { return mX * Renderer::getScreenWidth(); }
float Gun::y() { return mY * Renderer::getScreenHeight(); }
#else 
float Gun::x() { return mX; }
float Gun::y() { return mY; }
#endif
