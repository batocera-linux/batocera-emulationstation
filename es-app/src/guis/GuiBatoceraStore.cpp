#include "guis/GuiBatoceraStore.h"

#include "ApiSystem.h"
#include "components/OptionListComponent.h"
#include "guis/GuiSettings.h"
#include "views/ViewController.h"
#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"
#include "components/ComponentGrid.h"
#include "components/MultiLineMenuEntry.h"
#include "LocaleES.h"
#include "guis/GuiMsgBox.h"
#include "ContentInstaller.h"
#include "GuiLoading.h"
#include <unordered_set>
#include <algorithm>

#define WINDOW_WIDTH (float)Math::max((int)Renderer::getScreenHeight(), (int)(Renderer::getScreenWidth() * 0.73f))

GuiBatoceraStore::GuiBatoceraStore(Window* window)
	: GuiComponent(window), mMenu(window, _("CONTENT DOWNLOADER").c_str())
{
	auto theme = ThemeData::getMenuTheme();
	mWaitingLoad = false;

	mMenu.setSubTitle(_("SELECT CONTENT TO INSTALL / REMOVE"));
	
	addChild(&mMenu);

	mMenu.addButton(_("REFRESH"), "refresh", [&] { loadPackages(true); });
	mMenu.addButton(_("BACK"), "back", [&] { delete this; });
	centerWindow();
	
	mWindow->postToUiThread([this](Window* w) { loadPackages(); });
}

void GuiBatoceraStore::centerWindow()
{
	if (Renderer::isSmallScreen())
		mMenu.setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	else
		mMenu.setSize(WINDOW_WIDTH, Renderer::getScreenHeight() * 0.875f);

	mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
}

static bool sortPackagesByGroup(PacmanPackage& sys1, PacmanPackage& sys2)
{
	std::string name1 = Utils::String::toUpper(sys1.group);
	std::string name2 = Utils::String::toUpper(sys2.group);
	return name1.compare(name2) < 0;
}

void GuiBatoceraStore::loadPackages(bool updatePackageList)
{
	mWaitingLoad = true;

	Window* window = mWindow;

	mWindow->pushGui(new GuiLoading<std::vector<PacmanPackage>>(mWindow, _("PLEASE WAIT"),
		[this, window, updatePackageList]
		{	
			if (updatePackageList)
				ApiSystem::getInstance()->updateBatoceraStorePackageList();

			auto packs = ApiSystem::getInstance()->getBatoceraStorePackages();
			return packs;
		},
		[this, window, updatePackageList](std::vector<PacmanPackage> packages)
		{		
			int idx = updatePackageList ? -1 : mMenu.getCursorIndex();			
			mMenu.clear();

			std::unordered_set<std::string> groups;
			for (auto package : packages)
				if (groups.find(package.group) == groups.cend())
					groups.insert(package.group);

			bool hasGroups = (groups.size() > 1);

			if (hasGroups)
				std::sort(packages.begin(), packages.end(), sortPackagesByGroup);

			std::string lastGroup = "-1**ce-fakegroup-c";

			int i = 0;
			for (auto package : packages)
			{
				if (hasGroups && lastGroup != package.group)
				{
					if (package.group.empty())
						mMenu.addGroup(_("MISC"));
					else
					  mMenu.addGroup(_(Utils::String::toUpper(package.group).c_str()));

					i++;
				}

				lastGroup = package.group;

				ComponentListRow row;

				auto grid = std::make_shared<GuiBatoceraStoreEntry>(mWindow, package);
				row.addElement(grid, true);

				if (!grid->isInstallPending())
					row.makeAcceptInputHandler([this, package] { processPackage(package); });

				mMenu.addRow(row, i == idx);
				i++;
			}

			centerWindow();

			mWaitingLoad = false;
		}
	));
}

