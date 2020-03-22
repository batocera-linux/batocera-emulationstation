#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/BusyComponent.h"

#include <thread>

class GuiBluetooth : public GuiComponent
{
public:
	GuiBluetooth(Window* window);
	bool input(InputConfig* config, Input input) override;
	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:
	bool	load();
	void	onRemoveDevice(const std::string& value);
	void	onRemoveAll();

	MenuComponent mMenu;

	std::function<void(std::string)> mSaveFunction;

	bool		mWaitingLoad;
};
