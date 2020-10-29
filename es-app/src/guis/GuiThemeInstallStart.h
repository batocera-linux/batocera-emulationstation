#pragma once

#include <string>
#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/ComponentGrid.h"
#include "components/TextComponent.h"
#include "components/ImageComponent.h"
#include "ApiSystem.h"
#include "ContentInstaller.h"

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

	std::shared_ptr<ImageComponent>  mPreviewImage;

	BatoceraTheme mEntry;
	bool mIsPending;
};


// Batocera
class GuiThemeInstallStart : public GuiComponent, IContentInstalledNotify
{
public:
	GuiThemeInstallStart(Window* window);
	~GuiThemeInstallStart();

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;

	virtual std::vector<HelpPrompt> getHelpPrompts() override;

	void OnContentInstalled(int contentType, std::string contentName, bool success) override;

private:
	void loadThemesAsync();
	void loadList();

	void centerWindow();
	void processTheme(BatoceraTheme theme);

	int				mReloadList;
	MenuComponent	mMenu;

	std::vector<BatoceraTheme> mThemes;
};
