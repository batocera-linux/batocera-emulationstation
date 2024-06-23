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
#elif WIN32

#include <set>
#include <SDL_syswm.h>
#include <hidsdi.h>
#pragma comment(lib, "Hid.lib")

#define WIIMOTE_GUN "Wiimote Gun"

// Uncomment FORCE_REPLACEMOUSE to force using the mouse with GunManager ( for testing purpose )
// #define FORCE_REPLACEMOUSE 1

// Warning : never call SDL_SetRelativeMouseMode(true) with Windows or RAWINPUT will be managed by SDL

enum class LightGunType
{
	Mouse,
	SindenLightgun,
	Gun4Ir,
	MayFlashWiimote
};

class RawInputManager
{
public:
	static LightGunType getLightGunType(HANDLE hDevice)
	{
		std::string name = GetRawInputDevicePath(hDevice);

		std::string sindenDeviceIds[] = { "VID_16C0&PID_0F01", "VID_16C0&PID_0F02", "VID_16C0&PID_0F38", "VID_16C0&PID_0F39" };
		for (auto id : sindenDeviceIds)
			if (name.find(id) != std::string::npos)
				return LightGunType::SindenLightgun;

		std::string gun4irDeviceIds[] = { "VID_2341&PID_8042", "VID_2341&PID_8043", "VID_2341&PID_8044", "VID_2341&PID_8045" };
		for (auto id : gun4irDeviceIds)
			if (name.find(id) != std::string::npos)
				return LightGunType::Gun4Ir;

		std::string mayFlashWiimoteIds[] = { "VID_0079&PID_1802" };  // Mayflash Wiimote, using mode 1
		for (auto id : mayFlashWiimoteIds)
			if (name.find(id) != std::string::npos)
				return LightGunType::MayFlashWiimote;

		return LightGunType::Mouse;
	}

	static std::string GetRawInputDevicePath(HANDLE hDevice)
	{
		UINT bufferSize = 0;

		// Get the required buffer size for the device name
		if (GetRawInputDeviceInfoA(hDevice, RIDI_DEVICENAME, nullptr, &bufferSize) != 0) {
			return "";
		}

		// Allocate memory for the device name
		char* lpDeviceName = new char[bufferSize];
		if (lpDeviceName == nullptr)
			return "";

		// Get the device name
		if (GetRawInputDeviceInfoA(hDevice, RIDI_DEVICENAME, lpDeviceName, &bufferSize) == -1) {
			delete[] lpDeviceName;
			return "";
		}

		std::string deviceName(lpDeviceName);
		delete[] lpDeviceName;
		return deviceName;
	}

