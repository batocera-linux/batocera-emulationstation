#pragma once
#ifndef ES_CORE_COMPONENTS_SLIDER_COMPONENT_H
#define ES_CORE_COMPONENTS_SLIDER_COMPONENT_H

#include "components/ImageComponent.h"
#include "GuiComponent.h"

class Font;
class TextCache;

// Used to display/edit a value between some min and max values.
class SliderComponent : public GuiComponent
{
public:
	//Minimum value (far left of the slider), maximum value (far right of the slider), increment size (how much just pressing L/R moves by), unit to display (optional).
	SliderComponent(Window* window, float min, float max, float increment, const std::string& suffix = "", bool isAuto = false);

	void setValue(float val);
	float getValue();
	void setAutoMode(bool isAuto);
	std::string getSuffix() {return mSuffix; }

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;
	
	void onOpacityChanged() override;
	void onSizeChanged() override;
	
	virtual std::vector<HelpPrompt> getHelpPrompts() override;
	virtual void setColor(unsigned int color);

	inline void setOnValueChanged(const std::function<void(const float&)>& callback) { mValueChanged = callback; }

	virtual bool hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult = nullptr) override;
	virtual bool onMouseClick(int button, bool pressed, int x, int y) override;
	virtual void onMouseMove(int x, int y) override;

	// Public methods to handle AUTO state
	void setAuto(bool autoMode);
	bool getAuto() const;

private:
	void onValueChanged();
	unsigned int getCurColor() const;

	float mMin, mMax;
	float mValue;
	float mSingleIncrement;
	float mMoveRate;
	int mMoveAccumulator;
	unsigned int mColor;

	ImageComponent mKnob;

	std::string mSuffix;
	std::shared_ptr<Font> mFont;
	std::shared_ptr<TextCache> mValueCache;

	std::function<void(const float&)> mValueChanged;

	bool mIsAutoMode;
	bool	mIsKnobHot;
	int     mScreenX;
};

#endif // ES_CORE_COMPONENTS_SLIDER_COMPONENT_H
