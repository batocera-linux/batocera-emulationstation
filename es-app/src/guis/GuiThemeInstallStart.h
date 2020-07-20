#pragma once

#include <string>
#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/ComponentGrid.h"
#include "components/TextComponent.h"
#include "ApiSystem.h"

template<typename T>
class OptionListComponent;

class GuiBatoceraThemeEntry : public ComponentGrid
{
public:
	GuiBatoceraThemeEntry(Window* window, BatoceraTheme& entry);

	bool isInstallPending() { return mIsPending; }
	BatoceraTheme& getEntry() { return mEntry; }
	virtual void setColor(unsigned int color);

private:
	std::shared_ptr<TextComponent>  mImage;
	std::shared_ptr<TextComponent>  mText;
	std::shared_ptr<TextComponent>  mSubstring;

	BatoceraTheme mEntry;
	bool mIsPending;
};


// Batocera
class GuiThemeInstallStart : public GuiComponent
{
public:
	GuiThemeInstallStart(Window* window);
	bool input(InputConfig* config, Input input) override;

	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void loadThemes();
	void centerWindow();
	void processTheme(BatoceraTheme theme);

	MenuComponent	mMenu;
};
