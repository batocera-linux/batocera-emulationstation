#include "utils/StringListLock.h"
#include <map>

namespace Utils
{
	std::mutex StringListLock::mGlobalSetLock;

	StringListLock::StringListLock(StringListLockType& set, const std::string& name)
	{
		mName = name;
		mMap = &set;

		// Locked block
		{
			std::unique_lock<std::mutex> lock(mGlobalSetLock);

			auto it = mMap->find(name);
			if (it == mMap->cend())
			{
				mMutex = new std::mutex();
				(*mMap)[name] = std::pair<int, std::mutex*>(1, mMutex);
			}
			else
			{
				it->second.first++;
				mMutex = it->second.second;
			}
		}


		if (mMutex != nullptr)
			mMutex->lock();
	}

	StringListLock::~StringListLock()
	{
		// Locked block
		{
			std::unique_lock<std::mutex> lock(mGlobalSetLock);

			auto it = mMap->find(mName);
			if (it != mMap->cend())
			{
				it->second.first--;
				if (it->second.first <= 0)
				{
					mMutex->unlock();
					delete mMutex;

					mMap->erase(it);
					return;
				}
			}
		}

		mMutex->unlock();
		mMutex = nullptr;

		mMap = nullptr;
	}
}