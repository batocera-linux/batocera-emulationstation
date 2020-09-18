
#pragma once

#include "Window.h"
#include "components/TextComponent.h"
#include "components/ComponentGrid.h"
#include "math/Vector2i.h"
#include "math/Vector2f.h"
#include "ThemeData.h"

class MultiLineMenuEntry : public ComponentGrid
{
public:
	MultiLineMenuEntry(Window* window, const std::string& text, const std::string& substring) :
		ComponentGrid(window, Vector2i(1, 2))
	{
		auto theme = ThemeData::getMenuTheme();

		mText = std::make_shared<TextComponent>(mWindow, text.c_str(), theme->Text.font, theme->Text.color);
		mText->setVerticalAlignment(ALIGN_TOP);

		mSubstring = std::make_shared<TextComponent>(mWindow, substring.c_str(), theme->TextSmall.font, theme->Text.color);
		mSubstring->setOpacity(192);

		setEntry(mText, Vector2i(0, 0), true, true);
		setEntry(mSubstring, Vector2i(0, 1), false, true);

		setSize(Vector2f(0, mText->getSize().y() + mSubstring->getSize().y()));
	}

	virtual void setColor(unsigned int color)
	{
		mText->setColor(color);
		mSubstring->setColor(color);
	}

	std::shared_ptr<TextComponent> mText;
	std::shared_ptr<TextComponent> mSubstring;
};

class SmallMultiLineMenuEntry : public ComponentGrid
{
public:
	SmallMultiLineMenuEntry(Window* window, const std::string& text, const std::string& substring) :
		ComponentGrid(window, Vector2i(1, 2))
	{
		auto theme = ThemeData::getMenuTheme();

		mText = std::make_shared<TextComponent>(mWindow, text.c_str(), theme->TextSmall.font, theme->Text.color);
		mText->setVerticalAlignment(ALIGN_TOP);

		mSubstring = std::make_shared<TextComponent>(mWindow, substring.c_str(), theme->TextSmall.font, theme->Text.color);
		mSubstring->setOpacity(160);

		setEntry(mText, Vector2i(0, 0), true, true);
		setEntry(mSubstring, Vector2i(0, 1), false, true);

		setSize(Vector2f(0, mText->getSize().y() + mSubstring->getSize().y()));
	}

	virtual void setColor(unsigned int color)
	{
		mText->setColor(color);
		mSubstring->setColor(color);
	}

	std::shared_ptr<TextComponent> mText;
	std::shared_ptr<TextComponent> mSubstring;
};