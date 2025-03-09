#include "Settings.h"

#include "utils/FileSystemUtil.h"
#include "Log.h"
#include "Scripting.h"
#include "utils/Platform.h"
#include <pugixml/src/pugixml.hpp>
#include <algorithm>
#include <vector>
#include "utils/StringUtil.h"
#include "Paths.h"

Settings* Settings::sInstance = NULL;
static std::string mEmptyString = "";
Delegate<ISettingsChangedEvent> Settings::settingChanged;

IMPLEMENT_STATIC_BOOL_SETTING(DebugText, false)
IMPLEMENT_STATIC_BOOL_SETTING(DebugImage, false)
IMPLEMENT_STATIC_BOOL_SETTING(DebugGrid, false)
IMPLEMENT_STATIC_BOOL_SETTING(DebugMouse, false)
IMPLEMENT_STATIC_BOOL_SETTING(ShowControllerActivity, true)
IMPLEMENT_STATIC_BOOL_SETTING(ShowControllerBattery, true)
IMPLEMENT_STATIC_BOOL_SETTING(DrawClock, true)
IMPLEMENT_STATIC_BOOL_SETTING(ClockMode12, false)
IMPLEMENT_STATIC_BOOL_SETTING(DrawFramerate, false)
IMPLEMENT_STATIC_BOOL_SETTING(VolumePopup, true)
IMPLEMENT_STATIC_BOOL_SETTING(BackgroundMusic, true)
IMPLEMENT_STATIC_BOOL_SETTING(VSync, true)
IMPLEMENT_STATIC_BOOL_SETTING(PreloadMedias, false)
IMPLEMENT_STATIC_BOOL_SETTING(IgnoreLeadingArticles, false)
IMPLEMENT_STATIC_BOOL_SETTING(ShowFoldersFirst, true)
IMPLEMENT_STATIC_BOOL_SETTING(ScrollLoadMedias, false)
IMPLEMENT_STATIC_INT_SETTING(ScreenSaverTime, 5 * 60 * 1000)

#if WIN32
IMPLEMENT_STATIC_BOOL_SETTING(ShowNetworkIndicator, false)
#else
IMPLEMENT_STATIC_BOOL_SETTING(ShowNetworkIndicator, true)
#endif

void Settings::updateCachedSetting(const std::string& name)
{
	UPDATE_STATIC_BOOL_SETTING_EX("audio.bgmusic", BackgroundMusic)
	UPDATE_STATIC_BOOL_SETTING(DebugText)
	UPDATE_STATIC_BOOL_SETTING(DebugImage)
	UPDATE_STATIC_BOOL_SETTING(DebugGrid)
	UPDATE_STATIC_BOOL_SETTING(DebugMouse)
	UPDATE_STATIC_BOOL_SETTING(ShowControllerActivity)
	UPDATE_STATIC_BOOL_SETTING(ShowControllerBattery)
	UPDATE_STATIC_BOOL_SETTING(ShowNetworkIndicator)
	UPDATE_STATIC_BOOL_SETTING(DrawClock)
	UPDATE_STATIC_BOOL_SETTING(ClockMode12)
	UPDATE_STATIC_BOOL_SETTING(DrawFramerate)
	UPDATE_STATIC_BOOL_SETTING(ScrollLoadMedias)
	UPDATE_STATIC_BOOL_SETTING(VolumePopup)
	UPDATE_STATIC_BOOL_SETTING(VSync)
	UPDATE_STATIC_BOOL_SETTING(PreloadMedias)
	UPDATE_STATIC_BOOL_SETTING(IgnoreLeadingArticles)		
	UPDATE_STATIC_BOOL_SETTING(ShowFoldersFirst)
	UPDATE_STATIC_INT_SETTING(ScreenSaverTime)

	if (mLoaded)
		settingChanged.invoke([name](ISettingsChangedEvent* c) { c->onSettingChanged(name); });
}

