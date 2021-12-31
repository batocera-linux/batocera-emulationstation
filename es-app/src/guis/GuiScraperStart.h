#pragma once
#ifndef ES_APP_GUIS_GUI_SCRAPER_START_H
#define ES_APP_GUIS_GUI_SCRAPER_START_H

#include "GuiSettings.h"
#include "scrapers/Scraper.h"

class IGuiLoadingHandler;
class FileData;
template<typename T>
class OptionListComponent;
class SwitchComponent;
class SystemData;

typedef std::function<bool(FileData*)> FilterFunc;

//The starting point for a multi-game scrape.
//Allows the user to set various parameters (to set filters, to set which systems to scrape, to enable manual mode).
//Generates a list of "searches" that will be carried out by GuiScraperLog.
class GuiScraperStart : public GuiSettings
{
public:
	GuiScraperStart(Window* window);

	bool input(InputConfig* config, Input input) override;

	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void onShowScraperSettings();
	void pressedStart();
	void start();

	std::queue<ScraperSearchParams> getSearches(std::vector<SystemData*> systems, FilterFunc mediaSelector, FilterFunc dateSelector, IGuiLoadingHandler* handler = nullptr);

	std::shared_ptr<OptionListComponent<FilterFunc>> mDateFilters;
	std::shared_ptr<OptionListComponent<FilterFunc>> mFilters;
	std::shared_ptr<OptionListComponent<SystemData*>> mSystems;

	bool mOverwriteMedias;
};

#endif // ES_APP_GUIS_GUI_SCRAPER_START_H
