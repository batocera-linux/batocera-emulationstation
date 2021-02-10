#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "ApiSystem.h"
#include "ContentInstaller.h"

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
class GuiBezelInstaller : public GuiComponent, IContentInstalledNotify
{
public:
	GuiBezelInstaller(Window* window);
	~GuiBezelInstaller();

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;

	virtual std::vector<HelpPrompt> getHelpPrompts() override;

	void OnContentInstalled(int contentType, std::string contentName, bool success) override;
private:
	void processBezel(BatoceraBezel name);
	
	void loadBezelsAsync();
	void loadList();

	void centerWindow();

	int			  mReloadList;
	MenuComponent mMenu;	

	std::vector<BatoceraBezel> mBezels;
};
