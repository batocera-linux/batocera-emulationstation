#include "ResourceManager.h"

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <fstream>
#include <algorithm>
#include "Log.h"
#include "Settings.h"
#include "Paths.h"

auto array_deleter = [](unsigned char* p) { delete[] p; };
auto nop_deleter = [](unsigned char* /*p*/) { };

std::shared_ptr<ResourceManager> ResourceManager::sInstance = nullptr;

ResourceManager::ResourceManager()
{
}

std::shared_ptr<ResourceManager>& ResourceManager::getInstance()
{
	if(!sInstance)
		sInstance = std::shared_ptr<ResourceManager>(new ResourceManager());

	return sInstance;
}

std::vector<std::string> ResourceManager::getResourcePaths() const
{
	std::vector<std::string> paths;

	// check if theme overrides default resources
	if (!Paths::getUserThemesPath().empty())
	{
		std::string themePath = Paths::getUserThemesPath() + "/" + Settings::getInstance()->getString("ThemeSet") + "/resources";
		if (Utils::FileSystem::isDirectory(themePath))
			paths.push_back(themePath);
	}

	if (!Paths::getThemesPath().empty())
	{
		std::string roThemePath = Paths::getThemesPath() + "/" + Settings::getInstance()->getString("ThemeSet") + "/resources";
		if (Utils::FileSystem::isDirectory(roThemePath))
			paths.push_back(roThemePath);
	}

	// check in homepath
	paths.push_back(Paths::getUserEmulationStationPath() + "/resources");
	
	// check in emulationStation path
	paths.push_back(Paths::getEmulationStationPath() + "/resources");
		
	// check in Exe path
	if (Paths::getEmulationStationPath() != Paths::getExePath())
		paths.push_back(Paths::getExePath() + "/resources");

	// check in cwd
	auto cwd = Utils::FileSystem::getCWDPath() + "/resources";	
	if (std::find(paths.cbegin(), paths.cend(), cwd) == paths.cend())
		paths.push_back(cwd); 

	return paths;
}

std::string ResourceManager::getResourcePath(const std::string& path) const
{
	// check if this is a resource file
	if (path.size() < 2 || path[0] != ':' || path[1] != '/')
		return path;

	for (auto testPath : getResourcePaths())
	{
		std::string test = testPath + "/" + &path[2];
		if (Utils::FileSystem::exists(test))
			return test;
	}

#if WIN32
	if (Utils::String::startsWith(path, ":/locale/") || Utils::String::startsWith(path, ":/es_features.locale/"))
	{
		std::string test = Utils::FileSystem::getCanonicalPath(Paths::getEmulationStationPath() + "/" + &path[2]);
		if (Utils::FileSystem::exists(test))
			return test;

		test = Utils::FileSystem::getCanonicalPath(Paths::getUserEmulationStationPath() + "/" + &path[2]);
		if (Utils::FileSystem::exists(test))
			return test;
	}
#endif

	LOG(LogError) << "Resource path not found: " << path;

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
#if defined(_WIN32)
	std::ifstream stream(Utils::String::convertToWideString(path), std::ios::binary);
#else
	std::ifstream stream(path, std::ios::binary);
#endif

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