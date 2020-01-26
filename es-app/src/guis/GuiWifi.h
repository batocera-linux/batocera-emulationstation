#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/BusyComponent.h"

#include <thread>

class GuiWifi : public GuiComponent
{
public:
	GuiWifi(Window* window, const std::string title, std::string data, const std::function<void(std::string)>& onsave);
	bool input(InputConfig* config, Input input) override;
	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void	load(std::vector<std::string> ssids);

	void	onSave(const std::string& value);
	void	onManualInput();
	void	onRefresh();

	MenuComponent mMenu;

	std::string mTitle;
	std::string mInitialData;

	std::function<void(std::string)> mSaveFunction;

	bool		mWaitingLoad;
};
