#define _FILE_OFFSET_BITS 64

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "utils/ZipFile.h"
#include "utils/md5.h"

#include "Settings.h"
#include <sys/stat.h>
#include <string.h>
#include <algorithm>
#include <set>
#include <shared_mutex>

#if defined(_WIN32)
// because windows...
#include <direct.h>
#include <Windows.h>
#include <mutex>
#include <io.h> 
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
#include <unordered_map>
#include <optional>

#include "Paths.h"

namespace Utils
{
	namespace FileSystem
	{		
		struct FileCache
		{
			FileCache() 
			{
				_directory = false;
				_exists = false;
				_hidden = false;
				_symlink = false;
			}

			FileCache(bool exists, bool dir, bool symLink = false)
			{
				_directory = dir;
				_exists = exists;
				_hidden = false;
				_symlink = symLink;
			}

#if WIN32			
			FileCache(DWORD dwFileAttributes)
			{
				if (0xFFFFFFFF == dwFileAttributes)
				{
					_directory = false;
					_exists = false;
					_hidden = false;
					_symlink = false;
				}
				else
				{
					_exists = true;
					_directory = dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
					_hidden = dwFileAttributes & FILE_ATTRIBUTE_HIDDEN;
					_symlink = dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT;
				}
			}			
			
			FileCache(WIN32_FIND_DATAW& data)
			{
				_exists = true;
				_directory = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
				_hidden = data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN;
				_symlink = data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT;
			}

			static void add(const std::string& key, WIN32_FIND_DATAW& data)
			{
				if (!Settings::UseFileCache())
					return;

				std::unique_lock<std::shared_mutex> guard(mFileCacheMutex);

				auto [it, inserted] = mFileCache.try_emplace(hashPath(key), data);
				if (!inserted)
				{
					it->second._exists = true;
					it->second._directory = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
					it->second._hidden = data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN;
					it->second._symlink = data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT;
				}
			}

			static void add(const std::string& key, DWORD dwFileAttributes)
			{
				if (!Settings::UseFileCache())
					return;

				std::unique_lock<std::shared_mutex> guard(mFileCacheMutex);

				auto [it, inserted] = mFileCache.try_emplace(hashPath(key), dwFileAttributes); 
				if (!inserted)
				{
					if (0xFFFFFFFF == dwFileAttributes)
					{
						it->second._directory = false;
						it->second._exists = false;
						it->second._hidden = false;
						it->second._symlink = false;
					}
					else
					{
						it->second._exists = true;
						it->second._directory = dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
						it->second._hidden = dwFileAttributes & FILE_ATTRIBUTE_HIDDEN;
						it->second._symlink = dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT;
					}					
				}
			}
#endif

			static int fromStat64(const std::string& key, struct stat64* info)
			{
#if WIN32			
				int ret = _wstat64(Utils::String::convertToWideString(key).c_str(), info);

				std::unique_lock<std::shared_mutex> guard(mFileCacheMutex);
				mFileCache.try_emplace(hashPath(key), ret == 0, ret == 0 && S_ISDIR(info->st_mode));				
#else
				int ret = stat64(key.c_str(), info);

				FileCache cache(ret == 0, false);
				if (cache._exists)
				{
					cache._directory = S_ISDIR(info->st_mode);
					cache._symlink = S_ISLNK(info->st_mode);
					if (cache._symlink)
					{
						struct stat64 si;
						if (stat64(resolveSymlink(key).c_str(), &si) == 0)
							cache._directory = S_ISDIR(si.st_mode);
					}
				}

				std::unique_lock<std::shared_mutex> guard(mFileCacheMutex);
				mFileCache[hashPath(key)] = cache;			
#endif

				return ret;
			}

			static void add(const std::string& key, bool exists, bool dir, bool symlink = false)
			{
				if (!Settings::UseFileCache())
					return;

				std::unique_lock<std::shared_mutex> guard(mFileCacheMutex);
				auto [it, inserted] = mFileCache.try_emplace(hashPath(key), exists, dir, symlink);
				if (!inserted)
				{
					it->second._exists = exists;
					it->second._directory = dir;
					it->second._symlink = symlink;
				}
			}

			static void remove(const std::string& key)
			{
				if (!Settings::UseFileCache())
					return;

				std::unique_lock<std::shared_mutex> guard(mFileCacheMutex);

				mFileCache.erase(hashPath(key));
				
				auto parent = Utils::FileSystem::getParent(key);
				while (!parent.empty())
				{
					mFileCache.erase(hashPath(parent + "/*"));
					parent = Utils::FileSystem::getParent(parent);
				}
			}

