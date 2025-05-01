#pragma once
#ifndef ES_CORE_AUDIO_MANAGER_H
#define ES_CORE_AUDIO_MANAGER_H

#include <SDL_audio.h>
#include <memory>
#include <vector>
#include "SDL_mixer.h"
#include <string> 
#include <iostream> 
#include <deque>
#include <math.h>

class Sound;
class ThemeData;

class AudioManager
{	
private:
	AudioManager();

	static std::vector<std::shared_ptr<Sound>> sSoundVector;
	static AudioManager* sInstance;
	
	Mix_Music* mCurrentMusic; 
	void getMusicIn(const std::string &path, std::vector<std::string>& all_matching_files); 
	void playMusic(std::string path);
	static void musicEnd_callback();	

	std::string mSystemName;			// Per system music folder
	std::string mCurrentSong;			// Song name displayed in pop-ups
	std::string mCurrentThemeMusicDirectory;
	std::string mCurrentMusicPath;                  //  Stores the full path of the currently playing song
	std::deque<std::string> mLastPlayed;            // Stores recently played songs

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
	std::string getCurrentSongPath() const;
	void stop();

	void playRandomMusic(bool continueIfPlaying = true);
	void stopMusic(bool fadeOut=true);
	
	inline const std::string getSongName() const { return mCurrentSong; }

	bool songNameChanged() { return mSongNameChanged; }
	void resetSongNameChangedFlag() { mSongNameChanged = false; }
	
	inline bool isSongPlaying() { return (mCurrentMusic != NULL); }

	void changePlaylist(const std::shared_ptr<ThemeData>& theme, bool force = false);

	virtual ~AudioManager();

	float mMusicVolume;
	int mVideoPlaying;

	static void setVideoPlaying(bool state);
	static void update(int deltaTime);

	static int getMaxMusicVolume();

	//  New function to get the full path of the currently playing song

private:
	void playSong(const std::string& song);
	void setSongName(const std::string& song);
	void addLastPlayed(const std::string& newSong, int totalMusic);
	bool songWasPlayedRecently(const std::string& song);

	bool mSongNameChanged;
};

#endif // ES_CORE_AUDIO_MANAGER_H
