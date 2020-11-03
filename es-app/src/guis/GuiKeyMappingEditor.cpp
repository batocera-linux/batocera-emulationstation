#include "guis/GuiKeyMappingEditor.h"

#include "ApiSystem.h"
#include "components/OptionListComponent.h"
#include "guis/GuiSettings.h"
#include "views/ViewController.h"
#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"
#include "components/ComponentGrid.h"
#include "components/ButtonComponent.h"
#include "components/MultiLineMenuEntry.h"
#include "LocaleES.h"
#include "guis/GuiMsgBox.h"
#include "ContentInstaller.h"
#include "GuiLoading.h"
#include "guis/GuiKeyboardLayout.h"

#include <unordered_set>
#include <algorithm>

#define WINDOW_WIDTH (float)Math::max((int)Renderer::getScreenHeight(), (int)(Renderer::getScreenWidth() * 0.73f))


std::vector<MappingInfo> mappingNames =
{
	{ "up",      "UP",           ":/help/dpad_up.svg" },
	{ "down",    "DOWN",         ":/help/dpad_down.svg" },
	{ "left",    "LEFT",         ":/help/dpad_left.svg" },
	{ "right",   "RIGHT",        ":/help/dpad_right.svg" },
	{ "start",   "START",        ":/help/button_start.svg" },
	{ "select",  "SELECT",       ":/help/button_select.svg" },

#ifdef WIN32 // Invert icons on Windows : xbox controllers A/B are inverted
	{ "a",       "A",    ":/help/buttons_south.svg" },
	{ "b",       "B",   ":/help/buttons_east.svg" },
#else
	{ "a",       "A",    ":/help/buttons_east.svg" },
	{ "b",       "B",   ":/help/buttons_south.svg" },
#endif

	{ "x",               "X",   ":/help/buttons_north.svg" },
	{ "y",               "Y",    ":/help/buttons_west.svg" },

	{ "joystick1up",     "LEFT ANALOG UP",     ":/help/analog_up.svg" },
	{ "joystick1down",   "LEFT ANALOG DOWN",     ":/help/analog_down.svg" },
	{ "joystick1left",   "LEFT ANALOG LEFT",   ":/help/analog_left.svg" },
	{ "joystick1right",  "LEFT ANALOG RIGHT",   ":/help/analog_right.svg" },
	
	{ "joystick2up",     "RIGHT ANALOG UP",     ":/help/analog_up.svg" },
	{ "joystick2down",     "RIGHT ANALOG DOWN",     ":/help/analog_down.svg" },
	{ "joystick2left",   "RIGHT ANALOG LEFT",   ":/help/analog_left.svg" },
	{ "joystick2right",   "RIGHT ANALOG RIGHT",   ":/help/analog_right.svg" },
	{ "pageup",          "L1",      ":/help/button_l.svg" },
	{ "pagedown",        "R1",     ":/help/button_r.svg" },
	{ "l2",              "L2",       ":/help/button_lt.svg" },
	{ "r2",              "R2",      ":/help/button_rt.svg" },
	{ "l3",              "L3",       ":/help/analog_thumb.svg" },
	{ "r3",              "R3",      ":/help/analog_thumb.svg" },

	//{ "hotkey",          "HOTKEY",      ":/help/button_hotkey.svg" },
	
	{ "hotkey + start",          "HOTKEY + START",      ":/help/button_hotkey.svg", ":/help/button_start.svg" },

#ifdef WIN32 // Invert icons on Windows : xbox controllers A/B are inverted
	{ "hotkey + a",       "HOTKEY + A",    ":/help/button_hotkey.svg", ":/help/buttons_south.svg" },
	{ "hotkey + b",       "HOTKEY + B",   ":/help/button_hotkey.svg", ":/help/buttons_east.svg" },
#else
	{ "hotkey + a",       "HOTKEY + A",   ":/help/button_hotkey.svg", ":/help/buttons_east.svg" },
	{ "hotkey + b",       "HOTKEY + B",   ":/help/button_hotkey.svg", ":/help/buttons_south.svg" },
#endif

	{ "hotkey + x",       "HOTKEY + X",    ":/help/button_hotkey.svg", ":/help/buttons_north.svg" },
	{ "hotkey + y",       "HOTKEY + Y",   ":/help/button_hotkey.svg", ":/help/buttons_west.svg" }
};

