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
#include "LocaleES.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comsuppw.lib" ) // link with "comsuppw.lib" (or debug version: "comsuppwd.lib")
#pragma comment(lib, "ws2_32.lib")

#define VERSIONURL "https://github.com/fabricecaruso/batocera-emulationstation/releases/download/continuous-stable/version.info"
#define UPDATEURL  "https://github.com/fabricecaruso/batocera-emulationstation/releases/download/continuous-stable/EmulationStation-Win32.zip"

std::string getUrlFromUpdateType(std::string url)
{
	std::string updatesType = Settings::getInstance()->getString("updates.type");
	if (updatesType == "beta")
		return Utils::String::replace(url, "continuous-stable", "continuous-master");

	return url;
}

bool Win32ApiSystem::isScriptingSupported(ScriptId script)
{
#if _DEBUG
	if (script == ApiSystem::DISKFORMAT)
		return true;
#endif

	if (script == ApiSystem::NETPLAY)
	{
		if (!(Utils::FileSystem::exists(Utils::FileSystem::getExePath() + "\\7za.exe") ||
			Utils::FileSystem::exists(Utils::FileSystem::getEsConfigPath() + "\\7za.exe") ||
			Utils::FileSystem::exists("c:\\Program Files (x86)\\7-Zip\\7za.exe") ||
			Utils::FileSystem::exists("c:\\src\\7za.exe")))
			return false;
	}

	if (script == ApiSystem::KODI)
		return (Utils::FileSystem::exists("C:\\Program Files\\Kodi\\kodi.exe") || Utils::FileSystem::exists("C:\\Program Files (x86)\\Kodi\\kodi.exe"));

	if (script == ApiSystem::DECORATIONS && !Utils::FileSystem::exists(getEmulatorLauncherPath("decorations")))
		return false;

	if (script == ApiSystem::SHADERS && !Utils::FileSystem::exists(getEmulatorLauncherPath("shaders")))
		return false;
	
	std::vector<std::string> executables;

	switch (script)
	{
	case ApiSystem::WIFI:
		executables.push_back("batocera-wifi");
		break;
	case ApiSystem::RETROACHIVEMENTS:
		executables.push_back("batocera-retroachievements-info");
		executables.push_back("emulatorLauncher");
		break;
	case ApiSystem::BLUETOOTH:
		executables.push_back("batocera-bluetooth");
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
	}

	if (executables.size() == 0)
		return false;

	for (auto executable : executables)	
	{
		std::string path = Utils::FileSystem::getExePath() + "/" + executable + ".exe";
		if (!Utils::FileSystem::exists(path))
			path = Utils::FileSystem::getEsConfigPath() + "/" + executable + ".exe";
		if (!Utils::FileSystem::exists(path))
			path = Utils::FileSystem::getParent(Utils::FileSystem::getEsConfigPath()) + "/" + executable + ".exe";
		if (!Utils::FileSystem::exists(path))
			return false;
	}

	return true;
}

bool executeCMD(LPSTR lpCommandLine, std::string& output)
{
	bool ret = false;
	output = "";

	STARTUPINFO si;
	SECURITY_ATTRIBUTES sa;
	PROCESS_INFORMATION pi;
	HANDLE g_hChildStd_IN_Rd, g_hChildStd_OUT_Wr, g_hChildStd_OUT_Rd, g_hChildStd_IN_Wr;  //pipe handles
	char buf[1024];           //i/o buffer

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
			GetStartupInfo(&si);

			si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
			si.wShowWindow = SW_HIDE;
			//set the new handles for the child process
			si.hStdOutput = g_hChildStd_OUT_Wr;
			si.hStdError = g_hChildStd_OUT_Wr;
			si.hStdInput = g_hChildStd_IN_Rd;

			//spawn the child process
			if (CreateProcess(NULL, lpCommandLine, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
			{
				unsigned long bread;   //bytes read
				unsigned long avail;   //bytes available
				memset(buf, 0, sizeof(buf));

				for (;;)
				{
					if (!PeekNamedPipe(g_hChildStd_OUT_Rd, buf, 1023, &bread, &avail, NULL))
						break;

					if (bread > 0)
					{
						if (!ReadFile(g_hChildStd_OUT_Rd, buf, 1023, &bread, NULL))
							break;

						buf[bread] = 0;
						output += std::string(buf);
					}

					if (WaitForSingleObject(pi.hProcess, 10) == WAIT_OBJECT_0)
					{
						if (!PeekNamedPipe(g_hChildStd_OUT_Rd, buf, 1023, &bread, &avail, NULL))
							break;

						if (bread == 0)
							break;
					}
				}

				DWORD exit_code = 0;
				if (GetExitCodeProcess(pi.hProcess, &exit_code))
					ret = (exit_code == 0);
				else
					ret = true;

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
				ret = false;
			}
		}
		else
		{
			CloseHandle(g_hChildStd_IN_Rd);
			CloseHandle(g_hChildStd_IN_Wr);
			ret = false;
		}
	}

	return ret;
}

