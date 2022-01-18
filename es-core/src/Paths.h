#pragma once

#include <string>

class Paths
{
public:
	Paths();

	// Root storage
	static std::string& getRootPath() { return getInstance()->mRootPath; }

	// config storage
	static std::string& getSystemConfFilePath() { return getInstance()->mSystemConfFilePath; }
	
	// Logs
	static std::string& getLogPath() { return getInstance()->mLogPath; }

	// Screenshots
	static std::string& getScreenShotPath() { return getInstance()->mScreenShotsPath; } 
																		
	// Savestates
	static std::string& getSavesPath() { return getInstance()->mSaveStatesPath; } 
																   
	// Music
	static std::string& getMusicPath() { return getInstance()->mMusicPath; }
	static std::string& getUserMusicPath() { return getInstance()->mUserMusicPath; }

	// Themes
	static std::string& getThemesPath() { return getInstance()->mThemesPath; }
	static std::string& getUserThemesPath() { return getInstance()->mUserThemesPath; }

	// Keyboard mappings
	static std::string& getKeyboardMappingsPath() { return getInstance()->mKeyboardMappingsPath; } 
	static std::string& getUserKeyboardMappingsPath() { return getInstance()->mUserKeyboardMappingsPath; } 

	// Decorations
	static std::string& getDecorationsPath() { return getInstance()->mDecorationsPath; } 
	static std::string& getUserDecorationsPath() { return getInstance()->mUserDecorationsPath; } 

	// Shaders
	static std::string& getShadersPath() { return getInstance()->mShadersPath; } 
	static std::string& getUserShadersPath() { return getInstance()->mUserShadersPath; } 
																						
	// Retroachivement sounds
	static std::string& getRetroachivementSounds() { return getInstance()->mRetroachivementSounds; } 
	static std::string& getUserRetroachivementSounds() { return getInstance()->mUserRetroachivementSounds; } 

	// Emulationstation
	static std::string& getEmulationStationPath() { return getInstance()->mEmulationStationPath; }
	static std::string& getUserEmulationStationPath() { return getInstance()->mUserEmulationStationPath; }

	static std::string& getTimeZonesPath() { return getInstance()->mTimeZonesPath; }
		
	static std::string& getVersionInfoPath() { return getInstance()->mVersionInfoPath; }
	static std::string& getUserManualPath() { return getInstance()->mUserManualPath; }

	static std::string& getKodiPath() { return getInstance()->mKodiPath; }
	
	static std::string& getHomePath();
	static void setHomePath(const std::string& _path);

	static std::string& getExePath();
	static void setExePath(const std::string& _path);

	static std::string findEmulationStationFile(const std::string& fileName);

private:
	static Paths* getInstance() 
	{
		if (_instance == nullptr)
			_instance = new Paths();

		return _instance;
	}

	static Paths* _instance;

	void loadCustomConfiguration(bool overridesOnly);

	std::string mRootPath;
	std::string mSystemConfFilePath;
	std::string mLogPath;
	std::string mScreenShotsPath;
	std::string mSaveStatesPath;
	std::string mMusicPath;
	std::string mUserMusicPath;
	std::string mThemesPath;
	std::string mUserThemesPath;
	std::string mKeyboardMappingsPath;
	std::string mUserKeyboardMappingsPath;
	std::string mDecorationsPath;
	std::string mUserDecorationsPath;
	std::string mShadersPath;
	std::string mUserShadersPath;
	std::string mEmulationStationPath;
	std::string mUserEmulationStationPath;

	std::string mTimeZonesPath;
	std::string mRetroachivementSounds;
	std::string mUserRetroachivementSounds;

	std::string mUserManualPath;
	std::string mVersionInfoPath;
	std::string mKodiPath;	
};
