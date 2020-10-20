#include "components/ComponentTab.h"
#include "components/TextComponent.h"
#include "LocaleES.h"

#define PADDING_PX (Renderer::getScreenWidth() * 0.015f)
#define SELECTOR_PX (Renderer::getScreenWidth() * 0.0025f)

ComponentTab::ComponentTab(Window* window) : IList<ComponentTabItem, std::string>(window, LIST_SCROLL_STYLE_SLOW, LIST_NEVER_LOOP)
{
	mSelectorBarOffset = 0;
	mCameraOffset = 0;
	mFocused = false;
}

void ComponentTab::addTab(const std::string label, const std::string value, bool setCursorHere)
{
	auto theme = ThemeData::getMenuTheme();

	ComponentTabItem tab;
	auto grid = std::make_shared<TextComponent>(mWindow, label, theme->Text.font, theme->Text.color, ALIGN_CENTER);
	grid->setPadding(Vector4f(PADDING_PX, 0, PADDING_PX, 0));
	tab.addElement(grid, true);
	
	addTab(tab, value.empty() ? label : value, setCursorHere);
}

void ComponentTab::addTab(const ComponentTabItem& row, const std::string value, bool setCursorHere)
{
	IList<ComponentTabItem, std::string>::Entry e;
	e.name = "";
	e.object = value;
	e.data = row;

	this->add(e);

	for(auto it = mEntries.back().data.elements.cbegin(); it != mEntries.back().data.elements.cend(); it++)
		addChild(it->component.get());

	updateElementSize(mEntries.back().data);
	updateElementPosition(mEntries.back().data);

	if(setCursorHere)
	{
		mCursor = (int)mEntries.size() - 1;
		onCursorChanged(CURSOR_STOPPED);
	}
}

void ComponentTab::onSizeChanged()
{
	for(auto it = mEntries.cbegin(); it != mEntries.cend(); it++)
	{
		updateElementSize(it->data);
		updateElementPosition(it->data);
	}

	updateCameraOffset();
}

void ComponentTab::onFocusLost()
{
	mFocused = false;
}

void ComponentTab::onFocusGained()
{
	mFocused = true;
}

bool ComponentTab::input(InputConfig* config, Input input)
{
	if(size() == 0)
		return false;
	
	// input handler didn't consume the input - try to scroll
	if(config->isMappedLike("left", input))
		return listInput(input.value != 0 ? -1 : 0);
	else if(config->isMappedLike("right", input))
		return listInput(input.value != 0 ? 1 : 0);

	return false;
}

void ComponentTab::update(int deltaTime)
{
	listUpdate(deltaTime);

	if(size())
	{
		// update our currently selected row
		for(auto it = mEntries.at(mCursor).data.elements.cbegin(); it != mEntries.at(mCursor).data.elements.cend(); it++)
			it->component->update(deltaTime);
	}
}

void ComponentTab::onCursorChanged(const CursorState& state)
{
	// update the selector bar position
	// in the future this might be animated
	mSelectorBarOffset = 0;
	for(int i = 0; i < mCursor; i++)
		mSelectorBarOffset += getTabWidth(mEntries.at(i).data);

	updateCameraOffset();

	// this is terribly inefficient but we don't know what we came from so...
	if(size())
	{
		for(auto it = mEntries.cbegin(); it != mEntries.cend(); it++)
			it->data.elements.back().component->onFocusLost();

		mEntries.at(mCursor).data.elements.back().component->onFocusGained();
	}

	if(mCursorChangedCallback)
		mCursorChangedCallback(state);

	updateHelpPrompts();
}

void ComponentTab::updateCameraOffset()
{
	const float totalWidth = mSize.x();

	if (mCursor >= 0 && mCursor < mEntries.size())
	{
		int right = mSelectorBarOffset + getTabWidth(mEntries.at(mCursor).data);
		if (right > totalWidth)
			mCameraOffset = right - totalWidth;
		else 
			mCameraOffset = 0;
	}
	else 
		mCameraOffset = 0;
}

