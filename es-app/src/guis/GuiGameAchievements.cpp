#include "guis/GuiGameAchievements.h"
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
#include "GuiGameAchievements.h"

#define WINDOW_WIDTH (float)Math::max((int)(Renderer::getScreenHeight() * 0.875f), (int)(Renderer::getScreenWidth() * 0.6f))
#define IMAGESIZE (Renderer::getScreenHeight() * (48.0 / 720.0))
#define IMAGESPACER (Renderer::getScreenHeight() * (10.0 / 720.0))

void GuiGameAchievements::show(Window* window, int gameId)
{
	window->pushGui(new GuiLoading<GameInfoAndUserProgress>(window, _("PLEASE WAIT"),
		[window, gameId]
	{
		return RetroAchievements::getGameInfoAndUserProgress(gameId);
	},
		[window](GameInfoAndUserProgress ra)
	{
		window->pushGui(new GuiGameAchievements(window, ra));
	}));
}

GuiGameAchievements::GuiGameAchievements(Window* window, GameInfoAndUserProgress ra) : 
	GuiSettings(window, "", "", nullptr)
{
	// Required for WebImageComponent
	setUpdateType(ComponentListFlags::UPDATE_ALWAYS);

	setTitle(ra.Title);

	int totalPoints = 0;
	int userPoints = 0;

	for (auto game : ra.Achievements)
	{
		if (!game.DateEarned.empty())
			userPoints += Utils::String::toInteger(game.Points);

		totalPoints += Utils::String::toInteger(game.Points);
	}

	if (ra.Achievements.size() == 0)
		setSubTitle(_("THIS GAME HAS NO ACHIEVEMENT YET"));
	else
	{
		auto txt = "Achievements won : " + std::to_string(ra.NumAwardedToUser) + "/" + std::to_string(ra.NumAchievements);
		txt += " - Points : " + std::to_string(userPoints) + "/" + std::to_string(totalPoints);
		txt += " - Completion : " + ra.UserCompletion;

		setSubTitle(txt);
	}
		
	//setSubTitle(ra.ConsoleName);
	/*
	auto image = std::make_shared<WebImageComponent>(mWindow, 0);  // image expire immediately
	image->setImage("https://i.retroachievements.org/" + ra.ImageIcon);
	setTitleImage(image);
	*/
	for (auto game : ra.Achievements)
	{
		ComponentListRow row;

		auto itstring = std::make_shared<MultiLineMenuEntry>(mWindow, game.Title, 
			game.Description +
			_U("  -  ") + "Points : " + game.Points +
			(game.DateEarned.empty() ? "" : _U("  \uf091") + "  Unlocked on : " + game.DateEarned)
			);

		auto badge = std::make_shared<WebImageComponent>(mWindow);
		badge->setMaxSize(IMAGESIZE, IMAGESIZE);
		badge->setImage(game.getBadgeUrl());
		row.addElement(badge, false);

		auto spacer = std::make_shared<GuiComponent>(mWindow);
		spacer->setSize(IMAGESPACER, 0);
		row.addElement(spacer, false);
		/*
		if (!game.points.empty())
		{
			int gameId = Utils::String::toInteger(game.id);

			//std::string longmsg = game.name + "\n" + game.achievements + " achievements\n" + game.points + " points\nLast played : " + game.lastplayed;
			row.makeAcceptInputHandler([this, gameId]
			{
				GuiGameAchievements::show(mWindow, gameId);
				//mWindow->pushGui(new GuiMsgBox(mWindow, longmsg, _("OK"))); 
			});
		}
		*/
		row.addElement(itstring, true);
		addRow(row);
	}

	centerWindow();
}

void GuiGameAchievements::centerWindow()
{
	float width = (float)Math::min((int)Renderer::getScreenHeight(), (int)(Renderer::getScreenWidth() * 0.90f));

	if (Renderer::isSmallScreen())
		mMenu.setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	else
		mMenu.setSize(WINDOW_WIDTH, Renderer::getScreenHeight() * 0.87f);

	mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
}
