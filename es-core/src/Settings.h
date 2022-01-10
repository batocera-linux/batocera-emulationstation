#pragma once
#ifndef ES_CORE_SETTINGS_H
#define ES_CORE_SETTINGS_H

#include <map>
#include <string>
#include <vector>

#define DEFINE_BOOL_SETTING(XX) static bool XX() { return Settings::getInstance()->getBool(#XX); }; static bool set##XX(bool val) { return Settings::getInstance()->setBool(#XX, val); };
#define DEFINE_INT_SETTING(XX) static int XX() { return Settings::getInstance()->getInt(#XX); }; static bool set##XX(int val) { return Settings::getInstance()->setInt(#XX, val); };
#define DEFINE_FLOAT_SETTING(XX) static float XX() { return Settings::getInstance()->getFloat(#XX); }; static bool set##XX(float val) { return Settings::getInstance()->setFloat(#XX, val); };
#define DEFINE_STRING_SETTING(XX) static std::string XX() { return Settings::getInstance()->getString(#XX); }; static bool set##XX(const std::string& val) { return Settings::getInstance()->setString(#XX, val); };

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

	static bool DebugText;
	static bool DebugImage;
	static bool DebugGrid;

	DEFINE_BOOL_SETTING(PreloadMedias)
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
	
	DEFINE_BOOL_SETTING(ShowControllerActivity)
	DEFINE_BOOL_SETTING(ShowControllerBattery)

	DEFINE_STRING_SETTING(HiddenSystems)
	DEFINE_STRING_SETTING(TransitionStyle)
	DEFINE_STRING_SETTING(GameTransitionStyle)		
	DEFINE_STRING_SETTING(PowerSaverMode)		

	DEFINE_INT_SETTING(RecentlyScrappedFilter)

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
};

#endif // ES_CORE_SETTINGS_H
