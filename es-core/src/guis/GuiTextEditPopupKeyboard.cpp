#include "guis/GuiTextEditPopupKeyboard.h"
#include "components/MenuComponent.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include "LocaleES.h"
#include "SystemConf.h"

std::vector<std::vector<const char*>> kbUs {

	{ "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "_", "+" },
	{ "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "-", "=" },

	{ "à", "ä", "è", "ë", "ì", "ï", "ò", "ö", "ù", "ü", "¨", "¿" },
	{ "á", "â", "é", "ê", "í", "î", "ó", "ô", "ú", "û", "ñ", "¡" },

	{ "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "{", "}" },
	{ "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]" },

	{ "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "\"", "|" },
	{ "A", "S", "D", "F", "G", "H", "J", "K", "L", ":", "'", "\\" },

	{ "SHIFT", "~", "z", "x", "c", "v", "b", "n", "m", ",", ".", "?" },
	{ "SHIFT", "`", "Z", "X", "C", "V", "B", "N", "M", "<", ">", "/" },
};

std::vector<std::vector<const char*>> kbFr {
	{ "&", "é", "\"", "'", "(", "#", "è", "!", "ç", "à", ")", "-" },
	{ "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "@", "_" },
	
	{ "à", "ä", "ë", "ì", "ï", "ò", "ö", "ü", "\\", "|", "§", "°" },
	{ "á", "â", "ê", "í", "î", "ó", "ô", "ú", "û", "ñ", "¡", "¿" },
	
	{ "a", "z", "e", "r", "t", "y", "u", "i", "o", "p", "^", "$" },
	{ "A", "Z", "E", "R", "T", "Y", "U", "I", "O", "P", "¨", "*" },

	{ "q", "s", "d", "f", "g", "h", "j", "k", "l", "m", "ù", "`" },
	{ "Q", "S", "D", "F", "G", "H", "J", "K", "L", "M", "%", "£" },
	
	{ "SHIFT", "<", "w", "x", "c", "v", "b", "n", ",", ";", ":", "=" },
	{ "SHIFT", ">", "W", "X", "C", "V", "B", "N", "?", ".", "/", "+" }
};

GuiTextEditPopupKeyboard::GuiTextEditPopupKeyboard(Window* window, const std::string& title, const std::string& initValue,
	const std::function<void(const std::string&)>& okCallback, bool multiLine, const std::string acceptBtnText)
	: GuiComponent(window), mBackground(window, ":/frame.png"), mGrid(window, Vector2i(1, 7)), mMultiLine(multiLine)
{
	setTag("popup");

	mOkCallback = okCallback;

	auto theme = ThemeData::getMenuTheme();
	mBackground.setImagePath(theme->Background.path);
	mBackground.setEdgeColor(theme->Background.color);
	mBackground.setCenterColor(theme->Background.centerColor);
	mBackground.setCornerSize(theme->Background.cornerSize);

	addChild(&mBackground);
	addChild(&mGrid);

	mTitle = std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(title), theme->Title.font, theme->Title.color, ALIGN_CENTER);

	// Accept/Cancel/Delete/Space buttons
	std::vector<std::shared_ptr<ButtonComponent> > buttons;

	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, acceptBtnText, acceptBtnText, [this, okCallback] { okCallback(mText->getValue()); delete this; }));
	auto space = std::make_shared<ButtonComponent>(mWindow, _("SPACE"), _("SPACE"), [this] {
		mText->startEditing();
		mText->textInput(" ");
		mText->stopEditing();
	});

	if (Renderer::isSmallScreen())
		space->setSize(space->getSize().x(), space->getSize().y());
	else
		space->setSize(space->getSize().x() * 3, space->getSize().y());

	buttons.push_back(space);
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("DELETE"), _("DELETE A CHAR"), [this] {
		mText->startEditing();
		mText->textInput("\b");
		mText->stopEditing();
	}));

	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("RESET"), _("RESET"), [this, okCallback] { okCallback(""); delete this; }));

	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("CANCEL"), _("DISCARD CHANGES"), [this] { delete this; }));

	// Add buttons
	mButtons = makeButtonGrid(mWindow, buttons);

	mKeyboardGrid = std::make_shared<ComponentGrid>(mWindow, Vector2i(12, 5));

	mText = std::make_shared<TextEditComponent>(mWindow);
	mText->setValue(initValue);

	if (!multiLine)
		mText->setCursor(initValue.size());

	// Header
	mGrid.setEntry(mTitle, Vector2i(0, 0), false, true);

	// Text edit add
	mGrid.setEntry(mText, Vector2i(0, 1), true, false, Vector2i(1, 1), GridFlags::BORDER_TOP | GridFlags::BORDER_BOTTOM);

	std::vector< std::vector< std::shared_ptr<ButtonComponent> > > buttonList;

	// Keyboard
	// Case for if multiline is enabled, then don't create the keyboard.
	if (!mMultiLine) 
	{
		std::vector<std::vector<const char*>>* layout = &kbUs;

		std::string language = SystemConf::getInstance()->get("system.language");
		if (!language.empty())
		{
			auto shortNameDivider = language.find("_");
			if (shortNameDivider != std::string::npos)
				language = Utils::String::toLower(language.substr(0, shortNameDivider));
		}

		if (language == "fr")
			layout = &kbFr;

		for (unsigned int i = 0; i < 5; i++)
		{
			std::vector<std::shared_ptr<ButtonComponent>> buttons;
			for (unsigned int j = 0; j < 12; j++)
			{
				std::string lower = (*layout)[2 * i][j];
				std::string upper = (*layout)[2 * i + 1][j];

				std::shared_ptr<ButtonComponent> button = nullptr;

				if (lower == "SHIFT")
				{
					// Special case for shift key
					mShiftButton = std::make_shared<ButtonComponent>(mWindow, _U("\uF176"), _("SHIFTS FOR UPPER,LOWER, AND SPECIAL"), [this] {
						shiftKeys();
					}, false);
					
					button = mShiftButton;
				}
				else
					button = makeButton(lower, upper);					

				button->setRenderNonFocusedBackground(false);
				buttons.push_back(button);

				button->setSize(getButtonSize());
				mKeyboardGrid->setEntry(button, Vector2i(j, i), true, false);
				buttonList.push_back(buttons);
			}
		}		
		// END KEYBOARD IF
	}

	// Add keyboard keys
	mGrid.setEntry(mKeyboardGrid, Vector2i(0, 2), true, true, Vector2i(2, 4));
	mGrid.setEntry(mButtons, Vector2i(0, 6), true, false);

	// Determine size from text size
	float textHeight = mText->getFont()->getHeight();
	if (multiLine)
		textHeight *= 6;
	mText->setSize(0, textHeight);

	mGrid.setUnhandledInputCallback([this](InputConfig* config, Input input) -> bool 
	{		
		if (config->isMappedLike("down", input)) 
		{
			mGrid.setCursorTo(mText);
			return true;
		}
		else if (config->isMappedLike("up", input)) 
		{
			mGrid.moveCursor(Vector2i(0, 5));
			return true;
		}
		else if (config->isMappedLike("left", input))
		{		
			if (mGrid.getSelectedComponent() == mKeyboardGrid)
			{
				mKeyboardGrid->moveCursor(Vector2i(11, 0));
				return true;
			}
			else if (mGrid.getSelectedComponent() == mButtons)
			{
				mButtons->moveCursor(Vector2i(4, 0));
				return true;
			}
		}
		else if (config->isMappedLike("right", input))
		{
			if (mGrid.getSelectedComponent() == mKeyboardGrid)
			{
				mKeyboardGrid->moveCursor(Vector2i(-11, 0));
				return true;
			}
			else if (mGrid.getSelectedComponent() == mButtons)
			{
				mButtons->moveCursor(Vector2i(-4, 0));
				return true;
			}
		}

		return false;
	});

	// If multiline, set all diminsions back to default, else draw size for keyboard.
	if (mMultiLine) 
	{
		if (Renderer::isSmallScreen())
			setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
		else
			setSize(Renderer::getScreenWidth() * 0.5f, mTitle->getFont()->getHeight() + textHeight + mKeyboardGrid->getSize().y() + 40);

		setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);
	}
	else 
	{
		if (Renderer::isSmallScreen())
			setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
		else // Set size based on ScreenHieght * .08f by the amount of keyboard rows there are.
			setSize(Renderer::getScreenWidth() * 0.95f, mTitle->getFont()->getHeight() + textHeight + 40 + (Renderer::getScreenHeight() * 0.085f) * 6);

		setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);
	}	
	/*
	mWindow->postToUiThread([this](Window* w)
	{
		mText->startEditing();
	});*/
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

	if (Renderer::isSmallScreen())  // small screens // batocera
	{
		mKeyboardGrid->setSize(getButtonSize().x() * 12.0f, getButtonSize().y() * 5.0f);
		mKeyboardGrid->setPosition(Renderer::getScreenWidth() * 0.05f / 2.00f, mTitle->getFont()->getHeight() + mText->getFont()->getHeight() + 15 + 6);
	}
	else
	{
		mKeyboardGrid->setSize(getButtonSize().x() * 12.2f, getButtonSize().y() * 5.2f); // Small margin between buttons
		mKeyboardGrid->setPosition(Renderer::getScreenWidth() * 0.05f / 2.00f, mTitle->getFont()->getHeight() + mText->getFont()->getHeight() + 40 + 6);
	}
}

