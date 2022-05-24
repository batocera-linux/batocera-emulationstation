#include "components/TextComponent.h"

#include "utils/StringUtil.h"
#include "Log.h"
#include "Settings.h"

#define AUTO_SCROLL_RESET_DELAY 6000 // ms to reset to top after we reach the bottom
#define AUTO_SCROLL_DELAY 6000 // ms to wait before we start to scroll
#define AUTO_SCROLL_SPEED 150 // ms between scrolls


TextComponent::TextComponent(Window* window) : GuiComponent(window), 
	mFont(Font::get(FONT_SIZE_MEDIUM)), mUppercase(false), mColor(0x000000FF), mAutoCalcExtent(true, true),
	mHorizontalAlignment(ALIGN_LEFT), mVerticalAlignment(ALIGN_CENTER), mLineSpacing(1.5f), mBgColor(0),
	mRenderBackground(false), mGlowColor(0), mGlowSize(2), mPadding(Vector4f(0, 0, 0, 0)), mGlowOffset(Vector2f(0,0)),
	mReflection(0.0f, 0.0f), mReflectOnBorders(false), mAutoScroll(AutoScrollType::NONE), mTextLength(-1)
{	
	mMarqueeOffset = 0;
	mMarqueeOffset2 = 0;
	mMarqueeTime = 0;	
}

TextComponent::TextComponent(Window* window, const std::string& text, const std::shared_ptr<Font>& font, unsigned int color, Alignment align,
	Vector3f pos, Vector2f size, unsigned int bgcolor) : GuiComponent(window), 
	mFont(NULL), mUppercase(false), mColor(0x000000FF), mAutoCalcExtent(true, true),
	mHorizontalAlignment(align), mVerticalAlignment(ALIGN_CENTER), mLineSpacing(1.5f), mBgColor(0),
	mRenderBackground(false), mGlowColor(0), mGlowSize(2), mPadding(Vector4f(0, 0, 0, 0)), mGlowOffset(Vector2f(0, 0)),
	mReflection(0.0f, 0.0f), mReflectOnBorders(false), mAutoScroll(AutoScrollType::NONE), mTextLength(-1)
{
	setFont(font);
	setColor(color);
	setBackgroundColor(bgcolor);
	setText(text);
	setPosition(pos);
	setSize(size);

	mMarqueeOffset = 0;
	mMarqueeOffset2 = 0;
	mMarqueeTime = 0;	
}

void TextComponent::onSizeChanged()
{
	GuiComponent::onSizeChanged();
	mAutoCalcExtent = Vector2i((getSize().x() == 0), (getSize().y() == 0));
	onTextChanged();
}

void TextComponent::setFont(const std::shared_ptr<Font>& font)
{
	if (mFont == font)
		return;

	mFont = font;
	onTextChanged();
}

void TextComponent::setFont(std::string path, int size)
{
	std::shared_ptr<Font> font;
	int fontSize = size > 0 ? size : (mFont ? mFont->getSize() : FONT_SIZE_MEDIUM);
	std::string fontPath = !path.empty() ? path : (mFont ? mFont->getPath() : Font::getDefaultPath());

	font = Font::get(fontSize, fontPath);
	if (mFont != font)
	{
		mFont = font;
		onTextChanged();
	}
}

//  Set the color of the font/text
void TextComponent::setColor(unsigned int color)
{
	if (mColor == color)
		return;

	mColor = color;
	onColorChanged();
}

//  Set the color of the background box
void TextComponent::setBackgroundColor(unsigned int color)
{
	mBgColor = color;
}

void TextComponent::setRenderBackground(bool render)
{
	mRenderBackground = render;
}

//  Scale the opacity
void TextComponent::setOpacity(unsigned char opacity)
{
	if (opacity == mOpacity)
		return;

	mOpacity = opacity;
	onColorChanged();
}

void TextComponent::setText(const std::string& text)
{
	if (mText == text)
		return;

	mText = text;
	mMarqueeOffset = 0;
	mMarqueeOffset2 = 0;

	if (mAutoScroll != AutoScrollType::NONE && !mText.empty())
		mMarqueeTime = -AUTO_SCROLL_DELAY + AUTO_SCROLL_SPEED;
	else 
		mMarqueeTime = 0;

	onTextChanged();
}

void TextComponent::setUppercase(bool uppercase)
{
	if (mUppercase == uppercase)
		return;

	mUppercase = uppercase;
	onTextChanged();
}

