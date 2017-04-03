#include "guis/GuiAutoScrape.h"
#include "guis/GuiMsgBox.h"

#include "Window.h"
#include <boost/thread.hpp>
#include <string>
#include "Log.h"
#include "Settings.h"
#include "RecalboxSystem.h"
#include "Locale.h"

GuiAutoScrape::GuiAutoScrape(Window* window) : GuiComponent(window), mBusyAnim(window)
{
	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
        mLoading = true;
	mState = 1;
        mBusyAnim.setSize(mSize);
}

GuiAutoScrape::~GuiAutoScrape()
{
}

bool GuiAutoScrape::input(InputConfig* config, Input input)
{
        return false;
}

std::vector<HelpPrompt> GuiAutoScrape::getHelpPrompts()
{
	return std::vector<HelpPrompt>();
}

void GuiAutoScrape::render(const Eigen::Affine3f& parentTrans)
{
        Eigen::Affine3f trans = parentTrans * getTransform();

        renderChildren(trans);

        Renderer::setMatrix(trans);
        Renderer::drawRect(0.f, 0.f, mSize.x(), mSize.y(), 0x00000011);

        if(mLoading)
        mBusyAnim.render(trans);

}

void GuiAutoScrape::update(int deltaTime) {
        GuiComponent::update(deltaTime);
        mBusyAnim.update(deltaTime);
        
        Window* window = mWindow;

        if(mState == 1){
	  mLoading = true;
	  mHandle = new boost::thread(boost::bind(&GuiAutoScrape::threadAutoScrape, this));
	  mState = 0;
        }
	
        if(mState == 4){
            window->pushGui(
			    new GuiMsgBox(window, _("FINNISHED"), _("OK"),
                [this] {
					    mState = -1;
                })
            );
            mState = 0;
        }
	
        if(mState == 5){
            window->pushGui(
                    new GuiMsgBox(window, mResult.first, _("OK"),
                                  [this] {
                                      mState = -1;
                                  }
                    )
            );
            mState = 0;
        }
        if(mState == -1){
            delete this;
        }
}

void GuiAutoScrape::threadAutoScrape() 
{
  std::pair<std::string,int> scrapeStatus = RecalboxSystem::getInstance()->scrape(&mBusyAnim);
  if(scrapeStatus.second == 0){
    this->onAutoScrapeOk();
  }else {
    this->onAutoScrapeError(scrapeStatus);
  }
}

void GuiAutoScrape::onAutoScrapeError(std::pair<std::string, int> result)
{
    mLoading = false;
    mState = 5;
    mResult = result;
    mResult.first = _("AN ERROR OCCURED") + std::string(": ") + mResult.first;
}

void GuiAutoScrape::onAutoScrapeOk()
{
    mLoading = false;
    mState = 4;
}
