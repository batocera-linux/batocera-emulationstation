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

	static std::string getMimeType(const std::string &path);

private:
	Window*			mWindow;
	bool			mRunning;
	bool			mFirstRun;
	std::thread*	mThread;

	httplib::Server* mHttpServer;

	void run();
};


