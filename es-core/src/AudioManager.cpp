#include "AudioManager.h"

#include "Log.h"
#include "Settings.h"
#include "Sound.h"
#include <SDL.h>
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "SystemConf.h"
#include "id3v2lib/include/id3v2lib.h"
#include "ThemeData.h"

#ifdef _ENABLEEMUELEC
#include "platform.h"
#endif

#ifdef WIN32
#include <time.h>
#else
#include <unistd.h>
#endif

AudioManager* AudioManager::sInstance = NULL;
std::vector<std::shared_ptr<Sound>> AudioManager::sSoundVector;

AudioManager::AudioManager() : mInitialized(false), mCurrentMusic(nullptr), mMusicVolume(MIX_MAX_VOLUME), mVideoPlaying(false)
{
	init();
}

AudioManager::~AudioManager()
{
	deinit();
}

AudioManager* AudioManager::getInstance()
{
	//check if an AudioManager instance is already created, if not create one
	if (sInstance == nullptr)
		sInstance = new AudioManager();

	return sInstance;
}

bool AudioManager::isInitialized()
{
	if (sInstance == nullptr)
		return false;

	return sInstance->mInitialized;
}

void AudioManager::init()
{
	if (mInitialized)
		return;
	
	mMusicVolume = 0;
	mPlayingSystemThemeSong = "none";

	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
	{
		LOG(LogError) << "Error initializing SDL audio!\n" << SDL_GetError();
		return;
	}

	// Open the audio device and pause
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) < 0)
		LOG(LogError) << "MUSIC Error - Unable to open SDLMixer audio: " << SDL_GetError() << std::endl;
	else
	{
		LOG(LogInfo) << "SDL AUDIO Initialized";
		mInitialized = true;

		// Reload known sounds
		for (unsigned int i = 0; i < sSoundVector.size(); i++)
			sSoundVector[i]->init();
	}
}

void AudioManager::deinit()
{
	if (!mInitialized)
		return;

	LOG(LogDebug) << "AudioManager::deinit";

	mInitialized = false;

	//stop all playback
	stop();
	stopMusic();

	// Free known sounds from memory
	for (unsigned int i = 0; i < sSoundVector.size(); i++)
		sSoundVector[i]->deinit();

	Mix_HookMusicFinished(nullptr);
	Mix_HaltMusic();

#ifdef _ENABLEEMUELEC	
	LOG(LogInfo) << "Attempting to close SDL AUDIO";
	runSystemCommand("/emuelec/scripts/emuelec-utils audio alsa", "", nullptr); 
#endif

	//completely tear down SDL audio. else SDL hogs audio resources and emulators might fail to start...
	Mix_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);

	LOG(LogInfo) << "SDL AUDIO Deinitialized";
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
	LOG(LogWarning) << "AudioManager Error - tried to unregister a sound that wasn't registered!";
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

			if (anySystem || mSystemName == Utils::FileSystem::getFileName(*it))
				getMusicIn(*it, all_matching_files);
		}
		else
		{
			std::string extension = Utils::String::toLower(Utils::FileSystem::getExtension(*it));
			if (extension == ".mp3" || extension == ".ogg" || extension == ".flac"
				|| extension == ".wav" || extension == ".mod" || extension == ".xm"
				|| extension == ".stm" || extension == ".s3m" || extension == ".far"
				|| extension == ".it" || extension == ".669" || extension == ".mtm")
				all_matching_files.push_back(*it);
		}
	}
}

// batocera
void AudioManager::playRandomMusic(bool continueIfPlaying) 
{
	if (!Settings::getInstance()->getBool("audio.bgmusic"))
		return;
		
	std::vector<std::string> musics;

	// check in Theme music directory
	if (!mCurrentThemeMusicDirectory.empty())
		getMusicIn(mCurrentThemeMusicDirectory, musics);

	// check in User music directory
	if (musics.empty())
#ifdef _ENABLEEMUELEC
		getMusicIn("/storage/roms/BGM", musics);
#else
		getMusicIn("/userdata/music", musics);
#endif

	// check in system sound directory
	if (musics.empty())
#ifdef _ENABLEEMUELEC
		getMusicIn("/storage/.config/emuelec/BGM", musics);
#else
		getMusicIn("/usr/share/batocera/music", musics);
#endif

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
	if (mCurrentMusic != nullptr && continueIfPlaying)
		return;

	playMusic(musics.at(randomIndex));
	setSongName(musics.at(randomIndex));
	mPlayingSystemThemeSong = "";
}

