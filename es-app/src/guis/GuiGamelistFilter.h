#pragma once
#ifndef ES_APP_GUIS_GUI_GAME_LIST_FILTER_H
#define ES_APP_GUIS_GUI_GAME_LIST_FILTER_H

#include "components/MenuComponent.h"
#include "FileFilterIndex.h"
#include "GuiComponent.h"
#include <functional>

template<typename T>
class OptionListComponent;
class SystemData;

class GuiGamelistFilter : public GuiComponent
{
public:
	GuiGamelistFilter(Window* window, SystemData* system);
	GuiGamelistFilter(Window* window, FileFilterIndex* filterIndex);

	~GuiGamelistFilter();
	bool input(InputConfig* config, Input input) override;

	virtual std::vector<HelpPrompt> getHelpPrompts() override;

	inline void onFinalize(const std::function<void()>& func) { mOnFinalizeFunc = func; };

private:
	std::function<void()> mOnFinalizeFunc;

	void initializeMenu();
	void applyFilters();
	void resetAllFilters();
	void addFiltersToMenu();
	void addTextFilterToMenu();
	void addSystemFilterToMenu();

	std::map<FilterIndexType, std::shared_ptr< OptionListComponent<std::string> >> mFilterOptions;

	MenuComponent mMenu;
	SystemData* mSystem;
	FileFilterIndex* mFilterIndex;

	std::shared_ptr<OptionListComponent<SystemData*>> mSystemFilter;
	std::shared_ptr<GuiComponent> mTextFilter;
};

#endif // ES_APP_GUIS_GUI_GAME_LIST_FILTER_H