void TextComponent::render(const Transform4x4f& parentTrans)
{
	if (!mVisible)
		return;

	Transform4x4f trans = parentTrans * getTransform();

	if (!Renderer::isVisibleOnScreen(trans.translation().x(), trans.translation().y(), mSize.x() * trans.r0().x(), mSize.y() * trans.r1().y()))
		return;

	if (mRenderBackground && mBgColor != 0)
	{
		Renderer::setMatrix(trans);

		auto bgColor = mBgColor & 0xFFFFFF00 | (unsigned char)((mBgColor & 0xFF) * (mOpacity / 255.0));
		Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), bgColor, bgColor);
	}

	if (mTextCache == nullptr && mFont != nullptr && !mText.empty())
	{
		buildTextCache();
		onColorChanged();
	}

	if (mTextCache == nullptr || mFont == nullptr)
		return;

	if (mAutoScroll != AutoScrollType::NONE)
		Renderer::pushClipRect(trans.translation().x(), trans.translation().y(), mSize.x() * trans.r0().x(), mSize.y() * trans.r1().y());

	beginCustomClipRect();

	const Vector2f& textSize = mTextCache->metrics.size;

	float yOff = 0;
	switch (mVerticalAlignment)
	{
	case ALIGN_BOTTOM:
		yOff = (getSize().y() - textSize.y());
		break;
	case ALIGN_CENTER:
		yOff = (getSize().y() - textSize.y()) / 2.0f;
		break;
	}

	Vector3f off(mPadding.x(), mPadding.y() + yOff, 0);

	if (Settings::DebugText())
	{
		// draw the "textbox" area, what we are aligned within
		Renderer::setMatrix(trans);
		Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), 0xFF000033, 0xFF000033);
	}

	Transform4x4f drawTrans = trans;

	if (mMarqueeOffset != 0.0)
	{
		if (mAutoScroll == AutoScrollType::VERTICAL)
			trans.translate(off - Vector3f(0, (float)mMarqueeOffset, 0));
		else
			trans.translate(off - Vector3f((float)mMarqueeOffset, 0, 0));
	}
	else
		trans.translate(off);

	// draw the text area, where the text actually is going
	if (Settings::DebugText())
	{
		Renderer::setMatrix(trans);

		switch (mHorizontalAlignment)
		{
		case ALIGN_LEFT:
			Renderer::drawRect(0.0f, 0.0f, mTextCache->metrics.size.x(), mTextCache->metrics.size.y(), 0x00000033, 0x00000033);
			break;
		case ALIGN_CENTER:
			Renderer::drawRect((mSize.x() - mTextCache->metrics.size.x()) / 2.0f, 0.0f, mTextCache->metrics.size.x(), mTextCache->metrics.size.y(), 0x00000033, 0x00000033);
			break;
		case ALIGN_RIGHT:
			Renderer::drawRect(mSize.x() - mTextCache->metrics.size.x(), 0.0f, mTextCache->metrics.size.x(), mTextCache->metrics.size.y(), 0x00000033, 0x00000033);
			break;
		}
	}
		
	mFont->renderTextCacheEx(mTextCache.get(), trans, mGlowSize, mGlowColor, mGlowOffset, mOpacity);

	// render currently selected item text again if
	// marquee is scrolled far enough for it to repeat

	if (mMarqueeOffset2 != 0.0 && mAutoScroll != AutoScrollType::VERTICAL)
	{
		trans = drawTrans;
		trans.translate(off - Vector3f((float)mMarqueeOffset2, 0, 0));
		mFont->renderTextCacheEx(mTextCache.get(), trans, mGlowSize, mGlowColor, mGlowOffset, mOpacity);
	}

	if (mReflection.x() != 0 || mReflection.y() != 0)
	{
		Transform4x4f mirror = trans;
		mirror.translate(-off);
		mirror.r1().y() = -mirror.r1().y();
		mirror.r3().y() = mirror.r3().y() + off.y() + textSize.y();

		if (mReflectOnBorders)
			mirror.r3().y() = mirror.r3().y() + mSize.y();
		else
			mirror.r3().y() = mirror.r3().y() + textSize.y();

		Renderer::setMatrix(mirror);

		float baseOpacity = mOpacity / 255.0;
		float alpha = baseOpacity * ((mColor & 0x000000ff)) / 255.0;
		float alpha2 = baseOpacity * alpha * mReflection.y();

		alpha *= mReflection.x();

		const unsigned int colorT = Renderer::convertColor((mColor & 0xffffff00) + (unsigned char)(255.0*alpha));
		const unsigned int colorB = Renderer::convertColor((mColor & 0xffffff00) + (unsigned char)(255.0*alpha2));

		mFont->renderGradientTextCache(mTextCache.get(), colorB, colorT);
	}

	endCustomClipRect();

	if (mAutoScroll != AutoScrollType::NONE)
		Renderer::popClipRect();
}

