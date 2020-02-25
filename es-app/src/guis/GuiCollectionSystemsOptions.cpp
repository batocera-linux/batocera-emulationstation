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


GuiCollectionSystemsOptions::GuiCollectionSystemsOptions(Window* window) 
	: GuiSettings(window, _("GAME COLLECTION SETTINGS").c_str())
{
	initializeMenu();
}

void GuiCollectionSystemsOptions::initializeMenu()
{
	auto groupNames = SystemData::getAllGroupNames();
	if (groupNames.size() > 0)
	{
		auto ungroupedSystems = std::make_shared<OptionListComponent<std::string>>(mWindow, _("GROUPED SYSTEMS"), true);
		for (auto groupName : groupNames)
		{
			std::string description;
			for (auto zz : SystemData::getGroupChildSystemNames(groupName))
			{
				if (!description.empty())
					description += ", ";

				description += zz;
			}

			ungroupedSystems->addEx(groupName, description, groupName, !Settings::getInstance()->getBool(groupName + ".ungroup"));
		}

		addWithLabel(_("GROUPED SYSTEMS"), ungroupedSystems);

		addSaveFunc([this, ungroupedSystems, groupNames]
		{
			std::vector<std::string> checkedItems = ungroupedSystems->getSelectedObjects();
			for (auto groupName : groupNames)
			{
				bool isGroupActive = std::find(checkedItems.cbegin(), checkedItems.cend(), groupName) != checkedItems.cend();
				if (Settings::getInstance()->setBool(groupName + ".ungroup", !isGroupActive))
					setVariable("reloadSystems", true);
			}
		});
	}

	// get collections
	addSystemsToMenu();

	// add "Create New Custom Collection from Theme"
	std::vector<std::string> unusedFolders = CollectionSystemManager::get()->getUnusedSystemsFromTheme();
	if (unusedFolders.size() > 0)
	{
	  addEntry(_("CREATE NEW CUSTOM COLLECTION FROM THEME").c_str(), true,
		[this, unusedFolders] {
		     auto s = new GuiSettings(mWindow, _("SELECT THEME FOLDER").c_str());
		     std::shared_ptr< OptionListComponent<std::string> > folderThemes = std::make_shared< OptionListComponent<std::string> >(mWindow, _("SELECT THEME FOLDER"), true);

			// add Custom Systems
			for(auto it = unusedFolders.cbegin() ; it != unusedFolders.cend() ; it++ )
			{
				ComponentListRow row;
				std::string name = *it;

				std::function<void()> createCollectionCall = [name, this, s] {
					createCollection(name);
				};
				row.makeAcceptInputHandler(createCollectionCall);

				auto themeFolder = std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(name), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);
				row.addElement(themeFolder, true);
				s->addRow(row);				
			}
			mWindow->pushGui(s);
		});
	}

	auto createCustomCollection = [this](const std::string& newVal) {
		std::string name = newVal;
		// we need to store the first Gui and remove it, as it'll be deleted by the actual Gui
		Window* window = mWindow;
		GuiComponent* topGui = window->peekGui();
		window->removeGui(topGui);
		createCollection(name);
	};
	addEntry(_("CREATE NEW CUSTOM COLLECTION").c_str(), true, [this, createCustomCollection] {
		if (Settings::getInstance()->getBool("UseOSK")) {
			mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, _("New Collection Name"), "", createCustomCollection, false));
		}
		else {
			mWindow->pushGui(new GuiTextEditPopup(mWindow, _("New Collection Name"), "", createCustomCollection, false));
		}
	});

	std::shared_ptr<SwitchComponent> bundleCustomCollections = std::make_shared<SwitchComponent>(mWindow);
	bundleCustomCollections->setState(Settings::getInstance()->getBool("UseCustomCollectionsSystem"));
	addWithLabel(_("GROUP UNTHEMED CUSTOM COLLECTIONS"), bundleCustomCollections);
	addSaveFunc([this, bundleCustomCollections]
	{
		if (Settings::getInstance()->setBool("UseCustomCollectionsSystem", bundleCustomCollections->getState()))
			setVariable("reloadAll", true);
	});

	std::shared_ptr<SwitchComponent> sortAllSystemsSwitch = std::make_shared<SwitchComponent>(mWindow);
	sortAllSystemsSwitch->setState(Settings::getInstance()->getBool("SortAllSystems"));
	addWithLabel(_("SORT CUSTOM COLLECTIONS AND SYSTEMS"), sortAllSystemsSwitch);
	addSaveFunc([this, sortAllSystemsSwitch]
	{
		if (Settings::getInstance()->setBool("SortAllSystems", sortAllSystemsSwitch->getState()))
			setVariable("reloadAll", true);
	});
	
	std::shared_ptr<SwitchComponent> toggleSystemNameInCollections = std::make_shared<SwitchComponent>(mWindow);
	toggleSystemNameInCollections->setState(Settings::getInstance()->getBool("CollectionShowSystemInfo"));
	addWithLabel(_("SHOW SYSTEM NAME IN COLLECTIONS"), toggleSystemNameInCollections);
	addSaveFunc([this, toggleSystemNameInCollections]
	{
		if (Settings::getInstance()->setBool("CollectionShowSystemInfo", toggleSystemNameInCollections->getState()))
			setVariable("reloadAll", true);
	});


	

	if(CollectionSystemManager::get()->isEditing())
		addEntry((_("FINISH EDITING COLLECTION") + " : " + Utils::String::toUpper(CollectionSystemManager::get()->getEditingCollection())).c_str(), false, std::bind(&GuiCollectionSystemsOptions::exitEditMode, this));
	
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
			window->renderLoadingScreen(_("Loading..."));

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
			window->endRenderLoadingScreen();

			window->pushGui(ViewController::get());
		}
		else if (getVariable("reloadAll"))
		{			
			Settings::getInstance()->saveFile();

			CollectionSystemManager::get()->loadEnabledListFromSettings();
			CollectionSystemManager::get()->updateSystemsList();
			ViewController::get()->goToStart();
			ViewController::get()->reloadAll(mWindow);
			mWindow->endRenderLoadingScreen();
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

	Window* window = mWindow;
	CollectionSystemManager::get()->setEditMode(name);
	while(window->peekGui() && window->peekGui() != ViewController::get())
		delete window->peekGui();

	return;
}

