#include "guis/GuiThemeInstaller.h"

#include "ApiSystem.h"
#include "components/OptionListComponent.h"
#include "guis/GuiSettings.h"
#include "views/ViewController.h"
#include "utils/StringUtil.h"
#include "components/ComponentGrid.h"
#include "components/MultiLineMenuEntry.h"
#include "LocaleES.h"
#include "ContentInstaller.h"
#include "GuiLoading.h"
#include "components/WebImageComponent.h"
#include "components/ButtonComponent.h"

#define WINDOW_WIDTH (float)Math::min(Renderer::getScreenHeight() * 1.125f, Renderer::getScreenWidth() * 0.90f)

GuiThemeInstaller::GuiThemeInstaller(Window* window)
	: GuiComponent(window), mGrid(window, Vector2i(1, 4)), mBackground(window, ":/frame.png"), mReloadList(1), mTabFilter(0)
{
	addChild(&mBackground);
	addChild(&mGrid);

	// Form background
	auto theme = ThemeData::getMenuTheme();
	mBackground.setImagePath(theme->Background.path);
	mBackground.setEdgeColor(theme->Background.color);
	mBackground.setCenterColor(theme->Background.centerColor);
	mBackground.setCornerSize(theme->Background.cornerSize);
	mBackground.setPostProcessShader(theme->Background.menuShader);

	// Title
	mHeaderGrid = std::make_shared<ComponentGrid>(mWindow, Vector2i(1, 5));

	mTitle = std::make_shared<TextComponent>(mWindow, _("THEMES DOWNLOADER"), theme->Title.font, theme->Title.color, ALIGN_CENTER);
	mSubtitle = std::make_shared<TextComponent>(mWindow, _("SELECT THEMES TO INSTALL/REMOVE"), theme->TextSmall.font, theme->TextSmall.color, ALIGN_CENTER);
	mHeaderGrid->setEntry(mTitle, Vector2i(0, 1), false, true);
	mHeaderGrid->setEntry(mSubtitle, Vector2i(0, 3), false, true);

	mGrid.setEntry(mHeaderGrid, Vector2i(0, 0), false, true);

	// Tabs
	mTabs = std::make_shared<ComponentTab>(mWindow);
	mTabs->addTab(_("All"));
	mTabs->addTab(_("Installed"));
	mTabs->addTab(_("Available"));

	mTabs->setCursorChangedCallback([&](const CursorState& /*state*/)
	{
		if (mTabFilter != mTabs->getCursorIndex())
		{
			mTabFilter = mTabs->getCursorIndex();
			mReloadList = 3;
		}
	});

	mGrid.setEntry(mTabs, Vector2i(0, 1), false, true);

	// Entries
	mList = std::make_shared<ComponentList>(mWindow);
	mList->setUpdateType(ComponentListFlags::UpdateType::UPDATE_ALWAYS);

	mGrid.setEntry(mList, Vector2i(0, 2), true, true);

	// Buttons
	std::vector<std::shared_ptr<ButtonComponent>> buttons;
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("BACK"), _("BACK"), [this] { delete this; }));

	mButtonGrid = makeButtonGrid(mWindow, buttons);
	mGrid.setEntry(mButtonGrid, Vector2i(0, 3), true, false);

	mGrid.setUnhandledInputCallback([this](InputConfig* config, Input input) -> bool
	{
		if (config->isMappedLike("down", input)) { mGrid.setCursorTo(mList); mList->setCursorIndex(0); return true; }
		if (config->isMappedLike("up", input)) { mList->setCursorIndex(mList->size() - 1); mGrid.moveCursor(Vector2i(0, 1)); return true; }
		return false;
	});

	centerWindow();

	ContentInstaller::RegisterNotify(this);
}