void AudioManager::playMusic(std::string path)
{
	if (!mInitialized)
		return;

	// free the previous music
	stopMusic(false);

	if (!Settings::getInstance()->getBool("audio.bgmusic"))
		return;

	// load a new music
	mCurrentMusic = Mix_LoadMUS(path.c_str());
	if (mCurrentMusic == NULL)
	{
		LOG(LogError) << Mix_GetError() << " for " << path;
		return;
	}

	if (Mix_FadeInMusic(mCurrentMusic, 1, 1000) == -1)
	{
		stopMusic();
		return;
	}

	mCurrentMusicPath = path;
	Mix_HookMusicFinished(AudioManager::musicEnd_callback);
}

// batocera
void AudioManager::musicEnd_callback()
{
	if (!AudioManager::getInstance()->mPlayingSystemThemeSong.empty())
	{
		AudioManager::getInstance()->playMusic(AudioManager::getInstance()->mPlayingSystemThemeSong);
		return;
	}

	AudioManager::getInstance()->playRandomMusic(false);
}

// batocera
void AudioManager::stopMusic(bool fadeOut)
{
	if (mCurrentMusic == NULL)
		return;

	Mix_HookMusicFinished(nullptr);

	if (fadeOut)
	{
		// Fade-out is nicer on Batocera!
		while (!Mix_FadeOutMusic(500) && Mix_PlayingMusic())
			SDL_Delay(100);
	}

	Mix_HaltMusic();
	Mix_FreeMusic(mCurrentMusic);
	mCurrentMusicPath = "";
	mCurrentMusic = NULL;
}

// Fast string hash in order to use strings in switch/case
// How does this work? Look for Dan Bernstein hash on the internet
constexpr unsigned int sthash(const char *s, int off = 0)
{
	return !s[off] ? 5381 : (sthash(s, off+1)*33) ^ s[off];
}

// batocera
void AudioManager::setSongName(std::string song)
{
	if (song.empty())
	{
		mCurrentSong = "";
		return;
	}

	if (song == mCurrentSong)
		return;

	std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(song));
	// chiptunes mod song titles parsing
	if (ext == ".mod" || ext == ".s3m" || ext == ".stm" || ext == ".669"
			  || ext == ".mtm" || ext == ".far" || ext == ".xm" || ext == ".it" )
	{
		int title_offset;
		int title_break;
		struct {
			char title[108] = "";
		} info;
		switch (sthash(ext.c_str())) {
			case sthash(".mod"):
			case sthash(".stm"):
				title_offset = 0;
				title_break = 20;
				break;
			case sthash(".s3m"):
				title_offset = 0;
				title_break = 28;
				break;
			case sthash(".669"):
				title_offset = 0;
				title_break = 108;
				break;
			case sthash(".mtm"):
			case sthash(".it"):
				title_offset = 4;
				title_break = 20;
				break;
			case sthash(".far"):
				title_offset = 4;
				title_break = 40;
				break;
			case sthash(".xm"):
				title_offset = 17;
				title_break = 20;
				break;
			default:
				LOG(LogError) << "Error AudioManager unexpected case while loading mofile " << song;
				mCurrentSong = Utils::FileSystem::getStem(song.c_str());
				return;
		}

		FILE* file = fopen(song.c_str(), "r");
		if (file != NULL)
		{
			if (fseek(file, title_offset, SEEK_SET) < 0)
				LOG(LogError) << "Error AudioManager seeking " << song;
			else if (fread(&info, sizeof(info), 1, file) != 1)
				LOG(LogError) << "Error AudioManager reading " << song;
			else  {
				info.title[title_break] = '\0';
				mCurrentSong = info.title;
				fclose(file);
				return;
			}

			fclose(file);
		}
		else
			LOG(LogError) << "Error AudioManager opening modfile " << song;
	}

	// now only mp3 will be parsed for ID3: .ogg, .wav and .flac will display file name
	if (ext != ".mp3")
	{
		mCurrentSong = Utils::FileSystem::getStem(song.c_str());
		return;
	}

	LOG(LogDebug) << "AudioManager::setSongName";

	// First, start with an ID3 v1 tag	
	struct {
		char tag[3];	// i.e. "TAG"
		char title[30];
		char artist[30];
		char album[30];
		char year[4];
		char comment[30];
		unsigned char genre;
	} info;

	FILE* file = fopen(song.c_str(), "r");
	if (file != NULL)
	{
		if (fseek(file, -128, SEEK_END) < 0)
			LOG(LogError) << "Error AudioManager seeking " << song;
		else if (fread(&info, sizeof(info), 1, file) != 1)
			LOG(LogError) << "Error AudioManager reading " << song;
		else if (strncmp(info.tag, "TAG", 3) == 0) {
			mCurrentSong = info.title;
			fclose(file);
			return;
		}

		fclose(file);
	}
	else
		LOG(LogError) << "Error AudioManager opening mp3 file " << song;

	// Then let's try with an ID3 v2 tag
