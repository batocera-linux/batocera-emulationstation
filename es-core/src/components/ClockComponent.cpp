#include "components/ClockComponent.h"
#include "Settings.h"
#include <time.h>

ClockComponent::ClockComponent(Window* window) : TextComponent(window)
{	
	mClockElapsed = 0;
}

void ClockComponent::update(int deltaTime)
{
	TextComponent::update(deltaTime);
	
	setVisible(Settings::getInstance()->getBool("DrawClock"));

	if (!isVisible())
		return;
	
	mClockElapsed -= deltaTime;
	if (mClockElapsed <= 0)
	{
		time_t     clockNow = time(0);
		struct tm  clockTstruct = *localtime(&clockNow);

		if (clockTstruct.tm_year > 100)
		{
			// Display the clock only if year is more than 1900+100 ; rpi have no internal clock and out of the networks, the date time information has no value
			// Visit http://en.cppreference.com/w/cpp/chrono/c/strftime for more information about date/time format

			std::string clockBuf;
			if (Settings::getInstance()->getBool("ClockMode12"))
				clockBuf = Utils::Time::timeToString(clockNow, "%I:%M %p");
			else
				clockBuf = Utils::Time::timeToString(clockNow, "%H:%M");
				
			float sx = mSize.x();
				
			mAutoCalcExtent.x() = 1;

			setText(clockBuf);

			if (mSize.x() != sx)
			{
				auto sz = mSize;
				mSize.x() = sx;
				setSize(sz);
			}
		}

		mClockElapsed = 1000; // next update in 1000ms
	}
}
