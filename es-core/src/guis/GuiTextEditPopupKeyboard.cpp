#include "guis/GuiTextEditPopupKeyboard.h"
#include "components/MenuComponent.h"
#include "Log.h"
#include "LocaleES.h"

GuiTextEditPopupKeyboard::GuiTextEditPopupKeyboard(Window* window, const std::string& title, const std::string& initValue,
	const std::function<void(const std::string&)>& okCallback, bool multiLine, const std::string acceptBtnText)
	: GuiComponent(window), mBackground(window, ":/frame.png"), mGrid(window, Vector2i(1, 7)), mMultiLine(multiLine)
{
	addChild(&mBackground);
	addChild(&mGrid);

	mTitle = std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(title), Font::get(FONT_SIZE_LARGE), 0x555555FF, ALIGN_CENTER);
	mKeyboardGrid = std::make_shared<ComponentGrid>(mWindow, Vector2i(12, 5));

	mText = std::make_shared<TextEditComponent>(mWindow);
	mText->setValue(initValue);

	if (!multiLine)
		mText->setCursor(initValue.size());

	// Header
	mGrid.setEntry(mTitle, Vector2i(0, 0), false, true);

	// Text edit add
	mGrid.setEntry(mText, Vector2i(0, 1), true, false, Vector2i(1, 1), GridFlags::BORDER_TOP | GridFlags::BORDER_BOTTOM);


	// Keyboard
	// Case for if multiline is enabled, then don't create the keyboard.
	if (!mMultiLine) {

		// Locale for shifting upper/lower case
		std::locale loc;

		// Digit Row & Special Chara.
		for (int k = 0; k < 12; k++) {
			// Create string for button display name.
			std::string strName = "";
			strName += numRow[k];
			strName += " ";
			strName += numRowUp[k];

			// Init button and store in Vector
			digitButtons.push_back(std::make_shared<ButtonComponent>
				(mWindow, strName, numRow[k], [this, k, loc] {
				mText->startEditing();
				if (mShift) mText->textInput(numRowUp[k]);
				else mText->textInput(numRow[k]);
				mText->stopEditing();
			}));

			// Send just created button into mGrid
			digitButtons[k]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
			mKeyboardGrid->setEntry(digitButtons[k], Vector2i(k, 0), true, false);
		}

		// Accent Row & Special Chara.
		for (int k = 0; k < 12; k++) {
			// Create string for button display name.
			std::string strName = "";
			strName += specialRow[k];
			strName += " ";
			strName += specialRowUp[k];

			// Init button and store in Vector
			sButtons.push_back(std::make_shared<ButtonComponent>
				(mWindow, strName, specialRow[k], [this, k, loc] {
				mText->startEditing();
				if (mShift) mText->textInput(specialRowUp[k]);
				else mText->textInput(specialRow[k]);
				mText->stopEditing();
			}));

			// Send just created button into mGrid
			sButtons[k]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
			mKeyboardGrid->setEntry(sButtons[k], Vector2i(k, 1), true, false);
		}

		// Top row [Q - P]
		for (int k = 0; k < 10; k++) {
			kButtons.push_back(std::make_shared<ButtonComponent>
				(mWindow, topRowUp[k], topRowUp[k], [this, k, loc] {
				mText->startEditing();
				if (mShift) mText->textInput(topRowUp[k]);
				else mText->textInput(topRow[k]);
				mText->stopEditing();
			}));
		}

		// Top row - Add in the last three manualy because they're special chara [{ , }]
		for (int k = 10; k < 12; k++) {
			kButtons.push_back(std::make_shared<ButtonComponent>(mWindow, topRow[k], topRow[k], [this, k] {
				mText->startEditing();
				if (mShift) mText->textInput(topRowUp[k]);
				else mText->textInput(topRow[k]);
				mText->stopEditing();
			}));
		}

		for (int k = 0; k < 12; k++) {
			kButtons[k]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
			mKeyboardGrid->setEntry(kButtons[k], Vector2i(k, 2), true, false);
		}

		// Home row [A - L]
		for (int k = 0; k < 9; k++) {
			hButtons.push_back(std::make_shared<ButtonComponent>
				(mWindow, homeRowUp[k], homeRowUp[k], [this, k, loc] {
				mText->startEditing();
				if (mShift) mText->textInput(homeRowUp[k]);
				else mText->textInput(homeRow[k]);
				mText->stopEditing();
			}));
		}

		// Home row - Add in the last three manualy because they're special chara [" , |]
		for (int k = 9; k < 12; k++) {
			hButtons.push_back(std::make_shared<ButtonComponent>(mWindow, homeRow[k], homeRow[k], [this, k] {
				mText->startEditing();
				if (mShift) mText->textInput(homeRowUp[k]);
				else mText->textInput(homeRow[k]);
				mText->stopEditing();
			}));
		}

		for (int k = 0; k < 12; k++) {
			hButtons[k]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
			mKeyboardGrid->setEntry(hButtons[k], Vector2i(k, 3), true, false);
		}

		// Special case for shift key
		bButtons.push_back(std::make_shared<ButtonComponent>(mWindow, "SHIFT", _("SHIFTS FOR UPPER,LOWER, AND SPECIAL"), [this] {
			if (mShift) mShift = false;
			else mShift = true;
			shiftKeys();
		}));

		// Bottom row - Add in the first manualy because it is a special chara [~]
		for (int k = 0; k < 1; k++) {
			bButtons.push_back(std::make_shared<ButtonComponent>(mWindow, bottomRow[0], bottomRow[0], [this, k] {
				mText->startEditing();
				if (mShift) mText->textInput(bottomRowUp[k]);
				else mText->textInput(bottomRow[k]);
				mText->stopEditing();
			}));
		}

		// Bottom row [Z - M]
		for (int k = 1; k < 8; k++) {
			bButtons.push_back(std::make_shared<ButtonComponent>
				(mWindow, bottomRowUp[k], bottomRowUp[k], [this, k, loc] {
				mText->startEditing();
				if (mShift) mText->textInput(bottomRowUp[k]);
				else mText->textInput(bottomRow[k]);
				mText->stopEditing();
			}));
		}

		// Bottom row - Add in the last three manualy because they're special chara [< , > , /]
		for (int k = 8; k < 11; k++) {
			bButtons.push_back(std::make_shared<ButtonComponent>(mWindow, bottomRow[k], bottomRow[k], [this, k] {
				mText->startEditing();
				if (mShift) mText->textInput(bottomRowUp[k]);
				else mText->textInput(bottomRow[k]);
				mText->stopEditing();
			}));
		}

		// Do a sererate for loop because shift key makes it weird
		for (int k = 0; k < 11; k++) {
			bButtons[k]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
			mKeyboardGrid->setEntry(bButtons[k], Vector2i(k, 4), true, false);
		}

		// END KEYBOARD IF
	}

	// Add keyboard keys
	mGrid.setEntry(mKeyboardGrid, Vector2i(0, 2), true, true, Vector2i(2, 4));

	// Accept/Cancel/Delete/Space buttons
	std::vector<std::shared_ptr<ButtonComponent> > buttons;

	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, acceptBtnText, acceptBtnText, [this, okCallback] { okCallback(mText->getValue()); delete this; }));
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("SPACE"), _("SPACE"), [this] {
		mText->startEditing();
		mText->textInput(" ");
		mText->stopEditing();
	}));
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("DELETE"), _("DELETE A CHAR"), [this] {
		mText->startEditing();
		mText->textInput("\b");
		mText->stopEditing();
	}));
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("CANCEL"), _("DISCARD CHANGES"), [this] { delete this; }));

	// Add buttons
	mButtons = makeButtonGrid(mWindow, buttons);
	mGrid.setEntry(mButtons, Vector2i(0, 6), true, false);

	// Determine size from text size
	float textHeight = mText->getFont()->getHeight();
	if (multiLine)
		textHeight *= 6;
	mText->setSize(0, textHeight);

	// If multiline, set all diminsions back to default, else draw size for keyboard.
	if (mMultiLine) {
		setSize(Renderer::getScreenWidth() * 0.5f, mTitle->getFont()->getHeight() + textHeight + mKeyboardGrid->getSize().y() + 40);
		setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);
	}
	else {
		// Set size based on ScreenHieght * .08f by the amount of keyboard rows there are.
		setSize(Renderer::getScreenWidth() * 0.99f, mTitle->getFont()->getHeight() + textHeight + 40 + (Renderer::getScreenHeight() * 0.085f) * 6);
		setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);
	}
}