void TextComponent::onTextChanged()
{
	mTextLength = -1;
	mTextCache = nullptr;

	if (mAutoCalcExtent.x())
	{
		mSize = mFont->sizeText(mUppercase ? Utils::String::toUpper(mText) : mText, mLineSpacing);
		mSize[0] += mPadding.x() + mPadding.z();
	}
	else if(mAutoCalcExtent.y())
		mSize[1] = mFont->sizeWrappedText(mUppercase ? Utils::String::toUpper(mText) : mText, getSize().x(), mLineSpacing).y() + mPadding.y() + mPadding.w();
}

void TextComponent::buildTextCache()
{	
	if(!mFont || mText.empty())
	{
		mTextCache.reset();
		return;
	}

	int sx = mSize.x() - mPadding.x() - mPadding.z();
	int sy = mSize.y() - mPadding.y() - mPadding.w();

	std::string text = mUppercase ? Utils::String::toUpper(mText) : mText;

	std::shared_ptr<Font> f = mFont;
	const bool isMultiline = (mAutoScroll != AutoScrollType::HORIZONTAL) && (mSize.y() == 0 || sy > f->getHeight(mLineSpacing)*1.8f);

	bool addAbbrev = false;
	if(!isMultiline)
	{
		size_t newline = text.find('\n');
		text = text.substr(0, newline); // single line of text - stop at the first newline since it'll mess everything up
		addAbbrev = newline != std::string::npos;
	}

	auto color = mColor & 0xFFFFFF00 | (unsigned char)((mColor & 0xFF) * (mOpacity / 255.0));

	Vector2f size = f->sizeText(text);
	if (!isMultiline)
	{
		if (sx && text.size() && (size.x() > sx || addAbbrev) && (mAutoScroll != AutoScrollType::HORIZONTAL))
		{
			// abbreviate text
			const std::string abbrev = "...";
			Vector2f abbrevSize = f->sizeText(abbrev);

			while (text.size() && size.x() + abbrevSize.x() > sx)
			{
				size_t newSize = Utils::String::prevCursor(text, text.size());
				text.erase(newSize, text.size() - newSize);
				size = f->sizeText(text);
			}

			text.append(abbrev);
		}

		mTextCache = std::shared_ptr<TextCache>(f->buildTextCache(text, Vector2f(0, 0), color, sx, mHorizontalAlignment, mLineSpacing));
	}
	else
		mTextCache = std::shared_ptr<TextCache>(f->buildTextCache(f->wrapText(text, sx), Vector2f(0, 0), color, sx, mHorizontalAlignment, mLineSpacing));
}

void TextComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (!mShowing)
	{
		mMarqueeTime = 0;
		mMarqueeOffset = 0;
		mMarqueeOffset2 = 0;
		return;
	}

	int sy = mSize.y() - mPadding.y() - mPadding.w();
	const bool isMultiline = mAutoScroll != AutoScrollType::HORIZONTAL && (mSize.y() == 0 || sy > mFont->getHeight()*1.95f);

	if (mAutoScroll == AutoScrollType::HORIZONTAL && !isMultiline && mSize.x() > 0)
	{
		// always reset the marquee offsets
		mMarqueeOffset = 0;
		mMarqueeOffset2 = 0;

		std::string text = mUppercase ? Utils::String::toUpper(mText) : mText;

		// if we're not scrolling and this object's text goes outside our size, marquee it!

		if (mTextLength < 0)
			mTextLength = mFont->sizeText(text).x();

		const float textLength = mTextLength;
		const float limit = mSize.x() - mPadding.x() - mPadding.z();
		
		if (textLength > limit)
		{
			// loop
			// pixels per second ( based on nes-mini font at 1920x1080 to produce a speed of 200 )
			const float speed = mFont->sizeText("ABCDEFGHIJKLMNOPQRSTUVWXYZ").x() * 0.247f;
			const float delay = 1000;
			const float scrollLength = textLength;
			const float returnLength = speed * 1.5f;
			const float scrollTime = (scrollLength * 1000) / speed;
			const float returnTime = (returnLength * 1000) / speed;
			const int   maxTime = (int)(delay + scrollTime + returnTime);

			mMarqueeTime += deltaTime;
			while (mMarqueeTime > maxTime)
				mMarqueeTime -= maxTime;

			mMarqueeOffset = (int)(Math::Scroll::loop(delay, scrollTime + returnTime, (float)mMarqueeTime, scrollLength + returnLength));

			if (mMarqueeOffset > (scrollLength - (limit - returnLength)))
				mMarqueeOffset2 = (int)(mMarqueeOffset - (scrollLength + returnLength));
		}
	}
	else if(mAutoScroll == AutoScrollType::VERTICAL && mSize.y() > 0)
	{
		std::string text = mUppercase ? Utils::String::toUpper(mText) : mText;

		// if we're not scrolling and this object's text goes outside our size, marquee it!

		if (mTextLength < 0)
			mTextLength = mFont->sizeWrappedText(mUppercase ? Utils::String::toUpper(mText) : mText, getSize().x(), mLineSpacing).y();

		int textLength = mTextLength;
		int limit = mSize.y() - mPadding.y() - mPadding.y();

		if (textLength > limit)
		{
			mMarqueeTime += deltaTime;

			while (mMarqueeTime >= AUTO_SCROLL_SPEED)
			{
				mMarqueeOffset += 1;
				mMarqueeTime -= AUTO_SCROLL_SPEED;
			}
			
			if (textLength < limit)
				mMarqueeOffset = 0;
			else if (mMarqueeOffset + limit >= textLength)
			{
				mMarqueeOffset = textLength - limit;
				mMarqueeOffset2 += deltaTime;
				if (mMarqueeOffset2 >= AUTO_SCROLL_RESET_DELAY)
				{
					mMarqueeOffset = 0;
					mMarqueeOffset2 = 0;
					mMarqueeTime = -AUTO_SCROLL_DELAY + AUTO_SCROLL_SPEED;
				}
			}
		}
	}
	else
	{
		mMarqueeTime = 0;
		mMarqueeOffset = 0;
		mMarqueeOffset2 = 0;
	}
}