bool unzipFile(const std::string fileName, const std::string dest)
{
	bool	ret = false;

	HRESULT          hResult;
	IShellDispatch*	 pISD;
	Folder*			 pFromZip = nullptr;
	VARIANT          vDir, vFile, vOpt;

	OleInitialize(NULL);
	CoInitialize(NULL);

	hResult = CoCreateInstance(CLSID_Shell, NULL, CLSCTX_INPROC_SERVER, IID_IShellDispatch, (void **)&pISD);

	if (SUCCEEDED(hResult))
	{
		VariantInit(&vDir);
		vDir.vt = VT_BSTR;

		int zipDirLen = (lstrlen(fileName.c_str()) + 1) * sizeof(WCHAR);
		BSTR bstrZip = SysAllocStringByteLen(NULL, zipDirLen);
		MultiByteToWideChar(CP_ACP, 0, fileName.c_str(), -1, bstrZip, zipDirLen);
		vDir.bstrVal = bstrZip;

		hResult = pISD->NameSpace(vDir, &pFromZip);

		if (hResult == S_OK && pFromZip != nullptr)
		{
			if (!Utils::FileSystem::exists(dest))
				Utils::FileSystem::createDirectory(dest);

			Folder *pToFolder = NULL;

			VariantInit(&vFile);
			vFile.vt = VT_BSTR;

			int fnLen = (lstrlen(dest.c_str()) + 1) * sizeof(WCHAR);
			BSTR bstrFolder = SysAllocStringByteLen(NULL, fnLen);
			MultiByteToWideChar(CP_ACP, 0, dest.c_str(), -1, bstrFolder, fnLen);
			vFile.bstrVal = bstrFolder;

			hResult = pISD->NameSpace(vFile, &pToFolder);
			if (hResult == S_OK && pToFolder)
			{
				FolderItems *fi = NULL;
				pFromZip->Items(&fi);

				VariantInit(&vOpt);
				vOpt.vt = VT_I4;
				vOpt.lVal = FOF_NO_UI; //4; // Do not display a progress dialog box

				VARIANT newV;
				VariantInit(&newV);
				newV.vt = VT_DISPATCH;
				newV.pdispVal = fi;
				hResult = pToFolder->CopyHere(newV, vOpt);
				if (hResult == S_OK)
					ret = true;

				pFromZip->Release();
				pToFolder->Release();
			}
		}
		pISD->Release();
	}

	CoUninitialize();
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
	return executeCMD((char*)command.c_str(), output);
}

std::vector<std::string> Win32ApiSystem::executeEnumerationScript(const std::string command)
{
	LOG(LogDebug) << "ApiSystem::executeEnumerationScript -> " << command;

	std::vector<std::string> res;

	std::string executable;
	std::string parameters;
	splitCommand(command, &executable, &parameters);

	std::string path = Utils::FileSystem::getExePath() + "/" + executable + ".exe";
	if (!Utils::FileSystem::exists(path))
		path = Utils::FileSystem::getEsConfigPath() + "/" + executable + ".exe";
	if (!Utils::FileSystem::exists(path))
		path = Utils::FileSystem::getParent(Utils::FileSystem::getEsConfigPath()) + "/" + executable + ".exe";

	if (Utils::FileSystem::exists(path))
	{
		std::string cmd = parameters.empty() ? path : path + " " + parameters;

		std::string output;
		if (executeCMD((char*)cmd.c_str(), output))
		{
			for (std::string all : Utils::String::splitAny(output, "\r\n"))
				res.push_back(all);
		}
	}

	return res;
}

