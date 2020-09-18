#include "GuiGamelistOptions.h"
#include "guis/GuiGamelistFilter.h"
#include "scrapers/Scraper.h"
#include "views/gamelist/IGameListView.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "components/SwitchComponent.h"
#include "CollectionSystemManager.h"
#include "FileFilterIndex.h"
#include "FileSorts.h"
#include "GuiMetaDataEd.h"
#include "SystemData.h"
#include "LocaleES.h"
#include "guis/GuiMenu.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "scrapers/ThreadedScraper.h"
#include "ThreadedHasher.h"
#include "guis/GuiMenu.h"
#include "ApiSystem.h"
#include "guis/GuiImageViewer.h"

std::vector<std::string> GuiGamelistOptions::gridSizes {
	"automatic", "1x1",
	"2x1", "2x2", "2x3", "2x4", "2x5", "2x6", "2x7",
	"3x1", "3x2", "3x3", "3x4", "3x5", "3x6", "3x7",
	"4x1", "4x2", "4x3", "4x4", "4x5", "4x6", "4x7",
	"5x1", "5x2", "5x3", "5x4", "5x5", "5x6", "5x7",
	"6x1", "6x2", "6x3", "6x4", "6x5", "6x6", "6x7",
	"7x1", "7x2", "7x3", "7x4", "7x5", "7x6", "7x7"
};

