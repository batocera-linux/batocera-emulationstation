#pragma once
#ifndef ES_APP_GUIS_GUI_COLLECTION_SYSTEM_OPTIONS_H
#define ES_APP_GUIS_GUI_COLLECTION_SYSTEM_OPTIONS_H

#include "components/MenuComponent.h"
#include "GuiSettings.h"

template<typename T>
class OptionListComponent;
class SwitchComponent;
class SystemData;

class GuiCollectionSystemsOptions : public GuiSettings
{
public:
	GuiCollectionSystemsOptions(Window* window);
	~GuiCollectionSystemsOptions();	

private:
	void initializeMenu();	
	void addSystemsToMenu();
	
	void updateSettings(std::string newAutoSettings, std::string newCustomSettings);
	void createCollection(std::string inName);
	void createFilterCollection(std::string inName);
	
	std::shared_ptr< OptionListComponent<std::string> > autoOptionList;
	std::shared_ptr< OptionListComponent<std::string> > customOptionList;

	SystemData* mSystem;
};

#endif // ES_APP_GUIS_GUI_COLLECTION_SYSTEM_OPTIONS_H
