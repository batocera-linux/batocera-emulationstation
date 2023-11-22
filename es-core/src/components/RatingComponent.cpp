#include "components/RatingComponent.h"

#include "resources/TextureResource.h"
#include "utils/StringUtil.h"
#include "ThemeData.h"
#include "Settings.h"

RatingComponent::RatingComponent(Window* window) : GuiComponent(window), mColorShift(0xFFFFFFFF), mUnfilledColor(0xFFFFFFFF)
{
	mHorizontalAlignment = ALIGN_LEFT;
	mValue = 0.5f;
	mSize = Vector2f(64 * NUM_RATING_STARS, 64);

	updateVertices();
}

void RatingComponent::setValue(const std::string& value)
{
	float newValue = Math::clamp(0.0f, 1.0f, Utils::String::toFloat(value));
	if (mValue == newValue)
		return;

	mValue = newValue;
	updateVertices();
}

std::string RatingComponent::getValue() const
{
	// do not use std::to_string here as it will use the current locale
	// and that sometimes encodes decimals as commas
	std::stringstream ss;
	ss << mValue;
	return ss.str();
}

void RatingComponent::onOpacityChanged()
{
	updateColors();
}

void RatingComponent::setColorShift(unsigned int color)
{
	if (mColorShift == color)
		return;

	mColorShift = color;
	updateColors();
}

void RatingComponent::setUnfilledColor(unsigned int color)
{
	if (mUnfilledColor == color)
		return;

	mUnfilledColor = color;
	updateColors();
}

void RatingComponent::onSizeChanged()
{
	GuiComponent::onSizeChanged();

	if (mSize.y() == 0)
		mSize[1] = ((mSize.x() - mPadding.x() - mPadding.z()) / NUM_RATING_STARS) + mPadding.y() + mPadding.w();
	else if (mSize.x() == 0)
		mSize[0] = ((mSize.y() - mPadding.y() - mPadding.w()) * NUM_RATING_STARS) + mPadding.x() + mPadding.z();

	if (mSize.y() > 0)
	{
		size_t heightPx = (size_t)Math::round(mSize.y());
		if (mFilledTexture)
			mFilledTexture->rasterizeAt(heightPx, heightPx);
		if (mUnfilledTexture)
			mUnfilledTexture->rasterizeAt(heightPx, heightPx);
	}

	updateVertices();
}

void RatingComponent::onPaddingChanged()
{
	GuiComponent::onPaddingChanged();
	updateVertices();
}

void RatingComponent::updateVertices()
{
	const float numStars = NUM_RATING_STARS;
	const float sz = getSize().x() - mPadding.x() - mPadding.z();
	const float h = getSize().y() - mPadding.y() - mPadding.w(); // is the same as a single star's width
	
	float w = h * mValue * numStars;
	float fw = h * numStars;

	float opacity = getOpacity() / 255.0;
	const unsigned int color = Renderer::convertColor(mColorShift & 0xFFFFFF00 | (unsigned char)((mColorShift & 0xFF) * opacity));
	const unsigned int unFilledColor = Renderer::convertColor(mUnfilledColor & 0xFFFFFF00 | (unsigned char)((mUnfilledColor & 0xFF) * opacity));

	float x = 0;
	float y = 0;

	if (mPadding != Vector4f::Zero())
	{
		x = mPadding.x();
		y = mPadding.y();
		w += mPadding.x();
		fw += mPadding.x();
	}

	switch (mHorizontalAlignment)
	{
	case ALIGN_RIGHT:
		mVertices[0] = { { sz - fw, y },           { 0.0f,              1.0f }, color };
		mVertices[1] = { { sz - fw, h },           { 0.0f,              0.0f }, color };
		mVertices[2] = { { sz - fw + w,    y },    { mValue * numStars, 1.0f }, color };
		mVertices[3] = { { sz - fw + w,    h },    { mValue * numStars, 0.0f }, color };

		mVertices[4] = { { sz - fw, 0.0f },        { 0.0f,              1.0f }, unFilledColor };
		mVertices[5] = { { sz - fw, h },           { 0.0f,              0.0f }, unFilledColor };
		mVertices[6] = { { sz,   0.0f },           { numStars,          1.0f }, unFilledColor };
		mVertices[7] = { { sz,   h },              { numStars,          0.0f }, unFilledColor };
		break;

	default:
		mVertices[0] = { { x,    y    },           { 0.0f,              1.0f }, color };
		mVertices[1] = { { x,    h    },           { 0.0f,              0.0f }, color };
		mVertices[2] = { { w,    y    },           { mValue * numStars, 1.0f }, color };
		mVertices[3] = { { w,    h    },           { mValue * numStars, 0.0f }, color };
										           
		mVertices[4] = { { x,    y    },           { 0.0f,              1.0f }, unFilledColor };
		mVertices[5] = { { x,    h    },           { 0.0f,              0.0f }, unFilledColor };
		mVertices[6] = { { fw,   y    },           { numStars,          1.0f }, unFilledColor };
		mVertices[7] = { { fw,   h    },           { numStars,          0.0f }, unFilledColor };
		break;
	}

	// round vertices
	for (int i = 0; i < 8; ++i)
		mVertices[i].pos.round();
}

