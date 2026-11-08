#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/BusyComponent.h"
#include "components/AsyncNotificationComponent.h"

#include <thread>

namespace GuiUpdateState
{
	enum State
	{
		NO_UPDATE,
		UPDATER_RUNNING,
		UPDATE_READY
	};
}

class GuiUpdate : public GuiComponent 
{
public:
	static GuiUpdateState::State state;

    GuiUpdate(Window *window);
    virtual ~GuiUpdate();

    void render(const Transform4x4f &parentTrans) override;
    bool input(InputConfig *config, Input input) override;
    std::vector<HelpPrompt> getHelpPrompts() override;
    void update(int deltaTime) override;

private:
    BusyComponent mBusyAnim;
    bool mLoading;
    int mState;
	std::string mUpdateVersion;

    std::pair<std::string, int> mResult;

	std::thread* mPingHandle;

    void threadPing();
    void onUpdateAvailable();
    void onNoUpdateAvailable();
    void onPingError();
};

class ThreadedUpdater
{
public:
  ThreadedUpdater(Window* window, const std::string source = "WWW");
  ~ThreadedUpdater();

  void threadUpdate(const std::string source);

private:
  std::thread*				mHandle;
  AsyncNotificationComponent* mWndNotification;
  Window*						mWindow;

  void threadUpdate_network();
  void threadUpdate_fromlocalmedia();
  void threadUpdate_torrent();
};