bool GuiTextEditPopupKeyboard::input(InputConfig* config, Input input)
{
	if (GuiComponent::input(config, input))
		return true;

	// pressing start
	if (config->isMappedTo("start", input) && input.value)
	{
		if (mOkCallback)
			mOkCallback(mText->getValue());

		delete this;
		return true;
	}

	if ((config->getDeviceId() == DEVICE_KEYBOARD && input.id == SDLK_ESCAPE))
	{
		delete this;
		return true;
	}

	// pressing back when not text editing closes us
	if (config->isMappedTo(BUTTON_BACK, input) && input.value)
	{
		delete this;
		return true;
	}

	// For deleting a chara (Left Top Button)
	if (config->isMappedTo("pageup", input) && input.value) {
		bool editing = mText->isEditing();
		if (!editing)
			mText->startEditing();

		mText->textInput("\b");

		if (!editing)
			mText->stopEditing();
	}

	// For Adding a space (Right Top Button)
	if (config->isMappedTo("pagedown", input) && input.value) 
	{
		bool editing = mText->isEditing();
		if (!editing)
			mText->startEditing();

		mText->textInput(" ");

		if (!editing)
			mText->stopEditing();
	}

	// For Shifting (Y)
	if (config->isMappedTo("y", input) && input.value) 
		shiftKeys();

	if (config->isMappedTo("x", input) && input.value && mOkCallback != nullptr)
	{
		mOkCallback(""); 
		delete this;
		return true;
	}

	return false;
}
/*
void GuiTextEditPopupKeyboard::update(int deltatime) {

}*/

