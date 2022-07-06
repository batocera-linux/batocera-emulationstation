#pragma once
#ifndef ES_CORE_UTILS_STRINGLISTLOCK_H
#define ES_CORE_UTILS_STRINGLISTLOCK_H

#include <string>
#include <cstring>
#include <map>
#include <mutex>

namespace Utils
{
	typedef std::map<std::string, std::pair<int, std::mutex*>> StringListLockType;

	class StringListLock
	{
	public:
		StringListLock(StringListLockType& set, const std::string& name);
		~StringListLock();

	private:
		static std::mutex mGlobalSetLock;

		std::string mName;
		std::mutex* mMutex;
		StringListLockType* mMap;
	};

}

#endif // ES_CORE_UTILS_STRINGLISTLOCK_H
