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

struct SaveStateItem
{
	SaveStateItem() { saveState = nullptr; }
	SaveStateItem(SaveState* save) { saveState = save; }

	SaveState* saveState;
};

class GuiSaveState : public GuiComponent
{
public:
	GuiSaveState(Window* window, FileData* game, const std::function<void(SaveState* state)>& callback);

	bool input(InputConfig* config, Input input) override;
	void onSizeChanged() override;
	std::vector<HelpPrompt> getHelpPrompts() override;

	bool hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult = nullptr) override;
	bool onMouseClick(int button, bool pressed, int x, int y);


protected:
	void centerWindow();
	void loadGrid();

	std::shared_ptr<ImageGridComponent<SaveStateItem>> mGrid;
	std::shared_ptr<ThemeData> mTheme;
	std::shared_ptr<TextComponent>	mTitle;

	NinePatchComponent				mBackground;
	ComponentGrid					mLayout;
	
	std::function<void(SaveState* state)>			mRunCallback;

	FileData* mGame;
	SaveStateRepository* mRepository;
};