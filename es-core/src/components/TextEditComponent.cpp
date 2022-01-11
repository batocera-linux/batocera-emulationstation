#include "components/TextEditComponent.h"

#include "resources/Font.h"
#include "utils/StringUtil.h"
#include "LocaleES.h"
#include "Window.h"

#define TEXT_PADDING_HORIZ 10
#define TEXT_PADDING_VERT 2

#define CURSOR_REPEAT_START_DELAY 500
#define CURSOR_REPEAT_SPEED 28 // lower is faster

#define BLINKTIME	1000

TextEditComponent::TextEditComponent(Window* window) : GuiComponent(window),
	mBox(window, ":/textinput_ninepatch.png"), mFocused(false), 
	mScrollOffset(0.0f, 0.0f), mCursor(0), mEditing(false), mFont(Font::get(FONT_SIZE_MEDIUM, FONT_PATH_LIGHT)), 
	mCursorRepeatDir(0)
{
	mBlinkTime = 0;
	mDeferTextInputStart = false;

	auto theme = ThemeData::getMenuTheme();
	mBox.setImagePath(ThemeData::getMenuTheme()->Icons.textinput_ninepatch);

	addChild(&mBox);
	
	onFocusLost();

	setSize(4096, mFont->getHeight() + TEXT_PADDING_VERT);
}

void TextEditComponent::onFocusGained()
{
	mFocused = true;
	mBox.setImagePath(ThemeData::getMenuTheme()->Icons.textinput_ninepatch_active);	

	mWindow->postToUiThread([this]() { startEditing(); });
}

void TextEditComponent::onFocusLost()
{
	mFocused = false;
	mBox.setImagePath(ThemeData::getMenuTheme()->Icons.textinput_ninepatch);
	
	mWindow->postToUiThread([this]() { stopEditing(); });
}

void TextEditComponent::onSizeChanged()
{
	GuiComponent::onSizeChanged();
	mBox.fitTo(mSize, Vector3f::Zero(), Vector2f(-34, -32 - TEXT_PADDING_VERT));
	onTextChanged(); // wrap point probably changed
}

void TextEditComponent::setValue(const std::string& val)
{
	mText = val;
	onTextChanged();
}

std::string TextEditComponent::getValue() const
{
	return mText;
}

void TextEditComponent::textInput(const char* text)
{
	if(mEditing)
	{
		mCursorRepeatDir = 0;
		if(text[0] == '\b')
		{
			if(mCursor > 0)
			{
				size_t newCursor = Utils::String::prevCursor(mText, mCursor);
				mText.erase(mText.begin() + newCursor, mText.begin() + mCursor);
				mCursor = (unsigned int)newCursor;
			}
		}else{
			mText.insert(mCursor, text);
			mCursor += (unsigned int)strlen(text);
		}
	}

	onTextChanged();
	onCursorChanged();
}

bool TextEditComponent::hasAnyKeyPressed()
{
	bool anyKeyPressed = false;

	int numKeys;
	const Uint8* keys = SDL_GetKeyboardState(&numKeys);
	for (int i = 0; i < numKeys && !anyKeyPressed; i++)
		anyKeyPressed |= keys[i];

	return anyKeyPressed;
}

void TextEditComponent::startEditing()
{
	if (mEditing)
		return;
	
	if (hasAnyKeyPressed())
	{
		// Defer if a key is pressed to avoid repeat behaviour if a TextEditComponent is opened with a keypress
		mDeferTextInputStart = true;
	}
	else
	{
		mDeferTextInputStart = false;
		SDL_StartTextInput();
	}

	mEditing = true;
	updateHelpPrompts();
}

void TextEditComponent::stopEditing()
{
	if (!mEditing)
		return;

	SDL_StopTextInput();
	mEditing = false;
	mDeferTextInputStart = false;
	updateHelpPrompts();
}

