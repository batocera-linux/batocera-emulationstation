#include "AudioManager.h"

#include "Log.h"
#include "Settings.h"
#include "Sound.h"
#include <SDL.h>
#include <unistd.h>
#include "utils/FileSystemUtil.h"

std::vector<std::shared_ptr<Sound>> AudioManager::sSoundVector;
SDL_AudioSpec AudioManager::sAudioFormat;
std::shared_ptr<AudioManager> AudioManager::sInstance;
Mix_Music* AudioManager::currentMusic = NULL;

void AudioManager::mixAudio(void* /*unused*/, Uint8 *stream, int len)
{
	bool stillPlaying = false;

	//initialize the buffer to "silence"
	SDL_memset(stream, 0, len);

	//iterate through all our samples
	std::vector<std::shared_ptr<Sound>>::const_iterator soundIt = sSoundVector.cbegin();
	while (soundIt != sSoundVector.cend())
	{
		std::shared_ptr<Sound> sound = *soundIt;
		if(sound->isPlaying())
		{
			//calculate rest length of current sample
			Uint32 restLength = (sound->getLength() - sound->getPosition());
			if (restLength > (Uint32)len) {
				//if stream length is smaller than smaple lenght, clip it
				restLength = len;
			}
			//mix sample into stream
			SDL_MixAudio(stream, &(sound->getData()[sound->getPosition()]), restLength, SDL_MIX_MAXVOLUME);
			if (sound->getPosition() + restLength < sound->getLength())
			{
				//sample hasn't ended yet
				stillPlaying = true;
			}
			//set new sound position. if this is at or beyond the end of the sample, it will stop automatically
			sound->setPosition(sound->getPosition() + restLength);
		}
		//advance to next sound
		++soundIt;
	}

	//we have processed all samples. check if some will still be playing
	if (!stillPlaying) {
		//no. pause audio till a Sound::play() wakes us up
		SDL_PauseAudio(1);
	}
}

AudioManager::AudioManager()
{
	init();
}

AudioManager::~AudioManager()
{
	deinit();
}

std::shared_ptr<AudioManager> & AudioManager::getInstance()
{
	//check if an AudioManager instance is already created, if not create one
	if (sInstance == nullptr && Settings::getInstance()->getBool("EnableSounds")) {
		sInstance = std::shared_ptr<AudioManager>(new AudioManager);
	}
	return sInstance;
}

void AudioManager::init()
{
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
	{
		LOG(LogError) << "Error initializing SDL audio!\n" << SDL_GetError();
		return;
	}

	//stop playing all Sounds
	for(unsigned int i = 0; i < sSoundVector.size(); i++)
	{
		if(sSoundVector.at(i)->isPlaying())
		{
			sSoundVector[i]->stop();
		}
	}

	//Set up format and callback. Play 16-bit stereo audio at 44.1Khz
	sAudioFormat.freq = 44100;
	sAudioFormat.format = AUDIO_S16;
	sAudioFormat.channels = 2;
	sAudioFormat.samples = 4096;
	sAudioFormat.callback = mixAudio;
	sAudioFormat.userdata = NULL;

	// batocera
	//Open the audio device and pause
	// don't initialized sdl_audio (for navigation sounds) while it takes the sound card (and we have no sound server)
	//if (SDL_OpenAudio(&sAudioFormat, NULL) < 0) {
	//	LOG(LogError) << "AudioManager Error - Unable to open SDL audio: " << SDL_GetError() << std::endl;
	//}

	// batocera : don't initialize globally, just as needed
	// mixer: Open the audio device and pause
        //if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) < 0) {
        //    LOG(LogError) << "MUSIC Error - Unable to open SDLMixer audio: " << SDL_GetError() << std::endl;
        //}
}

void AudioManager::deinit()
{
	//stop all playback
	stop();
	//completely tear down SDL audio. else SDL hogs audio resources and emulators might fail to start...
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	sInstance = NULL;

	// batocera music
	if(currentMusic != NULL){
	  stopMusic();
	}
}

