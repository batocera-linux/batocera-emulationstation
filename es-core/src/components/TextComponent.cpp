#include "components/TextComponent.h"

#include "utils/StringUtil.h"
#include "Log.h"
#include "Settings.h"

#define AUTO_SCROLL_RESET_DELAY 6000
#define AUTO_SCROLL_DELAY 6000 // ms to wait before we start to scroll
#define AUTO_SCROLL_SPEED 150 // ms between scrolls

TextComponent::TextComponent(Window* window) : GuiComponent(window), 
	mFont(Font::get(FONT_SIZE_MEDIUM)), mUppercase(false), mColor(0x000000FF), mAutoCalcExtent(true, true),
	mHorizontalAlignment(ALIGN_LEFT), mVerticalAlignment(ALIGN_CENTER), mLineSpacing(1.5f), mBgColor(0),
	mRenderBackground(false), mGlowColor(0), mGlowSize(2), mGlowOffset(Vector2f(0,0)), mBindingDefaults(false), mAutoScrollDelay(AUTO_SCROLL_DELAY), mAutoScrollSpeed(AUTO_SCROLL_SPEED),
	mReflection(0.0f, 0.0f), mReflectOnBorders(false), mAutoScroll(AutoScrollType::NONE), mTextLength(-1), mMultiline(MultiLineType::AUTO),
	mHasBonusColor(false), mBonusColor(0)
{	
	mMarqueeOffset = 0;
	mMarqueeOffset2 = 0;
	mMarqueeTime = 0;	
}

TextComponent::TextComponent(Window* window, const std::string& text, const std::shared_ptr<Font>& font, unsigned int color, Alignment align,
	Vector3f pos, Vector2f size, unsigned int bgcolor) : GuiComponent(window), 
	mFont(NULL), mUppercase(false), mColor(0x000000FF), mAutoCalcExtent(true, true),
	mHorizontalAlignment(align), mVerticalAlignment(ALIGN_CENTER), mLineSpacing(1.5f), mBgColor(0),
	mRenderBackground(false), mGlowColor(0), mGlowSize(2), mGlowOffset(Vector2f(0, 0)), mBindingDefaults(false), mAutoScrollDelay(AUTO_SCROLL_DELAY), mAutoScrollSpeed(AUTO_SCROLL_SPEED),
	mReflection(0.0f, 0.0f), mReflectOnBorders(false), mAutoScroll(AutoScrollType::NONE), mTextLength(-1), mMultiline(MultiLineType::AUTO),
	mHasBonusColor(false), mBonusColor(0)
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

