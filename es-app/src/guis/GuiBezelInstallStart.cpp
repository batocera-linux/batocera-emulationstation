#include "guis/GuiBezelInstallStart.h"

#include "ApiSystem.h"
#include "components/OptionListComponent.h"
#include "guis/GuiSettings.h"
#include "views/ViewController.h"
#include "components/ComponentGrid.h"
#include "SystemData.h"
#include "LocaleES.h"
#include "ContentInstaller.h"
#include "components/MultiLineMenuEntry.h"
#include "GuiLoading.h"
#include <cstring>
#include "SystemConf.h"

#define WINDOW_WIDTH (float)Math::max((int)Renderer::getScreenHeight(), (int)(Renderer::getScreenWidth() * 0.65f))

GuiBezelInstallStart::GuiBezelInstallStart(Window* window)
	: GuiComponent(window), mMenu(window, _("THE BEZEL PROJECT").c_str()), mReloadList(1)
{
	addChild(&mMenu);
	mMenu.setSubTitle(_("SELECT BEZELS TO INSTALL / REMOVE"));
    mMenu.addButton(_("BACK"), "back", [&] { delete this; });

	centerWindow();

	ContentInstaller::RegisterNotify(this);
}

GuiBezelInstallStart::~GuiBezelInstallStart()
{
	ContentInstaller::UnregisterNotify(this);
}

void GuiBezelInstallStart::OnContentInstalled(int contentType, std::string contentName, bool success)
{
	if (contentType == ContentInstaller::CONTENT_BEZEL_INSTALL && success)
	{
		// When installed, set thebezelproject as default decorations for the system
		auto current = SystemConf::getInstance()->get(contentName + ".bezel");
		if (current == "" || current == "AUTO")
			SystemConf::getInstance()->set(contentName + ".bezel", "thebezelproject");
	}
	else if (contentType == ContentInstaller::CONTENT_BEZEL_UNINSTALL && success)
	{
		// When uninstalled, set "auto" as default decorations for the system, if it was thebezelproject before
		auto current = SystemConf::getInstance()->get(contentName + ".bezel");
		if (current == "thebezelproject")
			SystemConf::getInstance()->set(contentName + ".bezel", "");
	}

	if (contentType == ContentInstaller::CONTENT_BEZEL_INSTALL || contentType == ContentInstaller::CONTENT_BEZEL_UNINSTALL)
		mReloadList = 2;
}

void GuiBezelInstallStart::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mReloadList != 0)
	{
		bool silent = (mReloadList == 2);

		mReloadList = 0;

		if (silent)
			loadList();
		else
			loadBezelsAsync();
	}
}

void GuiBezelInstallStart::loadList()
{
	int idx = mMenu.getCursorIndex();
	mMenu.clear();

	int i = 0;
	for (auto bezel : mBezels)
	{
		ComponentListRow row;

		auto grid = std::make_shared<GuiBatoceraBezelEntry>(mWindow, bezel);
		row.addElement(grid, true);

		if (!grid->isInstallPending())
			row.makeAcceptInputHandler([this, bezel] { processBezel(bezel); });

		mMenu.addRow(row, i == idx);
		i++;
	}

	centerWindow();
	mReloadList = 0;
}

void GuiBezelInstallStart::loadBezelsAsync()
{
	Window* window = mWindow;

	mWindow->pushGui(new GuiLoading<std::vector<BatoceraBezel>>(mWindow, _("PLEASE WAIT"),
		[this]
	{
		return ApiSystem::getInstance()->getBatoceraBezelsList();
	},
		[this](std::vector<BatoceraBezel> bezels)
	{
		mBezels = bezels;
		loadList();
	}
	));
}

void GuiBezelInstallStart::centerWindow()
{
	if (Renderer::isSmallScreen())
		mMenu.setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	else
		mMenu.setSize(WINDOW_WIDTH, Renderer::getScreenHeight() * 0.875f);

	mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
}

