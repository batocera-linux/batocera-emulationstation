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
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "views/UIModeController.h"

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

	if (!UIModeController::getInstance()->isUIModeFull())
		mSubtitle->setText("");

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

	if (UIModeController::getInstance()->isUIModeFull())
	{
		buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("SAVE"), _("SAVE"), [this] {  save(); delete this; }));

		if (mMapping.isValid())
			buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("DELETE"), _("DELETE"), [this] { deleteMapping(); }));

		buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("CANCEL"), _("CANCEL"), [this] { delete this; }));
	}
	else 
		buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("CLOSE"), _("CLOSE"), [this] { delete this; }));

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
	
	mList->setCursorChangedCallback([&](const CursorState& state)
	{
		updateHelpPrompts();
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
	bool last = (idx == mList->size() - 1);
	mList->clear();

	int i = 0;
	bool gp = false;
	std::string mouseMapping = mMapping.getMouseMapping(mPlayer);


	//	mList->addGroup(_("JOYSTICK MAPPING"));
	//	i++;

	for (auto mappingName : mappingNames)
	{
		if (!mouseMapping.empty() && Utils::String::startsWith(mappingName.name, mouseMapping))
			continue;

		ComponentListRow row;

		KeyMappingFile::KeyMapping km = mMapping.getKeyMapping(mPlayer, mappingName.name);
		if (!UIModeController::getInstance()->isUIModeFull() && km.targets.size() == 0)
			continue;

		if (!gp && mappingName.name.find("+") != std::string::npos && UIModeController::getInstance()->isUIModeFull())
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

		if (UIModeController::getInstance()->isUIModeFull())
		{
			row.makeAcceptInputHandler([this, km, mappingName, accept]
			{
				if (GuiKeyboardLayout::isEnabled())
				{
					std::set<std::string> tgs = km.targets;
					mWindow->pushGui(new GuiKeyboardLayout(mWindow, accept, &tgs));
				}
			});
		}

		mList->addRow(row, i == idx, false, km.targets.size() == 0 ? "" : mappingName.name);
		i++;
	}

	if (UIModeController::getInstance()->isUIModeFull())
	{
		mList->addGroup(_("MOUSE CURSOR"));
		i++;

		ComponentListRow mouseRow;

		auto theme = ThemeData::getMenuTheme();

		auto text = std::make_shared<TextComponent>(mWindow, _("EMULATE MOUSE CURSOR"), theme->Text.font, theme->Text.color);
		mouseRow.addElement(text, true);

		auto imageSource = std::make_shared< OptionListComponent<std::string> >(mWindow, _("EMULATE MOUSE CURSOR"));

		imageSource->addRange({
			{ _("NO"), "" },
			{ _("LEFT ANALOG STICK") , "joystick1" },
			{ _("RIGHT ANALOG STICK") , "joystick2" } }, mouseMapping);

		mouseRow.addElement(imageSource, false, true);

		mList->addRow(mouseRow, idx > 0 && last);

		imageSource->setSelectedChangedCallback([this](const std::string& name)
		{
			if (mMapping.setMouseMapping(mPlayer, name))
			{
				mDirty = true;
				loadList(true);
			}
		});
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
		save();

		// close everything
		Window* window = mWindow;
		while (window->peekGui() && window->peekGui() != ViewController::get())
			delete window->peekGui();

		return true;
	}

	if (mList->size() > 0 && !mList->getSelected().empty())
	{
		std::string mappingName = mList->getSelected();
		std::string mappingDescription = mMapping.getMappingDescription(mPlayer, mappingName);

		if (config->isMappedTo("y", input) && input.value != 0)
		{
			auto updateVal = [this, mappingName](const std::string& newVal)
			{
				if (mMapping.updateMappingDescription(mPlayer, mappingName, newVal))
				{
					mDirty = true;
					loadList(true);
				}
			};

			if (Settings::getInstance()->getBool("UseOSK"))
				mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, _("EDIT DESCRIPTION"), mappingDescription, updateVal, false));
			else
				mWindow->pushGui(new GuiTextEditPopup(mWindow, _("EDIT DESCRIPTION"), mappingDescription, updateVal, false));

			return true;
		}		

		if (config->isMappedTo("x", input) && input.value != 0)
		{
			mWindow->pushGui(new GuiMsgBox(mWindow, _("ARE YOU SURE YOU WANT TO DELETE THIS ITEM ?"), _("YES"), [this, mappingName]
			{
				if (mMapping.removeMapping(mPlayer, mappingName))
				{
					mDirty = true;
					loadList(true);
				}
			}, _("NO"), nullptr));

			return true;
		}
	}

	if (mTabs->input(config, input))
		return true;

	return false;
}