void TextComponent::setBonusTextColor(unsigned int color) 
{ 
	if (mBonusColor == color)
		return;

	mBonusColor = color; 
	mHasBonusColor = true; 
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
void TextComponent::onOpacityChanged()
{
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
		mMarqueeTime = -mAutoScrollDelay + mAutoScrollSpeed;
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
	if (!mVisible || (mColor & 0xFF) == 0)
		return;

	Transform4x4f trans = parentTrans * getTransform();

	auto rect = Renderer::getScreenRect(trans, mSize);
	if (mRotation == 0 && trans.r0().y() == 0 && !Renderer::isVisibleOnScreen(rect))
		return;

	if (mRenderBackground && mBgColor != 0)
	{
		Renderer::setMatrix(trans);

		auto bgColor = mBgColor & 0xFFFFFF00 | (unsigned char)((mBgColor & 0xFF) * (getOpacity() / 255.0));
		Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), bgColor, bgColor);
	}

	if (mTextCache == nullptr && mFont != nullptr && !mText.empty())
	{
		buildTextCache();
		onColorChanged();
	}

	if (mTextCache == nullptr || mFont == nullptr)
	{
		GuiComponent::renderChildren(trans);
		return;
	}

	mTextCache->setCustomShader(nullptr);

	bool popClipRect = false;

	if (mRotation == 0 && trans.r0().y() == 0)
	{
		if (mExtraType == ExtraType::EXTRACHILDREN && mVerticalAlignment == Alignment::ALIGN_TOP && mMultiline == MultiLineType::MULTILINE && mAutoScroll == AutoScrollType::NONE)
		{
			// Clip multiline - non partial lines
			auto lineHeight = mFont->getHeight(mLineSpacing);
			if (lineHeight > 0)
			{
				auto lines = (int)(rect.h / lineHeight);
				rect.h = lineHeight * lines + 1;
			}

			Renderer::pushClipRect(rect);
			popClipRect = true;
		}
		else if (mAutoScroll != AutoScrollType::NONE || mExtraType == ExtraType::EXTRACHILDREN)
		{
			if (mAutoScroll == AutoScrollType::VERTICAL)
			{
				Renderer::ShaderInfo shader;
				shader.path = ":/shaders/vscrolleffect.glsl";
				shader.parameters["s_offset"] = std::to_string((float)mMarqueeOffset);
				shader.parameters["s_height"] = std::to_string(rect.h);
				shader.parameters["s_forcetop"] = "4";
				shader.parameters["s_forcebottom"] = "4";
				mTextCache->setCustomShader(&shader);

				// Clip multiline - non partial lines
				/*
				auto lineHeight = mFont->getHeight(mLineSpacing);
				if (lineHeight > 0)
				{
					auto nonPartialLines = (int)(rect.h / lineHeight);
					int decal = rect.h - (lineHeight * nonPartialLines);
					shader.parameters["s_forcebottom"] = std::to_string(Math::max(4, decal));
				}*/
			}

			Renderer::pushClipRect(rect);
			popClipRect = true;
		}
	}

	beginCustomClipRect();

	const Vector2f& textSize = mTextCache->metrics.size;

	float yOff = 0;
	switch (mVerticalAlignment)
	{
	case ALIGN_BOTTOM:
		yOff = (getSize().y() - textSize.y() - mPadding.w());
		break;
	case ALIGN_CENTER:
		yOff = (getSize().y() - textSize.y()) / 2.0f;
		break;
	}

	Vector3f off(mPadding.x(), mPadding.y() + yOff, 0);

	if (Settings::DebugText() || (Settings::DebugMouse() && mIsMouseOver))
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
		
	mFont->renderTextCacheEx(mTextCache.get(), trans, mGlowSize, mGlowColor, mGlowOffset, getOpacity());

	// render currently selected item text again if
	// marquee is scrolled far enough for it to repeat

	if (mMarqueeOffset2 != 0.0 && mAutoScroll != AutoScrollType::VERTICAL)
	{
		trans = drawTrans;
		trans.translate(off - Vector3f((float)mMarqueeOffset2, 0, 0));
		mFont->renderTextCacheEx(mTextCache.get(), trans, mGlowSize, mGlowColor, mGlowOffset, getOpacity());
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

		float baseOpacity = getOpacity() / 255.0;
		float alpha = baseOpacity * ((mColor & 0x000000ff)) / 255.0;
		float alpha2 = baseOpacity * alpha * mReflection.y();

		alpha *= mReflection.x();

		const unsigned int colorT = Renderer::convertColor((mColor & 0xffffff00) + (unsigned char)(255.0*alpha));
		const unsigned int colorB = Renderer::convertColor((mColor & 0xffffff00) + (unsigned char)(255.0*alpha2));

		mFont->renderGradientTextCache(mTextCache.get(), colorB, colorT);
	}

	GuiComponent::renderChildren(drawTrans);

	endCustomClipRect();

	if (popClipRect)
		Renderer::popClipRect();
}

