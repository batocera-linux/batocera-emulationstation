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
#include "LocaleES.h"
#include "GuiLoading.h"

#include "components/MultiLineMenuEntry.h"
#include "GuiGameAchievements.h"

#define WINDOW_WIDTH (float)Math::max((int)(Renderer::getScreenHeight() * 0.875f), (int)(Renderer::getScreenWidth() * 0.6f))
#define IMAGESIZE (Renderer::getScreenHeight() * (48.0 / 720.0))
#define IMAGESPACER (Renderer::getScreenHeight() * (10.0 / 720.0))
#define PROGRESSHEIGHT (Renderer::getScreenHeight() * 0.008f)

void GuiRetroAchievements::show(Window* window)
{
	window->pushGui(new GuiLoading<RetroAchievementInfo>(window, _("PLEASE WAIT"), 
		[window]
		{
			auto summary = RetroAchievements::getUserSummary();
			return RetroAchievements::toRetroAchivementInfo(summary);
		}, 
		[window](RetroAchievementInfo ra)
		{
			window->pushGui(new GuiRetroAchievements(window, ra));
		}));
}

RetroAchievementProgress::RetroAchievementProgress(Window* window, int value, int max, const std::string& label) : GuiComponent(window), mValue(value), mMax(max)
{ 
	auto theme = ThemeData::getMenuTheme();

	mText = std::make_shared<TextComponent>(mWindow, label, theme->TextSmall.font, theme->Text.color);		
	mText->setVerticalAlignment(Alignment::ALIGN_CENTER);
	mText->setHorizontalAlignment(Alignment::ALIGN_CENTER);
}

void RetroAchievementProgress::onSizeChanged()
{
	float padding = mSize.x() * 0.1f;
		
	float y = (mSize.y() + PROGRESSHEIGHT) / 2.0f;
		
	mText->setPosition(padding, y);
	mText->setSize(mSize.x() - 2.0f * padding, mText->getFont()->getLetterHeight());
}

void RetroAchievementProgress::setColor(unsigned int color)
{
	mText->setColor(color);
}

void RetroAchievementProgress::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();
	if (!isVisible() || !Renderer::isVisibleOnScreen(trans.translation().x(), trans.translation().y(), mSize.x(), mSize.y()))
		return;
		
	int padding = mSize.x() * 0.1f;
	int w = mSize.x() - 2.0 * padding;

	float height = PROGRESSHEIGHT;
	float y = mSize.y() / 2.0f - 1.5f * height;

	Renderer::setMatrix(trans);

			// Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), 0x0000FF32, 0x0000FF32);


	Renderer::drawRect(padding, y, w, height, 0x00000032, 0x00000032);

	if (mMax > 0)
	{
		int cur = (w * mValue) / mMax;
		Renderer::drawRect(padding, y, cur, height, 0xFFFF00C0, 0xFFFF00C0);
	}
		
	mText->render(trans);
}	

class RetroAchievementEntry : public ComponentGrid
{
public:
	RetroAchievementEntry(Window* window, RetroAchievementGame& ra) :
		ComponentGrid(window, Vector2i(4, 2))
	{
		mGameInfo = ra;

		auto theme = ThemeData::getMenuTheme();

		mImage = std::make_shared<WebImageComponent>(mWindow);
		setEntry(mImage, Vector2i(0, 0), true, false, Vector2i(1, 2));

		mText = std::make_shared<TextComponent>(mWindow, mGameInfo.name.c_str(), theme->Text.font, theme->Text.color);
		mText->setVerticalAlignment(ALIGN_TOP);

		mSubstring = std::make_shared<TextComponent>(mWindow, mGameInfo.points + " points", theme->TextSmall.font, theme->Text.color);
		mSubstring->setOpacity(192);

		setEntry(mText, Vector2i(2, 0), true, true);
		setEntry(mSubstring, Vector2i(2, 1), false, true);
		
		int percent = Math::round(mGameInfo.wonAchievements * 100.0f / mGameInfo.totalAchievements);
		
		char trstring[256];
		snprintf(trstring, 256, _("%d%% (%d of %d)").c_str(), percent, mGameInfo.wonAchievements, mGameInfo.totalAchievements);		
		mProgress = std::make_shared<RetroAchievementProgress>(mWindow, mGameInfo.wonAchievements, mGameInfo.totalAchievements, Utils::String::trim(trstring));

		setEntry(mProgress, Vector2i(3, 0), false, true, Vector2i(1, 2));

		setColWidthPerc(0, IMAGESIZE / WINDOW_WIDTH);
		setColWidthPerc(1, IMAGESPACER / WINDOW_WIDTH);
		setColWidthPerc(3, 0.25f);

		int height = Math::max(IMAGESIZE + IMAGESPACER, mText->getSize().y() + mSubstring->getSize().y());
		mImage->setMaxSize(height - IMAGESPACER, height - IMAGESPACER);
		mImage->setImage(ra.badge.empty() ? ":/cartridge.svg" : ra.badge);

		setSize(0, height);
	}

	virtual void setColor(unsigned int color)
	{
		mText->setColor(color);
		mSubstring->setColor(color);
		mProgress->setColor(color);
	}

private:
	std::shared_ptr<TextComponent> mText;
	std::shared_ptr<TextComponent> mSubstring;
	std::shared_ptr<RetroAchievementProgress> mProgress;

	std::shared_ptr<WebImageComponent> mImage;
	
	RetroAchievementGame mGameInfo;
};

GuiRetroAchievements::GuiRetroAchievements(Window* window, RetroAchievementInfo ra) : 
	GuiSettings(window, _("RETROACHIEVEMENTS") + " - " + ra.username, "", nullptr)
{
	// Required for WebImageComponent
	setUpdateType(ComponentListFlags::UPDATE_ALWAYS);

	if (!ra.error.empty())
	{
		setSubTitle(ra.error);
		return;
	}

	if (!ra.userpic.empty())
	{
		auto image = std::make_shared<WebImageComponent>(mWindow, 0);  // image expire immediately
		image->setImage(ra.userpic);
		setTitleImage(image);
	}

	setSubTitle(_("Points") + " :\t" + ra.totalpoints + "\r\n"+ _("Rank") + " :\t" + ra.rank);

	for (auto game : ra.games)
	{
		ComponentListRow row;

		auto itstring = std::make_shared<RetroAchievementEntry>(mWindow, game);
		/*
		auto badge = std::make_shared<WebImageComponent>(mWindow);
		badge->setMaxSize(IMAGESIZE, IMAGESIZE);
		badge->setImage(game.badge.empty() ? ":/cartridge.svg" : game.badge);			
		row.addElement(badge, false, false);

		auto spacer = std::make_shared<GuiComponent>(mWindow);
		spacer->setSize(IMAGESPACER, 0);
		row.addElement(spacer, false);
		*/
		if (!game.points.empty())
		{
			int gameId = Utils::String::toInteger(game.id);
			row.makeAcceptInputHandler([this, gameId] { GuiGameAchievements::show(mWindow, gameId); });

			//std::string longmsg = game.name + "\n" + game.achievements + " achievements\n" + game.points + " points\nLast played : " + game.lastplayed;
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
		mMenu.setSize(WINDOW_WIDTH, Renderer::getScreenHeight() * 0.901f);

	mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
}
