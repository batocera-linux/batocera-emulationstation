#include "guis/GuiInstall.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiRetroAchievements.h"
#include "guis/GuiSettings.h"
#include "components/WebImageComponent.h"
#include "Window.h"
#include "Log.h"
#include "Settings.h"
#include "GuiLoading.h"
#include "components/MultiLineMenuEntry.h"
#include "GuiGameAchievements.h"
#include "SystemData.h"
#include "FileData.h"
#include "views/ViewController.h"

#include <string>
#include "LocaleES.h"

#define WINDOW_WIDTH (float)Math::min(Renderer::getScreenHeight() * 1.125f, Renderer::getScreenWidth() * 0.90f)
#define IMAGESIZE (Renderer::getScreenHeight() * (48.0 / 720.0))
#define IMAGESPACER (Renderer::getScreenHeight() * (10.0 / 720.0))
#define PROGRESSHEIGHT (Renderer::getScreenHeight() * 0.008f)

void GuiRetroAchievements::show(Window* window)
{
	window->pushGui(new GuiLoading<RetroAchievementInfo>(window, _("PLEASE WAIT"), 
		[window](auto gui)
		{
			auto summary = RetroAchievements::getUserSummary();
			return RetroAchievements::toRetroAchivementInfo(summary);
		}, 
		[window](RetroAchievementInfo ra)
		{
			if (ra.username.empty() && !ra.error.empty())
				window->pushGui(new GuiMsgBox(window, _("AN ERROR OCCURRED") + "\r\n" + ra.error, _("OK")));
			else if (ra.username.empty())
				window->pushGui(new GuiMsgBox(window, _("AN ERROR OCCURRED"), _("OK")));
			else
				window->pushGui(new GuiRetroAchievements(window, ra));
		}));
}

RetroAchievementProgress::RetroAchievementProgress(Window* window, int valueSoftcore, int valueHardcore, int max, const std::string& label) : GuiComponent(window), 
	mValueSoftCore(valueSoftcore), mValueHardCore(valueHardcore), mMax(max)
{ 
	auto theme = ThemeData::getMenuTheme();

	mText = std::make_shared<TextComponent>(mWindow, label, theme->TextSmall.font, theme->Text.color);		
	mText->setVerticalAlignment(Alignment::ALIGN_CENTER);
	mText->setHorizontalAlignment(Alignment::ALIGN_CENTER);
}

void RetroAchievementProgress::onSizeChanged()
{
	GuiComponent::onSizeChanged();

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
	if (!isVisible())
		return;

	Transform4x4f trans = parentTrans * getTransform();

	auto rect = Renderer::getScreenRect(trans, mSize);
	if (!Renderer::isVisibleOnScreen(rect))
		return;
		
	int padding = mSize.x() * 0.1f;
	int w = mSize.x() - 2.0 * padding;

	float height = PROGRESSHEIGHT;
	float y = mSize.y() / 2.0f - 1.5f * height;

	Renderer::setMatrix(trans);

	Renderer::drawRect(padding, y, w, height, 0x00000032, 0x00000032);

	if (mMax > 0)
	{
		if (mValueSoftCore > 0 && mValueSoftCore > mValueHardCore)
		{
			int cur = (w * mValueSoftCore) / mMax;
			Renderer::drawRect(padding, y, cur, height, 0x0B71C1FF);
		}

		if (mValueHardCore > 0)
		{
			int cur = (w * mValueHardCore) / mMax;
			Renderer::drawRect(padding, y, cur, height, 0xCC9900FF);
		}
	}

	mText->render(trans);
}	

#include <iostream>
#include <string>
#include "components/CarouselComponent.h"
#include "ThemeData.h"