GuiGamelistOptions::GuiGamelistOptions(Window* window, SystemData* system, bool showGridFeatures) : GuiComponent(window),
	mSystem(system), mMenu(window, "OPTIONS"), fromPlaceholder(false), mFiltersChanged(false), mReloadAll(false)
{
	mGridSize = nullptr;

	addChild(&mMenu);

	mMenu.addButton(_("BACK"), _("go back"), [this] { delete this; });

	// check it's not a placeholder folder - if it is, only show "Filter Options"
	FileData* file = getGamelist()->getCursor();
	fromPlaceholder = file->isPlaceHolder();

	std::map<std::string, CollectionSystemData> customCollections = CollectionSystemManager::get()->getCustomCollectionSystems();
	auto customCollection = customCollections.find(getCustomCollectionName());

	auto theme = ThemeData::getMenuTheme();

	mMenu.addGroup(_("NAVIGATION"));

	if (!Settings::getInstance()->getBool("ForceDisableFilters"))
		if (customCollection == customCollections.cend() || customCollection->second.filteredIndex == nullptr)
			addTextFilterToMenu();

	ComponentListRow row;

	if (!fromPlaceholder)
	{
		// jump to letter
		row.elements.clear();

		std::vector<std::string> letters = getGamelist()->getEntriesLetters();
		if (!letters.empty())
		{
			mJumpToLetterList = std::make_shared<LetterList>(mWindow, _("JUMP TO..."), false); // batocera

			char curChar = (char)toupper(getGamelist()->getCursor()->getName()[0]);

			if (std::find(letters.begin(), letters.end(), std::string(1, curChar)) == letters.end())
				curChar = letters.at(0)[0];

			for (auto letter : letters)
				mJumpToLetterList->add(letter, letter[0], letter[0] == curChar);

			row.addElement(std::make_shared<TextComponent>(mWindow, _("JUMP TO..."), theme->Text.font, theme->Text.color), true); // batocera
			row.addElement(mJumpToLetterList, false);
			row.input_handler = [&](InputConfig* config, Input input)
			{
				if (config->isMappedTo(BUTTON_OK, input) && input.value)
				{
					jumpToLetter();
					return true;
				}
				else if (mJumpToLetterList->input(config, input))
				{
					return true;
				}
				return false;
			};
			mMenu.addRow(row);
		}
	}

	// sort list by
	unsigned int currentSortId = mSystem->getSortId();
	if (currentSortId > FileSorts::getSortTypes().size())
		currentSortId = 0;

	mListSort = std::make_shared<SortList>(mWindow, _("SORT GAMES BY"), false);
	for(unsigned int i = 0; i < FileSorts::getSortTypes().size(); i++)
	{
		const FileSorts::SortType& sort = FileSorts::getSortTypes().at(i);
		mListSort->add(sort.icon + sort.description, sort.id, sort.id == currentSortId); // TODO - actually make the sort type persistent
	}

	mMenu.addWithLabel(_("SORT GAMES BY"), mListSort); // batocera	

	// Show filtered menu
	if (!Settings::getInstance()->getBool("ForceDisableFilters"))
	{
		if (customCollection == customCollections.cend() || customCollection->second.filteredIndex == nullptr)
			mMenu.addEntry(_("OTHER FILTERS"), true, std::bind(&GuiGamelistOptions::openGamelistFilter, this));
	}


	// Game medias
	bool hasManual = ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::PDFEXTRACTION) && Utils::FileSystem::exists(file->getMetadata(MetaDataId::Manual));
	bool hasMap = Utils::FileSystem::exists(file->getMetadata(MetaDataId::Map));

	if (hasManual || hasMap)
	{
		mMenu.addGroup(_("GAME MEDIAS"));

		if (hasManual)
		{
			mMenu.addEntry(_("VIEW GAME MANUAL"), false, [window, file, this]
			{
				GuiImageViewer::showPdf(window, file->getMetadata(MetaDataId::Manual));
				delete this;
			});
		}

		if (hasMap)
		{
			mMenu.addEntry(_("VIEW GAME MAP"), false, [window, file, this]
			{
				GuiImageViewer::showImage(window, file->getMetadata(MetaDataId::Map));
				delete this;
			});
		}
	}




	if (customCollection != customCollections.cend() && customCollection->second.filteredIndex != nullptr)
	{
		mMenu.addGroup(_("COLLECTION"));
		mMenu.addEntry(_("EDIT DYNAMIC COLLECTION FILTERS"), false, std::bind(&GuiGamelistOptions::editCollectionFilters, this));
		mMenu.addEntry(_("DELETE COLLECTION"), false, std::bind(&GuiGamelistOptions::deleteCollection, this));
	}
	else if ((!mSystem->isCollection() || mSystem->getName() == "all") && mSystem->getIndex(false) != nullptr)
	{
		mMenu.addGroup(_("COLLECTION"));
		mMenu.addEntry(_("CREATE NEW DYNAMIC COLLECTION"), false, std::bind(&GuiGamelistOptions::createNewCollectionFilter, this));
	}	

	auto glv = ViewController::get()->getGameListView(system);
	std::string viewName = glv->getName();

	// GameList view style
	mViewMode = std::make_shared< OptionListComponent<std::string> >(mWindow, _("GAMELIST VIEW STYLE"), false);
	std::vector<std::pair<std::string, std::string>> styles;
	styles.push_back(std::pair<std::string, std::string>("automatic", _("automatic")));

	auto mViews = system->getTheme()->getViewsOfTheme();

	bool showViewStyle = mViews.size() > 2;

	for (auto it = mViews.cbegin(); it != mViews.cend(); ++it)
	{
		if (it->first == "basic" || it->first == "detailed" || it->first == "grid")
			styles.push_back(std::pair<std::string, std::string>(it->first, _(it->first.c_str())));
		else
			styles.push_back(*it);
	}

	std::string viewMode = system->getSystemViewMode();

	bool found = false;
	for (auto it = styles.cbegin(); it != styles.cend(); it++)
	{		
		bool sel = (viewMode.empty() && it->first == "automatic") || viewMode == it->first;
		if (sel)
			found = true;

		mViewMode->add(it->second, it->first, sel);
	}

	if (!found)
		mViewMode->selectFirstItem();

	if (UIModeController::getInstance()->isUIModeFull())
	{
		mMenu.addGroup(_("VIEW OPTIONS"));

		if (showViewStyle)
			mMenu.addWithLabel(_("GAMELIST VIEW STYLE"), mViewMode);

		mMenu.addEntry(_("VIEW CUSTOMISATION"), true, [this, system]() 
		{
			GuiMenu::openThemeConfiguration(mWindow, this, nullptr, system->getThemeFolder()); 
		});

		if ((customCollection != customCollections.cend() && customCollection->second.filteredIndex == nullptr) || CollectionSystemManager::get()->isEditing())
		{
			mMenu.addGroup(_("COLLECTION MANAGEMENT"));

			if (customCollection != customCollections.cend())
			{
				mMenu.addEntry(_("ADD/REMOVE GAMES TO THIS GAME COLLECTION"), false, std::bind(&GuiGamelistOptions::startEditMode, this));

				if (mSystem->getName() != CollectionSystemManager::get()->getCustomCollectionsBundle()->getName())
					mMenu.addEntry(_("DELETE COLLECTION"), false, std::bind(&GuiGamelistOptions::deleteCollection, this));
			}

			if (CollectionSystemManager::get()->isEditing())
				mMenu.addEntry(_("FINISH EDITING COLLECTION") + " : " + Utils::String::toUpper(CollectionSystemManager::get()->getEditingCollection()), false, std::bind(&GuiGamelistOptions::exitEditMode, this));
		}

		if (file->getType() == FOLDER && ((FolderData*) file)->isVirtualStorage())
			fromPlaceholder = true;
		else if (file->getType() == FOLDER && mSystem->getName() == CollectionSystemManager::get()->getCustomCollectionsBundle()->getName())
			fromPlaceholder = true;
		
		if (!fromPlaceholder)
		{
			mMenu.addGroup(_("GAME OPTIONS"));

			if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
			{
				auto srcSystem = file->getSourceFileData()->getSystem();
				auto sysOptions = mSystem->isGroupSystem() ? srcSystem : mSystem;

				if (sysOptions->hasFeatures() || sysOptions->hasEmulatorSelection())
					mMenu.addEntry(_("ADVANCED SYSTEM OPTIONS"), true, [this, sysOptions] { GuiMenu::popSystemConfigurationGui(mWindow, sysOptions); });

				if (file->getType() != FOLDER)
				{
					if (srcSystem->hasFeatures() || srcSystem->hasEmulatorSelection())
						mMenu.addEntry(_("ADVANCED GAME OPTIONS"), true, [this, file] { GuiMenu::popGameConfigurationGui(mWindow, file); });
				}
			}

			if (file->getType() == FOLDER)
				mMenu.addEntry(_("EDIT FOLDER METADATA"), true, std::bind(&GuiGamelistOptions::openMetaDataEd, this));
			else
				mMenu.addEntry(_("EDIT THIS GAME'S METADATA"), true, std::bind(&GuiGamelistOptions::openMetaDataEd, this));
		}
	}
	
	mMenu.setMaxHeight(Renderer::getScreenHeight() * 0.85f);
	// center the menu
	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	mMenu.animateTo(Vector2f((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2));
}

