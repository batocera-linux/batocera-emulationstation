#define _FILE_OFFSET_BITS 64

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include "Settings.h"
#include <sys/stat.h>
#include <string.h>
#include <algorithm>

#if defined(_WIN32)
// because windows...
#include <direct.h>
#include <Windows.h>
#include <mutex>
#define getcwd _getcwd
#define mkdir(x,y) _mkdir(x)
#define snprintf _snprintf
#define stat64 _stat64
#define unlink _unlink
#define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#define S_ISDIR(x) (((x) & S_IFMT) == S_IFDIR)
#else // _WIN32
#include <dirent.h>
#include <unistd.h>
#include <mutex>
#endif // _WIN32

#include <fstream>
#include <sstream>

namespace Utils
{
	namespace FileSystem
	{
		std::string getEsConfigPath()
		{
#if defined WIN32 || defined _ENABLEEMUELEC
			static std::string cfg;
			if (cfg.empty())
				cfg = Utils::FileSystem::getCanonicalPath(Utils::FileSystem::getHomePath() + "/.emulationstation");

			return cfg;
#else
			return "/userdata/system/configs/emulationstation"; // batocera
#endif
		}

		std::string getSharedConfigPath()
		{
#if defined WIN32 || defined _ENABLEEMUELEC
			return Utils::FileSystem::getExePath();
#else
			return "/usr/share/emulationstation"; // batocera
#endif
		}

	// FileCache

		struct FileCache
		{
			FileCache() {}

			FileCache(bool _exists, bool _dir)
			{
				directory = _dir;
				exists = _exists;
				hidden = false;
				isSymLink = false;
			}

#if WIN32			
			FileCache(DWORD dwFileAttributes)
			{
				if (0xFFFFFFFF == dwFileAttributes)
				{
					directory = false;
					exists = false;
					hidden = false;
					isSymLink = false;
				}
				else
				{
					exists = true;
					directory = dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
					hidden = dwFileAttributes & FILE_ATTRIBUTE_HIDDEN;
					isSymLink = dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT;
				}
			}
#else
			FileCache(const std::string& name, dirent* entry)
			{
				exists = true;
				hidden = (getFileName(name)[0] == '.');
				directory = (entry->d_type == 4); // DT_DIR;
				isSymLink = (entry->d_type == 10); // DT_LNK;
			}

			FileCache(dirent* entry, bool _hidden)
			{
				exists = true;
				hidden = _hidden;
				directory = (entry->d_type == 4); // DT_DIR;
				isSymLink = (entry->d_type == 10); // DT_LNK;
			}
#endif

			bool exists;
			bool directory;
			bool hidden;
			bool isSymLink;

			static int fromStat64(const std::string& key, struct stat64* info)
			{
				int ret = stat64(key.c_str(), info);

				std::unique_lock<std::mutex> lock(mFileCacheMutex);

				FileCache cache(ret == 0, false);
				if (cache.exists)
				{
					cache.directory = S_ISDIR(info->st_mode);
#ifndef WIN32
					cache.isSymLink = S_ISLNK(info->st_mode);
#endif
				}

				mFileCache[key] = cache;

				return ret;
			}

			static void add(const std::string& key, FileCache cache)
			{
				if (!mEnabled)
					return;

				std::unique_lock<std::mutex> lock(mFileCacheMutex);
				mFileCache[key] = cache;
			}

			static FileCache* get(const std::string& key)
			{
				if (!mEnabled)
					return nullptr;

				std::unique_lock<std::mutex> lock(mFileCacheMutex);

				auto it = mFileCache.find(key);
				if (it != mFileCache.cend())
					return &it->second;

				it = mFileCache.find(Utils::FileSystem::getParent(key) + "/*");
				if (it != mFileCache.cend())
				{
					mFileCache[key] = FileCache(false, false);
					return &mFileCache[key];
				}

				return nullptr;
			}

			static void resetCache()
			{
				std::unique_lock<std::mutex> lock(mFileCacheMutex);
				mFileCache.clear();
			}

			static void setEnabled(bool value) { mEnabled = value; }
			static bool isEnabled() { return mEnabled; }

		private:
			static std::map<std::string, FileCache> mFileCache;
			static std::mutex mFileCacheMutex;
			static bool mEnabled;
		};

		std::map<std::string, FileCache> FileCache::mFileCache;
		std::mutex FileCache::mFileCacheMutex;
		bool FileCache::mEnabled = false;

