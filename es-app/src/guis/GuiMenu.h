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

#ifdef _ENABLEEMUELEC
struct sScreenDimensions
{
	int width;
	int height;
};

struct sScreenBorders
{
	float left;
	float right;
	float top;
	float bottom;
};

#endif

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
	
#ifdef _ENABLEEMUELEC
	void openEmuELECSettings(); /* < emuelec */
    static void openDangerZone(Window* mWindow, std::string configName);
    static void createGamepadConfig(Window* window, GuiSettings* systemConfiguration);
    static void openExternalMounts(Window* mWindow, std::string configName);
		static void addFrameBufferOptions(Window* mWindow, GuiSettings* guiSettings, std::string configName, std::string header, std::string platform);
#endif

	void openSystemSettings();
	void openGamesSettings();	
	void openNetworkSettings(bool selectWifiEnable = false);	
	void openQuitMenu();
	void openSystemInformations();
	void openServicesSettings();
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
#ifdef _ENABLEEMUELEC
  static std::shared_ptr<OptionListComponent<std::string>> createNativeVideoResolutionModeOptionList(Window *window, std::string configname);

  static std::shared_ptr<OptionListComponent<std::string>> createJoyBtnOptionList(Window *window, std::string prefixName, std::string title, int selectId = -1);
  static std::shared_ptr<OptionListComponent<std::string>> createJoyBtnRemapOptionList(Window *window, std::string prefixName, std::string remapName, int btnIndex = -1);

  static void createBtnJoyCfgName(Window *window, GuiSettings *systemConfiguration, std::string prefixName);
  static void createBtnJoyCfgRemap(Window *window, GuiSettings *systemConfiguration, std::string prefixName, std::string remapName, int btnIndex = -1);
  
  static void deleteBtnJoyCfg(Window *window, GuiSettings *systemConfiguration, std::string prefixName);
  static void editJoyBtnRemapOptionList(Window *window, GuiSettings *systemConfiguration, std::string prefixName);
  static void removeJoyBtnEntry(int index);
  static void addJoyBtnEntry(std::string name, std::string val);
#endif
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
#ifdef _ENABLEEMUELEC
  static std::shared_ptr<OptionListComponent<std::string>> btn_choice;
  static std::shared_ptr<OptionListComponent<std::string>> del_choice;
  static std::shared_ptr<OptionListComponent<std::string>> edit_choice;
#endif
};

#endif // ES_APP_GUIS_GUI_MENU_H