void GuiGamelistOptions::addTextFilterToMenu()
{
	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	auto idx = mSystem->getIndex(false);

	if (idx != nullptr && idx->isFiltered())
	{
		mMenu.addEntry(_("RESET FILTERS"), false, [this]
		{
			mSystem->deleteIndex();
			mFiltersChanged = true;
			delete this;
		});
	}

	ComponentListRow row;
	
	auto lbl = std::make_shared<TextComponent>(mWindow, _("FILTER GAMES BY TEXT"), font, color);
	row.addElement(lbl, true); // label

	std::string searchText;
	
	if (idx != nullptr)
		searchText = idx->getTextFilter();

	mTextFilter = std::make_shared<TextComponent>(mWindow, searchText, font, color, ALIGN_RIGHT);
	row.addElement(mTextFilter, true);

	auto spacer = std::make_shared<GuiComponent>(mWindow);
	spacer->setSize(Renderer::getScreenWidth() * 0.005f, 0);
	row.addElement(spacer, false);

	auto bracket = std::make_shared<ImageComponent>(mWindow);

	auto searchIcon = theme->getMenuIcon("searchIcon");
	bracket->setImage(searchIcon.empty() ? ":/search.svg" : searchIcon);

	bracket->setResize(Vector2f(0, lbl->getFont()->getLetterHeight()));
	row.addElement(bracket, false);

	auto updateVal = [this](const std::string& newVal)
	{
		mTextFilter->setValue(Utils::String::toUpper(newVal));

		auto index = mSystem->getIndex(!newVal.empty());
		if (index != nullptr)
		{
			mFiltersChanged = true;

			index->setTextFilter(newVal);
			if (!index->isFiltered())
				mSystem->deleteIndex();

			delete this;
		}
	};

	row.makeAcceptInputHandler([this, updateVal]
	{
		if (Settings::getInstance()->getBool("UseOSK"))
			mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, _("FILTER GAMES BY TEXT"), mTextFilter->getValue(), updateVal, false));
		else
			mWindow->pushGui(new GuiTextEditPopup(mWindow, _("FILTER GAMES BY TEXT"), mTextFilter->getValue(), updateVal, false));
	});

	mMenu.addRow(row);
}

