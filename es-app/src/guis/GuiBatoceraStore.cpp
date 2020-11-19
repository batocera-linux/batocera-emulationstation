#include "guis/GuiBatoceraStore.h"

#include "ApiSystem.h"
#include "components/OptionListComponent.h"
#include "guis/GuiSettings.h"
#include "views/ViewController.h"
#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"
#include "components/ComponentGrid.h"
#include "components/ButtonComponent.h"
#include "components/MultiLineMenuEntry.h"
#include "LocaleES.h"
#include "guis/GuiMsgBox.h"
#include "ContentInstaller.h"
#include "GuiLoading.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiTextEditPopupKeyboard.h"

#include <unordered_set>
#include <algorithm>

#define WINDOW_WIDTH (float)Math::max((int)Renderer::getScreenHeight(), (int)(Renderer::getScreenWidth() * 0.73f))

GuiBatoceraStore::GuiBatoceraStore(Window* window)
	: GuiComponent(window), mGrid(window, Vector2i(1, 4)), mBackground(window, ":/frame.png")
{
	mReloadList = 1;

	addChild(&mBackground);
	addChild(&mGrid);

	// Form background
	auto theme = ThemeData::getMenuTheme();
	mBackground.setImagePath(theme->Background.path);
	mBackground.setEdgeColor(theme->Background.color);
	mBackground.setCenterColor(theme->Background.centerColor);
	mBackground.setCornerSize(theme->Background.cornerSize);

	// Title
	mHeaderGrid = std::make_shared<ComponentGrid>(mWindow, Vector2i(1, 5));

	mTitle = std::make_shared<TextComponent>(mWindow, _("CONTENT DOWNLOADER"), theme->Title.font, theme->Title.color, ALIGN_CENTER); // batocera
	mSubtitle = std::make_shared<TextComponent>(mWindow, _("SELECT CONTENT TO INSTALL / REMOVE"), theme->TextSmall.font, theme->TextSmall.color, ALIGN_CENTER);
	mHeaderGrid->setEntry(mTitle, Vector2i(0, 1), false, true);
	mHeaderGrid->setEntry(mSubtitle, Vector2i(0, 3), false, true);

	mGrid.setEntry(mHeaderGrid, Vector2i(0, 0), false, true);

	// Tabs
	mTabs = std::make_shared<ComponentTab>(mWindow);
	mGrid.setEntry(mTabs, Vector2i(0, 1), false, true);	

	// Entries
	mList = std::make_shared<ComponentList>(mWindow);
	mGrid.setEntry(mList, Vector2i(0, 2), true, true);

	// Buttons
	std::vector< std::shared_ptr<ButtonComponent> > buttons;
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("SEARCH"), _("SEARCH"), [this] {  showSearch(); }));
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("REFRESH"), _("REFRESH"), [this] {  loadPackagesAsync(true, true); }));
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("UPDATE INSTALLED CONTENT"), _("UPDATE INSTALLED CONTENT"), [this] {  loadPackagesAsync(true, false); }));
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

GuiBatoceraStore::~GuiBatoceraStore()
{
	ContentInstaller::UnregisterNotify(this);
}

void GuiBatoceraStore::OnContentInstalled(int contentType, std::string contentName, bool success)
{
	if (contentType == ContentInstaller::CONTENT_STORE_INSTALL || contentType == ContentInstaller::CONTENT_STORE_UNINSTALL)
		mReloadList = 2;
}

void GuiBatoceraStore::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mReloadList != 0)
	{
		bool refreshPackages = mReloadList == 2;
		bool silent = (mReloadList == 2 || mReloadList == 3);
		bool restoreIndex = (mReloadList != 3);		
		mReloadList = 0;

		if (silent)
		{
			if (refreshPackages)
				mPackages = queryPackages();

			if (!restoreIndex)
				mWindow->postToUiThread([this](Window* w) { loadList(false, false); });
			else 
				loadList(false, restoreIndex);
		}
		else 
			loadPackagesAsync(false, true);
	}
}

void GuiBatoceraStore::onSizeChanged()
{
	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));

	mGrid.setSize(mSize);

	const float titleHeight = mTitle->getFont()->getLetterHeight();
	const float subtitleHeight = mSubtitle->getFont()->getLetterHeight();
	const float titleSubtitleSpacing = mSize.y() * 0.03f;

	mGrid.setRowHeightPerc(0, (titleHeight + titleSubtitleSpacing + subtitleHeight + TITLE_VERT_PADDING) / mSize.y());

	if (mTabs->size() == 0)
		mGrid.setRowHeightPerc(1, 0.00001f);
	else 
		mGrid.setRowHeightPerc(1, (titleHeight + titleSubtitleSpacing) / mSize.y());

	mGrid.setRowHeightPerc(3, mButtonGrid->getSize().y() / mSize.y());

	mHeaderGrid->setRowHeightPerc(1, titleHeight / mHeaderGrid->getSize().y());
	mHeaderGrid->setRowHeightPerc(2, titleSubtitleSpacing / mHeaderGrid->getSize().y());
	mHeaderGrid->setRowHeightPerc(3, subtitleHeight / mHeaderGrid->getSize().y());
}

void GuiBatoceraStore::centerWindow()
{
	if (Renderer::isSmallScreen())
		setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	else
		setSize(WINDOW_WIDTH, Renderer::getScreenHeight() * 0.875f);

	setPosition((Renderer::getScreenWidth() - getSize().x()) / 2, (Renderer::getScreenHeight() - getSize().y()) / 2);
}

