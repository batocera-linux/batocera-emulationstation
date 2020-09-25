#pragma once

#include <string>
#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/ComponentGrid.h"
#include "components/TextComponent.h"
#include "ApiSystem.h"
#include "ContentInstaller.h"

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

	void OnContentInstalled(int contentType, std::string contentName, bool success) override;

private:
	void loadPackagesAsync(bool updatePackageList = false);
	void loadList(bool updatePackageList);
	void processPackage(PacmanPackage package);
	void centerWindow();

	MenuComponent	mMenu;
	int				mReloadList;
	std::vector<PacmanPackage> mPackages;
	
};
