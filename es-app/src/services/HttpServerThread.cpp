#include "HttpServerThread.h"
#include "httplib.h"
#include "Log.h"

#ifdef WIN32
#include <Windows.h>
#endif

#include "utils/Platform.h"
#include "Gamelist.h"
#include "SystemData.h"
#include "FileData.h"
#include "views/ViewController.h"
#include <unordered_map>
#include "CollectionSystemManager.h"
#include "guis/GuiMenu.h"
#include "guis/GuiMsgBox.h"
#include "utils/FileSystemUtil.h"
#include "HttpApi.h"
#include "Settings.h"
#include "ApiSystem.h"

#include "ThreadedHasher.h"
#include "scrapers/ThreadedScraper.h"
#include "guis/GuiUpdate.h"
#include "ContentInstaller.h"

/* 

Misc APIS
-----------------
GET  /caps                                                      -> capability info
GET  /restart
GET  /quit
GET  /emukill
GET  /reloadgames
POST /messagebox												-> body must contain the message text as text/plain
POST /notify													-> body must contain the message text as text/plain
POST /launch													-> body must contain the exact file path as text/plain
GET  /runningGame
GET  /isIdle

System/Games APIS
-----------------
GET  /systems
GET  /systems/{systemName}
GET  /systems/{systemName}/logo
GET  /systems/{systemName}/games/{gameId}		
POST /systems/{systemName}/games/{gameId}						-> body must contain the game metadatas to save as application/json
GET  /systems/{systemName}/games/{gameId}/media/{mediaType}
POST /systems/{systemName}/games/{gameId}/media/{mediaType}		-> body must contain the file bytes to save. Content-type must be valid.

Store APIs
----------
POST /addgames/{systemName}										-> body must contain partial gamelist.xml file as application/xml
POST /removegames/{systemName}									-> body must contains partial gamelist.xml file as application/xml

File APIs
---------
GET /resources/{path relative to resources}"					-> any file in resources
GET /{path relative to resources/services}"						-> any other file in resources/services

*/
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

static std::map<std::string, std::string> mimeTypes = 
{
	{ "txt", "text/plain" },
	{ "html", "text/html" },
	{ "htm", "text/html" },
	{ "css", "text/css" },
	{ "jpeg", "image/jpg" },
	{ "jpg", "image/jpg" },
	{ "png", "image/png" },
	{ "gif", "image/gif" },
	{ "webp", "image/webp" },
	{ "svg", "image/svg+xml" },
	{ "ico", "image/x-icon" },
	{ "json", "application/json" },
	{ "pdf", "application/pdf" },
	{ "js", "application/javascript" },
	{ "wasm", "application/wasm" },
	{ "xml", "application/xml" },
	{ "xhtml", "application/xhtml+xml" }
};

std::string HttpServerThread::getMimeType(const std::string &path)
{
	auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(path));
	if (ext[0] == '.')
		ext = ext.substr(1);

	auto it = mimeTypes.find(ext);
	if (it != mimeTypes.cend())
		return it->second;
	
	return "text/plain";
}

static bool isAllowed(const httplib::Request& req, httplib::Response& res)
{
	if (req.remote_addr != "127.0.0.1" && !Settings::getInstance()->getBool("PublicWebAccess"))
	{
		LOG(LogWarning) << "HttpServerThread : Access disabled for " + req.remote_addr;

		res.set_content("403 - Forbidden", "text/html");
		res.status = 403;
		return false;
	}

	return true;
}

