#include "components/HelpComponent.h"

#include "components/ComponentGrid.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "resources/TextureResource.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include "Settings.h"
#include "InputConfig.h"

// space between [icon] and [text] (px)
#define ICON_TEXT_SPACING Renderer::getScreenWidth() * 0.0042f

// space between [text] and next [icon] (px)
#define ENTRY_SPACING (ICON_TEXT_SPACING * 2.0f)

static const std::map<std::string, const char*> ICON_PATH_MAP{
	{ "up/down", ":/help/dpad_updown.svg" },
	{ "left/right", ":/help/dpad_leftright.svg" },
	{ "up/down/left/right", ":/help/dpad_all.svg" },
	{ "a", ":/help/button_a.svg" },
	{ "b", ":/help/button_b.svg" },
	{ "x", ":/help/button_x.svg" },
	{ "y", ":/help/button_y.svg" },
	{ "l", ":/help/button_l.svg" },
	{ "r", ":/help/button_r.svg" },
	{ "lr", ":/help/button_lr.svg" },
	{ "start", ":/help/button_start.svg" },
	{ "select", ":/help/button_select.svg" },
	{ "F1", ":/help/F1.svg" }
};

HelpComponent::HelpComponent(Window* window) : GuiComponent(window)
{
	mHotItem = -1;
}

void HelpComponent::clearPrompts()
{
	mGrid.reset();
	mPrompts.clear();
	updateGrid();
}

void HelpComponent::setPrompts(const std::vector<HelpPrompt>& prompts)
{
	mGrid.reset();
	mPrompts = prompts;
	updateGrid();
}

void HelpComponent::setStyle(const HelpStyle& style)
{
	mGrid.reset();
	mStyle = style;
	updateGrid();
}

void HelpComponent::updateGrid()
{
	if (!Settings::getInstance()->getBool("ShowHelpPrompts") || mPrompts.empty())
	{
		mGrid.reset();
		return;
	}

	std::shared_ptr<Font>& font = mStyle.font;

	// [icon] [spacer1] [text] [spacer2]

	std::vector<std::pair<std::shared_ptr<ImageComponent>, std::shared_ptr<TextComponent>>> items;

	int maxWidth = Renderer::getScreenWidth() - ENTRY_SPACING;
	
	if (Settings::DrawClock())
	{
		std::string clockBuf;
		if (Settings::ClockMode12())
			clockBuf = "__00:00_AM_";
		else
			clockBuf = "__00:00_";

		auto size = mStyle.font->sizeText(clockBuf);
		maxWidth = Renderer::getScreenWidth() - size.x();
	}

	bool is43screen = Renderer::getScreenProportion() < 1.4;

	float width = 0;
	const float height = Math::round(font->getLetterHeight() * 1.25f);
	for (auto it = mPrompts.cbegin(); it != mPrompts.cend(); it++)
	{
		auto icon = std::make_shared<ImageComponent>(mWindow);
		icon->setIsLinear(true);

		auto label = InputConfig::buttonLabel(it->first);

		if (mStyle.iconMap.find(label) != mStyle.iconMap.end() && ResourceManager::getInstance()->fileExists(mStyle.iconMap[label]))
			icon->setImage(mStyle.iconMap[label]);
		else
			icon->setImage(getIconTexture(label.c_str()));

		icon->setColorShift(mStyle.iconColor);
		icon->setResize(0, height);		

		std::string text = Utils::String::toUpper(it->second);
		
		if (is43screen)
		{
			// Remove splitted help for 4:3 screens
			auto split = text.find("/");
			if (split != std::string::npos)
				text = text.substr(0, split);

			split = text.find("(");
			if (split != std::string::npos)
				text = text.substr(0, split);
		}

		auto lbl = std::make_shared<TextComponent>(mWindow, text, font, mStyle.textColor);
		
		width += icon->getSize().x() + lbl->getSize().x() + ICON_TEXT_SPACING + ENTRY_SPACING;
		if (width >= maxWidth)
			break;

		if (mStyle.glowSize)
		{
			lbl->setGlowSize(mStyle.glowSize);
			lbl->setGlowColor(mStyle.glowColor);
			lbl->setGlowOffset(mStyle.glowOffset.x(), mStyle.glowOffset.y());
		}

		items.push_back({ icon, lbl });
	}

	mGrid = std::make_shared<ComponentGrid>(mWindow, Vector2i(items.size() * 4, 1));
	mGrid->setSize(width, height);
	for (unsigned int i = 0; i < items.size(); i++)
	{
		auto item = items.at(i);

		const int col = i * 4;
		mGrid->setColWidthPerc(col, item.first->getSize().x() / width);
		mGrid->setColWidthPerc(col + 1, ICON_TEXT_SPACING / width);
		mGrid->setColWidthPerc(col + 2, item.second->getSize().x() / width);
		mGrid->setColWidthPerc(col + 3, ENTRY_SPACING / width);

		mGrid->setEntry(item.first, Vector2i(col, 0), false, false);
		mGrid->setEntry(item.second, Vector2i(col + 2, 0), false, false);
	}

	mGrid->setPosition(Vector3f(mStyle.position.x(), mStyle.position.y(), 0.0f));
	mGrid->setOrigin(mStyle.origin);
}

