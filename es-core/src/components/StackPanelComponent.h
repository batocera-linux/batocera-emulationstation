#pragma once
#ifndef ES_CORE_COMPONENTS_STACK_COMPONENT_H
#define ES_CORE_COMPONENTS_STACK_COMPONENT_H

#include "GuiComponent.h"
#include "ThemeData.h"

#include <map>

class StackPanelComponent : public GuiComponent
{
public:
	StackPanelComponent(Window* window);

	std::string getThemeTypeName() override { return "stackpanel"; }

	void render(const Transform4x4f& parentTrans) override;
	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

	void onSizeChanged() override;
	void update(int deltaTime) override;
	void setSize(float w, float h);

	void recalcLayout() override;

private:
	void performLayout();

	bool mHorizontal;
	bool mReverse;

	float mSeparator;

	Vector2f mLastSize;
};

#endif // ES_CORE_COMPONENTS_STACK_COMPONENT_H
