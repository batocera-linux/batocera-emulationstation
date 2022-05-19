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

#define WINDOW_WIDTH (float)Math::min(Renderer::getScreenHeight() * 1.125f, Renderer::getScreenWidth() * 0.90f)

GuiThemeInstaller::GuiThemeInstaller(Window* window)
	: GuiComponent(window), mMenu(window, _("THEMES DOWNLOADER").c_str()), mReloadList(1)
{
	addChild(&mMenu);
	
	mMenu.setUpdateType(ComponentListFlags::UpdateType::UPDATE_ALWAYS);
	mMenu.setSubTitle(_("SELECT THEMES TO INSTALL/REMOVE")); 
	mMenu.addButton(_("BACK"), "back", [&] { delete this; });

	centerWindow();

	ContentInstaller::RegisterNotify(this);
}

GuiThemeInstaller::~GuiThemeInstaller()
{
	ContentInstaller::UnregisterNotify(this);
}

void GuiThemeInstaller::OnContentInstalled(int contentType, std::string contentName, bool success)
{
	if (contentType == ContentInstaller::CONTENT_THEME_INSTALL || contentType == ContentInstaller::CONTENT_THEME_UNINSTALL)
		mReloadList = 2;
}

void GuiThemeInstaller::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mReloadList != 0)
	{
		bool silent = (mReloadList == 2);

		mReloadList = 0;

		if (silent)
			loadList();
		else
			loadThemesAsync();
	}
}

void GuiThemeInstaller::loadList()
{
	int idx = mMenu.getCursorIndex();
	mMenu.clear();

	int i = 0;
	for (auto theme : mThemes)
	{
		ComponentListRow row;

		auto grid = std::make_shared<GuiBatoceraThemeEntry>(mWindow, theme);
		row.addElement(grid, true);

		if (!grid->isInstallPending())
			row.makeAcceptInputHandler([this, theme] { processTheme(theme); });

		mMenu.addRow(row, i == idx);
		i++;
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
	if (Renderer::isSmallScreen())
		mMenu.setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	else
		mMenu.setSize(WINDOW_WIDTH, Renderer::getScreenHeight() * 0.875f);

	mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
}

void GuiThemeInstaller::processTheme(BatoceraTheme theme)
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

		msgBox->addEntry(_U("\uF014 ") + _("REMOVE"), false, [this, msgBox, theme]
		{
			auto updateStatus = ApiSystem::getInstance()->uninstallBatoceraTheme(theme.name);

			if (updateStatus.second == 0)
				mWindow->displayNotificationMessage(_U("\uF019 ") + theme.name + " : " + _("THEME UNINSTALLED SUCCESSFULLY"));
			else
			{
				std::string error = _("AN ERROR OCCURED") + std::string(": ") + updateStatus.first;
				mWindow->displayNotificationMessage(_U("\uF019 ") + error);
			}

			mReloadList = 2;
			msgBox->close();
		});
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
	}
	return false;
}

std::vector<HelpPrompt> GuiThemeInstaller::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
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

	mIsPending =
		ContentInstaller::IsInQueue(ContentInstaller::CONTENT_THEME_INSTALL, entry.name) ||
		ContentInstaller::IsInQueue(ContentInstaller::CONTENT_THEME_UNINSTALL, entry.name);

	mImage = std::make_shared<TextComponent>(mWindow);
	mImage->setColor(theme->Text.color);
	if (!isInstalled)
		mImage->setOpacity(192);

	mImage->setHorizontalAlignment(Alignment::ALIGN_CENTER);
	mImage->setVerticalAlignment(Alignment::ALIGN_CENTER); // ALIGN_TOP
	mImage->setFont(theme->Text.font);
	mImage->setText(isInstalled ? _U("\uF058") : _U("\uF019"));
	mImage->setSize(theme->Text.font->getLetterHeight() * 1.5f, 0);

	mText = std::make_shared<TextComponent>(mWindow, entry.name, theme->Text.font, theme->Text.color);
	mText->setLineSpacing(1.5);
	mText->setVerticalAlignment(ALIGN_TOP);

	std::string details = entry.url;

	auto curr = Utils::String::toLower(Settings::getInstance()->getString("ThemeSet"));
	bool isCurrentTheme = 
		curr == Utils::String::toLower(Utils::FileSystem::getFileName(entry.url)) ||
		curr == Utils::String::toLower(entry.name);

	if (isCurrentTheme)
	{
		char trstring[1024];
		snprintf(trstring, 1024, _("CURRENT THEME").c_str(), entry.name.c_str());
		details = trstring;
		mIsPending = true;
	}
	else if (mIsPending)
	{
		char trstring[1024];
		snprintf(trstring, 1024, _("CURRENTLY IN DOWNLOAD QUEUE").c_str(), entry.name.c_str());
		details = trstring;
	}

	mSubstring = std::make_shared<TextComponent>(mWindow, details, theme->TextSmall.font, theme->Text.color);
	mSubstring->setOpacity(192);

	setEntry(mImage, Vector2i(0, 0), false, true, Vector2i(1, 4));
	setEntry(mText, Vector2i(2, 1), false, true);
	setEntry(mSubstring, Vector2i(2, 2), false, true);

	float refHeight = 0.16f;
	float refImageWidth = 0.12f;

//	refHeight = 0.18f;
//	refImageWidth = 0.13f;

	float h = mText->getSize().y() * 1.1f + mSubstring->getSize().y()/* + mDetails->getSize().y()*/;
	h = Renderer::getScreenHeight() * refHeight;
	float sw = WINDOW_WIDTH;

	setColWidthPerc(0, 50.0f / sw, false);
	setColWidthPerc(1, 0.015f, false);
	setColWidthPerc(3, 0.002f, false);

	setRowHeightPerc(0, (h - mText->getSize().y() - mSubstring->getSize().y()) / h / 2.0f, false);

	setRowHeightPerc(1, mText->getSize().y() / h, false);
	setRowHeightPerc(2, mSubstring->getSize().y() / h, false);

	if (mIsPending)
	{
		if (isCurrentTheme)
			mImage->setText(_U("\uF05E"));
		else
			mImage->setText(_U("\uF04B"));

		mImage->setOpacity(120);
		mText->setOpacity(150);
		mSubstring->setOpacity(120);
	}

	
	// image
	if (!mEntry.image.empty())
	{
		setColWidthPerc(4, refImageWidth, false);

		Vector2f maxSize(Renderer::getScreenWidth() * refImageWidth * 1.2f, Renderer::getScreenHeight() * (refHeight - 0.02f));

		mPreviewImage = std::make_shared<WebImageComponent>(window, 600); // image expire after 10 minutes
		mPreviewImage->setImage(mEntry.image, false, maxSize);
		mPreviewImage->setMaxSize(maxSize);		
		mPreviewImage->setRoundCorners(0.02f);

		setEntry(mPreviewImage, Vector2i(3, 0), false, false, Vector2i(1, 4));
	}
	else
		setColWidthPerc(4, 0.0001f, false);


	setSize(Vector2f(0, h));
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