			template<typename Func>
			static std::optional<bool> queryCacheEntry(const std::string& key, Func func)
			{
				if (!Settings::UseFileCache())
					return std::nullopt;

				auto hash = hashPath(key);
				{
					std::shared_lock<std::shared_mutex> lock(mFileCacheMutex);
					if (auto* e = getCacheEntry(key, hash))
						return func(e);
				}

				auto fetched = statKey(key);
				std::unique_lock<std::shared_mutex> lock(mFileCacheMutex);
				return func(&insertEntry(hash, key, std::move(fetched)));
			}

			static std::optional<bool> exists(const std::string& key)
			{
				return queryCacheEntry(key, [](const FileCache* e) { return std::optional<bool>(e->_exists); });
			}

			// Non-blocking probe: returns true if the key's existence is already
			// recorded in the cache (either exists or known-not-to-exist).
			// Never calls stat64 — safe to call from the render thread.
			static bool isCached(const std::string& key)
			{
				if (!Settings::UseFileCache())
					return false;

				auto hash = hashPath(key);
				std::shared_lock<std::shared_mutex> lock(mFileCacheMutex);
				if (mFileCache.count(hash))
					return true;
				return mFileCache.count(hashPath(Utils::FileSystem::getParent(key) + "/*")) > 0;
			}

			static std::optional<bool> isRegularFile(const std::string& key)
			{
				return queryCacheEntry(key, [](const FileCache* e) { return std::optional<bool>(e->_exists && !e->_directory && !e->_symlink); });
			}

			static std::optional<bool> isDirectory(const std::string& key)
			{
				return queryCacheEntry(key, [](const FileCache* e) { return std::optional<bool>(e->_exists && e->_directory); });
			}

			static std::optional<bool> isSymlink(const std::string& key)
			{
				return queryCacheEntry(key, [](const FileCache* e) { return std::optional<bool>(e->_exists && e->_symlink); });
			}

			static std::optional<bool> isHidden(const std::string& key)
			{
				return queryCacheEntry(key, [&key](const FileCache* e) { return std::optional<bool>(e->_exists && (e->_hidden || getFileName(key)[0] == '.')); });
			}

			static void resetCache()
			{
				std::unique_lock<std::shared_mutex> guard(mFileCacheMutex);
				mFileCache.clear();
			}

		private:
			// Looks up key in the cache. MUST be called under at least a shared_lock.
			// Returns pointer to the entry if found (including parent-wildcard hits,
			// which return a static "not exists" sentinel), or nullptr if not cached.
			static const FileCache* getCacheEntry(const std::string& key, size_t hash)
			{
				if (mFileCache.empty())
					return nullptr;

				auto it = mFileCache.find(hash);
				if (it != mFileCache.cend())
					return &it->second;

				it = mFileCache.find(hashPath(Utils::FileSystem::getParent(key) + "/*"));
				if (it != mFileCache.cend())
				{
					static const FileCache notExists(false, false);
					return &notExists;
				}

				return nullptr;
			}

			// Performs filesystem I/O with NO lock held — safe to call concurrently.
			static FileCache statKey(const std::string& key)
			{
#ifdef WIN32
				return FileCache(GetFileAttributesW(Utils::String::convertToWideString(key).c_str()));
#else
				struct stat64 info;
				int ret = stat64(key.c_str(), &info);

				bool exists = ret == 0;
				bool directory = exists && S_ISDIR(info.st_mode);
				bool symlink = exists && S_ISLNK(info.st_mode);
				if (symlink)
				{
					struct stat64 si;
					if (stat64(resolveSymlink(key).c_str(), &si) == 0)
						directory = S_ISDIR(si.st_mode);
				}

				return FileCache(exists, directory, symlink);
#endif
			}

			// Inserts a pre-computed entry. MUST be called with exclusive lock held.
			// Double-checks for races: returns the authoritative entry (ours or a
			// concurrent thread's insert, whichever arrived first).
			static const FileCache& insertEntry(size_t hash, const std::string& key, FileCache fetched)
			{
				auto it = mFileCache.find(hash);
				if (it != mFileCache.cend())
					return it->second;

				it = mFileCache.find(hashPath(Utils::FileSystem::getParent(key) + "/*"));
				if (it != mFileCache.cend())
					return mFileCache.try_emplace(hash, false, false).first->second;

				return mFileCache.try_emplace(hash, std::move(fetched)).first->second;
			}

			bool _exists;
			bool _directory;
			bool _hidden;
			bool _symlink;

			static std::unordered_map<size_t, FileCache> mFileCache;
			static std::shared_mutex mFileCacheMutex;

			static size_t hashPath(const std::string& path) 
			{ 
				return std::hash<std::string>{}(getGenericPath(path));
			}
		};

		std::unordered_map<size_t, FileCache> FileCache::mFileCache;
		std::shared_mutex FileCache::mFileCacheMutex;

		void FileSystemCache::reset()
		{
			FileCache::resetCache();
		}

