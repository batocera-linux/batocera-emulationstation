#include "guis/GuiBackup.h"
#include "guis/GuiMsgBox.h"
#include "Window.h"
#include <boost/thread.hpp>
#include <string>
#include "Log.h"
#include "Settings.h"
#include "ApiSystem.h"
#include "LocaleES.h"
#include "GuiBezelInstall.h"
#include "views/ViewController.h"

// Batcera - theBezelProject Install
GuiBezelInstall::GuiBezelInstall(Window* window, char *bezel) : GuiComponent(window), mBusyAnim(window)
{
	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
        mLoading = true;
	mState = 1;
        mBusyAnim.setSize(mSize);
	mBezelName = bezel; //FIXME
}

GuiBezelInstall::~GuiBezelInstall()
{
}

bool GuiBezelInstall::input(InputConfig* config, Input input)
{
        return false;
}

std::vector<HelpPrompt> GuiBezelInstall::getHelpPrompts()
{
	return std::vector<HelpPrompt>();
}

void GuiBezelInstall::render(const Transform4x4f& parentTrans)
{
        Transform4x4f trans = parentTrans * getTransform();

        renderChildren(trans);

        Renderer::setMatrix(trans);
        Renderer::drawRect(0.f, 0.f, mSize.x(), mSize.y(), 0x00000011);

        if(mLoading)
        mBusyAnim.render(trans);

}

void GuiBezelInstall::update(int deltaTime) {
        GuiComponent::update(deltaTime);
        mBusyAnim.update(deltaTime);
       
        Window* window = mWindow;
        if(mState == 1){
	  mLoading = true;
	  mHandle = new boost::thread(boost::bind(&GuiBezelInstall::threadBezel, this));
	  mState = 0;
        }

        if(mState == 2){
	  window->pushGui(
			  new GuiMsgBox(window, _("FINISHED") + "\n" + _("BEZELS INSTALLED SUCCESSFULLY"), _("OK"),
					[this] {
					  mState = -1;
					}
					)
			  );
	  // In case you have no active theme
	  // ViewController::get()->reloadAll();
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

void GuiBezelInstall::threadBezel() 
{
    // Batocera script will be invoked there 
    // FIXME
    std::pair<std::string,int> updateStatus = ApiSystem::getInstance()->installBatoceraBezel(&mBusyAnim, mBezelName);
    
    if(updateStatus.second == 0){
        this->onInstallOk();
    }else {
        this->onInstallError(updateStatus);
    } 
    
}

void GuiBezelInstall::onInstallError(std::pair<std::string, int> result)
{
    mLoading = false;
    mState = 3;
    mResult = result;
    mResult.first = _("AN ERROR OCCURED");
}

void GuiBezelInstall::onInstallOk()
{
    mLoading = false;
    mState = 2;
}


// And now the UnInstall for this
GuiBezelUninstall::GuiBezelUninstall(Window* window, char *bezel) : GuiComponent(window), mBusyAnim(window)
{
	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
        mLoading = true;
	mState = 1;
        mBusyAnim.setSize(mSize);
	mBezelName = bezel; //FIXME
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
	  mHandle = new boost::thread(boost::bind(&GuiBezelUninstall::threadBezel, this));
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
	  // In case you have no active theme
	  // ViewController::get()->reloadAll();
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
    // Batocera script will be invoked there 
    // FIXME
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


