#pragma once
#ifndef ES_APP_TEXTTOSPEECH_H
#define ES_APP_TEXTTOSPEECH_H

#include <string>
#include <memory>

#if WIN32
#include <thread>
#include <mutex>
#include <list>
#include <condition_variable>
#endif

/*!
Singleton pattern. Call getInstance() to get an object.
*/
class TextToSpeech
{
public:
	~TextToSpeech();
	static std::shared_ptr<TextToSpeech>& getInstance();

	bool isAvailable();
	bool isEnabled();
	void enable(bool v, bool playSay = true);
	bool toogle();

	void setLanguage(const std::string language);
	void say(const std::string text, bool expand = false, const std::string lang = "");

private:
	TextToSpeech();
	
	static std::weak_ptr<TextToSpeech> sInstance;

	bool m_isAvailable;
	bool m_enabled;

	void init();
	void deinit();

#if WIN32
	void SpeakThread();

	std::thread*				mSpeakThread;
	std::mutex					mMutex;
	std::condition_variable		mEvent;
	bool						mShouldTerminate;

	class SpeakItem
	{
	public:
		std::string text;
		bool expand;
	};

	std::list<SpeakItem*>		mSpeechQueue;
#endif

};

#endif // ES_APP_TEXTTOSPEECH_H
