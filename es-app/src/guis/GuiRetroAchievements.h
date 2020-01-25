#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/BusyComponent.h"


#include <thread>

class GuiRetroAchievements : public GuiComponent {
public:
    GuiRetroAchievements(Window *window);

    virtual ~GuiRetroAchievements();

    void render(const Transform4x4f &parentTrans) override;

    bool input(InputConfig *config, Input input) override;

    std::vector<HelpPrompt> getHelpPrompts() override;
    
    void update(int deltaTime) override;

private:
    BusyComponent mBusyAnim;
    bool mLoading;
    int mState;

    std::thread *mHandle;

    void threadDisplayRetroAchievements();
};
