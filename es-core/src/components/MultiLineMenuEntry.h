#pragma once

#include <string>
#include "components/ComponentGrid.h"

class TextComponent;
class Window;

class MultiLineMenuEntry : public ComponentGrid
{
public:
	MultiLineMenuEntry(Window* window, const std::string& text, const std::string& substring, bool multiLine = false);

	void setColor(unsigned int color) override;
	void onSizeChanged() override;

protected:
	bool mMultiLine;
	bool mSizeChanging;
	std::shared_ptr<TextComponent> mText;
	std::shared_ptr<TextComponent> mSubstring;
};
