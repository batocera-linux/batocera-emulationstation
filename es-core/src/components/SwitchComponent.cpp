#include "SwitchComponent.h"

#include "resources/Font.h"
#include "LocaleES.h"

SwitchComponent::SwitchComponent(Window* window, bool state, bool hasAuto, bool autoState) : GuiComponent(window), mImage(window), mState(state), mInitialState(state), mHasAuto(hasAuto), mAutoState(autoState), mInitialAutoState(autoState)
{
	float height = Font::get(FONT_SIZE_MEDIUM)->getLetterHeight();

	auto menuTheme = ThemeData::getMenuTheme();
	if (menuTheme->Text.font != nullptr)
		height = menuTheme->Text.font->getHeight(1.1f);

	mImage.setImage(menuTheme->Icons.off);
	mImage.setColorShift(menuTheme->Text.color);
	mImage.setResize(0, height);

	if (EsLocale::isRTL())
		mImage.setFlipX(true);

	mSize = mImage.getSize();
}

void SwitchComponent::setColor(unsigned int color) 
{
	mImage.setColorShift(color);
}

void SwitchComponent::onOpacityChanged()
{
	mImage.setOpacity(getOpacity());
}

void SwitchComponent::onSizeChanged()
{
	GuiComponent::onSizeChanged();
	mImage.setSize(mSize);
}

bool SwitchComponent::input(InputConfig* config, Input input)
{
	if(config->isMappedTo(BUTTON_OK, input) && input.value)
	{
		if(mHasAuto) {
			if(mAutoState) {
				mAutoState = false;
				mState     = true;
			} else if(mState) {
			       mState = false;
			} else {
			       mAutoState = true;
			}
		} else {
			mState = !mState;
		}
		onStateChanged();
		return true;
	}

	if (config->isMappedLike("left", input) && input.value && (mState || (mHasAuto && mAutoState)))
	{
		if(mHasAuto && !mAutoState) {
			mAutoState = true;
		} else {
			mAutoState = false;
			mState = false;
		}
		onStateChanged();
		return true;
	}

	if (config->isMappedLike("right", input) && input.value && (!mState || (mHasAuto && mAutoState)))
	{
		if(mHasAuto && !mAutoState) {
			mAutoState = true;
		} else {
			mAutoState = false;
			mState = true;
		}
		onStateChanged();
		return true;
	}

	return false;
}

void SwitchComponent::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();
	
	mImage.render(trans);

	renderChildren(trans);
}

bool SwitchComponent::getState() const
{
	return mState;
}

void SwitchComponent::setState(bool state)
{
	mState = state;
	mInitialState = mState;
	onStateChanged();
}

std::string SwitchComponent::getValue() const
{
	if(mHasAuto && mAutoState) return "auto";
	return mState ?  "true" : "false";
}

void SwitchComponent::setValue(const std::string& statestring)
{
	if(mHasAuto && statestring == "auto") {
		mAutoState = true;
	} else {
		if (statestring == "true")
		{
			mState = true;
		}else
		{
			mState = false;
		}
	}
	onStateChanged();
}

bool SwitchComponent::hasAuto() const {
     return mHasAuto;
}

void SwitchComponent::setHasAuto(bool hasAuto) {
     mHasAuto = hasAuto;
}

bool SwitchComponent::getAutoState() const {
     return mAutoState;
}

void SwitchComponent::setAutoState(bool bAuto) {
     mAutoState = bAuto;
}

void SwitchComponent::onStateChanged()
{
	auto theme = ThemeData::getMenuTheme();
	mImage.setImage((mHasAuto && mAutoState) ? theme->Icons.onoffauto : (mState ? theme->Icons.on : theme->Icons.off));

	if (mOnChangedCallback != nullptr)
		mOnChangedCallback();
}

std::vector<HelpPrompt> SwitchComponent::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt(BUTTON_OK, _("CHANGE")));
	return prompts;
}

bool SwitchComponent::changed() 
{
	return mInitialState != mState || (mHasAuto && mInitialAutoState != mAutoState);
}