GuiKeyMappingEditor::GuiKeyMappingEditor(Window* window, IKeyboardMapContainer* mapping)
	: GuiComponent(window), mGrid(window, Vector2i(1, 4)), mBackground(window, ":/frame.png")
{
	mPlayer = 0;
	mMapping = mapping->getKeyboardMapping();
	mDirty = false;

	//mMapping.players[0].mappings

	addChild(&mBackground);
	addChild(&mGrid);

	// Form background
	auto theme = ThemeData::getMenuTheme();
	mBackground.setImagePath(theme->Background.path);
	mBackground.setEdgeColor(theme->Background.color);
	mBackground.setCenterColor(theme->Background.centerColor);
	mBackground.setCornerSize(theme->Background.cornerSize);

	// Title
	mHeaderGrid = std::make_shared<ComponentGrid>(mWindow, Vector2i(1, 5));

	mTitle = std::make_shared<TextComponent>(mWindow, _("PAD TO KEYBOARD CONFIGURATION"), theme->Title.font, theme->Title.color, ALIGN_CENTER); // batocera
	mSubtitle = std::make_shared<TextComponent>(mWindow, _("SELECT ACTIONS TO CHANGE"), theme->TextSmall.font, theme->TextSmall.color, ALIGN_CENTER);
	mHeaderGrid->setEntry(mTitle, Vector2i(0, 1), false, true);
	mHeaderGrid->setEntry(mSubtitle, Vector2i(0, 3), false, true);

	mGrid.setEntry(mHeaderGrid, Vector2i(0, 0), false, true);

	// Tabs
	mTabs = std::make_shared<ComponentTab>(mWindow);
	mGrid.setEntry(mTabs, Vector2i(0, 1), false, true);	

	// Entries
	mList = std::make_shared<ComponentList>(mWindow);
	mGrid.setEntry(mList, Vector2i(0, 2), true, true);

	// Buttons
	std::vector< std::shared_ptr<ButtonComponent> > buttons;
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("SAVE"), _("SAVE"), [this] {  save(); delete this; }));

	if (mMapping.isValid())
		buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("DELETE"), _("DELETE"), [this] { deleteMapping(); }));

	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("CANCEL"), _("CANCEL"), [this] { delete this; }));

	mButtonGrid = makeButtonGrid(mWindow, buttons);
	mGrid.setEntry(mButtonGrid, Vector2i(0, 3), true, false);

	mGrid.setUnhandledInputCallback([this](InputConfig* config, Input input) -> bool
	{
		if (config->isMappedLike("down", input)) { mGrid.setCursorTo(mList); mList->setCursorIndex(0); return true; }
		if (config->isMappedLike("up", input)) { mList->setCursorIndex(mList->size() - 1); mGrid.moveCursor(Vector2i(0, 1)); return true; }
		return false;
	});

	mTabs->addTab(_("PLAYER 1"), "0", true);	
	mTabs->addTab(_("PLAYER 2"), "1");
//	mTabs->addTab(_("PLAYER 3"), "2");
//	mTabs->addTab(_("PLAYER 4"), "3");

	mTabs->setCursorChangedCallback([&](const CursorState& state)
	{
		mPlayer = atoi(mTabs->getSelected().c_str());
		loadList(false);
	});
	
	loadList(false);
	centerWindow();
}

void GuiKeyMappingEditor::onSizeChanged()
{
	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));

	mGrid.setSize(mSize);

	const float titleHeight = mTitle->getFont()->getLetterHeight();
	const float subtitleHeight = mSubtitle->getFont()->getLetterHeight();
	const float titleSubtitleSpacing = mSize.y() * 0.03f;

	mGrid.setRowHeightPerc(0, (titleHeight + titleSubtitleSpacing + subtitleHeight + TITLE_VERT_PADDING) / mSize.y());

	if (mTabs->size() == 0)
		mGrid.setRowHeightPerc(1, 0.00001f);
	else 
		mGrid.setRowHeightPerc(1, (titleHeight + titleSubtitleSpacing) / mSize.y());

	mGrid.setRowHeightPerc(3, mButtonGrid->getSize().y() / mSize.y());

	mHeaderGrid->setRowHeightPerc(1, titleHeight / mHeaderGrid->getSize().y());
	mHeaderGrid->setRowHeightPerc(2, titleSubtitleSpacing / mHeaderGrid->getSize().y());
	mHeaderGrid->setRowHeightPerc(3, subtitleHeight / mHeaderGrid->getSize().y());
}

void GuiKeyMappingEditor::centerWindow()
{
	if (Renderer::isSmallScreen())
		setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	else
		setSize(WINDOW_WIDTH, Renderer::getScreenHeight() * 0.875f);

	setPosition((Renderer::getScreenWidth() - getSize().x()) / 2, (Renderer::getScreenHeight() - getSize().y()) / 2);
}

static bool sortPackagesByGroup(PacmanPackage& sys1, PacmanPackage& sys2)
{
	std::string name1 = Utils::String::toUpper(sys1.group);
	std::string name2 = Utils::String::toUpper(sys2.group);

	if (name1 == name2)
		return Utils::String::compareIgnoreCase(sys1.description, sys2.description) < 0;		

	return name1.compare(name2) < 0;
}

