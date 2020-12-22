#include "Playlists.h"
#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"
#include "SystemData.h"
#include "FileData.h"

///////////// SystemRandomPlaylist ///////////// 

static std::random_device mRandomDevice;
std::map<std::string, std::vector<std::string>> SystemRandomPlaylist::mFileCache;

SystemRandomPlaylist::SystemRandomPlaylist(SystemData* system, PlaylistType type) : mMt19937(mRandomDevice())
{
	mFirstRun = true;
	mSystem = system;
	mType = type;
}

void SystemRandomPlaylist::resetCache()
{
	mFileCache.clear();
}

std::string SystemRandomPlaylist::getNextItem()
{
	if (mFirstRun)
	{		
		auto it = mFileCache.find(mSystem->getName() + "." + std::to_string(mType));
		if (it != mFileCache.cend())
			mPaths = it->second;
		else
		{
			std::vector<FileData*> files = mSystem->getRootFolder()->getFilesRecursive(GAME, false);

			for (auto file : files)
			{
				std::string path;

				switch (mType)
				{
				case IMAGE:
					path = file->getImagePath();
					break;
				case THUMBNAIL:
					path = file->getThumbnailPath();
					break;
				case MARQUEE:
					path = file->getMarqueePath();
					break;
				case FANART:
					path = file->getMetadata(MetaDataId::FanArt);
					break;
				case TITLESHOT:
					path = file->getMetadata(MetaDataId::TitleShot);
					break;
				case VIDEO:
					path = file->getVideoPath();
					break;
				}

				if (!path.empty())
					mPaths.push_back(path);
			}

			if (mType == FANART && mPaths.size() == 0)
			{
				for (auto file : files)
				{
					std::string path = file->getThumbnailPath();
					if (!path.empty())
						mPaths.push_back(path);
				}
			}

			mFileCache[mSystem->getName() + "." + std::to_string(mType)] = mPaths;
		}

		if (mPaths.size() > 0)
			mUniformDistribution = std::uniform_int_distribution<int>(0, mPaths.size() - 1);

		mFirstRun = false;
	}

	if (mPaths.size() > 0)
	{
		int idx = mUniformDistribution(mMt19937);
		if (idx >= 0 && idx < mPaths.size() && Utils::FileSystem::exists(mPaths[idx]))
			return mPaths[idx];

		// File not found ? Try the next file...
		int stopidx = idx;

		idx++;
		if (idx >= mPaths.size())
			idx = 0;

		while (idx != stopidx && idx < mPaths.size() && !Utils::FileSystem::exists(mPaths[idx]))
		{
			idx++;
			if (idx >= mPaths.size())
				idx = 0;
		}

		if (idx >= 0 && idx < mPaths.size() && Utils::FileSystem::exists(mPaths[idx]))
			return mPaths[idx];
	}

	return "";
}

///////////// M3uPlaylist ///////////// 

M3uPlaylist::M3uPlaylist(const std::string fileName) : mMt19937(mRandomDevice())
{
	mUseRandom = true;
	mDelay = 10000;
	mRotateOnShow = false;

	if (!Utils::FileSystem::exists(fileName))
		return;

	std::string directory = Utils::FileSystem::getParent(fileName);

	for (auto file : Utils::String::splitAny(Utils::FileSystem::readAllText(fileName), "\r\n"))
	{
		if (file.empty())
			continue;
		
		if (file[0] == '#')
		{
			if (Utils::String::startsWith(file, "##"))
				continue;

			if (file == "#RANDOM")
				mUseRandom = true;
			else if (file == "#RESETONSHOW")
				mRotateOnShow = true;
			else if (Utils::String::startsWith(file, "#EXT-X-TARGETDURATION:"))
			{
				int delay = atoi(file.substr(22).c_str());
				if (delay > 0)
					mDelay = delay;
			}
			
			continue;
		}

		if (file[0] == ';' || Utils::String::startsWith(file, "//"))
			continue;

		if (file[0] != '.' && file[0] != '/' && file[0] != '\\' && file.find(':') == std::string::npos)
			file = "./" + file;

		std::string path = Utils::FileSystem::resolveRelativePath(file, directory, false);
		if (Utils::FileSystem::exists(path))
			mPaths.push_back(path);
	}

	mPreviousIndex = -1;
	mUniformDistribution = std::uniform_int_distribution<int>(0, mPaths.size() - 1);
	mUniformDistribution(mMt19937);
}

std::string M3uPlaylist::getNextItem()
{
	int idx = mPreviousIndex + 1;
	
	if (mUseRandom)
	{
		idx = mUniformDistribution(mMt19937);
		if (idx == mPreviousIndex)
			idx = mUniformDistribution(mMt19937);
	}
	else if (idx >= mPaths.size())
		idx = 0;

	mPreviousIndex = idx;

	if (idx >= 0 && idx < mPaths.size())
		return mPaths[idx];

	return "";
}

int M3uPlaylist::getDelay()
{
	return mDelay;
}

bool M3uPlaylist::getRotateOnShow()
{
	return mRotateOnShow;
}
