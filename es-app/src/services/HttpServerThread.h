#pragma once

#include "Window.h"
#include <thread>

namespace httplib
{
	class Server;
}

class HttpServerThread
{
public:
	HttpServerThread(Window * window);
	virtual ~HttpServerThread();

private:
	Window*			mWindow;
	bool			mRunning;
	bool			mFirstRun;
	std::thread*	mThread;

	httplib::Server* mHttpServer;

	void run();
};


