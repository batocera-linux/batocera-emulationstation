#include <SystemData.h>
#include "guis/GuiCollectionSystemsOptions.h"

#include "components/OptionListComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiSettings.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "guis/GuiTextEditPopup.h"
#include "utils/StringUtil.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "Window.h"
#include "GuiMenu.h"
#include "ApiSystem.h"
#include "GuiGamelistFilter.h"
#include "views/gamelist/IGameListView.h"

GuiCollectionSystemsOptions::GuiCollectionSystemsOptions(Window* window) 
	: GuiSettings(window, _("GAME COLLECTION SETTINGS").c_str())
{
	initializeMenu();
}

void GuiCollectionSystemsOptions::initializeMenu()
{
	addGroup(_("COLLECTIONS TO DISPLAY"));

	// SYSTEMS DISPLAYED
	auto hiddenSystems = Utils::String::split(Settings::getInstance()->getString("HiddenSystems"), ';');
	auto displayedSystems = std::make_shared<OptionListComponent<SystemData*>>(mWindow, _("SYSTEMS DISPLAYED"), true);

	if (SystemData::IsManufacturerSupported && (Settings::getInstance()->getString("SortSystems") == "manufacturer" || Settings::getInstance()->getString("SortSystems") == "subgroup"))
	{
		std::string man;
		for (auto system : SystemData::sSystemVector)
		{
			if (system->isCollection()/* || system->isGroupChildSystem()*/)
				continue;

			if (man != system->getSystemMetadata().manufacturer)
			{
				displayedSystems->addGroup(system->getSystemMetadata().manufacturer);
				man = system->getSystemMetadata().manufacturer;
			}

			displayedSystems->add(system->getFullName(), system, std::find(hiddenSystems.cbegin(), hiddenSystems.cend(), system->getName()) == hiddenSystems.cend());
		}
	}
	else
	{
		for (auto system : SystemData::sSystemVector)
			if (!system->isCollection()/* && !system->isGroupChildSystem()*/)
				displayedSystems->add(system->getFullName(), system, std::find(hiddenSystems.cbegin(), hiddenSystems.cend(), system->getName()) == hiddenSystems.cend());
	}

	addWithLabel(_("SYSTEMS DISPLAYED"), displayedSystems);
	addSaveFunc([this, displayedSystems]
	{
		std::string hiddenSystems;

		std::vector<SystemData*> sys = displayedSystems->getSelectedObjects();

		for (auto system : SystemData::sSystemVector)
		{
			if (system->isCollection()/* || system->isGroupChildSystem()*/)
				continue;

			if (std::find(sys.cbegin(), sys.cend(), system) == sys.cend())
			{
				if (hiddenSystems.empty())
					hiddenSystems = system->getName();
				else
					hiddenSystems = hiddenSystems + ";" + system->getName();
			}
		}

		if (Settings::getInstance()->setString("HiddenSystems", hiddenSystems))
		{
			Settings::getInstance()->saveFile();
			setVariable("reloadSystems", true);
		}
	});

	// get collections
	addSystemsToMenu();

	// GROUPED SYSTEMS
	std::vector<std::string> groupNames;
	for (auto systemName : SystemData::getAllGroupNames())
		groupNames.push_back(systemName);

	if (groupNames.size() > 0)
	{
		std::sort(groupNames.begin(), groupNames.end());

		auto ungroupedSystems = std::make_shared<OptionListComponent<std::string>>(mWindow, _("GROUPED SYSTEMS"), true);
		for (auto groupName : groupNames)
		{
			std::vector<std::string> systemNames;
			for (auto systemName : SystemData::getGroupChildSystemNames(groupName))
				systemNames.push_back(systemName);

			std::sort(systemNames.begin(), systemNames.end());

			int count = 0;
			for (auto systemName : systemNames)
				if (systemName != groupName)
					count++;
	
			// Don't group if system count is only 1
			if (count == 1 && Settings::getInstance()->HideUniqueGroups())
				continue;

			SystemData* pSystem = SystemData::getSystem(groupName);
			if (pSystem != nullptr)
				ungroupedSystems->addGroup(Utils::String::toUpper(pSystem->getFullName()));
			else
				ungroupedSystems->addGroup(Utils::String::toUpper(groupName));

			std::string description;

			for (auto systemName : systemNames)
			{
				if (systemName == groupName)
					continue;

				bool isChecked = !Settings::getInstance()->getBool(groupName + ".ungroup") && !Settings::getInstance()->getBool(systemName + ".ungroup");
				
				SystemData* pSystem = SystemData::getSystem(systemName);
				if (pSystem != nullptr)
					ungroupedSystems->add(pSystem->getFullName(), systemName, isChecked);
				else
					ungroupedSystems->add(systemName, systemName, isChecked);
			}
		}

		addWithLabel(_("GROUPED SYSTEMS"), ungroupedSystems);
		
		addSaveFunc([this, ungroupedSystems, groupNames]
		{
			std::vector<std::string> checkedItems = ungroupedSystems->getSelectedObjects();

			for (auto groupName : groupNames)
			{
				if (std::find(checkedItems.cbegin(), checkedItems.cend(), groupName) == checkedItems.cend() && Settings::getInstance()->getBool(groupName + ".ungroup"))
					Settings::getInstance()->setBool(groupName + ".ungroup", false);

				for (auto systemName : SystemData::getGroupChildSystemNames(groupName))
				{
					if (systemName == groupName)
						continue;

					bool unGrouped = std::find(checkedItems.cbegin(), checkedItems.cend(), systemName) == checkedItems.cend();

					if (Settings::getInstance()->getBool(systemName + ".ungroup") == unGrouped)
						continue;

					Settings::getInstance()->setBool(systemName + ".ungroup", unGrouped);
					setVariable("reloadSystems", true);
				}
			}
		});
	}

	addGroup(_("CREATE CUSTOM COLLECTION"));

	auto unusedFolders = CollectionSystemManager::get()->getUnusedSystemsFromTheme();

	auto createCustomCollection = [this, unusedFolders](const std::string& label, bool dynamicCollection)
	{				
		auto window = mWindow;

		auto finalCreateCollection = [this, dynamicCollection, window](const std::string& newVal)
		{
			window->postToUiThread([this, newVal, dynamicCollection]()
			{
				if (dynamicCollection)
					createFilterCollection(newVal, true);
				else
					createCollection(newVal);
			});
		};

		if (unusedFolders.size() == 0)
		{
			if (Settings::getInstance()->getBool("UseOSK"))
				mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, _("New Collection Name"), "", finalCreateCollection, false));
			else
				mWindow->pushGui(new GuiTextEditPopup(mWindow, _("New Collection Name"), "", finalCreateCollection, false));

			return;
		}

		auto s = new GuiSettings(mWindow, label);

		s->addGroup(_("CREATE CUSTOM COLLECTION"));
		s->addEntry(_("ENTER NAME"), true, [this, window, finalCreateCollection]
			{
				if (Settings::getInstance()->getBool("UseOSK"))
					window->pushGui(new GuiTextEditPopupKeyboard(window, _("New Collection Name"), "", finalCreateCollection, false));
				else
					window->pushGui(new GuiTextEditPopup(window, _("New Collection Name"), "", finalCreateCollection, false));
			});

		s->addGroup(_("CREATE FROM THEME"));

		// add Custom Systems
		for (auto it = unusedFolders.cbegin(); it != unusedFolders.cend(); it++)
		{
			std::string name = *it;
			s->addEntry(Utils::String::toUpper(name), false, [name, this, dynamicCollection]
				{
					if (dynamicCollection)
						createFilterCollection(name, false);
					else
						createCollection(name);
				});
		}

		mWindow->pushGui(s);
	};

	addEntry(_("CREATE NEW EDITABLE COLLECTION").c_str(), true, [this, createCustomCollection] { createCustomCollection(_("CREATE NEW EDITABLE COLLECTION"), false); });	
	addEntry(_("CREATE NEW DYNAMIC COLLECTION").c_str(), true, [this, createCustomCollection] { createCustomCollection(_("CREATE NEW DYNAMIC COLLECTION"), true); });

	addGroup(_("OPTIONS"));

	// SORT COLLECTIONS AND SYSTEMS
	std::string sortMode = Settings::getInstance()->getString("SortSystems");

	auto sortType = std::make_shared< OptionListComponent<std::string> >(mWindow, _("SORT COLLECTIONS AND SYSTEMS"), false);
	sortType->add(_("NO"), "", sortMode.empty());
	sortType->add(_("ALPHABETICALLY"), "alpha", sortMode == "alpha");

	if (SystemData::IsManufacturerSupported)
	{
		sortType->add(_("BY MANUFACTURER"), "manufacturer", sortMode == "manufacturer");
		sortType->add(_("BY HARDWARE TYPE"), "hardware", sortMode == "hardware");
		sortType->add(_("BY MANUFACTURER AND TYPE"), "subgroup", sortMode == "subgroup");
		sortType->add(_("BY RELEASE YEAR"), "releaseDate", sortMode == "releaseDate");
	}

	if (!sortType->hasSelection())
		sortType->selectFirstItem();
	
	addWithLabel(_("SORT SYSTEMS"), sortType);
	addSaveFunc([this, sortType]
	{
		if (Settings::getInstance()->setString("SortSystems", sortType->getSelected()))
			setVariable("reloadAll", true);
	});
	
	// START ON SYSTEM
	std::string startupSystem = Settings::getInstance()->getString("StartupSystem");

	auto systemfocus_list = std::make_shared< OptionListComponent<std::string> >(mWindow, _("START ON SYSTEM"), false);
	systemfocus_list->add(_("NONE"), "", startupSystem == "");
	systemfocus_list->add(_("RESTORE LAST SELECTED"), "lastsystem", startupSystem == "lastsystem");

	if (SystemData::IsManufacturerSupported && (Settings::getInstance()->getString("SortSystems") == "manufacturer" || Settings::getInstance()->getString("SortSystems") == "subgroup"))
	{
		std::string man;
		for (auto system : SystemData::sSystemVector)
		{
			if (!system->isVisible())
				continue;

			if (man != system->getSystemMetadata().manufacturer)
			{
				systemfocus_list->addGroup(system->getSystemMetadata().manufacturer);
				man = system->getSystemMetadata().manufacturer;
			}

			systemfocus_list->add(system->getName(), system->getName(), startupSystem == system->getName());
		}
	}
	else
	{
		for (auto system : SystemData::sSystemVector)
			if (system->isVisible())
				systemfocus_list->add(system->getName(), system->getName(), startupSystem == system->getName());
	}

	if (!systemfocus_list->hasSelection())
		systemfocus_list->selectFirstItem();

	addWithLabel(_("START ON SYSTEM"), systemfocus_list);
	addSaveFunc([systemfocus_list] 
	{ 
		Settings::getInstance()->setString("StartupSystem", systemfocus_list->getSelected()); 
		Settings::getInstance()->setString("LastSystem", "");
	});

	// START ON GAMELIST
	auto startOnGamelist = std::make_shared<SwitchComponent>(mWindow);
	startOnGamelist->setState(Settings::getInstance()->getBool("StartupOnGameList"));
	addWithLabel(_("START ON GAMELIST"), startOnGamelist);
	addSaveFunc([startOnGamelist] { Settings::getInstance()->setBool("StartupOnGameList", startOnGamelist->getState()); });


	auto bundleCustomCollectionsEx = std::make_shared< OptionListComponent<std::string> >(mWindow, _("GROUP CUSTOM COLLECTIONS"), false);
	bundleCustomCollectionsEx->addRange({ { _("UNTHEMED"), "" },{ "NEVER", "false" },{ "ALWAYS", "true" } }, Settings::getInstance()->getString("UseCustomCollectionsSystemEx"));
	addWithLabel(_("GROUP CUSTOM COLLECTIONS"), bundleCustomCollectionsEx);
	addSaveFunc([this, bundleCustomCollectionsEx]
	{
		if (Settings::getInstance()->setString("UseCustomCollectionsSystemEx", bundleCustomCollectionsEx->getSelected()))
			setVariable("reloadAll", true);
	});


	// GROUP UNTHEMED CUSTOM COLLECTIONS
	/*
	std::shared_ptr<SwitchComponent> bundleCustomCollections = std::make_shared<SwitchComponent>(mWindow);
	bundleCustomCollections->setState(Settings::getInstance()->getBool("UseCustomCollectionsSystem"));
	addWithLabel(_("GROUP UNTHEMED CUSTOM COLLECTIONS"), bundleCustomCollections);
	addSaveFunc([this, bundleCustomCollections]
	{
		if (Settings::getInstance()->setBool("UseCustomCollectionsSystem", bundleCustomCollections->getState()))
			setVariable("reloadAll", true);
	});*/

	/*


	std::shared_ptr<SwitchComponent> sortAllSystemsSwitch = std::make_shared<SwitchComponent>(mWindow);
	sortAllSystemsSwitch->setState(Settings::getInstance()->getBool("SortAllSystems"));
	addWithLabel(_("SORT CUSTOM COLLECTIONS AND SYSTEMS"), sortAllSystemsSwitch);
	addSaveFunc([this, sortAllSystemsSwitch]
	{
		if (Settings::getInstance()->setBool("SortAllSystems", sortAllSystemsSwitch->getState()))
			setVariable("reloadAll", true);
	});
	*/
	std::shared_ptr<SwitchComponent> toggleSystemNameInCollections = std::make_shared<SwitchComponent>(mWindow);
	toggleSystemNameInCollections->setState(Settings::getInstance()->getBool("CollectionShowSystemInfo"));
	addWithLabel(_("SHOW SYSTEM NAME IN COLLECTIONS"), toggleSystemNameInCollections);
	addSaveFunc([this, toggleSystemNameInCollections]
	{
		if (Settings::getInstance()->setBool("CollectionShowSystemInfo", toggleSystemNameInCollections->getState()))
		{
			SystemData::resetSettings();
			FileData::resetSettings();
			setVariable("reloadAll", true);
		}
	});

	std::shared_ptr<SwitchComponent> alsoHideGames = std::make_shared<SwitchComponent>(mWindow);
	alsoHideGames->setState(Settings::HiddenSystemsShowGames());
	addWithLabel(_("SHOW GAMES OF HIDDEN SYSTEMS IN COLLECTIONS"), alsoHideGames);
	addSaveFunc([this, alsoHideGames]
	{
		if (Settings::setHiddenSystemsShowGames(alsoHideGames->getState()))
		{
			FileData::resetSettings();
			setVariable("reloadSystems", true);
		}
	});

	addSwitch(_("SHOW EMPTY SYSTEMS"), "LoadEmptySystems", true, [&] { setVariable("reloadSystems", true); });
	addSwitch(_("DON'T SHOW GROUPS WITH ONY ONE SYSTEM"), "HideUniqueGroups", true, [&] { setVariable("reloadSystems", true); });
		
