#pragma once
#ifndef ES_APP_COMPONENTS_RATING_COMPONENT_H
#define ES_APP_COMPONENTS_RATING_COMPONENT_H

#include "renderers/Renderer.h"
#include "GuiComponent.h"
#include "resources/Font.h"

class TextureResource;

#define NUM_RATING_STARS 5

// Used to visually display/edit some sort of "score" - e.g. 5/10, 3/5, etc.
// setSize(x, y) works a little differently than you might expect:
//   * (0, y != 0) - x will be automatically calculated (5*y).
//   * (x != 0, 0) - y will be automatically calculated (x/5).
//   * (x != 0, y != 0) - you better be sure x = y*5
class RatingComponent : public GuiComponent
{
public:
	RatingComponent(Window* window);

	std::string getThemeTypeName() override { return "rating"; }

	std::string getValue() const override;
	void setValue(const std::string& value) override; // Should be a normalized float (in the range [0..1]) - if it's not, it will be clamped.

	bool input(InputConfig* config, Input input) override;
	void render(const Transform4x4f& parentTrans);

	void onSizeChanged() override;
	void onOpacityChanged() override;
	void onPaddingChanged() override;

	// Multiply all pixels in the image by this color when rendering.
	void setColorShift(unsigned int color);

	virtual void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

	virtual std::vector<HelpPrompt> getHelpPrompts() override;
	void setHorizontalAlignment(Alignment align);
	void setUnfilledColor(unsigned int color);

	ThemeData::ThemeElement::Property getProperty(const std::string name) override;
	void setProperty(const std::string name, const ThemeData::ThemeElement::Property& value) override;

private:
	void updateVertices();
	void updateColors();

	float mValue;

	Renderer::Vertex mVertices[8];

	unsigned int mColorShift;
	unsigned int mUnfilledColor;
	Alignment mHorizontalAlignment;

	std::shared_ptr<TextureResource> mFilledTexture;
	std::shared_ptr<TextureResource> mUnfilledTexture;
};

#endif // ES_APP_COMPONENTS_RATING_COMPONENT_H
