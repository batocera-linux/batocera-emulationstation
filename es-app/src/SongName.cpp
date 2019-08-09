#include "SongName.h"
#include "ApiSystem.h"
#include "SystemConf.h"
#include "guis/GuiMsgBox.h"
#include "LocaleES.h"
#include "AudioManager.h"
#include "Log.h"

// Batocera
SongNameThread::SongNameThread(Window* window) : mWindow(window){
	mSong = "";
	mThreadHandle = new boost::thread(boost::bind(&SongNameThread::run, this));
}

SongNameThread::~SongNameThread() {
    	mThreadHandle->join();
}

void SongNameThread::run(){
	while (1) {
		if(SystemConf::getInstance()->get("audio.display_titles") == "1") { 
			mSong = AudioManager::getInstance()->getSongName();
			if(! mSong.empty()) {
				mWindow->displayMessage(_("Now playing: ") + mSong);
				mSong = "";
				AudioManager::getInstance()->setSongName("");
			}
			boost::this_thread::sleep(boost::posix_time::seconds(3));
		}
	}
}