void GuiBezelInstallStart::processBezel(BatoceraBezel bezel)
{
	if (bezel.name.empty())
		return;

	GuiSettings* msgBox = new GuiSettings(mWindow, bezel.name);
	msgBox->setSubTitle(bezel.url);
	msgBox->setTag("popup");

	if (bezel.isInstalled)
	{
		msgBox->addEntry(_U("\uF019 ") + _("UPDATE"), false, [this, msgBox, bezel]
		{
			char trstring[1024];
			snprintf(trstring, 1024, _("'%s' ADDED TO DOWNLOAD QUEUE").c_str(), bezel.name.c_str()); // batocera
			mWindow->displayNotificationMessage(_U("\uF019 ") + std::string(trstring));

			ContentInstaller::Enqueue(mWindow, ContentInstaller::CONTENT_BEZEL_INSTALL, bezel.name);
			mReloadList = 2;

			msgBox->close();
		});

		msgBox->addEntry(_U("\uF014 ") + _("REMOVE"), false, [this, msgBox, bezel]
		{
			auto updateStatus = ApiSystem::getInstance()->uninstallBatoceraBezel(bezel.name);

			if (updateStatus.second == 0)
				mWindow->displayNotificationMessage(_U("\uF019 ") + bezel.name + " : " + _("BEZELS UNINSTALLED SUCCESSFULLY"));
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
		msgBox->addEntry(_U("\uF019 ") + _("INSTALL"), false, [this, msgBox, bezel]
		{			
			char trstring[1024];
			snprintf(trstring, 1024, _("'%s' ADDED TO DOWNLOAD QUEUE").c_str(), bezel.name.c_str()); // batocera
			mWindow->displayNotificationMessage(_U("\uF019 ") + std::string(trstring));

			ContentInstaller::Enqueue(mWindow, ContentInstaller::CONTENT_BEZEL_INSTALL, bezel.name);
			mReloadList = 2;

			msgBox->close();
		});
	}

	mWindow->pushGui(msgBox);
}

bool GuiBezelInstallStart::input(InputConfig* config, Input input)
{
	if (GuiComponent::input(config, input))
		return true;

	if (input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		delete this;
		return true;
	}

	if (config->isMappedTo("start", input) && input.value != 0)
	{
		// close everything
		Window* window = mWindow;
		while (window->peekGui() && window->peekGui() != ViewController::get())
			delete window->peekGui();
	}
	return false;
}

std::vector<HelpPrompt> GuiBezelInstallStart::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("start", _("CLOSE")));
	return prompts;
}

//////////////////////////////////////////////////////////////
// GuiBatoceraStoreEntry
//////////////////////////////////////////////////////////////

GuiBatoceraBezelEntry::GuiBatoceraBezelEntry(Window* window, BatoceraBezel& entry) :
	ComponentGrid(window, Vector2i(4, 3))
{
	mEntry = entry;

	auto theme = ThemeData::getMenuTheme();

	bool isInstalled = entry.isInstalled;

	mIsPending = 
		ContentInstaller::IsInQueue(ContentInstaller::CONTENT_BEZEL_INSTALL, entry.name) || 
		ContentInstaller::IsInQueue(ContentInstaller::CONTENT_BEZEL_UNINSTALL, entry.name);

	mImage = std::make_shared<TextComponent>(mWindow);
	mImage->setColor(theme->Text.color);
	if (!isInstalled)
		mImage->setOpacity(192);

	mImage->setHorizontalAlignment(Alignment::ALIGN_CENTER);
	mImage->setVerticalAlignment(Alignment::ALIGN_TOP);
	mImage->setFont(theme->Text.font);
	mImage->setText(isInstalled ? _U("\uF058") : _U("\uF019"));
	mImage->setSize(theme->Text.font->getLetterHeight() * 1.5f, 0);

	mText = std::make_shared<TextComponent>(mWindow, entry.name, theme->Text.font, theme->Text.color);
	mText->setLineSpacing(1.5);
	mText->setVerticalAlignment(ALIGN_TOP);

	std::string details = entry.url;

	if (mIsPending)
	{
		char trstring[1024];
		snprintf(trstring, 1024, _("CURRENTLY IN DOWNLOAD QUEUE").c_str(), entry.name.c_str());
		details = trstring;
	}

	mSubstring = std::make_shared<TextComponent>(mWindow, details, theme->TextSmall.font, theme->Text.color);
	mSubstring->setOpacity(192);

	setEntry(mImage, Vector2i(0, 0), false, true, Vector2i(1, 3));
	setEntry(mText, Vector2i(2, 0), false, true);
	setEntry(mSubstring, Vector2i(2, 1), false, true);

	float h = mText->getSize().y() * 1.1f + mSubstring->getSize().y()/* + mDetails->getSize().y()*/;
	float sw = WINDOW_WIDTH;

	setColWidthPerc(0, 50.0f / sw, false);
	setColWidthPerc(1, 0.015f, false);
	setColWidthPerc(3, 0.002f, false);

	setRowHeightPerc(0, mText->getSize().y() / h, false);
	setRowHeightPerc(1, mSubstring->getSize().y() / h, false);

	if (mIsPending)
	{
		mImage->setText(_U("\uF04B"));
		mImage->setOpacity(120);
		mText->setOpacity(150);
		mSubstring->setOpacity(120);
	}

	setSize(Vector2f(0, h));
}

void GuiBatoceraBezelEntry::setColor(unsigned int color)
{
	mImage->setColor(color);
	mText->setColor(color);
	mSubstring->setColor(color);
}