GuiGamelistOptions::~GuiGamelistOptions()
{
	if (mSystem == nullptr)
		return;

	for (auto it = mSaveFuncs.cbegin(); it != mSaveFuncs.cend(); it++)
		(*it)();

	// apply sort
	if (!fromPlaceholder && mListSort->getSelected() != mSystem->getSortId())
	{
		mSystem->setSortId(mListSort->getSelected());
		
		FolderData* root = mSystem->getRootFolder();
		/*
		const FolderData::SortType& sort = FileSorts::getSortTypes().at(mListSort->getSelected());
		root->sort(sort);
		*/
		// notify that the root folder was sorted
		getGamelist()->onFileChanged(root, FILE_SORTED);
	}

	Vector2f gridSizeOverride(0, 0);

	if (mGridSize != NULL)
	{
		auto str = mGridSize->getSelected();

		size_t divider = str.find('x');
		if (divider != std::string::npos)
		{
			std::string first = str.substr(0, divider);
			std::string second = str.substr(divider + 1, std::string::npos);

			gridSizeOverride = Vector2f((float)atof(first.c_str()), (float)atof(second.c_str()));
		}
		else
			gridSizeOverride = mSystem->getGridSizeOverride();
	}
	else
		gridSizeOverride = mSystem->getGridSizeOverride();

	std::string viewMode = mViewMode->getSelected();

	if (mSystem->getSystemViewMode() != (viewMode == "automatic" ? "" : viewMode))
	{
		for (auto sm : Settings::getInstance()->getStringMap())
			if (Utils::String::startsWith(sm.first, "subset." + mSystem->getThemeFolder() + "."))
				Settings::getInstance()->setString(sm.first, "");
	}

	bool viewModeChanged = mSystem->setSystemViewMode(viewMode, gridSizeOverride);

	Settings::getInstance()->saveFile();

	if (mReloadAll)
	{
		mWindow->renderSplashScreen(_("Loading..."));
		ViewController::get()->reloadAll(mWindow);
		mWindow->closeSplashScreen();
	}
	else if (mFiltersChanged || viewModeChanged)
	{
		if (viewModeChanged)
			mSystem->loadTheme();

		if (!viewModeChanged && mSystem->isCollection())
			CollectionSystemManager::get()->reloadCollection(getCustomCollectionName());
		else
			ViewController::get()->reloadGameListView(mSystem, false);
	}
}

