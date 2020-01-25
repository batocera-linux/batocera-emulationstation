#include "guis/GuiInstall.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiRetroAchievements.h"
#include "guis/GuiThemeInstallStart.h"
#include "guis/GuiSettings.h"

#include "Window.h"
#include <string>
#include "Log.h"
#include "Settings.h"
#include "ApiSystem.h"
#include "LocaleES.h"

GuiRetroAchievements::GuiRetroAchievements(Window* window) : GuiComponent(window), mBusyAnim(window)
{
	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
        mLoading = true;
	mState = 1;
	mBusyAnim.setText(_("PLEASE WAIT"));
        mBusyAnim.setSize(mSize);
}

GuiRetroAchievements::~GuiRetroAchievements()
{
}

bool GuiRetroAchievements::input(InputConfig* config, Input input)
{
        return false;
}

std::vector<HelpPrompt> GuiRetroAchievements::getHelpPrompts()
{
	return std::vector<HelpPrompt>();
}

void GuiRetroAchievements::render(const Transform4x4f& parentTrans)
{
        Transform4x4f trans = parentTrans * getTransform();

        renderChildren(trans);

        Renderer::setMatrix(trans);
        Renderer::drawRect(0.f, 0.f, mSize.x(), mSize.y(), 0x00000011);

        if(mLoading)
          mBusyAnim.render(trans);

}

void GuiRetroAchievements::update(int deltaTime)
{
        GuiComponent::update(deltaTime);
        mBusyAnim.update(deltaTime);
        
        if(mState == 1){
	  mLoading = true;
	  mHandle = new std::thread(&GuiRetroAchievements::threadDisplayRetroAchievements, this);
	  mState = 0;
        }

        if(mState == -1){
	  delete this;
        }
}

void GuiRetroAchievements::threadDisplayRetroAchievements() 
{
	auto ra = ApiSystem::getInstance()->getRetroAchievements();

        Window *window = mWindow;

        auto s = new GuiSettings(mWindow, _("RETROACHIEVEMENTS").c_str());

        if (!ra.error.empty())
	{
                s->setSubTitle(ra.error);
	}
        else
        {
                if (!ra.userpic.empty() && Utils::FileSystem::exists(ra.userpic))
                {
                        auto image = std::make_shared<ImageComponent>(mWindow);
                        image->setImage(ra.userpic);
                        s->setTitleImage(image);
                }

                s->setSubTitle("Player " + ra.username + " (" + ra.totalpoints + " points) is " + ra.rank);

                for (auto game : ra.games)
                {               
                        ComponentListRow row;

                        auto itstring = std::make_shared<MultiLineMenuEntry>(window, game.name, game.achievements + " achievements");

			auto badge = std::make_shared<ImageComponent>(mWindow);

			if (game.badge.empty())
				badge->setImage(":/cartridge.svg");
			else
				badge->setImage(game.badge);

			badge->setSize(48, 48);
			row.addElement(badge,false);

			auto spacer = std::make_shared<GuiComponent>(mWindow);
			spacer->setSize(10, 0);
			row.addElement(spacer, false);

                        if (!game.points.empty())
                        {
                                std::string longmsg = game.name + "\n" + game.achievements + " achievements\n" + game.points + " points\nLast played : " + game.lastplayed;

                                row.makeAcceptInputHandler([this, longmsg] {
                                        mWindow->pushGui(new GuiMsgBox(mWindow, longmsg, _("OK")));
                                });
                        }

                        row.addElement(itstring, true);
                        s->addRow(row);
                        row.elements.clear();
                }

        }
	mWindow->pushGui(s);
	mState = -1;
}