std::shared_ptr<TextureResource> HelpComponent::getIconTexture(const char* name)
{
	auto it = mIconCache.find(name);
	if (it != mIconCache.cend())
		return it->second;

	auto pathLookup = ICON_PATH_MAP.find(name);
	if (pathLookup == ICON_PATH_MAP.cend())
	{
		LOG(LogError) << "Unknown help icon \"" << name << "\"!";
		return nullptr;
	}

	if (!ResourceManager::getInstance()->fileExists(pathLookup->second))
	{
		LOG(LogError) << "Help icon \"" << name << "\" - corresponding image file \"" << pathLookup->second << "\" misisng!";
		return nullptr;
	}

	std::shared_ptr<TextureResource> tex = TextureResource::get(pathLookup->second);
	mIconCache[std::string(name)] = tex;
	return tex;
}

void HelpComponent::setOpacity(unsigned char opacity)
{
	GuiComponent::setOpacity(opacity);

	for (auto i = 0; i < mGrid->getChildCount(); i++)
		mGrid->getChild(i)->setOpacity(opacity);
}

void HelpComponent::render(const Transform4x4f& parentTrans)
{
	if (mGrid)
	{		
		// Optimize Help rendering by splitting texts & images rendering ( TextComponents use the same shaders programs ) // mGrid->render(trans);
		Transform4x4f trans = parentTrans * getTransform() * mGrid->getTransform();
		
		std::vector<GuiComponent*> textComponents;

		if (Settings::DebugMouse() && mHotItem >= 0)
		{
			auto rect = GetComponentScreenRect(trans, mGrid->getSize());

			Renderer::setMatrix(trans);

			int gx = 0;

			for (int i = 0; i < mGrid->getGridSize().x(); i += 4)
			{
				int width = mGrid->getColWidth(i) + mGrid->getColWidth(i + 1) + mGrid->getColWidth(i + 2);
				int spacing = mGrid->getColWidth(i + 3);

				rect.x = gx - spacing / 2.0f;
				rect.w = width + spacing / 2.0f;

				if (mHotItem == i / 4)
				{
					Renderer::drawRect(rect.x, 0.0f, rect.w, rect.h, 0xFFFFFF10, 0xFFFFFF10);
					break;
				}

				gx += width + spacing;
			}
		}

		for (auto i = 0; i < mGrid->getChildCount(); i++)
		{
			auto child = mGrid->getChild(i);

			if (!child->isKindOf<TextComponent>())
				child->render(trans);
			else
				textComponents.push_back(child);
		}

		for (auto child : textComponents)
			child->render(trans);
	}
}

bool HelpComponent::hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult)
{	
	bool ret = false;

	if (mGrid)
	{
		Transform4x4f trans = parentTransform * getTransform() * mGrid->getTransform();

		auto rect = Renderer::getScreenRect(trans, mGrid->getSize(), true);

		if (mStyle.font->getHeight(1.5f) > Renderer::getScreenHeight() - mGrid->getPosition().y())
			rect.h = Renderer::getScreenHeight() - rect.y;

		if (rect.x < 20)
		{
			rect.w += rect.x;
			rect.x = 0;
		}

		if (rect.contains(x, y))
		{
			mIsMouseOver = true;

			ret = true;

			if (pResult != nullptr)
				pResult->push_back(this);
		}
		else 
			mIsMouseOver = false;

		mHotItem = -1;

		for (int i = 0; i < mGrid->getGridSize().x(); i += 4)
		{			
			int width = mGrid->getColWidth(i) + mGrid->getColWidth(i + 1) + mGrid->getColWidth(i + 2) + mGrid->getColWidth(i + 3);

			auto rcItem = Renderer::getScreenRect(trans, Vector2f(width, rect.h), true);
			if (rcItem.contains(x, y))
			{
				mHotItem = i / 4;
				break;
			}

			trans.translate(width, 0);
		}
	}
	else
		mIsMouseOver = false;

	return ret;
}

bool HelpComponent::onMouseClick(int button, bool pressed, int x, int y)
{
	if (button == 1 && pressed && mHotItem >= 0 && mHotItem < mPrompts.size())
	{
		auto action = mPrompts[mHotItem].action;
		if (action != nullptr)
			action();

		return true;
	}

	return button == 1 && mIsMouseOver;
}