void GuiThemeInstaller::onSizeChanged()
{
	GuiComponent::onSizeChanged();

	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));

	mGrid.setSize(mSize);

	const float titleHeight = mTitle->getFont()->getLetterHeight();
	const float subtitleHeight = mSubtitle->getFont()->getLetterHeight();
	const float titleSubtitleSpacing = mSize.y() * 0.03f;

	mGrid.setRowHeight(0, titleHeight + titleSubtitleSpacing + subtitleHeight + (Renderer::getScreenHeight()*0.05f));

	if (mTabs->size() == 0)
		mGrid.setRowHeight(1, 0.00001f);
	else
		mGrid.setRowHeight(1, titleHeight * 2);

	mGrid.setRowHeight(3, mButtonGrid->getSize().y());

	mHeaderGrid->setRowHeight(1, titleHeight);
	mHeaderGrid->setRowHeight(2, titleSubtitleSpacing);
	mHeaderGrid->setRowHeight(3, subtitleHeight);
}

GuiThemeInstaller::~GuiThemeInstaller()
{
	ContentInstaller::UnregisterNotify(this);
}

void GuiThemeInstaller::OnContentInstalled(int contentType, std::string contentName, bool success)
{
	if (contentType == ContentInstaller::CONTENT_THEME_INSTALL || contentType == ContentInstaller::CONTENT_THEME_UNINSTALL)
	{
		mReloadList = 2;

		for (auto theme : mThemes)
		{
			if (theme.name == contentName)
			{
				auto curr = Utils::String::toLower(Settings::getInstance()->getString("ThemeSet"));
				if (Utils::String::toLower(curr) == Utils::String::toLower(Utils::FileSystem::getFileName(theme.url)))
				{
					auto window = mWindow;

					window->postToUiThread([window]
					{						
						auto win = window;
						auto viewController = ViewController::get();

						GuiComponent *gui;
						while ((gui = win->peekGui()) != NULL)
						{
							win->removeGui(gui);

							if (gui != viewController)
								delete gui;
						}

						viewController->reloadAllGames(win);
					});
				}

				break;
			}
		}

	}
}

void GuiThemeInstaller::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mReloadList != 0)
	{
		bool silent = (mReloadList == 2 || mReloadList == 3);
		bool updateInstalledState = (mReloadList == 2);

		mReloadList = 0;

		if (silent)
		{
			if (updateInstalledState)
			{
				for (auto it = mThemes.begin(); it != mThemes.end(); it++)
					it->isInstalled = ApiSystem::getInstance()->isThemeInstalled(it->name, it->url);
			}

			loadList();
		}
		else
			loadThemesAsync();
	}
}

void GuiThemeInstaller::loadList()
{
	int idx = mList->getCursorIndex();
	mList->clear();

	int i = 0;
	for (auto theme : mThemes)
	{
		if (mTabFilter == 1 && !theme.isInstalled)
			continue;

		if (mTabFilter == 2 && theme.isInstalled)
			continue;

		ComponentListRow row;

		auto grid = std::make_shared<GuiBatoceraThemeEntry>(mWindow, theme);
		row.addElement(grid, true);

		if (!grid->isInstallPending())
		{
			bool current = grid->isCurrentTheme();
			row.makeAcceptInputHandler([this, theme, current] { processTheme(theme, current); });
		}

		mList->addRow(row, i == idx);
		i++;
	}
	
	if (i == 0)
	{
		auto theme = ThemeData::getMenuTheme();
		ComponentListRow row;
		row.selectable = false;
		auto text = std::make_shared<TextComponent>(mWindow, _("No items"), theme->TextSmall.font, theme->Text.color, ALIGN_CENTER);
		row.addElement(text, true);
		mList->addRow(row, false, false);
	}

	centerWindow();
	mReloadList = 0;
}

void GuiThemeInstaller::loadThemesAsync()
{
	Window* window = mWindow;

	mWindow->pushGui(new GuiLoading<std::vector<BatoceraTheme>>(mWindow, _("PLEASE WAIT"),
		[this, window](auto gui)
		{
			return ApiSystem::getInstance()->getBatoceraThemesList();		
		},
		[this, window](std::vector<BatoceraTheme> themes)
		{
			mThemes = themes;
			loadList();
		}
	));
}

