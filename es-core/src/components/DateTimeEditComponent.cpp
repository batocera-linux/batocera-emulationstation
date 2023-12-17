#include "components/DateTimeEditComponent.h"

#include "resources/Font.h"
#include "utils/StringUtil.h"

DateTimeEditComponent::DateTimeEditComponent(Window* window, DisplayMode dispMode) : GuiComponent(window),
	mEditing(false), mEditIndex(0), mRelativeUpdateAccumulator(0), mUppercase(false), mAutoSize(true)
{
	auto menuTheme = ThemeData::getMenuTheme();

	mFont = menuTheme->Text.font;
	mColor = menuTheme->Text.color;

	mDayIndex = 0;
	mMonthIndex = 1;
	mYearIndex = 2;
	setDisplayMode(dispMode);

	updateTextCache();
	mAutoSize = true;
}

void DateTimeEditComponent::setDisplayMode(DisplayMode mode)
{
	mDisplayMode = mode;

	switch (mode)
	{
	case DISP_DATE:
		mDateFormat = Utils::Time::getSystemDateFormat();
		break;
	case DISP_DATE_TIME:
		mDateFormat = Utils::Time::getSystemDateFormat() + " %H:%M:%S";
		break;
	case DISP_RELATIVE_TO_NOW:
		mDateFormat = "";
		break;
	}

	mDayIndex = 0;
	mMonthIndex = 1;
	mYearIndex = 2;

	if (!mDateFormat.empty())
	{
		int index = 0;

		const char* pc = mDateFormat.c_str();
		while (*pc)
		{
			if (*pc == '%')
			{
				pc++;

				switch (*pc)
				{
				case 'y':
				case 'Y':
					mYearIndex = index;
					index++;
					break;
				case 'm':
					mMonthIndex = index;
					index++;
					break;
				case 'd':
					mDayIndex = index;
					index++;
					break;
				}
			}

			pc++;
		}
	}

	updateTextCache();
}

bool DateTimeEditComponent::input(InputConfig* config, Input input)
{
	if (input.value == 0)
		return false;

	if (config->isMappedTo(BUTTON_OK, input))
	{
		if (mDisplayMode != DISP_RELATIVE_TO_NOW) //don't allow editing for relative times
			mEditing = !mEditing;

		if (mEditing)
		{
			//started editing
			mTimeBeforeEdit = mTime;

			//initialize to now if unset
			if (mTime.getTime() == Utils::Time::NOT_A_DATE_TIME)
			{
				mTime = Utils::Time::now();
				updateTextCache();
			}
		}

		return true;
	}

	if (mEditing)
	{
		if (config->isMappedTo(BUTTON_BACK, input))
		{
			mEditing = false;
			mTime = mTimeBeforeEdit;
			updateTextCache();
			return true;
		}

		int incDir = 0;
		if (config->isMappedLike("up", input) || config->isMappedTo("pageup", input))
			incDir = 1;
		else if (config->isMappedLike("down", input) || config->isMappedTo("pagedown", input))
			incDir = -1;

		if (incDir != 0)
		{
			tm new_tm = mTime;

			if (mEditIndex == mMonthIndex)
			{
				new_tm.tm_mon += incDir;

				if (new_tm.tm_mon > 11)
					new_tm.tm_mon = 0;
				else if (new_tm.tm_mon < 0)
					new_tm.tm_mon = 11;

			}
			else if (mEditIndex == mDayIndex)
			{
				const int days_in_month = Utils::Time::daysInMonth(new_tm.tm_year + 1900, new_tm.tm_mon + 1);
				new_tm.tm_mday += incDir;

				if (new_tm.tm_mday > days_in_month)
					new_tm.tm_mday = 1;
				else if (new_tm.tm_mday < 1)
					new_tm.tm_mday = days_in_month;

			}
			else if (mEditIndex == mYearIndex)
			{
				new_tm.tm_year += incDir;

				if (new_tm.tm_year < 0)
					new_tm.tm_year = 0;
			}

			//validate day
			const int days_in_month = Utils::Time::daysInMonth(new_tm.tm_year + 1900, new_tm.tm_mon + 1);
			if (new_tm.tm_mday > days_in_month)
				new_tm.tm_mday = days_in_month;

			mTime = new_tm;

			updateTextCache();
			return true;
		}

		if (config->isMappedLike("right", input))
		{
			mEditIndex++;
			if (mEditIndex >= (int)mCursorBoxes.size())
				mEditIndex--;

			return true;
		}

		if (config->isMappedLike("left", input))
		{
			mEditIndex--;
			if (mEditIndex < 0)
				mEditIndex++;

			return true;
		}
	}

	return GuiComponent::input(config, input);
}

