#include "MultiLineMenuEntry.h"
#include "Window.h"
#include "components/TextComponent.h"
#include "components/ComponentGrid.h"
#include "math/Vector2i.h"
#include "math/Vector2f.h"
#include "ThemeData.h"

MultiLineMenuEntry::MultiLineMenuEntry(Window* window, const std::string& text, const std::string& substring, bool multiLine) :
	ComponentGrid(window, Vector2i(1, 2))
{
	mMultiLine = multiLine;
	mSizeChanging = false;

	auto theme = ThemeData::getMenuTheme();

	mText = std::make_shared<TextComponent>(mWindow, text.c_str(), theme->Text.font, theme->Text.color);
	mText->setVerticalAlignment(ALIGN_TOP);

	mSubstring = std::make_shared<TextComponent>(mWindow, substring.c_str(), theme->TextSmall.font, theme->Text.color);		
	mSubstring->setVerticalAlignment(ALIGN_TOP);
	mSubstring->setOpacity(192);

	setEntry(mText, Vector2i(0, 0), true, true);
	setEntry(mSubstring, Vector2i(0, 1), false, true);

	float th = mText->getSize().y();
	float sh = mSubstring->getSize().y();
	float h = th + sh;

	setRowHeightPerc(0, (th * 0.9) / h);
	setRowHeightPerc(1, (sh * 1.1) / h);

	setSize(Vector2f(0, h));
}

void MultiLineMenuEntry::setColor(unsigned int color)
{
	mText->setColor(color);
	mSubstring->setColor(color);
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
		float sh = mSubstring->getSize().y();
		float h = th + sh;

		setRowHeightPerc(0, (th * 0.9) / h);
		setRowHeightPerc(1, (sh * 1.1) / h);

		setSize(Vector2f(mSize.x(), h));

		mSizeChanging = false;
	}
}
