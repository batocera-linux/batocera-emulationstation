#pragma once
#ifndef ES_CORE_COMPONENTS_PPSH_COMPONENT_H
#define ES_CORE_COMPONENTS_PPSH_COMPONENT_H

#include "GuiComponent.h"
#include "ThemeData.h"

#include <map>

class PostProcessShaderComponent : public GuiComponent
{
public:
	PostProcessShaderComponent(Window* window);

	std::string getThemeTypeName() override { return "screenshader"; }

	void render(const Transform4x4f& parentTrans) override;
	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

	ThemeData::ThemeElement::Property getProperty(const std::string name) override;
	void setProperty(const std::string name, const ThemeData::ThemeElement::Property& value) override;

private:
	std::string mShaderPath;
	std::map<std::string, std::string> mParameters;	
};

#endif // ES_CORE_COMPONENTS_PPSH_COMPONENT_H
