#include "components/ImageComponent.h"

class AnimatedGifPlaylist : public IPlaylist
{
public:
	AnimatedGifPlaylist(const std::string& fileName, int totalFrames, int delay);

	std::string getNextItem() override;
	int getDelay() override;
	bool getRotateOnShow() override;

private:
	std::string mFileName;
	int  mTotalFrames;
	int  mDelay;

	int  mCurrentIndex;
};
