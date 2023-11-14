#include "VolumeControl.h"

#include "math/Misc.h"
#include "Log.h"
#include "Settings.h"

#ifdef WIN32
#include <mmdeviceapi.h>
#endif

#ifdef _ENABLE_PULSE_
#include <thread>
#include <condition_variable>
#include <pulse/pulseaudio.h>
#include "utils/StringUtil.h"
#include "SystemConf.h"

class PulseAudioControl
{
#define DEFAULT_SINK_NAME "@DEFAULT_SINK@"

public:
	PulseAudioControl()
	{
		mContext = nullptr;
    	mMainLoop = nullptr;

	  	mReady   = 0;
		mMute    = 0;
		mVolume  = 100;

		mThread = new std::thread(&PulseAudioControl::run, this);
		WaitEvent();

		LOG(LogDebug) << "PulseAudioControl. Ready = " << mReady;
	}

	 ~PulseAudioControl()
	{
		exit();
	}

	bool isReady() { return mContext != nullptr && mReady; }

	int getVolume()
	{
		return mVolume;
	}

	void setVolume(int value, bool setSinkVolume = true)
	{
		mVolume = value;
		
		if (mContext == nullptr || !setSinkVolume)
			return;

		pa_operation* o = pa_context_get_sink_info_by_name(mContext, DEFAULT_SINK_NAME, set_sink_volume_callback, this);
      	if (o != NULL)
			pa_operation_unref(o);
	}

	void exit()
	{
		LOG(LogDebug) << "PulseAudioControl.exit";

		mReady = false;

		if(mThread != nullptr) {
		  if (mMainLoop != nullptr) {
		    pa_mainloop_quit(mMainLoop, 0);
		  }
		  mThread->join();
		  mThread = nullptr;
		}
	}

	void run()	
	{
		mMainLoop = pa_mainloop_new();
		pa_mainloop_api* pa_mlapi = pa_mainloop_get_api(mMainLoop);

  		pa_signal_init(pa_mlapi);

		mContext = pa_context_new(pa_mlapi, "EmulationStation");

		pa_context_set_state_callback(mContext, context_state_callback, this);
		pa_context_connect(mContext, nullptr, pa_context_flags::PA_CONTEXT_NOFLAGS, nullptr);

		int result = 0;
		pa_mainloop_run(mMainLoop, &result);

		pa_context_unref(mContext);
		pa_mainloop_free(mMainLoop);
		mMainLoop = nullptr;

		LOG(LogDebug) << "PulseAudioControl End Mainloop";
	}

private:
	static void quit(void* userdata, int code)
	{
		PulseAudioControl* pThis = (PulseAudioControl*)userdata;
	}

	static void simple_callback(pa_context *c, int success, void *userdata) 
	{
  		if (!success) 
		{
			LOG(LogError) << "PulseAudioControl Failure : " << pa_strerror(pa_context_errno(c));    		
			quit(userdata, 1);
  		}
	}

	static void get_sink_volume_callback(pa_context *c, const pa_sink_info *i, int is_last, void *userdata) 
	{
		PulseAudioControl* pThis = (PulseAudioControl*)userdata;

		int channel = 0;

		if (is_last == 0)
		{
			pThis->mMute = i->mute;
			pThis->mVolume = (unsigned)(((uint64_t) i->volume.values[channel] * 100 + (uint64_t)PA_VOLUME_NORM / 2) / (uint64_t)PA_VOLUME_NORM);		
		}
	}

	static void set_sink_volume_callback(pa_context *c, const pa_sink_info *i, int is_last, void *userdata) 
	{
		PulseAudioControl* pThis = (PulseAudioControl*)userdata;

		pa_cvolume cv;

		if (is_last == 0)
		{	
			pa_cvolume_set(&cv, i->channel_map.channels, (pa_volume_t) (pThis->mVolume * (double) PA_VOLUME_NORM / 100));
			pa_operation_unref(pa_context_set_sink_volume_by_name(c, DEFAULT_SINK_NAME, &cv, simple_callback, NULL));
		}
	}

	static void subscribe_callback(pa_context *c, pa_subscription_event_type_t type, uint32_t idx, void *userdata) 
	{		
		unsigned facility = type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
		pa_operation *o = NULL;

		switch (facility) {
		case PA_SUBSCRIPTION_EVENT_SINK:
			if( (o = pa_context_get_sink_info_by_name(c, DEFAULT_SINK_NAME, get_sink_volume_callback, userdata)) != NULL) pa_operation_unref(o);
			break;
		}
	}