	// FileSystemCacheActivator

		int FileSystemCacheActivator::mReferenceCount = 0;

		FileSystemCacheActivator::FileSystemCacheActivator()
		{
			if (mReferenceCount == 0)
			{
				FileCache::setEnabled(true);
				FileCache::resetCache();
			}

			mReferenceCount++;
		}

		FileSystemCacheActivator::~FileSystemCacheActivator()
		{
			mReferenceCount--;

			if (mReferenceCount <= 0)
			{
				FileCache::setEnabled(false);
				FileCache::resetCache();
			}
		}

	// Methods

		stringList getDirContent(const std::string& _path, const bool _recursive, const bool includeHidden)
		{
			std::string path = getGenericPath(_path);
			stringList  contentList;

			// only parse the directory, if it's a directory
			if(isDirectory(path))
			{
				// tell filecache we enumerated the folder
				FileCache::add(path + "/*", FileCache(true, true));

#if defined(_WIN32)
				WIN32_FIND_DATAW findData;
				std::string      wildcard = path + "/*";

				HANDLE hFind = FindFirstFileExW(std::wstring(wildcard.begin(), wildcard.end()).c_str(),
					FINDEX_INFO_LEVELS::FindExInfoBasic, &findData, FINDEX_SEARCH_OPS::FindExSearchNameMatch
					, NULL, FIND_FIRST_EX_LARGE_FETCH);

				if(hFind != INVALID_HANDLE_VALUE)
				{
					// loop over all files in the directory
					do
					{
						std::string name = Utils::String::convertFromWideString(findData.cFileName);

						// ignore "." and ".."
						if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && name == "." || name == "..")
							continue;

						std::string fullName(getGenericPath(path + "/" + name));
						FileCache::add(fullName, FileCache((DWORD)findData.dwFileAttributes));

						if (!includeHidden && (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == FILE_ATTRIBUTE_HIDDEN)
							continue;

						contentList.push_back(fullName);						

						if (_recursive && (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
							contentList.merge(getDirContent(fullName, true, includeHidden));
					}
					while(FindNextFileW(hFind, &findData));

					FindClose(hFind);
				}
#else // _WIN32
				DIR* dir = opendir(path.c_str());

				if(dir != NULL)
				{
					struct dirent* entry;

					// loop over all files in the directory
					while((entry = readdir(dir)) != NULL)
					{
						std::string name(entry->d_name);

						// ignore "." and ".."
						if((name != ".") && (name != ".."))
						{
							std::string fullName(getGenericPath(path + "/" + name));

							FileCache::add(fullName, FileCache(fullName, entry));

							if (!includeHidden && Utils::FileSystem::isHidden(fullName))
								continue;

							contentList.push_back(fullName);

							if(_recursive && isDirectory(fullName))
								contentList.merge(getDirContent(fullName, true));
						}
					}

					closedir(dir);
				}
#endif // _WIN32

			}

			// sort the content list
			// contentList.sort();

			// return the content list
			return contentList;

		} // getDirContent

		fileList getDirectoryFiles(const std::string& _path)
		{
			std::string path = getGenericPath(_path);
			fileList  contentList;

			// tell filecache we enumerated the folder
			FileCache::add(path + "/*", FileCache(true, true));

			// only parse the directory, if it's a directory
			// if (isDirectory(path))
			{			
#if defined(_WIN32)
				WIN32_FIND_DATAW findData;
				std::string      wildcard = path + "/*";
				
				HANDLE hFind = FindFirstFileExW(std::wstring(wildcard.begin(), wildcard.end()).c_str(),
					FINDEX_INFO_LEVELS::FindExInfoBasic, &findData, FINDEX_SEARCH_OPS::FindExSearchNameMatch
					, NULL, FIND_FIRST_EX_LARGE_FETCH);

				if (hFind != INVALID_HANDLE_VALUE)
				{
					// loop over all files in the directory
					do
					{
						std::string name = Utils::String::convertFromWideString(findData.cFileName);

						if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && name == "." || name == "..")
							continue;

						FileInfo fi;
						fi.path = path + "/" + name;
						fi.hidden = (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == FILE_ATTRIBUTE_HIDDEN;
						fi.directory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
						contentList.push_back(fi);

						FileCache::add(fi.path, FileCache((DWORD)findData.dwFileAttributes));
					} 
					while (FindNextFileW(hFind, &findData));

					FindClose(hFind);
				}
#else // _WIN32
				DIR* dir = opendir(path.c_str());

				if (dir != NULL)
				{
					struct dirent* entry;

					// loop over all files in the directory
					while ((entry = readdir(dir)) != NULL)
					{
						std::string name(entry->d_name);

						// ignore "." and ".."
						if ((name != ".") && (name != ".."))
						{
							std::string fullName(getGenericPath(path + "/" + name));

							FileInfo fi;
							fi.path = fullName;
							fi.hidden = Utils::FileSystem::isHidden(fullName);
							fi.directory = (entry->d_type == 4); // DT_DIR;

							FileCache::add(fullName, FileCache(entry, fi.hidden));

							//DT_LNK
							contentList.push_back(fi);
						}
					}

					closedir(dir);
				}
#endif // _WIN32

			}

			// return the content list
			return contentList;

		} // getDirectoryFiles

		std::vector<std::string> getPathList(const std::string& _path)
		{
			std::vector<std::string>  pathList;
			std::string path  = getGenericPath(_path);
			size_t      start = 0;
			size_t      end   = 0;

			// split at '/'
			while((end = path.find("/", start)) != std::string::npos)
			{
				if(end != start)
					pathList.push_back(std::string(path, start, end - start));

				start = end + 1;
			}

			// add last folder / file to pathList
			if(start != path.size())
				pathList.push_back(std::string(path, start, path.size() - start));

			// return the path list
			return pathList;

		} // getPathList

		std::string homePath;

		void setHomePath(const std::string& _path)
		{
			homePath = Utils::FileSystem::getGenericPath(_path);
		}

		std::string getHomePath()
		{
			if (homePath.length())
				return homePath;

#ifdef WIN32
			// Is it a portable installation ? Check if ".emulationstation/es_systems.cfg" exists in the exe's path
			if (!homePath.length())
			{
				std::string portableCfg = getExePath() + "/.emulationstation/es_systems.cfg";
				if (Utils::FileSystem::exists(portableCfg))
					homePath = getExePath();
			}
#endif

			// HOME has different usages in Linux & Windows
			// On Windows,  "HOME" is not a system variable but a user's environment variable that can be defined by users in batch files. 
			// If defined : The environment variable has priority over all
			char* envHome = getenv("HOME");
			if (envHome)
				homePath = getGenericPath(envHome);		

#ifdef WIN32
			// On Windows, getenv("HOME") is not the system's user path but a user environment variable.
			// Instead we get the home user's path using %HOMEDRIVE%/%HOMEPATH% which are system variables.
			if (!homePath.length())
			{
				char* envHomeDrive = getenv("HOMEDRIVE");
				char* envHomePath = getenv("HOMEPATH");
				if (envHomeDrive && envHomePath)
					homePath = getGenericPath(std::string(envHomeDrive) + "/" + envHomePath);
			}
#endif // _WIN32

			// no homepath found, fall back to current working directory
			if (!homePath.length())
				homePath = getCWDPath();

			homePath = getGenericPath(homePath);
				
			// return constructed homepath
			return homePath;

		} // getHomePath

		std::string getCWDPath()
		{
			char temp[2048];

			// return current working directory path
			return (getcwd(temp, 2048) ? getGenericPath(temp) : "");

		} // getCWDPath

		std::string exePath;

		void setExePath(const std::string& _path)
		{
			std::string path = getCanonicalPath(_path);
			if (isRegularFile(path))
				path = getParent(path);

			exePath = Utils::FileSystem::getGenericPath(path);
		} // setExePath

		std::string getExePath()
		{
			return exePath;

		} // getExePath

		std::string getPreferredPath(const std::string& _path)
		{
			std::string path   = _path;
			size_t      offset = std::string::npos;
#if defined(_WIN32)
			// convert '/' to '\\'
			while((offset = path.find('/')) != std::string::npos)
				path.replace(offset, 1, "\\");
#endif // _WIN32
			return path;
		}

		std::string getGenericPath(const std::string& _path)
		{
			std::string path   = _path;
			size_t      offset = std::string::npos;

			// remove "\\\\?\\"
			if((path.find("\\\\?\\")) == 0)
				path.erase(0, 4);

			// convert '\\' to '/'
			while((offset = path.find('\\')) != std::string::npos)
				path.replace(offset, 1 ,"/");

			// remove double '/'
			while((offset = path.find("//")) != std::string::npos)
				path.erase(offset, 1);

			// remove trailing '/'
			while(path.length() && ((offset = path.find_last_of('/')) == (path.length() - 1)))
				path.erase(offset, 1);

			// return generic path
			return path;

		} // getGenericPath

		std::string getEscapedPath(const std::string& _path)
		{
			std::string path = getGenericPath(_path);

#if defined(_WIN32)
			// windows escapes stuff by just putting everything in quotes
			return '"' + getPreferredPath(path) + '"';
#else // _WIN32
			// insert a backslash before most characters that would mess up a bash path
			const char* invalidChars = "\\ '\"!$^&*(){}[]?;<>";
			const char* invalidChar  = invalidChars;

			while(*invalidChar)
			{
				size_t start  = 0;
				size_t offset = 0;

				while((offset = path.find(*invalidChar, start)) != std::string::npos)
				{
					start = offset + 1;

					if((offset == 0) || (path[offset - 1] != '\\'))
					{
						path.insert(offset, 1, '\\');
						++start;
					}
				}

				++invalidChar;
			}

			// return escaped path
			return path;
#endif // _WIN32

		} // getEscapedPath

		std::string getCanonicalPath(const std::string& _path)
		{
			// temporary hack for builtin resources
			if(_path.size() >= 2 && _path[0] == ':' && _path[1] == '/')
				return _path;

#if WIN32
			std::string path = _path[0] == '.' ? getAbsolutePath(_path) : getGenericPath(_path);
			if (path.find("./") == std::string::npos)
				return path;
#else
			std::string path = exists(_path) ? getAbsolutePath(_path) : getGenericPath(_path);
#endif

			// cleanup path
			bool scan = true;
			while(scan)
			{
				auto pathList = getPathList(path);

				path.clear();
				scan = false;

				for(auto it = pathList.cbegin(); it != pathList.cend(); ++it)
				{
					// ignore empty
					if((*it).empty())
						continue;

					// remove "/./"
					if((*it) == ".")
						continue;

					// resolve "/../"
					if((*it) == "..")
					{
						path = getParent(path);
						continue;
					}

#if defined(_WIN32)
					// append folder to path
					path += (path.size() == 0) ? (*it) : ("/" + (*it));
#else // _WIN32
					// append folder to path
					path += ("/" + (*it));

					/* 
					// resolve symlink - Disabled : slow & useless
					if(isSymlink(path))
					{
						std::string resolved = resolveSymlink(path);

						if(resolved.empty())
							return "";

						if(isAbsolute(resolved))
							path = resolved;
						else
							path = getParent(path) + "/" + resolved;

						for(++it; it != pathList.cend(); ++it)
							path += (path.size() == 0) ? (*it) : ("/" + (*it));

						scan = true;
						break;
					}*/
#endif // _WIN32
				}
			}

			// return canonical path
			return path;

		} // getCanonicalPath

		std::string getAbsolutePath(const std::string& _path, const std::string& _base)
		{
			if (isAbsolute(_path))
				return getGenericPath(_path);

			return getCanonicalPath(_base + "/" + _path);
		} // getAbsolutePath

		std::string getParent(const std::string& _path)
		{
			std::string path   = getGenericPath(_path);
			size_t      offset = std::string::npos;

			// find last '/' and erase it
			if((offset = path.find_last_of('/')) != std::string::npos)
				return path.erase(offset);

			// no parent found
			return path;

		} // getParent

		std::string getFileName(const std::string& _path)
		{
			std::string path   = getGenericPath(_path);
			size_t      offset = std::string::npos;

			// find last '/' and return the filename
			if((offset = path.find_last_of('/')) != std::string::npos)
				return ((path[offset + 1] == 0) ? "." : std::string(path, offset + 1));

			// no '/' found, entire path is a filename
			return path;

		} // getFileName

		std::string getStem(const std::string& _path)
		{
			std::string fileName = getFileName(_path);
			size_t      offset   = std::string::npos;

			// empty fileName
			if(fileName == ".")
				return fileName;

			// find last '.' and erase the extension
			if((offset = fileName.find_last_of('.')) != std::string::npos)
				return fileName.erase(offset);

			// no '.' found, filename has no extension
			return fileName;

		} // getStem

		std::string getExtension(const std::string& _path)
		{
			std::string fileName = getFileName(_path);
			size_t      offset   = std::string::npos;

			// empty fileName
			if(fileName == ".")
				return fileName;

			// find last '.' and return the extension
			if((offset = fileName.find_last_of('.')) != std::string::npos)
				return std::string(fileName, offset);

			// no '.' found, filename has no extension
			return ".";

		} // getExtension

		std::string resolveRelativePath(const std::string& _path, const std::string& _relativeTo, const bool _allowHome)
		{
			// nothing to resolve
			if(!_path.length())
				return _path;

			if (_path.length() == 1 && _path[0] == '.')
				return getGenericPath(_relativeTo);

			// replace '.' with relativeTo
			if((_path[0] == '.') && (_path[1] == '/' || _path[1] == '\\'))
				return getGenericPath(_relativeTo + &(_path[1]));

			// replace '~' with homePath
			if(_allowHome && (_path[0] == '~') && (_path[1] == '/' || _path[1] == '\\'))
				return getCanonicalPath(getHomePath() + &(_path[1]));

			// nothing to resolve
			return getGenericPath(_path);

		} // resolveRelativePath

		std::string createRelativePath(const std::string& _path, const std::string& _relativeTo, const bool _allowHome)
		{
			if (_relativeTo.empty())
				return _path;

			if (_path == _relativeTo)
				return "";

			bool        contains = false;
			std::string path     = removeCommonPath(_path, _relativeTo, contains);

			if(contains)
			{
				// success
				return ("./" + path);
			}

			if(_allowHome)
			{
#if WIN32
				auto from_dirs = getPathList(getHomePath());
				auto to_dirs = getPathList(_path);

				if (from_dirs.size() == 0 || to_dirs.size() == 0 || from_dirs[0] != to_dirs[0])
					return path;

				std::string output;
				output.reserve(_path.size());
				output = "~/";

				std::vector<std::string>::const_iterator to_it = to_dirs.begin(), to_end = to_dirs.end(), from_it = from_dirs.begin(), from_end = from_dirs.end();

				while ((to_it != to_end) && (from_it != from_end) && *to_it == *from_it)
				{
					++to_it;
					++from_it;
				}

				while (from_it != from_end)
				{
					output += "../";
					++from_it;
				}

				while (to_it != to_end)
				{
					output += *to_it;
					++to_it;

					if (to_it != to_end)
						output += "/";
				}

				return output;
#else				
				path = removeCommonPath(_path, getHomePath(), contains);
				if(contains)
				{
					// success
					return ("~/" + path);
				}
#endif
			}

			// nothing to resolve
			return path;

		} // createRelativePath

		std::string removeCommonPath(const std::string& _path, const std::string& _common, bool& _contains)
		{
			std::string path = _path; // getGenericPath(_path);
			//std::string common = isDirectory(_common) ? getGenericPath(_common) : getParent(_common);

			// check if path contains common
			if(path.find(_common) == 0 && path != _common)
			{
				_contains = true;
				int trailingSlash = _common.find_last_of('/') == (_common.length() - 1) ? 0 : 1;
				return path.substr(_common.length() + trailingSlash);
			}

			// it didn't
			_contains = false;
			return path;

		} // removeCommonPath

		std::string resolveSymlink(const std::string& _path)
		{
			std::string path = getGenericPath(_path);
			std::string resolved;

#if defined(_WIN32)
			HANDLE hFile = CreateFile(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);

			if(hFile != INVALID_HANDLE_VALUE)
			{
				resolved.resize(GetFinalPathNameByHandle(hFile, nullptr, 0, FILE_NAME_NORMALIZED) + 1);
				if(GetFinalPathNameByHandle(hFile, (LPSTR)resolved.data(), (DWORD)resolved.size(), FILE_NAME_NORMALIZED) > 0)
				{
					resolved.resize(resolved.size() - 1);
					resolved = getGenericPath(resolved);
				}
				CloseHandle(hFile);
			}
#else // _WIN32
			struct stat info;

			// check if lstat succeeded
			if(lstat(path.c_str(), &info) == 0)
			{
				resolved.resize(info.st_size);
				if(readlink(path.c_str(), (char*)resolved.data(), resolved.size()) > 0)
					resolved = getGenericPath(resolved);
			}
#endif // _WIN32

			// return resolved path
			return resolved;

		} // resolveSymlink

		bool removeFile(const std::string& _path)
		{
			std::string path = getGenericPath(_path);

			// don't remove if it doesn't exists
			if(!exists(path))
				return true;

			// try to remove file
			return (unlink(path.c_str()) == 0);

		} // removeFile

		bool createDirectory(const std::string& _path)
		{
			std::string path = getGenericPath(_path);

			// don't create if it already exists
			if(exists(path))
				return true;

			// try to create directory
			if(mkdir(path.c_str(), 0755) == 0)
				return true;

			// failed to create directory, try to create the parent
			std::string parent = getParent(path);

			// only try to create parent if it's not identical to path
			if(parent != path)
				createDirectory(parent);

			// try to create directory again now that the parent should exist
			return (mkdir(path.c_str(), 0755) == 0);

		} // createDirectory

		bool exists(const std::string& _path)
		{
			if (_path.empty())
				return false;

			auto it = FileCache::get(_path);
			if (it != nullptr)
				return it->exists;

#ifdef WIN32		
			DWORD dwAttr = GetFileAttributes(_path.c_str());
			FileCache::add(_path, FileCache(dwAttr));
			if (0xFFFFFFFF == dwAttr)
				return false;

			return true;
#else
			std::string path = getGenericPath(_path);
			struct stat64 info;

			return FileCache::fromStat64(path, &info) == 0;
#endif
		} // exists

		bool isAbsolute(const std::string& _path)
		{
			if(_path.size() >= 2 && _path[0] == ':' && _path[1] == '/')
				return true;
		
			std::string path = getGenericPath(_path);

#ifdef WIN32
			return ((path.size() > 1) && (path[1] == ':'));
#else // _WIN32
			return ((path.size() > 0) && (path[0] == '/'));
#endif // _WIN32

		} // isAbsolute

		bool isRegularFile(const std::string& _path)
		{
			auto it = FileCache::get(_path);
			if (it != nullptr)
				return it->exists && !it->directory && !it->isSymLink;

			std::string path = getGenericPath(_path);
			struct stat64 info;

			// check if stat64 succeeded
			if (FileCache::fromStat64(path, &info) != 0) //if(stat64(path.c_str(), &info) != 0)				
				return false;

			// check for S_IFREG attribute
			return (S_ISREG(info.st_mode));

		} // isRegularFile

		bool isDirectory(const std::string& _path)
		{
			auto it = FileCache::get(_path);
			if (it != nullptr)
				return it->exists && it->directory;

#ifdef WIN32
			// check for symlink attribute
			DWORD Attributes = GetFileAttributes(_path.c_str());
			FileCache::add(_path, FileCache(Attributes));
			return (Attributes != INVALID_FILE_ATTRIBUTES) && (Attributes & FILE_ATTRIBUTE_DIRECTORY);
#else // _WIN32
			std::string path = getGenericPath(_path);
			struct stat64 info;

			// check if stat succeeded
			if (FileCache::fromStat64(path, &info) != 0) //if(stat64(path.c_str(), &info) != 0)
				return false;

			// check for S_IFDIR attribute
			return (S_ISDIR(info.st_mode));
#endif
		} // isDirectory

		bool isSymlink(const std::string& _path)
		{
		
			auto it = FileCache::get(_path);
			if (it != nullptr)
				return it->exists && it->isSymLink;
				
			std::string path = getGenericPath(_path);

#ifdef WIN32
			// check for symlink attribute
			DWORD Attributes = GetFileAttributes(path.c_str());
			FileCache::add(_path, FileCache(Attributes));
			if((Attributes != INVALID_FILE_ATTRIBUTES) && (Attributes & FILE_ATTRIBUTE_REPARSE_POINT))
				return true;

			// not a symlink
			return false;
#else // WIN32
			struct stat64 info;

			// check if lstat succeeded
			if (FileCache::fromStat64(path, &info) != 0) //if(stat64(path.c_str(), &info) != 0)			
				return false;

			// check for S_IFLNK attribute
			return (S_ISLNK(info.st_mode));
#endif //_WIN32
			
		} // isSymlink

		bool isHidden(const std::string& _path)
		{
			auto it = FileCache::get(_path);
			if (it != nullptr)
				return it->exists && it->hidden;

			std::string path = getGenericPath(_path);

#ifdef WIN32
			// check for hidden attribute
			DWORD Attributes = GetFileAttributes(path.c_str());
			FileCache::add(_path, FileCache(Attributes));
			if((Attributes != INVALID_FILE_ATTRIBUTES) && (Attributes & FILE_ATTRIBUTE_HIDDEN))
				return true;
#endif // _WIN32

			// filenames starting with . are hidden in linux, we do this check for windows as well
			if(getFileName(path)[0] == '.')
				return true;

			// not hidden
			return false;

		} // isHidden

		std::string combine(const std::string& _path, const std::string& filename)
		{
			std::string gp = getGenericPath(_path);

			if (Utils::String::startsWith(filename, "/.."))
			{
				auto f = getPathList(filename);

				int count = 0;
				for (auto it = f.cbegin(); it != f.cend(); ++it)
				{
					if (*it != "..")
						break;

					count++;
				}

				if (count > 0)
				{
					auto list = getPathList(gp);

					std::string result;

					for (int i = 0; i < list.size() - count; i++)
					{
						if (result.empty())
							result = list.at(i);
						else
							result = result + "/" + list.at(i);
					}

					std::vector<std::string> fn(f.begin(), f.end());
					for (int i = count; i < fn.size(); i++)
					{
						if (result.empty())
							result = fn.at(i);
						else
							result = result + "/" + fn.at(i);
					}

					return result;
				}
			}


			if (!Utils::String::endsWith(gp, "/") && !Utils::String::endsWith(gp, "\\"))
				if (!Utils::String::startsWith(filename, "/") && !Utils::String::startsWith(filename, "\\"))
					gp += "/";

			return gp + filename;
		}

		size_t getFileSize(const std::string& _path)
		{
			std::string path = getGenericPath(_path);
			struct stat64 info;

			// check if stat64 succeeded
			if ((stat64(path.c_str(), &info) == 0))
				return (size_t)info.st_size;

			return 0;
		}

		Utils::Time::DateTime getFileCreationDate(const std::string& _path)
		{
			std::string path = getGenericPath(_path);
			struct stat64 info;

			// check if stat64 succeeded
			if ((stat64(path.c_str(), &info) == 0))
				return Utils::Time::DateTime(info.st_ctime);

			return Utils::Time::DateTime();
		}

		std::string	readAllText(const std::string fileName)
		{
			std::ifstream t(fileName);
			std::stringstream buffer;
			buffer << t.rdbuf();
			return buffer.str();
		}

		void writeAllText(const std::string fileName, const std::string text)
		{
			std::fstream fs;
			fs.open(fileName.c_str(), std::fstream::out);
			fs << text;
			fs.close();
		}

		bool copyFile(const std::string src, const std::string dst)
		{
			std::string path = getGenericPath(src);
			std::string pathD = getGenericPath(dst);

			// don't remove if it doesn't exists
			if (!exists(path))
				return true;

			char buf[512];
			size_t size;

			FILE* source = fopen(path.c_str(), "rb");
			if (source == nullptr)
				return false;

			FILE* dest = fopen(pathD.c_str(), "wb");
			if (dest == nullptr)
			{
				fclose(source);
				return false;
			}

			while (size = fread(buf, 1, 512, source))
				fwrite(buf, 1, size, dest);

			fclose(dest);
			fclose(source);

			return true;
		} // removeFile

		void deleteDirectoryFiles(const std::string path)
		{
			std::vector<std::string> directories;

			auto files = Utils::FileSystem::getDirContent(path, true, true);
			std::reverse(std::begin(files), std::end(files));
			for (auto file : files)
			{
				if (Utils::FileSystem::isDirectory(file))
					directories.push_back(file);
				else
					Utils::FileSystem::removeFile(file);
			}

			for (auto file : directories)
				rmdir(Utils::FileSystem::getPreferredPath(file).c_str());
		}

		std::string megaBytesToString(unsigned long size)
		{
			static const char *SIZES[] = { "MB", "GB", "TB" };
			int div = 0;
			unsigned long rem = 0;

			while (size >= 1024 && div < (sizeof SIZES / sizeof *SIZES))
			{
				rem = (size % 1024);
				div++;
				size /= 1024;
			}

			double size_d = (float)size + (float)rem / 1024.0;

			std::ostringstream out;
			out.precision(2);
			out << std::fixed << size_d << " " << SIZES[div];
			return out.str();
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

	} // FileSystem::

} // Utils::



