#pragma once

#ifndef ES_CORE_COMPONENTS_BATTTEXT_COMPONENT_H
#define ES_CORE_COMPONENTS_BATTTEXT_COMPONENT_H

#include "GuiComponent.h"
#include "components/TextComponent.h"
#include "utils/Platform.h"

class Window;

class BatteryTextComponent : public TextComponent
{
public:
	BatteryTextComponent(Window* window);

	std::string getThemeTypeName() override { return "batteryText"; }

	virtual void update(int deltaTime);

private:
	int mBatteryInfoCheckTime;
	Utils::Platform::BatteryInformation mBatteryInfo;
};

#endif // ES_CORE_COMPONENTS_BATTTEXT_COMPONENT_H