		void FileSystemCache::reset(const std::string& file)
		{
			FileCache::remove(file);
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
				FileCache::add(path + "/*", true, true);

#if defined(_WIN32)
				WIN32_FIND_DATAW findData;
				std::string      wildcard = path + "/*";
				
				HANDLE hFind = FindFirstFileExW(Utils::String::convertToWideString(wildcard).c_str(),
					FINDEX_INFO_LEVELS::FindExInfoBasic, &findData, FINDEX_SEARCH_OPS::FindExSearchNameMatch
					, NULL, FIND_FIRST_EX_LARGE_FETCH);

				if (hFind != INVALID_HANDLE_VALUE)
				{
					std::string pathPrefix = path + "/";

					// loop over all files in the directory
					do
					{
						if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && 
							(findData.cFileName[0] == L'.' && (findData.cFileName[1] == L'\0' || (findData.cFileName[1] == L'.' && findData.cFileName[2] == L'\0'))))
							continue;

						std::string fullName = getGenericPath(pathPrefix + Utils::String::convertFromWideString(findData.cFileName));
						FileCache::add(fullName, findData);

						if (!includeHidden && (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == FILE_ATTRIBUTE_HIDDEN)
							continue;

						contentList.push_back(fullName);

						if (_recursive && (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
						{
							for (auto item : getDirContent(fullName, true, includeHidden))
								contentList.push_back(item);
						}
					} while (FindNextFileW(hFind, &findData));

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
							
							bool hidden = getFileName(fullName)[0] == '.';
							bool directory = false;							

							if (entry->d_type == 10)
							{
								struct stat64 info;
								if (stat64(resolveSymlink(fullName).c_str(), &info) == 0)
									directory = S_ISDIR(info.st_mode);
								else
									directory = false;
							}
							else
								directory = (entry->d_type == 4);

							FileCache::add(fullName, true, directory, entry->d_type == 10);

							if (!includeHidden && hidden)
								continue;

							contentList.push_back(fullName);

							if (_recursive && directory)
							{
								for (auto item : getDirContent(fullName, true, includeHidden))
									contentList.push_back(item);
							}
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

#if WIN32
		static time_t to_time_t(FILETIME& ft)
		{
			ULARGE_INTEGER ull;
			ull.LowPart = ft.dwLowDateTime;
			ull.HighPart = ft.dwHighDateTime;
			time_t ret = (ull.QuadPart / 10000000ULL - 11644473600ULL);
			return ret;
		}
#endif

		fileList getDirectoryFiles(const std::string& _path)
		{
			std::string path = getGenericPath(_path);
			fileList  contentList;

			// tell filecache we enumerated the folder
			FileCache::add(path + "/*", true, true);

			// only parse the directory, if it's a directory
			// if (isDirectory(path))
			{			
#if defined(_WIN32)

				if (_path.empty() || _path == "\\" || _path == "/")
				{
					char drive = 'A';

					DWORD uDriveMask = ::GetLogicalDrives();
					while (uDriveMask)
					{
						if (uDriveMask & 1)
						{
							contentList.emplace_back();
							FileInfo& fi = contentList.back();												
							fi.path = std::string(1, drive) + ":";
							fi.hidden = false;
							fi.directory = true;							
						}

						drive++;
						uDriveMask >>= 1;
					}

					return contentList;
				}

				WIN32_FIND_DATAW findData;
				std::string      wildcard = path + "/*";
				
				HANDLE hFind = FindFirstFileExW(Utils::String::convertToWideString(wildcard).c_str(),
					FINDEX_INFO_LEVELS::FindExInfoBasic, &findData, FINDEX_SEARCH_OPS::FindExSearchNameMatch
					, NULL, FIND_FIRST_EX_LARGE_FETCH);

				if (hFind != INVALID_HANDLE_VALUE)
				{
					std::string pathPrefix = path + "/";

					// loop over all files in the directory
					do
					{
						if (findData.cFileName == nullptr)
							continue;

						if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY &&
							(findData.cFileName[0] == '.' && (findData.cFileName[1] == '.' || findData.cFileName[1] == 0)))
							continue;

						contentList.emplace_back();
						FileInfo& fi = contentList.back();						
						fi.path = pathPrefix + Utils::String::convertFromWideString(findData.cFileName);
						fi.hidden = (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == FILE_ATTRIBUTE_HIDDEN;
						fi.directory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
						fi.lastWriteTime = to_time_t(findData.ftLastWriteTime);						

						FileCache::add(fi.path, findData);						
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
							fi.hidden = getFileName(fullName)[0] == '.';

							if (entry->d_type == 10) // DT_LNK
							{
								struct stat64 si;
								if (stat64(resolveSymlink(fullName).c_str(), &si) == 0)
									fi.directory = S_ISDIR(si.st_mode);
								else
									fi.directory = false;
							}
							else
								fi.directory = (entry->d_type == 4); // DT_DIR;

							FileCache::add(fullName, true, fi.directory, entry->d_type == 10);

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

		std::string getCWDPath()
		{
			// return current working directory path
			char temp[2048];
			return (getcwd(temp, 2048) ? getGenericPath(temp) : "");
		}

		std::string getPreferredPath(const std::string& _path)
		{
#if _WIN32
			std::string path   = _path;

			// convert '/' to '\\'

			size_t      offset = std::string::npos;
			while((offset = path.find('/')) != std::string::npos)
				path.replace(offset, 1, "\\");

			return path;
#else
			return _path;
#endif
		}

		std::string getGenericPath(const std::string& _path)
		{
			if (_path.empty())
				return _path;

			std::string path   = _path;
			size_t      offset = std::string::npos;

			// remove "\\\\?\\"
			if(path[0] == '\\' && (path.find("\\\\?\\")) == 0)
				path.erase(0, 4);

			// convert '\\' to '/'
			while ((offset = path.find('\\')) != std::string::npos)
				path[offset] = '/';// .replace(offset, 1, "/");

			// remove double '/'
			while((offset = path.find("//")) != std::string::npos)
				path.erase(offset, 1);

			// remove trailing '/'
			while(path.length() && ((offset = path.find_last_of('/')) == (path.length() - 1)))
				path.erase(offset, 1);

			// return generic path
			return path;
		}

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

		}

		std::string getCanonicalPath(const std::string& _path)
		{
			// temporary hack for builtin resources
			if (_path.size() >= 2 && _path[0] == ':' && _path[1] == '/')
				return _path;

#if WIN32
			std::string path = _path[0] == '.' ? getAbsolutePath(_path) : _path;
			if (path.find("./") == std::string::npos && path.find(".\\") == std::string::npos)
				return path;
#else
			// For absolute paths, skip the stat — exists() is only needed to resolve
			// relative paths, and getAbsolutePath() on an already-absolute path is a no-op.
			std::string path = isAbsolute(_path) ? getGenericPath(_path) : (exists(_path) ? getAbsolutePath(_path) : getGenericPath(_path));
			// Early return if there are no . or .. components to normalize
			if (path.find("/.") == std::string::npos)
				return path;
#endif
			
			int indexes[32];
			int index = -1;
			char fp[4096];

			int pos = 0;
			int ofs = 0;

			bool pointset = false;
			bool twopointset = false;

			for (int i = 0; i < path.size(); i++)
			{
				char c = path[i];

				if (c == '/' || c == '\\')
				{
					if (twopointset)
					{
						if (index > 0)
							pos = indexes[--index] + 1;

						twopointset = false;
						pointset = false;
						continue;
					}
					else if (pointset)
					{
						pos = indexes[index] + 1;
						pointset = false;
						continue;
					}
					else
						indexes[++index] = pos;

					fp[pos++] = '/';
					continue;
				}
				else if (c == '.')
				{
					if (pointset)
					{
						twopointset = true;
						pointset = false;
					}
					else if (index >= 0 && i > 0 && (path[i-1] == '/' || path[i - 1] == '\\'))
						pointset = true;
				}
				else
				{
					twopointset = false;
					pointset = false;
				}

				fp[pos++] = c;
			}

			fp[pos] = 0;
			return fp;			

		}

		std::string getAbsolutePath(const std::string& _path, const std::string& _base)
		{
			if (isAbsolute(_path))
				return getGenericPath(_path);

			return getCanonicalPath(_base + "/" + _path);
		}

		std::string getParent(const std::string& _path)
		{			
			for (int i = _path.size() - 1; i > 0; i--)
				if (_path[i] == '/' || _path[i] == '\\')
					return _path.substr(0, i);
			
			return "";
		}

		std::string getFileName(const std::string& _path)
		{
			for (int i = _path.size() - 1; i > 0; i--)
				if (_path[i] == '/' || _path[i] == '\\')
					return _path.substr(i + 1);

			return _path;
		}

		std::string getStem(const std::string& _path)
		{
			int lastPathSplit = 0;
			int extPos = -1;

			auto* p = _path.c_str();
			for (int i = _path.size() - 1 ; i >= 0 ; i--)
			{
				if (extPos < 0 && p[i] == '.')
					extPos = i;

				if (p[i] == '/' || p[i] == '\\')
				{
					lastPathSplit = i + 1;
					break;
				}
			}

			if (extPos < 0)
			{
				if (lastPathSplit == 0)
					return _path;

				return _path.substr(lastPathSplit);
			}

			return _path.substr(lastPathSplit, extPos - lastPathSplit);
		}

		std::string getExtension(const std::string& _path, bool withPoint)
		{
			const char* path = _path.c_str();
			const char* ptr = path + _path.length();

			do
			{
				if (ptr[-1] == '.')
					return withPoint ? ptr -1 : ptr;

				--ptr;
			} 
			while (ptr > path);

			return "";
		}

		std::string changeExtension(const std::string& _path, const std::string& extension)
		{
			if (_path.empty())
				return _path;

			std::string str = _path;
			int length = _path.length();
			while (--length >= 0)
			{
				char ch = _path[length];
				if (ch == '.')
				{
					str = _path.substr(0, length);
					break;
				}

				if (((ch == '/') || (ch == '\\')) || (ch == ':'))
					break;
			}

			if (extension.empty())
				return str;

			if (extension.length() == 0 || extension[0] != '.')
				str = str + ".";

			return str + extension;
		}



		std::string resolveRelativePath(const std::string& _path, const std::string& _relativeTo, const bool _allowHome)
		{
			// nothing to resolve
			if(!_path.length())
				return _path;

			if (_path.length() == 1 && _path[0] == '.')
				return getGenericPath(_relativeTo);

			// replace '.' with relativeTo
			if ((_path[0] == '.') && (_path[1] == '/' || _path[1] == '\\'))
			{
				if (_path[2] == '.' && (_path[3] == '.' || _path[3] == '/' || _path[3] == '\\')) // ./.. or ././ ?
					return getCanonicalPath(_relativeTo + &(_path[1]));

				return getGenericPath(_relativeTo + &(_path[1]));
			}

			// replace '~' with homePath
			if(_allowHome && (_path[0] == '~') && (_path[1] == '/' || _path[1] == '\\'))
				return getCanonicalPath(Paths::getHomePath() + &(_path[1]));

			// nothing to resolve
			return getGenericPath(_path);

		} // resolveRelativePath

		std::string createRelativePath(const std::string& _path, const std::string& _relativeTo, const bool _allowHome)
		{
			if (_relativeTo.empty())
				return _path;

			if (_path[0] == '.' && _path[1] == '/')
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
				auto from_dirs = getPathList(Paths::getHomePath());
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
			if (lstat(path.c_str(), &info) == 0)
			{
				resolved.resize(4096);

				int cnt = readlink(path.c_str(), (char*)resolved.data(), resolved.size());
				if (cnt > 0)
				{
					resolved.resize(cnt);

					if (resolved[0] == '/')
						resolved = getGenericPath(resolved);
					else
						resolved = getAbsolutePath(resolved, getParent(path));
				}
			}
#endif // _WIN32

			// return resolved path
			return resolved;

		} // resolveSymlink

		bool removeDirectory(const std::string& _path)
		{
			std::string path = getGenericPath(_path);

			// don't remove if it doesn't exists
			if (!exists(path, false))
				return true;

			FileCache::remove(path);
#if WIN32			
			return RemoveDirectoryW(Utils::String::convertToWideString(getPreferredPath(_path)).c_str());
#else
			return (rmdir(path.c_str()) == 0);
#endif
		}

		bool removeFile(const std::string& _path)
		{
			std::string path = getGenericPath(_path);

			// don't remove if it doesn't exists
			if (!exists(path, false))
				return true;

			FileCache::remove(path);
#if WIN32			
			if (isDirectory(_path))
				return RemoveDirectoryW(Utils::String::convertToWideString(getPreferredPath(_path)).c_str());
						
			if (!DeleteFileW(Utils::String::convertToWideString(_path).c_str()))
			{
				SetFileAttributesW(Utils::String::convertToWideString(_path).c_str(), FILE_ATTRIBUTE_NORMAL);
				return DeleteFileW(Utils::String::convertToWideString(_path).c_str());
			}

			return true;
#else
			if (isDirectory(_path))
				return (rmdir(path.c_str()) == 0);

			// try to remove file
			return (unlink(path.c_str()) == 0);
#endif

		} // removeFile

		bool createDirectory(const std::string& _path)
		{
			std::string path = getGenericPath(_path);

			// don't create if it already exists
			if (exists(path, false))
				return true;

			FileCache::remove(path);

#ifdef WIN32	
			if (::CreateDirectoryW(Utils::String::convertToWideString(_path).c_str(), nullptr))
				return true;
#endif

			// try to create directory
			if (mkdir(path.c_str(), 0755) == 0)
				return true;

			// failed to create directory, try to create the parent
			std::string parent = getParent(path);

			// only try to create parent if it's not identical to path
			if (parent != path)
				createDirectory(parent);
						
			// try to create directory again now that the parent should exist
			if (mkdir(path.c_str(), 0755) == 0)
				return true;

			return false;
		} // createDirectory

		bool exists(const std::string& _path, bool enableCache)
		{
			if (_path.empty())
				return false;

			if (enableCache)
			{
				auto val = FileCache::exists(_path);
				if (val)
					return *val;
			}

#ifdef WIN32			
			DWORD dwAttr = GetFileAttributesW(Utils::String::convertToWideString(_path).c_str());
			FileCache::add(_path, dwAttr);
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
			auto it = FileCache::isRegularFile(_path);
			if (it) return *it;

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
			auto it = FileCache::isDirectory(_path);
			if (it) return *it;

#ifdef WIN32
			// check for symlink attribute
			DWORD dwAttributes = GetFileAttributesW(Utils::String::convertToWideString(_path).c_str());
			FileCache::add(_path, dwAttributes);
			return (dwAttributes != INVALID_FILE_ATTRIBUTES) && (dwAttributes & FILE_ATTRIBUTE_DIRECTORY);
#else // _WIN32
			std::string path = getGenericPath(_path);
			struct stat64 info;

			// check if stat succeeded
			if (FileCache::fromStat64(path, &info) != 0) //if(stat64(path.c_str(), &info) != 0)
				return false;

			if (S_ISLNK(info.st_mode))
			{
				if (FileCache::fromStat64(resolveSymlink(path), &info) != 0) //if(stat64(path.c_str(), &info) != 0)
					return false;
			}

			// check for S_IFDIR attribute
			return (S_ISDIR(info.st_mode));
#endif
		} // isDirectory

		bool isSymlink(const std::string& _path)
		{		
			auto it = FileCache::isSymlink(_path);
			if (it) return *it;
				
			std::string path = getGenericPath(_path);

#ifdef WIN32
			// check for symlink attribute
			DWORD dwAttributes = GetFileAttributes(path.c_str());
			FileCache::add(_path, dwAttributes);
			if((dwAttributes != INVALID_FILE_ATTRIBUTES) && (dwAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
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
			auto it = FileCache::isHidden(_path);
			if (it) return *it;

			std::string path = getGenericPath(_path);

#ifdef WIN32
			// check for hidden attribute
			DWORD dwAttributes = GetFileAttributes(path.c_str());
			FileCache::add(_path, dwAttributes);
			if ((dwAttributes != INVALID_FILE_ATTRIBUTES) && (dwAttributes & FILE_ATTRIBUTE_HIDDEN))
				return true;
#endif // _WIN32

			// Filenames starting with . are hidden in linux, we do this check for windows as well
			if (getFileName(path)[0] == '.')
				return true;
		
			return false;
		}

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

			if (gp.empty())
				return filename;
			else if (Utils::String::endsWith(gp, ":/"))
				return gp + filename;
			else if (Utils::String::endsWith(gp, ":"))
				return gp + "/" + filename;

			if (!Utils::String::endsWith(gp, "/") && !Utils::String::endsWith(gp, "\\"))
				if (!Utils::String::startsWith(filename, "/") && !Utils::String::startsWith(filename, "\\"))
					gp += "/";

			return gp + filename;
		}

		unsigned long long getFileSize(const std::string& _path)
		{
			std::string path = getGenericPath(_path);
			struct stat64 info;


#if defined(_WIN32)
			if ((_wstat64(Utils::String::convertToWideString(path).c_str(), &info) == 0))
				return (unsigned long long)info.st_size;
#else
			// check if stat64 succeeded
			if ((stat64(path.c_str(), &info) == 0))
				return (unsigned long long)info.st_size;
#endif

			return 0;
		}

		Utils::Time::DateTime getFileCreationDate(const std::string& _path)
		{
			std::string path = getGenericPath(_path);
			struct stat64 info;

			// check if stat64 succeeded
#if defined(_WIN32)
			if ((_wstat64(Utils::String::convertToWideString(path).c_str(), &info) == 0))
				return Utils::Time::DateTime(info.st_ctime);
#else
			if ((stat64(path.c_str(), &info) == 0))
				return Utils::Time::DateTime(info.st_ctime);
#endif
			return Utils::Time::DateTime();
		}

		Utils::Time::DateTime getFileModificationDate(const std::string& _path)
		{
			std::string path = getGenericPath(_path);
			struct stat64 info;

			// check if stat64 succeeded
#if defined(_WIN32)
			if ((_wstat64(Utils::String::convertToWideString(path).c_str(), &info) == 0))
				return Utils::Time::DateTime(info.st_mtime);
#else
			if ((stat64(path.c_str(), &info) == 0))
				return Utils::Time::DateTime(info.st_mtime);
#endif
			return Utils::Time::DateTime();
		}

		static void skipUtf8Bom(std::ifstream& file) 
		{
			if (!file.is_open())
				return;

			char bom[3];
			if (file.read(bom, 3) && bom[0] == '\xEF' && bom[1] == '\xBB' && bom[2] == '\xBF')			
				return;
			
			file.seekg(0);
		}

		std::vector<char> readAllBytes(const std::string& fileName)
		{
			std::ifstream file(WINSTRINGW(fileName), std::ios::binary | std::ios::ate);
			if (!file.is_open())
				return {};

			std::streamsize size = file.tellg();
			if (size <= 0)
				return {};

			file.seekg(0, std::ios::beg);
			
			std::vector<char> buffer(size);			
			if (!file.read(buffer.data(), size))
				return {};

			return buffer;
		}

		std::list<std::string> readAllLines(const std::string& fileName)
		{
			std::list<std::string> lines;

			std::ifstream file(WINSTRINGW(fileName));
			if (!file.is_open()) 
				return lines;

			skipUtf8Bom(file);

			std::string line;
			while (std::getline(file, line))
				lines.push_back(line);

			file.close();
			return lines;
		}

		std::string	readAllText(const std::string& fileName)
		{
			std::ifstream t(WINSTRINGW(fileName));

			skipUtf8Bom(t);

			std::stringstream buffer;
			buffer << t.rdbuf();
			return buffer.str();
		}

		void writeAllText(const std::string& fileName, const std::string& text)
		{
#if defined(_WIN32)
			FILE* file = _wfopen(Utils::String::convertToWideString(fileName).c_str(), L"wb");
#else
			FILE* file = fopen(fileName.c_str(), "wb");
#endif
			if (file == nullptr)
				return;		

			void* buffer = (void*) text.data();
			size_t size = text.size();

			fwrite(buffer, 1, size, file);
			fclose(file);

			FileCache::remove(getGenericPath(fileName));
		}

		bool renameFile(const std::string& src, const std::string& dst, bool overWrite)
		{
			std::string path = getGenericPath(src);			
			if (!exists(path, false))
				return true;

			if (overWrite && exists(dst, false))
				removeFile(dst);

			FileCache::remove(path);
			FileCache::remove(dst);

#if WIN32			
			return MoveFileW(Utils::String::convertToWideString(path).c_str(), Utils::String::convertToWideString(dst).c_str());
#else
			return std::rename(src.c_str(), dst.c_str()) == 0;
#endif
		}

		bool copyFile(const std::string& src, const std::string& dst)
		{
			std::string path = getGenericPath(src);
			std::string pathD = getGenericPath(dst);

			// don't remove if it doesn't exists
			if (!exists(path, false))
				return true;

			Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(pathD));

			char buf[512];
			size_t size;

#if defined(_WIN32)
			FILE* source = _wfopen(Utils::String::convertToWideString(path).c_str(), L"rb");
#else
			FILE* source = fopen(path.c_str(), "rb");
#endif
			if (source == nullptr)
				return false;

#if defined(_WIN32)
			FILE* dest = _wfopen(Utils::String::convertToWideString(pathD).c_str(), L"wb");
#else
			FILE* dest = fopen(pathD.c_str(), "wb");
#endif
			if (dest == nullptr)
			{
				fclose(source);
				return false;
			}

			while (size = fread(buf, 1, 512, source))
				fwrite(buf, 1, size, dest);

			fclose(dest);
			fclose(source);

			FileCache::remove(path);
			FileCache::remove(pathD);

			return true;
		} // removeFile

		void deleteDirectoryFiles(const std::string& path, bool deleteDirectory)
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
				removeDirectory(file);

			if (deleteDirectory)
				removeDirectory(path);
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

		std::string kiloBytesToString(unsigned long size)
		{
			static const char *SIZES[] = { "KB", "MB", "GB", "TB" };
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

		std::string getTempPath()
		{
			static std::string path;

			if (path.empty())
			{
#ifdef WIN32
				// Set tmp files to local drive : usually faster since it's generally a SSD Drive
				WCHAR lpTempPathBuffer[MAX_PATH];
				lpTempPathBuffer[0] = 0;
				DWORD dwRetVal = ::GetTempPathW(MAX_PATH, lpTempPathBuffer);
				if (dwRetVal > 0 && dwRetVal <= MAX_PATH)
					path = Utils::FileSystem::getGenericPath(Utils::String::convertFromWideString(lpTempPathBuffer)) + "/emulationstation.tmp";
				else
#endif
					path = Utils::FileSystem::getGenericPath(Paths::getUserEmulationStationPath() + "/tmp");
			}

			if (!Utils::FileSystem::isDirectory(path))
				Utils::FileSystem::createDirectory(path);

			return path;
		}

		std::string getPdfTempPath()
		{
			static std::string pdfpath;

			if (pdfpath.empty())
			{
#ifdef WIN32
				// Extract PDFs to local drive : usually faster since it's generally a SSD Drive
				WCHAR lpTempPathBuffer[MAX_PATH];
				lpTempPathBuffer[0] = 0;
				DWORD dwRetVal = ::GetTempPathW(MAX_PATH, lpTempPathBuffer);
				if (dwRetVal > 0 && dwRetVal <= MAX_PATH)
					pdfpath = Utils::FileSystem::getGenericPath(Utils::String::convertFromWideString(lpTempPathBuffer)) + "/pdftmp";
				else
#endif						
					pdfpath = Utils::FileSystem::getGenericPath(Paths::getUserEmulationStationPath() + "/pdftmp");
			}

			if (!Utils::FileSystem::isDirectory(pdfpath))
				Utils::FileSystem::createDirectory(pdfpath);

			return pdfpath;
		}
		
		std::string getFileCrc32(const std::string& filename)
		{
			std::string hex;

#if defined(_WIN32)
			FILE* file = _wfopen(Utils::String::convertToWideString(filename).c_str(), L"rb");
#else			
			FILE* file = fopen(filename.c_str(), "rb");
#endif
			if (file)
			{
				// Retroarch CRC calculations are limited in size. See encoding_crc32.c
				#define CRC32_BUFFER_SIZE 1048576
				#define CRC32_MAX_MB 64

				char* buffer = new char[CRC32_BUFFER_SIZE];
				if (buffer)
				{
					size_t size;
					unsigned int file_crc32 = 0;

					for (int i = 0; i < CRC32_MAX_MB; i++)
					{
						size = fread(buffer, 1, CRC32_BUFFER_SIZE, file);
						if (size == 0)
							break;

						file_crc32 = Utils::Zip::ZipFile::computeCRC(file_crc32, buffer, size);
					}

					hex = Utils::String::toHexString(file_crc32);

					delete[] buffer;
				}

				fclose(file);
			}

			return hex;
		}

		std::string getFileMd5(const std::string& filename)
		{
			std::string hex;

#if defined(_WIN32)
			FILE* file = _wfopen(Utils::String::convertToWideString(filename).c_str(), L"rb");
#else			
			FILE* file = fopen(filename.c_str(), "rb");
#endif
			if (file)
			{
				#define CRCBUFFERSIZE 64 * 1024
				char* buffer = new char[CRCBUFFERSIZE];

				if (buffer)
				{
					MD5 md5;

					size_t size;
					while (size = fread(buffer, 1, CRCBUFFERSIZE, file))
						md5.update(buffer, size);

					md5.finalize();
					hex = md5.hexdigest();

					delete[] buffer;
				}

				fclose(file);
			}

			return hex;
		}		

		static std::set<std::string> _imageExtensions = { ".jpg", ".png", ".jpeg", ".gif", ".webp" };
		static std::set<std::string> _videoExtensions = { ".mp4", ".avi", ".mkv", ".webm" };
		static std::set<std::string> _audioExtensions = { ".mp3", ".wav", ".ogg", ".flac", ".mod", ".xm", ".stm", ".s3m", ".far", ".it", ".669", ".mtm", ".opus" };

		bool isSVG(const std::string& _path)
		{
			return Utils::String::toLower(Utils::FileSystem::getExtension(_path)) == ".svg";
		}

		bool isImage(const std::string& _path)
		{
			return _imageExtensions.find(Utils::String::toLower(Utils::FileSystem::getExtension(_path))) != _imageExtensions.cend();
		}

		bool isVideo(const std::string& _path)
		{
			return _videoExtensions.find(Utils::String::toLower(Utils::FileSystem::getExtension(_path))) != _videoExtensions.cend();
		}

		bool isAudio(const std::string& _path)
		{
			return _audioExtensions.find(Utils::String::toLower(Utils::FileSystem::getExtension(_path))) != _audioExtensions.cend();
		}

#ifdef WIN32
		void splitCommand(const std::string& cmd, std::string* executable, std::string* parameters)
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
		void preloadFileSystemCache(const std::string& path, bool trySaveStates)
		{
#if WIN32		
			if (path.empty())
				return;

			if (!Settings::UseFileCache())
				return;

			static std::set<std::string> preloaded;

			if (preloaded.find(path) != preloaded.cend())
				return;

			preloaded.insert(path);

			auto doWork = [&](std::string* dir)
			{
				if (trySaveStates)
				{
					std::string systemName = Utils::FileSystem::getFileName(*dir);
					Utils::FileSystem::getDirContent(Utils::FileSystem::combine(Paths::getSavesPath(), systemName));
				}

				Utils::FileSystem::getDirContent(Utils::FileSystem::combine(*dir, "/images"));
				Utils::FileSystem::getDirContent(Utils::FileSystem::combine(*dir, "/videos"));
				Utils::FileSystem::getDirContent(Utils::FileSystem::combine(*dir, "/manuals"));

				delete dir;
			};

			std::thread thread(doWork, new std::string(path));

			::SetThreadDescription(thread.native_handle(), L"PreloadFileSystemCache");
			::SetThreadPriority(thread.native_handle(), THREAD_PRIORITY_LOWEST);
			thread.detach();
#endif
		}

	} // FileSystem::

} // Utils::



