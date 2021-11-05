#pragma once

#ifndef ES_APP_GUIS_GUI_HASH_START_H
#define ES_APP_GUIS_GUI_HASH_START_H

#include "components/MenuComponent.h"
#include "scrapers/Scraper.h"
#include "ThreadedHasher.h"

class FileData;
template<typename T>
class OptionListComponent;
class SwitchComponent;
class SystemData;

//The starting point for a multi-game scrape.
//Allows the user to set various parameters (to set filters, to set which systems to scrape, to enable manual mode).
//Generates a list of "searches" that will be carried out by GuiScraperLog.
class GuiHashStart : public GuiComponent
{
public:
	GuiHashStart(Window* window, ThreadedHasher::HasherType type);

	bool input(InputConfig* config, Input input) override;

	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:	
	void start();

	ThreadedHasher::HasherType mType;
	std::shared_ptr< OptionListComponent<bool> > mFilters;
	std::shared_ptr< OptionListComponent<SystemData*> > mSystems;
	//std::shared_ptr<SwitchComponent> mApproveResults;

	MenuComponent mMenu;
	bool mOverwriteMedias;
};

#endif // ES_APP_GUIS_GUI_SCRAPER_START_H
