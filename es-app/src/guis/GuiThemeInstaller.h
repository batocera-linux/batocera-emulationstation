#pragma once

#include <string>
#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/ComponentGrid.h"
#include "components/TextComponent.h"
#include "components/ImageComponent.h"
#include "ApiSystem.h"
#include "ContentInstaller.h"
#include "components/ComponentTab.h"

template<typename T>
class OptionListComponent;

class GuiBatoceraThemeEntry : public ComponentGrid
{
public:
	GuiBatoceraThemeEntry(Window* window, BatoceraTheme& entry);

	bool isInstallPending() { return mIsPending; }
	bool isCurrentTheme() { return mIsCurrentTheme; }

	BatoceraTheme& getEntry() { return mEntry; }
	virtual void setColor(unsigned int color);

	void update(int deltaTime) override;

private:
	std::shared_ptr<TextComponent>  mImage;
	std::shared_ptr<TextComponent>  mText;
	std::shared_ptr<TextComponent>  mSubstring;

	std::shared_ptr<ImageComponent>  mPreviewImage;

	BatoceraTheme mEntry;
	bool mIsPending;
	bool mIsCurrentTheme;
};

class GuiThemeInstaller : public GuiComponent, IContentInstalledNotify
{
public:
	GuiThemeInstaller(Window* window);
	~GuiThemeInstaller();

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;

	virtual std::vector<HelpPrompt> getHelpPrompts() override;
	virtual void onSizeChanged() override;

	void OnContentInstalled(int contentType, std::string contentName, bool success) override;

private:
	void loadThemesAsync();
	void loadList();

	void centerWindow();
	void processTheme(BatoceraTheme theme, bool isCurrentTheme);

	int				mReloadList;
	int				mTabFilter;
	//MenuComponent	mMenu;

	NinePatchComponent				mBackground;
	ComponentGrid					mGrid;

	std::shared_ptr<TextComponent>	mTitle;
	std::shared_ptr<TextComponent>	mSubtitle;

	std::shared_ptr<ComponentList>	mList;

	std::shared_ptr<ComponentGrid>	mHeaderGrid;
	std::shared_ptr<ComponentGrid>	mButtonGrid;

	std::shared_ptr<ComponentTab>	mTabs;


	std::vector<BatoceraTheme> mThemes;
};