#if defined(WIN32) && !defined(_DEBUG)		
	if (!ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
		addEntry(_("UPDATE GAMELISTS"), false, [this] { GuiMenu::updateGameLists(mWindow); }); // Game List Update
#endif		
	
	addSaveFunc([this]
	{
		std::string newAutoSettings = Utils::String::vectorToCommaString(autoOptionList->getSelectedObjects());
		std::string newCustomSettings = Utils::String::vectorToCommaString(customOptionList->getSelectedObjects());

		bool dirty = Settings::getInstance()->setString("CollectionSystemsAuto", newAutoSettings);
		dirty |= Settings::getInstance()->setString("CollectionSystemsCustom", newCustomSettings);

		if (dirty)
			setVariable("reloadAll", true);
	});

	onFinalize([this]
	{
		if (getVariable("reloadSystems"))
		{
			Window* window = mWindow;
			window->renderSplashScreen(_("Loading..."));

			ViewController::get()->goToStart();
			delete ViewController::get();
			ViewController::init(window);
			CollectionSystemManager::deinit();
			CollectionSystemManager::init(window);
			SystemData::loadConfig(window);

			GuiComponent* gui;
			while ((gui = window->peekGui()) != NULL) 
			{
				window->removeGui(gui);
				if (gui != this)
					delete gui;
			}
			ViewController::get()->reloadAll(nullptr, false); // Avoid reloading themes a second time
			window->closeSplashScreen();

			window->pushGui(ViewController::get());
		}
		else if (getVariable("reloadAll"))
		{			
			Settings::getInstance()->saveFile();

			CollectionSystemManager::get()->loadEnabledListFromSettings();
			CollectionSystemManager::get()->updateSystemsList();
			ViewController::get()->goToStart();
			ViewController::get()->reloadAll(mWindow);
			mWindow->closeSplashScreen();
		}
	});
}

