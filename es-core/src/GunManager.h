#pragma once
#ifndef ES_CORE_GUNMANAGER_H
#define ES_CORE_GUNMANAGER_H

#include <string>
#include <vector>
#include <deque>

#ifdef HAVE_UDEV
#include <libudev.h>
#elif WIN32
#include <SDL.h>
#include <Windows.h>
#endif

class InputConfig;
class Window;
class Stabilizer;
union SDL_Event;

class GunData
{
	friend class GunManager;

public:
	GunData() : mX(-1), mY(-1), mLButtonDown(false), mRButtonDown(false), mMButtonDown(false), mStartButtonDown(false), mSelectButtonDown(false),
		mDPadUpButtonDown(false), mDPadDownButtonDown(false), mDPadLeftButtonDown(false), mDPadRightButtonDown(false) 
	{ }

	float x();
	float y();

	bool isLButtonDown() { return mLButtonDown; }
	bool isRButtonDown() { return mRButtonDown; }
	bool isMButtonDown() { return mMButtonDown; }

	bool isStartButtonDown() { return mStartButtonDown; }
	bool isSelectButtonDown() { return mSelectButtonDown; }
	bool isDPadUpButtonDown() { return mDPadUpButtonDown; }
	bool isDPadDownButtonDown() { return mDPadDownButtonDown; }
	bool isDPadLeftButtonDown() { return mDPadLeftButtonDown; }
	bool isDPadRightButtonDown() { return mDPadRightButtonDown; }

private:

	float mX;
	float mY;

	bool mLButtonDown;
	bool mRButtonDown;
	bool mMButtonDown;
	bool mStartButtonDown;
	bool mSelectButtonDown;
	bool mDPadUpButtonDown;
	bool mDPadDownButtonDown;
	bool mDPadLeftButtonDown;
	bool mDPadRightButtonDown;
};

class Gun : public GunData
{
	friend class GunManager;

public:
	Gun() 
	{
		mIndex = 0;
		mNeedBorders = false;
		m_isMouse = false;
		m_pStabilizer = nullptr;
		m_lastTick = 0;

#ifdef HAVE_UDEV
		dev = nullptr;
		fd = 0;
#elif WIN32
		m_internalButtonState = 0;
		m_internalX = 0;
		m_internalY = 0;			
#endif
	}

	Gun(const Gun& src) = delete;

	~Gun();

	int index() { return mIndex; }
	std::string& name() { return mName; }
	bool needBorders() { return mNeedBorders; }
	bool isMouse() { return m_isMouse; }

	Stabilizer* getStabilizer();

	bool isLastTickElapsed();

private:
	Stabilizer* m_pStabilizer;

	std::string mName;
	int  mIndex;
	bool mNeedBorders;
	bool m_isMouse;

#ifdef HAVE_UDEV
	std::string devpath;
	udev_device* dev;
	int fd;
	bool	mIsCalibrating;
#elif WIN32
	std::string mPath;
	int		m_internalButtonState;
	float	m_internalX;
	float	m_internalY;
#endif

	int		m_lastTick;
};

//you should only ever instantiate one of these, by the way
class GunManager
{
public:
	GunManager();
	virtual ~GunManager();

	std::vector<Gun*>& getGuns() { return mGuns; }
	void updateGuns(Window* window);

	bool isReplacingMouse();

private:
	bool updateGunPosition(Gun* gun);
	int readGunEvents(Gun* gun);
	void renumberGuns();

	std::vector<Gun*> mGuns;

#ifdef HAVE_UDEV
  struct udev *udev;
  struct udev_monitor *udev_monitor;

  static bool udev_input_poll_hotplug_available(struct udev_monitor *dev);
  void udev_initial_gunsList();
  bool udev_addGun(struct udev_device *dev, Window* window, bool needGunBorder);
  bool udev_removeGun(struct udev_device *dev, Window* window);
  void udev_closeGun(Gun* gun);
#elif WIN32
	static bool mMessageHookRegistered;	
	static void WindowsMessageHook(void* userdata, void* hWnd, unsigned int message, Uint64 wParam, Sint64 lParam);
	void enableRawInputCapture(bool enable);
#endif
};

#endif // ES_CORE_GUNMANAGER_H
