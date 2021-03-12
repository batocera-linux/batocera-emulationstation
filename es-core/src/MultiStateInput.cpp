#include "MultiStateInput.h"
#include "Window.h"

constexpr auto HOLD_TIME = 500; // ms

MultiStateInput::MultiStateInput(const std::string& buttonName)
{	
	mButtonName = buttonName;
	mTimeHoldingButton = 0;
	mIsPressed = false;
}

bool MultiStateInput::isShortPressed(InputConfig* config, Input input)
{
	if (config->isMappedTo(mButtonName, input))
	{
		bool pressed = (input.value != 0);

		if (pressed == mIsPressed)
			return false;

		mIsPressed = pressed;
		if (!mIsPressed)
			return true;

		mTimeHoldingButton = 0;
	}
	else
		mIsPressed = false;

	return false;
}

bool MultiStateInput::isLongPressed(int deltaTime)
{
	if (mIsPressed)
	{
		mTimeHoldingButton += deltaTime;
		if (mTimeHoldingButton >= HOLD_TIME)
		{
			mIsPressed = false;
			return true;
		}
	}

	return false;
}