std::vector<HelpPrompt> GuiKeyMappingEditor::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;// = mList->getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("start", _("SAVE")));

	if (mList->size() > 0 && !mList->getSelected().empty())
	{
		prompts.push_back(HelpPrompt("x", _("DELETE MAPPING")));
		prompts.push_back(HelpPrompt("y", _("EDIT DESCRIPTION")));
	}

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

void GuiKeyMappingEditor::updateHelpPrompts()
{
	if (mWindow->peekGui() != this)
		return;

	std::vector<HelpPrompt> prompts = getHelpPrompts();
	mWindow->setHelpPrompts(prompts, getHelpStyle());
}

//////////////////////////////////////////////////////////////
// GuiKeyMappingEditorEntry
//////////////////////////////////////////////////////////////

GuiKeyMappingEditorEntry::GuiKeyMappingEditorEntry(Window* window, MappingInfo& trigger, KeyMappingFile::KeyMapping& target) :
	ComponentGrid(window, Vector2i(trigger.combination.empty()? 5 : 6, 1))
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

//	if (!target.description.empty())
//		mTargetText->setText(mTarget.toTargetString() + _U("      \uF05A ") + target.description);

	mDescription = std::make_shared<TextComponent>(mWindow, mTarget.description.empty() ? "" : _U("\uF05A  ") + mTarget.description, theme->Text.font, theme->Text.color);
	mDescription->setLineSpacing(1.5);

	setEntry(mImage, Vector2i(0, 0), false, true);

	if (mImageCombi != nullptr)
	{
		setEntry(mImageCombi, Vector2i(1, 0), false, true);
		setEntry(mText, Vector2i(3, 0), false, true);
		setEntry(mTargetText, Vector2i(4, 0), false, true);
		setEntry(mDescription, Vector2i(5, 0), false, true);
	}
	else
	{
		setEntry(mText, Vector2i(2, 0), false, true);
		setEntry(mTargetText, Vector2i(3, 0), false, true);
		setEntry(mDescription, Vector2i(4, 0), false, true);
	}

	float h = mText->getSize().y();
	mImage->setMaxSize(h, h * 0.80f);
	mImage->setPadding(Vector4f(8, 8, 8, 8));

	int titleCol = 2;
	float w = h * 1.15f / WINDOW_WIDTH;
	setColWidthPerc(0, h * 1.15f / WINDOW_WIDTH, false);	

	if (mImageCombi != nullptr)
	{
		mImageCombi->setMaxSize(h, h * 0.80f);
		mImageCombi->setPadding(Vector4f(8, 8, 8, 8));

		w += h * 1.15f / WINDOW_WIDTH;
		setColWidthPerc(1, h * 1.15f / WINDOW_WIDTH, false);

		w += 0.015f;
		setColWidthPerc(2, 0.015f, false);

		titleCol++;
	}
	else
	{
		w += 0.015f;
		setColWidthPerc(1, 0.015f, false);
	}

	float ww = WINDOW_WIDTH;
	float titleWidth = (ww * (0.42f - w)) / ww;
	setColWidthPerc(titleCol, titleWidth, false);

	setColWidthPerc(titleCol+2, 0.32f, false);
	
	setSize(0, mText->getSize().y() * 1.0f);
}

void GuiKeyMappingEditorEntry::setColor(unsigned int color)
{
	mImage->setColor(color);

	if (mImageCombi)
		mImageCombi->setColor(color);

	mText->setColor(color);
	mTargetText->setColor(color);
	mDescription->setColor(color);
}




