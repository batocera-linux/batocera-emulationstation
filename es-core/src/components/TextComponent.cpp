#include "components/TextComponent.h"

#include "utils/StringUtil.h"
#include "Log.h"
#include "Settings.h"

TextComponent::TextComponent(Window* window) : GuiComponent(window), 
	mFont(Font::get(FONT_SIZE_MEDIUM)), mUppercase(false), mColor(0x000000FF), mAutoCalcExtent(true, true),
	mHorizontalAlignment(ALIGN_LEFT), mVerticalAlignment(ALIGN_CENTER), mLineSpacing(1.5f), mBgColor(0),
	mRenderBackground(false), mGlowColor(0), mGlowSize(2), mPadding(Vector4f(0, 0, 0, 0)), mGlowOffset(Vector2f(0,0)),
	mReflection(0.0f, 0.0f), mReflectOnBorders(false)
{	
	mMarqueeOffset = 0;
	mMarqueeOffset2 = 0;
	mMarqueeTime = 0;

	mAutoScroll = false;

}

TextComponent::TextComponent(Window* window, const std::string& text, const std::shared_ptr<Font>& font, unsigned int color, Alignment align,
	Vector3f pos, Vector2f size, unsigned int bgcolor) : GuiComponent(window), 
	mFont(NULL), mUppercase(false), mColor(0x000000FF), mAutoCalcExtent(true, true),
	mHorizontalAlignment(align), mVerticalAlignment(ALIGN_CENTER), mLineSpacing(1.5f), mBgColor(0),
	mRenderBackground(false), mGlowColor(0), mGlowSize(2), mPadding(Vector4f(0, 0, 0, 0)), mGlowOffset(Vector2f(0, 0)),
	mReflection(0.0f, 0.0f), mReflectOnBorders(false)
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

	mAutoScroll = false;
}

void TextComponent::onSizeChanged()
{
	mAutoCalcExtent = Vector2i((getSize().x() == 0), (getSize().y() == 0));
	onTextChanged();
}

void TextComponent::setFont(const std::shared_ptr<Font>& font)
{
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
	mMarqueeTime = 0;

	onTextChanged();
}

void TextComponent::setUppercase(bool uppercase)
{
	mUppercase = uppercase;
	onTextChanged();
}

void TextComponent::renderSingleGlow(const Transform4x4f& parentTrans, float yOff, float x, float y)
{
	Vector3f off = Vector3f(mPadding.x() + x + mGlowOffset.x(), mPadding.y() + yOff + y + mGlowOffset.y(), 0);
	Transform4x4f trans = parentTrans * getTransform();

	trans.translate(off);
	trans.round();

	Renderer::setMatrix(trans);

	unsigned char alpha = (unsigned char) ((mGlowColor & 0xFF) * (mOpacity / 255.0));
	unsigned int color = (mGlowColor & 0xFFFFFF00) | alpha;

	mTextCache->setColor(color);
	mFont->renderTextCache(mTextCache.get());	
}

void TextComponent::renderGlow(const Transform4x4f& parentTrans, float yOff, float xOff)
{
	Transform4x4f glowTrans = parentTrans;
	if (xOff != 0.0)
		glowTrans.translate(Vector3f(xOff, 0, 0));

	int x = -mGlowSize;
	int y = -mGlowSize;
	renderSingleGlow(glowTrans, yOff, x, y);

	for (int i = 0; i < 2 * mGlowSize; i++)
		renderSingleGlow(glowTrans, yOff, ++x, y);

	for (int i = 0; i < 2 * mGlowSize; i++)
		renderSingleGlow(glowTrans, yOff, x, ++y);

	for (int i = 0; i < 2 * mGlowSize; i++)
		renderSingleGlow(glowTrans, yOff, --x, y);

	for (int i = 0; i < 2 * mGlowSize; i++)
		renderSingleGlow(glowTrans, yOff, x, --y);
}

