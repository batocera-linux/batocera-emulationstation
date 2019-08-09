#pragma once

#include "Window.h"
#include <boost/thread.hpp>

// Batocera
class SongNameThread {

public:
    SongNameThread(Window * window);
    virtual ~SongNameThread();

private:
    Window* mWindow;
    std::string mSong;
    boost::thread * mThreadHandle;
    void run();
};
