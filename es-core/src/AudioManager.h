#pragma once
#ifndef ES_CORE_AUDIO_MANAGER_H
#define ES_CORE_AUDIO_MANAGER_H

#include <SDL_audio.h>
#include <memory>
#include <vector>
#include "Music.h"

class Sound;

class AudioManager
{
	static SDL_AudioSpec sAudioFormat;
	static std::vector<std::shared_ptr<Sound>> sSoundVector;
	static std::shared_ptr<AudioManager> sInstance;

	static void mixAudio(void *unused, Uint8 *stream, int len);

	AudioManager();

public:
	static std::shared_ptr<AudioManager> & getInstance();

	void init();
	void deinit();

	void registerSound(std::shared_ptr<Sound> & sound);
	void unregisterSound(std::shared_ptr<Sound> & sound);

	void play();
	void stop();

    void stopMusic();
    void resumeMusic();
    void playRandomMusic();
    void playCheckSound();

    virtual ~AudioManager();

private:
    std::shared_ptr<Music> getRandomMusic(std::string themeSoundDirectory);
};

#endif // ES_CORE_AUDIO_MANAGER_H
