#pragma once
#ifndef ES_CORE_GUNMANAGER_H
#define ES_CORE_GUNMANAGER_H

#include <string>
#include <vector>

#ifdef HAVE_UDEV
#include <libudev.h>
#endif

class InputConfig;
class Window;
union SDL_Event;

class Gun
{
	friend class GunManager;

public:
	Gun() 
	{
		mIndex = 0;
		mX = -1; 
		mY = -1;
		mLButtonDown = false;
		mRButtonDown = false;
		mStartButtonDown = false;
		mSelectButtonDown = false;
		mDPadUpButtonDown    = false;
		mDPadDownButtonDown  = false;
		mDPadLeftButtonDown  = false;
		mDPadRightButtonDown = false;
		mNeedBorders = false;
	}

	int index() { return mIndex; }
	std::string& name() { return mName; }

	float x();
	float y();

	bool isLButtonDown() { return mLButtonDown; }
	bool isRButtonDown() { return mRButtonDown; }
	bool isStartButtonDown() { return mStartButtonDown; }
	bool isSelectButtonDown() { return mSelectButtonDown; }
	bool isDPadUpButtonDown() { return mDPadUpButtonDown; }
	bool isDPadDownButtonDown() { return mDPadDownButtonDown; }
	bool isDPadLeftButtonDown() { return mDPadLeftButtonDown; }
	bool isDPadRightButtonDown() { return mDPadRightButtonDown; }

	bool needBorders() { return mNeedBorders; }

private:
	std::string mName;

	float mX;
	float mY;

	bool mLButtonDown;
	bool mRButtonDown;
  	bool mStartButtonDown;
  	bool mSelectButtonDown;
	bool mDPadUpButtonDown;
	bool mDPadDownButtonDown;
	bool mDPadLeftButtonDown;
	bool mDPadRightButtonDown;

	int mIndex;

#ifdef HAVE_UDEV
	std::string devpath;
	udev_device* dev;
	int fd;
#endif

	bool mNeedBorders;
};

//you should only ever instantiate one of these, by the way
class GunManager
{
public:
	GunManager();
	virtual ~GunManager();

	std::vector<Gun*>& getGuns() { return mGuns; }
	void updateGuns(Window* window);

private:
	bool updateGunPosition(Gun* gun);
	int readGunEvents(Gun* gun);

	std::vector<Gun*> mGuns;

#ifdef HAVE_UDEV
  struct udev *udev;
  struct udev_monitor *udev_monitor;

  static bool udev_input_poll_hotplug_available(struct udev_monitor *dev);
  void udev_initial_gunsList();
  bool udev_addGun(struct udev_device *dev, Window* window, bool needGunBorder);
  bool udev_removeGun(struct udev_device *dev, Window* window);
  void udev_closeGun(Gun* gun);
#endif
};

#endif // ES_CORE_GUNMANAGER_H
