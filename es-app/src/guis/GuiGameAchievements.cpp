#include "guis/GuiGameAchievements.h"
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
#include "views/ViewController.h"

#define WINDOW_WIDTH (float)Math::min(Renderer::getScreenHeight() * 1.125f, Renderer::getScreenWidth() * 0.90f)
#define IMAGESIZE (Renderer::getScreenHeight() * (48.0 / 720.0))
#define IMAGESPACER (Renderer::getScreenHeight() * (10.0 / 720.0))

void GuiGameAchievements::show(Window* window, int gameId)
{
	window->pushGui(new GuiLoading<GameInfoAndUserProgress>(window, _("PLEASE WAIT"),
		[window, gameId](auto gui)
	{
		return RetroAchievements::getGameInfoAndUserProgress(gameId);
	},
		[window](GameInfoAndUserProgress ra)
	{
		window->pushGui(new GuiGameAchievements(window, ra));
	}));
}


class GameAchievementEntry : public ComponentGrid
{
public:
	GameAchievementEntry(Window* window, Achievement& ra) :
		ComponentGrid(window, Vector2i(3, 2))
	{
		mGameInfo = ra;

		auto theme = ThemeData::getMenuTheme();

		mImage = std::make_shared<WebImageComponent>(mWindow);

		setEntry(mImage, Vector2i(0, 0), false, false, Vector2i(1, 2));
				
		std::string desc = mGameInfo.Description;
		desc += _U(" - ") + _("Points") + ": " + mGameInfo.Points;
		if (!mGameInfo.DateEarned.empty())
			desc += _U("  \uf091  ") + _("Unlocked on") + ": " + mGameInfo.DateEarned;

		mText = std::make_shared<TextComponent>(mWindow, mGameInfo.Title, theme->Text.font, theme->Text.color);
		mText->setVerticalAlignment(ALIGN_TOP);

		mSubstring = std::make_shared<TextComponent>(mWindow, desc, theme->TextSmall.font, theme->Text.color);
		mSubstring->setOpacity(192);

		setEntry(mText, Vector2i(2, 0), false, true);
		setEntry(mSubstring, Vector2i(2, 1), false, true);

		setColWidthPerc(0, IMAGESIZE / WINDOW_WIDTH);
		setColWidthPerc(1, IMAGESPACER / WINDOW_WIDTH);

		int height = Math::max(IMAGESIZE + IMAGESPACER, mText->getSize().y() + mSubstring->getSize().y());
		mImage->setMaxSize(height - IMAGESPACER, height - IMAGESPACER);
		mImage->setImage(mGameInfo.getBadgeUrl());

		setSize(0, height);
	}

	virtual void setColor(unsigned int color)
	{
		mText->setColor(color);
		mSubstring->setColor(color);
	}

private:
	std::shared_ptr<TextComponent> mText;
	std::shared_ptr<TextComponent> mSubstring;

	std::shared_ptr<WebImageComponent> mImage;

	Achievement mGameInfo;
};


GuiGameAchievements::GuiGameAchievements(Window* window, GameInfoAndUserProgress ra) : 
	GuiSettings(window, "", "", nullptr)
{
	// Required for WebImageComponent
	setUpdateType(ComponentListFlags::UPDATE_ALWAYS);

	setTitle(ra.Title);

	mMenu.clearButtons();

	mFile = GuiRetroAchievements::getFileData(std::to_string(ra.ID));
	if (mFile != nullptr)
	{
		auto file = mFile;
		mMenu.addButton(_("LAUNCH"), _("LAUNCH"), [this, file]
		{ 			
			Window* window = mWindow;
			while (window->peekGui() && window->peekGui() != ViewController::get())
				delete window->peekGui();

			ViewController::get()->launch(file);
		});
	}

	mMenu.addButton(_("BACK"), _("go back"), [this] { close(); });

	int totalPoints = 0;
	int userPoints = 0;

	for (auto game : ra.Achievements)
	{
		if (!game.DateEarned.empty())
			userPoints += Utils::String::toInteger(game.Points);

		totalPoints += Utils::String::toInteger(game.Points);
	}

	if (ra.Achievements.size() == 0)
		setSubTitle(_("THIS GAME HAS NO ACHIEVEMENTS YET"));
	else
	{
		auto txt = _("Achievements won") + ": \t" + std::to_string(ra.NumAwardedToUser) + "/" + std::to_string(ra.NumAchievements);
		txt += "\r\n" + _("Points") + ": \t" + std::to_string(userPoints) + "/" + std::to_string(totalPoints);
//		txt += "\r\n" + _("Completion") + " : \t" + ra.UserCompletion;

		setSubTitle(txt);
	}

	auto image = std::make_shared<WebImageComponent>(mWindow);
	image->setImage(ra.getImageUrl());
	setTitleImage(image);

	if (ra.Achievements.size() > 0)
	{
		int percent = Math::round(ra.NumAwardedToUser * 100.0f / ra.Achievements.size());

		char trstring[256];
		snprintf(trstring, 256, _("%d%% complete").c_str(), percent);
		mProgress = std::make_shared<RetroAchievementProgress>(mWindow, ra.NumAwardedToUser, ra.Achievements.size(), Utils::String::trim(trstring));
	}

	for (auto game : ra.Achievements)
	{
		ComponentListRow row;

		auto itstring = std::make_shared<GameAchievementEntry>(mWindow, game);
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
		mMenu.setSize(WINDOW_WIDTH, Renderer::getScreenHeight() * 0.901f);

	mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
}

void GuiGameAchievements::render(const Transform4x4f& parentTrans)
{
	GuiSettings::render(parentTrans);

	if (mProgress != nullptr)
	{
		auto theme = ThemeData::getMenuTheme();

		float h = theme->TextSmall.font->sizeText("A8O\rA8O", 1.1).y();
		float sz = mMenu.getHeaderGridHeight() + Renderer::getScreenHeight() * 0.005;

		float width = (float)Math::min((int)Renderer::getScreenHeight(), (int)(Renderer::getScreenWidth() * 0.90f));
		float iw = mMenu.getTitleHeight() / width;

		float xx = mMenu.getSize().x() - (mMenu.getSize().x() * iw);

		mProgress->setPosition(xx * 0.55f, sz);
		mProgress->setSize(xx * 0.36f, h);

		Transform4x4f trans = parentTrans * mMenu.getTransform();
		mProgress->render(trans);
	}
}

bool GuiGameAchievements::input(InputConfig* config, Input input)
{
	if (config->isMappedTo("x", input) && input.value != 0)
	{
		if (mFile != nullptr)
		{
			auto file = mFile;
			if (file != nullptr)
			{
				Window* window = mWindow;
				while (window->peekGui() && window->peekGui() != ViewController::get())
					delete window->peekGui();

				ViewController::get()->launch(file);
			}
		}

		return true;
	}

	return GuiSettings::input(config, input);
}
std::vector<HelpPrompt> GuiGameAchievements::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));

	if (mFile != nullptr)
		prompts.push_back(HelpPrompt("x", _("LAUNCH")));

	return prompts;
}
