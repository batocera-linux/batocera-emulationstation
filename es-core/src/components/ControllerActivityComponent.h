#pragma once
#ifndef ES_APP_COMPONENTS_CONTROLLERACTIVITY_COMPONENT_H
#define ES_APP_COMPONENTS_CONTROLLERACTIVITY_COMPONENT_H

#include "renderers/Renderer.h"
#include "GuiComponent.h"
#include "resources/Font.h"

class TextureResource;

class ControllerActivityComponent : public GuiComponent
{
public:
	ControllerActivityComponent(Window* window);



	void onSizeChanged() override;

	// Multiply all pixels in the image by this color when rendering.
	void setColorShift(unsigned int color);

	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;
	bool input(InputConfig* config, Input input) override;

	virtual void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

	void setSpacing(float spacing) { mSpacing = spacing; }
	void setHorizontalAlignment(Alignment align) { mHorizontalAlignment = align; }


	void setActivityColor(unsigned int color) { mActivityColor = color; }
	void setHotkeyColor(unsigned int color) { mHotkeyColor = color; }

private:
	void	init();

	unsigned int mColorShift;
	unsigned int mActivityColor;
	unsigned int mHotkeyColor;

	std::shared_ptr<TextureResource> mPadTexture;	

	class PlayerPad
	{
	public:
		int  keyState;
		int  timeOut;
	};

	PlayerPad mPads[MAX_PLAYERS];

	float mSpacing;
	Alignment mHorizontalAlignment;
};

#endif // ES_APP_COMPONENTS_RATING_COMPONENT_H