void GuiCollectionSystemsOptions::createCollection(std::string inName) 
{
	std::string name = CollectionSystemManager::get()->getValidNewCollectionName(inName);
	SystemData* newSys = CollectionSystemManager::get()->addNewCustomCollection(name);
	customOptionList->add(name, name, true);

	std::string outAuto = Utils::String::vectorToCommaString(autoOptionList->getSelectedObjects());
	std::string outCustom = Utils::String::vectorToCommaString(customOptionList->getSelectedObjects());
	updateSettings(outAuto, outCustom);

	ViewController::get()->goToSystemView(newSys);

	// Make sure the physical file is created
	CollectionSystemManager::get()->saveCustomCollection(newSys);

	Window* window = mWindow;
	while (window->peekGui() && window->peekGui() != ViewController::get())
		delete window->peekGui();

	window->renderSplashScreen();

	CollectionSystemManager::get()->reloadCollection(name, false);
	ViewController::get()->goToGameList(newSys, true);

	window->closeSplashScreen();
}

void GuiCollectionSystemsOptions::createFilterCollection(std::string inName, bool editFilters)
{
	std::string name = CollectionSystemManager::get()->getValidNewCollectionName(inName);
	if (name.empty())
		return;

	CollectionFilter cf;

	if (!editFilters)
		cf.setTextFilter(Utils::String::toUpper(name));

	cf.create(name);
	
	SystemData* newSys = CollectionSystemManager::get()->addNewCustomCollection(name);

	std::string setting = Settings::getInstance()->getString("CollectionSystemsCustom");
	setting = setting.empty() ? name : setting + "," + name;
	if (Settings::getInstance()->setString("CollectionSystemsCustom", setting))
		Settings::getInstance()->saveFile();
		
	CollectionSystemManager::get()->loadEnabledListFromSettings();
	CollectionSystemManager::get()->updateSystemsList();
	ViewController::get()->reloadAll(nullptr, false);
	ViewController::get()->goToStart();

	std::map<std::string, CollectionSystemData> customCollections = CollectionSystemManager::get()->getCustomCollectionSystems();
	auto customCollection = customCollections.find(name);
	if (customCollection == customCollections.cend())
		return;

	Window* window = mWindow;

	GuiComponent* topGui = window->peekGui();
	if (topGui != nullptr)
		window->removeGui(topGui);

	while (window->peekGui() && window->peekGui() != ViewController::get())
		delete window->peekGui();

	if (!editFilters)
	{
		CollectionSystemManager::get()->reloadCollection(name, false);
		ViewController::get()->goToGameList(newSys, true);
		return;
	}

	GuiGamelistFilter* ggf = new GuiGamelistFilter(window, customCollection->second.filteredIndex);
	
	ggf->onFinalize([name, newSys, window]()
	{
		window->renderSplashScreen();

		CollectionSystemManager::get()->reloadCollection(name, false);						
		ViewController::get()->goToGameList(newSys, true);

		window->closeSplashScreen();
	});
	
	window->pushGui(ggf);
}

