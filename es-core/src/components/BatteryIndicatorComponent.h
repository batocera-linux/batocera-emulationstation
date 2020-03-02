#pragma once

#ifndef ES_APP_COMPONENTS_BATTERYINDICATOR_COMPONENT_H
#define ES_APP_COMPONENTS_BATTERYINDICATOR_COMPONENT_H

#include "renderers/Renderer.h"
#include "GuiComponent.h"
#include "resources/Font.h"
#include "components/ImageComponent.h"

class TextureResource;

class BatteryIndicatorComponent : public GuiComponent
{
public:
	BatteryIndicatorComponent(Window* window);

	// Multiply all pixels in the image by this color when rendering.
	void setColorShift(unsigned int color);

	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;

	virtual void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;
	void onSizeChanged() override;

	bool hasBattery() { return mHasBattery; }

private:
	void	updateBatteryInfo();

	void	init();

	ImageComponent mImage;	
	std::string mTexturePath;

	int mCheckTime;

	bool mHasBattery;
	int  mBatteryLevel;
	bool mIsCharging;	

	std::string mIncharge;
	std::string mFull;
	std::string mAt75;
	std::string mAt50;
	std::string mAt25;
	std::string mEmpty;
};

#endif // ES_APP_COMPONENTS_RATING_COMPONENT_H
