#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "ApiSystem.h"

template<typename T>
class OptionListComponent;

// Batocera
class GuiBios : public GuiComponent
{
public:
	static void show(Window* window);

	bool input(InputConfig* config, Input input) override;

	virtual std::vector<HelpPrompt> getHelpPrompts() override;

protected:
	GuiBios(Window* window, const std::vector<BiosSystem> bioses);

private:
	void refresh();
	void loadList();
	void centerWindow();

	MenuComponent mMenu;	

	std::vector<BiosSystem> mBios;
};
