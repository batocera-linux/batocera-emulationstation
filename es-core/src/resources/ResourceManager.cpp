#include "ResourceManager.h"

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <fstream>
#include <algorithm>
#include "Log.h"
#include "Settings.h"
#include "Paths.h"
#include "utils/ConcurrentVector.h"
#include <unordered_map>

auto array_deleter = [](unsigned char* p) { delete[] p; };

std::shared_ptr<ResourceManager> ResourceManager::sInstance = nullptr;

ResourceManager::ResourceManager()
{
}

std::shared_ptr<ResourceManager>& ResourceManager::getInstance()
{
	if (!sInstance)
		sInstance = std::shared_ptr<ResourceManager>(new ResourceManager());

	return sInstance;
}

static std::mutex                                 _cacheBuildLock;
static std::vector<std::string>                   _cachedPaths;
static std::string                                _cachedThemeSet;
static ConcurrentMap<std::string, std::string>    _resourcePathCache;

std::vector<std::string> ResourceManager::getResourcePaths() const
{
	auto themeSet = Settings::getInstance()->getString("ThemeSet");

	std::unique_lock<std::mutex> lock(_cacheBuildLock);
	if (_cachedPaths.size() == 0 || _cachedThemeSet != themeSet)
	{	
		_cachedThemeSet = themeSet;
		_cachedPaths.clear();
		_resourcePathCache.clear();

		// check if theme overrides default resources
		if (!Paths::getUserThemesPath().empty())
		{
			std::string themePath = Paths::getUserThemesPath() + "/" + themeSet + "/resources";
			if (Utils::FileSystem::isDirectory(themePath))
				_cachedPaths.push_back(themePath);
		}

		if (!Paths::getThemesPath().empty())
		{
			std::string roThemePath = Paths::getThemesPath() + "/" + themeSet + "/resources";
			if (Utils::FileSystem::isDirectory(roThemePath))
				_cachedPaths.push_back(roThemePath);
		}

		// check in homepath
		_cachedPaths.push_back(Paths::getUserEmulationStationPath() + "/resources");

		// check in emulationStation path
		_cachedPaths.push_back(Paths::getEmulationStationPath() + "/resources");

		// check in Exe path
		if (Paths::getEmulationStationPath() != Paths::getExePath())
			_cachedPaths.push_back(Paths::getExePath() + "/resources");

		// check in cwd
		auto cwd = Utils::FileSystem::getCWDPath() + "/resources";
		if (std::find(_cachedPaths.cbegin(), _cachedPaths.cend(), cwd) == _cachedPaths.cend())
			_cachedPaths.push_back(cwd);
	}

	return _cachedPaths;
}

std::string ResourceManager::getResourcePath(const std::string& path) const
{
	// check if this is a resource file
	if (path.size() < 2 || path[0] != ':' || path[1] != '/')
		return path;
	
	std::string value;
	if (_resourcePathCache.find(path, value))
		return value;

	for (auto testPath : getResourcePaths())
	{
		std::string test = testPath + "/" + &path[2];
		if (Utils::FileSystem::exists(test))
		{
			_resourcePathCache.insert(path, test);
			return test;
		}
	}

#if WIN32
	if (Utils::String::startsWith(path, ":/locale/") || Utils::String::startsWith(path, ":/es_features.locale/"))
	{
		std::string test = Utils::FileSystem::getCanonicalPath(Paths::getEmulationStationPath() + "/" + &path[2]);
		if (Utils::FileSystem::exists(test))
		{
			_resourcePathCache.insert(path, test);
			return test;
		}

		test = Utils::FileSystem::getCanonicalPath(Paths::getUserEmulationStationPath() + "/" + &path[2]);
		if (Utils::FileSystem::exists(test))
		{
			_resourcePathCache.insert(path, test);
			return test;
		}
	}
#endif

	LOG(LogDebug) << "Resource path not found: " << path;
	_resourcePathCache.insert(path, path);

	// not a resource, return unmodified path
	return path;
}

const ResourceData ResourceManager::getFileData(const std::string& path) const
{
	//check if its a resource
	const std::string respath = getResourcePath(path);

	auto size = Utils::FileSystem::getFileSize(respath);
	if (size > 0)
	{
		ResourceData data = loadFile(respath, size);
		return data;
	}

	//if the file doesn't exist, return an "empty" ResourceData
	ResourceData data = {NULL, 0};
	return data;
}

ResourceData ResourceManager::loadFile(const std::string& path, size_t size) const
{
	std::ifstream stream(WINSTRINGW(path), std::ios::binary);

	if (size == 0 || size == SIZE_MAX)
	{
		stream.seekg(0, stream.end);
		size = (size_t)stream.tellg();
		stream.seekg(0, stream.beg);
	}

	//supply custom deleter to properly free array
	std::shared_ptr<unsigned char> data(new unsigned char[size], array_deleter);
	stream.read((char*)data.get(), size);
	stream.close();

	ResourceData ret = {data, size};
	return ret;
}

bool ResourceManager::fileExists(const std::string& path) const
{
	// Animated Gifs : Check if the extension contains a ',' -> If it's the case, we have the multi-image index as argument
	if (Utils::FileSystem::getExtension(path).find(',') != std::string::npos)
	{
		auto idx = path.rfind(',');
		if (idx != std::string::npos)
			return fileExists(path.substr(0, idx));
	}

	if (path[0] != ':' && path[0] != '~' && path[0] != '/')
		return Utils::FileSystem::exists(path);

	//if it exists as a resource file, return true
	if(getResourcePath(path) != path)
		return true;
		
	return Utils::FileSystem::exists(Utils::FileSystem::getCanonicalPath(path));
}

void ResourceManager::unloadAll()
{
	auto iter = mReloadables.cbegin();
	while(iter != mReloadables.cend())
	{
		std::shared_ptr<ReloadableInfo> info = *iter;

		if (!info->data.expired())
		{
			if (!info->locked)
				info->reload = info->data.lock()->unload();
			else
				info->locked = false;

			iter++;
		}
		else
			iter = mReloadables.erase(iter);	
	}
}

void ResourceManager::reloadAll()
{
	auto iter = mReloadables.cbegin();
	while(iter != mReloadables.cend())
	{
		std::shared_ptr<ReloadableInfo> info = *iter;

		if(!info->data.expired())
		{
			if (info->reload)
			{
				info->data.lock()->reload();
				info->reload = false;
			}

			iter++;
		}
		else
			iter = mReloadables.erase(iter);		
	}
}

void ResourceManager::addReloadable(std::weak_ptr<IReloadable> reloadable)
{
	std::shared_ptr<ReloadableInfo> info = std::make_shared<ReloadableInfo>();
	info->data = reloadable;
	info->reload = false;
	info->locked = false;
	mReloadables.push_back(info);
}

void ResourceManager::removeReloadable(std::weak_ptr<IReloadable> reloadable)
{
	auto iter = mReloadables.cbegin();
	while (iter != mReloadables.cend())
	{
		std::shared_ptr<ReloadableInfo> info = *iter;

		if (!info->data.expired())
		{
			if (info->data.lock() == reloadable.lock())
			{
				info->locked = true;
				break;
			}

			iter++;
		}
		else
			iter = mReloadables.erase(iter);
	}
}