void RatingComponent::updateColors()
{
	float opacity = getOpacity() / 255.0;

	const unsigned int color = Renderer::convertColor(mColorShift & 0xFFFFFF00 | (unsigned char)((mColorShift & 0xFF) * opacity));
	for (int i = 0; i < 4; i++)
		mVertices[i].col = color;

	const unsigned int unFilledColor = Renderer::convertColor(mUnfilledColor & 0xFFFFFF00 | (unsigned char)((mUnfilledColor & 0xFF) * opacity));
	for (int i = 4; i < 8; i++)
		mVertices[i].col = unFilledColor;
}

void RatingComponent::render(const Transform4x4f& parentTrans)
{
	if (mFilledTexture == nullptr)
		mFilledTexture = TextureResource::get(":/star_filled.svg", true, true);

	if (mUnfilledTexture == nullptr)
		mUnfilledTexture = TextureResource::get(":/star_unfilled.svg", true, true);

	if (!isVisible() || mFilledTexture == nullptr || mUnfilledTexture == nullptr)
		return;

	Transform4x4f trans = parentTrans * getTransform();

	auto rect = Renderer::getScreenRect(trans, mSize);
	if (mRotation == 0 && trans.r0().y() == 0 && !Renderer::isVisibleOnScreen(rect))
		return;

	Renderer::setMatrix(trans);

	if (Settings::DebugImage())
		Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), 0xFFFF0033, 0xFFFF0033);

	if (mUnfilledTexture->bind())
	{
		Renderer::drawTriangleStrips(&mVertices[4], 4);
		Renderer::bindTexture(0);
	}

	if (mFilledTexture->bind())
	{
		Renderer::drawTriangleStrips(&mVertices[0], 4);
		Renderer::bindTexture(0);
	}

	renderChildren(trans);
}

bool RatingComponent::input(InputConfig* config, Input input)
{
	if (config->isMappedTo(BUTTON_OK, input) && input.value != 0)
	{
		mValue += 1.f / NUM_RATING_STARS;
		if (mValue > 1.0f)
			mValue = 0.0f;

		updateVertices();
	}

	return GuiComponent::input(config, input);
}

void RatingComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	GuiComponent::applyTheme(theme, view, element, properties);

	using namespace ThemeFlags;

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "rating");
	if (!elem)
		return;

	bool imgChanged = false;

	if (properties & PATH && elem->has("filledPath"))
	{
		mFilledTexture = TextureResource::get(elem->get<std::string>("filledPath"), true, true);
		imgChanged = true;
	}

	if (properties & PATH && elem->has("unfilledPath"))
	{
		mUnfilledTexture = TextureResource::get(elem->get<std::string>("unfilledPath"), true, true);
		imgChanged = true;
	}

	if (properties & COLOR)
	{
		if (elem->has("color"))
			setColorShift(elem->get<unsigned int>("color"));

		if (elem->has("unfilledColor"))
			setUnfilledColor(elem->get<unsigned int>("unfilledColor"));
		else
			setUnfilledColor(mColorShift);
	}

	if (properties & ALIGNMENT && elem->has("horizontalAlignment"))
	{
		std::string str = elem->get<std::string>("horizontalAlignment");
		if (str == "left")
			setHorizontalAlignment(ALIGN_LEFT);
		else if (str == "right")
			setHorizontalAlignment(ALIGN_RIGHT);
		else
			setHorizontalAlignment(ALIGN_CENTER);
	}

	if (elem->has("value"))
	{
		mValue = Math::clamp(0.0f, 1.0f, elem->get<float>("value"));		
		updateVertices();
	}

	if (imgChanged)
		onSizeChanged();
}

std::vector<HelpPrompt> RatingComponent::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt(BUTTON_OK, "add star"));
	return prompts;
}

void RatingComponent::setHorizontalAlignment(Alignment align)
{
	if (mHorizontalAlignment == align)
		return;

	mHorizontalAlignment = align;
	updateVertices();
}

ThemeData::ThemeElement::Property RatingComponent::getProperty(const std::string name)
{
	if (name == "value")
		return mValue;

	return GuiComponent::getProperty(name);
}

void RatingComponent::setProperty(const std::string name, const ThemeData::ThemeElement::Property& value)
{
	if (name == "value" && value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
	{
		mValue = Math::clamp(0.0f, 1.0f, value.f);
		updateVertices();
	}
	else 
		GuiComponent::setProperty(name, value);
}