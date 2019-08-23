#include "AudioManager.h"

#include "Log.h"
#include "Settings.h"
#include "Sound.h"
#include <SDL.h>
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "SystemConf.h"
#include "id3v2lib/include/id3v2lib.h"

#ifdef WIN32
#include <time.h>
#else
#include <unistd.h>
#endif

std::vector<std::shared_ptr<Sound>> AudioManager::sSoundVector;
std::shared_ptr<AudioManager> AudioManager::sInstance;
Mix_Music* AudioManager::currentMusic = NULL;

AudioManager::AudioManager()
{
	init();
}

AudioManager::~AudioManager()
{
	//stop all playback
	stop();
	stopMusic();

	// Stop playing all Sounds & reload them 
	for (unsigned int i = 0; i < sSoundVector.size(); i++)
		sSoundVector[i]->deinit();

	Mix_HookMusicFinished(nullptr);
	Mix_HaltMusic();

	//completely tear down SDL audio. else SDL hogs audio resources and emulators might fail to start...
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

std::shared_ptr<AudioManager> & AudioManager::getInstance()
{
	//check if an AudioManager instance is already created, if not create one
	if (sInstance == nullptr) // 	if (!Settings::getInstance()->getBool("EnableSounds"))
		sInstance = std::shared_ptr<AudioManager>(new AudioManager);

	return sInstance;
}

void AudioManager::init()
{
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
	{
		LOG(LogError) << "Error initializing SDL audio!\n" << SDL_GetError();
		return;
	}

	// Stop playing all Sounds & reload them 
	for (unsigned int i = 0; i < sSoundVector.size(); i++)
	{
		if (sSoundVector.at(i)->isPlaying())
		{
			sSoundVector[i]->stop();
			sSoundVector[i]->init();
		}
	}

	//Open the audio device and pause
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) < 0)
		LOG(LogError) << "MUSIC Error - Unable to open SDLMixer audio: " << SDL_GetError() << std::endl;
	else
		LOG(LogInfo) << "SDL AUDIO Initialized";
}


void AudioManager::deinit()
{
	sInstance = NULL;
}

void AudioManager::registerSound(std::shared_ptr<Sound> & sound)
{
	getInstance();
	sSoundVector.push_back(sound);
}

void AudioManager::unregisterSound(std::shared_ptr<Sound> & sound)
{
	getInstance();
	for (unsigned int i = 0; i < sSoundVector.size(); i++)
	{
		if (sSoundVector.at(i) == sound)
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
}

void AudioManager::stop()
{
	// Stop playing all Sounds
	for (unsigned int i = 0; i < sSoundVector.size(); i++)
		if (sSoundVector.at(i)->isPlaying())
			sSoundVector[i]->stop();
}

// batocera - per system music folder
void AudioManager::setName(std::string newname)
{
	mSystem = newname;
}

// batocera
void AudioManager::getMusicIn(const std::string &path, std::vector<std::string>& all_matching_files)
{
	if (!Utils::FileSystem::isDirectory(path))
		return;

	bool anySystem = !Settings::getInstance()->getBool("audio.persystem");

	auto dirContent = Utils::FileSystem::getDirContent(path);
	for (auto it = dirContent.cbegin(); it != dirContent.cend(); ++it)
	{
		if (Utils::FileSystem::isDirectory(*it))
		{
			if (*it == "." || *it == "..")
				continue;

			if (anySystem || mSystem == Utils::FileSystem::getFileName(*it))
				getMusicIn(*it, all_matching_files);
		}
		else
		{
			std::string extension = Utils::String::toLower(Utils::FileSystem::getExtension(*it));
			if (extension == ".mp3" || extension == ".ogg")
				all_matching_files.push_back(*it);
		}
	}
}

// batocera
void AudioManager::playRandomMusic(bool continueIfPlaying) {
	// check in User music directory
	std::vector<std::string> musics;
	getMusicIn("/userdata/music", musics);

	// check in system sound directory
	if (musics.empty())
		getMusicIn("/usr/share/batocera/music", musics);

	// check in .emulationstation/music directory
	if (musics.empty())
		getMusicIn(Utils::FileSystem::getHomePath() + "/.emulationstation/music", musics);

	if (musics.empty())
		return;

#if defined(WIN32)
	srand(time(NULL) % getpid());
#else
	srand(time(NULL) % getpid() + getppid());
#endif

	int randomIndex = rand() % musics.size();

	// continue playing ?
	if (currentMusic != NULL && continueIfPlaying)
		return;

	playMusic(musics.at(randomIndex));
	Mix_HookMusicFinished(AudioManager::musicEnd_callback);

	setSongName(musics.at(randomIndex));
}

void AudioManager::playMusic(std::string path)
{
	// free the previous music
	stopMusic();

	// load a new music
	currentMusic = Mix_LoadMUS(path.c_str());
	if (currentMusic == NULL)
	{
		LOG(LogError) << Mix_GetError() << " for " << path;
		return;
	}

	if (Mix_FadeInMusic(currentMusic, 1, 1000) == -1)
	{
		stopMusic();
		return;
	}

	Mix_HookMusicFinished(AudioManager::musicEnd_callback);
}


// batocera
void AudioManager::musicEnd_callback()
{
	AudioManager::getInstance()->playRandomMusic(false);
}

// batocera
void AudioManager::stopMusic()
{
	if (currentMusic == NULL)
		return;

	Mix_HookMusicFinished(nullptr);
	
	// Fade-out is nicer on Batocera!
	while (!Mix_FadeOutMusic(500))
		SDL_Delay(100);	
	
	Mix_FreeMusic(currentMusic);
	Mix_HaltMusic();

	currentMusic = NULL;
}

// batocera
void AudioManager::setSongName(std::string song)
{
	if (song.empty()) {
		mCurrentSong = "";
		return;
	}

	if (song == mCurrentSong)
		return;

	// First, start with an ID3 v1 tag
	FILE *f;
	struct {
		char tag[3];	// i.e. "TAG"
		char title[30];
		char artist[30];
		char album[30];
		char year[4];
		char comment[30];
		unsigned char genre;
	} info;

	if (!(f = fopen(song.c_str(), "r"))) {
		LOG(LogError) << "Error AudioManager opening " << song;
		return;
	}
	if (fseek(f, -128, SEEK_END) < 0) {
		LOG(LogError) << "Error AudioManager seeking " << song;
		return;
	}
	if (fread(&info, sizeof(info), 1, f) != 1) {
		LOG(LogError) << "Error AudioManager reading " << song;
		return;
	}
	if (strncmp(info.tag, "TAG", 3) == 0) {
		mCurrentSong = info.title;
		return;
	}

	// Then let's try with an ID3 v2 tag
#define MAX_STR_SIZE 60 // Empiric max size of a MP3 title
	ID3v2_tag* tag = load_tag(song.c_str());
	if (tag != NULL) {
		ID3v2_frame* title_frame = tag_get_title(tag);
		ID3v2_frame_text_content* title_content = parse_text_frame_content(title_frame);
		if (title_content->size < MAX_STR_SIZE)
			title_content->data[title_content->size] = '\0';
		if ((strlen(title_content->data) > 3) && (strlen(title_content->data) < MAX_STR_SIZE)) {
			mCurrentSong = title_content->data;
			return;
		}
	}

	// Otherwise, let's just use the filename
	mCurrentSong = Utils::FileSystem::getFileName(song.c_str());
}
