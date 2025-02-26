#pragma once
#ifndef ES_CORE_SETTINGS_H
#define ES_CORE_SETTINGS_H

#include <map>
#include <string>
#include <vector>
#include "utils/Delegate.h"

// Non-cached settings macros
#define DEFINE_BOOL_SETTING(XX) static bool XX() { return Settings::getInstance()->getBool(#XX); }; static bool set##XX(bool val) { return Settings::getInstance()->setBool(#XX, val); };
#define DEFINE_INT_SETTING(XX) static int XX() { return Settings::getInstance()->getInt(#XX); }; static bool set##XX(int val) { return Settings::getInstance()->setInt(#XX, val); };
#define DEFINE_FLOAT_SETTING(XX) static float XX() { return Settings::getInstance()->getFloat(#XX); }; static bool set##XX(float val) { return Settings::getInstance()->setFloat(#XX, val); };
#define DEFINE_STRING_SETTING(XX) static std::string XX() { return Settings::getInstance()->getString(#XX); }; static bool set##XX(const std::string& val) { return Settings::getInstance()->setString(#XX, val); };

// Cached static settings macros
#define DECLARE_STATIC_BOOL_SETTING(XX) \
private: \
static bool _##XX; \
public: \
static bool XX() { return _##XX; }; \
static bool set##XX(bool val) { _##XX = val; return Settings::getInstance()->setBool(#XX, val); };

#define DECLARE_STATIC_INT_SETTING(XX) \
private: \
static int _##XX; \
public: \
inline static int XX() { return _##XX; }; \
static bool set##XX(bool val) { _##XX = val; return Settings::getInstance()->setInt(#XX, val); };

// Cached static settings macros. To use in cpp to implement cached settings
#define IMPLEMENT_STATIC_BOOL_SETTING(XX, VAL) bool Settings::_##XX = VAL;
#define UPDATE_STATIC_BOOL_SETTING(XX) if (name == #XX) _##XX = getBool(#XX);
#define UPDATE_STATIC_BOOL_SETTING_EX(NAME, XX) if (name == NAME) _##XX = getBool(NAME);

#define IMPLEMENT_STATIC_INT_SETTING(XX, VAL) int Settings::_##XX = VAL;
#define UPDATE_STATIC_INT_SETTING(XX) if (name == #XX) _##XX = getInt(#XX);

class ISettingsChangedEvent
{
public:
	virtual void onSettingChanged(const std::string& name) = 0;
};

//This is a singleton for storing settings.
class Settings
{
public:
	static Settings* getInstance();

	void loadFile();
	bool saveFile();

	//You will get a warning if you try a get on a key that is not already present.
	bool getBool(const std::string& name);
	int getInt(const std::string& name);
	float getFloat(const std::string& name);
	std::string getString(const std::string& name);

	bool setBool(const std::string& name, bool value);
	bool setInt(const std::string& name, int value);
	bool setFloat(const std::string& name, float value);
	bool setString(const std::string& name, const std::string& value);

	std::map<std::string, std::string>& getStringMap() { return mStringMap; }

	// Cached settings using static fields. They must be implemented using IMPLEMENT_STATIC_xx_SETTING & updated with UPDATE_STATIC_xxx_SETTING
	DECLARE_STATIC_BOOL_SETTING(DebugText)
	DECLARE_STATIC_BOOL_SETTING(DebugImage)
	DECLARE_STATIC_BOOL_SETTING(DebugGrid)
	DECLARE_STATIC_BOOL_SETTING(DebugMouse)
	DECLARE_STATIC_BOOL_SETTING(DrawClock)
	DECLARE_STATIC_BOOL_SETTING(ShowControllerActivity)
	DECLARE_STATIC_BOOL_SETTING(ShowControllerBattery)
	DECLARE_STATIC_BOOL_SETTING(ShowNetworkIndicator)
	DECLARE_STATIC_BOOL_SETTING(DrawFramerate)
	DECLARE_STATIC_BOOL_SETTING(VolumePopup)
	DECLARE_STATIC_BOOL_SETTING(BackgroundMusic)
	DECLARE_STATIC_BOOL_SETTING(ClockMode12)
	DECLARE_STATIC_BOOL_SETTING(VSync)
	DECLARE_STATIC_BOOL_SETTING(PreloadMedias)
	DECLARE_STATIC_BOOL_SETTING(IgnoreLeadingArticles)
	DECLARE_STATIC_BOOL_SETTING(ShowFoldersFirst)
	DECLARE_STATIC_BOOL_SETTING(ScrollLoadMedias)
	DECLARE_STATIC_INT_SETTING(ScreenSaverTime);

	// Non-cached settings with only shortcut methods
	DEFINE_BOOL_SETTING(ShowHiddenFiles)
	DEFINE_BOOL_SETTING(HiddenSystemsShowGames)
	DEFINE_BOOL_SETTING(AllImagesAsync)
	DEFINE_BOOL_SETTING(IgnoreGamelist)
	DEFINE_BOOL_SETTING(SaveGamelistsOnExit)
	DEFINE_BOOL_SETTING(RemoveMultiDiskContent)	
	DEFINE_BOOL_SETTING(ParseGamelistOnly)
	DEFINE_BOOL_SETTING(ThreadedLoading)
	DEFINE_BOOL_SETTING(CheevosCheckIndexesAtStart)
	DEFINE_BOOL_SETTING(NetPlayCheckIndexesAtStart)
	DEFINE_BOOL_SETTING(NetPlayShowMissingGames)			
	DEFINE_BOOL_SETTING(LoadEmptySystems)		
	DEFINE_BOOL_SETTING(HideUniqueGroups)
	DEFINE_BOOL_SETTING(DrawGunCrosshair)
	DEFINE_STRING_SETTING(HiddenSystems)
	DEFINE_STRING_SETTING(TransitionStyle)
	DEFINE_STRING_SETTING(GameTransitionStyle)		
	DEFINE_STRING_SETTING(PowerSaverMode)		
	DEFINE_INT_SETTING(RecentlyScrappedFilter)

	static Delegate<ISettingsChangedEvent> settingChanged;

private:
	static Settings* sInstance;

	Settings();

	//Clear everything and load default values.
	void setDefaults();

	std::map<std::string, bool> mBoolMap;
	std::map<std::string, int> mIntMap;
	std::map<std::string, float> mFloatMap;
	std::map<std::string, std::string> mStringMap;

	bool mWasChanged;

	std::map<std::string, bool> mDefaultBoolMap;
	std::map<std::string, int> mDefaultIntMap;
	std::map<std::string, float> mDefaultFloatMap;
	std::map<std::string, std::string> mDefaultStringMap;	

	bool mLoaded;
	void updateCachedSetting(const std::string& name);
};

#endif // ES_CORE_SETTINGS_H
