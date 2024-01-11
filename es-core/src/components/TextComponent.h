#pragma once
#ifndef ES_CORE_COMPONENTS_TEXT_COMPONENT_H
#define ES_CORE_COMPONENTS_TEXT_COMPONENT_H

#include "resources/Font.h"
#include "GuiComponent.h"

class ThemeData;


// Used to display text.
// TextComponent::setSize(x, y) works a little differently than most components:
//  * (0, 0)                     - will automatically calculate a size that fits the text on one line (expand horizontally)
//  * (x != 0, 0)                - wrap text so that it does not reach beyond x. Will automatically calculate a vertical size (expand vertically).
//  * (x != 0, y <= fontHeight)  - will truncate text so it fits within this box.
class TextComponent : public GuiComponent
{
public:
	TextComponent(Window* window);
	TextComponent(Window* window, const std::string& text, const std::shared_ptr<Font>& font, unsigned int color = 0x000000FF, Alignment align = ALIGN_LEFT,
		Vector3f pos = Vector3f::Zero(), Vector2f size = Vector2f::Zero(), unsigned int bgcolor = 0x00000000);

	std::string getThemeTypeName() override { return "text"; }

	void setFont(const std::shared_ptr<Font>& font);
	void setFont(std::string path, int size);
	void setUppercase(bool uppercase);
	void onSizeChanged() override;
	const std::string getText() { return mText; }
	void setText(const std::string& text);
	void setColor(unsigned int color);
	void setHorizontalAlignment(Alignment align);
	void setVerticalAlignment(Alignment align);
	void setLineSpacing(float spacing);
	void setBackgroundColor(unsigned int color);
	void setRenderBackground(bool render);

	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;

	std::string getValue() const override;
	void setValue(const std::string& value) override;
	
	void onOpacityChanged() override;
	void onPaddingChanged() override;

	inline std::shared_ptr<Font> getFont() const { return mFont; }

	virtual void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

	void setGlowColor(unsigned int color) { mGlowColor = color; };
	void setGlowSize(unsigned int size) { mGlowSize = size; };
	void setGlowOffset(float x, float y) { mGlowOffset = Vector2f(x,y); };

	enum AutoScrollType : unsigned int
	{
		NONE = 0,
		HORIZONTAL = 1,
		VERTICAL = 2
	};

	AutoScrollType getAutoScroll() { return mAutoScroll; }
	void setAutoScroll(AutoScrollType value);
	void setAutoScroll(bool scroll);

	unsigned int getColor() { return mColor; }

	std::string getOriginalThemeText() { return mSourceText; }

	ThemeData::ThemeElement::Property getProperty(const std::string name) override;
	void setProperty(const std::string name, const ThemeData::ThemeElement::Property& value) override;

	virtual void onShow() override;

	enum MultiLineType : unsigned int
	{
		AUTO = 0,
		MULTILINE = 1,
		SINGLELINE = 2
	};

	MultiLineType getMultiLine() { return mMultiline; }
	void setMultiLine(MultiLineType value);

	bool getBindingDefaults() { return mBindingDefaults; }

	void setBonusTextColor(unsigned int color);

protected:
	void buildTextCache();
	virtual void onTextChanged();

	std::string mText;
	std::shared_ptr<Font> mFont;
	std::string mSourceText;

	bool		mBindingDefaults;

	Vector2i mAutoCalcExtent;

private:	
	void onColorChanged();

	unsigned int mColor;
	unsigned int mBgColor;

	bool mRenderBackground;
	
	bool mUppercase;
	
	std::shared_ptr<TextCache> mTextCache;
	Alignment mHorizontalAlignment;
	Alignment mVerticalAlignment;
	float mLineSpacing;

	unsigned int mGlowColor;
	unsigned int mGlowSize;
	Vector2f	 mGlowOffset;
	
	Vector2f	mReflection;
	bool		mReflectOnBorders;

	int mMarqueeOffset;
	int mMarqueeOffset2;
	int mMarqueeTime;

	int mTextLength;

	int mAutoScrollDelay;
	int mAutoScrollSpeed;

	unsigned int mBonusColor;
	bool mHasBonusColor;

	AutoScrollType mAutoScroll;
	MultiLineType  mMultiline;
};

#endif // ES_CORE_COMPONENTS_TEXT_COMPONENT_H
