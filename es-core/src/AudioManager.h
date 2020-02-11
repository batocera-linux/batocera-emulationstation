#pragma once
#ifndef ES_CORE_AUDIO_MANAGER_H
#define ES_CORE_AUDIO_MANAGER_H

#include <SDL_audio.h>
#include <memory>
#include <vector>
#include "SDL_mixer.h"
#include <string> // batocera
#include <iostream> // batocera

class Sound;
class ThemeData;

class AudioManager
{	
private:
	AudioManager();

	static std::vector<std::shared_ptr<Sound>> sSoundVector;
	static AudioManager* sInstance;
	
	Mix_Music* mCurrentMusic; // batocera
	void getMusicIn(const std::string &path, std::vector<std::string>& all_matching_files); // batocera
	void playMusic(std::string path);
	static void musicEnd_callback();	// batocera

	std::string mSystemName;				// batocera (per system music folder)
	std::string mCurrentSong;			// batocera (pop-up for SongName.cpp)
	std::string mCurrentThemeMusicDirectory;
	std::string mCurrentMusicPath;

	bool		mInitialized;
	std::string	mPlayingSystemThemeSong;

public:
	static AudioManager* getInstance();
	static bool isInitialized();
	
	void init();
	void deinit();

	void registerSound(std::shared_ptr<Sound> & sound);
	void unregisterSound(std::shared_ptr<Sound> & sound);

	void play();
	void stop();

	// batocera
	void playRandomMusic(bool continueIfPlaying = true);
	void stopMusic(bool fadeOut=true);
	
	inline const std::string getSongName() const { return mCurrentSong; }
	void setSongName(std::string song); 

	void changePlaylist(const std::shared_ptr<ThemeData>& theme, bool force = false);

	virtual ~AudioManager();

	float mMusicVolume;
	int mVideoPlaying;

	static void setVideoPlaying(bool state);
	static void update(int deltaTime);
};

#endif // ES_CORE_AUDIO_MANAGER_H
