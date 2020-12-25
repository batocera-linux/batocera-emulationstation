#include "components/ImageComponent.h"
#include <random>
#include <vector>
#include <map>

class SystemData;

class SystemRandomPlaylist : public IPlaylist
{
public:
	enum PlaylistType
	{
		IMAGE,
		THUMBNAIL,
		MARQUEE,
		FANART,
		TITLESHOT,
		VIDEO
	};

	SystemRandomPlaylist(SystemData* system, PlaylistType type);
	std::string getNextItem() override;

	static void resetCache();

private:
	SystemData*		mSystem;
	bool			mFirstRun;
	PlaylistType	mType;

	std::vector<std::string> mPaths;

	static std::map<std::string, std::vector<std::string>> mFileCache;

	//std::random_device	mRandomDevice;
	std::mt19937		mMt19937;
	std::uniform_int_distribution<int> mUniformDistribution;
};

class M3uPlaylist : public IPlaylist
{
public:
	M3uPlaylist(const std::string fileName);
	std::string getNextItem() override;
	int getDelay() override;
	bool getRotateOnShow() override;

private:
	std::vector<std::string> mPaths;

	int  mPreviousIndex;
	
	bool mUseRandom;
	bool mRotateOnShow;
	int  mDelay;

	//std::random_device	mRandomDevice;
	std::mt19937		mMt19937;
	std::uniform_int_distribution<int> mUniformDistribution;
};
