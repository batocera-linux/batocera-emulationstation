#include "guis/GuiTextEditPopupKeyboard.h"
#include "components/MenuComponent.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include "LocaleES.h"
#include "SystemConf.h"

#define OSK_WIDTH (Renderer::ScreenSettings::fullScreenMenus() ? Renderer::getScreenWidth() : Renderer::getScreenWidth() * 0.78f)
#define OSK_HEIGHT (Renderer::ScreenSettings::fullScreenMenus() ? Renderer::getScreenHeight() : Renderer::getScreenHeight() * 0.60f)

#define OSK_PADDINGX (Renderer::getScreenWidth() * 0.02f)
#define OSK_PADDINGY (Renderer::getScreenWidth() * 0.01f)

#define BUTTON_GRID_HORIZ_PADDING (Renderer::getScreenWidth()*0.0052083333)
#define BUTTON_LAYER_SIZE (4)

std::vector<std::vector<const char*>> kbUs {

	{ "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "_", "+", "DEL" },
	{ "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "-", "=", "DEL" },
	{ "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "_", "+", "DEL" },
	{ "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "-", "=", "DEL" },

	{ "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "{", "}", "OK" },
	{ "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "OK" },
	{ "à", "ä", "è", "ë", "ì", "ï", "ò", "ö", "ù", "ü", "¨", "¿", "OK" },
	{ "à", "ä", "è", "ë", "ì", "ï", "ò", "ö", "ù", "ü", "¨", "¿", "OK" },

	{ "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "\"", "|", "-rowspan-" },
	{ "A", "S", "D", "F", "G", "H", "J", "K", "L", ":", "'", "\\", "-rowspan-" },
	{ "á", "â", "é", "ê", "í", "î", "ó", "ô", "ú", "û", "ñ", "¡", "-rowspan-" },
	{ "á", "â", "é", "ê", "í", "î", "ó", "ô", "ú", "û", "ñ", "¡", "-rowspan-" },

	{ "~", "z", "x", "c", "v", "b", "n", "m", ",", ".", "?", "ALT", "-colspan-" },
	{ "`", "Z", "X", "C", "V", "B", "N", "M", "<", ">", "/", "ALT", "-colspan-" },
	{ "€", "", "", "", "", "", "", "", "", "", "", "ALT", "-colspan-" },
	{ "€", "", "", "", "", "", "", "", "", "", "", "ALT", "-colspan-" },

	{ "SHIFT", "-colspan-", "SPACE", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "RESET", "-colspan-", "CANCEL", "-colspan-" },
	{ "SHIFT", "-colspan-", "SPACE", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "RESET", "-colspan-", "CANCEL", "-colspan-" },
	{ "SHIFT", "-colspan-", "SPACE", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "RESET", "-colspan-", "CANCEL", "-colspan-" },
	{ "SHIFT", "-colspan-", "SPACE", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "RESET", "-colspan-", "CANCEL", "-colspan-" }
};

std::vector<std::vector<const char*>> kbFr {
	{ "&", "é", "\"", "'", "(", "#", "è", "!", "ç", "à", ")", "-", "DEL" },
	{ "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "@", "_", "DEL" },
	{ "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "@", "_", "DEL" },
	{ "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "@", "_", "DEL" },

	{ "a", "z", "e", "r", "t", "y", "u", "i", "o", "p", "^", "$", "OK" },
	{ "A", "Z", "E", "R", "T", "Y", "U", "I", "O", "P", "¨", "*", "OK" },
	{ "à", "ä", "ë", "ì", "ï", "ò", "ö", "ü", "\\", "|", "§", "°", "OK" },
	{ "à", "ä", "ë", "ì", "ï", "ò", "ö", "ü", "\\", "|", "§", "°", "OK" },

	{ "q", "s", "d", "f", "g", "h", "j", "k", "l", "m", "ù", "`", "-rowspan-" },
	{ "Q", "S", "D", "F", "G", "H", "J", "K", "L", "M", "%", "£", "-rowspan-" },
	{ "á", "â", "ê", "í", "î", "ó", "ô", "ú", "û", "ñ", "¡", "¿", "-rowspan-" },
	{ "á", "â", "ê", "í", "î", "ó", "ô", "ú", "û", "ñ", "¡", "¿", "-rowspan-" },

	{ "<", "w", "x", "c", "v", "b", "n", ",", ";", ":", "=", "ALT", "-colspan-" },
	{ ">", "W", "X", "C", "V", "B", "N", "?", ".", "/", "+", "ALT", "-colspan-" },
	{ "€", "", "", "", "", "", "", "", "", "", "", "ALT", "-colspan-" },
	{ "€", "", "", "", "", "", "", "", "", "", "", "ALT", "-colspan-" },

	{ "SHIFT", "-colspan-", "SPACE", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "RESET", "-colspan-", "CANCEL", "-colspan-" },
	{ "SHIFT", "-colspan-", "SPACE", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "RESET", "-colspan-", "CANCEL", "-colspan-" },
	{ "SHIFT", "-colspan-", "SPACE", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "RESET", "-colspan-", "CANCEL", "-colspan-" },
	{ "SHIFT", "-colspan-", "SPACE", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "RESET", "-colspan-", "CANCEL", "-colspan-" }
};

std::vector<std::vector<const char*>> kbKr{
	{ "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "DEL" },
	{ "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", "DEL" },
	{ "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "DEL" },
	{ "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", "DEL" },

	{ "ㅂ", "ㅈ", "ㄷ", "ㄱ", "ㅅ", "ㅛ", "ㅕ", "ㅑ", "ㅐ", "ㅔ", "[", "]", "OK" },
	{ "ㅃ", "ㅉ", "ㄸ", "ㄲ", "ㅆ", "ㅛ", "ㅕ", "ㅑ", "ㅒ", "ㅖ", "{", "}", "OK" },
	{ "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "OK" },
	{ "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "{", "}", "OK" },

	{ "ㅁ", "ㄴ", "ㅇ", "ㄹ", "ㅎ", "ㅗ", "ㅓ", "ㅏ", "ㅣ", ";", "'", "\\", "-rowspan-" },
	{ "ㅁ", "ㄴ", "ㅇ", "ㄹ", "ㅎ", "ㅗ", "ㅓ", "ㅏ", "ㅣ", ":", "\"", "|", "-rowspan-" },
	{ "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'", "\\", "-rowspan-" },
	{ "A", "S", "D", "F", "G", "H", "J", "K", "L", ":", "\"", "|", "-rowspan-" },

	{ "ㅋ", "ㅌ", "ㅊ", "ㅍ", "ㅠ", "ㅜ", "ㅡ", ",", ".", "/", "`", "ALT", "-colspan-" },
	{ "ㅋ", "ㅌ", "ㅊ", "ㅍ", "ㅠ", "ㅜ", "ㅡ", "<", ">", "?", "~", "ALT", "-colspan-" },
	{ "z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "`", "ALT", "-colspan-" },
	{ "Z", "X", "C", "V", "B", "N", "M", "<", ">", "?", "~", "ALT", "-colspan-" },

	{ "SHIFT", "-colspan-", "SPACE", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "RESET", "-colspan-", "CANCEL", "-colspan-" },
	{ "SHIFT", "-colspan-", "SPACE", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "RESET", "-colspan-", "CANCEL", "-colspan-" },
	{ "SHIFT", "-colspan-", "SPACE", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "RESET", "-colspan-", "CANCEL", "-colspan-" },
	{ "SHIFT", "-colspan-", "SPACE", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "-colspan-", "RESET", "-colspan-", "CANCEL", "-colspan-" }
};

GuiTextEditPopupKeyboard::GuiTextEditPopupKeyboard(Window* window, const std::string& title, const std::string& initValue,
	const std::function<void(const std::string&)>& okCallback, bool multiLine, const std::string acceptBtnText)
	: GuiComponent(window), mBackground(window, ":/frame.png"), mGrid(window, Vector2i(1, 6)), mMultiLine(multiLine)
{
	setTag("popup");

	mOkCallback = okCallback;

	auto theme = ThemeData::getMenuTheme();
	mBackground.setImagePath(theme->Background.path);
	mBackground.setEdgeColor(theme->Background.color);
	mBackground.setCenterColor(theme->Background.centerColor);
	mBackground.setCornerSize(theme->Background.cornerSize);
	mBackground.setPostProcessShader(theme->Background.menuShader);

	addChild(&mBackground);
	addChild(&mGrid);

	mTitle = std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(title), theme->Title.font, theme->Title.color, ALIGN_CENTER);
	
	mKeyboardGrid = std::make_shared<ComponentGrid>(mWindow, Vector2i(kbUs[0].size(), kbUs.size() / BUTTON_LAYER_SIZE));

	mText = std::make_shared<TextEditComponent>(mWindow);
	mText->setValue(initValue);

	if (!multiLine)
		mText->setCursor(initValue.size());

	// Header
	mGrid.setEntry(mTitle, Vector2i(0, 0), false, true);

	// Text edit add
	mGrid.setEntry(mText, Vector2i(0, 1), true, false, Vector2i(1, 1), GridFlags::BORDER_TOP);

	std::vector< std::vector< std::shared_ptr<ButtonComponent> > > buttonList;

	// Keyboard
	// Case for if multiline is enabled, then don't create the keyboard.
	// if (!mMultiLine) 
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
		else if (language == "ko")
			layout = &kbKr;

		for (unsigned int i = 0; i < layout->size() / BUTTON_LAYER_SIZE; i++)
		{			
			std::vector<std::shared_ptr<ButtonComponent>> buttons;
			for (unsigned int j = 0; j < (*layout)[i].size(); j++)
			{
				std::string lower = (*layout)[BUTTON_LAYER_SIZE * i][j];
				if (lower.empty() || lower == "-rowspan-" || lower == "-colspan-")
					continue;

				std::string upper = (*layout)[BUTTON_LAYER_SIZE * i + 1][j];
				std::string lowerAlted = (*layout)[BUTTON_LAYER_SIZE * i + 2][j];
				std::string upperAlted = (*layout)[BUTTON_LAYER_SIZE * i + 3][j];

				std::shared_ptr<ButtonComponent> button = nullptr;

				if (lower == "DEL")
				{
					lower = _U("\uF177");
					upper = _U("\uF177");
					lowerAlted = _U("\uF177");
					upperAlted = _U("\uF177");
				}
				else if (lower == "OK")
				{
					lower = _U("\uF058");

					if (mMultiLine)
					{
						upper = _U("\uF149");
						lowerAlted = _U("\uF149");
						upperAlted = _U("\uF149");
					}
					else
					{
						upper = _U("\uF058");
						lowerAlted = _U("\uF058");
						upperAlted = _U("\uF058");
					}
				}
				else if (lower == "SPACE")
				{
					lower = " ";
					upper = " ";
				}
				else if (lower != "SHIFT" && lower.length() > 1)
				{
					lower = _(lower.c_str());
					upper = _(upper.c_str());
					lowerAlted = _(lowerAlted.c_str());
					upperAlted = _(upperAlted.c_str());
				}

				if (lower == "SHIFT")
				{
					// Special case for shift key
					mShiftButton = std::make_shared<ButtonComponent>(mWindow, _U("\uF176"), _("SHIFTS FOR UPPER,LOWER, AND SPECIAL"), [this] { shiftKeys(); }, false);					
					button = mShiftButton;
				}
				else if (lower == "ALT")
				{
					mAltButton = std::make_shared<ButtonComponent>(mWindow, _U("\uF141"), _("ALT GR"), [this] { altKeys(); }, false);
					button = mAltButton;
				}
				else
					button = makeButton(lower, upper, lowerAlted, upperAlted);

				button->setPadding(Vector4f(BUTTON_GRID_HORIZ_PADDING / 4.0f, BUTTON_GRID_HORIZ_PADDING / 4.0f, BUTTON_GRID_HORIZ_PADDING / 4.0f, BUTTON_GRID_HORIZ_PADDING / 4.0f));
				button->setRenderNonFocusedBackground(false);
				buttons.push_back(button);

				int colSpan = 1;
				for (unsigned int cs = j + 1; cs < (*layout)[i].size(); cs++)
				{
					if (std::string((*layout)[BUTTON_LAYER_SIZE * i][cs]) == "-colspan-")
						colSpan++;
					else
						break;
				}
				
				int rowSpan = 1;
				for (unsigned int cs = (BUTTON_LAYER_SIZE * i) + BUTTON_LAYER_SIZE; cs < layout->size(); cs += BUTTON_LAYER_SIZE)
				{
					if (std::string((*layout)[cs][j]) == "-rowspan-")
						rowSpan++;
					else
						break;
				}

				mKeyboardGrid->setEntry(button, Vector2i(j, i), true, true, Vector2i(colSpan, rowSpan));

				buttonList.push_back(buttons);
			}
		}		
		// END KEYBOARD IF
	}

	// Add keyboard keys
	mGrid.setEntry(mKeyboardGrid, Vector2i(0, 2), true, true, Vector2i(2, 4));

	// Determine size from text size
	float textHeight = mText->getFont()->getHeight();
	if (multiLine)
		textHeight *= 4;

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
			mGrid.setCursorTo(mKeyboardGrid);
			return true;
		}
		else if (config->isMappedLike("left", input))
		{		
			if (mGrid.getSelectedComponent() == mKeyboardGrid)
			{
				Vector2i curCursor = mKeyboardGrid->getCursor();
				mKeyboardGrid->setCursorTo(Vector2i(kbUs[0].size() - 1, curCursor.y()));
				return true;
			}
		}
		else if (config->isMappedLike("right", input))
		{
			if (mGrid.getSelectedComponent() == mKeyboardGrid)
			{
				Vector2i curCursor = mKeyboardGrid->getCursor();
				mKeyboardGrid->setCursorTo(Vector2i(0, curCursor.y()));
				return true;
			}
		}

		return false;
	});

	
	// If multiline, set all diminsions back to default, else draw size for keyboard.
	if (mMultiLine) 
	{
		if (Renderer::ScreenSettings::fullScreenMenus())
			setSize(OSK_WIDTH, Renderer::getScreenHeight());
		else
			setSize(OSK_WIDTH, OSK_HEIGHT - mText->getFont()->getHeight() + textHeight);

		setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);
	}
	else
	{
		//setSize(OSK_WIDTH, mTitle->getFont()->getHeight() + textHeight + 40 + (Renderer::getScreenHeight() * 0.085f) * 6);
		setSize(OSK_WIDTH, OSK_HEIGHT);
		setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);
		animateTo(Vector2f((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2));
	}
}

void GuiTextEditPopupKeyboard::onSizeChanged()
{
	GuiComponent::onSizeChanged();

	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));

	mText->setSize(mSize.x() - (2 * OSK_PADDINGX), mText->getSize().y());

	// update grid
	mGrid.setRowHeight(0, mTitle->getFont()->getHeight() + OSK_PADDINGY);
	mGrid.setRowHeight(1, mText->getSize().y() + 2 * OSK_PADDINGY);
	mGrid.setRowHeight(2, mKeyboardGrid->getSize().y());
	mGrid.setSize(mSize);

	auto pos = mKeyboardGrid->getPosition();
	auto sz = mKeyboardGrid->getSize();

	mKeyboardGrid->setSize(mSize.x() - OSK_PADDINGX - OSK_PADDINGX, sz.y() - OSK_PADDINGY); // Small margin between buttons
	mKeyboardGrid->setPosition(OSK_PADDINGX, pos.y());	
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

void GuiTextEditPopupKeyboard::toggleKeyState(bool& state, std::shared_ptr<ButtonComponent>& button)
{
	state = !state;

	if (state)
	{
		button->setRenderNonFocusedBackground(true);
		button->setColorShift(0xFF0000FF);
	}
	else
	{
		button->setRenderNonFocusedBackground(false);
		button->removeColorShift();
	}

	updateKeyboardButtons();
}

void GuiTextEditPopupKeyboard::updateKeyboardButtons()
{
	for (auto& kb : keyboardButtons)
	{
		const std::string& text = (mAlt && mShift) ? kb.altedShiftedKey
			: (mAlt) ? kb.altedKey
			: (mShift) ? kb.shiftedKey
			: kb.key;

		auto sz = kb.button->getSize();
		kb.button->setText(text, text, false);
		kb.button->setSize(sz);
	}
}

// Shifts the keys when user hits the shift button.
void GuiTextEditPopupKeyboard::shiftKeys() 
{
	toggleKeyState(mShift, mShiftButton);
}

void GuiTextEditPopupKeyboard::altKeys()
{
	if (mShift)
		toggleKeyState(mShift, mShiftButton);

	toggleKeyState(mAlt, mAltButton);
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

std::shared_ptr<ButtonComponent> GuiTextEditPopupKeyboard::makeButton(const std::string& key, const std::string& shiftedKey, const std::string& altedKey, const std::string& altedShiftedKey)
{
	std::shared_ptr<ButtonComponent> button = std::make_shared<ButtonComponent>(mWindow, key, key, [this, key, shiftedKey, altedKey, altedShiftedKey]
	{						
		if (key == _U("\uF058") || key.find("OK") != std::string::npos)
		{	
			if (mMultiLine && (mShift || mAlt))
			{
				mText->startEditing(); mText->textInput("\r\n"); mText->stopEditing();
			}
			else
			{
				mOkCallback(mText->getValue());
				delete this;
			}
			return;
		}
		else if (key == _U("\uF177") || key == "DEL")
		{
			mText->startEditing(); mText->textInput("\b"); mText->stopEditing();
			return;
		}
		else if (key == _("SPACE") || key == " ")
		{
			mText->startEditing(); mText->textInput(" "); mText->stopEditing();
			return;
		}
		else if (key == _("RESET"))
		{
			mOkCallback(""); 
			delete this;
			return;
		}
		else if (key == _("CANCEL"))
		{
			delete this;
			return;
		}

		if (mAlt)
		{
			if (mShift && altedShiftedKey.empty())
				return;
			if (altedKey.empty())
				return;
		}

		mText->startEditing();

		const char* text;
		if (mAlt && mShift)
			text = altedShiftedKey.c_str();
		else if (mAlt)
			text = altedKey.c_str();
		else if (mShift)
			text = shiftedKey.c_str();
		else
			text = key.c_str();
		mText->textInput(text);

		mText->stopEditing();

		if (Utils::String::isKorean(text) && mShift)
			shiftKeys();
	}, false);
	
	KeyboardButton kb(button, key, shiftedKey, altedKey, altedShiftedKey);
	keyboardButtons.push_back(kb);
	return button;
}