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

int runShutdownCommand()
{
#ifdef WIN32 // windows
	return system("shutdown -s -t 0");
#else // osx / linux
	return system("poweroff"); // batocera
#endif
}

int runRestartCommand()
{
#ifdef WIN32 // windows
	return system("shutdown -r -t 0");
#else // osx / linux
	return system("reboot"); // batocera
#endif
}

#ifdef WIN32
void splitCommand(std::string cmd, std::string* executable, std::string* parameters)
{
	std::string c = Utils::String::trim(cmd);
	size_t exec_end;

	if (c[0] == '\"')
	{
		exec_end = c.find_first_of('\"', 1);
		if (std::string::npos != exec_end)
		{
			*executable = c.substr(1, exec_end - 1);
			*parameters = c.substr(exec_end + 1);
		}
		else
		{
			*executable = c.substr(1, exec_end);
			std::string().swap(*parameters);
		}
	}
	else
	{
		exec_end = c.find_first_of(' ', 0);
		if (std::string::npos != exec_end)
		{
			*executable = c.substr(0, exec_end);
			*parameters = c.substr(exec_end + 1);
		}
		else
		{
			*executable = c.substr(0, exec_end);
			std::string().swap(*parameters);
		}
	}
}
#endif

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

	splitCommand(command, &exe, &args);
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
	return system((cmd_utf8 + " 2> /storage/.config/emuelec/logs/es_launch_stderr.log | head -300 > //storage/.config/emuelec/logs/es_launch_stdout.log").c_str()); // emuelec
#else
	return system((cmd_utf8 + " 2> /userdata/system/logs/es_launch_stderr.log | head -300 > /userdata/system/logs/es_launch_stdout.log").c_str()); // batocera
#endif
}

int quitES(const std::string& filename)
{
	if (!filename.empty())
		touch(filename);
	SDL_Event* quit = new SDL_Event();
	quit->type = SDL_QUIT;
	SDL_PushEvent(quit);
	return 0;
}

void touch(const std::string& filename)
{
#ifdef WIN32
	FILE* fp = fopen(filename.c_str(), "ab+");
	if (fp != NULL)
		fclose(fp);
#else
	int fd = open(filename.c_str(), O_CREAT|O_WRONLY, 0644);
	if (fd >= 0)
		close(fd);
#endif
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
