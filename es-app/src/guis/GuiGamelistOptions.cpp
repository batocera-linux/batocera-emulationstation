#include "GuiGamelistOptions.h"
#include "guis/GuiGamelistFilter.h"
#include "scrapers/Scraper.h"
#include "views/gamelist/IGameListView.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
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
#include "views/SystemView.h"
#include "GuiGameAchievements.h"
#include "guis/GuiGameOptions.h"
#include "views/gamelist/ISimpleGameListView.h"

std::vector<std::string> GuiGamelistOptions::gridSizes {
	"automatic", 
	"1x1", "1x2", "1x3", "1x4", "1x5", "1x6", "1x7",
	"2x1", "2x2", "2x3", "2x4", "2x5", "2x6", "2x7",
	"3x1", "3x2", "3x3", "3x4", "3x5", "3x6", "3x7",
	"4x1", "4x2", "4x3", "4x4", "4x5", "4x6", "4x7",
	"5x1", "5x2", "5x3", "5x4", "5x5", "5x6", "5x7",
	"6x1", "6x2", "6x3", "6x4", "6x5", "6x6", "6x7",
	"7x1", "7x2", "7x3", "7x4", "7x5", "7x6", "7x7"
};

GuiGamelistOptions::GuiGamelistOptions(Window* window, IGameListView* gamelist, SystemData* system, bool showGridFeatures) : GuiComponent(window),
	mSystem(system), mMenu(window, _("VIEW OPTIONS")), fromPlaceholder(false), mFiltersChanged(false), mReloadAll(false), mGamelist(nullptr)
{
	auto idx = system->getIndex(false);

	bool isInRelevancyMode = (idx != nullptr && idx->hasRelevency());
	if (isInRelevancyMode)
		mGamelist = gamelist;

	mGridSize = nullptr;

	addChild(&mMenu);

	mMenu.addButton(_("BACK"), _("go back"), [this] { delete this; });

	// check it's not a placeholder folder - if it is, only show "Filter Options"
	FileData* file = getGamelist()->getCursor();
	fromPlaceholder = file->isPlaceHolder();

	std::map<std::string, CollectionSystemData> customCollections = CollectionSystemManager::get()->getCustomCollectionSystems();
	auto customCollection = customCollections.find(getCustomCollectionName());

	auto theme = ThemeData::getMenuTheme();

	if (!isInRelevancyMode)
	{
		mMenu.addGroup(_("NAVIGATION"));

		if (!Settings::getInstance()->getBool("ForceDisableFilters"))
		{
			addTextFilterToMenu();

			std::string filterInfo;

			auto idx = mSystem->getIndex(false);
			if (idx != nullptr && idx->isFiltered())
				filterInfo = idx->getDisplayLabel(false);

			if (!filterInfo.empty())
				mMenu.addWithDescription(_("OTHER FILTERS"), filterInfo, nullptr, std::bind(&GuiGamelistOptions::openGamelistFilter, this));
			else
				mMenu.addEntry(_("OTHER FILTERS"), true, std::bind(&GuiGamelistOptions::openGamelistFilter, this));			
		}

		ISimpleGameListView* simpleView = dynamic_cast<ISimpleGameListView*>(getGamelist());
		if (simpleView != nullptr)
		{
			mMenu.addEntry(_("SELECT RANDOM GAME"), false, [this, simpleView]
			{
				simpleView->moveToRandomGame();
				delete this;
			});
		}
		
		if (!fromPlaceholder)
		{
			// jump to letter
			ComponentListRow row;
			row.elements.clear();

			std::vector<std::string> letters = getGamelist()->getEntriesLetters();
			if (!letters.empty())
			{
				mJumpToLetterList = std::make_shared<LetterList>(mWindow, _("JUMP TO GAME BEGINNING WITH THE LETTER"), false);

				
#ifdef _ENABLEEMUELEC				
				unsigned int sortId = system->getSortId();
				std::string cursorName = (sortId == FileSorts::SORTNAME_ASCENDING || sortId == FileSorts::SORTNAME_DESCENDING)
					? getGamelist()->getCursor()->getSortOrName()
					: getGamelist()->getCursor()->getName();
#else 
				const std::string& cursorName = getGamelist()->getCursor()->getName();
#endif
				std::string curChar;

				if (Utils::String::isKorean(cursorName.c_str()))
				{
					const char* koreanLetter = nullptr;

					std::string nameChar = cursorName.substr(0, 3);
					if (!Utils::String::splitHangulSyllable(nameChar.c_str(), &koreanLetter) || !koreanLetter)
						curChar = std::string(letters.at(0)); // Korean supports chosung search only. set default.
					else
						curChar = std::string(koreanLetter, 3);
				}
				else
				{
					curChar = std::string(1, toupper(cursorName[0]));
				}

				if (std::find(letters.begin(), letters.end(), curChar) == letters.end())
					curChar = letters.at(0);

				for (const auto& letter : letters)
					mJumpToLetterList->add(letter, letter, letter == curChar);

				row.addElement(std::make_shared<TextComponent>(mWindow, _("JUMP TO GAME BEGINNING WITH THE LETTER"), theme->Text.font, theme->Text.color), true);
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
		for (unsigned int i = 0; i < FileSorts::getSortTypes().size(); i++)
		{
			const FileSorts::SortType& sort = FileSorts::getSortTypes().at(i);
			mListSort->add(sort.icon + sort.description, sort.id, sort.id == currentSortId); // TODO - actually make the sort type persistent
#ifdef _ENABLEEMUELEC			
			if (i == (FileSorts::getSortTypes().size()-3))
				break;
			if (i == FileSorts::FILENAME_DESCENDING)
			{
			  {
					const FileSorts::SortType& st = FileSorts::getSortTypes().at(FileSorts::SORTNAME_ASCENDING);
					mListSort->add(st.icon + st.description, st.id, st.id == currentSortId);
				}
				{
					const FileSorts::SortType& st = FileSorts::getSortTypes().at(FileSorts::SORTNAME_DESCENDING);
					mListSort->add(st.icon + st.description, st.id, st.id == currentSortId);
				}
			}
#endif
		}

		mMenu.addWithLabel(_("SORT GAMES BY"), mListSort);	
	}

	if (!isInRelevancyMode)
	{
		if (customCollection != customCollections.cend())
		{
			if (customCollection->second.filteredIndex != nullptr)
			{
				mMenu.addGroup(_("DYNAMIC COLLECTION"));

				std::string filterInfo;
				if (customCollection->second.filteredIndex->isFiltered())
					filterInfo = customCollection->second.filteredIndex->getDisplayLabel(true);

				if (!filterInfo.empty())
					mMenu.addWithDescription(_("EDIT DYNAMIC COLLECTION FILTERS"), filterInfo, nullptr, std::bind(&GuiGamelistOptions::editCollectionFilters, this));
				else
					mMenu.addEntry(_("EDIT DYNAMIC COLLECTION FILTERS"), false, std::bind(&GuiGamelistOptions::editCollectionFilters, this));
			}
			else 
				mMenu.addGroup(_("CUSTOM COLLECTION"));

			mMenu.addEntry(_("DELETE COLLECTION"), false, std::bind(&GuiGamelistOptions::deleteCollection, this));
		}
		else if ((!mSystem->isCollection() || mSystem->getName() == "all") && mSystem->getIndex(false) != nullptr)
		{
			mMenu.addGroup(_("COLLECTION"));
			mMenu.addEntry(_("CREATE NEW DYNAMIC COLLECTION"), false, std::bind(&GuiGamelistOptions::createNewCollectionFilter, this));
		}
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
		if (!isInRelevancyMode)
		{
			mMenu.addGroup(_("VIEW OPTIONS"));

			if (showViewStyle)
				mMenu.addWithLabel(_("GAMELIST VIEW STYLE"), mViewMode);

			mMenu.addEntry(_("VIEW CUSTOMIZATION"), true, [this, system]()
			{
				GuiMenu::openThemeConfiguration(mWindow, this, nullptr, system->getThemeFolder());
			});
		}

		if (file->getType() == FOLDER && ((FolderData*) file)->isVirtualStorage())
			fromPlaceholder = true;
		else if (file->getType() == FOLDER && mSystem->getName() == CollectionSystemManager::get()->getCustomCollectionsBundle()->getName())
			fromPlaceholder = true;
		
		if (!fromPlaceholder)
		{
			auto srcSystem = file->getSourceFileData()->getSystem();
			auto sysOptions = !mSystem->isGameSystem() || mSystem->isGroupSystem() ? srcSystem : mSystem;

			bool showSystemOptions = ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS) && (sysOptions->hasFeatures() || sysOptions->hasEmulatorSelection());
			if (showSystemOptions)
			{
				mMenu.addGroup(_("OPTIONS"));

				if (file && file->hasKeyboardMapping())
				{
					mMenu.addEntry(_("VIEW PAD TO KEYBOARD INFORMATION"), true, [this, file]
					{
						GuiMenu::editKeyboardMappings(mWindow, file, false);
					});
				}
				else if (srcSystem->getKeyboardMapping().isValid())
				{
					mMenu.addEntry(_("VIEW PAD TO KEYBOARD INFORMATION"), true, [this, srcSystem]
					{
						GuiMenu::editKeyboardMappings(mWindow, srcSystem, false);
					});
				}
					
				mMenu.addEntry(_("ADVANCED SYSTEM OPTIONS"), true, [this, sysOptions] { GuiMenu::popSystemConfigurationGui(mWindow, sysOptions); });
			}
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

	bool saveSort = !fromPlaceholder || mSystem == CollectionSystemManager::get()->getCustomCollectionsBundle();

	// apply sort
	if (mListSort && saveSort && mListSort->getSelected() != mSystem->getSortId())
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
		{
			mSystem->loadTheme();
			ViewController::get()->reloadSystemListViewTheme(mSystem);
		}

		if (!viewModeChanged && mSystem->isCollection() && mSystem != CollectionSystemManager::get()->getCustomCollectionsBundle())
			CollectionSystemManager::get()->reloadCollection(getCustomCollectionName());
		else
			ViewController::get()->reloadGameListView(mSystem);
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

void GuiGamelistOptions::openMetaDataEd()
{
	if (ThreadedScraper::isRunning() || ThreadedHasher::isRunning())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("THIS FUNCTION IS DISABLED WHILE THE SCRAPER IS RUNNING")));
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
			{
				system->getRootFolder()->removeFromVirtualFolders(file);
				delete file;
			}

			delete pThis;
		};
	}

	mWindow->pushGui(new GuiMetaDataEd(mWindow, &file->getMetadata(), file->getMetadata().getMDD(), p, Utils::FileSystem::getFileName(file->getPath()),
		std::bind(&ViewController::onFileChanged, ViewController::get(), file, FILE_METADATA_CHANGED), deleteBtnFunc, file));
}