void GuiTextEditPopupKeyboard::onSizeChanged()
{
	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));

	mText->setSize(mSize.x() - 40, mText->getSize().y());

	// update grid
	mGrid.setRowHeightPerc(0, mTitle->getFont()->getHeight() / mSize.y());
	mGrid.setRowHeightPerc(2, mKeyboardGrid->getSize().y() / mSize.y());
	mGrid.setRowHeightPerc(6, mButtons->getSize().y() / mSize.y());

	mGrid.setSize(mSize);

	// force the keyboard size and position here
	// for an unknown reason, without setting that, the position is "sometimes" (1/2 on s905x for example) not displayed correctly
	// as if a variable were not correctly initialized
	mKeyboardGrid->setSize(Renderer::getScreenWidth() * 0.95f, (mText->getFont()->getHeight() + 6) * 5);

	if(Renderer::getScreenWidth() < 400 && Renderer::getScreenHeight() < 400) { // small screens // batocera
	  mKeyboardGrid->setPosition(Renderer::getScreenWidth() * 0.05f / 2.00f, mTitle->getFont()->getHeight() + mText->getFont()->getHeight() + 15 + 6);
	} else {
	  mKeyboardGrid->setPosition(Renderer::getScreenWidth() * 0.05f / 2.00f, mTitle->getFont()->getHeight() + mText->getFont()->getHeight() + 40 + 6);
	}
}

