#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/BusyComponent.h"

#include <thread>

class GuiBluetoothDevices : public GuiComponent
{
public:
	GuiBluetoothDevices(Window* window);
	bool input(InputConfig* config, Input input) override;
	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:
	bool	load();
	void	onRemoveDevice(const std::string& id, const std::string& name = "");
	void	onRemoveAll();
	void	onConnectDevice(const std::string& id);
	void	onDisconnectDevice(const std::string& id);

	MenuComponent mMenu;

	std::function<void(std::string)> mSaveFunction;

	bool		mWaitingLoad;
};
