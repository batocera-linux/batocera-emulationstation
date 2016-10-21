#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/ComponentGrid.h"
#include <functional>

class GuiKeyboard : public GuiComponent
{
public:
	GuiKeyboard(Window* window);

	bool input(InputConfig* config, Input input) override;
	void onSizeChanged() override;
	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	MenuComponent mMenu;
	TextComponent mVersion;

	ComponentGrid cGrid;
};