bool TextEditComponent::input(InputConfig* config, Input input)
{	
	bool const cursor_left = (config->getDeviceId() != DEVICE_KEYBOARD && config->isMappedLike("left", input)) ||
		(config->getDeviceId() == DEVICE_KEYBOARD && input.id == SDLK_LEFT);
	bool const cursor_right = (config->getDeviceId() != DEVICE_KEYBOARD && config->isMappedLike("right", input)) ||
		(config->getDeviceId() == DEVICE_KEYBOARD && input.id == SDLK_RIGHT);

	if(input.value == 0)
	{
		if(cursor_left || cursor_right)
			mCursorRepeatDir = 0;

		return false;
	}

	bool const cursor_up = (config->getDeviceId() != DEVICE_KEYBOARD && config->isMappedLike("up", input)) ||
		(config->getDeviceId() == DEVICE_KEYBOARD && input.id == SDLK_UP);
	bool const cursor_down = (config->getDeviceId() != DEVICE_KEYBOARD && config->isMappedLike("down", input)) ||
		(config->getDeviceId() == DEVICE_KEYBOARD && input.id == SDLK_DOWN);

	if (cursor_up || cursor_down)
	{
		if (mEditing)
			stopEditing();

		return false;
	}

	if((config->isMappedTo(BUTTON_OK, input) || (config->getDeviceId() == DEVICE_KEYBOARD && input.id == SDLK_RETURN)) && mFocused && !mEditing)
	{
		startEditing();
		return true;
	}

	if(mEditing)
	{
		if(config->getDeviceId() == DEVICE_KEYBOARD && input.id == SDLK_RETURN)
		{
			if(isMultiline())
			{
				textInput("\n");
			}
			else
			{
				stopEditing();
				return false;
			}

			return true;
		}

		if((config->getDeviceId() == DEVICE_KEYBOARD && input.id == SDLK_ESCAPE)) // || (config->getDeviceId() != DEVICE_KEYBOARD && config->isMappedTo(BUTTON_BACK, input)))
		{
			stopEditing();
			return false;
		}

		if(cursor_left || cursor_right)
		{
			mBlinkTime = 0;
			mCursorRepeatDir = cursor_left ? -1 : 1;
			mCursorRepeatTimer = -(CURSOR_REPEAT_START_DELAY - CURSOR_REPEAT_SPEED);
			moveCursor(mCursorRepeatDir);
			return true;
		} 
		else if(config->getDeviceId() == DEVICE_KEYBOARD)
		{
			switch(input.id)
			{
				case SDLK_HOME:
					setCursor(0);
					return true;

				case SDLK_END:
					setCursor(std::string::npos);
					return true;

				case SDLK_DELETE:
					if(mCursor < mText.length())
					{
						// Fake as Backspace one char to the right
						moveCursor(1);
						textInput("\b");
					}
					return true;
			}

			// All input on keyboard
			return true;
		}
	}

	return false;
}

void TextEditComponent::update(int deltaTime)
{
	if (mEditing && mDeferTextInputStart)
	{
		if (!hasAnyKeyPressed())
		{
			SDL_StartTextInput();
			mDeferTextInputStart = false;
		}
	}

	mBlinkTime += deltaTime;
	if (mBlinkTime >= BLINKTIME)
		mBlinkTime = 0;

	updateCursorRepeat(deltaTime);
	GuiComponent::update(deltaTime);
}

void TextEditComponent::updateCursorRepeat(int deltaTime)
{
	if(mCursorRepeatDir == 0)
		return;

	mCursorRepeatTimer += deltaTime;
	while(mCursorRepeatTimer >= CURSOR_REPEAT_SPEED)
	{
		moveCursor(mCursorRepeatDir);
		mCursorRepeatTimer -= CURSOR_REPEAT_SPEED;
	}
}

void TextEditComponent::moveCursor(int amt)
{
	mCursor = (unsigned int)Utils::String::moveCursor(mText, mCursor, amt);
	onCursorChanged();
}

void TextEditComponent::setCursor(size_t pos)
{
	if(pos == std::string::npos)
		mCursor = (unsigned int)mText.length();
	else
		mCursor = (int)pos;

	moveCursor(0);
}

void TextEditComponent::onTextChanged()
{
	std::string wrappedText = (isMultiline() ? mFont->wrapText(mText, getTextAreaSize().x()) : mText);
	mTextCache = std::unique_ptr<TextCache>(mFont->buildTextCache(wrappedText, 0, 0, (ThemeData::getMenuTheme()->Text.color & 0xFFFFFF00) | getOpacity()));
	
	if(mCursor > (int)mText.length())
		mCursor = (unsigned int)mText.length();
}

