#pragma once
#ifndef ES_CORE_COMPONENTS_PPSH_COMPONENT_H
#define ES_CORE_COMPONENTS_PPSH_COMPONENT_H

#include "GuiComponent.h"

#include <map>

class PostProcessShaderComponent : public GuiComponent
{
public:
	PostProcessShaderComponent(Window* window);

	void render(const Transform4x4f& parentTrans) override;
	virtual void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

private:
	std::string mShaderPath;
	std::map<std::string, std::string> mParameters;	
};

#endif // ES_CORE_COMPONENTS_PPSH_COMPONENT_H
