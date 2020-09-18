#pragma once
#ifndef ES_APP_GUIS_GUI_GENERAL_SCREENSAVER_OPTIONS_H
#define ES_APP_GUIS_GUI_GENERAL_SCREENSAVER_OPTIONS_H

#include "GuiScreensaverOptions.h"

class TextComponent;

class GuiGeneralScreensaverOptions : public GuiScreensaverOptions
{
public:
	GuiGeneralScreensaverOptions(Window* window, int selectItem = -1);

private:
	void addVideoScreensaverOptions(int selectItem);
	void addSlideShowScreensaverOptions(int selectItem);	

	std::shared_ptr<TextComponent> addEditableTextComponent(const std::string label, std::string value);
};

#endif // ES_APP_GUIS_GUI_GENERAL_SCREENSAVER_OPTIONS_H
