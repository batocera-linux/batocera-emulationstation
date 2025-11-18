#pragma once

#ifndef ES_CORE_COMPONENTS_BATTERYICON_COMPONENT_H
#define ES_CORE_COMPONENTS_BATTERYICON_COMPONENT_H

#include "GuiComponent.h"
#include "components/ImageComponent.h"
#include "utils/Platform.h"
#include "Window.h"
#include "PowerSaver.h"
#include "watchers/WatchersManager.h"

class Window;

class BatteryIconComponent : public ImageComponent, IWatcherNotify
{
public:
	BatteryIconComponent(Window* window);
	~BatteryIconComponent();

	std::string getThemeTypeName() override { return "batteryIcon"; }

	void update(int deltaTime) override;
	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

	void OnWatcherChanged(IWatcher* component) override;

	void onSizeChanged() override;

private:
	Utils::Platform::BatteryInformation mBatteryInfo;
	bool mDirty;

    std::shared_ptr<TextureResource> mTexIncharge;
    std::shared_ptr<TextureResource> mTexFull;
    std::shared_ptr<TextureResource> mTexAt75;
    std::shared_ptr<TextureResource> mTexAt50;
    std::shared_ptr<TextureResource> mTexAt25;
    std::shared_ptr<TextureResource> mTexEmpty;
};

#endif // ES_CORE_COMPONENTS_NETWORK_COMPONENT_H