void TextEditComponent::onCursorChanged()
{
	if(isMultiline())
	{
		Vector2f textSize = mFont->getWrappedTextCursorOffset(mText, getTextAreaSize().x(), mCursor); 

		if(mScrollOffset.y() + getTextAreaSize().y() < textSize.y() + mFont->getHeight()) //need to scroll down?
		{
			mScrollOffset[1] = textSize.y() - getTextAreaSize().y() + mFont->getHeight();
		}else if(mScrollOffset.y() > textSize.y()) //need to scroll up?
		{
			mScrollOffset[1] = textSize.y();
		}
	}else{
		Vector2f cursorPos = mFont->sizeText(mText.substr(0, mCursor));

		if(mScrollOffset.x() + getTextAreaSize().x() < cursorPos.x())
		{
			mScrollOffset[0] = cursorPos.x() - getTextAreaSize().x();
		}else if(mScrollOffset.x() > cursorPos.x())
		{
			mScrollOffset[0] = cursorPos.x();
		}
	}
}

void TextEditComponent::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = getTransform() * parentTrans;

	if (!Renderer::isVisibleOnScreen(trans.translation().x(), trans.translation().y(), mSize.x(), mSize.y()))
		return;

	renderChildren(trans);

	// text + cursor rendering
	// offset into our "text area" (padding)
	trans.translation() += Vector3f(getTextAreaPos().x(), getTextAreaPos().y(), 0);

	Vector2i clipPos((int)trans.translation().x(), (int)trans.translation().y());
	Vector3f dimScaled = trans * Vector3f(getTextAreaSize().x(), getTextAreaSize().y(), 0); // use "text area" size for clipping
	Vector2i clipDim((int)(dimScaled.x() - trans.translation().x()), (int)(dimScaled.y() - trans.translation().y()));
	Renderer::pushClipRect(clipPos, clipDim);

	trans.translate(Vector3f(-mScrollOffset.x(), -mScrollOffset.y(), 0));
	Renderer::setMatrix(trans);

	if(mTextCache)
	{
		mFont->renderTextCache(mTextCache.get());
	}

	// pop the clip early to allow the cursor to be drawn outside of the "text area"
	Renderer::popClipRect();

	// draw cursor
	//if(mEditing)
	{
		Vector2f cursorPos;
		if(isMultiline())
		{
			cursorPos = mFont->getWrappedTextCursorOffset(mText, getTextAreaSize().x(), mCursor);
		}
		else
		{
			cursorPos = mFont->sizeText(mText.substr(0, mCursor));
			cursorPos[1] = 0;
		}

		if (!mEditing || mBlinkTime < BLINKTIME / 2)
		{
			float cursorHeight = mFont->getHeight() * 0.8f;

			auto cursorColor = (ThemeData::getMenuTheme()->Text.color & 0xFFFFFF00) | getOpacity();
			if (!mEditing)
				cursorColor = (ThemeData::getMenuTheme()->Text.color & 0xFFFFFF00) | (unsigned char) (getOpacity() * 0.25f);

			Renderer::drawRect(cursorPos.x(), cursorPos.y() + (mFont->getHeight() - cursorHeight) / 2, 2.0f, cursorHeight, cursorColor, cursorColor); // 0x000000FF
		}
	}
}

bool TextEditComponent::isMultiline()
{
	return (getSize().y() > mFont->getHeight() * 1.25f);
}

Vector2f TextEditComponent::getTextAreaPos() const
{
	return Vector2f(TEXT_PADDING_HORIZ / 2.0f, TEXT_PADDING_VERT / 2.0f);
}

Vector2f TextEditComponent::getTextAreaSize() const
{
	return Vector2f(mSize.x() - TEXT_PADDING_HORIZ, mSize.y() - TEXT_PADDING_VERT);
}

std::vector<HelpPrompt> TextEditComponent::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	if(mEditing)
	{
		prompts.push_back(HelpPrompt("up/down/left/right", _("MOVE CURSOR"))); // batocera
		//prompts.push_back(HelpPrompt(BUTTON_BACK, _("STOP EDITING")));
	}else{
		prompts.push_back(HelpPrompt(BUTTON_OK, _("EDIT")));
	}
	return prompts;
}