void TextComponent::onShow()
{
	GuiComponent::onShow();
	
	mMarqueeOffset = 0;
	mMarqueeOffset2 = 0;

	if (mAutoScroll == AutoScrollType::VERTICAL)
		mMarqueeTime = -AUTO_SCROLL_DELAY + AUTO_SCROLL_SPEED;
	else
		mMarqueeTime = 0;
}

void TextComponent::onColorChanged()
{
	if (mTextCache)
		mTextCache->setColor(mColor & 0xFFFFFF00 | (unsigned char)((mColor & 0xFF) * (mOpacity / 255.0)));
}

void TextComponent::setHorizontalAlignment(Alignment align)
{
	if (mHorizontalAlignment == align)
		return;

	mHorizontalAlignment = align;
	onTextChanged();
}

void TextComponent::setVerticalAlignment(Alignment align)
{
	mVerticalAlignment = align;
}

void TextComponent::setLineSpacing(float spacing)
{
	if (mLineSpacing == spacing)
		return;

	mLineSpacing = spacing;
	onTextChanged();
}

void TextComponent::setValue(const std::string& value)
{
	setText(value);
}

std::string TextComponent::getValue() const
{
	return mText;
}

void TextComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	GuiComponent::applyTheme(theme, view, element, properties);

	using namespace ThemeFlags;

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "text");
	if(!elem)
		return;

	if (properties & ALIGNMENT)
	{
		if (elem->has("alignment"))
		{
			std::string str = elem->get<std::string>("alignment");
			if (str == "left")
				setHorizontalAlignment(ALIGN_LEFT);
			else if (str == "center")
				setHorizontalAlignment(ALIGN_CENTER);
			else if (str == "right")
				setHorizontalAlignment(ALIGN_RIGHT);
			else
				LOG(LogError) << "Unknown text alignment string: " << str;
		}

		if (elem->has("verticalAlignment"))
		{
			std::string str = elem->get<std::string>("verticalAlignment");
			if (str == "top")
				setVerticalAlignment(ALIGN_TOP);
			else if (str == "center")
				setVerticalAlignment(ALIGN_CENTER);
			else if (str == "bottom")
				setVerticalAlignment(ALIGN_BOTTOM);
			else
				LOG(LogError) << "Unknown text alignment string: " << str;
		}

		if (elem->has("padding"))
		{
			Vector2f scale = getParent() ? getParent()->getSize() : Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
			mPadding = elem->get<Vector4f>("padding") * Vector4f(scale.x(), scale.y(), scale.x(), scale.y());
		}
		else
			mPadding = Vector4f::Zero();
	}

	if (properties & TEXT)
	{
		if (elem->has("text"))
		{
			mSourceText = elem->get<std::string>("text");
			setText(mSourceText);
		}
		else
			mSourceText = "";
	}

	if(properties & FORCE_UPPERCASE && elem->has("forceUppercase"))
		setUppercase(elem->get<bool>("forceUppercase"));

	if(properties & LINE_SPACING && elem->has("lineSpacing"))
		setLineSpacing(elem->get<float>("lineSpacing"));

	if (properties & COLOR)
	{
		if (elem->has("color"))
			setColor(elem->get<unsigned int>("color"));

		if (elem->has("backgroundColor"))
		{
			setBackgroundColor(elem->get<unsigned int>("backgroundColor"));
			setRenderBackground(true);
		}
		else 
			setRenderBackground(false);

		if (elem->has("glowColor"))
			mGlowColor = elem->get<unsigned int>("glowColor");
		else
			mGlowColor = 0;

		if (elem->has("glowSize"))
			mGlowSize = (int)elem->get<float>("glowSize");

		if (elem->has("glowOffset"))
			mGlowOffset = elem->get<Vector2f>("glowOffset");

		if (elem->has("reflexion"))
			mReflection = elem->get<Vector2f>("reflexion");
		else
			mReflection = Vector2f::Zero();

		if (elem->has("reflexionOnFrame"))
			mReflectOnBorders = elem->get<bool>("reflexionOnFrame");
		else
			mReflectOnBorders = false;

		if (elem->has("singleLineScroll"))
			setAutoScroll(elem->get<bool>("singleLineScroll"));
		else if (elem->has("autoScroll"))
		{
			auto autoScroll = elem->get<std::string>("autoScroll");
			if (autoScroll == "horizontal")
				setAutoScroll(AutoScrollType::HORIZONTAL);
			else if (autoScroll == "vertical")
				setAutoScroll(AutoScrollType::VERTICAL);
			else
				setAutoScroll(AutoScrollType::NONE);
		}
		else 
			setAutoScroll(AutoScrollType::NONE);
	}

	setFont(Font::getFromTheme(elem, properties, mFont));
}

