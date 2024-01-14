#pragma once

#ifndef ES_CORE_COMPONENTS_RECTANGLE_COMPONENT_H
#define ES_CORE_COMPONENTS_RECTANGLE_COMPONENT_H

#include "GuiComponent.h"
#include "components/ImageComponent.h"
#include "renderers/Renderer.h"

class Window;
class TextureResource;

class RectangleComponent : public GuiComponent
{
public:
	RectangleComponent(Window* window);
	~RectangleComponent();

	std::string getThemeTypeName() override { return "rectangle"; }

	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;
	void render(const Transform4x4f& parentTrans) override;

	ThemeData::ThemeElement::Property getProperty(const std::string name) override;
	void setProperty(const std::string name, const ThemeData::ThemeElement::Property& value) override;

private:
	unsigned int mColor;
	unsigned int mBorderColor;
	float		 mBorderSize;
	float		 mRoundCorners;

	std::shared_ptr<TextureResource> mWhiteTexture;
	Renderer::ShaderInfo mCustomShader;
};

#endif // ES_CORE_COMPONENTS_RECTANGLE_COMPONENT_H
