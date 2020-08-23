#include "guis/GuiUpdate.h"
#include "guis/GuiMsgBox.h"
#include "Window.h"
#include <string>
#include "Log.h"
#include "Settings.h"
#include "ApiSystem.h"
#include "platform.h"
#include "LocaleES.h"
#include "components/AsyncNotificationComponent.h"

GuiUpdateState::State GuiUpdate::state = GuiUpdateState::State::NO_UPDATE;

class ThreadedUpdater
{
public:
	ThreadedUpdater(Window* window) : mWindow(window)
	{
		GuiUpdate::state = GuiUpdateState::State::UPDATER_RUNNING;

		mWndNotification = mWindow->createAsyncNotificationComponent();
#ifdef _ENABLEEMUELEC
		mWndNotification->updateTitle(_U("\uF019 ") + _("UPDATING EMUELEC"));
#else
		mWndNotification->updateTitle(_U("\uF019 ") + _("UPDATING BATOCERA"));
#endif
		mHandle = new std::thread(&ThreadedUpdater::threadUpdate, this);
	}

	~ThreadedUpdater()
	{
		mWndNotification->close();
		mWndNotification = nullptr;
	}

	void threadUpdate()
	{
		std::pair<std::string, int> updateStatus = ApiSystem::getInstance()->updateSystem([this](const std::string info)
		{
			auto pos = info.find(">>>");
			if (pos != std::string::npos)
			{
				std::string percent(info.substr(pos));		
				percent = Utils::String::replace(percent, ">", "");
				percent = Utils::String::replace(percent, "%", "");
				percent = Utils::String::replace(percent, " ", "");

				int value = atoi(percent.c_str());

				std::string text(info.substr(0, pos));
				text = Utils::String::trim(text);

				mWndNotification->updatePercent(value);
				mWndNotification->updateText(text);
			}
			else
			{
				mWndNotification->updatePercent(-1);
				mWndNotification->updateText(info);
			}
		});

		if (updateStatus.second == 0)
		{
			GuiUpdate::state = GuiUpdateState::State::UPDATE_READY;

			mWndNotification->updateTitle(_U("\uF019 ") + _("UPDATE IS READY"));
			mWndNotification->updateText(_("REBOOT SYSTEM TO APPLY"));

			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::hours(12));
		}
		else
		{
			GuiUpdate::state = GuiUpdateState::State::NO_UPDATE;

			std::string error = _("AN ERROR OCCURED") + std::string(": ") + updateStatus.first;
			mWindow->displayNotificationMessage(error);
		}

		delete this;
	}

private:
	std::thread*				mHandle;
	AsyncNotificationComponent* mWndNotification;
	Window*						mWindow;
};


GuiUpdate::GuiUpdate(Window* window) : GuiComponent(window), mBusyAnim(window)
{
	LOG(LogInfo) << "Starting GuiUpdate";

	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	mState = 0;	
    mLoading = true;
    mPingHandle = new std::thread(&GuiUpdate::threadPing, this);
    mBusyAnim.setSize(mSize);
}

GuiUpdate::~GuiUpdate()
{	
	mPingHandle->join();
	delete mPingHandle;
}

void GuiUpdate::threadPing()
{	
	if (ApiSystem::getInstance()->ping())
	{
		std::vector<std::string> msgtbl;
		if (ApiSystem::getInstance()->canUpdate(msgtbl))
			onUpdateAvailable();
		else
			onNoUpdateAvailable();
	}
	else
		onPingError();
}

void GuiUpdate::onUpdateAvailable()
{
	mLoading = false;
	LOG(LogInfo) << "GuiUpdate : Update available" << "\n";
	mState = 1;
}

void GuiUpdate::onNoUpdateAvailable()
{
	mLoading = false;
	LOG(LogInfo) << "GuiUpdate : No update available" << "\n";
	mState = 6;
}

void GuiUpdate::onPingError()
{
	LOG(LogError) << "GuiUpdate : Ping failed" << "\n";

	mLoading = false;
	mState = 3;
}

void GuiUpdate::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mLoading)
		mBusyAnim.update(deltaTime);

	Window* window = mWindow;

	switch (mState)
	{
		case 1:
		
			mState = 0;
			window->pushGui(new GuiMsgBox(window, _("REALLY UPDATE?"), _("YES"), [this] 
			{
				mState = 2;
				mLoading = true;

				mState = -1;
				new ThreadedUpdater(mWindow);

			}, _("NO"), [this]  { mState = -1; }));		
		
			break;

		case 3:
		
			mState = 0;
			window->pushGui(new GuiMsgBox(window, _("NETWORK CONNECTION NEEDED"), _("OK"), [this] 
			{
				mState = -1;
			}));			
		
			break;

		case 6:

			mState = 0;
			window->pushGui(new GuiMsgBox(window, _("NO UPDATE AVAILABLE"), _("OK"), [this] 
			{
				mState = -1;
			}));

			break;

		case -1:
			delete this;
			break;
	}
}

void GuiUpdate::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();

	renderChildren(trans);

	Renderer::setMatrix(trans);
	Renderer::drawRect(0.f, 0.f, mSize.x(), mSize.y(), 0x00000011);

	if (mLoading)
		mBusyAnim.render(trans);
}

bool GuiUpdate::input(InputConfig* config, Input input)
{
	return false;
}

std::vector<HelpPrompt> GuiUpdate::getHelpPrompts()
{
	return std::vector<HelpPrompt>();
}