#define MAX_STR_SIZE 255 // Empiric max size of a MP3 title

	ID3v2_tag* tag = load_tag(song.c_str());
	if (tag != NULL)
	{
		ID3v2_frame* title_frame = tag_get_title(tag);
		if (title_frame != NULL)
		{
			ID3v2_frame_text_content* title_content = parse_text_frame_content(title_frame);
			if (title_content != NULL)
			{
				if (title_content->size < MAX_STR_SIZE)
					title_content->data[title_content->size] = '\0';

				if ((strlen(title_content->data) > 3) && (strlen(title_content->data) < MAX_STR_SIZE))
				{
					mCurrentSong = title_content->data;
					return;
				}
			}
		}
	}

	mCurrentSong = Utils::FileSystem::getStem(song.c_str());
}

void AudioManager::changePlaylist(const std::shared_ptr<ThemeData>& theme, bool force)
{
	if (!force && mSystemName == theme->getSystemThemeFolder())
		return;

	mSystemName = theme->getSystemThemeFolder();
	mCurrentThemeMusicDirectory = "";

	if (!Settings::getInstance()->getBool("audio.bgmusic"))
		return;

	const ThemeData::ThemeElement* elem = theme->getElement("system", "directory", "sound");

	if (Settings::getInstance()->getBool("audio.thememusics"))
	{
		if (elem && elem->has("path") && !Settings::getInstance()->getBool("audio.persystem"))
			mCurrentThemeMusicDirectory = elem->get<std::string>("path");

		std::string bgSound;

		elem = theme->getElement("system", "bgsound", "sound");
		if (elem && elem->has("path") && Utils::FileSystem::exists(elem->get<std::string>("path")))
		{
			bgSound = Utils::FileSystem::getCanonicalPath(elem->get<std::string>("path"));
			if (bgSound == mCurrentMusicPath)
				return;
		}

		// Found a music for the system
		if (!bgSound.empty())
		{
			mPlayingSystemThemeSong = bgSound;
			playMusic(bgSound);
			// setSongName(bgSound); ???
			return;
		}
	}
	
	if (force || !mPlayingSystemThemeSong.empty() || Settings::getInstance()->getBool("audio.persystem"))
		playRandomMusic(false);
}

void AudioManager::setVideoPlaying(bool state)
{
	if (sInstance == nullptr || !sInstance->mInitialized || !Settings::getInstance()->getBool("audio.bgmusic"))
		return;
	
	if (state && !Settings::getInstance()->getBool("VideoLowersMusic"))
		return;

	sInstance->mVideoPlaying = state;
}

int AudioManager::getMaxMusicVolume()
{
	int ret = (Settings::getInstance()->getInt("MusicVolume") * MIX_MAX_VOLUME) / 100;
	if (ret > MIX_MAX_VOLUME)
		return MIX_MAX_VOLUME;

	if (ret < 0)
		return 0;

	return ret;
}

void AudioManager::update(int deltaTime)
{
	if (sInstance == nullptr || !sInstance->mInitialized || !Settings::getInstance()->getBool("audio.bgmusic"))
		return;

	float deltaVol = deltaTime / 8.0f;

//	#define MINVOL 5

	int maxVol = getMaxMusicVolume();
	int minVol = maxVol / 20;
	if (maxVol > 0 && minVol == 0)
		minVol = 1;

	if (sInstance->mVideoPlaying && sInstance->mMusicVolume != minVol)
	{		
		if (sInstance->mMusicVolume > minVol)
		{
			sInstance->mMusicVolume -= deltaVol;
			if (sInstance->mMusicVolume < minVol)
				sInstance->mMusicVolume = minVol;
		}

		Mix_VolumeMusic((int)sInstance->mMusicVolume);
	}
	else if (!sInstance->mVideoPlaying && sInstance->mMusicVolume != maxVol)
	{
		if (sInstance->mMusicVolume < maxVol)
		{
			sInstance->mMusicVolume += deltaVol;
			if (sInstance->mMusicVolume > maxVol)
				sInstance->mMusicVolume = maxVol;
		}
		else
			sInstance->mMusicVolume = maxVol;

		Mix_VolumeMusic((int)sInstance->mMusicVolume);
	}
}