std::string Win32ApiSystem::getCRC32(std::string fileName, bool fromZipContents)
{
	std::string cmd = "7zr h \"" + fileName + "\"";

	std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(fileName));
	if (fromZipContents && (ext == ".7z" || ext == ".zip"))
		cmd = "7zr l -slt \"" + fileName + "\"";

	std::string crc;
	std::string fn = Utils::FileSystem::getFileName(fileName);

	// Windows : use x86 7za to test. x64 version fails ( cuz our process is x86 )
	if (Utils::FileSystem::exists(Utils::FileSystem::getExePath() + "\\7za.exe"))
		cmd = Utils::String::replace(cmd, "7zr ", Utils::FileSystem::getExePath() + "\\7za.exe ");
	else if (Utils::FileSystem::exists(Utils::FileSystem::getEsConfigPath() + "\\7za.exe"))
		cmd = Utils::String::replace(cmd, "7zr ", Utils::FileSystem::getEsConfigPath() + "\\7za.exe ");
	else if (Utils::FileSystem::exists("c:\\Program Files (x86)\\7-Zip\\7za.exe"))
		cmd = Utils::String::replace(cmd, "7zr ", "\"c:\\Program Files (x86)\\7-Zip\\7za.exe\" ");
	else if (Utils::FileSystem::exists("c:\\src\\7za.exe"))
		cmd = Utils::String::replace(cmd, "7zr ", "c:\\src\\7za.exe ");

	std::string output;
	if (executeCMD((char*)cmd.c_str(), output))
	{
		for (std::string all : Utils::String::splitAny(output, "\r\n"))
		{
			int idx = all.find("CRC = ");
			if (idx != std::string::npos)
				crc = all.substr(idx + 6);
			else if (all.find(fn) == (all.size() - fn.size()) && all.length() > 8 && all[9] == ' ')
				crc = all.substr(0, 8);
		}
	}

	return crc;
}

unsigned long Win32ApiSystem::getFreeSpaceGB(std::string mountpoint)
{
	LOG(LogDebug) << "ApiSystem::getFreeSpaceGB";

	unsigned __int64 i64FreeBytesToCaller, i64TotalBytes, i64FreeBytes;

	std::string drive = Utils::FileSystem::getHomePath()[0] + std::string(":");

	BOOL  fResult = GetDiskFreeSpaceExA(drive.c_str(),
		(PULARGE_INTEGER)&i64FreeBytesToCaller,
		(PULARGE_INTEGER)&i64TotalBytes,
		(PULARGE_INTEGER)&i64FreeBytes);

	if (fResult)
		return i64FreeBytes / (1024 * 1024 * 1024);

	return 0;
}

std::string Win32ApiSystem::getVersion()
{
	LOG(LogDebug) << "ApiSystem::getVersion";

	std::string localVersion;
	std::string localVersionFile = Utils::FileSystem::getExePath() + "/version.info";
	if (Utils::FileSystem::exists(localVersionFile))
	{
		localVersion = Utils::FileSystem::readAllText(localVersionFile);
		localVersion = Utils::String::replace(Utils::String::replace(localVersion, "\r", ""), "\n", "");
		return localVersion;
	}

	return PROGRAM_VERSION_STRING;
}