// these values are NOT saved to es_settings.xml
// since they're set through command-line arguments, and not the in-program settings menu
std::vector<const char*> settings_dont_save {
	{ "Debug" },
	{ "DebugText" },
	{ "DebugImage" },
	{ "DebugGrid" },
	{ "DebugMouse" },
	{ "ForceKid" },
	{ "ForceKiosk" },
	{ "IgnoreGamelist" },
	{ "HideConsole" },
	{ "ShowExit" },
	{ "ExitOnRebootRequired" },
	{ "AlternateSplashScreen" },
	{ "SplashScreen" },
	{ "SplashScreenProgress" },
	// { "VSync" },
	{ "FullscreenBorderless" },
	{ "Windowed" },
	{ "WindowWidth" },
	{ "WindowHeight" },
	{ "ScreenWidth" },
	{ "ScreenHeight" },
	{ "ScreenOffsetX" },
	{ "ScreenOffsetY" },
	{ "ScreenRotate" },
	{ "MonitorID" },
};

Settings::Settings() : mLoaded(false)
{
	setDefaults();
	loadFile();
	mLoaded = true;
}

Settings* Settings::getInstance()
{
	if(sInstance == NULL)
		sInstance = new Settings();

	return sInstance;
}

void Settings::setDefaults()
{
	mWasChanged = false;
	mBoolMap.clear();
	mIntMap.clear();

	mBoolMap["BackgroundJoystickInput"] = false;
	mBoolMap["ParseGamelistOnly"] = false;
	mBoolMap["ShowHiddenFiles"] = false;
	mBoolMap["ShowParentFolder"] = true;
	mBoolMap["IgnoreLeadingArticles"] = Settings::_IgnoreLeadingArticles;
	mBoolMap["ShowFoldersFirst"] = Settings::_ShowFoldersFirst;
	mBoolMap["DrawFramerate"] = false;
	mBoolMap["ScrollLoadMedias"] = false;	
	mBoolMap["ShowExit"] = true;
	mBoolMap["ExitOnRebootRequired"] = false;
	mBoolMap["Windowed"] = false;
	mBoolMap["SplashScreen"] = true;
	mStringMap["AlternateSplashScreen"] = "";
	mBoolMap["SplashScreenProgress"] = true;
	mBoolMap["StartupOnGameList"] = false;
	mStringMap["StartupSystem"] = "lastsystem";

#if WIN32
	mBoolMap["HidJoysticks"] = true;
	mBoolMap["ShowOnlyExit"] = true;
	mBoolMap["FullscreenBorderless"] = true;
#else
	mBoolMap["ShowOnlyExit"] = false;
	mBoolMap["FullscreenBorderless"] = false;
#endif
	mBoolMap["TTS"] = false;

	mIntMap["MonitorID"] = -1;

    mBoolMap["UseOSK"] = true; // on screen keyboard
    mBoolMap["DrawClock"] = Settings::_DrawClock;
	mBoolMap["ClockMode12"] = Settings::_ClockMode12;
	mBoolMap["ShowControllerNotifications"] = true;	
	mBoolMap["ShowControllerActivity"] = Settings::_ShowControllerActivity;
	mBoolMap["ShowControllerBattery"] = Settings::_ShowControllerBattery;
    mIntMap["SystemVolume"] = 95;
    mBoolMap["Overscan"] = false;
    mStringMap["Language"] = "en_US";
    mStringMap["INPUT P1"] = "DEFAULT";
    mStringMap["INPUT P2"] = "DEFAULT";
    mStringMap["INPUT P3"] = "DEFAULT";
    mStringMap["INPUT P4"] = "DEFAULT";
    mStringMap["INPUT P5"] = "DEFAULT";
    mStringMap["Overclock"] = "none";

	mBoolMap["VSync"] = Settings::_VSync;
	mStringMap["FolderViewMode"] = "never";
	mStringMap["HiddenSystems"] = "";

	mBoolMap["PublicWebAccess"] = false;	
	mBoolMap["FirstJoystickOnly"] = false;
    mBoolMap["EnableSounds"] = false;
	mBoolMap["ShowHelpPrompts"] = true;
	mBoolMap["ScrapeRatings"] = true;
	mBoolMap["ScrapeNames"] = true;	
	mBoolMap["ScrapeDescription"] = true;
	mBoolMap["ScrapePadToKey"] = true;
	mBoolMap["ScrapeOverWrite"] = true;	
	mBoolMap["IgnoreGamelist"] = false;
	mBoolMap["HideConsole"] = true;
	mBoolMap["QuickSystemSelect"] = true;
	mBoolMap["MoveCarousel"] = true;
	mBoolMap["SaveGamelistsOnExit"] = true;
	mStringMap["ShowBattery"] = "text";
	mBoolMap["CheckBiosesAtLaunch"] = true;
	mBoolMap["RemoveMultiDiskContent"] = true;

	mBoolMap["ShowNetworkIndicator"] = Settings::_ShowNetworkIndicator;

	mBoolMap["Debug"] = false;

	mBoolMap["InvertButtons"] = false;

	mBoolMap["GameOptionsAtNorth"] = false;
	mBoolMap["LoadEmptySystems"] = false;
	mBoolMap["HideUniqueGroups"] = true;
	mBoolMap["DrawGunCrosshair"] = true;
	
	mIntMap["RecentlyScrappedFilter"] = 3;
	
	mIntMap["ScreenSaverTime"] = Settings::_ScreenSaverTime;
	mIntMap["ScraperResizeWidth"] = 640;
	mIntMap["ScraperResizeHeight"] = 0;

#if defined(_WIN32) || defined(TINKERBOARD) || defined(X86) || defined(X86_64) || defined(ODROIDN2) || defined(ODROIDC2) || defined(ODROIDXU4) || defined(RPI4)
	// Boards > 1Gb RAM
	mIntMap["MaxVRAM"] = 256;
#elif defined(ODROIDGOA) || defined(GAMEFORCE) || defined(RK3326) || defined(RPIZERO2) || defined(RPI2) || defined(RPI3) || defined(ROCKPRO64)
	// Boards with 1Gb RAM
	mIntMap["MaxVRAM"] = 128;
#elif defined(_RPI_)
	// Rpi 0, 1
	mIntMap["MaxVRAM"] = 128;
#else
	// Other boards
	mIntMap["MaxVRAM"] = 100;
#endif

	mStringMap["TransitionStyle"] = "auto";
	mStringMap["GameTransitionStyle"] = "auto";

	mStringMap["ThemeSet"] = "";
	mStringMap["ScreenSaverBehavior"] = "dim";
	mStringMap["GamelistViewStyle"] = "automatic";

	mStringMap["Scraper"] = "ScreenScraper";
	mStringMap["ScrapperImageSrc"] = "ss";
	mStringMap["ScrapperThumbSrc"] = "box-2D";
	mStringMap["ScrapperLogoSrc"] = "wheel";
	mBoolMap["ScrapeVideos"] = false;
	mBoolMap["ScrapeShortTitle"] = false;

	mBoolMap["ScreenSaverDateTime"] = false;
	mStringMap["ScreenSaverDateFormat"] = "%Y-%m-%d";
	mStringMap["ScreenSaverTimeFormat"] = "%H:%M:%S";
	mBoolMap["ScreenSaverMarquee"] = true;
	mBoolMap["ScreenSaverControls"] = true;
	mStringMap["ScreenSaverGameInfo"] = "never";
	mBoolMap["StretchVideoOnScreenSaver"] = false;
	mStringMap["PowerSaverMode"] = "default"; 

	mBoolMap["StopMusicOnScreenSaver"] = true;

	mBoolMap["RetroachievementsMenuitem"] = true;
	mIntMap["ScreenSaverSwapImageTimeout"] = 10000;
	mBoolMap["SlideshowScreenSaverStretch"] = false;
	mBoolMap["SlideshowScreenSaverCustomImageSource"] = false;
	mStringMap["SlideshowScreenSaverImageFilter"] = ".png,.jpg";
	mBoolMap["SlideshowScreenSaverRecurse"] = false;
	mBoolMap["SlideshowScreenSaverGameName"] = true;
	mStringMap["ScreenSaverDecorations"] = "systems";

	mBoolMap["ShowCheevosIcon"] = true;

	mBoolMap["ShowWheelIconOnGames"] = true;
	mBoolMap["ShowGunIconOnGames"] = true;
	mBoolMap["ShowTrackballIconOnGames"] = true;
	mBoolMap["ShowSpinnerIconOnGames"] = true;

	mBoolMap["SlideshowScreenSaverCustomVideoSource"] = false;
	mStringMap["SlideshowScreenSaverVideoFilter"] = ".mp4,.avi";
	mBoolMap["SlideshowScreenSaverVideoRecurse"] = false;

	// This setting only applies to raspberry pi but set it for all platforms so
	// we don't get a warning if we encounter it on a different platform
	mBoolMap["VideoOmxPlayer"] = false;

	#if defined(_RPI_)
		// we're defaulting to OMX Player for full screen video on the Pi
		mBoolMap["ScreenSaverOmxPlayer"] = true;
	#else
		mBoolMap["ScreenSaverOmxPlayer"] = false;
	#endif

	mIntMap["ScreenSaverSwapVideoTimeout"] = 30000;

	mBoolMap["VideoAudio"] = true;
	mBoolMap["ScreenSaverVideoMute"] = false;
	mBoolMap["VideoLowersMusic"] = true;
	mBoolMap["VolumePopup"] = Settings::_VolumePopup;

	mIntMap["MusicVolume"] = 128;

	// Audio out device for Video playback using OMX player.
	mStringMap["OMXAudioDev"] = "both";
	mStringMap["CollectionSystemsAuto"] = "all,favorites"; // 2players,4players,favorites,recent
	mStringMap["CollectionSystemsCustom"] = "";
	mBoolMap["SortAllSystems"] = true; 
	mStringMap["SortSystems"] = "manufacturer";
	
	mStringMap["UseCustomCollectionsSystemEx"] = "";
	
	mBoolMap["HiddenSystemsShowGames"] = true;
	mBoolMap["CollectionShowSystemInfo"] = true;
	mBoolMap["FavoritesFirst"] = false;

	mBoolMap["LocalArt"] = false;
	mBoolMap["WebServices"] = false;

	// Audio out device for volume control
	#ifdef _RPI_
		mStringMap["AudioDevice"] = "PCM";
	#else
		mStringMap["AudioDevice"] = "Master";
	#endif

	mStringMap["AudioCard"] = "default";
	mStringMap["UIMode"] = "Full";
	mStringMap["UIMode_passkey"] = "aaaba"; 
	mBoolMap["ForceKiosk"] = false;
	mBoolMap["ForceKid"] = false;
	mBoolMap["ForceDisableFilters"] = false;

	mStringMap["ThemeColorSet"] = "";
	mStringMap["ThemeIconSet"] = "";
	mStringMap["ThemeMenu"] = "";
	mStringMap["ThemeSystemView"] = "";
	mStringMap["ThemeGamelistView"] = "";
	mStringMap["ThemeRegionName"] = "";
	mStringMap["DefaultGridSize"] = "";

	mBoolMap["ThreadedLoading"] = true;
	mBoolMap["AsyncImages"] = true;
	mBoolMap["PreloadUI"] = false;
	mBoolMap["PreloadMedias"] = Settings::_PreloadMedias;
	mBoolMap["OptimizeVRAM"] = true;
	mBoolMap["OptimizeVideo"] = true;

	mBoolMap["ShowFilenames"] = false;

#if defined(_WIN32) || defined(X86) || defined(X86_64)
	mBoolMap["HideWindow"] = false;
#else
	mBoolMap["HideWindow"] = true;
#endif

	mBoolMap["HideWindowFullReinit"] = false;

	mIntMap["WindowWidth"]   = 0;
	mIntMap["WindowHeight"]  = 0;
	mIntMap["ScreenWidth"]   = 0;
	mIntMap["ScreenHeight"]  = 0;
	mIntMap["ScreenOffsetX"] = 0;
	mIntMap["ScreenOffsetY"] = 0;
	mIntMap["ScreenRotate"]  = 0;

	mStringMap["INPUT P1NAME"] = "DEFAULT";
	mStringMap["INPUT P2NAME"] = "DEFAULT";
	mStringMap["INPUT P3NAME"] = "DEFAULT";
	mStringMap["INPUT P4NAME"] = "DEFAULT";
	mStringMap["INPUT P5NAME"] = "DEFAULT";
	mStringMap["INPUT P6NAME"] = "DEFAULT";
	mStringMap["INPUT P7NAME"] = "DEFAULT";
	mStringMap["INPUT P8NAME"] = "DEFAULT";

	// Audio settings
	mBoolMap["audio.bgmusic"] = Settings::_BackgroundMusic;
	mBoolMap["audio.persystem"] = false;
	mBoolMap["audio.display_titles"] = true;
	mBoolMap["audio.thememusics"] = true;
	mIntMap["audio.display_titles_time"] = 10;

	mBoolMap["NetPlayCheckIndexesAtStart"] = false;
	mBoolMap["NetPlayAutomaticallyCreateLobby"] = false;
	mBoolMap["NetPlayShowOnlyRelayServerGames"] = false;
	mBoolMap["NetPlayShowMissingGames"] = false;
	
	mBoolMap["CheevosCheckIndexesAtStart"] = false;	

	mBoolMap["AllImagesAsync"] = true;

#if WIN32
	mBoolMap["updates.enabled"] = true;
	mBoolMap["global.retroachievements"] = false;
	mBoolMap["global.retroachievements.hardcore"] = false;
	mBoolMap["global.retroachievements.leaderboards"] = false;
	mBoolMap["global.retroachievements.verbose"] = false;
	mBoolMap["global.retroachievements.screenshot"] = false;
	mBoolMap["global.retroachievements.encore"] = false;
	mBoolMap["global.retroachievements.richpresence"] = false;

	mBoolMap["global.netplay_public_announce"] = true;
	mBoolMap["global.netplay"] = false;

	mBoolMap["kodi.enabled"] = false;
	mBoolMap["kodi.atstartup"] = false;
	mBoolMap["wifi.enabled"] = false;
#endif

	mFloatMap["GunMoveTolerence"] = 2.5;

	mDefaultBoolMap = mBoolMap;
	mDefaultIntMap = mIntMap;
	mDefaultFloatMap = mFloatMap;
	mDefaultStringMap = mStringMap;
}

