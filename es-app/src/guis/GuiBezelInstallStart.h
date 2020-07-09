#pragma once

#include "GuiComponent.h"
#include "GuiThemeInstallStart.h"
#include "components/MenuComponent.h"
#include "ApiSystem.h"

template<typename T>
class OptionListComponent;

class GuiBatoceraBezelEntry : public ComponentGrid
{
public:
	GuiBatoceraBezelEntry(Window* window, BatoceraBezel& entry);

	bool isInstallPending() { return mIsPending; }
	BatoceraBezel& getEntry() { return mEntry; }
	virtual void setColor(unsigned int color);

private:
	std::shared_ptr<TextComponent>  mImage;
	std::shared_ptr<TextComponent>  mText;
	std::shared_ptr<TextComponent>  mSubstring;

	BatoceraBezel mEntry;
	bool mIsPending;
};


// Batocera
class GuiBezelInstallStart : public GuiComponent
{
public:
	GuiBezelInstallStart(Window* window);
	bool input(InputConfig* config, Input input) override;
	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void processBezel(BatoceraBezel name);
	void loadBezels();
	void centerWindow();

	MenuComponent mMenu;	
};
