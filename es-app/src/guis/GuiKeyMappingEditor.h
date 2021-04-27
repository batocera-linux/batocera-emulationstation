#pragma once

#include <string>
#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/ComponentGrid.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "components/ComponentTab.h"
#include "KeyboardMapping.h"

template<typename T>
class OptionListComponent;

struct MappingInfo
{
	std::string name;
	std::string dispName;
	std::string icon;
	std::string combination;
};


class GuiKeyMappingEditorEntry : public ComponentGrid
{
public:
	GuiKeyMappingEditorEntry(Window* window, MappingInfo& trigger, KeyMappingFile::KeyMapping& target);
	virtual void setColor(unsigned int color);

private:
	std::shared_ptr<ImageComponent>  mImage;
	std::shared_ptr<ImageComponent>  mImageCombi;
	std::shared_ptr<TextComponent>  mText;
	std::shared_ptr<TextComponent>  mTargetText;
	std::shared_ptr<TextComponent>  mDescription;

	MappingInfo mMappingInfo;
	KeyMappingFile::KeyMapping mTarget;
};

// Batocera
class GuiKeyMappingEditor : public GuiComponent
{
public:
	GuiKeyMappingEditor(Window* window, IKeyboardMapContainer* mapping, bool editable);

	bool input(InputConfig* config, Input input) override;
	void save();
	void deleteMapping();
	void close();

	virtual std::vector<HelpPrompt> getHelpPrompts() override;
	virtual void onSizeChanged() override;

private:
	void initMappingNames();
	void updateHelpPrompts();
	void loadList(bool restoreIndex = true);
	void centerWindow();	

	NinePatchComponent				mBackground;
	ComponentGrid					mGrid;

	std::shared_ptr<TextComponent>	mTitle;
	std::shared_ptr<TextComponent>	mSubtitle;

	std::shared_ptr<ComponentList>	mList;

	std::shared_ptr<ComponentGrid>	mHeaderGrid;
	std::shared_ptr<ComponentGrid>	mButtonGrid;

	std::shared_ptr<ComponentTab>	mTabs;

	KeyMappingFile					mMapping;

	int								mPlayer;
	bool							mDirty;

	std::vector<MappingInfo>		mMappingNames;

	bool							mEditable;
};