template <typename K, typename V>
void saveMap(pugi::xml_node &node, std::map<K, V>& map, const char* type, std::map<K, V>& defaultMap, V defaultValue)
{
	for(auto iter = map.cbegin(); iter != map.cend(); iter++)
	{
		// key is on the "don't save" list, so don't save it
		if(std::find(settings_dont_save.cbegin(), settings_dont_save.cend(), iter->first) != settings_dont_save.cend())
			continue;

		auto def = defaultMap.find(iter->first);
		if (def != defaultMap.cend() && def->second == iter->second)
			continue;

		if (def == defaultMap.cend() && iter->second == defaultValue)
			continue;

		pugi::xml_node parent_node= node.append_child(type);
		parent_node.append_attribute("name").set_value(iter->first.c_str());
		parent_node.append_attribute("value").set_value(iter->second);
	}
}

bool Settings::saveFile()
{
	if (!mWasChanged)
		return false;

	mWasChanged = false;

	LOG(LogDebug) << "Settings::saveFile() : Saving Settings to file.";

	const std::string path = Paths::getUserEmulationStationPath() + "/es_settings.cfg";

	pugi::xml_document doc;

	pugi::xml_node config = doc.append_child("config"); // root element

	saveMap<std::string, bool>(config, mBoolMap, "bool", mDefaultBoolMap, false);
	saveMap<std::string, int>(config, mIntMap, "int", mDefaultIntMap, 0);
	saveMap<std::string, float>(config, mFloatMap, "float", mDefaultFloatMap, 0);

	//saveMap<std::string, std::string>(config, mStringMap, "string");
	for(auto iter = mStringMap.cbegin(); iter != mStringMap.cend(); iter++)
	{
		// key is on the "don't save" list, so don't save it
		if (std::find(settings_dont_save.cbegin(), settings_dont_save.cend(), iter->first) != settings_dont_save.cend())
			continue;

		// Value is not known, and empty, don't save it
		auto def = mDefaultStringMap.find(iter->first);
		if (def == mDefaultStringMap.cend() && iter->second.empty())
			continue;

		// Value is know and has default value, don't save it
		if (def != mDefaultStringMap.cend() && def->second == iter->second)
			continue;

		pugi::xml_node node = config.append_child("string");
		node.append_attribute("name").set_value(iter->first.c_str());
		node.append_attribute("value").set_value(iter->second.c_str());
	}

	doc.save_file(WINSTRINGW(path).c_str());

	Scripting::fireEvent("config-changed");
	Scripting::fireEvent("settings-changed");

	return true;
}

