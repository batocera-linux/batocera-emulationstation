#include "components/SliderComponent.h"
#include "resources/Font.h"
#include "LocaleES.h"
#include "Window.h"
#include "utils/HtmlColor.h"

#define MOVE_REPEAT_DELAY 500  // Time in milliseconds before repeating starts
#define MOVE_REPEAT_RATE 40    // Time in milliseconds between increments

SliderComponent::SliderComponent(Window* window, float min, float max, float increment, const std::string& suffix)
    : GuiComponent(window),
    mMin(min),
    mMax(max),
    mSingleIncrement(increment),
    mValue(min),  // Ensure mValue is initialized here
    mSuffix(suffix),
    mMoveRate(0),
    mMoveAccumulator(0),
    mKnob(window)
{
    assert((min - max) != 0);

    auto menuTheme = ThemeData::getMenuTheme();
    mColor = menuTheme->Text.color;

    mScreenX = -1;
    mIsKnobHot = false;

    // Default value is AUTO (one step below the minimum)
    setAuto(true);

    mKnob.setOrigin(0.5f, 0.5f);
    mKnob.setImage(ThemeData::getMenuTheme()->Icons.knob);
    mKnob.setColorShift(mColor);

    if (Renderer::isSmallScreen())
        setSize(Renderer::getScreenWidth() * 0.25f, menuTheme->Text.font->getLetterHeight());
    else
        setSize(Renderer::getScreenWidth() * 0.15f, menuTheme->Text.font->getLetterHeight());
}

void SliderComponent::setAuto(bool autoMode)
{
    if (autoMode)
    {
        // Set AUTO value to one step below the minimum
        mValue = mMin - mSingleIncrement;
    }
    else
    {
        // Ensure AUTO mode is properly exited
        if (mValue == (mMin - mSingleIncrement))
            mValue = mMin; // Optionally set to min or another default value
    }
}

bool SliderComponent::getAuto() const
{
    // Check if the slider is in AUTO mode
    return mValue == (mMin - mSingleIncrement);
}

void SliderComponent::onOpacityChanged()
{
    mKnob.setOpacity(getOpacity());

    if (mValueCache)
        mValueCache->setColor(getCurColor());
}

unsigned int SliderComponent::getCurColor() const
{
    return Utils::HtmlColor::applyColorOpacity(mColor, getOpacity());
}

void SliderComponent::setColor(unsigned int color)
{
    mColor = color;
    mKnob.setColorShift(mColor);
    onValueChanged();
}

bool SliderComponent::input(InputConfig* config, Input input)
{
    if (config->isMappedLike("left", input))
    {
        if (input.value)
        {
            if (getAuto())
            {
                // If currently at AUTO, do nothing
                return input.value;
            }
            else if (mValue <= mMin)
            {
                // If at or below the min, move to AUTO
                setAuto(true);
            }
            else
            {
                // Move left, decrease the value
                setValue(mValue - mSingleIncrement);
                mMoveRate = -mSingleIncrement;
                mMoveAccumulator = -MOVE_REPEAT_DELAY;
            }

            return input.value;
        }
        else
        {
            mMoveRate = 0;
        }
    }

    if (config->isMappedLike("right", input))
    {
        if (input.value)
        {
            if (getAuto())
            {
                // If at AUTO, move to min
                setValue(mMin);
            }
            else
            {
                // Move right, increase the value
                setValue(mValue + mSingleIncrement);
                mMoveRate = mSingleIncrement;
                mMoveAccumulator = -MOVE_REPEAT_DELAY;
            }

            return input.value;
        }
        else
        {
            mMoveRate = 0;
        }
    }

    return GuiComponent::input(config, input);
}

void SliderComponent::update(int deltaTime)
{
    if (mMoveRate != 0)
    {
        mMoveAccumulator += deltaTime;

        // Apply the movement in small increments
        while (mMoveAccumulator >= MOVE_REPEAT_RATE)
        {
            // Increment the value based on the move rate
            float newValue = mValue + mMoveRate;
            if (newValue < mMin)
                newValue = mMin;
            else if (newValue > mMax)
                newValue = mMax;

            setValue(newValue);

            mMoveAccumulator -= MOVE_REPEAT_RATE;
        }
    }

    GuiComponent::update(deltaTime);
}