GuiCollectionSystemsOptions::~GuiCollectionSystemsOptions()
{

}

void GuiCollectionSystemsOptions::addSystemsToMenu()
{
	std::map<std::string, CollectionSystemData> &autoSystems = CollectionSystemManager::get()->getAutoCollectionSystems();

	autoOptionList = std::make_shared< OptionListComponent<std::string> >(mWindow, _("SELECT COLLECTIONS"), true);
	autoOptionList->addGroup(_("AUTOMATIC COLLECTIONS"));

	bool hasGroup = false;
	bool hasGenreGroup = false;

	auto arcadeGames = CollectionSystemManager::get()->getArcadeCollection()->getRootFolder()->getFilesRecursive(GAME);

	// add Auto Systems && preserve order
	for (auto systemDecl : CollectionSystemManager::getSystemDecls())
	{
		auto it = autoSystems.find(systemDecl.name);
		if (it == autoSystems.cend())
			continue;

        if (it->second.decl.displayIfEmpty)
            autoOptionList->add(it->second.decl.longName, it->second.decl.name, it->second.isEnabled);
		else if (it->second.decl.isGenreCollection()) // Genre collections
		{
			if (!hasGenreGroup)
			{
				autoOptionList->addGroup(_("PER GENRE"));
				hasGenreGroup = true;
			}

			autoOptionList->add(it->second.decl.longName, it->second.decl.name, it->second.isEnabled);
		}
        else if (it->second.decl.isArcadeSubSystem()) // Arcade collections
        {
			bool hasGames = false;

			if (it->second.isPopulated)
				hasGames = it->second.system->getRootFolder()->getChildren().size() > 0;
			else
			{
				for (auto file : arcadeGames)
				{
					if (file->getMetadata(MetaDataId::ArcadeSystemName) == systemDecl.themeFolder)
					{
						hasGames = true;
						break;
					}
				}
			}

			if (!hasGames)
				continue;

			if (!hasGroup)
			{
				autoOptionList->addGroup(_("ARCADE SYSTEMS"));
				hasGroup = true;
			}

            autoOptionList->add(it->second.decl.longName, it->second.decl.name, it->second.isEnabled);
        }
	}
	addWithLabel(_("AUTOMATIC GAME COLLECTIONS"), autoOptionList);

	std::map<std::string, CollectionSystemData> customSystems = CollectionSystemManager::get()->getCustomCollectionSystems();

	customOptionList = std::make_shared< OptionListComponent<std::string> >(mWindow, _("SELECT COLLECTIONS"), true);

	customOptionList->addGroup(_("EDITABLE COLLECTIONS"));

	for (auto sys : customSystems)
		if (sys.second.filteredIndex == nullptr)
			customOptionList->add(sys.second.decl.longName, sys.second.decl.name, sys.second.isEnabled);

	customOptionList->addGroup(_("DYNAMIC COLLECTIONS"));

	for (auto sys : customSystems)
		if (sys.second.filteredIndex != nullptr)
			customOptionList->add(sys.second.decl.longName, sys.second.decl.name, sys.second.isEnabled);

	if (customOptionList->size() > 0)
		addWithLabel(_("CUSTOM GAME COLLECTIONS"), customOptionList);
}

void GuiCollectionSystemsOptions::updateSettings(std::string newAutoSettings, std::string newCustomSettings)
{
	bool dirty = Settings::getInstance()->setString("CollectionSystemsAuto", newAutoSettings);
	dirty |= Settings::getInstance()->setString("CollectionSystemsCustom", newCustomSettings);

	if (dirty)
	{
		Settings::getInstance()->saveFile();
		CollectionSystemManager::get()->loadEnabledListFromSettings();
		CollectionSystemManager::get()->updateSystemsList();		
		ViewController::get()->reloadAll();
		ViewController::get()->goToStart();
	}
}