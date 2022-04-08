#include "AnimatedGifPlaylist.h"
#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"

AnimatedGifPlaylist::AnimatedGifPlaylist(const std::string& fileName, int totalFrames, int delay)
{
	mDelay = delay;
	mTotalFrames = totalFrames;
	mFileName = fileName;

	mCurrentIndex = 0;
}

std::string AnimatedGifPlaylist::getNextItem()
{
	auto index = mCurrentIndex;

	mCurrentIndex++;
	if (mCurrentIndex >= mTotalFrames)
		mCurrentIndex = 0;

	return mFileName + "," + std::to_string(index);
}

int AnimatedGifPlaylist::getDelay()
{
	return mDelay;
}

bool AnimatedGifPlaylist::getRotateOnShow()
{ 
	return false; 
}