	static void context_state_callback(pa_context *c, void *userdata) 
	{
		PulseAudioControl* pThis = (PulseAudioControl*)userdata;

		switch (pa_context_get_state(c)) {
		case PA_CONTEXT_CONNECTING:
		case PA_CONTEXT_AUTHORIZING:
		case PA_CONTEXT_SETTING_NAME:
			break;

		case PA_CONTEXT_READY:
			LOG(LogDebug) << "PulseAudioControl Ready";

 			pa_context_set_subscribe_callback(c, subscribe_callback, userdata);
    		pa_context_subscribe(c, PA_SUBSCRIPTION_MASK_SINK, NULL, NULL);

			pThis->mReady = 1;
			pThis->FireEvent();
			break;

		case PA_CONTEXT_TERMINATED:
			LOG(LogDebug) << "PulseAudioControl Context terminated";    		
			pThis->mReady = 0;
			break;

		case PA_CONTEXT_FAILED:
		default:
			LOG(LogError) << "PulseAudioControl Connection failure : " << pa_strerror(pa_context_errno(c));    		
			pThis->mReady = 0;
			pThis->FireEvent();

			quit(userdata, 1);
		}
	}

private:
	int mReady;
	int mMute;
	int mVolume;

	void FireEvent()
	{
		mEvent.notify_one();
	}

	void WaitEvent()
	{
		std::unique_lock<std::mutex> lock(mLock);
		mEvent.wait(lock);
	}

	std::thread*	mThread;

	std::mutex					mLock;
	std::condition_variable		mEvent;		

	pa_context* 	mContext;
    pa_mainloop* 	mMainLoop;
};

static PulseAudioControl PulseAudio;

#endif

#if defined(__linux__)
#if defined(_RPI_) || defined(_VERO4K_)
		std::string VolumeControl::mixerName = "PCM";
#else
		std::string VolumeControl::mixerName = "Master";
#endif
	std::string VolumeControl::mixerCard = "default";
#endif

std::weak_ptr<VolumeControl> VolumeControl::sInstance;


VolumeControl::VolumeControl()
	: internalVolume(0)
#if defined (__APPLE__)
	#error TODO: Not implemented for MacOS yet!!!
#elif defined(__linux__)
	, mixerIndex(0), mixerHandle(nullptr), mixerElem(nullptr), mixerSelemId(nullptr)
#elif defined(WIN32) || defined(_WIN32)
	, mixerHandle(nullptr), endpointVolume(nullptr)
#endif
{
	init();
}

VolumeControl::~VolumeControl()
{
#ifdef _ENABLE_PULSE_
	if (PulseAudio.isReady())
		PulseAudio.exit();
#endif

	deinit();
}

std::shared_ptr<VolumeControl> & VolumeControl::getInstance()
{
	//check if an VolumeControl instance is already created, if not create one
	static std::shared_ptr<VolumeControl> sharedInstance = sInstance.lock();
	if (sharedInstance == nullptr) {
		sharedInstance.reset(new VolumeControl);
		sInstance = sharedInstance;
	}
	return sharedInstance;
}