void Settings::loadFile()
{
	const std::string path = Paths::getUserEmulationStationPath() + "/es_settings.cfg";
	if(!Utils::FileSystem::exists(path))
		return;

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(WINSTRINGW(path).c_str());
	if(!result)
	{
		LOG(LogError) << "Could not parse Settings file!\n   " << result.description();
		return;
	}

	pugi::xml_node root = doc;

	pugi::xml_node config = doc.child("config");
	if (config)
		root = config;

	for(pugi::xml_node node = root.child("bool"); node; node = node.next_sibling("bool"))
		setBool(node.attribute("name").as_string(), node.attribute("value").as_bool());
	for(pugi::xml_node node = root.child("int"); node; node = node.next_sibling("int"))
		setInt(node.attribute("name").as_string(), node.attribute("value").as_int());
	for(pugi::xml_node node = root.child("float"); node; node = node.next_sibling("float"))
		setFloat(node.attribute("name").as_string(), node.attribute("value").as_float());
	for(pugi::xml_node node = root.child("string"); node; node = node.next_sibling("string"))
		setString(node.attribute("name").as_string(), node.attribute("value").as_string());
	
	// Migrate old preferences
	auto it = mBoolMap.find("UseCustomCollectionsSystem");
	if (it != mBoolMap.cend())
	{
		if (it->second == false)
			mStringMap["UseCustomCollectionsSystemEx"] = "false";

		mBoolMap.erase(it);
	}

	mWasChanged = false;
}

