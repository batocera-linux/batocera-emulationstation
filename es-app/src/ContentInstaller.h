#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <list>
#include <utility>

class Window;
class AsyncNotificationComponent;


class ContentInstaller
{
public:
	enum ContentType : int
	{
		CONTENT_THEME_INSTALL = 0,
		CONTENT_THEME_UNINSTALL = 1,
		CONTENT_BEZEL_INSTALL = 2,
		CONTENT_BEZEL_UNINSTALL = 3,
		CONTENT_STORE_INSTALL = 4,
		CONTENT_STORE_UNINSTALL = 5,
	};

	static void Enqueue(Window* window, ContentType type, const std::string contentName);
	static bool IsInQueue(ContentType type, const std::string contentName);

private: // Methods
	ContentInstaller(Window* window);
	~ContentInstaller();

	void updateNotificationComponentTitle(bool incQueueSize);
	void updateNotificationComponentContent(const std::string info);

	void threadUpdate();

private:
	AsyncNotificationComponent* mWndNotification;
	Window*						mWindow;
	std::thread*				mHandle;

	int							mCurrent;
	int							mQueueSize;

private:
	static ContentInstaller*					  mInstance;
	static std::mutex							  mLock;
	static std::list<std::pair<int, std::string>> mQueue;
	static std::list<std::pair<int, std::string>> mProcessingQueue;
};