void SliderComponent::render(const Transform4x4f& parentTrans)
{
    Transform4x4f trans = parentTrans * getTransform();

    auto rect = Renderer::getScreenRect(trans, mSize);
    if (!Renderer::isVisibleOnScreen(rect))
        return;

    Renderer::setMatrix(trans);

    // Render suffix
    if (mValueCache)
        mFont->renderTextCache(mValueCache.get());

    float width = mSize.x() - mKnob.getSize().x() - (mValueCache ? mValueCache->metrics.size.x() + 4 : 0);

    // Render line
    const float lineWidth = 2;
    unsigned int lineColor = getCurColor();
    Renderer::drawRect(mKnob.getSize().x() / 2, mSize.y() / 2 - lineWidth / 2, width, lineWidth, lineColor);

    // Render knob
    mKnob.setColorShift(getCurColor());
    mKnob.render(trans);

    GuiComponent::renderChildren(trans);
}

void SliderComponent::setValue(float value)
{
    if (value == mValue)
        return;

    // Handle AUTO position dynamically
    if (value == (mMin - mSingleIncrement))
    {
        mValue = mMin - mSingleIncrement;  // AUTO
    }
    else
    {
        if (value < mMin)
            mValue = mMin;
        else if (value > mMax)
            mValue = mMax;
        else
            mValue = value;
    }

    onValueChanged();

    if (mValueChanged)
        mValueChanged(mValue);
}

float SliderComponent::getValue()
{
    return mValue;
}

void SliderComponent::onSizeChanged()
{
    GuiComponent::onSizeChanged();
    mFont = Font::get((int)(mSize.y()), FONT_PATH_LIGHT);
    onValueChanged();
}

void SliderComponent::onValueChanged()
{
    if (mFont)
    {
        std::stringstream ss;

        if (getAuto())
        {
            ss << "AUTO";  // Display "AUTO"
        }
        else
        {
            ss << std::fixed;
            if (mSingleIncrement < 1)
            {
                ss.precision(1);
            }
            else
            {
                ss.precision(0);
            }
            ss << mValue;
            ss << mSuffix;
        }

        const std::string val = ss.str();

        ss.str("");
        ss.clear();
        ss << std::fixed;
        if (mSingleIncrement < 1)
        {
            ss.precision(1);
        }
        else
        {
            ss.precision(0);
        }
        ss << mMax;
        ss << mSuffix;
        const std::string max = ss.str();

        Vector2f textSize = mFont->sizeText(max);
        mValueCache = std::shared_ptr<TextCache>(mFont->buildTextCache(val, mSize.x() - textSize.x(), (mSize.y() - textSize.y()) / 2, getCurColor()));
        mValueCache->metrics.size[0] = textSize.x(); // fudge the width
    }

    mKnob.setResize(0, mSize.y() * 0.7f);
    float lineLength = mSize.x() - mKnob.getSize().x() - (mValueCache ? mValueCache->metrics.size.x() + 4 : 0);

    if (getAuto())
    {
        // Position knob off-screen or handle AUTO specially
        mKnob.setPosition(-mKnob.getSize().x(), mSize.y() / 2); // Adjust as needed
    }
    else
    {
        mKnob.setPosition(((mValue - mMin) / (mMax - mMin)) * lineLength, mSize.y() / 2);
    }
}

std::vector<HelpPrompt> SliderComponent::getHelpPrompts()
{
    std::vector<HelpPrompt> prompts;
    prompts.push_back(HelpPrompt("left/right", _("CHANGE")));
    return prompts;
}

bool SliderComponent::hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult)
{
    auto ret = false; // GuiComponent::hitTest(x, y, parentTransform, pResult);

    auto trans = getTransform() * parentTransform;

    mScreenX = trans.translation().x();

    if (mKnob.hitTest(x, y, trans, pResult))
    {
        if (pResult)
            pResult->push_back(this);

        mIsKnobHot = true;
    }

    return ret || mIsKnobHot;
}

bool SliderComponent::onMouseClick(int button, bool pressed, int x, int y)
{
    if (button == 1)
    {
        if (pressed)
        {
            if (mIsKnobHot)
                mWindow->setMouseCapture(this);
        }
        else if (mWindow->hasMouseCapture(this))
            mWindow->releaseMouseCapture();

        return mWindow->hasMouseCapture(this);
    }

    return false;
}

void SliderComponent::onMouseMove(int x, int y)
{
    if (!mWindow->hasMouseCapture(this))
        return;

    float lineLength = mSize.x() - mKnob.getSize().x() - (mValueCache ? mValueCache->metrics.size.x() + 4 : 0);
    if (lineLength == 0)
        return;

    int pos = x - mScreenX;
    if (pos < 0)
        pos = 0;
    else if (pos >= lineLength)
        pos = lineLength;

    // Determine value based on position
    float value = (float)pos / lineLength;
    value = mMin + (value) * (mMax - mMin);

    // Special case for AUTO: If the position is at the extreme left, set to AUTO
    if (pos == 0)
    {
        setAuto(true);  // Set to AUTO mode dynamically
    }
    else
    {
        setValue(value);
    }
}