void DateTimeEditComponent::update(int deltaTime)
{
	if (mDisplayMode == DISP_RELATIVE_TO_NOW)
	{
		mRelativeUpdateAccumulator += deltaTime;
		if (mRelativeUpdateAccumulator > 1000)
		{
			mRelativeUpdateAccumulator = 0;
			updateTextCache();
		}
	}

	GuiComponent::update(deltaTime);
}

void DateTimeEditComponent::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();

	auto rect = Renderer::getScreenRect(trans, mSize);
	if (!Renderer::isVisibleOnScreen(rect))
		return;

	if (mTextCache)
	{
		// vertically center
		Vector3f off(0, (mSize.y() - mTextCache->metrics.size.y()) / 2, 0);
		trans.translate(off);

		Renderer::setMatrix(trans);

		std::shared_ptr<Font> font = getFont();

		mTextCache->setColor((mColor & 0xFFFFFF00) | getOpacity());
		font->renderTextCache(mTextCache.get());

		if (mEditing)
		{
			if (mEditIndex >= 0 && (unsigned int)mEditIndex < mCursorBoxes.size())
			{
				Renderer::drawRect(mCursorBoxes[mEditIndex][0], mCursorBoxes[mEditIndex][1],
					mCursorBoxes[mEditIndex][2], mCursorBoxes[mEditIndex][3], 0x00000022, 0x00000022);
			}
		}
	}
}

void DateTimeEditComponent::setValue(const std::string& val)
{
	mTime = val;
	updateTextCache();
}

std::string DateTimeEditComponent::getValue() const
{
	return mTime;
}

DateTimeEditComponent::DisplayMode DateTimeEditComponent::getCurrentDisplayMode() const
{
	return mDisplayMode;
}

std::string DateTimeEditComponent::getDisplayString() const
{
	if (mDisplayMode == DISP_RELATIVE_TO_NOW)
		return Utils::Time::getElapsedSinceString(mTime.getTime());

	if (mTime.getTime() == 0)
		return "";

	return Utils::Time::timeToString(mTime, mDateFormat);
}

std::shared_ptr<Font> DateTimeEditComponent::getFont() const
{
	return mFont ? mFont : Font::get(FONT_SIZE_MEDIUM);
}

void DateTimeEditComponent::updateTextCache()
{
	const std::string dispString = mUppercase ? Utils::String::toUpper(getDisplayString()) : getDisplayString();

	std::shared_ptr<Font> font = getFont();
	mTextCache = std::unique_ptr<TextCache>(font->buildTextCache(dispString, 0, 0, mColor));

	if (mAutoSize)
	{
		mSize = mTextCache->metrics.size;

		if (getParent())
			getParent()->onSizeChanged();
	}

	// Set up cursor positions
	mCursorBoxes.clear();

	if (dispString.empty() || mDisplayMode == DISP_RELATIVE_TO_NOW)
		return;

	int lastSep = 0;
	int i = 0;
	while (true)
	{
		char c = dispString[i];
		if (c == '/' || c == '-' || c == '.' || c == 0)
		{
			auto before = lastSep == 0 ? "" : dispString.substr(0, lastSep);
			auto offset = font->sizeText(before);

			auto sub = dispString.substr(lastSep, i - lastSep);
			auto size = font->sizeText(sub);

			mCursorBoxes.push_back(Vector4f(offset[0], offset[1] - size[1], size[0], size[1]));

			lastSep = i + 1;
		}

		if (c == 0)
			break;

		i++;
	}
}

void DateTimeEditComponent::setColor(unsigned int color)
{
	mColor = color;
	if (mTextCache)
		mTextCache->setColor(color);
}

void DateTimeEditComponent::setFont(std::shared_ptr<Font> font)
{
	mFont = font;
	updateTextCache();
}

void DateTimeEditComponent::onSizeChanged()
{
	GuiComponent::onSizeChanged();

	mAutoSize = false;
	updateTextCache();
}

void DateTimeEditComponent::setUppercase(bool uppercase)
{
	mUppercase = uppercase;
	updateTextCache();
}

void DateTimeEditComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "datetime");
	if (!elem)
		return;

	// We set mAutoSize BEFORE calling GuiComponent::applyTheme because it calls
	// setSize(), which will call updateTextCache(), which will reset mSize if 
	// mAutoSize == true, ignoring the theme's value.
	if (properties & ThemeFlags::SIZE)
		mAutoSize = !elem->has("size");

	GuiComponent::applyTheme(theme, view, element, properties);

	using namespace ThemeFlags;

	if (properties & COLOR && elem->has("color"))
		setColor(elem->get<unsigned int>("color"));

	if (properties & FORCE_UPPERCASE && elem->has("forceUppercase"))
		setUppercase(elem->get<bool>("forceUppercase"));

	setFont(Font::getFromTheme(elem, properties, mFont));
}
