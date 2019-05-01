#pragma once
#ifndef ES_APP_GUIS_GUI_MENU_H
#define ES_APP_GUIS_GUI_MENU_H

#include "components/MenuComponent.h"
#include "GuiComponent.h"
#include "guis/GuiSettings.h"
#include "components/OptionListComponent.h"
#include <SystemData.h>

class StrInputConfig
{
 public:
  StrInputConfig(std::string ideviceName, std::string ideviceGUIDString) {
    deviceName = ideviceName;
    deviceGUIDString = ideviceGUIDString;
  }

  std::string deviceName;
  std::string deviceGUIDString;
};

class GuiMenu : public GuiComponent
{
public:
	GuiMenu(Window* window);
	~GuiMenu();

	bool input(InputConfig* config, Input input) override;
	void onSizeChanged() override;
	std::vector<HelpPrompt> getHelpPrompts() override;
	HelpStyle getHelpStyle() override;
	static void openQuitMenu_batocera_static(Window *window); // batocera

private:
	void addEntry(const char* name, unsigned int color, bool add_arrow, const std::function<void()>& func);
	void addVersionInfo();
	void openCollectionSystemSettings();
	void openConfigInput();
	void openOtherSettings();
	void openQuitMenu();
	void openScraperSettings();
	void openScreensaverOptions();
	void openSoundSettings();
	void openUISettings();

	// batocera
	void openKodiLauncher_batocera();
	void openSystemSettings_batocera();
	void openGamesSettings_batocera();
	void openControllersSettings_batocera();
	void openUISettings_batocera();
	void openSoundSettings_batocera();
	void openNetworkSettings_batocera();
	void openScraperSettings_batocera();
	void openQuitMenu_batocera();

	void createInputTextRow(GuiSettings * gui, std::string title, const char* settingsID, bool password);
	MenuComponent mMenu;
	TextComponent mVersion;

	std::shared_ptr<OptionListComponent<std::string>> createRatioOptionList(Window *window, std::string configname) const;
	std::shared_ptr<OptionListComponent<std::string>> createVideoResolutionModeOptionList(Window *window, std::string configname) const;
	void popSystemConfigurationGui(SystemData *systemData, std::string previouslySelectedEmulator) const;

	std::vector<StrInputConfig*> mLoadedInput; // used to keep information about loaded devices in case there are unpluged between device window load and save
	void clearLoadedInput();
	std::vector<std::string> getDecorationsSets();
};

#endif // ES_APP_GUIS_GUI_MENU_H