void ComponentTab::render(const Transform4x4f& parentTrans)
{
	if(!size())
		return;

	auto menuTheme = ThemeData::getMenuTheme();
	unsigned int selectorColor = menuTheme->Text.selectorColor;
	unsigned int selectorGradientColor = menuTheme->Text.selectorGradientColor;
	unsigned int selectedColor = menuTheme->Title.color;
	unsigned int bgColor = menuTheme->Background.color;
	unsigned int separatorColor = menuTheme->Text.separatorColor;
	unsigned int textColor = menuTheme->Text.color;
	bool selectorGradientHorz = menuTheme->Text.selectorGradientType;

	Transform4x4f trans = parentTrans * getTransform();

	// clip everything to be inside our bounds
	Vector3f dim(mSize.x(), mSize.y(), 0);
	dim = trans * dim - trans.translation();
	
	Renderer::pushClipRect(
		Vector2i((int)trans.translation().x(), (int)trans.translation().y()),
		Vector2i((int)Math::round(dim.x()), (int)Math::round(dim.y() + 1)));

	// scroll the camera
	trans.translate(Vector3f(-Math::round(mCameraOffset), 0, 0));

	// draw our entries
	std::vector<GuiComponent*> drawAfterCursor;
	
	for(unsigned int i = 0; i < mEntries.size(); i++)
	{
		auto& entry = mEntries.at(i);
		
		bool drawAll = (i != (unsigned int)mCursor);
		for(auto it = entry.data.elements.cbegin(); it != entry.data.elements.cend(); it++)
		{
			if(drawAll || it->invert_when_selected)
			{
				it->component->setColor(menuTheme->Text.color);
				it->component->render(trans);
			}
			else
				drawAfterCursor.push_back(it->component.get());			
		}
	}

	// custom rendering
	Renderer::setMatrix(trans);

	const float selectedTabWidth = getTabWidth(mEntries.at(mCursor).data);

	auto& entry = mEntries.at(mCursor);
		
	if ((selectorColor != bgColor) && ((selectorColor & 0xFF) != 0x00)) 
	{			
		if (mFocused)
		{				
			Renderer::drawRect(mSelectorBarOffset, 0.0f, selectedTabWidth, mSize.y(), bgColor, Renderer::Blend::ZERO, Renderer::Blend::ONE_MINUS_SRC_COLOR);
			Renderer::drawRect(mSelectorBarOffset, 0.0f, selectedTabWidth, mSize.y(), selectorColor, selectorGradientColor, selectorGradientHorz, Renderer::Blend::ONE, Renderer::Blend::ONE);
		}
		else
		{
			Renderer::drawRect(mSelectorBarOffset, mSize.y() - SELECTOR_PX, selectedTabWidth, SELECTOR_PX, bgColor, Renderer::Blend::ZERO, Renderer::Blend::ONE_MINUS_SRC_COLOR);
			Renderer::drawRect(mSelectorBarOffset, mSize.y() - SELECTOR_PX, selectedTabWidth, SELECTOR_PX, selectorColor, selectorGradientColor, selectorGradientHorz, Renderer::Blend::ONE, Renderer::Blend::ONE);
		}
	}

	for (auto& element : entry.data.elements)
	{
		element.component->setColor(selectedColor);
		drawAfterCursor.push_back(element.component.get());
	}

	for(auto it = drawAfterCursor.cbegin(); it != drawAfterCursor.cend(); it++)
		(*it)->render(trans);

	// reset matrix if one of these components changed it
	if(drawAfterCursor.size())
		Renderer::setMatrix(trans);

	// draw separators
	float x = 0;
	for(unsigned int i = 0; i < mEntries.size(); i++)
	{
		Renderer::drawRect(x, 0.0f, 1.0f, mSize.y(), separatorColor);
		x += getTabWidth(mEntries.at(i).data);
	}

	Renderer::drawRect(x , 0.0f, 1.0f, mSize.y(), separatorColor);
	Renderer::popClipRect();
}

float ComponentTab::getTabWidth(const ComponentTabItem& row) const
{
	float width = 0;

	for(unsigned int i = 0; i < row.elements.size(); i++)
		if(row.elements.at(i).component->getSize().x() > width)
			width = row.elements.at(i).component->getSize().x();

	return width;
}

float ComponentTab::getTotalTabWidth() const
{
	float width = 0;

	for(auto it = mEntries.cbegin(); it != mEntries.cend(); it++)
		width += getTabWidth(it->data);

	return width;
}

void ComponentTab::updateElementPosition(const ComponentTabItem& row)
{
	float xOffset = 0;

	for(auto it = mEntries.cbegin(); it != mEntries.cend() && &it->data != &row; it++)
		xOffset += getTabWidth(it->data);
	
	// assumes updateElementSize has already been called
	float colWidth = getTabWidth(row);

	float y = 0.0f;
	for(unsigned int i = 0; i < row.elements.size(); i++)
	{
		const auto comp = row.elements.at(i).component;
		
		// center vertically
		comp->setPosition((colWidth - comp->getSize().x()) / 2 + xOffset, y);
		y += comp->getSize().y();
	}
}

void ComponentTab::updateElementSize(const ComponentTabItem& row)
{
	float height = mSize.y();
	std::vector< std::shared_ptr<GuiComponent> > resizeVec;

	for(auto it = row.elements.cbegin(); it != row.elements.cend(); it++)
	{
		if(it->resize_height)
			resizeVec.push_back(it->component);
		else
			height -= it->component->getSize().y();
	}

	// redistribute the "unused" width equally among the components with resize_width set to true
	height = height / resizeVec.size();
	for(auto item : resizeVec)
		item->setSize(item->getSize().x() , height); // + 2 * PADDING_PX
}

void ComponentTab::textInput(const char* text)
{
	if(!size())
		return;

	mEntries.at(mCursor).data.elements.back().component->textInput(text);
}

std::vector<HelpPrompt> ComponentTab::getHelpPrompts()
{
	if(!size())
		return std::vector<HelpPrompt>();

	std::vector<HelpPrompt> prompts = mEntries.at(mCursor).data.elements.back().component->getHelpPrompts();

	if(size() > 1)
	{
		bool addMovePrompt = true;
		for(auto it = prompts.cbegin(); it != prompts.cend(); it++)
		{
			if(it->first == "up/down" || it->first == "up/down/left/right")
			{
				addMovePrompt = false;
				break;
			}
		}

		if(addMovePrompt)
			prompts.push_back(HelpPrompt(_("up/down"), _("CHOOSE"))); // batocera
	}

	return prompts;
}

bool ComponentTab::moveCursor(int amt)
{
	bool ret = listInput(amt);
	listInput(0);
	return ret;
}