class RetroAchievementEntry : public ComponentGrid
{
public:
	RetroAchievementEntry(Window* window, RetroAchievementGame& ra) :
		ComponentGrid(window, Vector2i(4, 4))
	{
		mSelected = false;
		mGameInfo = ra;
		mFileData = GuiRetroAchievements::getFileData(mGameInfo.id);

		auto theme = ThemeData::getMenuTheme();
		
		std::map<std::string, std::string> sysMap;
		sysMap["name"] = mFileData ? mFileData->getName() : mGameInfo.name;
		sysMap["consoleName"] = mFileData ? mFileData->getSourceFileData()->getSystem()->getFullName() : mGameInfo.consoleName;		
		sysMap["badge"] = mGameInfo.badge.empty() ? ":/cartridge.svg" : mGameInfo.badge;
		sysMap["wonAchievementsSoftcore"] = std::to_string(mGameInfo.wonAchievementsSoftcore);
		sysMap["wonAchievementsHardcore"] = std::to_string(mGameInfo.wonAchievementsHardcore);
		sysMap["totalAchievements"] = std::to_string(mGameInfo.totalAchievements);
		sysMap["activeOpacity"] = mFileData == nullptr ? "0.6" : "1";
		sysMap["extraOpacity"] = mFileData == nullptr ? "0.3" : "0.7";

		auto templ = new CarouselItemTemplate("template", mWindow);
		templ->loadFromString(R"=====(
			<stackpanel size="1">
				<separator>8</separator>				
				<text multiline="false" linespacing="1" verticalAlignment="center" size="0 1">
					<text>${name}</text>
					<fontPath>${menu.text.font.path}</fontPath>
					<fontSize>${menu.text.font.size}</fontSize>
					<color>${menu.text.color}</color>
					<opacity>${activeOpacity}</opacity>
			  		<storyboard event="activate">
						<animation property="color" to="${menu.text.selectedcolor}"/>
					</storyboard>
					<storyboard event="deactivate">
						<animation property="color" to="${menu.text.color}"/>
					</storyboard>
				</text>			
				<text multiline="false" linespacing="1" verticalAlignment="center" size="0 1">
					<text>[${consoleName}]</text>
					<fontPath>${menu.textsmall.font.path}</fontPath>
					<fontSize>${menu.textsmall.font.size}</fontSize>
					<color>${menu.text.color}</color>
					<opacity>${extraOpacity}</opacity>
					<padding>0 2 0 0</padding> 
					<storyboard event="activate">
						<animation property="color" to="${menu.text.selectedcolor}"/>
					</storyboard>
					<storyboard event="deactivate">
						<animation property="color" to="${menu.text.color}"/>
					</storyboard>
				</text>
			</stackpanel>
			)=====", &sysMap);
		
		mItemTemplate = std::shared_ptr<CarouselItemTemplate>(templ);
		mItemTemplate->updateBindings(nullptr);

		mImage = std::make_shared<WebImageComponent>(mWindow);
		setEntry(mImage, Vector2i(0, 0), false, false, Vector2i(1, 4));
		
		std::string desc; // = mGameInfo.points + " points";
		
		if (mGameInfo.scoreHardcore != mGameInfo.scoreSoftcore || mGameInfo.scoreHardcore == 0)
			desc = Utils::String::format(_("%d of %d softcore points").c_str(), mGameInfo.scoreSoftcore, mGameInfo.possibleScore);

		if (mGameInfo.scoreHardcore != 0)
		{
			if (!desc.empty())
				desc = desc + " - ";

			desc = desc + Utils::String::format(_("%d of %d hardcore points").c_str(), mGameInfo.scoreHardcore, mGameInfo.possibleScore);
		}

		// "42 of 75 softcore points"
		
	//	desc = mFileData ? mFileData->getSourceFileData()->getSystem()->getFullName() : mGameInfo.consoleName;

		mSubstring = std::make_shared<TextComponent>(mWindow, desc, theme->TextSmall.font, theme->Text.color);
		mSubstring->setOpacity(192);

		setEntry(mItemTemplate, Vector2i(2, 1), false, true); // mText
		setEntry(mSubstring, Vector2i(2, 2), false, true);

		int percent = mGameInfo.totalAchievements == 0 ? 0 : Math::round(mGameInfo.wonAchievementsSoftcore * 100.0f / mGameInfo.totalAchievements);
		
		char trstring[256];
		snprintf(trstring, 256, _("%d%% (%d of %d)").c_str(), percent, mGameInfo.wonAchievementsSoftcore, mGameInfo.totalAchievements);
		mProgress = std::make_shared<RetroAchievementProgress>(mWindow, mGameInfo.wonAchievementsSoftcore, mGameInfo.wonAchievementsHardcore, mGameInfo.totalAchievements, Utils::String::trim(trstring));

		setEntry(mProgress, Vector2i(3, 0), false, true, Vector2i(1, 4));

		float textHeight = theme->Text.font->getHeight();
		int height = Math::max(IMAGESIZE + IMAGESPACER, textHeight + mSubstring->getSize().y());

		float hTxt = textHeight / height;
		float hSub = mSubstring->getSize().y() / height;		
		float topPadding = Math::max(0.0f, (height - textHeight - mSubstring->getSize().y()) / height / 2.0f);

		setRowHeightPerc(0, topPadding);
		setRowHeightPerc(1, hTxt);
		setRowHeightPerc(2, hSub);
		setRowHeightPerc(3, Math::max(0.0f, 1.0f - topPadding - hTxt - hSub));

		setColWidthPerc(0, (height - IMAGESPACER) / WINDOW_WIDTH);
		setColWidthPerc(1, IMAGESPACER / WINDOW_WIDTH);
		setColWidthPerc(3, 0.25f);
	
		mImage->setMaxSize(height - IMAGESPACER, height - IMAGESPACER);
		mImage->setImage(ra.badge.empty() ? ":/cartridge.svg" : ra.badge);

		
		if (mFileData == nullptr)
		{
			mImage->setColorShift(0x80808080);
			mImage->setOpacity(120);
			mSubstring->setOpacity(120);
			mProgress->setOpacity(120);
		}

		setSize(0, height);
	}