// Shifts the keys when user hits the shift button.
void GuiTextEditPopupKeyboard::shiftKeys() 
{
	mShift = !mShift;

	if (mShift)
		mShiftButton->setColorShift(0xFF0000FF);
	else
		mShiftButton->removeColorShift();

	for (auto & kb : keyboardButtons)
	{
		const std::string& text = mShift ? kb.shiftedKey : kb.key;
		kb.button->setText(text, text, false);
		kb.button->setSize(getButtonSize());
	}
}

std::vector<HelpPrompt> GuiTextEditPopupKeyboard::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mGrid.getHelpPrompts();

	if (mOkCallback != nullptr)
		prompts.push_back(HelpPrompt("x", _("RESET")));

	prompts.push_back(HelpPrompt("y", _("SHIFT")));
	prompts.push_back(HelpPrompt("start", _("OK")));
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("r", _("SPACE")));
	prompts.push_back(HelpPrompt("l", _("DELETE")));
	return prompts;
}

std::shared_ptr<ButtonComponent> GuiTextEditPopupKeyboard::makeButton(const std::string& key, const std::string& shiftedKey)
{
	std::shared_ptr<ButtonComponent> button = std::make_shared<ButtonComponent>(mWindow, key, key, [this, key, shiftedKey] 
	{
		mText->startEditing();

		if (mShift)
			mText->textInput(shiftedKey.c_str());
		else
			mText->textInput(key.c_str());

		mText->stopEditing();
	}, false);
	
	KeyboardButton kb(button, key, shiftedKey);
	keyboardButtons.push_back(kb);
	return button;
}

const Vector2f GuiTextEditPopupKeyboard::getButtonSize()
{
	if (Renderer::isSmallScreen())
	{
		float height = (Renderer::getScreenHeight() - mText->getSize().y() - mTitle->getSize().y() - mButtons->getSize().y()) / 6.0;
		return Vector2f((Renderer::getScreenWidth() * 0.95f) / 12.0f, height);
	}

	return Vector2f((Renderer::getScreenWidth() * 0.89f) / 12.0f, mText->getFont()->getHeight() + 6.0f);
}