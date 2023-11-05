#pragma once

#ifndef ES_CORE_COMPONENTS_NETWORK_COMPONENT_H
#define ES_CORE_COMPONENTS_NETWORK_COMPONENT_H

#include "GuiComponent.h"
#include "components/ImageComponent.h"

class Window;

class NetworkIconComponent : public ImageComponent
{
public:
	NetworkIconComponent(Window* window);

	std::string getThemeTypeName() override { return "networkIcon"; }

	void update(int deltaTime) override;
	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

private:
	int mNetworkCheckTime;

	std::string mNetworkIcon;
	std::string mPlanemodeIcon;
};

#endif // ES_CORE_COMPONENTS_NETWORK_COMPONENT_H
