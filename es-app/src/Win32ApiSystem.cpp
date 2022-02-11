#if WIN32

#include "Win32ApiSystem.h"
#include "Log.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "platform.h"
#include "EmulationStation.h"
#include "HttpReq.h"
#include <Windows.h>
#include <ShlDisp.h>
#include <comutil.h> // #include for _bstr_t
#include <thread>
#include <direct.h>
#include <algorithm>
#include "LocaleES.h"
#include "Paths.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comsuppw.lib" ) // link with "comsuppw.lib" (or debug version: "comsuppwd.lib")
#pragma comment(lib, "ws2_32.lib")

#define VERSIONURL "https://github.com/fabricecaruso/batocera-emulationstation/releases/download/continuous-stable/version.info"
#define UPDATEURL  "https://github.com/fabricecaruso/batocera-emulationstation/releases/download/continuous-stable/EmulationStation-Win32.zip"

#define LAUNCHERURL "https://github.com/fabricecaruso/batocera-ports/releases/download/continuous/batocera-ports.zip"

Win32ApiSystem::Win32ApiSystem()
{
	m_hJob = CreateJobObject(NULL, NULL);
	if (m_hJob)
	{
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
		jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		SetInformationJobObject(m_hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
	}
}

void Win32ApiSystem::deinit()
{
	if (m_hJob)
	{
		CloseHandle(m_hJob);
		m_hJob = NULL;
	}
}

std::string getUrlFromUpdateType(std::string url)
{
	std::string updatesType = Settings::getInstance()->getString("updates.type");
	if (updatesType == "beta")
		return Utils::String::replace(url, "continuous-stable", "continuous-master");
	else if (updatesType == "unstable")
		return Utils::String::replace(url, "continuous-stable", "continuous-beta");

	return url;
}

bool Win32ApiSystem::isScriptingSupported(ScriptId script)
{
#if _DEBUG
	if (script == ApiSystem::DISKFORMAT)
		return true;
#endif

	if (script == ApiSystem::NETPLAY)
		return !getSevenZipCommand().empty();

	if (script == ApiSystem::KODI)
		return Utils::FileSystem::exists(Paths::getKodiPath());

	if (script == ApiSystem::THEMESDOWNLOADER)
		return true;

	if (script == ApiSystem::DECORATIONS || script == ApiSystem::THEBEZELPROJECT)
		return Utils::FileSystem::exists(Paths::getUserDecorationsPath()) || Utils::FileSystem::exists(Paths::getDecorationsPath()) || Utils::FileSystem::exists(Paths::getUserEmulationStationPath() + "/decorations");

	if (script == ApiSystem::SHADERS)
		return Utils::FileSystem::exists(Paths::getShadersPath()) || Utils::FileSystem::exists(Paths::getUserShadersPath());

	std::vector<std::string> executables;

	switch (script)
	{
	case ApiSystem::WIFI:
		executables.push_back("batocera-wifi");
		break;
	case ApiSystem::RETROACHIVEMENTS:
#ifdef CHEEVOS_DEV_LOGIN
		executables.push_back("emulatorLauncher");
#endif
		break;
	case ApiSystem::BLUETOOTH:
#if _DEBUG
		executables.push_back("batocera-bluetooth");
#endif
		break;
	case ApiSystem::RESOLUTION:
		executables.push_back("batocera-resolution");
		break;
	case ApiSystem::BIOSINFORMATION:
		executables.push_back("batocera-systems");
		break;
	case ApiSystem::NETPLAY:
	case ApiSystem::GAMESETTINGS:
	case ApiSystem::DECORATIONS:
	case ApiSystem::SHADERS:
		executables.push_back("emulatorLauncher");
		break;
	case ApiSystem::PDFEXTRACTION:
		executables.push_back("pdftoppm");
		executables.push_back("pdfinfo");		
		break;
	case ApiSystem::BATOCERASTORE:
		executables.push_back("batocera-store");
		break;
	case ApiSystem::EVMAPY:
		executables.push_back("emulatorLauncher");
		break;
	case ApiSystem::PADSINFO:
		executables.push_back("batocera-padsinfo");
		break;
	case ApiSystem::UPGRADE:
		return true;
	}

	if (executables.size() == 0)
		return false;

	for (auto executable : executables)	
	{
		std::string path = Paths::findEmulationStationFile(executable + ".exe");
		if (path.empty() || !Utils::FileSystem::exists(path))
			return false;
	}

	return true;
}

int Win32ApiSystem::executeCMD(const char* lpCommandLine, std::string& output, const char* lpCurrentDirectory, const std::function<void(const std::string)>& func)
{
	int ret = -1;
	output = "";

	std::string lineOutput;

#define BUFSIZE		8192

	STARTUPINFOW si;
	SECURITY_ATTRIBUTES sa;
	PROCESS_INFORMATION pi;
	HANDLE g_hChildStd_IN_Rd, g_hChildStd_OUT_Wr, g_hChildStd_OUT_Rd, g_hChildStd_IN_Wr;  //pipe handles
	char buf[BUFSIZE];           //i/o buffer

	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	if (CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &sa, 0))   //create stdin pipe
	{
		if (CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &sa, 0))  //create stdout pipe
		{
			//set startupinfo for the spawned process
			/*The dwFlags member tells CreateProcess how to make the process.
			STARTF_USESTDHANDLES: validates the hStd* members.
			STARTF_USESHOWWINDOW: validates the wShowWindow member*/
			GetStartupInfoW(&si);

			si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
			si.wShowWindow = SW_HIDE;
			//set the new handles for the child process
			si.hStdOutput = g_hChildStd_OUT_Wr;
			si.hStdError = g_hChildStd_OUT_Wr;
			si.hStdInput = g_hChildStd_IN_Rd;

			//spawn the child process
			std::wstring commandLineW = Utils::String::convertToWideString(lpCommandLine);
			std::wstring directory = lpCurrentDirectory == NULL ? L"" : Utils::String::convertToWideString(lpCurrentDirectory);
			if (CreateProcessW(NULL, (LPWSTR)commandLineW.c_str(), NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, lpCurrentDirectory == NULL ? NULL : (LPWSTR)directory.c_str(), &si, &pi))
			{
				if (m_hJob != nullptr)
					AssignProcessToJobObject(m_hJob, pi.hProcess);

				unsigned long bread;   //bytes read
				unsigned long avail;   //bytes available
				memset(buf, 0, sizeof(buf));

				for (;;)
				{
					if (!PeekNamedPipe(g_hChildStd_OUT_Rd, buf, BUFSIZE-1, &bread, &avail, NULL))
						break;

					if (bread > 0)
					{
						if (!ReadFile(g_hChildStd_OUT_Rd, buf, BUFSIZE - 1, &bread, NULL))
							break;

						buf[bread] = 0;

						std::string data = std::string(buf);
						output += data;

						if (func != nullptr)
						{
							lineOutput += data;

							auto pos = lineOutput.find("\r\n");
							while (pos != std::string::npos)
							{
								std::string line = Utils::String::replace(Utils::String::trim(lineOutput.substr(0, pos)), "\f", "");
								func(line);

								lineOutput = lineOutput.substr(pos + 2);
								pos = lineOutput.find("\r\n");
							}
						}
					}

					if (WaitForSingleObject(pi.hProcess, 10) == WAIT_OBJECT_0)
					{
						if (!PeekNamedPipe(g_hChildStd_OUT_Rd, buf, BUFSIZE - 1, &bread, &avail, NULL))
							break;

						if (bread == 0)
							break;
					}
				}

				DWORD exit_code = 0;
				if (GetExitCodeProcess(pi.hProcess, &exit_code))
					ret = exit_code;
				else
					ret = 0;

				//clean up all handles
				CloseHandle(pi.hThread);
				CloseHandle(pi.hProcess);
				CloseHandle(g_hChildStd_IN_Rd);
				CloseHandle(g_hChildStd_OUT_Wr);
				CloseHandle(g_hChildStd_OUT_Rd);
				CloseHandle(g_hChildStd_IN_Wr);
			}
			else
			{

				CloseHandle(g_hChildStd_IN_Rd);
				CloseHandle(g_hChildStd_OUT_Wr);
				CloseHandle(g_hChildStd_OUT_Rd);
				CloseHandle(g_hChildStd_IN_Wr);
				ret = -1;
			}
		}
		else
		{
			CloseHandle(g_hChildStd_IN_Rd);
			CloseHandle(g_hChildStd_IN_Wr);
			ret = -1;
		}
	}

	return ret;
}