void GuiThemeInstaller::centerWindow()
{
	if (Renderer::ScreenSettings::fullScreenMenus())
		setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	else
		setSize(WINDOW_WIDTH, Renderer::getScreenHeight() * 0.875f);

	setPosition((Renderer::getScreenWidth() - getSize().x()) / 2, (Renderer::getScreenHeight() - getSize().y()) / 2);
}

void GuiThemeInstaller::processTheme(BatoceraTheme theme, bool isCurrentTheme)
{
	if (theme.name.empty())
		return;

	GuiSettings* msgBox = new GuiSettings(mWindow, theme.name);
	msgBox->setSubTitle(theme.url);
	msgBox->setTag("popup");

	if (theme.isInstalled)
	{
		msgBox->addEntry(_U("\uF019 ") + _("UPDATE"), false, [this, msgBox, theme]
		{
			char trstring[1024];
			snprintf(trstring, 1024, _("'%s' ADDED TO DOWNLOAD QUEUE").c_str(), theme.name.c_str());
			mWindow->displayNotificationMessage(_U("\uF019 ") + std::string(trstring));

			ContentInstaller::Enqueue(mWindow, ContentInstaller::CONTENT_THEME_INSTALL, theme.name);
			
			mReloadList = 2;			
			msgBox->close();
		});

		if (!isCurrentTheme)
		{
			msgBox->addEntry(_U("\uF014 ") + _("REMOVE"), false, [this, msgBox, theme]
			{
				auto updateStatus = ApiSystem::getInstance()->uninstallBatoceraTheme(theme.name);

				if (updateStatus.second == 0)
					mWindow->displayNotificationMessage(_U("\uF019 ") + theme.name + " : " + _("THEME UNINSTALLED SUCCESSFULLY"));
				else
				{
					std::string error = _("AN ERROR OCCURRED") + std::string(": ") + updateStatus.first;
					mWindow->displayNotificationMessage(_U("\uF019 ") + error);
				}

				mReloadList = 2;
				msgBox->close();
			});
		}
	}
	else
	{
		msgBox->addEntry(_U("\uF019 ") + _("INSTALL"), false, [this, msgBox, theme]
		{
			char trstring[1024];
			snprintf(trstring, 1024, _("'%s' ADDED TO DOWNLOAD QUEUE").c_str(), theme.name.c_str());
			mWindow->displayNotificationMessage(_U("\uF019 ") + std::string(trstring));

			ContentInstaller::Enqueue(mWindow, ContentInstaller::CONTENT_THEME_INSTALL, theme.name);

			mReloadList = 2;
			msgBox->close();			
		});
	}

	mWindow->pushGui(msgBox);
}

bool GuiThemeInstaller::input(InputConfig* config, Input input)
{
	if(GuiComponent::input(config, input))
		return true;
	
	if(input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		delete this;
		return true;
	}

	if(config->isMappedTo("start", input) && input.value != 0)
	{
		// close everything
		Window* window = mWindow;
		while(window->peekGui() && window->peekGui() != ViewController::get())
			delete window->peekGui();

		return true;
	}

	if (mTabs->input(config, input))
		return true;

	return false;
}

std::vector<HelpPrompt> GuiThemeInstaller::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mList->getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("start", _("CLOSE")));
	return prompts;
}


//////////////////////////////////////////////////////////////
// GuiBatoceraThemeEntry
//////////////////////////////////////////////////////////////