void TextComponent::onTextChanged()
{
	mTextLength = -1;
	mTextCache = nullptr;

	if (mAutoCalcExtent.x())
	{
		auto text = mUppercase ? Utils::String::toUpper(mText) : mText;
		if (mMultiline == MultiLineType::SINGLELINE)
		{
			size_t newline = text.find('\n');
			if (newline != std::string::npos)
				text = text.substr(0, newline); // single line of text - stop at the first newline since it'll mess everything up
		}

		mSize = mFont->sizeText(text, mLineSpacing);
		mSize[0] += mPadding.x() + mPadding.z();
	}
	else if (mAutoCalcExtent.y())
	{
		if (mMultiline == MultiLineType::SINGLELINE)
			mSize[1] = mFont->getHeight() + mPadding.y() + mPadding.w();
		else
			mSize[1] = mFont->sizeWrappedText(mUppercase ? Utils::String::toUpper(mText) : mText, getSize().x(), mLineSpacing).y() + mPadding.y() + mPadding.w();
	}
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
	
	bool isMultiline = mMultiline == MultiLineType::MULTILINE;
	if (mMultiline == MultiLineType::AUTO)
		isMultiline = (mAutoScroll != AutoScrollType::HORIZONTAL) && (mSize.y() == 0 || sy > mFont->getHeight(mLineSpacing) * 1.8f);

	bool addAbbrev = false;
	if (!isMultiline)
	{
		size_t newline = text.find('\n');
		if (newline != std::string::npos)
			text = text.substr(0, newline); // single line of text - stop at the first newline since it'll mess everything up

		addAbbrev = newline != std::string::npos;
	}

	auto color = mColor & 0xFFFFFF00 | (unsigned char)((mColor & 0xFF) * (getOpacity() / 255.0));

	Vector2f size = f->sizeText(text);
	if (!isMultiline)
	{
		if (sx && text.size() && (size.x() > (sx + 1) || addAbbrev) && (mAutoScroll == AutoScrollType::NONE))
		{
			// abbreviate text
			const std::string abbrev = "...";
			Vector2f abbrevSize = f->sizeText(abbrev);

			std::string textCopy = text;

			size_t cursor = 0;
			while (cursor >= 0 && cursor < textCopy.size())
			{
				auto newCursor = Utils::String::nextCursor(textCopy, cursor);
				if (cursor == newCursor)
					continue;

				cursor = newCursor;
				
				std::string testText = textCopy.substr(0, cursor);
				size = f->sizeText(testText);

				if (size.x() + abbrevSize.x() > sx)
					break;

				text = testText;
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

	bool isMultiline = mMultiline == MultiLineType::MULTILINE;
	if (mMultiline == MultiLineType::AUTO)
		isMultiline = (mAutoScroll != AutoScrollType::HORIZONTAL) && (mSize.y() == 0 || sy > mFont->getHeight(mLineSpacing) * 1.8f); // 1.95f

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
			{
				mMarqueeOffset2 = (int)(mMarqueeOffset - (scrollLength + returnLength));
				if (mMarqueeOffset2 == 0)
					mMarqueeOffset = 0;
			}
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

			while (mMarqueeTime >= mAutoScrollSpeed)
			{
				mMarqueeOffset += 1;
				mMarqueeTime -= mAutoScrollSpeed;
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
					mMarqueeTime = -mAutoScrollDelay + mAutoScrollSpeed;
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

	if (mAutoScroll != AutoScrollType::NONE && !mText.empty())
		mMarqueeTime = -mAutoScrollDelay + mAutoScrollSpeed;
	else
		mMarqueeTime = 0;
}

void TextComponent::onColorChanged()
{
	if (!mTextCache)
		return;

	if (mHasBonusColor)
		mTextCache->setColors(mColor & 0xFFFFFF00 | (unsigned char)((mColor & 0xFF) * (getOpacity() / 255.0)), mBonusColor);
	else
		mTextCache->setColor(mColor & 0xFFFFFF00 | (unsigned char)((mColor & 0xFF) * (getOpacity() / 255.0)));
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

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, getThemeTypeName());
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

		if (elem->has("emptyTextDefaults"))
			mBindingDefaults = elem->get<bool>("emptyTextDefaults");
		else
			mBindingDefaults = mExtraType != ExtraType::EXTRACHILDREN;
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

		if (elem->has("extraTextColor"))
			setBonusTextColor(elem->get<unsigned int>("extraTextColor"));

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

		if (elem->has("multiLine"))
		{
			auto multiLine = elem->get<std::string>("multiLine");
			if (multiLine == "true")
				setMultiLine(MultiLineType::MULTILINE);
			else if (multiLine == "false")
				setMultiLine(MultiLineType::SINGLELINE);
			else 
				setMultiLine(MultiLineType::AUTO);
		}

		if (elem->has("autoScrollDelay"))
			mAutoScrollDelay = (int) Math::clamp(elem->get<float>("autoScrollDelay"), 0, 1000000);

		if (elem->has("autoScrollSpeed"))
			mAutoScrollSpeed = (int)Math::clamp(elem->get<float>("autoScrollSpeed"), 10, 1000000);

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

	if (mAutoScroll != AutoScrollType::NONE && !mText.empty())
		mMarqueeTime = -mAutoScrollDelay + mAutoScrollSpeed;
	else
		mMarqueeTime = 0;

	onSizeChanged();	
}

void TextComponent::setMultiLine(MultiLineType value)
{
	if (mMultiline == value)
		return;

	mMultiline = value;
	onTextChanged();
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
	else if (name == "fontPath")
		return mFont ? mFont->getPath() : std::string();
	else if (name == "fontSize")
		return mFont ? mFont->getSize() / ((float)Math::min(Renderer::getScreenHeight(), Renderer::getScreenWidth())) : 0.0f;
	else if (name == "value")
		return mText;
	else if (name == "autoScroll")
	{
		if (getAutoScroll() == AutoScrollType::HORIZONTAL)
			return "horizontal";
		if (getAutoScroll() == AutoScrollType::VERTICAL)
			return "vertical";
		
		return std::string();
	}
	else if (name == "autoScrollDelay")
		return (unsigned int) mAutoScrollDelay;
	else if (name == "autoScrollSpeed")
		return (unsigned int)mAutoScrollSpeed;	
	else if (name == "singleLineScroll")
		return getAutoScroll() == AutoScrollType::HORIZONTAL;
	else if (name == "multiLine")
	{
		if (getMultiLine() == MultiLineType::MULTILINE)
			return "true";
		if (getMultiLine() == MultiLineType::SINGLELINE)
			return "false";
		
		return std::string();
	}

	return GuiComponent::getProperty(name);
}

void TextComponent::setProperty(const std::string name, const ThemeData::ThemeElement::Property& value)
{
	Vector2f scale = getParent() ? getParent()->getSize() : Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	
	if (value.type == ThemeData::ThemeElement::Property::PropertyType::Float && name == "fontSize")
		setFont("", value.f * (float)Math::min(Renderer::getScreenHeight(), Renderer::getScreenWidth()));
	if (value.type == ThemeData::ThemeElement::Property::PropertyType::String && name == "fontPath")
		setFont(value.s, 0);
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::String && name == "multiLine")
	{
		if (value.s == "true")
			setAutoScroll(MultiLineType::MULTILINE);
		else if (value.s == "false")
			setAutoScroll(MultiLineType::SINGLELINE);
		else
			setAutoScroll(MultiLineType::AUTO);
	}
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::String && name == "autoScroll")
	{
		if (value.s == "horizontal")
			setAutoScroll(AutoScrollType::HORIZONTAL);
		else if (value.s == "vertical")
			setAutoScroll(AutoScrollType::VERTICAL);
		else
			setAutoScroll(AutoScrollType::NONE);
	}
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Bool && name == "singleLineScroll")
		setAutoScroll(value.b);
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Int && name == "color")
		setColor(value.i);
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Int && name == "autoScrollDelay")
		mAutoScrollDelay = value.i;
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Int && name == "autoScrollSpeed")
		mAutoScrollSpeed = value.i;
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Int && name == "backgroundColor")
	{
		setBackgroundColor(value.i);
		setRenderBackground(value.i != 0);
	}
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Int && name == "glowColor")
		setGlowColor(value.i);
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Float && name == "glowSize")
		setGlowSize(value.f);
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Pair && name == "glowOffset")
		setGlowOffset(value.v.x(), value.v.y());
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Pair && name == "reflexion")
		mReflection = value.v;
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Float && name == "lineSpacing")
		setLineSpacing(value.f);
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::String && name == "text")
		setText(value.s);	
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::String && name == "value")
		setValue(value.s);
	else
		GuiComponent::setProperty(name, value);
}

void TextComponent::onPaddingChanged() 
{ 
	onTextChanged();
}