bool downloadGitRepository(const std::string url, const std::string fileName, const std::string label, const std::function<void(const std::string)>& func)
{
	if (func != nullptr)
		func("Downloading " + label);

	long downloadSize = 0;

	std::string statUrl = Utils::String::replace(url, "https://github.com/", "https://api.github.com/repos/");
	if (statUrl != url)
	{
		HttpReq statreq(statUrl);
		if (statreq.wait())
		{
			std::string content = statreq.getContent();
			auto pos = content.find("\"size\": ");
			if (pos != std::string::npos)
			{
				auto end = content.find(",", pos);
				if (end != std::string::npos)
					downloadSize = atoi(content.substr(pos + 8, end - pos - 8).c_str()) * 1024;
			}
		}
	}

	HttpReq httpreq(url + "/archive/master.zip", fileName);

	int curPos = -1;
	while (httpreq.status() == HttpReq::REQ_IN_PROGRESS)
	{
		if (downloadSize > 0)
		{
			double pos = httpreq.getPosition();
			if (pos > 0 && curPos != pos)
			{
				if (func != nullptr)
				{
					std::string pc = std::to_string((int)(pos * 100.0 / downloadSize));
					func(std::string("Downloading " + label + " >>> " + pc + " %"));
				}

				curPos = pos;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	if (httpreq.status() != HttpReq::REQ_SUCCESS)
		return false;

	return true;
}

bool Win32ApiSystem::executeScript(const std::string command)
{
	LOG(LogInfo) << "Running " << command;

	std::string output;
	return executeCMD(command.c_str(), output) == 0;
}

std::pair<std::string, int> Win32ApiSystem::executeScript(const std::string command, const std::function<void(const std::string)>& func)
{
	std::string executable;
	std::string parameters;
	Utils::FileSystem::splitCommand(command, &executable, &parameters);

	std::string path = Paths::findEmulationStationFile(executable + ".exe");
	if (!path.empty() && Utils::FileSystem::exists(path))
	{
		std::string cmd = parameters.empty() ? path : path + " " + parameters;

		std::string output;
		auto ret = executeCMD(cmd.c_str(), output);
		return std::pair<std::string, int>(output, ret);
	}

	return std::pair<std::string, int>("", -1);
}

std::vector<std::string> Win32ApiSystem::executeEnumerationScript(const std::string command)
{
	LOG(LogDebug) << "ApiSystem::executeEnumerationScript -> " << command;

	std::vector<std::string> res;

	std::string executable;
	std::string parameters;
	Utils::FileSystem::splitCommand(command, &executable, &parameters);

	std::string path;

	if (executable.find(":") != std::string::npos && Utils::FileSystem::exists(executable))
		path = Utils::FileSystem::getPreferredPath(executable);
	else
		path = Paths::findEmulationStationFile(executable + ".exe");

	if (Utils::FileSystem::exists(path))
	{
		std::string cmd = parameters.empty() ? path : path + " " + parameters;

		std::string output;
		if (executeCMD(cmd.c_str(), output) == 0)
		{
			for (std::string all : Utils::String::splitAny(output, "\r\n"))
				res.push_back(all);
		}
	}

	return res;
}

unsigned long Win32ApiSystem::getFreeSpaceGB(std::string mountpoint)
{
	LOG(LogDebug) << "ApiSystem::getFreeSpaceGB";

	unsigned __int64 i64FreeBytesToCaller, i64TotalBytes, i64FreeBytes;

	std::string drive = Paths::getUserEmulationStationPath()[0] + std::string(":");

	BOOL  fResult = GetDiskFreeSpaceExA(drive.c_str(),
		(PULARGE_INTEGER)&i64FreeBytesToCaller,
		(PULARGE_INTEGER)&i64TotalBytes,
		(PULARGE_INTEGER)&i64FreeBytes);

	if (fResult)
		return i64FreeBytes / (1024 * 1024 * 1024);

	return 0;
}

std::pair<std::string, int> Win32ApiSystem::installBatoceraTheme(std::string thname, const std::function<void(const std::string)>& func)
{
	for (auto theme : getBatoceraThemesList())
	{
		if (theme.name != thname)
			continue;
		
		std::string themeFileName = Utils::FileSystem::getFileName(theme.url);
		std::string zipFile = Paths::getUserEmulationStationPath() + "/themes/" + themeFileName + ".zip";

		Utils::FileSystem::removeFile(zipFile);

		if (downloadGitRepository(theme.url, zipFile, thname, func))
		{
			if (func != nullptr)
				func(_("Extracting") + " " + thname);

			unzipFile(zipFile, Paths::getUserEmulationStationPath() + "/themes");
			Utils::FileSystem::removeFile(zipFile);

			std::string folderName = Paths::getUserEmulationStationPath() + "/themes/" + themeFileName + "-master";
			if (Utils::FileSystem::exists(folderName))
			{
				std::string finalfolderName = Paths::getUserEmulationStationPath() + "/themes/" + themeFileName;
				if (Utils::FileSystem::exists(finalfolderName))
					Utils::FileSystem::deleteDirectoryFiles(finalfolderName, true);

				Utils::FileSystem::renameFile(folderName, finalfolderName);
			}

			return std::pair<std::string, int>(std::string("OK"), 0);
		}
	}

	return std::pair<std::string, int>(std::string(""), 1);
}

std::pair<std::string, int> Win32ApiSystem::uninstallBatoceraTheme(std::string thname, const std::function<void(const std::string)>& func)
{
	for (auto theme : getBatoceraThemesList())
	{
		if (!theme.isInstalled || theme.name != thname)
			continue;
		
		std::string themeFileName = Utils::FileSystem::getFileName(theme.url);
		std::string folderName = Paths::getUserEmulationStationPath() + "/themes/" + themeFileName + "-master";
			
		if (!Utils::FileSystem::exists(folderName))
			folderName = Utils::String::replace(folderName, "-master", "");

		if (Utils::FileSystem::exists(folderName))
		{
			Utils::FileSystem::deleteDirectoryFiles(folderName, true);
			return std::pair<std::string, int>("OK", 0);
		}

		break;
	}

	return std::pair<std::string, int>(std::string(""), 1);
}

std::vector<BatoceraTheme> Win32ApiSystem::getBatoceraThemesList()
{
	LOG(LogDebug) << "Win32ApiSystem::getBatoceraThemesList";

	std::vector<BatoceraTheme> res;

	HttpReq httpreq(getUpdateUrl() + "/themes.txt");
	if (httpreq.wait())
	{
		auto lines = Utils::String::split(httpreq.getContent(), '\n');
		for (auto line : lines)
		{
			auto parts = Utils::String::splitAny(line, " \t");
			if (parts.size() > 1)
			{
				auto themeName = parts[0];
				std::string themeUrl = Utils::FileSystem::getFileName(parts[1]);

				bool themeExists = false;

				std::vector<std::string> paths =
				{
					Paths::getUserThemesPath(),
					Paths::getThemesPath(),
					Paths::getUserEmulationStationPath() + "/themes",
					"/etc/emulationstation/themes" // Backward compatibility with Retropie
				};
				
				for (auto path : paths)
				{
					if (Utils::FileSystem::isDirectory(path + "/" + themeUrl + "-master"))
					{
						themeExists = true;
						break;
					}
					else if (Utils::FileSystem::isDirectory(path + "/" + themeUrl))
					{
						themeExists = true;
						break;
					}
					else if (Utils::FileSystem::isDirectory(path + "/" + themeName))
					{
						themeExists = true;
						break;
					}
				}

				auto parts = Utils::String::splitAny(line, " \t");
				if (parts.size() < 2)
					continue;

				BatoceraTheme bt;
				bt.isInstalled = themeExists;
				bt.name = themeName;
				bt.url = parts[1];
				res.push_back(bt);
			}
		}
	}

	getBatoceraThemesImages(res);

	return res;
}

std::vector<std::string> Win32ApiSystem::getSystemInformations()
{
	LOG(LogDebug) << "ApiSystem::getSystemInformations";

	std::vector<std::string> res;

	int CPUInfo[4] = { -1 };
	unsigned   nExIds, i = 0;
	char CPUBrandString[0x40];
	// Get the information associated with each extended ID.
	__cpuid(CPUInfo, 0x80000000);
	nExIds = CPUInfo[0];
	for (i = 0x80000000; i <= nExIds; ++i)
	{
		__cpuid(CPUInfo, i);
		// Interpret CPU brand string
		if (i == 0x80000002)
			memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
		else if (i == 0x80000003)
			memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
		else if (i == 0x80000004)
			memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
	}

	//string includes manufacturer, model and clockspeed
	res.push_back("CPU:" + std::string(CPUBrandString));

	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	res.push_back("CORES:" + std::to_string(sysInfo.dwNumberOfProcessors));

	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof(statex);
	GlobalMemoryStatusEx(&statex);
	res.push_back("MEMORY:" + std::to_string((statex.ullTotalPhys / 1024) / 1024) + "MB");

	return res;
}

std::vector<std::string> Win32ApiSystem::getAvailableStorageDevices()
{
	std::vector<std::string> res;
	return res;
}

std::vector<BatoceraBezel> Win32ApiSystem::getBatoceraBezelsList()
{
	LOG(LogDebug) << "ApiSystem::getBatoceraBezelsList";

	std::vector<BatoceraBezel> res;

	HttpReq request(getUpdateUrl() + "/bezels.txt");
	if (request.wait())
	{
		auto lines = Utils::String::split(request.getContent(), '\n');
		for (auto line : lines)
		{
			auto parts = Utils::String::splitAny(line, " \t");
			if (parts.size() < 2)
				continue;

			BatoceraBezel bz;
			bz.name = parts[0];
			bz.url = parts[1];
			bz.folderPath = parts.size() < 3 ? "" : parts[2];

			std::string theBezelProject = Paths::getUserDecorationsPath() + "/thebezelproject/games/" + parts[0];
			bz.isInstalled = Utils::FileSystem::exists(theBezelProject);
			
			if (bz.name != "?")
				res.push_back(bz);
		}
	}

	return res;
}

std::pair<std::string, int> Win32ApiSystem::installBatoceraBezel(std::string bezelsystem, const std::function<void(const std::string)>& func)
{
	LOG(LogDebug) << "ApiSystem::installBatoceraBezel";

	for (auto bezel : getBatoceraBezelsList())
	{		
		if (bezel.name == bezelsystem)
		{
			std::string themeUrl = bezel.url;
			std::string subFolder = bezel.folderPath;

			std::string themeFileName = Utils::FileSystem::getFileName(themeUrl);
			std::string zipFile = Utils::FileSystem::getCanonicalPath(Paths::getUserDecorationsPath() + "/" + themeFileName + ".zip");

			if (downloadGitRepository(themeUrl, zipFile, bezelsystem, func))
			{
				std::string theBezelProject = Paths::getUserDecorationsPath() + "/thebezelproject/games/"+ bezelsystem;
				Utils::FileSystem::createDirectory(theBezelProject);

				std::string tmp = Paths::getUserDecorationsPath() + "/thebezelproject/games/" + bezelsystem + "/tmp";
				Utils::FileSystem::createDirectory(tmp);

				if (func != nullptr)
					func(_("Extracting") + " " + bezelsystem+ " bezels");
				
				auto shouldProcessFile = [subFolder](const std::string name)
				{
					std::string ext = Utils::FileSystem::getExtension(name);
					if (ext != ".cfg" && ext != ".png")
						return false;

					if (!subFolder.empty() && Utils::FileSystem::getGenericPath(name).find(subFolder.c_str()) == std::string::npos)
						return false;

					if (subFolder.empty() && name.find("/overlay/GameBezels/") == std::string::npos)
						return false;

					return true;
				};				

				unzipFile(zipFile, tmp, shouldProcessFile);

				Utils::FileSystem::removeFile(zipFile);

				auto files = Utils::FileSystem::getDirContent(tmp, true, true);
				for (auto file : files)
				{
					if (!shouldProcessFile(file))
						continue;
					
					std::string dest = theBezelProject + "/" + Utils::FileSystem::getFileName(file);
					Utils::FileSystem::renameFile(file, dest);
				}

				Utils::FileSystem::deleteDirectoryFiles(tmp, true);

				return std::pair<std::string, int>(std::string("OK"), 0);
			}

			break;
		}
	}

	return std::pair<std::string, int>("", 1);
}

std::pair<std::string, int> Win32ApiSystem::uninstallBatoceraBezel(std::string bezelsystem, const std::function<void(const std::string)>& func)
{
	std::string theBezelProject = Paths::getUserDecorationsPath() + "/thebezelproject/games/" + bezelsystem;
	Utils::FileSystem::deleteDirectoryFiles(theBezelProject, true);

	return std::pair<std::string, int>("OK", 0);
}


std::string Win32ApiSystem::getFreeSpaceUserInfo() 
{
  return getFreeSpaceInfo(Paths::getUserEmulationStationPath()[0] + std::string(":"));
}

std::string Win32ApiSystem::getFreeSpaceSystemInfo() 
{	
	TCHAR winDir[MAX_PATH];
	GetWindowsDirectory(winDir, MAX_PATH);
	return getFreeSpaceInfo(winDir[0] + std::string(":"));
}

std::string Win32ApiSystem::getFreeSpaceInfo(const std::string drive)
{
	LOG(LogDebug) << "ApiSystem::getFreeSpaceInfo";

	unsigned __int64 i64FreeBytesToCaller, i64TotalBytes, i64FreeBytes;

	BOOL  fResult = GetDiskFreeSpaceExA(drive.c_str(),
		(PULARGE_INTEGER)&i64FreeBytesToCaller,
		(PULARGE_INTEGER)&i64TotalBytes,
		(PULARGE_INTEGER)&i64FreeBytes);

	if (fResult)
	{
		unsigned long long total = i64TotalBytes;
		unsigned long long free = i64FreeBytes;
		unsigned long long used = total - free;
		unsigned long percent = 0;
		std::ostringstream oss;
		if (total != 0) 
		{  //for small SD card ;) with share < 1GB
			percent = used * 100 / total;
			oss << Utils::FileSystem::megaBytesToString(used / (1024L * 1024L)) << "/" << Utils::FileSystem::megaBytesToString(total / (1024L * 1024L)) << " (" << percent << "%)";
		}
		else
			oss << "N/A";

		return oss.str();
	}

	return "N/A";
}


bool Win32ApiSystem::ping()
{
	LOG(LogDebug) << "ApiSystem::ping";

	bool connected = false;

	HMODULE hWinInet = LoadLibrary("WININET.DLL");
	if (hWinInet != NULL)
	{
		typedef BOOL(WINAPI *PF_INETGETCONNECTEDSTATE)(LPDWORD, DWORD);
		PF_INETGETCONNECTEDSTATE pfInternetGetConnectedState;

		pfInternetGetConnectedState = (PF_INETGETCONNECTEDSTATE) ::GetProcAddress(hWinInet, "InternetGetConnectedState");
		// affectation du pointeur sur la fonction
		if (pfInternetGetConnectedState != NULL)
		{
			DWORD TypeCon;
			if (pfInternetGetConnectedState(&TypeCon, 0))
				connected = true;
		}

		FreeLibrary(hWinInet);
	}

	return connected;
}

static std::string getScriptPath(const std::string& name)
{
	std::vector<std::string> paths = 
	{
		Paths::getEmulationStationPath(),
		Paths::getUserEmulationStationPath(),
		Utils::FileSystem::getParent(Paths::getUserEmulationStationPath())
	};

	for (auto path : paths)
	{
		std::string esUpdatePath = Utils::FileSystem::combine(path, name + ".cmd");
		if (Utils::FileSystem::exists(esUpdatePath))
			return Utils::FileSystem::getPreferredPath(esUpdatePath);

		esUpdatePath = Utils::FileSystem::combine(path, name + ".bat");
		if (Utils::FileSystem::exists(esUpdatePath))
			return Utils::FileSystem::getPreferredPath(esUpdatePath);

		esUpdatePath = Utils::FileSystem::combine(path, name + ".exe");
		if (Utils::FileSystem::exists(esUpdatePath))
			return Utils::FileSystem::getPreferredPath(esUpdatePath);
	}

	return "";
}

void Win32ApiSystem::installEmulationStationZip(const std::string& zipFile)
{
	std::string path = Paths::getUserEmulationStationPath() + "/update";

	if (Utils::FileSystem::exists(path))
		Utils::FileSystem::deleteDirectoryFiles(path);
	else
		Utils::FileSystem::createDirectory(path);

	unzipFile(zipFile, path);
	Utils::FileSystem::removeFile(zipFile);

	auto files = Utils::FileSystem::getDirContent(path, true, true);

	auto pluginFolder = Paths::getEmulationStationPath() + "/plugins";
	for (auto pluginFile : Utils::FileSystem::getDirContent(pluginFolder, true))
	{
		if (Utils::FileSystem::isDirectory(pluginFile))
			continue;

		Utils::FileSystem::removeFile(pluginFile + ".old");

		std::string pluginRelativeFile = Utils::FileSystem::createRelativePath(pluginFile, pluginFolder, false);
		if (Utils::String::startsWith(pluginRelativeFile, "./"))
			pluginRelativeFile = pluginRelativeFile.substr(2);

		bool existsInArchive = false;

		for (auto installedFile : files)
		{
			if (Utils::FileSystem::isDirectory(installedFile))
				continue;

			std::string relative = Utils::FileSystem::createRelativePath(installedFile, path, false);
			if (Utils::String::startsWith(relative, "./"))
				relative = relative.substr(2);

			if (relative == pluginRelativeFile)
			{
				existsInArchive = true;
				break;
			}
		}

		if (!existsInArchive)
		{
			Utils::FileSystem::removeFile(pluginFile);
			Utils::FileSystem::renameFile(pluginFile, pluginFile + ".old");
		}
	}

	for (auto file : files)
	{
		std::string relative = Utils::FileSystem::createRelativePath(file, path, false);
		if (Utils::String::startsWith(relative, "./"))
			relative = relative.substr(2);

		std::string localPath = Paths::getEmulationStationPath() + "/" + relative;

		if (Utils::FileSystem::isDirectory(file))
		{
			if (!Utils::FileSystem::exists(localPath))
				Utils::FileSystem::createDirectory(localPath);
		}
		else
		{
			// Avoid replacing development exe/lib
			if ((Utils::String::containsIgnoreCase(localPath, "/RelWithDebInfo/") || Utils::String::containsIgnoreCase(localPath, "/Debug/")) &&
				(Utils::FileSystem::getExtension(localPath) == ".exe" || Utils::FileSystem::getExtension(localPath) == ".lib"))
				continue;

			if (Utils::FileSystem::exists(localPath))
			{
				Utils::FileSystem::removeFile(localPath + ".old");
				Utils::FileSystem::renameFile(localPath, localPath + ".old");
			}

			if (Utils::FileSystem::copyFile(file, localPath))
			{
				Utils::FileSystem::removeFile(localPath + ".old");
				Utils::FileSystem::removeFile(file);
			}
		}
	}

	Utils::FileSystem::deleteDirectoryFiles(path);
}

std::pair<std::string, int> Win32ApiSystem::updateSystem(const std::function<void(const std::string)>& func)
{
	std::string esUpdateScript = getScriptPath("es-update");
	if (!esUpdateScript.empty())
	{
		std::string esUpdateDirectory = Utils::FileSystem::getPreferredPath(Utils::FileSystem::getParent(esUpdateScript));

		std::string updatesType = Settings::getInstance()->getString("updates.type");
		if (updatesType == "beta" || updatesType == "unstable")
			esUpdateScript += " -branch " + updatesType;

		std::string output;
		auto ret = executeCMD(esUpdateScript.c_str(), output, esUpdateDirectory.c_str(), func);
		if (ret != 0)
		{
			auto lines = Utils::String::split(Utils::String::replace(output, "\r", ""), '\n', true);
			if (lines.size() > 0)
				return std::pair<std::string, int>(lines[lines.size() - 1], ret);

			return std::pair<std::string, int>("error", ret);
		}
		else
		{
			auto lines = Utils::String::split(Utils::String::replace(output, "\r", ""), '\n', true);
			if (lines.size() > 0)
			{			
				std::string lastLine = Utils::String::trim(Utils::String::replace(lines[lines.size() - 1], "\f", ""));
				if (!lastLine.find(".zip") != std::string::npos && Utils::FileSystem::exists(lastLine))
				{
					if (func != nullptr)
						func(std::string("Updating EmulationStation"));

					installEmulationStationZip(lastLine);
					return std::pair<std::string, int>("done.", ret);
				}
			}

			return std::pair<std::string, int>("done.", ret);
		}
	}

	std::string url = getUrlFromUpdateType(UPDATEURL);

	std::string fileName = Utils::FileSystem::getFileName(url);
	std::string path = Paths::getUserEmulationStationPath() + "/update";

	if (Utils::FileSystem::exists(path))
		Utils::FileSystem::deleteDirectoryFiles(path);
	else
		Utils::FileSystem::createDirectory(path);

	std::string zipFile = path + "/" + fileName;

	if (downloadFile(url, zipFile, "update", func))
	{
		if (func != nullptr)
			func(std::string("Extracting update"));

		installEmulationStationZip(zipFile);
		updateEmulatorLauncher(func);

		return std::pair<std::string, int>("done.", 0);
	}

	return std::pair<std::string, int>("error.", 1);
}

void Win32ApiSystem::updateEmulatorLauncher(const std::function<void(const std::string)>& func)
{
	std::string updatesType = Settings::getInstance()->getString("updates.type");
	if (updatesType != "beta" && updatesType != "unstable")
		return;

	// Check emulatorLauncher exists
	std::string emulatorLauncherPath = Paths::findEmulationStationFile("emulatorLauncher.exe");
	if (emulatorLauncherPath.empty() || !Utils::FileSystem::exists(emulatorLauncherPath))
		return;

	emulatorLauncherPath = Utils::FileSystem::getParent(emulatorLauncherPath);

	std::string url = LAUNCHERURL;
	std::string fileName = Utils::FileSystem::getFileName(url);
	std::string path = Paths::getUserEmulationStationPath() + "/update";

	if (Utils::FileSystem::exists(path))
		Utils::FileSystem::deleteDirectoryFiles(path);
	else
		Utils::FileSystem::createDirectory(path);

	std::string zipFile = path + "/" + fileName;

	if (downloadFile(url, zipFile, "batocera-ports", func))
	{
		if (func != nullptr)
			func(std::string("Extracting batocera-ports"));

		unzipFile(zipFile, path);
		Utils::FileSystem::removeFile(zipFile);

		auto files = Utils::FileSystem::getDirContent(path, true, true);
		for (auto file : files)
		{
			std::string relative = Utils::FileSystem::createRelativePath(file, path, false);
			if (Utils::String::startsWith(relative, "./"))
				relative = relative.substr(2);

			std::string localPath = emulatorLauncherPath + "/" + relative;

			if (Utils::FileSystem::isDirectory(file))
			{
				if (!Utils::FileSystem::exists(localPath))
					Utils::FileSystem::createDirectory(localPath);
			}
			else
			{
				if (Utils::FileSystem::exists(localPath))
				{
					Utils::FileSystem::removeFile(localPath + ".old");
					Utils::FileSystem::renameFile(localPath, localPath + ".old");
				}

				if (Utils::FileSystem::copyFile(file, localPath))
				{
					Utils::FileSystem::removeFile(localPath + ".old");
					Utils::FileSystem::removeFile(file);
				}
			}
		}

		Utils::FileSystem::deleteDirectoryFiles(path);
	}
}

bool Win32ApiSystem::canUpdate(std::vector<std::string>& output)
{
	// Update using 'es-checkversion.cmd' scripts ?
	std::string esUpdateScript = getScriptPath("es-checkversion");
	if (!esUpdateScript.empty())
	{
		std::string esUpdateDirectory = Utils::FileSystem::getPreferredPath(Utils::FileSystem::getParent(esUpdateScript));

		std::string updatesType = Settings::getInstance()->getString("updates.type");
		if (updatesType == "beta" || updatesType == "unstable")
			esUpdateScript += " -branch " + updatesType;

		std::string cmdOutput; 
		auto ret = executeCMD(esUpdateScript.c_str(), cmdOutput, esUpdateDirectory.c_str());
		if (ret == 0 && !cmdOutput.empty())
		{
			auto lines = Utils::String::split(Utils::String::replace(cmdOutput, "\r", ""), '\n', true);
			if (lines.size() > 0)
				output.push_back(lines[lines.size() - 1]);
		}

		return (ret == 0);
	}

	std::string localVersion;
	std::string localVersionFile = Paths::getEmulationStationPath() + "/version.info";
	if (Utils::FileSystem::exists(localVersionFile))
	{
		localVersion = Utils::FileSystem::readAllText(localVersionFile);
		localVersion = Utils::String::replace(Utils::String::replace(localVersion, "\r", ""), "\n", "");
	}

	HttpReq httpreq(getUrlFromUpdateType(VERSIONURL));
	if (httpreq.wait())
	{
		std::string serverVersion = httpreq.getContent();
		serverVersion = Utils::String::replace(Utils::String::replace(serverVersion, "\r", ""), "\n", "");
		if (!serverVersion.empty() && serverVersion != localVersion)
		{
			output.push_back(serverVersion);
			return true;
		}
	}

	return false;
}

bool Win32ApiSystem::launchKodi(Window *window)
{
	std::string args;
	
	std::string command = Paths::getKodiPath();
	if (!Utils::FileSystem::exists(command))
		return false;

	if (!Utils::String::startsWith(command, "C:\\Program Files"))
		args = "-p";

	ApiSystem::launchExternalWindow_before(window);

	std::wstring wexe = Utils::String::convertToWideString(command);
	std::wstring wargs = Utils::String::convertToWideString(args);

	SHELLEXECUTEINFOW lpExecInfo;
	lpExecInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
	lpExecInfo.lpFile = wexe.c_str();
	lpExecInfo.lpDirectory = NULL;
	lpExecInfo.fMask = SEE_MASK_DOENVSUBST | SEE_MASK_NOCLOSEPROCESS;
	lpExecInfo.hwnd = NULL;
	lpExecInfo.nShow = SW_SHOW;  // show command prompt with normal window size 
	lpExecInfo.hInstApp = (HINSTANCE)SE_ERR_DDEFAIL;   //WINSHELLAPI BOOL WINAPI result;
	lpExecInfo.lpVerb = L"open";
	lpExecInfo.lpParameters = wargs.c_str();

	ShellExecuteExW(&lpExecInfo);

	bool ret = lpExecInfo.hProcess != NULL;
	if (lpExecInfo.hProcess != NULL)
	{
		WaitForSingleObject(lpExecInfo.hProcess, INFINITE);
		CloseHandle(lpExecInfo.hProcess);
	}

	ApiSystem::launchExternalWindow_after(window);

	return ret;
}

void Win32ApiSystem::setReadyFlag(bool ready)
{
	if (!ready && !isReadyFlagSet())
		return;

	std::string dir = Paths::getUserEmulationStationPath() + "/tmp";
	if (!Utils::FileSystem::exists(dir))
		Utils::FileSystem::createDirectory(dir);

	std::string file = dir + "/emulationstation.ready";

	if (!ready)
	{
		Utils::FileSystem::removeFile(file);
		return;
	}

	Utils::FileSystem::writeAllText(file, "ok");
}

bool Win32ApiSystem::isReadyFlagSet()
{
	return Utils::FileSystem::exists(Paths::getUserEmulationStationPath() + "/tmp/emulationstation.ready");
}

std::vector<std::string> Win32ApiSystem::getVideoModes()
{
	std::vector<std::string> ret;

	DEVMODE vDevMode;

	int i = 0;
	while (EnumDisplaySettings(nullptr, i, &vDevMode))
	{
		if (vDevMode.dmDisplayFixedOutput == 0)
		{			
			if (vDevMode.dmBitsPerPel == 32)
				ret.push_back(
					std::to_string(vDevMode.dmPelsWidth)+"x"+
					std::to_string(vDevMode.dmPelsHeight)+"x"+
					std::to_string(vDevMode.dmBitsPerPel)+"x"+
					std::to_string(vDevMode.dmDisplayFrequency) + ":" +
					std::to_string(vDevMode.dmPelsWidth) + "x" +
					std::to_string(vDevMode.dmPelsHeight) + " " +
					std::to_string(vDevMode.dmDisplayFrequency) + "Hz");
			else
			{
				ret.push_back(
					std::to_string(vDevMode.dmPelsWidth) + "x" +
					std::to_string(vDevMode.dmPelsHeight) + "x" +
					std::to_string(vDevMode.dmBitsPerPel) + "x" +
					std::to_string(vDevMode.dmDisplayFrequency) + ":" +
					std::to_string(vDevMode.dmPelsWidth) + "x" +
					std::to_string(vDevMode.dmPelsHeight) + "x" +
					std::to_string(vDevMode.dmBitsPerPel) + " " +
					std::to_string(vDevMode.dmDisplayFrequency) + "Hz");
			}				
		}

		i++;
	}

	return ret;
}


std::vector<std::string> Win32ApiSystem::getShaderList(const std::string systemName)
{
	Utils::FileSystem::FileSystemCacheActivator fsc;

	std::vector<std::string> ret;

	std::vector<std::string> folderList;

	for (auto folder : { Paths::getUserShadersPath(), Paths::getShadersPath() })
	{
		for (auto file : Utils::FileSystem::getDirContent(folder + "/configs", true))
		{
			if (Utils::FileSystem::getFileName(file) == "rendering-defaults.yml")
			{
				auto parent = Utils::FileSystem::getFileName(Utils::FileSystem::getParent(file));
				if (parent == "configs")
					continue;

				if (std::find(ret.cbegin(), ret.cend(), parent) == ret.cend())
				{
					if (!systemName.empty())
					{
						auto data = Utils::FileSystem::readAllText(file);

						auto idx = data.find(systemName + ":");
						if (idx != std::string::npos)
						{
							auto lines = Utils::String::split(data.substr(idx), '\n', true);
							if (lines.size() > 1 && lines[1].find("shader:") != std::string::npos && lines[1].find("disabled") != std::string::npos)
								continue;
						}
					}

					ret.push_back(parent);
				}
			}
		}
	}

	std::sort(ret.begin(), ret.end());
	return ret;
}


std::string Win32ApiSystem::getSevenZipCommand()
{	
	auto file = Paths::findEmulationStationFile("7za.exe");
	if (!file.empty() && Utils::FileSystem::exists(Paths::getUserEmulationStationPath() + "\\7za.exe"))
		return file;

	if (Utils::FileSystem::exists("C:\\Program Files (x86)\\7-Zip\\7za.exe"))
		return "\"C:\\Program Files (x86)\\7-Zip\\7za.exe\"";

	return "";
}


std::string Win32ApiSystem::getHostsName()
{
	char buffer[256] = "";
	DWORD size = sizeof(buffer);
	if (GetComputerNameA(buffer, &size))
	{
		buffer[size] = '\0';
		return buffer;
	}

	return "127.0.0.1";
}
#endif