void TextComponent::setAutoScroll(bool scroll) 
{ 
	setAutoScroll(scroll ? AutoScrollType::HORIZONTAL : AutoScrollType::NONE); 
}

void TextComponent::setAutoScroll(AutoScrollType value)
{
	if (mAutoScroll == value)
		return;

	mAutoScroll = value;
	onSizeChanged();	
}

ThemeData::ThemeElement::Property TextComponent::getProperty(const std::string name)
{
	Vector2f scale = getParent() ? getParent()->getSize() : Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	if (name == "size" || name == "maxSize" || name == "minSize")
		return mSize / scale;
	else if (name == "color")
		return mColor;
	else if (name == "backgroundColor")
		return mBgColor;
	else if (name == "glowColor")
		return mGlowColor;
	else if (name == "glowSize")
		return mGlowSize;
	else if (name == "glowOffset")
		return mGlowOffset;
	else if (name == "reflexion")
		return mReflection;
	else if (name == "lineSpacing")
		return mLineSpacing;
	else if (name == "text")
		return mText;
	else if (name == "value")
		return mText;

	return GuiComponent::getProperty(name);
}

void TextComponent::setProperty(const std::string name, const ThemeData::ThemeElement::Property& value)
{
	Vector2f scale = getParent() ? getParent()->getSize() : Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	if (name == "color" && value.type == ThemeData::ThemeElement::Property::PropertyType::Int)
		setColor(value.i);
	else if (name == "backgroundColor" && value.type == ThemeData::ThemeElement::Property::PropertyType::Int)
		setBackgroundColor(value.i);
	else if (name == "glowColor" && value.type == ThemeData::ThemeElement::Property::PropertyType::Int)
		setGlowColor(value.i);
	else if (name == "glowSize" && value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
		setGlowSize(value.f);
	else if (name == "glowOffset" && value.type == ThemeData::ThemeElement::Property::PropertyType::Pair)
		setGlowOffset(value.v.x(), value.v.y());
	else if (name == "reflexion" && value.type == ThemeData::ThemeElement::Property::PropertyType::Pair)
		mReflection = value.v;
	else if (name == "lineSpacing" && value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
		setLineSpacing(value.f);
	else if (name == "text" && value.type == ThemeData::ThemeElement::Property::PropertyType::String)
		setText(value.s);	
	else if (name == "value" && value.type == ThemeData::ThemeElement::Property::PropertyType::String)
		setValue(value.s);
	else
		GuiComponent::setProperty(name, value);
}

void TextComponent::setPadding(const Vector4f padding) 
{ 
	if (mPadding == padding) 
		return;  
	
	mPadding = padding; 
	onTextChanged();
}