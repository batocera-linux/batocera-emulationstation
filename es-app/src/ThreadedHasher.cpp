#include "ThreadedHasher.h"
#include "Window.h"
#include "FileData.h"
#include "components/AsyncNotificationComponent.h"
#include "LocaleES.h"
#include "guis/GuiMsgBox.h"
#include "Gamelist.h"

#include "SystemConf.h"
#include "SystemData.h"
#include "FileData.h"
#include <unordered_set>
#include <queue>
#include "ApiSystem.h"
#include "utils/StringUtil.h"

#define ICONINDEX _U("\uF1EC ")

ThreadedHasher* ThreadedHasher::mInstance = nullptr;
bool ThreadedHasher::mPaused = false;

ThreadedHasher::ThreadedHasher(Window* window, std::queue<FileData*> searchQueue)
	: mWindow(window)
{
	mExit = false;

	mSearchQueue = searchQueue;
	mTotal = mSearchQueue.size();

	mWndNotification = mWindow->createAsyncNotificationComponent();
	mWndNotification->updateTitle(ICONINDEX + _("HASHING GAMES"));

	mHandle = new std::thread(&ThreadedHasher::run, this);
}

ThreadedHasher::~ThreadedHasher()
{
	mWndNotification->close();
	mWndNotification = nullptr;

	ThreadedHasher::mInstance = nullptr;
}

std::string ThreadedHasher::formatGameName(FileData* game)
{
	return "[" + game->getSystemName() + "] " + game->getName();
}

void ThreadedHasher::hashFile(FileData* fileData)
{
	std::string idx = std::to_string(mTotal + 1 - mSearchQueue.size()) + "/" + std::to_string(mTotal);
	int percent = 100 - (mSearchQueue.size() * 100 / mTotal);
		
	mWndNotification->updateText(formatGameName(fileData));
	mWndNotification->updatePercent(percent);

	fileData->checkCrc32(true);
}

void ThreadedHasher::run()
{
	while (!mExit && !mSearchQueue.empty())
	{
		if (mPaused)
		{
			while (!mExit && mPaused)
			{
				std::this_thread::yield();
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}
		}

		hashFile(mSearchQueue.front());
		mSearchQueue.pop();
	}

	delete this;
	ThreadedHasher::mInstance = nullptr;
}

void ThreadedHasher::start(Window* window, bool forceAllGames, bool silent)
{
	if (ThreadedHasher::mInstance != nullptr)
	{
		if (silent)
			return;

		window->pushGui(new GuiMsgBox(window, _("GAME HASHING IS RUNNING. DO YOU WANT TO STOP IT ?"), _("YES"), []
		{
			ThreadedHasher::stop();
		}, _("NO"), nullptr));

		return;
	}
	
	std::queue<FileData*> searchQueue;
	
	for (auto sys : SystemData::sSystemVector)
	{
		if (!sys->isNetplaySupported())
			continue;

		for (auto file : sys->getRootFolder()->getFilesRecursive(GAME))
			if (forceAllGames || file->getMetadata(MetaDataId::Crc32).empty())
				searchQueue.push(file);
	}

	if (searchQueue.size() == 0)
	{
		if (!silent)
			window->pushGui(new GuiMsgBox(window, _("NO GAMES FIT THAT CRITERIA.")));

		return;
	}

	ThreadedHasher::mInstance = new ThreadedHasher(window, searchQueue);
}

void ThreadedHasher::stop()
{
	auto thread = ThreadedHasher::mInstance;
	if (thread == nullptr)
		return;

	try
	{
		thread->mExit = true;
	}
	catch (...) {}
}

