#pragma once

#ifndef ES_CORE_COMPONENTS_BATTERYICON_COMPONENT_H
#define ES_CORE_COMPONENTS_BATTERYICON_COMPONENT_H

#include "GuiComponent.h"
#include "components/ImageComponent.h"
#include "utils/Platform.h"

class Window;

class BatteryIconComponent : public ImageComponent
{
public:
	BatteryIconComponent(Window* window);

	std::string getThemeTypeName() override { return "batteryIcon"; }

	void update(int deltaTime) override;
	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

private:
	int mBatteryInfoCheckTime;
	Utils::Platform::BatteryInformation mBatteryInfo;

	std::string mIncharge;
	std::string mFull;
	std::string mAt75;
	std::string mAt50;
	std::string mAt25;
	std::string mEmpty;
};

#endif // ES_CORE_COMPONENTS_NETWORK_COMPONENT_H
