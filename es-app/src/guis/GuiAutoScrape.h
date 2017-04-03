#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/BusyComponent.h"


#include <boost/thread.hpp>

class GuiAutoScrape : public GuiComponent {
public:
    GuiAutoScrape(Window *window);

    virtual ~GuiAutoScrape();

    void render(const Eigen::Affine3f &parentTrans) override;

    bool input(InputConfig *config, Input input) override;

    std::vector<HelpPrompt> getHelpPrompts() override;

    void update(int deltaTime) override;

private:
    BusyComponent mBusyAnim;
    bool mLoading;
    int mState;
    std::pair<std::string, int> mResult;

    boost::thread *mHandle;

    void onAutoScrapeError(std::pair<std::string, int>);

    void onAutoScrapeOk();

    void threadAutoScrape();
};
