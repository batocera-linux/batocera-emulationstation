#pragma once

#include <functional>

#include "GuiComponent.h"
#include "Window.h"
#include "components/ImageGridComponent.h"
#include "components/NinePatchComponent.h"
#include "components/ComponentGrid.h"
#include "components/TextComponent.h"
#include "SaveState.h"

class ThemeData;
class FileData;
class SaveStateRepository;

class GuiSaveState : public GuiComponent
{
public:
	GuiSaveState(Window* window, FileData* game, const std::function<void(const SaveState& state)>& callback);

	bool input(InputConfig* config, Input input) override;
	void onSizeChanged() override;
	std::vector<HelpPrompt> getHelpPrompts() override;

protected:
	void centerWindow();
	void loadGrid();

	std::shared_ptr<ImageGridComponent<SaveState>> mGrid;
	std::shared_ptr<ThemeData> mTheme;
	std::shared_ptr<TextComponent>	mTitle;

	NinePatchComponent				mBackground;
	ComponentGrid					mLayout;
	
	std::function<void(const SaveState& state)>			mRunCallback; // batocera

	FileData* mGame;
	SaveStateRepository* mRepository;
};