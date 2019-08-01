#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"

template<typename T>
class OptionListComponent;

// Batocera
class GuiThemeInstallStart : public GuiComponent
{
public:
	GuiThemeInstallStart(Window* window);
	bool input(InputConfig* config, Input input) override;

	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void start();

	MenuComponent mMenu;
	char *mSelectedTheme;
};
