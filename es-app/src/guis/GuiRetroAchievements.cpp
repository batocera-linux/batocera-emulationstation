#include "guis/GuiInstall.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiRetroAchievements.h"
#include "guis/GuiThemeInstallStart.h"
#include "guis/GuiSettings.h"
#include "components/WebImageComponent.h"

#include "Window.h"
#include <string>
#include "Log.h"
#include "Settings.h"
#include "ApiSystem.h"
#include "LocaleES.h"
#include "GuiLoading.h"

#include "components/MultiLineMenuEntry.h"

#define WINDOW_WIDTH (float)Math::max((int)(Renderer::getScreenHeight() * 0.875f), (int)(Renderer::getScreenWidth() * 0.6f))
#define IMAGESIZE (Renderer::getScreenHeight() * (48.0 / 720.0))
#define IMAGESPACER (Renderer::getScreenHeight() * (10.0 / 720.0))

void GuiRetroAchievements::show(Window* window)
{
	window->pushGui(new GuiLoading<RetroAchievementInfo>(window, _("PLEASE WAIT"), 
		[window]
		{
			return ApiSystem::getInstance()->getRetroAchievements();		
		}, 
		[window](RetroAchievementInfo ra)
		{
			window->pushGui(new GuiRetroAchievements(window, ra));
		}));
}

GuiRetroAchievements::GuiRetroAchievements(Window* window, RetroAchievementInfo ra) : GuiSettings(window, _("RETROACHIEVEMENTS").c_str(), "", nullptr)
{
	// Required for WebImageComponent
	setUpdateType(ComponentListFlags::UPDATE_ALWAYS);

	if (!ra.error.empty())
	{
		setSubTitle(ra.error);
		return;
	}

	if (!ra.userpic.empty() && !Renderer::isSmallScreen())
	{
		auto image = std::make_shared<WebImageComponent>(mWindow, 0);  // image expire immediately
		image->setImage(ra.userpic);
		setTitleImage(image);
	}

	setSubTitle("Player : " + ra.username + "  Points : " + ra.totalpoints + "  Rank : " + ra.rank);

	for (auto game : ra.games)
	{
		ComponentListRow row;

		auto itstring = std::make_shared<MultiLineMenuEntry>(mWindow, game.name, game.achievements + " achievements");

		auto badge = std::make_shared<WebImageComponent>(mWindow);
		badge->setMaxSize(IMAGESIZE, IMAGESIZE);
		badge->setImage(game.badge.empty() ? ":/cartridge.svg" : game.badge);			
		row.addElement(badge, false);

		auto spacer = std::make_shared<GuiComponent>(mWindow);
		spacer->setSize(IMAGESPACER, 0);
		row.addElement(spacer, false);

		if (!game.points.empty())
		{
			std::string longmsg = game.name + "\n" + game.achievements + " achievements\n" + game.points + " points\nLast played : " + game.lastplayed;
			row.makeAcceptInputHandler([this, longmsg] { mWindow->pushGui(new GuiMsgBox(mWindow, longmsg, _("OK"))); });
		}

		row.addElement(itstring, true);
		addRow(row);		
	}

	centerWindow();
}

void GuiRetroAchievements::centerWindow()
{
	float width = (float)Math::min((int)Renderer::getScreenHeight(), (int)(Renderer::getScreenWidth() * 0.90f));

	if (Renderer::isSmallScreen())
		mMenu.setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	else
		mMenu.setSize(WINDOW_WIDTH, Renderer::getScreenHeight() * 0.87f);

	mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
}
