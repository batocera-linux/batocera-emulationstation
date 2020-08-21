#pragma once
#ifndef ES_APP_GUIS_GUI_VIDEO_SCREENSAVER_OPTIONS_H
#define ES_APP_GUIS_GUI_VIDEO_SCREENSAVER_OPTIONS_H

#include "GuiScreensaverOptions.h"

class GuiVideoScreensaverOptions : public GuiScreensaverOptions
{
public:
	GuiVideoScreensaverOptions(Window* window, const char* title);
	virtual ~GuiVideoScreensaverOptions();

	void save() override;

private:
	void addEditableTextComponent(const std::string label, std::shared_ptr<GuiComponent> ed, std::string value);
};

#endif // ES_APP_GUIS_GUI_VIDEO_SCREENSAVER_OPTIONS_H
