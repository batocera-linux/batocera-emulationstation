#include "TextToSpeech.h"

#include "Log.h"
#include "LocaleES.h"

#ifdef _ENABLE_TTS_
#include <espeak/speak_lib.h>
#endif

std::weak_ptr<TextToSpeech> TextToSpeech::sInstance;

TextToSpeech::TextToSpeech()
{
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
  if (sharedInstance == nullptr) {
    sharedInstance.reset(new TextToSpeech);
    sInstance = sharedInstance;
  }
  return sharedInstance;
}

void TextToSpeech::init()
{
  bool setdone = false;
  m_enabled = false;
#ifdef _ENABLE_TTS_
  m_isAvailable = espeak_Initialize(AUDIO_OUTPUT_PLAYBACK, 0, NULL, 0) != EE_INTERNAL_ERROR;
  if(m_isAvailable) {
	char* envv;
	envv = getenv("LANGUAGE");
	if(envv != NULL) {
	  if(std::string(envv).length() >= 4) {
	    setLanguage(envv);
	    setdone = true;
	  }
	}

	if(setdone == false) {
	  envv = getenv("LANG");
	  if(envv != NULL) {
	    if(std::string(envv).length() >= 4) {
	      setLanguage(envv);
	      setdone = true;
	    }
	  }
	}
  }
#else
  m_isAvailable = false;
#endif
}

void TextToSpeech::deinit()
{
#ifdef _ENABLE_TTS_
  if(espeak_Terminate() != EE_OK) {
    LOG(LogError) << "TTS::deinit() - Failed to terminate";
  }
#endif
  m_isAvailable = false;
  m_enabled = false;
}

bool TextToSpeech::isAvailable() {
  return m_isAvailable;
}

bool TextToSpeech::enabled() {
  return m_enabled;
}

void TextToSpeech::enable(bool v) {
  if(m_isAvailable == false) return;

  if(v) {
    m_enabled = true;
    say(_("TEXT TO SPEECH ENABLED"));
  } else {
    say(_("TEXT TO SPEECH DISABLED"));
    m_enabled = false;
  }
}

bool TextToSpeech::toogle() {
  enable(!enabled());
  return enabled();
}

void TextToSpeech::setLanguage(const std::string language) {
  if(m_isAvailable == false) return;

#ifdef _ENABLE_TTS_
  std::string voice = "default";
  std::string language_part1 = language.substr(0, 5);

  // espeak --voices
  //if(language_part1 == "ar_YE") voice = "";
  if(language_part1 == "ca_ES") voice = "catalan";
  if(language_part1 == "cy_GB") voice = "welsh";
  if(language_part1 == "de_DE") voice = "german";
  if(language_part1 == "el_GR") voice = "greek";
  if(language_part1 == "es_ES") voice = "spanish";
  if(language_part1 == "es_MX") voice = "spanish-latin-am";
  //if(language_part1 == "eu_ES") voice = "";
  if(language_part1 == "fr_FR") voice = "french";
  if(language_part1 == "hu_HU") voice = "hungarian";
  if(language_part1 == "it_IT") voice = "italian";
  //if(language_part1 == "ja_JP") voice = "";
  //if(language_part1 == "ko_KR") voice = "";
  if(language_part1 == "nb_NO") voice = "norwegian";
  if(language_part1 == "nl_NL") voice = "dutch";
  if(language_part1 == "nn_NO") voice = "norwegian";
  //if(language_part1 == "oc_FR") voice = "";
  if(language_part1 == "pl_PL") voice = "polish";
  if(language_part1 == "pt_BR") voice = "brazil";
  if(language_part1 == "pt_PT") voice = "portugal";
  if(language_part1 == "ru_RU") voice = "russian";
  if(language_part1 == "sv_SE") voice = "swedish";
  if(language_part1 == "tr_TR") voice = "turkish";
  //if(language_part1 == "uk_UA") voice = "";
  if(language_part1 == "zh_CN") voice = "Mandarin"; // or cantonese ?
  if(language_part1 == "zh_TW") voice = "Mandarin"; // or cantonese ?

  if(espeak_SetVoiceByName(voice.c_str()) == EE_OK) {
    LOG(LogInfo) << "TTS::setLanguage() - set to " << language_part1 << " (" << voice << ")";
  } else {
    LOG(LogError) << "TTS::setLanguage() - Failed with " << language_part1 << " (" << voice << ")";
  }
#endif
}

void TextToSpeech::say(const std::string text, bool expand) {
  if(m_enabled == false) return;

#ifdef _ENABLE_TTS_
  if(m_isAvailable) {
    if(text == "") return; // nothing to say
    
    //LOG(LogError) << "-" << text << "-";
    if(expand == false && espeak_IsPlaying() == 1) {
      espeak_Cancel();
    }
    if(espeak_Synth(text.c_str(),
		    text.length() * 4 /* potentially 4 bytes by char */,
		    0,
		    POS_CHARACTER,
		    0,
		    espeakCHARS_UTF8,
		    NULL,
		    NULL) != EE_OK) {
      LOG(LogError) << "TTS::say() - Failed";
    }
  }
#endif
}