void TextComponent::render(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	Transform4x4f trans = parentTrans * getTransform();

	if (!Renderer::isVisibleOnScreen(trans.translation().x(), trans.translation().y(), mSize.x(), mSize.y()))
		return;

	if (mRenderBackground)
	{
		Renderer::setMatrix(trans);

		auto bgColor = mBgColor & 0xFFFFFF00 | (unsigned char)((mBgColor & 0xFF) * (mOpacity / 255.0));
		Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), bgColor, bgColor);
	}

	if (mAutoScroll)
		Renderer::pushClipRect(Vector2i(trans.translation().x(), trans.translation().y()), Vector2i(mSize.x(), mSize.y()));

	if (mTextCache && mFont)
	{
		const Vector2f& textSize = mTextCache->metrics.size;
		float yOff = 0;
		switch(mVerticalAlignment)
		{
			case ALIGN_TOP:
				yOff = 0;
				break;
			case ALIGN_BOTTOM:
				yOff = (getSize().y() - textSize.y());
				break;
			case ALIGN_CENTER:
				yOff = (getSize().y() - textSize.y()) / 2.0f;
				break;
		}
		Vector3f off(mPadding.x(), mPadding.y() + yOff, 0);

		if(Settings::getInstance()->getBool("DebugText"))
		{
			// draw the "textbox" area, what we are aligned within
			Renderer::setMatrix(trans);
			Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), 0xFF000033, 0xFF000033);
		}

		if ((mGlowColor & 0x000000FF) != 0 && mGlowSize > 0)
		{
			renderGlow(parentTrans, yOff, -mMarqueeOffset);
			onColorChanged();
		}

		Transform4x4f drawTrans = trans;

		if (mMarqueeOffset != 0.0)
			trans.translate(off - Vector3f((float)mMarqueeOffset, 0, 0));
		else
			trans.translate(off);

//		trans.translate(off);
		Renderer::setMatrix(trans);

		// draw the text area, where the text actually is going
		if(Settings::getInstance()->getBool("DebugText"))
		{
			switch(mHorizontalAlignment)
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
		
		mFont->renderTextCache(mTextCache.get());

		// render currently selected item text again if
		// marquee is scrolled far enough for it to repeat
		
		if (mMarqueeOffset2 != 0.0)
		{
			trans = drawTrans;
			trans.translate(off - Vector3f((float)mMarqueeOffset2, 0, 0));

			if ((mGlowColor & 0x000000FF) != 0 && mGlowSize > 0)
			{
				renderGlow(parentTrans, yOff, -mMarqueeOffset2);
				onColorChanged();
			}

			Renderer::setMatrix(trans);
			mFont->renderTextCache(mTextCache.get());
			Renderer::setMatrix(drawTrans);
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

		if (mAutoScroll)
			Renderer::popClipRect();
	}
}

void TextComponent::calculateExtent()
{
	if(mAutoCalcExtent.x())
	{
		mSize = mFont->sizeText(mUppercase ? Utils::String::toUpper(mText) : mText, mLineSpacing);
	}else{
		if(mAutoCalcExtent.y())
		{
			mSize[1] = mFont->sizeWrappedText(mUppercase ? Utils::String::toUpper(mText) : mText, getSize().x(), mLineSpacing).y();
		}
	}
}

void TextComponent::onTextChanged()
{
	calculateExtent();

	if(!mFont || mText.empty())
	{
		mTextCache.reset();
		return;
	}

	int sx = mSize.x() - mPadding.x() - mPadding.z();
	int sy = mSize.y() - mPadding.y() - mPadding.w();

	std::string text = mUppercase ? Utils::String::toUpper(mText) : mText;

	std::shared_ptr<Font> f = mFont;
	const bool isMultiline = !mAutoScroll && (mSize.y() == 0 || sy > f->getHeight(mLineSpacing)*1.8f);

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
		if (sx && text.size() && (size.x() > sx || addAbbrev) && !mAutoScroll)
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

	int sy = mSize.y() - mPadding.y() - mPadding.w();
	const bool isMultiline = !mAutoScroll && (mSize.y() == 0 || sy > mFont->getHeight()*1.95f);

	if (mAutoScroll && !isMultiline && mSize.x() > 0)
	{
		// always reset the marquee offsets
		mMarqueeOffset = 0;
		mMarqueeOffset2 = 0;

		std::string text = mUppercase ? Utils::String::toUpper(mText) : mText;

		// if we're not scrolling and this object's text goes outside our size, marquee it!
		const float textLength = mFont->sizeText(text).x();
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
	else
	{
		mMarqueeTime = 0;
		mMarqueeOffset = 0;
		mMarqueeOffset2 = 0;
	}
}

void TextComponent::onColorChanged()
{
	if(mTextCache)
	{
		auto color = mColor & 0xFFFFFF00 | (unsigned char)((mColor & 0xFF) * (mOpacity / 255.0));
		mTextCache->setColor(color);
	}
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
			mAutoScroll = elem->get<bool>("singleLineScroll");
		else
			mAutoScroll = false;
	}

	setFont(Font::getFromTheme(elem, properties, mFont));
}

void TextComponent::setAutoScroll(bool value)
{
	if (mAutoScroll == value)
		return;

	mAutoScroll = value;
	onTextChanged();
}