void GuiGamelistOptions::openGamelistFilter()
{
	mReloadAll = false;
	mFiltersChanged = true;
	GuiGamelistFilter* ggf = new GuiGamelistFilter(mWindow, mSystem);
	mWindow->pushGui(ggf);
}

std::string GuiGamelistOptions::getCustomCollectionName()
{
	std::string editingSystem = mSystem->getName();

	// need to check if we're editing the collections bundle, as we will want to edit the selected collection within
	if (editingSystem == CollectionSystemManager::get()->getCustomCollectionsBundle()->getName())
	{
		FileData* file = getGamelist()->getCursor();
		// do we have the cursor on a specific collection?
		if (file->getType() == FOLDER)
			return file->getName();

		return file->getSystem()->getName();
	}

	return editingSystem;
}

void GuiGamelistOptions::startEditMode()
{
	CollectionSystemManager::get()->setEditMode(getCustomCollectionName());
	delete this;
}

void GuiGamelistOptions::exitEditMode()
{
	CollectionSystemManager::get()->exitEditMode();
	delete this;
}

void GuiGamelistOptions::openMetaDataEd()
{
	if (ThreadedScraper::isRunning() || ThreadedHasher::isRunning())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("THIS FUNCTION IS DISABLED WHEN SCRAPING IS RUNNING")));
		return;
	}

	// open metadata editor
	// get the FileData that hosts the original metadata
	FileData* file = getGamelist()->getCursor()->getSourceFileData();
	ScraperSearchParams p;
	p.game = file;
	p.system = file->getSystem();

	std::function<void()> deleteBtnFunc = nullptr;

	SystemData* system = file->getSystem();
	if (system->isGroupChildSystem())
		system = system->getParentGroupSystem();

	if (file->getType() == GAME)
	{
		deleteBtnFunc = [this, file, system]
		{
			auto pThis = this;

			CollectionSystemManager::get()->deleteCollectionFiles(file);
			file->deleteGameFiles();

			auto view = ViewController::get()->getGameListView(system, false);
			if (view != nullptr)
				view.get()->remove(file);
			else
				delete file;

			delete pThis;
		};
	}

	mWindow->pushGui(new GuiMetaDataEd(mWindow, &file->getMetadata(), file->getMetadata().getMDD(), p, Utils::FileSystem::getFileName(file->getPath()),
		std::bind(&IGameListView::onFileChanged, ViewController::get()->getGameListView(system).get(), file, FILE_METADATA_CHANGED), deleteBtnFunc, file));
}

void GuiGamelistOptions::jumpToLetter()
{
	char letter = mJumpToLetterList->getSelected();
	IGameListView* gamelist = getGamelist();

	if (mListSort->getSelected() != 0)
	{
		mListSort->selectFirstItem();
		mSystem->setSortId(0);
		
		FolderData* root = mSystem->getRootFolder();
		/*
		const FolderData::SortType& sort = FileSorts::getSortTypes().at(0);
		root->sort(sort);
		*/
		getGamelist()->onFileChanged(root, FILE_SORTED);
	}

	// this is a really shitty way to get a list of files
	const std::vector<FileData*>& files = gamelist->getCursor()->getParent()->getChildrenListToDisplay();

	long min = 0;
	long max = (long)files.size() - 1;
	long mid = 0;

	while(max >= min)
	{
		mid = ((max - min) / 2) + min;

		// game somehow has no first character to check
		if(files.at(mid)->getName().empty())
			continue;

		char checkLetter = (char)toupper(files.at(mid)->getName()[0]);

		if(checkLetter < letter)
			min = mid + 1;
		else if(checkLetter > letter || (mid > 0 && (letter == toupper(files.at(mid - 1)->getName()[0]))))
			max = mid - 1;
		else
			break; //exact match found
	}

	gamelist->setCursor(files.at(mid));

	delete this;
}

