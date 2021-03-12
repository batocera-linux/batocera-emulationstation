#pragma once

#include "GuiComponent.h"

class Window;

class MultiStateInput
{
public:
	MultiStateInput(const std::string& buttonName);

	bool isShortPressed(InputConfig* config, Input input);
	bool isLongPressed(int deltaTime);

private:
	std::string mButtonName;
	int mTimeHoldingButton;
	bool mIsPressed;
};