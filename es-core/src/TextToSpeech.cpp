#include "TextToSpeech.h"
#include "Log.h"
#include "LocaleES.h"

#if WIN32
#include "SystemConf.h"
#include <sapi.h>
#include <sphelper.h>
#include <thread>

#pragma comment(lib, "sapi.lib")

void TextToSpeech::SpeakThread()
{
	if (FAILED(::CoInitializeEx(NULL, COINITBASE_MULTITHREADED)))
		return;

	ISpVoice* pVoice = NULL;
	HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void **)&pVoice);
	if (!SUCCEEDED(hr))
		return;

	m_isAvailable = true;
	
	while (true)
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mEvent.wait(lock, [this]() { return mShouldTerminate || !mSpeechQueue.empty(); });

		if (mShouldTerminate)
			break;

		SpeakItem* item = nullptr;

		if (!mSpeechQueue.empty())
		{
			item = mSpeechQueue.front();
			mSpeechQueue.pop_front();
		}

		lock.unlock();

		if (item != nullptr)
		{
			pVoice->Speak(Utils::String::convertToWideString(item->text).c_str(), SPF_ASYNC | SPF_IS_XML | (item->expand ? 0 : SPF_PURGEBEFORESPEAK), NULL);
			delete item;
		}
	}

	pVoice->Pause();
	pVoice->Release();
}
#elif defined(_ENABLE_TTS_)
#include <espeak/speak_lib.h>
#endif

std::weak_ptr<TextToSpeech> TextToSpeech::sInstance;

TextToSpeech::TextToSpeech()
{
	m_isAvailable = false;
	m_enabled = false;

	init();
}

TextToSpeech::~TextToSpeech()
{
	deinit();
}

std::shared_ptr<TextToSpeech> & TextToSpeech::getInstance()
{
	//check if an TextToSpeech instance is already created, if not create one
	static std::shared_ptr<TextToSpeech> sharedInstance = sInstance.lock();
	if (sharedInstance == nullptr) 
	{
		sharedInstance.reset(new TextToSpeech);
		sInstance = sharedInstance;
	}
	return sharedInstance;
}

void TextToSpeech::init()
{
	if (m_isAvailable)
		return;

#if WIN32
	m_isAvailable = false;
	mShouldTerminate = false;
	mSpeakThread = new std::thread(&TextToSpeech::SpeakThread, this);
#elif defined(_ENABLE_TTS_)	
	m_isAvailable = espeak_Initialize(AUDIO_OUTPUT_PLAYBACK, 0, NULL, 0) != EE_INTERNAL_ERROR;
	if (!m_isAvailable)
		return;
	
	char* envv = getenv("LANGUAGE");
	if (envv != nullptr && std::string(envv).length() >= 4) 
		setLanguage(envv);
	else
	{
		envv = getenv("LANG");
		if (envv != nullptr && std::string(envv).length() >= 4)					
			setLanguage(envv);					
	}	
#endif
}

void TextToSpeech::deinit()
{
	if (!m_isAvailable)
		return;

	m_isAvailable = false;
	m_enabled = false;

#if WIN32

	if (mSpeakThread != nullptr)
	{
		mShouldTerminate = true;

		mEvent.notify_all();

		mSpeakThread->join();
		delete mSpeakThread;
		mSpeakThread = nullptr;
	}
#elif defined(_ENABLE_TTS_)
	if (espeak_Terminate() != EE_OK) {
		LOG(LogError) << "TTS::deinit() - Failed to terminate";
	}
#endif
}

bool TextToSpeech::isAvailable()
{
	return m_isAvailable;
}

bool TextToSpeech::isEnabled()
{
	return m_enabled;
}

void TextToSpeech::enable(bool v, bool playSay) 
{
	if (v == m_enabled)
		return;
	
	if (m_isAvailable && playSay)
	{
		m_enabled = true; // Force
		say(v ? _("SCREEN READER ENABLED") : _("SCREEN READER DISABLED"));
	}

	m_enabled = v;
}

bool TextToSpeech::toogle() 
{
	enable(!m_enabled);
	return m_enabled;
}

void TextToSpeech::setLanguage(const std::string language) 
{
	if (m_isAvailable == false)
		return;

#ifdef _ENABLE_TTS_
	std::string voice = "default";
	std::string language_part1 = language.substr(0, 5);

	// espeak --voices
	//if(language_part1 == "ar_YE") voice = "";
	if (language_part1 == "ca_ES") voice = "catalan";
	if (language_part1 == "cy_GB") voice = "welsh";
	if (language_part1 == "de_DE") voice = "german";
	if (language_part1 == "el_GR") voice = "greek";
	if (language_part1 == "es_ES") voice = "spanish";
	if (language_part1 == "es_MX") voice = "spanish-latin-am";
	//if(language_part1 == "eu_ES") voice = "";
	if (language_part1 == "fi_FI") voice = "finnish";
	if (language_part1 == "fr_FR") voice = "french";
	if (language_part1 == "hu_HU") voice = "hungarian";
	if (language_part1 == "id_ID") voice = "";
	if (language_part1 == "it_IT") voice = "italian";
	//if(language_part1 == "ja_JP") voice = "";
	//if(language_part1 == "ko_KR") voice = "";
	if (language_part1 == "nb_NO") voice = "norwegian";
	if (language_part1 == "nl_NL") voice = "dutch";
	if (language_part1 == "nn_NO") voice = "norwegian";
	//if(language_part1 == "oc_FR") voice = "";
	if (language_part1 == "pl_PL") voice = "polish";
	if (language_part1 == "pt_BR") voice = "brazil";
	if (language_part1 == "pt_PT") voice = "portugal";
	if (language_part1 == "ru_RU") voice = "russian";
	//if (language_part1 == "sk_SK") voice = "";
	if (language_part1 == "sv_SE") voice = "swedish";
	if (language_part1 == "tr_TR") voice = "turkish";
	//if(language_part1 == "uk_UA") voice = "";
	if (language_part1 == "zh_CN") voice = "Mandarin"; // or cantonese ?
	if (language_part1 == "zh_TW") voice = "Mandarin"; // or cantonese ?

	if (espeak_SetVoiceByName(voice.c_str()) == EE_OK) {
		LOG(LogInfo) << "TTS::setLanguage() - set to " << language_part1 << " (" << voice << ")";
	}
	else {
		LOG(LogError) << "TTS::setLanguage() - Failed with " << language_part1 << " (" << voice << ")";
	}
#endif
}

void TextToSpeech::say(const std::string text, bool expand, const std::string lang)
{
	if (!m_isAvailable || !m_enabled || text.empty())
		return;

#if WIN32
	std::string language = lang;
	if (language.empty())
		language = SystemConf::getInstance()->get("system.language");
		
	if (language.empty() || language == "en")
		language = "en-US";

	std::string full = "<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xml:lang='" + language + "'>" + text + "</speak>";
		
	std::unique_lock<std::mutex> lock(mMutex);

	if (!expand)
		mSpeechQueue.clear();

	SpeakItem* item = new SpeakItem();
	item->text = full;
	item->expand = expand;
	mSpeechQueue.push_back(item);

	mEvent.notify_one();

#elif defined(_ENABLE_TTS_)

	if (expand == false && espeak_IsPlaying() == 1)
		espeak_Cancel();
	
	if (espeak_Synth(text.c_str(),
		text.length() * 4 /* potentially 4 bytes by char */,
		0,
		POS_CHARACTER,
		0,
		espeakCHARS_UTF8,
		NULL,
		NULL) != EE_OK) {
		LOG(LogError) << "TTS::say() - Failed";
	}	
#endif
}