bool GuiGamelistOptions::input(InputConfig* config, Input input)
{
	if ((config->isMappedTo(BUTTON_BACK, input) || config->isMappedTo("select", input)) && input.value)
	{
		delete this;
		return true;
	}

	return mMenu.input(config, input);
}

HelpStyle GuiGamelistOptions::getHelpStyle()
{
	HelpStyle style = HelpStyle();
	style.applyTheme(mSystem->getTheme(), "system");
	return style;
}

std::vector<HelpPrompt> GuiGamelistOptions::getHelpPrompts()
{
	auto prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("CLOSE")));
	return prompts;
}

IGameListView* GuiGamelistOptions::getGamelist()
{
	return ViewController::get()->getGameListView(mSystem).get();
}

void GuiGamelistOptions::editCollectionFilters()
{
	std::map<std::string, CollectionSystemData> customCollections = CollectionSystemManager::get()->getCustomCollectionSystems();
	auto customCollection = customCollections.find(getCustomCollectionName());
	if (customCollection == customCollections.cend())
		return;

	if (customCollection->second.filteredIndex == nullptr)
		return;

	mReloadAll = false;
	mFiltersChanged = true;
	GuiGamelistFilter* ggf = new GuiGamelistFilter(mWindow, customCollection->second.filteredIndex);
	mWindow->pushGui(ggf);
}

void GuiGamelistOptions::createNewCollectionFilter()
{
	std::string defName = Utils::String::toLower(mSystem->getIndex(false)->getTextFilter());

	if (Settings::getInstance()->getBool("UseOSK"))
		mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, _("New Collection Name"), defName, [this](std::string val) { createCollection(val); }, false));
	else
		mWindow->pushGui(new GuiTextEditPopup(mWindow, _("New Collection Name"), defName, [this](std::string val) { createCollection(val); }, false));

}

void GuiGamelistOptions::createCollection(std::string inName)
{
	std::string name = CollectionSystemManager::get()->getValidNewCollectionName(inName);

	std::string setting = Settings::getInstance()->getString("CollectionSystemsCustom");
	setting = setting.empty() ? name : setting + "," + name;
	Settings::getInstance()->setString("CollectionSystemsCustom", setting);

	CollectionFilter cf;
	cf.createFromSystem(name, mSystem);
	cf.save();

	SystemData* newSys = CollectionSystemManager::get()->addNewCustomCollection(name);

	mReloadAll = false;
	mFiltersChanged = false;

	Window* window = mWindow;

	window->renderSplashScreen();

	GuiComponent* topGui = window->peekGui();
	window->removeGui(topGui);

	while (window->peekGui() && window->peekGui() != ViewController::get())
		delete window->peekGui();

	CollectionSystemManager::get()->loadEnabledListFromSettings();
	CollectionSystemManager::get()->updateSystemsList();
	ViewController::get()->goToStart();
	ViewController::get()->reloadAll();

	ViewController::get()->goToSystemView(newSys);

	window->closeSplashScreen();
}

void GuiGamelistOptions::deleteCollection()
{
	if (getCustomCollectionName() == CollectionSystemManager::get()->getCustomCollectionsBundle()->getName())
		return;

	mWindow->pushGui(new GuiMsgBox(mWindow, _("ARE YOU SURE ?"), _("YES"),
		[this]
		{
			std::map<std::string, CollectionSystemData> customCollections = CollectionSystemManager::get()->getCustomCollectionSystems();
			auto customCollection = customCollections.find(getCustomCollectionName());
			if (customCollection == customCollections.cend())
				return;

			if (CollectionSystemManager::get()->deleteCustomCollection(&customCollection->second))
			{
				mWindow->renderSplashScreen();
		
				CollectionSystemManager::get()->loadEnabledListFromSettings();
				CollectionSystemManager::get()->updateSystemsList();
				ViewController::get()->goToStart();
				ViewController::get()->reloadAll();

				mWindow->closeSplashScreen();
				delete this;
			}
		}, _("NO"), nullptr));
}