static bool sortPackagesByGroup(PacmanPackage& sys1, PacmanPackage& sys2)
{
	if (sys1.group == sys2.group)
		return Utils::String::compareIgnoreCase(sys1.description, sys2.description) < 0;		

	return sys1.group.compare(sys2.group) < 0;
}

void GuiBatoceraStore::loadList(bool updatePackageList, bool restoreIndex)
{
	int idx = updatePackageList || !restoreIndex ? -1 : mList->getCursorIndex();
	mList->clear();

	std::unordered_set<std::string> repositories;
	for (auto& package : mPackages)
		if (repositories.find(package.repository) == repositories.cend())
			repositories.insert(package.repository);

	if (repositories.size() > 0 && mTabs->size() == 0 && mTabFilter.empty())
	{
		std::vector<std::string> gps;
		for (auto gp : repositories)
			gps.push_back(gp);

		std::sort(gps.begin(), gps.end());
		for (auto group : gps)
			mTabs->addTab(group);

		mTabFilter = gps[0];

		mTabs->setCursorChangedCallback([&](const CursorState& /*state*/)
		{
			if (mTabFilter != mTabs->getSelected())
			{
				mTabFilter = mTabs->getSelected();
				mWindow->postToUiThread([this](Window* w) 
				{ 
					mReloadList = 3;					
				});
			}
		});
	}
	
	std::string lastGroup = "-1**ce-fakegroup-c";

	int i = 0;
	for (auto package : mPackages)
	{
		if (!mTabFilter.empty() && package.repository != mTabFilter)				
			continue;		

		if (!mTextFilter.empty() && !Utils::String::containsIgnoreCase(package.name, mTextFilter))
			continue;

		if (lastGroup != package.group)
			mList->addGroup(package.group, false);

		lastGroup = package.group;

		ComponentListRow row;

		auto grid = std::make_shared<GuiBatoceraStoreEntry>(mWindow, package);
		row.addElement(grid, true);

		if (!grid->isInstallPending())
			row.makeAcceptInputHandler([this, package] { processPackage(package); });

		mList->addRow(row, i == idx, false);
		i++;
	}

	if (i == 0)
	{
		auto theme = ThemeData::getMenuTheme();
		ComponentListRow row;
		row.selectable = false;
		auto text = std::make_shared<TextComponent>(mWindow, _("There are no items in this view"), theme->TextSmall.font, theme->Text.color, ALIGN_CENTER);
		row.addElement(text, true, false);
		mList->addRow(row, false, false);
	}

	centerWindow();

	mReloadList = 0;
}

std::vector<PacmanPackage> GuiBatoceraStore::queryPackages()
{
	auto systemNames = SystemData::getKnownSystemNames();
	auto packages = ApiSystem::getInstance()->getBatoceraStorePackages();

	std::vector<PacmanPackage> copy;
	for (auto& package : packages)
	{
		if (package.group.empty())
			package.group = _("MISC");
		else
		{
			if (Utils::String::startsWith(package.group, "sys-"))
			{
				auto sysName = package.group.substr(4);

				auto itSys = systemNames.find(sysName);
				if (itSys == systemNames.cend())
					continue;

				package.group = Utils::String::toUpper(itSys->second);
			}
			else
				package.group = _(Utils::String::toUpper(package.group).c_str());
		}

		copy.push_back(package);
	}

	std::sort(copy.begin(), copy.end(), sortPackagesByGroup);
	return copy;
}

void GuiBatoceraStore::loadPackagesAsync(bool updatePackageList, bool refreshOnly)
{
	Window* window = mWindow;

	mWindow->pushGui(new GuiLoading<std::vector<PacmanPackage>>(mWindow, _("PLEASE WAIT"),
		[this,  updatePackageList, refreshOnly]
		{	
			if (updatePackageList)
				if (refreshOnly)
					ApiSystem::getInstance()->refreshBatoceraStorePackageList();
				else
					ApiSystem::getInstance()->updateBatoceraStorePackageList();

			return queryPackages();
		},
		[this, updatePackageList](std::vector<PacmanPackage> packages)
		{		
			mPackages = packages;
			loadList(updatePackageList);
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
			mReloadList = 2;

			msgBox->close();
		});

		msgBox->addEntry(_U("\uF014 ") + _("REMOVE"), false, [this, msgBox, package]
		{
			mWindow->displayNotificationMessage(_U("\uF014 ") + _("UNINSTALL ADDED TO QUEUE"));

			ContentInstaller::Enqueue(mWindow, ContentInstaller::CONTENT_STORE_UNINSTALL, package.name);			
			mReloadList = 2;

			msgBox->close();
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
			mReloadList = 2;

			msgBox->close();
		});
	}

	mWindow->pushGui(msgBox);
}

bool GuiBatoceraStore::input(InputConfig* config, Input input)
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

	if (config->isMappedTo("y", input) && input.value != 0)
	{
		showSearch();
		return true;
	}

	if (mTabs->input(config, input))
		return true;

	return false;
}

void GuiBatoceraStore::showSearch()
{
	auto updateVal = [this](const std::string& newVal)
	{
		mTextFilter = newVal;
		mReloadList = 3;
	};

	if (Settings::getInstance()->getBool("UseOSK"))
		mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, _("SEARCH"), mTextFilter, updateVal, false));
	else
		mWindow->pushGui(new GuiTextEditPopup(mWindow, _("SEARCH"), mTextFilter, updateVal, false));
}

std::vector<HelpPrompt> GuiBatoceraStore::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mList->getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("y", _("SEARCH")));
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