std::pair<std::string, int> Win32ApiSystem::installBatoceraTheme(std::string thname, const std::function<void(const std::string)>& func)
{
	for (auto theme : getBatoceraThemesList())
	{
		auto parts = Utils::String::splitAny(theme, " \t");
		if (parts.size() < 2)
			continue;

		if (parts[1] == thname)
		{
			std::string themeUrl = parts.size() < 3 ? "" : (parts[2] == "-" ? parts[3] : parts[2]);

			std::string themeFileName = Utils::FileSystem::getFileName(themeUrl);
			std::string zipFile = Utils::FileSystem::getEsConfigPath() + "/themes/" + themeFileName + ".zip";
			zipFile = Utils::String::replace(zipFile, "/", "\\");

			if (downloadGitRepository(themeUrl, zipFile, thname, func))
			{
				if (func != nullptr)
					func(_("Extracting") + " " + thname);

				unzipFile(zipFile, Utils::String::replace(Utils::FileSystem::getEsConfigPath() + "/themes", "/", "\\"));

				std::string folderName = Utils::FileSystem::getEsConfigPath() + "/themes/" + themeFileName + "-master";
				std::string finalfolderName = Utils::String::replace(folderName, "-master", "");

				rename(folderName.c_str(), finalfolderName.c_str());

				Utils::FileSystem::removeFile(zipFile);

				return std::pair<std::string, int>(std::string("OK"), 0);
			}

			break;
		}
	}

	return std::pair<std::string, int>(std::string(""), 1);
}

std::vector<std::string> Win32ApiSystem::getBatoceraThemesList()
{
	LOG(LogDebug) << "Win32ApiSystem::getBatoceraThemesList";

	std::vector<std::string> res;

	HttpReq httpreq("https://batocera.org/upgrades/themes.txt");
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

				std::vector<std::string> paths{
					"/etc/emulationstation/themes",
					Utils::FileSystem::getEsConfigPath() + "/themes",
					"/userdata/themes" // batocera
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

				if (themeExists)
					res.push_back("[I]\t" + line);
				else
					res.push_back("[A]\t" + line);
			}
		}
	}

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
	res.push_back("DEFAULT");
	return res;
}

std::vector<std::string> Win32ApiSystem::getBatoceraBezelsList()
{
	LOG(LogDebug) << "ApiSystem::getBatoceraBezelsList";

	std::vector<std::string> res;

	HttpReq request("https://batocera.org/upgrades/bezels.txt");
	if (request.wait())
	{
		auto lines = Utils::String::split(request.getContent(), '\n');
		for (auto line : lines)
		{
			auto parts = Utils::String::splitAny(line, " \t");
			if (parts.size() < 2)
				continue;

			std::string theBezelProject = getEmulatorLauncherPath("decorations") + "/thebezelproject/games/" + parts[0];
			if (Utils::FileSystem::exists(theBezelProject))
				res.push_back("[I]\t" + line);
			else
				res.push_back("[A]\t" + line);
		}
	}

	return res;
}

std::pair<std::string, int> Win32ApiSystem::installBatoceraBezel(std::string bezelsystem, const std::function<void(const std::string)>& func)
{
	LOG(LogDebug) << "ApiSystem::installBatoceraBezel";

	for (auto bezel : getBatoceraBezelsList())
	{
		auto parts = Utils::String::splitAny(bezel, " \t");
		if (parts.size() < 2)
			continue;

		if (parts[1] == bezelsystem)
		{
			std::string themeUrl = parts.size() > 1 ? parts[2] : "";
			std::string subFolder = parts.size() > 2 ? parts[3] : "";

			std::string themeFileName = Utils::FileSystem::getFileName(themeUrl);
			std::string zipFile = getEmulatorLauncherPath("decorations") + "/" + themeFileName + ".zip";
			zipFile = Utils::String::replace(zipFile, "/", "\\");

			if (downloadGitRepository(themeUrl, zipFile, bezelsystem, func))
			{
				std::string theBezelProject = getEmulatorLauncherPath("decorations") + "/thebezelproject/games/"+ bezelsystem;
				Utils::FileSystem::createDirectory(theBezelProject);

				std::string tmp = getEmulatorLauncherPath("decorations") + "/thebezelproject/games/" + bezelsystem + "/tmp";
				Utils::FileSystem::createDirectory(tmp);

				if (func != nullptr)
					func(_("Extracting") + " " + bezelsystem+ " bezels");

				unzipFile(Utils::FileSystem::getPreferredPath(zipFile), Utils::FileSystem::getPreferredPath(tmp));
				Utils::FileSystem::removeFile(zipFile);

				auto files = Utils::FileSystem::getDirContent(tmp, true, true);
				for (auto file : files)
				{
					std::string ext = Utils::FileSystem::getExtension(file);
					if (ext != ".cfg" && ext != ".png")
						continue;

					if (!subFolder.empty() && Utils::FileSystem::getGenericPath(file).find(subFolder.c_str()) == std::string::npos)
						continue;
					else if (subFolder.empty() && file.find("/overlay/GameBezels/") == std::string::npos)
						continue;
					
					std::string dest;
					dest = Utils::FileSystem::getPreferredPath(theBezelProject + "/" + Utils::FileSystem::getFileName(file));
					rename(Utils::FileSystem::getPreferredPath(file).c_str(), dest.c_str());					
				}

				Utils::FileSystem::deleteDirectoryFiles(tmp);
				rmdir(Utils::FileSystem::getPreferredPath(tmp).c_str());

				return std::pair<std::string, int>(std::string("OK"), 0);
			}

			break;
		}
	}

	return std::pair<std::string, int>("", 1);
}

