#include "NetworkThread.h"
#include "ApiSystem.h"
#include "SystemConf.h"
#include "guis/GuiMsgBox.h"
#include "LocaleES.h"

NetworkThread::NetworkThread(Window* window) : mWindow(window){
    
    // creer le thread
    mFirstRun = true;
    mRunning = true;
    mThreadHandle = new boost::thread(boost::bind(&NetworkThread::run, this));

}

NetworkThread::~NetworkThread() {
    	mThreadHandle->join();
}

void NetworkThread::run(){
    while(mRunning){
        if(mFirstRun){
            boost::this_thread::sleep(boost::posix_time::seconds(15));
            mFirstRun = false;
        }else {
            boost::this_thread::sleep(boost::posix_time::hours(1));
        }
	if(SystemConf::getInstance()->get("updates.enabled") == "1") {
	  std::vector<std::string> msgtbl;
	  if(ApiSystem::getInstance()->canUpdate(msgtbl)){
	    std::string msg = "";
	    for(int i=0; i<msgtbl.size(); i++) {
	      if(i != 0) msg += "\n";
	      msg += msgtbl[i];
	    }
	    mWindow->displayMessage(_("UPDATE AVAILABLE") + std::string(": ") + msg);
	    mRunning = false;
	  }
	}
    }
}

