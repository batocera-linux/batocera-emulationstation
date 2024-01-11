#pragma once

#ifndef ES_CORE_COMPONENTS_CLOCK_COMPONENT_H
#define ES_CORE_COMPONENTS_CLOCK_COMPONENT_H

#include "GuiComponent.h"
#include "components/TextComponent.h"

class Window;

class ClockComponent : public TextComponent
{
public:
	ClockComponent(Window* window);

	std::string getThemeTypeName() override { return "clock"; }

	virtual void update(int deltaTime);

private:
	int mClockElapsed;
};

#endif // ES_CORE_COMPONENTS_CLOCK_COMPONENT_H
