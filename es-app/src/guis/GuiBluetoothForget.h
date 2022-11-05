#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/BusyComponent.h"

#include <thread>

class GuiBluetoothForget : public GuiComponent
{
public:
	GuiBluetoothForget(Window* window);
	bool input(InputConfig* config, Input input) override;
	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:
	bool	load();
	void	onRemoveDevice(const std::string& id, const std::string& name = "");
	void	onRemoveAll();

	MenuComponent mMenu;

	std::function<void(std::string)> mSaveFunction;

	bool		mWaitingLoad;
};
