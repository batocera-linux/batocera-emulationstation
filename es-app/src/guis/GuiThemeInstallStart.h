#pragma once

#include <string>
#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/ComponentGrid.h"
#include "components/TextComponent.h"

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
	void start(std::string themeName);

	MenuComponent	mMenu;
};