	virtual void onFocusGained() 
	{ 
		if (mSelected)
			return;

		mSelected = true;
		mItemTemplate->playDefaultActivationStoryboard(mSelected);
	}

	virtual void	onFocusLost() 
	{ 
		if (!mSelected)
			return;

		mSelected = false;
		mItemTemplate->playDefaultActivationStoryboard(mSelected);
	}

	virtual void setColor(unsigned int color)
	{
		mSubstring->setColor(color);
		mProgress->setColor(color);
	}

	std::string gameId()
	{
		if (mFileData == nullptr)
			return "";

		return mFileData->getMetadata(MetaDataId::CheevosId);
	}

private:
	bool mSelected;
	FileData* mFileData;
	std::shared_ptr<TextComponent> mSubstring;
	std::shared_ptr<RetroAchievementProgress> mProgress;

	std::shared_ptr<WebImageComponent> mImage;
	std::shared_ptr<CarouselItemTemplate> mItemTemplate;
		
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

	auto txt = _("Softcore points") + ":\t" + ra.softpoints; 
	txt += "\r\n" + _("Points (hardcore)") + ":\t" + ra.points;
	if (!ra.rank.empty())
		txt += "\r\n" + _("Rank") + ":\t" + ra.rank;

	setSubTitle(txt);

	if (!ra.userpic.empty())
	{
		auto image = std::make_shared<WebImageComponent>(mWindow, 0);  // image expire immediately
		image->setImage(ra.userpic);
		setTitleImage(image);
	}

	for (auto game : ra.games)
	{
		ComponentListRow row;

		auto itstring = std::make_shared<RetroAchievementEntry>(mWindow, game);		
		if (!game.id.empty())
		{			
			int gameId = Utils::String::toInteger(game.id);
			row.makeAcceptInputHandler([this, gameId] { GuiGameAchievements::show(mWindow, gameId); });

			//std::string longmsg = game.name + "\n" + game.achievements + " achievements\n" + game.points + " points\nLast played : " + game.lastplayed;
		}

		row.addElement(itstring, true);
		//addRow(row);		

		mMenu.getList()->addRow(row, false, false, itstring->gameId());
	}

	mMenu.getList()->setCursorChangedCallback([&](const CursorState& state) { updateHelpPrompts(); });

	centerWindow();
}

void GuiRetroAchievements::centerWindow()
{
	float width = (float)Math::min((int)Renderer::getScreenHeight(), (int)(Renderer::getScreenWidth() * 0.90f));

	if (Renderer::ScreenSettings::fullScreenMenus())
		mMenu.setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	else
		mMenu.setSize(WINDOW_WIDTH, Renderer::getScreenHeight() * 0.901f);

	mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
}

bool GuiRetroAchievements::input(InputConfig* config, Input input)
{
	if (config->isMappedTo("x", input) && input.value != 0)
	{
		if (mMenu.getList()->size() > 0 && !mMenu.getList()->getSelected().empty())
		{
			auto file = getFileData(mMenu.getList()->getSelected());
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

std::vector<HelpPrompt> GuiRetroAchievements::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt(BUTTON_OK, _("VIEW DETAILS")));

	if (mMenu.getList()->size() > 0 && !mMenu.getList()->getSelected().empty())
		prompts.push_back(HelpPrompt("x", _("LAUNCH")));

	return prompts;
}

FileData* GuiRetroAchievements::getFileData(const std::string& cheevosGameId)
{
	for (auto sys : SystemData::sSystemVector)
	{
		if (!sys->isCheevosSupported())
			continue;

		for (auto file : sys->getRootFolder()->getFilesRecursive(GAME))
			if (file->getMetadata(MetaDataId::CheevosId) == cheevosGameId)
				return file;
	}

	return nullptr;
}
