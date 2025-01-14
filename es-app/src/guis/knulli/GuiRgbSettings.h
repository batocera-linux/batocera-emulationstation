#pragma once
#include "guis/GuiSettings.h"
#include "components/SliderComponent.h"
#include "components/OptionListComponent.h"
#include "components/SwitchComponent.h"
#include <array>
#include <memory>

class GuiRgbSettings : public GuiSettings
{
public:
    GuiRgbSettings(Window* window);

private:
    std::shared_ptr<OptionListComponent<std::string>> createModeOptionList();
    std::shared_ptr<SliderComponent> createSlider(std::string label, float min, float max, float step, std::string unit, std::string description, bool show);
    void setConfigValueForSlider(std::shared_ptr<SliderComponent> slider, float defaultValue, std::string settingsID);
    std::shared_ptr<SwitchComponent> createSwitch(std::string label, std::string variable, std::string description, bool show);
    std::array<float, 3> getRgbValues();
    void setRgbValues(float red, float green, float blue);
    void initializeOnChangeListeners();
    void applyValues();
    void restoreDefaultColors();

    bool isH700;
    bool isA133;
    std::shared_ptr<OptionListComponent<std::string>> optionListMode;
    std::shared_ptr<SliderComponent> sliderLedBrightness;
    std::shared_ptr<SliderComponent> sliderLedSpeed;
    std::shared_ptr<SliderComponent> sliderLedRed;
    std::shared_ptr<SliderComponent> sliderLedGreen;
    std::shared_ptr<SliderComponent> sliderLedBlue;
    std::shared_ptr<SwitchComponent> switchAdaptiveBrightness;
    std::shared_ptr<SliderComponent> sliderLowBatteryThreshold;
    std::shared_ptr<SwitchComponent> switchBatteryCharging;
    std::shared_ptr<SwitchComponent> switchRetroAchievements;

};