void GuiCollectionSystemsOptions::exitEditMode()
{
	CollectionSystemManager::get()->exitEditMode();
	close();
}

GuiCollectionSystemsOptions::~GuiCollectionSystemsOptions()
{

}

void GuiCollectionSystemsOptions::addSystemsToMenu()
{

	std::map<std::string, CollectionSystemData> &autoSystems = CollectionSystemManager::get()->getAutoCollectionSystems();

	autoOptionList = std::make_shared< OptionListComponent<std::string> >(mWindow, _("SELECT COLLECTIONS"), true);

	bool hasGroup = false;

	// add Auto Systems && preserve order
	for (auto systemDecl : CollectionSystemManager::getSystemDecls())
	{
		auto it = autoSystems.find(systemDecl.name);
		if (it == autoSystems.cend())
			continue;

        if (it->second.decl.displayIfEmpty)
            autoOptionList->add(it->second.decl.longName, it->second.decl.name, it->second.isEnabled);
        else
        {
            if (!it->second.isPopulated)
                CollectionSystemManager::get()->populateAutoCollection(&(it->second));

			if (it->second.system->getRootFolder()->getChildren().size() == 0)
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

	// add Custom Systems
	for(std::map<std::string, CollectionSystemData>::const_iterator it = customSystems.cbegin() ; it != customSystems.cend() ; it++ )
	{
		customOptionList->add(it->second.decl.longName, it->second.decl.name, it->second.isEnabled);
	}
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
		ViewController::get()->goToStart();
		ViewController::get()->reloadAll();
	}
}