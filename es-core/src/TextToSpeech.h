#pragma once
#ifndef ES_APP_TEXTTOSPEECH_H
#define ES_APP_TEXTTOSPEECH_H

#include <string>
#include <memory>

/*!
Singleton pattern. Call getInstance() to get an object.
*/
class TextToSpeech
{
 private:
  static std::weak_ptr<TextToSpeech> sInstance;
  bool m_isAvailable;
  bool m_enabled;

  TextToSpeech();

  void init();
  void deinit();
  
 public:
  static std::shared_ptr<TextToSpeech> & getInstance();

  bool isAvailable();
  bool enabled();
  void enable(bool v);
  bool toogle();

  void setLanguage(const std::string language);
  void say(const std::string text, bool expand = false);

  ~TextToSpeech();
};

#endif // ES_APP_TEXTTOSPEECH_H