std::pair<std::string, int> Win32ApiSystem::uninstallBatoceraBezel(BusyComponent* ui, std::string bezelsystem)
{
	std::string theBezelProject = getEmulatorLauncherPath("decorations") + "/thebezelproject/games/" + bezelsystem;
	Utils::FileSystem::deleteDirectoryFiles(theBezelProject);
	rmdir(theBezelProject.c_str());

	return std::pair<std::string, int>("OK", 0);
}

std::string Win32ApiSystem::getFreeSpaceInfo()
{
	LOG(LogDebug) << "ApiSystem::getFreeSpaceInfo";

	unsigned __int64 i64FreeBytesToCaller, i64TotalBytes, i64FreeBytes;

	std::string drive = Utils::FileSystem::getHomePath()[0] + std::string(":");

	BOOL  fResult = GetDiskFreeSpaceExA(drive.c_str(),
		(PULARGE_INTEGER)&i64FreeBytesToCaller,
		(PULARGE_INTEGER)&i64TotalBytes,
		(PULARGE_INTEGER)&i64FreeBytes);

	if (fResult)
	{
		unsigned long total = i64TotalBytes / (1024L * 1024L);
		unsigned long free = i64FreeBytes / (1024L * 1024L);
		unsigned long used = total - free;
		unsigned long percent = 0;
		std::ostringstream oss;
		if (total != 0) {  //for small SD card ;) with share < 1GB
			percent = used * 100 / total;
			oss << used << "GB/" << total << "GB (" << percent << "%)";
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

std::pair<std::string, int> Win32ApiSystem::updateSystem(const std::function<void(const std::string)>& func)
{
	std::string url = getUrlFromUpdateType(UPDATEURL);

	std::string fileName = Utils::FileSystem::getFileName(url);
	std::string path = Utils::FileSystem::getHomePath() + "/.emulationstation/update";

	if (Utils::FileSystem::exists(path))
		Utils::FileSystem::deleteDirectoryFiles(path);

	Utils::FileSystem::createDirectory(path);

	std::string zipFile = path + "/" + fileName;

	if (downloadFile(url, zipFile, "update", func))
	{
		if (func != nullptr)
			func(std::string("Extracting update"));

		unzipFile(Utils::FileSystem::getPreferredPath(zipFile), Utils::FileSystem::getPreferredPath(path));
		Utils::FileSystem::removeFile(zipFile);

		auto files = Utils::FileSystem::getDirContent(path, true, true);
		for (auto file : files)
		{
			std::string relative = Utils::FileSystem::createRelativePath(file, path, false);
			if (Utils::String::startsWith(relative, "./"))
				relative = relative.substr(2);

			std::string localPath = Utils::FileSystem::getExePath() + "/" + relative;

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
					rename(localPath.c_str(), (localPath + ".old").c_str());
				}

				if (Utils::FileSystem::copyFile(file, localPath))
				{
					Utils::FileSystem::removeFile(localPath + ".old");
					Utils::FileSystem::removeFile(file);
				}
			}
		}

		Utils::FileSystem::deleteDirectoryFiles(path);

		return std::pair<std::string, int>("done.", 0);
	}

	return std::pair<std::string, int>("done.", 0);
}

bool Win32ApiSystem::canUpdate(std::vector<std::string>& output)
{
	std::string localVersion;
	std::string localVersionFile = Utils::FileSystem::getExePath() + "/version.info";
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
	std::string command = "C:\\Program Files\\Kodi\\kodi.exe";
	if (!Utils::FileSystem::exists(command))
	{
		command = "C:\\Program Files (x86)\\Kodi\\kodi.exe";
		if (!Utils::FileSystem::exists(command))
			return false;
	}

	ApiSystem::launchExternalWindow_before(window);

	SHELLEXECUTEINFO lpExecInfo;
	lpExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	lpExecInfo.lpFile = command.c_str();
	lpExecInfo.fMask = SEE_MASK_DOENVSUBST | SEE_MASK_NOCLOSEPROCESS;
	lpExecInfo.lpVerb = "open";

	ShellExecuteEx(&lpExecInfo);

	bool ret = lpExecInfo.hProcess != NULL;
	if (lpExecInfo.hProcess != NULL)
	{
		WaitForSingleObject(lpExecInfo.hProcess, INFINITE);
		CloseHandle(lpExecInfo.hProcess);
	}

	ApiSystem::launchExternalWindow_after(window);

	return ret;
}

std::string Win32ApiSystem::getIpAdress()
{	
	// Init WinSock
	WSADATA wsa_Data;
	int wsa_ReturnCode = WSAStartup(0x101, &wsa_Data);
	if (wsa_ReturnCode != 0)
		return "NOT CONNECTED";

	char* szLocalIP = nullptr;

	// Get the local hostname
	char szHostName[255];
	if (gethostname(szHostName, 255) == 0)
	{
		struct hostent *host_entry;
		host_entry = gethostbyname(szHostName);
		if (host_entry != nullptr)
			szLocalIP = inet_ntoa(*(struct in_addr *)*host_entry->h_addr_list);
	}

	WSACleanup();

	if (szLocalIP == nullptr)
		return "NOT CONNECTED";

	return std::string(szLocalIP); // "127.0.0.1"
}

std::string Win32ApiSystem::getEmulatorLauncherPath(const std::string variable)
{
	std::string path = Utils::FileSystem::getExePath() + "/emulatorLauncher.cfg";
	if (!Utils::FileSystem::exists(path))
		path = Utils::FileSystem::getEsConfigPath() + "/emulatorLauncher.cfg";
	if (!Utils::FileSystem::exists(path))
		path = Utils::FileSystem::getParent(Utils::FileSystem::getEsConfigPath()) + "/emulatorLauncher.cfg";

	if (!Utils::FileSystem::exists(path))
		return "";

	std::string line;
	std::ifstream systemConf(path);
	if (systemConf && systemConf.is_open())
	{
		while (std::getline(systemConf, line))
		{
			int idx = line.find("=");
			if (idx == std::string::npos || line.find("#") == 0 || line.find(";") == 0)
				continue;

			std::string key = line.substr(0, idx);
			if (key == variable)
			{
				systemConf.close();

				std::string relativeTo = Utils::FileSystem::getParent(path);
				return Utils::FileSystem::getAbsolutePath(line.substr(idx + 1), relativeTo);
			}
		}
		systemConf.close();
	}

	return "";
}

void Win32ApiSystem::setReadyFlag(bool ready)
{
	if (!ready && !isReadyFlagSet())
		return;

	std::string dir = Utils::FileSystem::getEsConfigPath() + "/tmp";
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
	return Utils::FileSystem::exists(Utils::FileSystem::getEsConfigPath() + "/tmp/emulationstation.ready");
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

#endif