void GuiKeyMappingEditor::loadList(bool restoreIndex)
{
	int idx = !restoreIndex ? -1 : mList->getCursorIndex();
	mList->clear();

	bool gp = false;

	int i = 0;
	for (auto mappingName : mappingNames)
	{		
		ComponentListRow row;

		KeyMappingFile::PlayerMapping pm;
		if (mPlayer < mMapping.players.size())
			pm = mMapping.players[mPlayer];

		KeyMappingFile::KeyMapping km;		
		for (auto mapping : pm.mappings)
			if (mapping.triggerEquals(mappingName.name))
				km = mapping;
		

		if (!gp && mappingName.name.find("+") != std::string::npos)
		{
			mList->addGroup(_("COMBINATIONS"));
			gp = true;
			i++;
		}

		auto accept = [this, mappingName](const std::set<std::string>& target)
		{
			if (mMapping.updateMapping(mPlayer, mappingName.name, target))
			{
				mDirty = true;
				loadList(true);
			}
		};

		auto grid = std::make_shared<GuiKeyMappingEditorEntry>(mWindow, mappingName, km);
		row.addElement(grid, true);
		row.makeAcceptInputHandler([this, km, mappingName, accept]
		{  			
			if (GuiKeyboardLayout::isEnabled())
			{
				std::set<std::string> tgs = km.targets;
				mWindow->pushGui(new GuiKeyboardLayout(mWindow, accept, &tgs));
			}
		});

		mList->addRow(row, i == idx, false);
		i++;
	}

	centerWindow();
}

bool GuiKeyMappingEditor::input(InputConfig* config, Input input)
{
	if(GuiComponent::input(config, input))
		return true;
	
	if(input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		close();
		return true;
	}

	if(config->isMappedTo("start", input) && input.value != 0)
	{
		// close everything
		Window* window = mWindow;
		while(window->peekGui() && window->peekGui() != ViewController::get())
			delete window->peekGui();
	}

	if (mTabs->input(config, input))
		return true;

	return false;
}

std::vector<HelpPrompt> GuiKeyMappingEditor::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mList->getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("start", _("SAVE")));
	return prompts;
}

void GuiKeyMappingEditor::close()
{
	if (!mDirty)
	{
		delete this; 
		return;
	}
	
	mWindow->pushGui(new GuiMsgBox(mWindow,
		_("SAVE CHANGES?"),
		_("YES"), [this] { save(); delete this; },
		_("NO"), [this] { delete this; }
	));	
}

void GuiKeyMappingEditor::deleteMapping()
{
	mWindow->pushGui(new GuiMsgBox(mWindow,
		_("ARE YOU SURE ?"),
		_("YES"), [this] { mMapping.deleteFile(); delete this; },
		_("NO"), [this] { }
	));
}

void GuiKeyMappingEditor::save()
{
	if (!mDirty)
		return;

	mMapping.save();
	mDirty = false;
}

//////////////////////////////////////////////////////////////
// GuiKeyMappingEditorEntry
//////////////////////////////////////////////////////////////

GuiKeyMappingEditorEntry::GuiKeyMappingEditorEntry(Window* window, MappingInfo& trigger, KeyMappingFile::KeyMapping& target) :
	ComponentGrid(window, Vector2i(trigger.combination.empty()? 4 : 5, 1))
{
	mMappingInfo = trigger;
	mTarget = target;

	auto theme = ThemeData::getMenuTheme();

	mImage = std::make_shared<ImageComponent>(mWindow);
	mImage->setColor(theme->Text.color);
	mImage->setImage(mMappingInfo.icon);

	if (!trigger.combination.empty())
	{
		mImageCombi = std::make_shared<ImageComponent>(mWindow);
		mImageCombi->setColor(theme->Text.color);
		mImageCombi->setImage(mMappingInfo.combination);
	}

	mText = std::make_shared<TextComponent>(mWindow, _(mMappingInfo.dispName.c_str()), theme->Text.font, theme->Text.color);
	mText->setLineSpacing(1.5);

	mTargetText = std::make_shared<TextComponent>(mWindow, mTarget.toTargetString(), theme->Text.font, theme->Text.color);
	mTargetText->setLineSpacing(1.5);

	setEntry(mImage, Vector2i(0, 0), false, true);

	if (mImageCombi != nullptr)
	{
		setEntry(mImageCombi, Vector2i(1, 0), false, true);
		setEntry(mText, Vector2i(3, 0), false, true);
		setEntry(mTargetText, Vector2i(4, 0), false, true);
	}
	else
	{
		setEntry(mText, Vector2i(2, 0), false, true);
		setEntry(mTargetText, Vector2i(3, 0), false, true);
	}

	float h = mText->getSize().y();
	mImage->setMaxSize(h, h * 0.85f);
	mImage->setPadding(Vector4f(8, 8, 8, 8));

	setColWidthPerc(0, h * 1.15f / WINDOW_WIDTH, false);	

	if (mImageCombi != nullptr)
	{
		mImageCombi->setMaxSize(h, h * 0.85f);
		mImageCombi->setPadding(Vector4f(8, 8, 8, 8));

		setColWidthPerc(1, h * 1.15f / WINDOW_WIDTH, false);
		setColWidthPerc(2, 0.015f, false);
	}
	else
		setColWidthPerc(1, 0.015f, false);	


	setSize(0, mText->getSize().y() * 1.1f);
}

void GuiKeyMappingEditorEntry::setColor(unsigned int color)
{
	mImage->setColor(color);

	if (mImageCombi)
		mImageCombi->setColor(color);

	mText->setColor(color);
	mTargetText->setColor(color);
}




