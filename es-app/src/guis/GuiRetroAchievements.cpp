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
#include "GuiLoading.h"

#include "components/MultiLineMenuEntry.h"

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
	if (!ra.error.empty())
	{
		setSubTitle(ra.error);
		return;
	}

	if (!ra.userpic.empty() && Utils::FileSystem::exists(ra.userpic) && !Renderer::isSmallScreen())
	{
		auto image = std::make_shared<ImageComponent>(mWindow);
		image->setImage(ra.userpic);
		setTitleImage(image);
	}

	setSubTitle("Player " + ra.username + " (" + ra.totalpoints + " points) is " + ra.rank);

	for (auto game : ra.games)
	{
		ComponentListRow row;

		auto itstring = std::make_shared<MultiLineMenuEntry>(mWindow, game.name, game.achievements + " achievements");
		auto badge = std::make_shared<ImageComponent>(mWindow);

		if (game.badge.empty())
			badge->setImage(":/cartridge.svg");
		else
			badge->setImage(game.badge);

		badge->setSize(48, 48);
		row.addElement(badge, false);

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
		addRow(row);		
	}
}
