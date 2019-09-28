#include "guis/GuiBackup.h"
#include "guis/GuiMsgBox.h"
#include "Window.h"
#include <string>
#include "Log.h"
#include "Settings.h"
#include "ApiSystem.h"
#include "LocaleES.h"
#include "GuiBezelUnInstall.h"

// And now the UnInstall for this
GuiBezelUninstall::GuiBezelUninstall(Window* window, char *bezel) : GuiComponent(window), mBusyAnim(window)
{
	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
        mLoading = true;
	mState = 1;
        mBusyAnim.setSize(mSize);
	mBezelName = bezel;
}

GuiBezelUninstall::~GuiBezelUninstall()
{
}

bool GuiBezelUninstall::input(InputConfig* config, Input input)
{
        return false;
}

std::vector<HelpPrompt> GuiBezelUninstall::getHelpPrompts()
{
	return std::vector<HelpPrompt>();
}

void GuiBezelUninstall::render(const Transform4x4f& parentTrans)
{
        Transform4x4f trans = parentTrans * getTransform();

        renderChildren(trans);

        Renderer::setMatrix(trans);
        Renderer::drawRect(0.f, 0.f, mSize.x(), mSize.y(), 0x00000011);

        if(mLoading)
        mBusyAnim.render(trans);

}

void GuiBezelUninstall::update(int deltaTime) {
        GuiComponent::update(deltaTime);
        mBusyAnim.update(deltaTime);
       
        Window* window = mWindow;
        if(mState == 1){
	  mLoading = true;
	  mHandle = new std::thread(&GuiBezelUninstall::threadBezel, this);
	  mState = 0;
        }

        if(mState == 2){
	  window->pushGui(
			  new GuiMsgBox(window, _("FINISHED") + "\n" + _("BEZELS DELETED SUCCESSFULLY"), _("OK"),
					[this] {
					  mState = -1;
					}
					)
			  );
	  mState = 0;
        }
        if(mState == 3){
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

void GuiBezelUninstall::threadBezel() 
{
    std::pair<std::string,int> updateStatus = ApiSystem::getInstance()->uninstallBatoceraBezel(&mBusyAnim, mBezelName);
    
    if(updateStatus.second == 0){
        this->onInstallOk();
    }else {
        this->onInstallError(updateStatus);
    } 
    
}

void GuiBezelUninstall::onInstallError(std::pair<std::string, int> result)
{
    mLoading = false;
    mState = 3;
    mResult = result;
    mResult.first = _("AN ERROR OCCURED");
}

void GuiBezelUninstall::onInstallOk()
{
    mLoading = false;
    mState = 2;
}