	static std::string GetRawInputDeviceName(HANDLE hDevice)
	{
		std::string path = GetRawInputDevicePath(hDevice);

		std::string ret;

		HANDLE hhid = CreateFileA(path.c_str(), 0, 3, nullptr, 3, 0x00000080, nullptr);
		if (hhid != INVALID_HANDLE_VALUE)
		{
			wchar_t buf[255];
			if (HidD_GetProductString(hhid, &buf[0], 255))
				ret = Utils::String::convertFromWideString(buf);

			CloseHandle(hhid);
		}

		return ret;
	}
};
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
#elif WIN32
	enableRawInputCapture(false);
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
				gun->m_lastTick = SDL_GetTicks();

				switch (input_events[i].code) 
				{
				case KEY_CONFIG:
					if (input_events[i].value == 1) {
						// starting calibration
						gun->mIsCalibrating = true;
						return 1;
					}
					else {
						// stopping calibration
						gun->mIsCalibrating = false;
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

class Stabilizer
{
public:
	Stabilizer()
	{
		mWindowSize = 6;
		mBuffer.resize(mWindowSize);
	}

	void add(float x, float y)
	{
		mBuffer.push_back({ x, y });
		if (mBuffer.size() > mWindowSize)
			mBuffer.pop_front();
	}

	std::pair<float, float> getStabilizedPosition()
	{
		if (mBuffer.empty())
			return { 0.0f, 0.0f };

		float sumX = 0.0f, sumY = 0.0f;
		for (const auto& pos : mBuffer)
		{
			sumX += pos.first;
			sumY += pos.second;
		}

		return { sumX / (float)mBuffer.size(), sumY / (float)mBuffer.size() };
	}

private:
	int mWindowSize;
	std::deque<std::pair<float, float>> mBuffer;
};

Stabilizer* Gun::getStabilizer()
{
	if (m_pStabilizer == nullptr)
		m_pStabilizer = new Stabilizer();

	return m_pStabilizer;
}

bool Gun::isLastTickElapsed()
{
#ifdef HAVE_UDEV
	if (mIsCalibrating)
		return false;
#endif

	return m_lastTick == 0 || (SDL_GetTicks() - m_lastTick) > 5000;
}

#if WIN32

bool GunManager::mMessageHookRegistered = false;



void GunManager::enableRawInputCapture(bool enable)
{
	if (mMessageHookRegistered == enable)
		return;

	// Register to receive raw input from mice
	RAWINPUTDEVICE rid[1];
	rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
	rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
	rid[0].dwFlags = enable ? 0 : RIDEV_REMOVE;
	rid[0].hwndTarget = NULL;

	if (enable)
	{
		SDL_SysWMinfo wmInfo;
		SDL_VERSION(&wmInfo.version);
		if (SDL_GetWindowWMInfo(Renderer::getSDLWindow(), &wmInfo))
		{
			rid[0].dwFlags = RIDEV_INPUTSINK;
			rid[0].hwndTarget = (HWND)wmInfo.info.win.window;
		}
	}

	RegisterRawInputDevices(rid, 1, sizeof(RAWINPUTDEVICE));

	if (enable)
		SDL_SetWindowsMessageHook(&GunManager::WindowsMessageHook, this);
	else 
		SDL_SetWindowsMessageHook(nullptr, nullptr);

	mMessageHookRegistered = enable;
}

static bool _isScreenPointOverWindow(HWND hWnd, int x, int y)
{
	POINT cursorPos{ x, y };
	if (x == -9999 && y == -9999)
		GetCursorPos(&cursorPos);

	// Get the client rectangle of the window
	RECT rc;
	GetClientRect(hWnd, &rc);
	ClientToScreen(hWnd, reinterpret_cast<POINT*>(&rc.left)); // convert top-left
	ClientToScreen(hWnd, reinterpret_cast<POINT*>(&rc.right));

	// Check if the cursor is within the client area
	return PtInRect(&rc, cursorPos);
}

void GunManager::WindowsMessageHook(void* userdata, void* hWnd, unsigned int message, Uint64 wParam, Sint64 lParam)
{
	if (message != WM_INPUT || userdata == nullptr)
		return;

	RAWINPUT rawInput;
	UINT size = sizeof(RAWINPUT);
	UINT ret = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &rawInput, &size, sizeof(RAWINPUTHEADER));
	if (ret == (UINT)-1)
		return;

	if (rawInput.header.dwType != RIM_TYPEMOUSE)
		return;

	GunManager* mgr = (GunManager*)userdata;

	auto path = RawInputManager::GetRawInputDevicePath(rawInput.header.hDevice);	
	auto it = std::find_if(mgr->mGuns.cbegin(), mgr->mGuns.cend(), [path](Gun* pGun) { return pGun->mPath == path; });
	if (it == mgr->mGuns.cend())
		return;

	Gun* gun = *it;

	if (gun->m_isMouse)
	{
		auto itFirstMouse = std::find_if(mgr->mGuns.cbegin(), mgr->mGuns.cend(), [path](Gun* pGun) { return pGun->m_isMouse; });
		if (itFirstMouse != mgr->mGuns.cend())
			gun = *itFirstMouse;
	}

	// Handle mouse input
	RAWMOUSE& rawMouse = rawInput.data.mouse;

	RECT rect;
	::GetWindowRect((HWND)hWnd, &rect);
	RECT rectClient;
	::GetClientRect((HWND)hWnd, &rectClient);

	if (gun->m_isMouse)
	{
		POINT cursorPos;
		GetCursorPos(&cursorPos);		
		ScreenToClient((HWND)hWnd, &cursorPos);

		if (gun->m_internalX != cursorPos.x || gun->m_internalY != cursorPos.y)
		{
			gun->m_internalX = cursorPos.x;
			gun->m_internalY = cursorPos.y;
			gun->m_lastTick = SDL_GetTicks();
		}
	}
	else if (rawMouse.lLastX != 0 && rawMouse.lLastY != 0)
	{
		if ((rawMouse.usFlags & 0x01) == MOUSE_MOVE_ABSOLUTE)
		{
			bool isVirtualDesktop = (rawMouse.usFlags & MOUSE_VIRTUAL_DESKTOP) == MOUSE_VIRTUAL_DESKTOP;

			int width = GetSystemMetrics(isVirtualDesktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
			int height = GetSystemMetrics(isVirtualDesktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);

			POINT cursorPos;
			cursorPos.x = int((rawMouse.lLastX / 65535.0f) * width);
			cursorPos.y = int((rawMouse.lLastY / 65535.0f) * height);
			ScreenToClient((HWND)hWnd, &cursorPos);

			gun->getStabilizer()->add(cursorPos.x, cursorPos.y);

			auto stabilizedPos = gun->getStabilizer()->getStabilizedPosition();

			if (gun->m_internalX != stabilizedPos.first || gun->m_internalY != stabilizedPos.second)
			{
				gun->m_internalX = stabilizedPos.first;
				gun->m_internalY = stabilizedPos.second;
				gun->m_lastTick = SDL_GetTicks();
			}
		}
		else if ((rawMouse.usFlags & 0x01) == MOUSE_MOVE_RELATIVE)
		{
			gun->m_internalX = Math::clamp(gun->mX + rawMouse.lLastX, -1, rectClient.right - rectClient.left + 1);
			gun->m_internalY = Math::clamp(gun->mY + rawMouse.lLastY, -1, rectClient.bottom - rectClient.top + 1);
			gun->m_lastTick = SDL_GetTicks();
		}
	}

	// https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawmouse
	if (rawMouse.usButtonFlags)
	{
		bool clickEnabled = true;

		if (GetActiveWindow() != hWnd)
			clickEnabled = false;
		else
		{
			POINT ptScreen{ gun->m_internalX, gun->m_internalY };
			ClientToScreen((HWND)hWnd, &ptScreen);
			clickEnabled = _isScreenPointOverWindow((HWND)hWnd, ptScreen.x, ptScreen.y) && ::WindowFromPoint(ptScreen) == (HWND)hWnd;
		}

		int	buttonState = gun->m_internalButtonState;

		// Check the button flags
		if (rawMouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN && clickEnabled)
			buttonState |= 1;

		if (rawMouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
			buttonState &= ~1;

		if (rawMouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN && clickEnabled)
			buttonState |= 2;

		if (rawMouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
			buttonState &= ~2;

		if (rawMouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN && clickEnabled)
			buttonState |= 4;

		if (rawMouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
			buttonState &= ~4;

		if (rawMouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN && clickEnabled)
			buttonState |= 8;

		if (rawMouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)
			buttonState &= ~8;

		if (rawMouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN && clickEnabled)
			buttonState |= 16;

		if (rawMouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)
			buttonState &= ~16;

		if (buttonState != gun->m_internalButtonState)
		{
			gun->m_internalButtonState = buttonState;
			gun->m_lastTick = SDL_GetTicks();
		}
	}
}
#endif

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
#elif WIN32

	static int updateGunCheck = 0;

	// Check gun changed every 10 times it's called
	if (updateGunCheck++ > 10)
	{
		updateGunCheck = 0;

		// Get the number of raw input devices
		UINT numDevices;
		if (GetRawInputDeviceList(NULL, &numDevices, sizeof(RAWINPUTDEVICELIST)) == -1)
			return;

		// Allocate memory for the device list
		RAWINPUTDEVICELIST* deviceList = new RAWINPUTDEVICELIST[numDevices];
		if (deviceList == nullptr)
			return;

		// Get the raw input device list
		if (GetRawInputDeviceList(deviceList, &numDevices, sizeof(RAWINPUTDEVICELIST)) == -1)
			delete[] deviceList;

		// Enumerate and print information about connected mice

		std::set<std::string> knownPaths;

		bool hasPhysicalGuns = false;

#if FORCE_REPLACEMOUSE
		hasPhysicalGuns = true;
#else
		for (UINT i = 0; i < numDevices; i++)
		{
			std::string devicePath = RawInputManager::GetRawInputDevicePath(deviceList[i].hDevice);
			if (deviceList[i].dwType != RIM_TYPEMOUSE && RawInputManager::getLightGunType(deviceList[i].hDevice) != LightGunType::Mouse)
			{
				hasPhysicalGuns = true;
				break;
			}
		}
#endif
		bool hasWiimoteGun = false;

		HWND hWndWiimoteGun = FindWindow("WiimoteGun", NULL);
		if (hWndWiimoteGun)
		{
			int mode = (int)GetProp(hWndWiimoteGun, "mode");
			hasWiimoteGun = (mode == 1);
		}

		if (hasPhysicalGuns || hasWiimoteGun)
		{
			for (UINT i = 0; i < numDevices; i++)
			{
				if (deviceList[i].dwType != RIM_TYPEMOUSE)
					continue;

				HANDLE hDevice = deviceList[i].hDevice;

				auto gunType = RawInputManager::getLightGunType(hDevice);
				if (hasWiimoteGun && gunType == LightGunType::Mouse)
					continue;

				std::string devicePath = RawInputManager::GetRawInputDevicePath(hDevice);
				knownPaths.insert(devicePath);

				if (std::any_of(mGuns.cbegin(), mGuns.cend(), [devicePath](Gun* pGun) { return pGun->mPath == devicePath; }))
					continue;

				Gun* newgun = new Gun();
				newgun->mIndex = mGuns.size();
				newgun->mName = RawInputManager::GetRawInputDeviceName(hDevice);
				newgun->mPath = devicePath;
				newgun->mNeedBorders = gunType == LightGunType::SindenLightgun;
				newgun->m_isMouse = gunType == LightGunType::Mouse;
				newgun->m_lastTick = SDL_GetTicks();

				if (newgun->m_isMouse)
				{
					int x, y;
					auto buttons = SDL_GetMouseState(&x, &y);
					newgun->mX = newgun->m_internalX = x;
					newgun->mY = newgun->m_internalY = y;
				}
				else if (window != NULL && !newgun->mName.empty())
					window->displayNotificationMessage(_U("\uF05B ") + Utils::String::format(_("%s connected").c_str(), Utils::String::trim(newgun->mName).c_str()));

				mGuns.push_back(newgun);
			}
		}

		// Remove guns
		auto iter = mGuns.cbegin();
		while (iter != mGuns.cend())
		{
			Gun* gun = *iter;

			if (gun->mName == WIIMOTE_GUN || knownPaths.find(gun->mPath) != knownPaths.cend())
				iter++;
			else
			{
				if (window != NULL && !gun->m_isMouse)
					window->displayNotificationMessage(_U("\uF05B ") + Utils::String::format(_("%s disconnected").c_str(), Utils::String::trim(gun->mName).c_str()));

				LOG(LogInfo) << "Gun removed found at " << gun->mPath;

				iter = mGuns.erase(iter);
				delete gun;
				renumberGuns();
			}
		}

		if (hasWiimoteGun && !std::any_of(mGuns.cbegin(), mGuns.cend(), [](Gun* pGun) { return pGun->mName == WIIMOTE_GUN; }))
		{
			Gun* newgun = new Gun();
			newgun->mIndex = mGuns.size();
			newgun->mName = WIIMOTE_GUN;
			newgun->m_isMouse = false;
			newgun->m_lastTick = SDL_GetTicks();

			mGuns.push_back(newgun);

			if (window != NULL)
				window->displayNotificationMessage(_U("\uF05B ") + Utils::String::format(_("%s connected").c_str(), Utils::String::trim(newgun->mName).c_str()));
		}
		else if (!hasWiimoteGun && mGuns.size())
		{
			for (auto it = mGuns.cbegin(); it != mGuns.cend(); it++)
			{
				Gun* gun = *it;
				if (gun->mName == WIIMOTE_GUN)
				{
					if (window != NULL)
						window->displayNotificationMessage(_U("\uF05B ") + Utils::String::format(_("%s disconnected").c_str(), Utils::String::trim(gun->mName).c_str()));

					mGuns.erase(it);
					delete gun;

					renumberGuns();
					break;
				}
			}
		}

		if (hasPhysicalGuns && !mMessageHookRegistered)
		{
			SDL_ShowCursor(0);
			enableRawInputCapture(true);			
		}
		else if (!hasPhysicalGuns && mMessageHookRegistered)
			enableRawInputCapture(false);

		delete[] deviceList;
	}
#endif

	float gunMoveTolerence = Settings::getInstance()->getFloat("GunMoveTolerence") / 100.0;

	int gunEvent = 0;
	for (Gun* gun : mGuns)
	{
		GunData* data = gun;
		GunData oldGun = *data;

		int evt = readGunEvents(gun);
		if (evt != 0)
			gunEvent = evt;

		updateGunPosition(gun);
		
		if (fabsf(oldGun.mX - gun->mX) > gunMoveTolerence || fabsf(oldGun.mY - gun->mY) > gunMoveTolerence)		
			window->processMouseMove(gun->x(), gun->y(), true);
		
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
#elif WIN32
		// gun pads
		if (gun->isLButtonDown() != oldGun.isLButtonDown())
		{
			if (!window->processMouseButton(1, gun->isLButtonDown(), gun->x(), gun->y()))
				window->input(InputManager::getInstance()->getInputConfigByDevice(DEVICE_MOUSE), Input(DEVICE_MOUSE, TYPE_BUTTON, 1, gun->isLButtonDown() ? 1 : 0, false));
		}

		if (gun->isRButtonDown() != oldGun.isRButtonDown())
		{
			if (!window->processMouseButton(3, gun->isRButtonDown(), gun->x(), gun->y()))
				window->input(InputManager::getInstance()->getInputConfigByDevice(DEVICE_MOUSE), Input(DEVICE_MOUSE, TYPE_BUTTON, 3, gun->isRButtonDown() ? 1 : 0, false));
		}

		if (gun->isMButtonDown() != oldGun.isMButtonDown())
		{
			auto config = InputManager::getInstance()->getInputConfigByDevice(DEVICE_KEYBOARD);

			Input input;
			if (config->getInputByName(BUTTON_OK, &input))
				window->input(config, Input(input.device, input.type, input.id, gun->isMButtonDown() ? 1 : 0, true));
		}

		if (gun->isStartButtonDown() != oldGun.isStartButtonDown())
		{
			auto config = InputManager::getInstance()->getInputConfigByDevice(DEVICE_KEYBOARD);

			Input input;
			if (config->getInputByName("start", &input))
				window->input(config, Input(input.device, input.type, input.id, gun->isStartButtonDown() ? 1 : 0, true));
		}

		if (gun->isSelectButtonDown() != oldGun.isSelectButtonDown())
		{
			auto config = InputManager::getInstance()->getInputConfigByDevice(DEVICE_KEYBOARD);

			Input input;
			if (config->getInputByName("select", &input))
				window->input(config, Input(input.device, input.type, input.id, gun->isSelectButtonDown() ? 1 : 0, true));
		}
#endif
	}

	if (gunEvent == 1 || gunEvent == 2)
		window->setGunCalibrationState(gunEvent == 1);
}

void GunManager::renumberGuns()
{
	// Renumber guns
	int idx = 0;
	for (Gun* gun : mGuns) 
		gun->mIndex = idx++;	
}

bool GunManager::isReplacingMouse()
{
#if WIN32
	return mGuns.size();
#endif
	return false;
}

bool GunManager::updateGunPosition(Gun* gun) 
{
#ifdef HAVE_UDEV
	struct input_absinfo absinfo;

	float x = gun->mX;
	float y = gun->mY;

	if (ioctl(gun->fd, EVIOCGABS(ABS_X), &absinfo) == -1) 
		return false;

	x = ((float)(absinfo.value - absinfo.minimum)) / ((float)(absinfo.maximum - absinfo.minimum));

	if (ioctl(gun->fd, EVIOCGABS(ABS_Y), &absinfo) == -1) 
		return false;

	y = ((float)(absinfo.value - absinfo.minimum)) / ((float)(absinfo.maximum - absinfo.minimum));

	if (x != gun->mX || y != gun->mY)
	{
		gun->mX = x;
		gun->mY = y;
		gun->m_lastTick = SDL_GetTicks();
	}

	return true;
#endif

#ifdef WIN32
	if (gun->mName == WIIMOTE_GUN)
	{
		int x, y;
		auto buttons = SDL_GetMouseState(&x, &y);
		gun->mX = x;
		gun->mY = y;
		gun->mLButtonDown = (buttons & SDL_BUTTON_LMASK) != 0;
		gun->mRButtonDown = (buttons & SDL_BUTTON_RMASK) != 0;
		gun->mMButtonDown = (buttons & SDL_BUTTON_MMASK) != 0;
		return true;
	}

	gun->mX = gun->m_internalX;
	gun->mY = gun->m_internalY;
	gun->mLButtonDown = (gun->m_internalButtonState & 1) == 1;
	gun->mRButtonDown = (gun->m_internalButtonState & 2) == 2;
	gun->mMButtonDown = (gun->m_internalButtonState & 4) == 4;
	gun->mStartButtonDown = (gun->m_internalButtonState & 8) == 8;
	gun->mSelectButtonDown = (gun->m_internalButtonState & 16) == 16;	
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
	newgun->m_lastTick = SDL_GetTicks();

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

			renumberGuns();
			return true;
		}
	}
	return false;
}
float GunData::x() { return mX * Renderer::getScreenWidth(); }
float GunData::y() { return mY * Renderer::getScreenHeight(); }
#else 
float GunData::x() { return mX; }
float GunData::y() { return mY; }
#endif

Gun::~Gun()
{
	if (m_pStabilizer != nullptr)
	{
		delete m_pStabilizer;
		m_pStabilizer = nullptr;
	}
}
