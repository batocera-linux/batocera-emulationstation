#include "MultiLineMenuEntry.h"
#include "Window.h"
#include "components/TextComponent.h"
#include "components/ComponentGrid.h"
#include "math/Vector2i.h"
#include "math/Vector2f.h"
#include "ThemeData.h"
#include "utils/HtmlColor.h"

#define SUBSTRING_OPACITY	192

MultiLineMenuEntry::MultiLineMenuEntry(Window* window, const std::string& text, const std::string& substring, bool multiLine) :
	ComponentGrid(window, Vector2i(1, 2))
{
	mMultiLine = multiLine;
	mSizeChanging = false;

	auto theme = ThemeData::getMenuTheme();

	mText = std::make_shared<TextComponent>(mWindow, text.c_str(), theme->Text.font, theme->Text.color);
	mText->setMultiLine(TextComponent::MultiLineType::SINGLELINE);
	mText->setVerticalAlignment(ALIGN_TOP);

	mSubstring = std::make_shared<TextComponent>(mWindow, substring.c_str(), theme->TextSmall.font, Utils::HtmlColor::applyColorOpacity(theme->Text.color, SUBSTRING_OPACITY));
	mSubstring->setVerticalAlignment(ALIGN_TOP);

	if (!multiLine)
		mSubstring->setMultiLine(TextComponent::MultiLineType::SINGLELINE);

	setEntry(mText, Vector2i(0, 0), true, true);
	setEntry(mSubstring, Vector2i(0, 1), false, true);

	float th = mText->getSize().y();

	if (mSubstring->getText().empty())
	{
		setRowHeight(0, th);
		setRowHeight(1, 0);

		setSize(Vector2f(0, th));
	}
	else
	{
		float sh = mSubstring->getSize().y();
		float h = th + sh;

		setRowHeightPerc(0, (th * 0.9) / h);
		setRowHeightPerc(1, (sh * 1.1) / h);

		setSize(Vector2f(0, h));
	}
}

void MultiLineMenuEntry::setColor(unsigned int color)
{
	mText->setColor(color);
	mSubstring->setColor(Utils::HtmlColor::applyColorOpacity(color, SUBSTRING_OPACITY));
}

std::string MultiLineMenuEntry::getDescription()
{
	return mSubstring->getText();
}

void MultiLineMenuEntry::setDescription(const std::string& description)
{
	mSubstring->setText(description);
	onSizeChanged();
}

void MultiLineMenuEntry::onSizeChanged()
{		
	ComponentGrid::onSizeChanged();

	if (mMultiLine && mSubstring && mSize.x() > 0 && !mSizeChanging)
	{
		mSizeChanging = true;

		mText->setSize(mSize.x(), 0);
		mSubstring->setSize(mSize.x(), 0);

		float th = mText->getSize().y();

		if (mSubstring->getText().empty())
		{
			setRowHeight(0, th);
			setRowHeight(1, 0);

			setSize(Vector2f(mSize.x(), th));
		}
		else
		{
			float sh = mSubstring->getSize().y();
			float h = th + sh;

			setRowHeightPerc(0, (th * 0.9) / h);
			setRowHeightPerc(1, (sh * 1.1) / h);

			setSize(Vector2f(mSize.x(), h));
		}

		mSizeChanging = false;
	}
}

void MultiLineMenuEntry::onFocusGained()
{
	ComponentGrid::onFocusGained();

	if (!mMultiLine && mSubstring)
		mSubstring->setAutoScroll(TextComponent::AutoScrollType::HORIZONTAL);

}

void MultiLineMenuEntry::onFocusLost()
{
	ComponentGrid::onFocusLost();

	if (!mMultiLine && mSubstring)
		mSubstring->setAutoScroll(TextComponent::AutoScrollType::NONE);
}
