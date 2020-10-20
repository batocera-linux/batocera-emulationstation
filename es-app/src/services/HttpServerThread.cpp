#include "HttpServerThread.h"
#include "httplib.h"
#include "Log.h"

#ifdef WIN32
#include <Windows.h>
#endif

#include "platform.h"
#include "Gamelist.h"
#include "SystemData.h"
#include "FileData.h"
#include "views/ViewController.h"
#include <unordered_map>
#include "CollectionSystemManager.h"
#include "guis/GuiMenu.h"
#include "guis/GuiMsgBox.h"
#include "utils/FileSystemUtil.h"

HttpServerThread::HttpServerThread(Window* window) : mWindow(window)
{
	LOG(LogDebug) << "HttpServerThread : Starting";

	mHttpServer = nullptr;

	// creer le thread
	mFirstRun = true;
	mRunning = true;
	mThread = new std::thread(&HttpServerThread::run, this);
}

HttpServerThread::~HttpServerThread()
{
	LOG(LogDebug) << "HttpServerThread : Exit";

	if (mHttpServer != nullptr)
	{
		mHttpServer->stop();
		delete mHttpServer;
		mHttpServer = nullptr;
	}

	mRunning = false;
	mThread->join();
	delete mThread;
}

void HttpServerThread::run()
{
	mHttpServer = new httplib::Server();

	mHttpServer->Get("/", [=](const httplib::Request & /*req*/, httplib::Response &res) {
		res.set_redirect("/index.html");
	});

	mHttpServer->Get("/index.html", [](const httplib::Request& req, httplib::Response& res)
	{
		res.set_content(
			"<!DOCTYPE html>\r\n"
		    "<html lang='fr'>\r\n"
			"<head>\r\n"			
			"<title>EmulationStation services</title>\r\n"
			"</head>\r\n"
			"<body style='font-family: Open Sans, sans-serif;'>\r\n"
			"<p>EmulationStation services</p>\r\n"

			"<script type='text/javascript'>\r\n"

			"function quitES() {\r\n"
			"var xhr = new XMLHttpRequest();\r\n"			
			"xhr.open('GET', '/quit');\r\n"
			"xhr.send(); }\r\n"

			"function reloadGamelists() {\r\n"
			"var xhr = new XMLHttpRequest();\r\n"
			"xhr.open('GET', '/reloadgames');\r\n"
			"xhr.send(); }\r\n"

			"</script>\r\n"

			"<input type='button' value='Reload games' onClick='reloadGamelists()'/>\r\n"
			"<br/>"
			"<input type='button' value='Quit' onClick='quitES()'/>\r\n"

			"</body>\r\n</html>", "text/html");
	});

	mHttpServer->Get("/quit", [](const httplib::Request& req, httplib::Response& res)
	{
		quitES();		
	});

	mHttpServer->Get("/restart", [](const httplib::Request& req, httplib::Response& res)
	{
		quitES(QuitMode::REBOOT);
	});

	mHttpServer->Get("/reloadgames", [this](const httplib::Request& req, httplib::Response& res)
	{
		mWindow->postToUiThread([](Window* w)
		{
			GuiMenu::updateGameLists(w, false);
		});
	});

	mHttpServer->Post("/messagebox", [this](const httplib::Request& req, httplib::Response& res)
	{
		if (req.body.empty())
		{
			res.set_content("400 bad request - body is missing", "text/html");
			res.status = 400;
			return;
		}

		auto msg = req.body;
		mWindow->postToUiThread([msg](Window* w) { w->pushGui(new GuiMsgBox(w, msg)); });
	});

	mHttpServer->Post("/notify", [this](const httplib::Request& req, httplib::Response& res)
	{
		if (req.body.empty())
		{
			res.set_content("400 bad request - body is missing", "text/html");
			res.status = 400;
			return;
		}

		mWindow->displayNotificationMessage(req.body);
	});

	mHttpServer->Post("/launch", [this](const httplib::Request& req, httplib::Response& res)
	{
		if (req.body.empty())
		{
			res.set_content("400 bad request - body is missing", "text/html");
			res.status = 400;
			return;
		}

		auto path = Utils::FileSystem::getAbsolutePath(req.body);

		for (auto system : SystemData::sSystemVector)
		{
			if (system->isCollection() || system->isGroupSystem())
				continue;

			for (auto file : system->getRootFolder()->getFilesRecursive(GAME))
			{
				if (file->getFullPath() == path || file->getPath() == path)
				{
					mWindow->postToUiThread([file](Window* w) { ViewController::get()->launch(file); });					
					return;
				}
			}
		}
	});

	mHttpServer->Post(R"(/addgames/(/?.*))", [this](const httplib::Request& req, httplib::Response& res)
	{
		if (req.body.empty())
		{
			res.set_content("400 bad request - body is missing", "text/html");
			res.status = 400;
			return;
		}

		std::string systemName = req.matches[1];

		bool deleteSystem = false;

		SystemData* system = SystemData::getSystem(systemName);		
		if (system == nullptr)
		{
			system = SystemData::loadSystem(systemName, false);
			if (system == nullptr)
			{
				res.set_content("404 System not found", "text/html");
				res.status = 404;
				return;
			}

			deleteSystem = true;
		}
			
		std::unordered_map<std::string, FileData*> fileMap;
		for (auto file : system->getRootFolder()->getFilesRecursive(GAME))
			fileMap[file->getPath()] = file;

		auto fileList = loadGamelistFile(req.body, system, fileMap, SIZE_MAX, false);
		if (fileList.size() == 0)
		{
			res.set_content("204 No game added / updated", "text/html");
			res.status = 204;

			if (deleteSystem)
				delete system;

			return;
		}
	
		if (deleteSystem)
		{			
			for (auto file : system->getRootFolder()->getFilesRecursive(GAME))
				if (fileMap.find(file->getPath()) != fileMap.cend())
					file->getMetadata().setDirty();

			updateGamelist(system);
			delete system;

			res.set_content("201 Game added. System not updated", "text/html");
			res.status = 201;

			mWindow->postToUiThread([](Window* w) { GuiMenu::updateGameLists(w, false); });
		}
		else
		{
			for (auto file : fileList)
				if (file->getMetadata().wasChanged())
					saveToGamelistRecovery(file);

			mWindow->postToUiThread([system](Window* w)
			{
				ViewController::get()->onFileChanged(system->getRootFolder(), FILE_METADATA_CHANGED); // Update root folder			
			});
		}

		res.set_content("OK", "text/html");
	});
	
	mHttpServer->Post(R"(/removegames/(/?.*))", [this](const httplib::Request& req, httplib::Response& res)
	{
		if (req.body.empty())
		{
			res.set_content("400 bad request - body is missing", "text/html");
			res.status = 400;
			return;
		}

		auto systemName = req.matches[1];

		SystemData* system = SystemData::getSystem(systemName);
		if (system == nullptr)
		{
			res.set_content("404 not found", "text/html");
			res.status = 404;
			return;
		}

		std::unordered_map<std::string, FileData*> fileMap;
		for (auto file : system->getRootFolder()->getFilesRecursive(GAME))
			fileMap[file->getPath()] = file;

		auto fileList = loadGamelistFile(req.body, system, fileMap, SIZE_MAX, false);
		if (fileList.size() == 0)
		{
			res.set_content("204 No game removed", "text/html");
			res.status = 204;
			return;
		}

		std::vector<SystemData*> systems;
		systems.push_back(system);

		for (auto file : fileList)
		{
			removeFromGamelistRecovery(file);

			auto filePath = file->getPath();
			if (Utils::FileSystem::exists(filePath))
				Utils::FileSystem::removeFile(filePath);

			file->getParent()->removeChild(file);						

			for (auto sys : SystemData::sSystemVector)
			{
				if (!sys->isCollection())
					continue;

				auto copy = sys->getRootFolder()->FindByPath(filePath);
				if (copy != nullptr)
				{
					copy->getParent()->removeChild(file);
					systems.push_back(sys);
				}
			}

			// delete file; intentionnal mem leak
		}

		mWindow->postToUiThread([systems](Window* w)
		{
			for (auto changedSystem : systems)
				ViewController::get()->onFileChanged(changedSystem->getRootFolder(), FILE_METADATA_CHANGED); // Update root folder			
		});

		res.set_content("OK", "text/html");
	});

	try
	{
#if WIN32
		mHttpServer->listen("localhost", 1234);
#else
		mHttpServer->listen("0.0.0.0", 1234);
#endif
	}
	catch (...)
	{

	}
}