void AudioManager::registerSound(std::shared_ptr<Sound> & sound)
{
	getInstance();
	sSoundVector.push_back(sound);
}

void AudioManager::unregisterSound(std::shared_ptr<Sound> & sound)
{
	getInstance();
	for(unsigned int i = 0; i < sSoundVector.size(); i++)
	{
		if(sSoundVector.at(i) == sound)
		{
			sSoundVector[i]->stop();
			sSoundVector.erase(sSoundVector.cbegin() + i);
			return;
		}
	}
	LOG(LogError) << "AudioManager Error - tried to unregister a sound that wasn't registered!";
}

void AudioManager::play()
{
	getInstance();

	//unpause audio, the mixer will figure out if samples need to be played...
	SDL_PauseAudio(0);
}

void AudioManager::stop()
{
	//stop playing all Sounds
	for(unsigned int i = 0; i < sSoundVector.size(); i++)
	{
		if(sSoundVector.at(i)->isPlaying())
		{
			sSoundVector[i]->stop();
		}
	}
	//pause audio
	SDL_PauseAudio(1);
}

// batocera
void AudioManager::getMusicIn(const std::string &path, std::vector<std::string>& all_matching_files) {
    Utils::FileSystem::stringList dirContent;
    std::string extension;

    if(!Utils::FileSystem::isDirectory(path)) {
        return;
    }

    dirContent = Utils::FileSystem::getDirContent(path);
    for (Utils::FileSystem::stringList::const_iterator it = dirContent.cbegin(); it != dirContent.cend(); ++it) {
      if(Utils::FileSystem::isRegularFile(*it)) {
	extension = it->substr(it->find_last_of(".") + 1);
	if(strcmp(extension.c_str(), "mp3") == 0 || strcmp(extension.c_str(), "ogg") == 0) {
	  all_matching_files.push_back(*it);
	}
      } else if(Utils::FileSystem::isDirectory(*it)) {
	if(strcmp(extension.c_str(), ".") != 0 && strcmp(extension.c_str(), "..") != 0) {
	  getMusicIn(*it, all_matching_files);
	}
      }
    }
}

// batocera
void AudioManager::playRandomMusic(bool continueIfPlaying) {
  // check in User music directory
  std::vector<std::string> musics;
  getMusicIn("/userdata/music", musics);
  if (musics.empty()) {
    // check in system sound directory
    getMusicIn("/usr/share/batocera/music", musics);
    if(musics.empty()) return;
  }
#if defined(WIN32)
  srand(time(NULL) % getpid());
#else
  srand(time(NULL) % getpid() + getppid());
#endif

  int randomIndex = rand() % musics.size();

  // continue playing ?
  if(currentMusic != NULL) {
    if(continueIfPlaying) return;
  }

  // free the previous music
  if(currentMusic != NULL) {
    Mix_FreeMusic(currentMusic);
  } else {
    // mixer: Open the audio device and pause
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) < 0) {
      LOG(LogError) << "MUSIC Error - Unable to open SDLMixer audio: " << SDL_GetError() << std::endl;
      return;
    }
  }

  // load a new music
  currentMusic = Mix_LoadMUS(musics.at(randomIndex).c_str());
  if(currentMusic == NULL) {
    LOG(LogError) <<Mix_GetError() << " for " << musics.at(randomIndex);
    Mix_CloseAudio();
    return;
  }

  if(Mix_PlayMusic(currentMusic, 1) == -1){
    Mix_FreeMusic(currentMusic);
    currentMusic = NULL;
    Mix_CloseAudio();
    return;
  }
  Mix_HookMusicFinished(AudioManager::musicEnd_callback);
}

// batocera
void AudioManager::musicEnd_callback() {
    AudioManager::getInstance()->playRandomMusic(false);
}

// batocera
void AudioManager::stopMusic() {
  if(currentMusic != NULL) {
    Mix_FreeMusic(currentMusic);
    Mix_CloseAudio();
    currentMusic = NULL;
  }
}