GuiBatoceraThemeEntry::GuiBatoceraThemeEntry(Window* window, BatoceraTheme& entry) :
	ComponentGrid(window, Vector2i(5, 4))
{
	mEntry = entry;

	auto theme = ThemeData::getMenuTheme();

	bool isInstalled = entry.isInstalled;

	mIsPending = ContentInstaller::IsInQueue(ContentInstaller::CONTENT_THEME_INSTALL, entry.name) || ContentInstaller::IsInQueue(ContentInstaller::CONTENT_THEME_UNINSTALL, entry.name);

	auto curr = Utils::String::toLower(Settings::getInstance()->getString("ThemeSet"));
	mIsCurrentTheme = curr == Utils::String::toLower(Utils::FileSystem::getFileName(entry.url)) || curr == Utils::String::toLower(entry.name);

	mImage = std::make_shared<TextComponent>(mWindow);
	mImage->setColor(theme->Text.color);
	mImage->setHorizontalAlignment(Alignment::ALIGN_CENTER);
	mImage->setVerticalAlignment(Alignment::ALIGN_TOP);
	mImage->setFont(theme->Text.font);
	mImage->setText(isInstalled ? _U("\uF058") : _U("\uF019"));
	mImage->setSize(theme->Text.font->getLetterHeight() * 1.5f, 0);

	mText = std::make_shared<TextComponent>(mWindow, Utils::String::replace(entry.name, "_", " "), theme->Text.font, theme->Text.color);
	mText->setLineSpacing(1.5);
	mText->setVerticalAlignment(ALIGN_TOP);

	std::string details = _U("\uf007  ") + entry.author;

	if (entry.size > 0)
		details = details + _U("  \uf019  ") + std::to_string(entry.size) + "Mb";
	
	if (!entry.lastUpdate.empty())
	{
		details = details + "\r\n";
		details = details + _U("\uf073  ") + entry.lastUpdate + "  ";
		for (int i = 0; i < entry.upToDate; i++)
			details = details + _U("\uf006 ");		
	}

	details = details + "\r\n";
	details = details + _U("\uf0AC  ") + entry.url;

	if (mIsCurrentTheme)
		details = _U("\uf05A  ") + _("CURRENT THEME") + std::string("  ") + details;
	else if (mIsPending)
		details = _("CURRENTLY IN DOWNLOAD QUEUE");	

	mSubstring = std::make_shared<TextComponent>(mWindow, details, theme->TextSmall.font, theme->Text.color);
	mSubstring->setOpacity(192);

	setEntry(mImage, Vector2i(0, 1), false, true);
	setEntry(mText, Vector2i(2, 1), false, true);
	setEntry(mSubstring, Vector2i(2, 2), false, true);

	float windowWidth = WINDOW_WIDTH;

	float itemHeight = (theme->Text.font->getHeight() + theme->TextSmall.font->getHeight() * 3) * 1.15f;
	itemHeight = Math::max(itemHeight, Renderer::getScreenHeight() * 0.132f);

	float refImageWidth = (itemHeight / windowWidth) * 1.7777f; // 16:9
	refImageWidth = Math::min(refImageWidth, 0.24);

	setColWidthPerc(0, 42.0f / windowWidth, false);
	setColWidthPerc(1, 0.015f, false);
	setColWidthPerc(3, 0.002f, false);
	setColWidthPerc(4, refImageWidth, false);

	setRowHeightPerc(0, (itemHeight - mText->getSize().y() - mSubstring->getSize().y()) / itemHeight / 2.0f, false);
	setRowHeightPerc(1, mText->getSize().y() / itemHeight, false);
	setRowHeightPerc(2, mSubstring->getSize().y() / itemHeight, false);

	if (mIsPending)
	{
		if (mIsCurrentTheme)
			mImage->setText(_U("\uF05E"));
		else
			mImage->setText(_U("\uF04B"));

		mImage->setOpacity(120);
		mText->setOpacity(150);
		mSubstring->setOpacity(120);
	}
	
	if (!mEntry.image.empty())
	{
		Vector2f maxSize(windowWidth * refImageWidth, itemHeight * 0.85f);

		mPreviewImage = std::make_shared<WebImageComponent>(window, 600); // image expire after 10 minutes
		mPreviewImage->setImage(mEntry.image, false, maxSize);		
		mPreviewImage->setMaxSize(maxSize);
		mPreviewImage->setRoundCorners(0.03f);

		setEntry(mPreviewImage, Vector2i(4, 0), false, false, Vector2i(1, 4));
	}
		
	setSize(Vector2f(0, itemHeight));
}

void GuiBatoceraThemeEntry::setColor(unsigned int color)
{
	mImage->setColor(color);
	mText->setColor(color);
	mSubstring->setColor(color);
}

void GuiBatoceraThemeEntry::update(int deltaTime)
{
	ComponentGrid::update(deltaTime);

	if (mImage != nullptr)
		mImage->update(deltaTime);
}