#pragma once
#ifndef ES_APP_GUIS_GUI_GAME_OPTIONS_H
#define ES_APP_GUIS_GUI_GAME_OPTIONS_H

#include "components/MenuComponent.h"
#include "components/OptionListComponent.h"
#include "FileData.h"
#include "GuiComponent.h"

class IGameListView;
class SystemData;

class GuiGameOptions : public GuiComponent
{
public:
	GuiGameOptions(Window* window, FileData* game);
	virtual ~GuiGameOptions();

	virtual bool input(InputConfig* config, Input input) override;
	virtual std::vector<HelpPrompt> getHelpPrompts() override;
	virtual HelpStyle getHelpStyle() override;

	static std::vector<std::string> gridSizes;

	void close();

private:
	static void deleteGame(FileData* file);

	inline void addSaveFunc(const std::function<void()>& func) { mSaveFuncs.push_back(func); };		
	void openMetaDataEd();

	std::string getCustomCollectionName();
	void deleteCollection();

	MenuComponent mMenu;

	FileData* mGame;
	SystemData* mSystem;
	IGameListView* getGamelist();

	bool mHasAdvancedGameOptions;


	std::vector<std::function<void()>> mSaveFuncs;
	bool mReloadAll;	
};

#endif // ES_APP_GUIS_GUI_GAME_OPTIONS_H