void GuiBatoceraStore::processPackage(PacmanPackage package)
{	
	if (package.name.empty())
		return;

	GuiSettings* msgBox = new GuiSettings(mWindow, package.name);
	msgBox->setSubTitle(package.description);
	msgBox->setTag("popup");
	
	if (package.isInstalled())
	{
		msgBox->addEntry(_U("\uF019 ") + _("UPDATE"), false, [this, msgBox, package]
		{
			char trstring[1024];
			snprintf(trstring, 1024, _("'%s' ADDED TO DOWNLOAD QUEUE").c_str(), package.name.c_str()); // batocera
			mWindow->displayNotificationMessage(_U("\uF019 ") + std::string(trstring));

			ContentInstaller::Enqueue(mWindow, ContentInstaller::CONTENT_STORE_INSTALL, package.name);
			msgBox->close();

			auto pThis = this;
			msgBox->close();
			pThis->loadPackages();
		});

		msgBox->addEntry(_U("\uF014 ") + _("REMOVE"), false, [this, msgBox, package]
		{
			mWindow->displayNotificationMessage(_U("\uF014 ") + _("UNINSTALL ADDED TO QUEUE"));

			ContentInstaller::Enqueue(mWindow, ContentInstaller::CONTENT_STORE_UNINSTALL, package.name);			
			
			auto pThis = this;
			msgBox->close();
			pThis->loadPackages();
		});
	}
	else
	{
		msgBox->addEntry(_U("\uF019 ") + _("INSTALL"), false, [this, msgBox, package]
		{
			char trstring[1024];
			snprintf(trstring, 1024, _("'%s' ADDED TO DOWNLOAD QUEUE").c_str(), package.name.c_str()); // batocera
			mWindow->displayNotificationMessage(_U("\uF019 ") + std::string(trstring));

			ContentInstaller::Enqueue(mWindow, ContentInstaller::CONTENT_STORE_INSTALL, package.name);
			
			auto pThis = this;
			msgBox->close();
			pThis->loadPackages();
		});
	}

	mWindow->pushGui(msgBox);
}

bool GuiBatoceraStore::input(InputConfig* config, Input input)
{
	if (mWaitingLoad)
		return true;

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

std::vector<HelpPrompt> GuiBatoceraStore::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	return prompts;
}

//////////////////////////////////////////////////////////////
// GuiBatoceraStoreEntry
//////////////////////////////////////////////////////////////

GuiBatoceraStoreEntry::GuiBatoceraStoreEntry(Window* window, PacmanPackage& entry) :
	ComponentGrid(window, Vector2i(4, 3))
{
	mEntry = entry;

	auto theme = ThemeData::getMenuTheme();

	bool isInstalled = entry.isInstalled();

	mIsPending = ContentInstaller::IsInQueue(ContentInstaller::CONTENT_STORE_INSTALL, entry.name) || ContentInstaller::IsInQueue(ContentInstaller::CONTENT_STORE_UNINSTALL, entry.name);

	mImage = std::make_shared<TextComponent>(mWindow);
	mImage->setColor(theme->Text.color);
	if (!isInstalled)
		mImage->setOpacity(192);

	mImage->setHorizontalAlignment(Alignment::ALIGN_CENTER);
	mImage->setVerticalAlignment(Alignment::ALIGN_TOP);
	mImage->setFont(theme->Text.font);
	mImage->setText(isInstalled ? _U("\uF058") : _U("\uF019"));	
	mImage->setSize(theme->Text.font->getLetterHeight() * 1.5f, 0);

	std::string label = entry.description;

	mText = std::make_shared<TextComponent>(mWindow, label, theme->Text.font, theme->Text.color);
	mText->setLineSpacing(1.5);
	mText->setVerticalAlignment(ALIGN_TOP);

	std::string details =
		_U("\uf0C5  ") + entry.name +
		_U("  \uf085  ") + entry.available_version +
		_U("  \uf007  ") + entry.packager;
		
	if (!entry.repository.empty() && entry.repository != "batocera")
		details = details + _U("  \uf114  ") + entry.repository;

	if (entry.installed_size > 0)
		details = details + _U("  \uf0A0  ") + Utils::FileSystem::megaBytesToString(entry.installed_size / 1024.0);
	else if (entry.download_size > 0)
		details = details + _U("  \uf0A0  ") + Utils::FileSystem::megaBytesToString(entry.download_size / 1024.0);

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

void GuiBatoceraStoreEntry::setColor(unsigned int color)
{
	mImage->setColor(color);
	mText->setColor(color);
	mSubstring->setColor(color);
}


