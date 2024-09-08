#pragma once
#ifndef ES_CORE_COMPONENTS_SWITCH_COMPONENT_H
#define ES_CORE_COMPONENTS_SWITCH_COMPONENT_H

#include "components/ImageComponent.h"
#include "GuiComponent.h"

// A very simple "on/off" switch.
// Should hopefully be switched to use images instead of text in the future.
class SwitchComponent : public GuiComponent
{
public:
	SwitchComponent(Window* window, bool state = false, bool hasAuto = false, bool autoState = false);

	bool input(InputConfig* config, Input input) override;
	void render(const Transform4x4f& parentTrans) override;
	void onSizeChanged() override;
	void onOpacityChanged();

	bool getState() const;
	void setState(bool state);
	std::string getValue() const;
	void setValue(const std::string& statestring) override;
	bool changed();

	bool hasAuto() const;
	void setHasAuto(bool hasAuto);
	bool getAutoState() const;
	void setAutoState(bool bAuto);

	virtual std::vector<HelpPrompt> getHelpPrompts() override;

	void setColor(unsigned int color);

	inline void setOnChangedCallback(const std::function<void()>& callback) {
		mOnChangedCallback = callback;
	}

private:
	void onStateChanged();

	ImageComponent mImage;
	bool mState;
	bool mInitialState;
	bool mHasAuto;
	bool mAutoState;
	bool mInitialAutoState;

	std::function<void()> mOnChangedCallback; 
};

#endif // ES_CORE_COMPONENTS_SWITCH_COMPONENT_H