void VolumeControl::init()
{
	//initialize audio mixer interface
#if defined (__APPLE__)
	#error TODO: Not implemented for MacOS yet!!!
#elif defined(__linux__)

#ifdef _ENABLE_PULSE_
	// Read initial volume from systemconf
	std::string volume = SystemConf::getInstance()->get("audio.volume");
	PulseAudio.setVolume(volume.empty() ? 100 : Utils::String::toInteger(volume), false);
	return;
#endif

	//try to open mixer device
	if (mixerHandle == nullptr)
	{
		// Allow users to override the AudioCard and MixerName in es_settings.cfg
		auto audioCard = Settings::getInstance()->getString("AudioCard");
		if (!audioCard.empty())
			mixerCard = audioCard;

		auto audioDevice = Settings::getInstance()->getString("AudioDevice");
		if (!audioDevice.empty())
			mixerName = audioDevice;

		snd_mixer_selem_id_alloca(&mixerSelemId);
		//sets simple-mixer index and name
		snd_mixer_selem_id_set_index(mixerSelemId, mixerIndex);
		snd_mixer_selem_id_set_name(mixerSelemId, mixerName.c_str());
		//open mixer
		if (snd_mixer_open(&mixerHandle, 0) >= 0)
		{
			LOG(LogDebug) << "VolumeControl::init() - Opened ALSA mixer";
			//ok. attach to defualt card
			if (snd_mixer_attach(mixerHandle, mixerCard.c_str()) >= 0)
			{
				LOG(LogDebug) << "VolumeControl::init() - Attached to default card";
				//ok. register simple element class
				if (snd_mixer_selem_register(mixerHandle, NULL, NULL) >= 0)
				{
					LOG(LogDebug) << "VolumeControl::init() - Registered simple element class";
					//ok. load registered elements
					if (snd_mixer_load(mixerHandle) >= 0)
					{
						LOG(LogDebug) << "VolumeControl::init() - Loaded mixer elements";
						//ok. find elements now
						mixerElem = snd_mixer_find_selem(mixerHandle, mixerSelemId);
						if (mixerElem != nullptr)
						{
							//wohoo. good to go...
							LOG(LogDebug) << "VolumeControl::init() - Mixer initialized";
						}
						else
						{
							LOG(LogInfo) << "VolumeControl::init() - Unable to find mixer " << mixerName << " -> Search for alternative mixer";

							snd_mixer_selem_id_t *mxid = nullptr;
							snd_mixer_selem_id_alloca(&mxid);

							for (snd_mixer_elem_t* mxe = snd_mixer_first_elem(mixerHandle); mxe != nullptr; mxe = snd_mixer_elem_next(mxe))
							{
								if (snd_mixer_selem_has_playback_volume(mxe) != 0 && snd_mixer_selem_is_active(mxe) != 0)
								{
									snd_mixer_selem_get_id(mxe, mxid);
									mixerName = snd_mixer_selem_id_get_name(mxid);

									LOG(LogInfo) << "mixername : " << mixerName;

									snd_mixer_selem_id_set_name(mixerSelemId, mixerName.c_str());
									mixerElem = snd_mixer_find_selem(mixerHandle, mixerSelemId);
									if (mixerElem != nullptr)
									{
										//wohoo. good to go...
										LOG(LogDebug) << "VolumeControl::init() - Mixer initialized";
										break;
									}
									else
										LOG(LogDebug) << "VolumeControl::init() - Mixer not initialized";
								}
							}

							if (mixerElem == nullptr)
							{
								LOG(LogError) << "VolumeControl::init() - Failed to find mixer elements!";
								snd_mixer_close(mixerHandle);
								mixerHandle = nullptr;
							}
						}
					}
					else
					{
						LOG(LogError) << "VolumeControl::init() - Failed to load mixer elements!";
						snd_mixer_close(mixerHandle);
						mixerHandle = nullptr;
					}
				}
				else
				{
					LOG(LogError) << "VolumeControl::init() - Failed to register simple element class!";
					snd_mixer_close(mixerHandle);
					mixerHandle = nullptr;
				}
			}
			else
			{
				LOG(LogError) << "VolumeControl::init() - Failed to attach to default card!";
				snd_mixer_close(mixerHandle);
				mixerHandle = nullptr;
			}
		}
		else
		{
			LOG(LogError) << "VolumeControl::init() - Failed to open ALSA mixer!";
		}
	}
#elif defined(WIN32) || defined(_WIN32)
	//get windows version information
	OSVERSIONINFOEXA osVer = {sizeof(OSVERSIONINFO)};
	::GetVersionExA(reinterpret_cast<LPOSVERSIONINFOA>(&osVer));
	//check windows version
	if(osVer.dwMajorVersion < 6)
	{
		//Windows older than Vista. use mixer API. open default mixer
		if (mixerHandle == nullptr)
		{
			if (mixerOpen(&mixerHandle, 0, NULL, 0, 0) == MMSYSERR_NOERROR)
			{
				//retrieve info on the volume slider control for the "Speaker Out" line
				MIXERLINECONTROLS mixerLineControls;
				mixerLineControls.cbStruct = sizeof(MIXERLINECONTROLS);
				mixerLineControls.dwLineID = 0xFFFF0000; //Id of "Speaker Out" line
				mixerLineControls.cControls = 1;
				//mixerLineControls.dwControlID = 0x00000000; //Id of "Speaker Out" line's volume slider
				mixerLineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME; //Get volume control
				mixerLineControls.pamxctrl = &mixerControl;
				mixerLineControls.cbmxctrl = sizeof(MIXERCONTROL);
				if (mixerGetLineControls((HMIXEROBJ)mixerHandle, &mixerLineControls, MIXER_GETLINECONTROLSF_ONEBYTYPE) != MMSYSERR_NOERROR)
				{
					LOG(LogError) << "VolumeControl::getVolume() - Failed to get mixer volume control!";
					mixerClose(mixerHandle);
					mixerHandle = nullptr;
				}
			}
			else
			{
				LOG(LogError) << "VolumeControl::init() - Failed to open mixer!";
			}
		}
	}
	else 
	{
		//Windows Vista or above. use EndpointVolume API. get device enumerator
		if (endpointVolume == nullptr)
		{
			CoInitialize(nullptr);
			IMMDeviceEnumerator * deviceEnumerator = nullptr;
			CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator), (LPVOID *)&deviceEnumerator);
			if (deviceEnumerator != nullptr)
			{
				//get default endpoint
				IMMDevice * defaultDevice = nullptr;
				deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);
				if (defaultDevice != nullptr)
				{
					//retrieve endpoint volume
					defaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr, (LPVOID *)&endpointVolume);
					if (endpointVolume == nullptr)
					{
						LOG(LogError) << "VolumeControl::init() - Failed to get default audio endpoint volume!";
					}
					//release default device. we don't need it anymore
					defaultDevice->Release();
				}
				else
				{
					LOG(LogError) << "VolumeControl::init() - Failed to get default audio endpoint!";
				}
				//release device enumerator. we don't need it anymore
				deviceEnumerator->Release();
			}
			else
			{
				LOG(LogError) << "VolumeControl::init() - Failed to get audio endpoint enumerator!";
				CoUninitialize();
			}
		}
	}
