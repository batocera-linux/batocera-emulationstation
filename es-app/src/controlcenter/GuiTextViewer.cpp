#include "controlcenter/GuiTextViewer.h"

#include "renderers/Renderer.h"
#include "resources/Font.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "InputConfig.h"
#include "LocaleES.h"
#include "ThemeData.h"
#include "Window.h"

#include <cmath>
#include <fstream>

// Math::clamp only applies the lower bound
static float clampf(float v, float lo, float hi)
{
	return v < lo ? lo : v > hi ? hi : v;
}



static const size_t MAX_TEXT_BYTES = 512 * 1024;
static const char* MONO_FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";

GuiTextViewer::GuiTextViewer(Window* window, const std::string& title, const std::string& path)
	: GuiComponent(window), mTitle(title), mScroll(0), mRepeatDir(0), mRepeatTime(0), mDragging(false), mDownY(0), mDownScroll(0)
{
	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	auto menuTheme = ThemeData::getMenuTheme();
	mTitleFont = menuTheme->Title.font;
	mFontPath = Utils::FileSystem::exists(MONO_FONT) ? MONO_FONT : menuTheme->Text.font->getPath();

	mPad = mSize.y() * 0.03f;
	mTitleH = mTitleFont->getHeight(2.0f);

	std::string text;
	std::ifstream file(path, std::ios::binary);
	if (file)
	{
		text.resize(MAX_TEXT_BYTES);
		file.read(&text[0], MAX_TEXT_BYTES);
		text.resize((size_t)file.gcount());
	}

	text = Utils::String::replace(text, "\r", "");
	text = Utils::String::replace(text, "\t", "    ");
	mParagraphs = Utils::String::split(text, '\n', false);

	setFontSize((int)(menuTheme->Text.font->getSize() * 0.8f));
}

GuiTextViewer::~GuiTextViewer()
{
}

void GuiTextViewer::setFontSize(int size)
{
	mFontSize = clampf(size, (int)(mSize.y() * 0.015f), (int)(mSize.y() * 0.08f));
	mFont = Font::get(mFontSize, mFontPath);
	mCache.clear();

	float width = mSize.x() - 3 * mPad;
	mLines.clear();
	for (auto& paragraph : mParagraphs)
	{
		if (paragraph.empty())
		{
			mLines.push_back("");
			continue;
		}

		for (auto& line : Utils::String::split(mFont->wrapText(paragraph, width), '\n', false))
			mLines.push_back(line);
	}

	mScroll = clampf(mScroll, 0.0f, maxScroll());
}

float GuiTextViewer::maxScroll()
{
	float viewH = mSize.y() - mTitleH - 2 * mPad;
	return std::max(0.0f, mLines.size() * mFont->getHeight(1.2f) - viewH);
}

void GuiTextViewer::scrollBy(float delta)
{
	mScroll = clampf(mScroll + delta, 0.0f, maxScroll());
}

bool GuiTextViewer::input(InputConfig* config, Input input)
{
	bool vertical = config->isMappedLike("up", input) || config->isMappedLike("down", input);
	if (vertical && input.value == 0)
	{
		mRepeatDir = 0;
		return true;
	}

	if (input.value == 0)
		return true;

	float line = mFont->getHeight(1.2f);
	float page = mSize.y() - mTitleH - 2 * mPad - line;

	if (config->isMappedTo(BUTTON_BACK, input) || config->isMappedTo(BUTTON_OK, input))
		delete this;
	else if (config->isMappedLike("up", input) || config->isMappedLike("down", input))
	{
		mRepeatDir = config->isMappedLike("up", input) ? -1 : 1;
		mRepeatTime = -350;
		scrollBy(mRepeatDir * line);
	}
	else if (config->isMappedLike("left", input))
		scrollBy(-page);
	else if (config->isMappedLike("right", input))
		scrollBy(page);
	else if (config->isMappedTo("pageup", input))
		setFontSize(mFontSize - std::max(1, mFontSize / 8));
	else if (config->isMappedTo("pagedown", input))
		setFontSize(mFontSize + std::max(1, mFontSize / 8));

	return true;
}