bool GuiTextEditPopupKeyboard::input(InputConfig* config, Input input)
{
	if (GuiComponent::input(config, input))
		return true;

	// pressing back when not text editing closes us
	if (config->isMappedTo(BUTTON_BACK, input) && input.value)
	{
		delete this;
		return true;
	}

	// For deleting a chara (Left Top Button)
	if (config->isMappedTo("PageUp", input) && input.value) {
		mText->startEditing();
		mText->textInput("\b");
		mText->stopEditing();
	}

	// For Adding a space (Right Top Button)
	if (config->isMappedTo("PageDown", input) && input.value) {
		mText->startEditing();
		mText->textInput(" ");
	}

	// For Shifting (Y)
	if (config->isMappedTo("y", input) && input.value) {
		if (mShift) mShift = false;
		else mShift = true;
		shiftKeys();
	}

	

	return false;
}

void GuiTextEditPopupKeyboard::update(int deltatime) {

}

// Shifts the keys when user hits the shift button.
void GuiTextEditPopupKeyboard::shiftKeys() {
	if (mShift) {
		// FOR SHIFTING UP
		// Change Shift button color
		bButtons[0]->setColorShift(0xEBFD00AA);
		// Change Special chara
		kButtons[10]->setText("[", "[");
		kButtons[11]->setText("]", "]");
		hButtons[9]->setText(":", ":");
		hButtons[10]->setText("'", "'");
		hButtons[11]->setText("\\", "\\");
		bButtons[1]->setText("`", "`");
		bButtons[9]->setText("<", "<");
		bButtons[10]->setText(">", ">");
		bButtons[11]->setText("/", "/");
		// Resize Special chara key
		kButtons[10]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
		kButtons[11]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
		hButtons[9]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
		hButtons[10]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
		hButtons[11]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
		bButtons[1]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
		bButtons[9]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
		bButtons[10]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
		bButtons[11]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
	} else {
		// UNSHIFTING
		// Remove button color
		bButtons[0]->removeColorShift();
		// Change Special chara
		kButtons[10]->setText("{", "{");
		kButtons[11]->setText("}", "}");
		hButtons[9]->setText(";", ";");
		hButtons[10]->setText("\"", "\"");
		hButtons[11]->setText("|", "|");
		bButtons[1]->setText("~", "~");
		bButtons[9]->setText(",", ",");
		bButtons[10]->setText(".", ".");
		bButtons[11]->setText("?", "?");
		// Resize Special chara key
		kButtons[10]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
		kButtons[11]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
		hButtons[9]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
		hButtons[10]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
		hButtons[11]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
		bButtons[1]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
		bButtons[9]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
		bButtons[10]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
		bButtons[11]->setSize((Renderer::getScreenWidth() * 0.95f) / 12, (mText->getFont()->getHeight() + 6));
	}

}

std::vector<HelpPrompt> GuiTextEditPopupKeyboard::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mGrid.getHelpPrompts();
	prompts.push_back(HelpPrompt("y", _("SHIFT")));
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("r", _("SPACE")));
	prompts.push_back(HelpPrompt("l", _("DELETE")));
	return prompts;
}