#endif
}

void VolumeControl::deinit()
{
	//deinitialize audio mixer interface
#if defined (__APPLE__)
	#error TODO: Not implemented for MacOS yet!!!
#elif defined(__linux__)

#ifdef _ENABLE_PULSE_
	return;
#endif

	if (mixerHandle != nullptr) {
		snd_mixer_detach(mixerHandle, mixerCard.c_str());
		snd_mixer_free(mixerHandle);
		snd_mixer_close(mixerHandle);
		mixerHandle = nullptr;
		mixerElem = nullptr;
	}
#elif defined(WIN32) || defined(_WIN32)
	if (mixerHandle != nullptr) {
		mixerClose(mixerHandle);
		mixerHandle = nullptr;
	}
	else if (endpointVolume != nullptr) {
		endpointVolume->Release();
		endpointVolume = nullptr;
		CoUninitialize();
	}
#endif
}

int VolumeControl::getVolume() const
{
	int volume = 0;

#if defined (__APPLE__)
	#error TODO: Not implemented for MacOS yet!!!
#elif defined(__linux__)

#ifdef _ENABLE_PULSE_
	return PulseAudio.getVolume();	
#endif

	if (mixerElem != nullptr)
	{
		if (mixerHandle != nullptr)
			snd_mixer_handle_events(mixerHandle);
		/*
		int mute_state;
		if (snd_mixer_selem_has_playback_switch(mixerElem)) 
		{
			snd_mixer_selem_get_playback_switch(mixerElem, SND_MIXER_SCHN_UNKNOWN, &mute_state);
			if (!mute_state) // system Muted
				return 0;
		}
		*/
		//get volume range
		long minVolume;
		long maxVolume;
		if (snd_mixer_selem_get_playback_volume_range(mixerElem, &minVolume, &maxVolume) == 0)
		{
			//ok. now get volume
			long rawVolume;
			if (snd_mixer_selem_get_playback_volume(mixerElem, SND_MIXER_SCHN_MONO, &rawVolume) == 0)
			{
				//worked. bring into range 0-100
				rawVolume -= minVolume;
				if (rawVolume > 0)
					volume = (rawVolume * 100.0) / (maxVolume - minVolume) + 0.5;
			}
			else
			{
				LOG(LogError) << "VolumeControl::getVolume() - Failed to get mixer volume!";
			}
		}
		else
		{
			LOG(LogError) << "VolumeControl::getVolume() - Failed to get volume range!";
		}
	}
#elif defined(WIN32) || defined(_WIN32)
	if (mixerHandle != nullptr)
	{
		//Windows older than Vista. use mixer API. get volume from line control
		MIXERCONTROLDETAILS_UNSIGNED value;
		MIXERCONTROLDETAILS mixerControlDetails;
		mixerControlDetails.cbStruct = sizeof(MIXERCONTROLDETAILS);
		mixerControlDetails.dwControlID = mixerControl.dwControlID;
		mixerControlDetails.cChannels = 1; //always 1 for a MIXERCONTROL_CONTROLF_UNIFORM control
		mixerControlDetails.cMultipleItems = 0; //always 0 except for a MIXERCONTROL_CONTROLF_MULTIPLE control
		mixerControlDetails.paDetails = &value;
		mixerControlDetails.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);

		if (mixerGetControlDetails((HMIXEROBJ)mixerHandle, &mixerControlDetails, MIXER_GETCONTROLDETAILSF_VALUE) == MMSYSERR_NOERROR) 
			volume = (int)Math::round((value.dwValue * 100) / 65535.0f);
	}
	else if (endpointVolume != nullptr)
	{
		// Windows Vista or above. use EndpointVolume API
		float floatVolume = 0.0f; //0-1

		BOOL mute = FALSE;
		if (endpointVolume->GetMute(&mute) == S_OK)
		{
			if (mute)
				return 0;
		}

		if (endpointVolume->GetMasterVolumeLevelScalar(&floatVolume) == S_OK)
			volume = (int)Math::round(floatVolume * 100.0f);
	}
