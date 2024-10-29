#include "ThreadedHasher.h"
#include "Window.h"
#include "FileData.h"
#include "components/AsyncNotificationComponent.h"
#include "guis/GuiMsgBox.h"
#include "Gamelist.h"
#include "RetroAchievements.h"
#include "SystemConf.h"
#include "SystemData.h"
#include "FileData.h"
#include "ApiSystem.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include <unordered_set>
#include <queue>

#include "LocaleES.h"

#define ICONINDEX _U("\uF002 ")

ThreadedHasher* ThreadedHasher::mInstance = nullptr;
bool ThreadedHasher::mPaused = false;

static std::mutex mLoaderLock;

ThreadedHasher::ThreadedHasher(Window* window, HasherType type, std::queue<FileData*> searchQueue, bool forceAllGames)
	: mWindow(window)
{
	mForce = forceAllGames;
	mExit = false;
	mType = type;

	mSearchQueue = searchQueue;
	mTotal = mSearchQueue.size();

	if ((mType & HASH_CHEEVOS_MD5) == HASH_CHEEVOS_MD5)
	{
		try
		{
			mCheevosHashes = RetroAchievements::getCheevosHashes();
			if (mCheevosHashes.size() == 0)
				while (!mSearchQueue.empty())
					mSearchQueue.pop();
		}
		catch (const std::exception& e)
		{
			mType = (HasherType)0;
			throw e;
		}
	}

	mWndNotification = mWindow->createAsyncNotificationComponent();

	if (mType == HASH_CHEEVOS_MD5)
		mWndNotification->updateTitle(ICONINDEX + _("SEARCHING RETROACHIEVEMENTS"));
	else 
		mWndNotification->updateTitle(ICONINDEX + _("SEARCHING NETPLAY GAMES"));

	int num_threads = std::thread::hardware_concurrency() / 2;
	if (num_threads == 0)
		num_threads = 1;

	mThreadCount = num_threads;
	for (size_t i = 0; i < num_threads; i++)
		mThreads.push_back(new std::thread(&ThreadedHasher::run, this));
}

ThreadedHasher::~ThreadedHasher()
{
	if ((mType & HASH_CHEEVOS_MD5) == HASH_CHEEVOS_MD5)
		mWindow->displayNotificationMessage(ICONINDEX + _("INDEXING COMPLETED") + std::string(". ") + _("UPDATE GAMELISTS TO APPLY CHANGES."));

	mWndNotification->close();
	mWndNotification = nullptr;

	ThreadedHasher::mInstance = nullptr;
}

std::string ThreadedHasher::formatGameName(FileData* game)
{
	return "[" + game->getSystemName() + "] " + game->getName();
}

void ThreadedHasher::updateUI(const std::string label)
{
	std::string idx = std::to_string(mTotal + 1 - mSearchQueue.size()) + "/" + std::to_string(mTotal);
	int percent = 100 - (mSearchQueue.size() * 100 / mTotal);
		
	mWndNotification->updateText(label);
	mWndNotification->updatePercent(percent);	
}

void ThreadedHasher::run()
{
	std::unique_lock<std::mutex> lock(mLoaderLock);

	bool cheevos = ((mType & HASH_CHEEVOS_MD5) == HASH_CHEEVOS_MD5);
	bool netplay = ((mType & HASH_NETPLAY_CRC) == HASH_NETPLAY_CRC);

	while (!mExit && !mSearchQueue.empty())
	{
		FileData* game = mSearchQueue.front();

		auto label = formatGameName(game);

		LOG(LogDebug) << "Hashing " << formatGameName(game);
		updateUI(label);

		mSearchQueue.pop();

		lock.unlock();

		if (mPaused)
		{
			while (!mExit && mPaused)
			{
				std::this_thread::yield();
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}
		}		

		if (netplay)
		{
			LOG(LogDebug) << "CheckCrc32 : " << label;
			game->checkCrc32(mForce);
		}

		if (cheevos)
		{
			LOG(LogDebug) << "CheckCheevosHash : " << label;
			game->checkCheevosHash(mForce);

			auto hash = Utils::String::toUpper(game->getMetadata(MetaDataId::CheevosHash));
			if (!hash.empty())
			{
				auto cheevos = mCheevosHashes.find(hash);
				if (cheevos != mCheevosHashes.cend())
					game->setMetadata(MetaDataId::CheevosId, cheevos->second);
				else
					game->setMetadata(MetaDataId::CheevosId, "");
			}

			LOG(LogDebug) << "CheckCheevosHash OK : " << label;;
		}		

		lock.lock();
	}

	mThreadCount--;

	if (mThreadCount == 0)
	{
		lock.unlock();
		delete this;
		ThreadedHasher::mInstance = nullptr;

	}
}

bool ThreadedHasher::checkCloseIfRunning(Window* window)
{
	if (ThreadedHasher::mInstance != nullptr)
	{
		window->pushGui(new GuiMsgBox(window, _("GAME HASHING IS RUNNING. DO YOU WANT TO STOP IT?"), _("YES"), []
		{
			ThreadedHasher::stop();
		}, _("NO"), nullptr));

		return false;
	}

	return true;
}

void ThreadedHasher::start(Window* window, HasherType type, bool forceAllGames, bool silent, std::set<std::string>* systems)
{
	if (ThreadedHasher::mInstance != nullptr)
	{
		if (silent)
			return;

		if (!checkCloseIfRunning(window))
			return;
	}
	
	std::queue<FileData*> searchQueue;
	
	for (auto sys : SystemData::sSystemVector)
	{
		if (sys->isGroupSystem() || sys->isCollection())
			continue;

		bool takeNetplay = ((type & HASH_NETPLAY_CRC) == HASH_NETPLAY_CRC) && sys->isNetplaySupported();
		bool takeCheevos = ((type & HASH_CHEEVOS_MD5) == HASH_CHEEVOS_MD5) && sys->isCheevosSupported();

		if (!takeNetplay && !takeCheevos)
			continue;

		if (systems != nullptr && systems->find(sys->getName()) == systems->cend())
			continue;
		
		if (!sys->isGameSystem() || sys->getRootFolder() == nullptr)
			continue;

		if (sys->isGroupChildSystem() ? sys->isHidden() : !sys->isVisible())
			continue;

		for (auto file : sys->getRootFolder()->getFilesRecursive(GAME))
		{
			bool netPlay = takeNetplay && (forceAllGames || file->getMetadata(MetaDataId::Crc32).empty());
			bool cheevos = takeCheevos && (forceAllGames || file->getMetadata(MetaDataId::CheevosHash).empty());

			if (cheevos)
			{
				std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(file->getPath()));
				
				if (ext == ".pbp" || ext == ".cso") // Currently unsupported formats
					cheevos = false;
			}

			if (netPlay || cheevos)
				searchQueue.push(file);
		}
	}

	if (searchQueue.size() == 0)
	{
		if (!silent)
			window->pushGui(new GuiMsgBox(window, _("NO GAMES FIT THAT CRITERIA.")));

		return;
	}

	try
	{
		ThreadedHasher::mInstance = new ThreadedHasher(window, type, searchQueue, forceAllGames);
	}
	catch (const std::exception& e)
	{
		LOG(LogError) << "Game Hash failed : " << e.what();

		if (!silent)
			window->pushGui(new GuiMsgBox(window, e.what()));
	}	
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

