#pragma once

#include "GuiComponent.h"
#include "components/ComponentGrid.h"
#include "components/NinePatchComponent.h"
#include "ApiSystem.h"

template<typename T>
class OptionListComponent;

class TextComponent;
class ComponentList;
class ComponentTab;

class GuiBios : public GuiComponent
{
public:
	static void show(Window* window);

	bool input(InputConfig* config, Input input) override;

	virtual std::vector<HelpPrompt> getHelpPrompts() override;
	virtual void onSizeChanged() override;

protected:
	GuiBios(Window* window, const std::vector<BiosSystem> bioses);

private:
	void refresh();
	void loadList();
	void centerWindow();

	std::vector<BiosSystem> mBios;

	NinePatchComponent				mBackground;
	ComponentGrid					mGrid;

	std::shared_ptr<TextComponent>	mTitle;
	std::shared_ptr<ComponentList>	mList;
	std::shared_ptr<ComponentGrid>	mButtonGrid;
	std::shared_ptr<ComponentTab>	mTabs;

	int				mTabFilter;
};
