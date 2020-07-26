#include "platform.h"

#include <SDL_events.h>
#ifdef WIN32
#include <codecvt>
#else
#include <unistd.h>
#endif
#include <fcntl.h>
#include "Window.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include "Scripting.h"

int runShutdownCommand()
{
#ifdef WIN32 // windows
	return system("shutdown -s -t 0");
#else // osx / linux	
	return system("shutdown -h now");
#endif
}

int runRestartCommand()
{
#ifdef WIN32 // windows	
	return system("shutdown -r -t 0");
#else // osx / linux	
	return system("shutdown -r now");
#endif
}

int runSystemCommand(const std::string& cmd_utf8, const std::string& name, Window* window)
{
	if (window != NULL)
		window->renderSplashScreen();

#ifdef WIN32
	// on Windows we use _wsystem to support non-ASCII paths
	// which requires converting from utf8 to a wstring
	//typedef std::codecvt_utf8<wchar_t> convert_type;
	//std::wstring_convert<convert_type, wchar_t> converter;
	//std::wstring wchar_str = converter.from_bytes(cmd_utf8);
	std::string command = cmd_utf8;

#define BUFFER_SIZE 8192

	TCHAR szEnvPath[BUFFER_SIZE];
	DWORD dwLen = ExpandEnvironmentStringsA(command.c_str(), szEnvPath, BUFFER_SIZE);
	if (dwLen > 0 && dwLen < BUFFER_SIZE)
		command = std::string(szEnvPath);

	std::string exe;
	std::string args;

	Utils::FileSystem::splitCommand(command, &exe, &args);
	exe = Utils::FileSystem::getPreferredPath(exe);

	SHELLEXECUTEINFO lpExecInfo;
	lpExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	lpExecInfo.lpFile = exe.c_str();
	lpExecInfo.fMask = SEE_MASK_DOENVSUBST | SEE_MASK_NOCLOSEPROCESS;
	lpExecInfo.hwnd = NULL;
	lpExecInfo.lpVerb = "open"; // to open  program

	lpExecInfo.lpDirectory = NULL;
	lpExecInfo.nShow = SW_SHOW;  // show command prompt with normal window size 
	lpExecInfo.hInstApp = (HINSTANCE)SE_ERR_DDEFAIL;   //WINSHELLAPI BOOL WINAPI result;
	lpExecInfo.lpParameters = args.c_str(); //  file name as an argument	

	// Don't set directory for relative paths
	if (!Utils::String::startsWith(exe, ".") && !Utils::String::startsWith(exe, "/") && !Utils::String::startsWith(exe, "\\"))
		lpExecInfo.lpDirectory = Utils::FileSystem::getAbsolutePath(Utils::FileSystem::getParent(exe)).c_str();

	ShellExecuteEx(&lpExecInfo);

	if (lpExecInfo.hProcess != NULL)
	{
		if (window == NULL)
			WaitForSingleObject(lpExecInfo.hProcess, INFINITE);
		else
		{
			while (WaitForSingleObject(lpExecInfo.hProcess, 50) == 0x00000102L)
			{
				bool polled = false;

				SDL_Event event;
				while (SDL_PollEvent(&event))
					polled = true;

				if (window != NULL && polled)
					window->renderSplashScreen();
			}
		}

		CloseHandle(lpExecInfo.hProcess);
		return 0;
	}

	return 1;
#elif _ENABLEEMUELEC
	return system((cmd_utf8 + " 2> /storage/.config/emuelec/logs/es_launch_stderr.log > /storage/.config/emuelec/logs/es_launch_stdout.log").c_str()); // emuelec
#else
	return system((cmd_utf8 + " 2> /userdata/system/logs/es_launch_stderr.log | head -300 > /userdata/system/logs/es_launch_stdout.log").c_str()); // batocera
#endif
}

QuitMode quitMode = QuitMode::QUIT;

int quitES(QuitMode mode)
{
	quitMode = mode;

	switch (quitMode)
	{
		case QuitMode::QUIT:
			Scripting::fireEvent("quit");
			break;

		case QuitMode::REBOOT:
		case QuitMode::FAST_REBOOT:
			Scripting::fireEvent("quit", "reboot");
			Scripting::fireEvent("reboot");
			break;

		case QuitMode::SHUTDOWN:
		case QuitMode::FAST_SHUTDOWN:
			Scripting::fireEvent("quit", "shutdown");
			Scripting::fireEvent("shutdown");
			break;
	}


	SDL_Event* quit = new SDL_Event();
	quit->type = SDL_QUIT;
	SDL_PushEvent(quit);
	return 0;
}

void touch(const std::string& filename)
{
#ifndef WIN32
	int fd = open(filename.c_str(), O_CREAT | O_WRONLY, 0644);
	if (fd >= 0)
		close(fd);

	// system(("touch " + filename).c_str());
#endif	
}

void processQuitMode()
{

	switch (quitMode)
	{
	case QuitMode::RESTART:
		LOG(LogInfo) << "Restarting EmulationStation";
		touch("/tmp/restart.please");
		break;
	case QuitMode::REBOOT:
	case QuitMode::FAST_REBOOT:
		LOG(LogInfo) << "Rebooting system";
		touch("/tmp/reboot.please");
		runRestartCommand();
		break;
	case QuitMode::SHUTDOWN:
	case QuitMode::FAST_SHUTDOWN:
		LOG(LogInfo) << "Shutting system down";
		touch("/tmp/shutdown.please");
		runShutdownCommand();
		break;
	}
}

bool isFastShutdown()
{
	return quitMode == QuitMode::FAST_REBOOT || quitMode == QuitMode::FAST_SHUTDOWN;
}


#ifdef _ENABLEEMUELEC

/* < emuelec */
std::string getShOutput(const std::string& mStr)
{
    std::string result, file;
    FILE* pipe{popen(mStr.c_str(), "r")};
    char buffer[256];

    while(fgets(buffer, sizeof(buffer), pipe) != NULL)
    {
        file = buffer;
        result += file.substr(0, file.size() - 1);
    }

    pclose(pipe);
    return result;
}
/* emuelec >*/
#endif
