#pragma once

#include <string>
#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/ComponentGrid.h"
#include "components/TextComponent.h"
#include "ApiSystem.h"
#include "ContentInstaller.h"
#include "components/ComponentTab.h"

template<typename T>
class OptionListComponent;

class GuiBatoceraStoreEntry : public ComponentGrid
{
public:
	GuiBatoceraStoreEntry(Window* window, PacmanPackage& entry);

	bool isInstallPending() { return mIsPending; }
	PacmanPackage& getEntry() { return mEntry; }
	virtual void setColor(unsigned int color);

private:
	std::shared_ptr<TextComponent>  mImage;
	std::shared_ptr<TextComponent>  mText;
	std::shared_ptr<TextComponent>  mSubstring;

	PacmanPackage mEntry;
	bool mIsPending;
};

// Batocera
class GuiBatoceraStore : public GuiComponent, IContentInstalledNotify
{
public:
	GuiBatoceraStore(Window* window);
	~GuiBatoceraStore();

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;

	virtual std::vector<HelpPrompt> getHelpPrompts() override;
	virtual void onSizeChanged() override;

	void OnContentInstalled(int contentType, std::string contentName, bool success) override;

private:
	static std::vector<PacmanPackage> queryPackages();
	void loadPackagesAsync(bool updatePackageList = false, bool refreshOnly = true);
	void loadList(bool updatePackageList, bool restoreIndex = true);
	void processPackage(PacmanPackage package);
	void centerWindow();
	void showSearch();

	int				mReloadList;
	std::vector<PacmanPackage> mPackages;
	

	NinePatchComponent				mBackground;
	ComponentGrid					mGrid;

	std::shared_ptr<TextComponent>	mTitle;
	std::shared_ptr<TextComponent>	mSubtitle;

	std::shared_ptr<ComponentList>	mList;

	std::shared_ptr<ComponentGrid>	mHeaderGrid;
	std::shared_ptr<ComponentGrid>	mButtonGrid;

	std::shared_ptr<ComponentTab>	mTabs;

	std::string						mTabFilter;
	std::string						mTextFilter;
};