#endif

	// clamp to 0-100 range
	if (volume < 0)
		volume = 0;

	if (volume > 100)
		volume = 100;

	return volume;
}

void VolumeControl::setVolume(int volume)
{
	//clamp to 0-100 range
	if (volume < 0)
	{
		volume = 0;
	}
	if (volume > 100)
	{
		volume = 100;
	}
	//store values in internal variables
	internalVolume = volume;
#if defined (__APPLE__)
	#error TODO: Not implemented for MacOS yet!!!
#elif defined(__linux__)

#ifdef _ENABLE_PULSE_
	if (PulseAudio.isReady())
	{
		PulseAudio.setVolume(volume);
	}
	return;
#endif

	if (mixerElem != nullptr)
	{
		//get volume range
		long minVolume;
		long maxVolume;
		if (snd_mixer_selem_get_playback_volume_range(mixerElem, &minVolume, &maxVolume) == 0)
		{
			//ok. bring into minVolume-maxVolume range and set
			long rawVolume = (volume * (maxVolume - minVolume) / 100) + minVolume;
			if (snd_mixer_selem_set_playback_volume(mixerElem, SND_MIXER_SCHN_FRONT_LEFT, rawVolume) < 0 
				|| snd_mixer_selem_set_playback_volume(mixerElem, SND_MIXER_SCHN_FRONT_RIGHT, rawVolume) < 0)
			{
				LOG(LogError) << "VolumeControl::getVolume() - Failed to set mixer volume!";
			}
		}
		else
		{
			LOG(LogError) << "VolumeControl::getVolume() - Failed to get volume range!";
		}
	}
#elif defined(WIN32) || defined(_WIN32)
	if (mixerHandle != nullptr)
	{
		//Windows older than Vista. use mixer API. get volume from line control
		MIXERCONTROLDETAILS_UNSIGNED value;
		value.dwValue = (volume * 65535) / 100;
		MIXERCONTROLDETAILS mixerControlDetails;
		mixerControlDetails.cbStruct = sizeof(MIXERCONTROLDETAILS);
		mixerControlDetails.dwControlID = mixerControl.dwControlID;
		mixerControlDetails.cChannels = 1; //always 1 for a MIXERCONTROL_CONTROLF_UNIFORM control
		mixerControlDetails.cMultipleItems = 0; //always 0 except for a MIXERCONTROL_CONTROLF_MULTIPLE control
		mixerControlDetails.paDetails = &value;
		mixerControlDetails.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
		if (mixerSetControlDetails((HMIXEROBJ)mixerHandle, &mixerControlDetails, MIXER_SETCONTROLDETAILSF_VALUE) != MMSYSERR_NOERROR)
		{
			LOG(LogError) << "VolumeControl::setVolume() - Failed to set mixer volume!";
		}
	}
	else if (endpointVolume != nullptr)
	{
		//Windows Vista or above. use EndpointVolume API
		float floatVolume = 0.0f; //0-1
		if (volume > 0) {
			floatVolume = (float)volume / 100.0f;
		}
		if (endpointVolume->SetMasterVolumeLevelScalar(floatVolume, nullptr) != S_OK)
		{
			LOG(LogError) << "VolumeControl::setVolume() - Failed to set master volume!";
		}
	}
#endif
}

bool VolumeControl::isAvailable()
{
#if defined (__APPLE__)
	return false;
#elif defined(__linux__)

#ifdef _ENABLE_PULSE_
	return PulseAudio.isReady();
#endif

	return mixerHandle != nullptr && mixerElem != nullptr;
#elif defined(WIN32) || defined(_WIN32)
	return mixerHandle != nullptr || endpointVolume != nullptr;
#endif
}