void GuiTextViewer::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mRepeatDir == 0)
		return;

	mRepeatTime += deltaTime;
	while (mRepeatTime >= 40)
	{
		mRepeatTime -= 40;
		scrollBy(mRepeatDir * mFont->getHeight(1.2f));
	}
}

void GuiTextViewer::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();
	auto menuTheme = ThemeData::getMenuTheme();

	Renderer::setMatrix(trans);
	Renderer::drawRect(0, 0, mSize.x(), mSize.y(), (menuTheme->Background.color & 0xFFFFFF00) | 0xFF);

	auto title = std::unique_ptr<TextCache>(mTitleFont->buildTextCache(Utils::String::toUpper(mTitle), Vector2f(0, 0), menuTheme->Title.color, mSize.x() - 2 * mPad, ALIGN_CENTER, 1.0f));
	Transform4x4f t = trans;
	t.translate(Vector3f(mPad, (mTitleH - title->metrics.size.y()) / 2, 0));
	Renderer::setMatrix(t);
	mTitleFont->renderTextCache(title.get());

	float top = mTitleH + mPad;
	float viewH = mSize.y() - top - mPad;
	float lineH = mFont->getHeight(1.2f);

	Vector3f tr = trans.translation();
	Renderer::pushClipRect((int)tr.x(), (int)(tr.y() + top), (int)mSize.x(), (int)viewH);

	int first = std::max(0, (int)(mScroll / lineH));
	int last = std::min((int)mLines.size(), first + (int)(viewH / lineH) + 2);
	for (int i = first; i < last; i++)
	{
		auto it = mCache.find(i);
		if (it == mCache.cend())
			it = mCache.insert(std::make_pair(i, std::unique_ptr<TextCache>(mFont->buildTextCache(mLines[i], 0, 0, menuTheme->Text.color)))).first;

		Transform4x4f lt = trans;
		lt.translate(Vector3f(1.5f * mPad, std::round(top + i * lineH - mScroll), 0));
		Renderer::setMatrix(lt);
		mFont->renderTextCache(it->second.get());
	}

	Renderer::popClipRect();

	if (mCache.size() > 400)
		mCache.clear();

	float maxS = maxScroll();
	if (maxS > 0)
	{
		float barH = std::max(lineH, viewH * viewH / (viewH + maxS));
		float barY = top + (viewH - barH) * (mScroll / maxS);
		Renderer::setMatrix(trans);
		Renderer::drawRect(mSize.x() - 0.6f * mPad, barY, 0.3f * mPad, barH, (menuTheme->Text.color & 0xFFFFFF00) | 0x80);
	}
}

bool GuiTextViewer::hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult)
{
	if (pResult)
		pResult->push_back(this);

	return true;
}

bool GuiTextViewer::onMouseClick(int button, bool pressed, int x, int y)
{
	if (button != 1)
		return true;

	if (pressed)
	{
		mDragging = false;
		mDownY = (float)y;
		mDownScroll = mScroll;
		mWindow->setMouseCapture(this);
		return true;
	}

	mWindow->releaseMouseCapture();

	if (!mDragging)
		delete this;

	return true;
}

void GuiTextViewer::onMouseMove(int x, int y)
{
	if (!mWindow->hasMouseCapture(this))
		return;

	float d = y - mDownY;
	if (std::abs(d) > mPad * 0.5f)
		mDragging = true;

	if (mDragging)
		mScroll = clampf(mDownScroll - d, 0.0f, maxScroll());
}

bool GuiTextViewer::onMouseWheel(int delta)
{
	scrollBy(-delta * 3 * mFont->getHeight(1.2f));
	return true;
}

std::vector<HelpPrompt> GuiTextViewer::getHelpPrompts()
{
	return { HelpPrompt("up/down", _("SCROLL")), HelpPrompt("lr", _("ZOOM")), HelpPrompt(BUTTON_BACK, _("CLOSE")) };
}
