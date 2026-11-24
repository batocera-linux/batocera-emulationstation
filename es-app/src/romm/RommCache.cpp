#include "romm/RommCache.h"
#include "romm/RommCatalog.h"
#include "romm/RommConfig.h"
#include "romm/RommMedia.h"

#include "Log.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <unordered_set>
#include <vector>
#include <string>

namespace Romm
{
	static void removeCounted(const std::string& path, CacheCleanupResult& result)
	{
		unsigned long long size = Utils::FileSystem::getFileSize(path);
		if (Utils::FileSystem::removeFile(path))
		{
			result.files++;
			result.bytes += size;
		}
	}

	CacheCleanupResult clearMediaCache(const std::vector<Media>& types)
	{
		CacheCleanupResult result;

		RommConfig config = RommConfig::load();
		if (!config.isPaired())
			return result;

		for (auto type : types)
		{
			std::string dir = getMediaCacheDir(config.baseUrl(), type);
			if (!Utils::FileSystem::isDirectory(dir))
				continue;

			for (auto file : Utils::FileSystem::getDirContent(dir, false, false))
				if (!Utils::FileSystem::isDirectory(file))
					removeCounted(file, result);
		}

		Utils::FileSystem::FileSystemCache::reset();
		LOG(LogInfo) << "RomM: media cache cleared, " << result.files << " files";
		return result;
	}

	CacheCleanupResult cleanupCache()
	{
		CacheCleanupResult result;

		RommConfig config = RommConfig::load();
		std::string root = getCacheRoot();
		if (!Utils::FileSystem::isDirectory(root))
			return result;

		std::string current = config.isPaired() ? getServerCacheDir(config.baseUrl()) : "";

		// Caches of other servers (or of a server this device is no longer paired with)
		for (auto dir : Utils::FileSystem::getDirContent(root, false, false))
		{
			if (!Utils::FileSystem::isDirectory(dir) || Utils::FileSystem::getGenericPath(dir) == Utils::FileSystem::getGenericPath(current))
				continue;

			for (auto file : Utils::FileSystem::getDirContent(dir, true, false))
				if (!Utils::FileSystem::isDirectory(file))
					removeCounted(file, result);

			Utils::FileSystem::deleteDirectoryFiles(dir, true);
		}

		if (current.empty() || !Utils::FileSystem::isDirectory(current))
			return result;

		// Leftover temporary files (interrupted downloads or writes)
		for (auto file : Utils::FileSystem::getDirContent(current, true, false))
			if (Utils::String::toLower(Utils::FileSystem::getExtension(file)) == ".tmp" && !Utils::FileSystem::isDirectory(file))
				removeCounted(file, result);

		// Media still useful: selected type, game in the library, visible system, not on the device
		std::unordered_set<std::string> wanted[(size_t)Media::Count];

		std::string libraryDir = getLibraryDir(config.baseUrl());
		if (Utils::FileSystem::isDirectory(libraryDir))
		{
			for (auto file : Utils::FileSystem::getDirContent(libraryDir, false, false))
			{
				if (Utils::String::toLower(Utils::FileSystem::getExtension(file)) != ".xml")
					continue;

				std::vector<CatalogRecord> records;
				readSnapshotFile(file, records);

				for (auto& r : records)
				{
					if (config.hiddenSystems.find(r.system) != config.hiddenSystems.cend())
						continue;

					std::string systemPath = RommCatalog::getSystemPath(r.system);
					if (!systemPath.empty() && Utils::FileSystem::exists(systemPath + "/" + r.fileName, false))
						continue; // on the device: its media are next to the rom

					for (auto& m : allMedia())
						if (config.fetchesFromLibrary(m.type) && !r.mediaFile[mediaIndex(m.type)].empty())
							wanted[mediaIndex(m.type)].insert(r.mediaFile[mediaIndex(m.type)]);
				}
			}
		}

		for (auto& m : allMedia())
		{
			std::string dir = getMediaCacheDir(config.baseUrl(), m.type);
			if (!Utils::FileSystem::isDirectory(dir))
				continue;

			for (auto file : Utils::FileSystem::getDirContent(dir, false, false))
				if (wanted[mediaIndex(m.type)].find(Utils::FileSystem::getFileName(file)) == wanted[mediaIndex(m.type)].cend())
					removeCounted(file, result);
		}

		Utils::FileSystem::FileSystemCache::reset();
		LOG(LogInfo) << "RomM: cache clean up, " << result.files << " files, " << (result.bytes / 1024) << " KB freed";
		return result;
	}
}