void HttpServerThread::run()
{
	mHttpServer = new httplib::Server();

	mHttpServer->Get("/", [=](const httplib::Request & req, httplib::Response &res) 
	{
		if (!isAllowed(req, res))
			return;

		res.set_redirect("/index.html");
	});

	mHttpServer->Get("/favicon.png", [](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

		auto data = ResourceManager::getInstance()->getFileData(":/window_icon_256.png");
		if (data.ptr)
			res.set_content((char*)data.ptr.get(), data.length, "image/png");
	});

	mHttpServer->Get("/index.html", [](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

		auto data = ResourceManager::getInstance()->getFileData(":/services/index.html");
		if (data.ptr)
		{
			res.set_content((char*)data.ptr.get(), data.length, "text/html");
			return;
		}

		res.set_content(
			"<!DOCTYPE html>\r\n"
		    "<html lang='fr'>\r\n"
			"<head>\r\n"			
			"<title>EmulationStation services</title>\r\n"
			"<link rel=\"shortcut icon\" href=\"favicon.png\">\r\n"
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

			"function emuKill() {\r\n"
			"var xhr = new XMLHttpRequest();\r\n"
			"xhr.open('GET', '/emukill');\r\n"
			"xhr.send(); }\r\n"

			"</script>\r\n"

			"<img src='vid.jpg'/>\r\n"

			"<input type='button' value='Reload games' onClick='reloadGamelists()'/>\r\n"
			"<br/>"
			"<input type='button' value='Quit' onClick='quitES()'/>\r\n"
			"<br/>"
			"<input type='button' value='Kill emulator' onClick='emuKill()'/>\r\n"

			"</body>\r\n</html>", "text/html");
	});

	mHttpServer->Get("/quit", [this](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

#if BATOCERA
		// http://127.0.0.1/quit?confirm=switchscreen
		if (req.has_param("confirm") && req.get_param_value("confirm") == "switchscreen") 
		{
			Window* win = mWindow;
			mWindow->postToUiThread([win]() { win->pushGui(new GuiMsgBox(win, _("DO YOU WANT TO SWITCH THE SCREEN ?"), _("YES"), [] { Utils::Platform::quitES(); }, _("NO"), nullptr)); });
			return;
		}
#endif

		// http://127.0.0.1/quit?confirm=menu
		if (req.has_param("confirm") && req.get_param_value("confirm") == "menu")
		{
			GuiMenu::openQuitMenu_static(mWindow);
			return;
		}

		Utils::Platform::quitES();
	});

	mHttpServer->Get("/restart", [](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

		Utils::Platform::quitES(Utils::Platform::QuitMode::REBOOT);
	});

	mHttpServer->Get("/emukill", [](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

		ApiSystem::getInstance()->emuKill();
	});

	mHttpServer->Get("/caps", [](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

		res.set_content(HttpApi::getCaps(), "application/json");
	});

	mHttpServer->Get("/systems", [](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

		res.set_content(HttpApi::getSystemList(), "application/json");
	});

	mHttpServer->Get("/runningGame", [](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

		std::string ret = HttpApi::getRunnningGameInfo();
		if (ret.empty())
		{
			res.set_content("{\"msg\":\"NO GAME RUNNING\"}", "application/json");
			res.status = 201;
		}
		else
			res.set_content(ret, "application/json");
	});

	mHttpServer->Get("/isIdle", [](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

		bool idle = 
			HttpApi::getRunnningGameInfo().empty() && 
			!ThreadedScraper::isRunning() && 
			!ContentInstaller::isRunning() &&
			!ThreadedHasher::isRunning() && 
			GuiUpdate::state != GuiUpdateState::UPDATER_RUNNING;

		if (idle)
		{
			res.set_content("[ true ]", "application/json");
			res.status = 200;
		}
		else
		{
			res.set_content("[ false ]", "application/json");
			res.status = 201;
		}
	});	

	mHttpServer->Get(R"(/systems/(/?.*)/logo)", [](const httplib::Request& req, httplib::Response& res)
	{		
		if (!isAllowed(req, res))
			return;

		std::string systemName = req.matches[1];
		SystemData* system = SystemData::getSystem(systemName);
		if (system != nullptr)
		{
			auto theme = system->getTheme();
			if (theme != nullptr)
			{
				const ThemeData::ThemeElement* elem = theme->getElement("system", "logo", "image");
				if (elem && elem->has("path"))
				{
					std::string logo = elem->get<std::string>("path");
					auto data = ResourceManager::getInstance()->getFileData(logo);
					if (data.ptr)
					{
						res.set_content((char*)data.ptr.get(), data.length, getMimeType(logo).c_str());
						return;
					}
				}
			}
		}

		res.set_content("404 not found", "text/html");
		res.status = 404;
	});
	
	mHttpServer->Get(R"(/systems/(/?.*)/games)", [](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

		std::string systemName = req.matches[1];
		SystemData* system = SystemData::getSystem(systemName);
		if (system != nullptr)
		{
			res.set_content(HttpApi::getSystemGames(system), "application/json");
			return;
		}
		
		res.set_content("404 system not found", "text/html");
		res.status = 404;		
	});

	mHttpServer->Get(R"(/systems/(/?.*)/games/(/?.*)/media/(/?.*))", [](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

		std::string systemName = req.matches[1];
		SystemData* system = SystemData::getSystem(systemName);
		if (system != nullptr)
		{
			std::string gameId = req.matches[2];
			auto game = HttpApi::findFileData(system, gameId);
			if (game != nullptr)
			{
				std::string metadataName = req.matches[3];

				if (game->getMetadata().getType(metadataName) == MD_PATH)
				{
					std::string path = game->getMetadata().get(metadataName);
					if (!path.empty())
					{
						auto data = ResourceManager::getInstance()->getFileData(path);
						if (data.ptr)
						{
							res.set_content((char*)data.ptr.get(), data.length, getMimeType(path).c_str());
							return;
						}

						return;
					}
				}
			}
		}

		res.set_content("404 media not found", "text/html");
		res.status = 404;
	});

	mHttpServer->Post(R"(/systems/(/?.*)/games/(/?.*)/media/(/?.*))", [this](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

		if (req.body.empty())
		{
			res.set_content("400 bad request - body is missing", "text/html");
			res.status = 400;
			return;
		}

		if (!req.has_header("Content-Type"))
		{
			res.set_content("400 missing content-type", "text/html");
			res.status = 400;
			return;
		}

		std::string contentType = req.get_header_value("Content-Type");
		
		std::string systemName = req.matches[1];
		SystemData* system = SystemData::getSystem(systemName);
		if (system != nullptr)
		{
			std::string gameId = req.matches[2];
			auto game = HttpApi::findFileData(system, gameId);
			if (game != nullptr)
			{
				std::string metadataName = req.matches[3];

				if (game->getMetadata().getType(metadataName) == MD_PATH)
				{
					if (HttpApi::ImportMedia(game, metadataName, contentType, req.body))
					{
						if (ViewController::hasInstance())
							mWindow->postToUiThread([game]() { ViewController::get()->onFileChanged(game, FileChangeType::FILE_METADATA_CHANGED); });

						return;
					}
				}
			}
		}

		res.set_content("404 media not found", "text/html");
		res.status = 404;
	});


	mHttpServer->Post(R"(/systems/(/?.*)/games/(/?.*))", [this](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

		if (req.body.empty())
		{
			res.set_content("400 bad request - body is missing", "text/html");
			res.status = 400;
			return;
		}

		std::string systemName = req.matches[1];
		SystemData* system = SystemData::getSystem(systemName);
		if (system != nullptr)
		{
			std::string gameId = req.matches[2];
			auto game = HttpApi::findFileData(system, gameId);
			if (game != nullptr)
			{
				if (HttpApi::ImportFromJson(game, req.body))
				{
					if (ViewController::hasInstance())
						mWindow->postToUiThread([game]() { ViewController::get()->onFileChanged(game, FileChangeType::FILE_METADATA_CHANGED); });					

					return;
				}
			}
		}

		res.set_content("404 game not found", "text/html");
		res.status = 404;
	});


	mHttpServer->Get(R"(/systems/(/?.*)/games/(/?.*))", [](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

		std::string systemName = req.matches[1];
		SystemData* system = SystemData::getSystem(systemName);
		if (system != nullptr)
		{
			std::string gameId = req.matches[2];
			auto game = HttpApi::findFileData(system, gameId);
			if (game != nullptr)
			{
				bool localpaths = req.has_param("localpaths") && req.get_param_value("localpaths") == "true";
				res.set_content(HttpApi::ToJson(game, localpaths), "application/json");
				return;
			}
		}

		res.set_content("404 game not found", "text/html");
		res.status = 404;
	});

	mHttpServer->Get(R"(/systems/(/?.*))", [](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

		std::string systemName = req.matches[1];
		SystemData* system = SystemData::getSystem(systemName);
		if (system != nullptr)
		{
			bool localpaths = req.has_param("localpaths") && req.get_param_value("localpaths") == "true";
			res.set_content(HttpApi::ToJson(system, localpaths), "application/json");
			return;
		}

		res.set_content("404 not found", "text/html");
		res.status = 404;
	});

	
	mHttpServer->Get("/reloadgames", [this](const httplib::Request& req, httplib::Response& res)
	{	
		if (!isAllowed(req, res))
			return;

		Window* w = mWindow;
		mWindow->postToUiThread([w]()
		{
			GuiMenu::updateGameLists(w, false);
		});
	});

	mHttpServer->Post("/messagebox", [this](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

		if (req.body.empty())
		{
			res.set_content("400 bad request - body is missing", "text/html");
			res.status = 400;
			return;
		}

		auto msg = req.body;
		Window* w = mWindow;
		mWindow->postToUiThread([msg, w]() { w->pushGui(new GuiMsgBox(w, msg)); });
	});

	mHttpServer->Post("/notify", [this](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

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
		if (!isAllowed(req, res))
			return;

		if (req.body.empty())
		{
			res.set_content("400 bad request - body is missing", "text/html");
			res.status = 400;
			return;
		}

		auto path = Utils::FileSystem::getAbsolutePath(req.body);

		for (auto system : SystemData::sSystemVector)
		{
			if (system->isCollection() || !system->isGameSystem())
				continue;

			for (auto file : system->getRootFolder()->getFilesRecursive(GAME))
			{
				if (file->getFullPath() == path || file->getPath() == path)
				{
					mWindow->postToUiThread([file]() { ViewController::get()->launch(file); });					
					return;
				}
			}
		}
	});

	mHttpServer->Post(R"(/addgames/(/?.*))", [this](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

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
	
		for (auto file : fileList)
			file->getMetadata().setDirty();

		for (auto file : system->getRootFolder()->getFilesRecursive(GAME))
			if (fileMap.find(file->getPath()) != fileMap.cend())
				file->getMetadata().setDirty();

		updateGamelist(system);

		if (deleteSystem)
		{		
			delete system;

			res.set_content("201 Game added. System not updated", "text/html");
			res.status = 201;

			Window* w = mWindow;
			mWindow->postToUiThread([w]() { GuiMenu::updateGameLists(w, false); });
		}
		else if (ViewController::hasInstance())
		{
			mWindow->postToUiThread([system]()
			{
				ViewController::get()->onFileChanged(system->getRootFolder(), FILE_METADATA_CHANGED); // Update root folder			
			});
		}

		res.set_content("OK", "text/html");
	});
	
	mHttpServer->Post(R"(/removegames/(/?.*))", [this](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res))
			return;

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

			for (auto sys : SystemData::sSystemVector)
			{
				if (!sys->isCollection())
					continue;

				auto copy = sys->getRootFolder()->FindByPath(filePath);
				if (copy != nullptr)
				{
					sys->getRootFolder()->removeFromVirtualFolders(file);
					systems.push_back(sys);
				}
			}

			system->getRootFolder()->removeFromVirtualFolders(file);
			// delete file; intentionnal mem leak
		}

		if (ViewController::hasInstance())
		{
			mWindow->postToUiThread([systems]()
			{
				for (auto changedSystem : systems)
					ViewController::get()->onFileChanged(changedSystem->getRootFolder(), FILE_REMOVED); // Update root folder			
			});
		}
		
		res.set_content("OK", "text/html");
	});

	mHttpServer->Get(R"(/resources/(/?.*))", [](const httplib::Request& req, httplib::Response& res)  // (.*)
	{
		if (!isAllowed(req, res))
			return;

		std::string url = req.matches[1];
		auto data = ResourceManager::getInstance()->getFileData(":/" + url);
		if (data.ptr)
			res.set_content((char*)data.ptr.get(), data.length, getMimeType(url).c_str());
		else
		{
			res.set_content("404 not found", "text/html");
			res.status = 404;
			return;
		}
	});

	mHttpServer->Get(R"(/(/?.*))", [](const httplib::Request& req, httplib::Response& res)  // (.*)
	{
		if (!isAllowed(req, res))
			return;

		std::string url = req.matches[1];

		auto data = ResourceManager::getInstance()->getFileData(":/services/" + url);
		if (data.ptr)
			res.set_content((char*)data.ptr.get(), data.length, getMimeType(url).c_str());
		else 
		{
			res.set_content("404 not found", "text/html");
			res.status = 404;
			return;
		}
	});

	try
	{
		std::string ip = "127.0.0.1";

		if (Settings::getInstance()->getBool("PublicWebAccess"))
			ip = "0.0.0.0";

		mHttpServer->listen(ip.c_str(), 1234);
	}
	catch (...)
	{

	}
}
