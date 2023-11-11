#pragma once

#ifndef ES_CORE_COMPONENTS_NETWORK_COMPONENT_H
#define ES_CORE_COMPONENTS_NETWORK_COMPONENT_H

#include "GuiComponent.h"
#include "components/ImageComponent.h"
#include "watchers/WatchersManager.h"

class Window;

class NetworkIconComponent : public ImageComponent, IWatcherNotify
{
public:
	NetworkIconComponent(Window* window);
	~NetworkIconComponent();

	std::string getThemeTypeName() override { return "networkIcon"; }

	void update(int deltaTime) override;
	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

	void OnWatcherChanged(IWatcher* component) override;
	
private:
	bool mConnected;
	bool mPlaneMode;
	bool mDirty;
	
	std::string mNetworkIcon;
	std::string mPlanemodeIcon;
};

#endif // ES_CORE_COMPONENTS_NETWORK_COMPONENT_H
