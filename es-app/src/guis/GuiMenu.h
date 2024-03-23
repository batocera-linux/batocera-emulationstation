#pragma once
#ifndef ES_APP_GUIS_GUI_MENU_H
#define ES_APP_GUIS_GUI_MENU_H

#include "components/MenuComponent.h"
#include "GuiComponent.h"
#include "guis/GuiSettings.h"
#include "components/OptionListComponent.h"
#include <SystemData.h>
#include "KeyboardMapping.h"
#include "utils/VectorEx.h"

struct DecorationSetInfo
{
	DecorationSetInfo() {}
	DecorationSetInfo(const std::string _name, const std::string _path, const std::string _imageUrl)
	{
		name = _name; path = _path; imageUrl = _imageUrl;
	}

	std::string name;
	std::string path;
	std::string imageUrl;
};

class GuiMenu : public GuiComponent
{
public:
	GuiMenu(Window* window, bool animate = true);

	bool input(InputConfig* config, Input input) override;
	void onSizeChanged() override;
	std::vector<HelpPrompt> getHelpPrompts() override;	
	static void openQuitMenu_static(Window *window, bool quickAccessMenu = false, bool animate = true);

	static void popSystemConfigurationGui(Window* mWindow, SystemData *systemData);
	static void popGameConfigurationGui(Window* mWindow, FileData* fileData);

	static void openThemeConfiguration(Window* mWindow, GuiComponent* s, std::shared_ptr<OptionListComponent<std::string>> theme_set, const std::string systemTheme = "");

	static void updateGameLists(Window* window, bool confirm = true);
	static void editKeyboardMappings(Window *window, IKeyboardMapContainer* mapping, bool editable);

private:
	void addEntry(const std::string& name, bool add_arrow, const std::function<void()>& func, const std::string iconName = "");
	void addVersionInfo();
	void openCollectionSystemSettings();
	void openConfigInput();	
	void openScraperSettings();
	void openScreensaverOptions();	
	void openSoundSettings();
	void openUISettings();
	void openUpdatesSettings();
	
	void openSystemSettings();
	void openGamesSettings();	
	void openNetworkSettings(bool selectWifiEnable = false);	
	void openQuitMenu();
	void openSystemInformations();
	void openServicesSettings();
	void openDmdSettings();
	void openDeveloperSettings();
	void openNetplaySettings(); 
	void openRetroachievementsSettings();
	void openMissingBiosSettings();
	void openFormatDriveSettings();
	void exitKidMode();

	// windows
	void openEmulatorSettings();
	void openSystemEmulatorSettings(SystemData* system);

	static void openWifiSettings(Window* win, std::string title, std::string data, const std::function<void(std::string)>& onsave);

	MenuComponent mMenu;
	TextComponent mVersion;

	static std::shared_ptr<OptionListComponent<std::string>> createRatioOptionList(Window *window, std::string configname);
	static std::shared_ptr<OptionListComponent<std::string>> createVideoResolutionModeOptionList(Window *window, std::string configname, std::string configoptname = "videomode");
	static void popSpecificConfigurationGui(Window* mWindow, std::string title, std::string configName, SystemData *systemData, FileData* fileData, bool selectCoreLine = false);

	static void openLatencyReductionConfiguration(Window* mWindow, std::string configName);

	static void addDecorationSetOptionListComponent(Window* window, GuiSettings* parentWindow, const std::vector<DecorationSetInfo>& sets, const std::string& configName = "global");
	static void createDecorationItemTemplate(Window* window, std::vector<DecorationSetInfo> sets, std::string data, ComponentListRow& row);

	static void addFeatureItem(Window* window, GuiSettings* settings, const CustomFeature& feat, const std::string& configName, const std::string& system, const std::string& emulator, const std::string& core);
	static void addFeatures(const VectorEx<CustomFeature>& features, Window* window, GuiSettings* settings, const std::string& configName, const std::string& system, const std::string& emulator, const std::string& core, const std::string& defaultGroupName = "", bool addDefaultGroupOnlyIfNotFirst = false);

	bool checkNetwork();

	static void saveSubsetSettings();
	static void loadSubsetSettings(const std::string themeName);

public:
	static std::vector<DecorationSetInfo> getDecorationsSets(SystemData* system = nullptr);

	virtual bool hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult = nullptr) override;
	virtual bool onMouseClick(int button, bool pressed, int x, int y);
};

#endif // ES_APP_GUIS_GUI_MENU_H