#ifdef _ENABLEEMUELEC
char getSortLetter(int sortId, FileData* fData) {
	if (sortId == FileSorts::SORTNAME_ASCENDING || sortId == FileSorts::SORTNAME_DESCENDING)	
		return toupper(fData->getSortOrName()[0]);
	if (sortId == FileSorts::FILENAME_ASCENDING || sortId == FileSorts::FILENAME_DESCENDING)	
		return toupper(fData->getName()[0]);
	return 0;
}
#endif

void GuiGamelistOptions::jumpToLetter()
{
	std::string letter = mJumpToLetterList->getSelected();
	IGameListView* gamelist = getGamelist();

	if (mListSort->getSelected() != 0)
	{
#ifdef _ENABLEEMUELEC
				int nameSorts[4] = {
					FileSorts::FILENAME_ASCENDING,
					FileSorts::FILENAME_DESCENDING,
					FileSorts::SORTNAME_ASCENDING,
					FileSorts::SORTNAME_DESCENDING};
				int val = mListSort->getSelected();
				if (std::find(std::begin(nameSorts), std::end(nameSorts), val) != std::end(nameSorts))
				{
					mSystem->setSortId(val);
				}
				else {
					mListSort->selectFirstItem();
					mSystem->setSortId(0);
				}
#else
		mListSort->selectFirstItem();
		mSystem->setSortId(0);
#endif

		FolderData* root = mSystem->getRootFolder();
		if (root != nullptr)
			gamelist->onFileChanged(root, FILE_SORTED);
	}

	long letterIndex = -1;

	auto files = gamelist->getFileDataEntries();
	for (int i = files.size() - 1; i >= 0; i--)
	{
		const std::string& name = files.at(i)->getName();
		if (name.empty())
			continue;

		std::string checkLetter;

		if (Utils::String::isKorean(name.c_str()))
		{
			const char* koreanLetter = nullptr;

			std::string nameChar = name.substr(0, 3);
			if (!Utils::String::splitHangulSyllable(nameChar.c_str(), &koreanLetter) || !koreanLetter)
				continue;

			checkLetter = std::string(koreanLetter, 3);
		}
		else
		{
			checkLetter = std::string(1, toupper(name[0]));
		}

		if (letterIndex >= 0 && checkLetter != letter)
			break;

		if (checkLetter == letter)
			letterIndex = i;
	}

	if (letterIndex >= 0)
		gamelist->setCursor(files.at(letterIndex));

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
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("CLOSE"), [&] { delete this; }));
	return prompts;
}

IGameListView* GuiGamelistOptions::getGamelist()
{
	if(mGamelist != nullptr)
		return mGamelist; 
	
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

	mWindow->pushGui(new GuiMsgBox(mWindow, _("ARE YOU SURE YOU WANT TO DELETE THIS ITEM?"), _("YES"),
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

bool GuiGamelistOptions::onMouseClick(int button, bool pressed, int x, int y)
{
	if (pressed && button == 1 && !mMenu.isMouseOver())
	{
		delete this;
		return true;
	}

	return (button == 1);
}