//Print a warning message if the setting we're trying to get doesn't already exist in the map, then return the value in the map.
#define SETTINGS_GETSET(type, mapName, getMethodName, setMethodName, defaultValue) type Settings::getMethodName(const std::string& name) \
{ \
	auto it = mapName.find(name); \
	if (it == mapName.cend()) \
		return defaultValue; \
\
	return it->second; \
} \
bool Settings::setMethodName(const std::string& name, type value) \
{ \
	if (mapName.count(name) == 0 || mapName[name] != value) { \
		mapName[name] = value; \
		if (std::find(settings_dont_save.cbegin(), settings_dont_save.cend(), name) == settings_dont_save.cend()) \
			mWasChanged = true; \
		updateCachedSetting(name); \
		return true; \
	} \
	return false; \
}

SETTINGS_GETSET(bool, mBoolMap, getBool, setBool, false);
SETTINGS_GETSET(int, mIntMap, getInt, setInt, 0);
SETTINGS_GETSET(float, mFloatMap, getFloat, setFloat, 0.0f);

std::string Settings::getString(const std::string& name)
{
	auto it = mStringMap.find(name);
	if (it == mStringMap.cend())
		return mEmptyString;

	return it->second;
}

bool Settings::setString(const std::string& name, const std::string& value)
{
	if (mStringMap.count(name) == 0 || mStringMap[name] != value)
	{
		if (value == "" && mStringMap.count(name) == 0)
			return false;

		mStringMap[name] = value;

		if (std::find(settings_dont_save.cbegin(), settings_dont_save.cend(), name) == settings_dont_save.cend())
			mWasChanged = true;

		updateCachedSetting(name);
		return true;
	}

	return false;
}
