#include "components/ComponentList.h"
#include "components/TextComponent.h"
#include "LocaleES.h"

#define TOTAL_HORIZONTAL_PADDING_PX 20

ComponentList::ComponentList(Window* window) : IList<ComponentListRow, std::string>(window, LIST_SCROLL_STYLE_SLOW, LIST_NEVER_LOOP), mScrollbar(window)
{
	mSelectorBarOffset = 0;
	mCameraOffset = 0;
	mFocused = false;

	mScrollbar.loadFromMenuTheme();	
}

void ComponentList::addRow(const ComponentListRow& row, bool setCursorHere, bool updateSize, const std::string userData)
{
	IList<ComponentListRow, std::string>::Entry e;
	e.name = "";
	e.object = userData;
	e.data = row;

	this->add(e);

	ComponentListRow& data = mEntries.back().data;

	for(auto it = data.elements.cbegin(); it != data.elements.cend(); it++)
		addChild(it->component.get());

	if (updateSize)
	{
		updateElementSize(data);
		updateElementPosition(data);
	}

	// Fix group initial cursor position
	if (mCursor == 0 && mEntries.size() == 2 && !mEntries[0].data.selectable)
		setCursorHere = true;

	if(setCursorHere)
	{
		mCursor = (int)mEntries.size() - 1;
		onCursorChanged(CURSOR_STOPPED);
	}
}

void ComponentList::addGroup(const std::string& label, bool forceVisible)
{	
	auto theme = ThemeData::getMenuTheme();
	if (!forceVisible && !theme->Group.visible)
		return;

	ComponentListRow row;

	auto group = std::make_shared<TextComponent>(mWindow, label, theme->Group.font, theme->Group.color);

	if (EsLocale::isRTL() && ((Alignment)theme->Group.alignment) == Alignment::ALIGN_LEFT)
		group->setHorizontalAlignment(Alignment::ALIGN_RIGHT);
	else
		group->setHorizontalAlignment((Alignment) theme->Group.alignment);

	group->setBackgroundColor(theme->Group.backgroundColor); // 0x00000010	
	if (theme->Group.backgroundColor != 0)
		group->setRenderBackground(true);

	group->setLineSpacing(theme->Group.lineSpacing);
	group->setPadding(Vector4f(TOTAL_HORIZONTAL_PADDING_PX / 2, 0, TOTAL_HORIZONTAL_PADDING_PX / 2, 0));

	row.addElement(group, true, false);
	row.selectable = false;
	addRow(row);
}

void ComponentList::onSizeChanged()
{
	float yOffset = 0;
	for(auto it = mEntries.cbegin(); it != mEntries.cend(); it++)
	{
		updateElementSize(it->data);
		updateElementPosition(it->data, yOffset);
		yOffset += getRowHeight(it->data);
	}

	updateCameraOffset();
}

void ComponentList::onFocusLost()
{
	mFocused = false;
}

void ComponentList::onFocusGained()
{
	mFocused = true;
}

bool ComponentList::input(InputConfig* config, Input input)
{
	if(size() == 0)
		return false;

	// give it to the current row's input handler
	if(mEntries.at(mCursor).data.input_handler)
	{
		if(mEntries.at(mCursor).data.input_handler(config, input))
			return true;
	}else{
		// no input handler assigned, do the default, which is to give it to the rightmost element in the row
		auto& row = mEntries.at(mCursor).data;
		if(row.elements.size())
		{
			if (EsLocale::isRTL())
			{
				if (row.elements.front().component->input(config, input))
					return true;
			}
			else
			{
				if (row.elements.back().component->input(config, input))
					return true;
			}
		}
	}

	// input handler didn't consume the input - try to scroll
	if(config->isMappedLike("up", input))
	{
		return listInput(input.value != 0 ? -1 : 0);
	}else if(config->isMappedLike("down", input))
	{
		return listInput(input.value != 0 ? 1 : 0);

#ifdef _ENABLEEMUELEC
	}else if(config->isMappedTo("lefttrigger", input))
	{
		return listInput(input.value != 0 ? -6 : 0);
	}else if(config->isMappedTo("righttrigger", input)){
		return listInput(input.value != 0 ? 6 : 0);
	}
#else
	}else if(config->isMappedTo("pageup", input))
	{
		return listInput(input.value != 0 ? -6 : 0);
	}else if(config->isMappedTo("pagedown", input)){
		return listInput(input.value != 0 ? 6 : 0);
	}
#endif
	return false;
}

void ComponentList::update(int deltaTime)
{
	mScrollbar.update(deltaTime);

	listUpdate(deltaTime);

	if(size())
	{
		// update our currently selected row
		for(auto it = mEntries.at(mCursor).data.elements.cbegin(); it != mEntries.at(mCursor).data.elements.cend(); it++)
			it->component->update(deltaTime);
	}
}

void ComponentList::onCursorChanged(const CursorState& state)
{
	mScrollbar.onCursorChanged();

	// update the selector bar position
	// in the future this might be animated
	mSelectorBarOffset = 0;
	for(int i = 0; i < mCursor; i++)
	{
		mSelectorBarOffset += getRowHeight(mEntries.at(i).data);
	}

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

void ComponentList::updateCameraOffset()
{
	// move the camera to scroll
	const float totalHeight = getTotalRowHeight();
	if(totalHeight > mSize.y() && mCursor < mEntries.size())
	{
		float target = mSelectorBarOffset + getRowHeight(mEntries.at(mCursor).data)/2 - (mSize.y() / 2);

		// clamp it
		mCameraOffset = 0;
		unsigned int i = 0;
		while(mCameraOffset < target && i < mEntries.size())
		{
			mCameraOffset += getRowHeight(mEntries.at(i).data);
			i++;
		}

		if(mCameraOffset < 0)
			mCameraOffset = 0;
		else if(mCameraOffset + mSize.y() > totalHeight)
			mCameraOffset = totalHeight - mSize.y();
	}
	else
		mCameraOffset = 0;
}

void ComponentList::render(const Transform4x4f& parentTrans)
{
	if(!size())
		return;

	auto menuTheme = ThemeData::getMenuTheme();
	unsigned int selectorColor = menuTheme->Text.selectorColor;
	unsigned int selectorGradientColor = menuTheme->Text.selectorGradientColor;
	unsigned int selectedColor = menuTheme->Text.selectedColor;
	unsigned int bgColor = menuTheme->Background.color;
	unsigned int separatorColor = menuTheme->Text.separatorColor;
	unsigned int textColor = menuTheme->Text.color;
	bool selectorGradientHorz = menuTheme->Text.selectorGradientType;

	Transform4x4f trans = parentTrans * getTransform();

	// clip everything to be inside our bounds
	Vector3f dim(mSize.x(), mSize.y(), 0);
	dim = trans * dim - trans.translation();
	Renderer::pushClipRect(Vector2i((int)trans.translation().x(), (int)trans.translation().y()),
		Vector2i((int)Math::round(dim.x()), (int)Math::round(dim.y() + 1)));

	// scroll the camera
	trans.translate(Vector3f(0, -Math::round(mCameraOffset), 0));

	// draw our entries
	std::vector<GuiComponent*> drawAfterCursor;
	bool drawAll;
	for(unsigned int i = 0; i < mEntries.size(); i++)
	{
		auto& entry = mEntries.at(i);
		
		drawAll = !mFocused || i != (unsigned int)mCursor;
		for(auto it = entry.data.elements.cbegin(); it != entry.data.elements.cend(); it++)
		{
			if(drawAll || it->invert_when_selected)
			{
				if (entry.data.selectable)
					it->component->setColor(textColor);				
				else 
					it->component->setColor(menuTheme->Group.color);

				it->component->render(trans);
			}
			else
				drawAfterCursor.push_back(it->component.get());			
		}
	}

	// custom rendering
	Renderer::setMatrix(trans);

	// draw selector bar
	if(mFocused)
	{
		// inversion: src * (1 - dst) + dst * 0 = where src = 1
		// need a function that goes roughly 0x777777 -> 0xFFFFFF
		// and 0xFFFFFF -> 0x777777
		// (1 - dst) + 0x77

		const float selectedRowHeight = getRowHeight(mEntries.at(mCursor).data);

		auto& entry = mEntries.at(mCursor);
		
		if (entry.data.selectable)
		{
			if ((selectorColor != bgColor) && ((selectorColor & 0xFF) != 0x00)) {

				Renderer::drawRect(0.0f, mSelectorBarOffset, mSize.x(), selectedRowHeight, bgColor, Renderer::Blend::ZERO, Renderer::Blend::ONE_MINUS_SRC_COLOR);
				Renderer::drawRect(0.0f, mSelectorBarOffset, mSize.x(), selectedRowHeight, selectorColor, selectorGradientColor, selectorGradientHorz, Renderer::Blend::ONE, Renderer::Blend::ONE);
			}

			for (auto& element : entry.data.elements)
			{
				element.component->setColor(selectedColor);
				drawAfterCursor.push_back(element.component.get());
			}
		}		

		for(auto it = drawAfterCursor.cbegin(); it != drawAfterCursor.cend(); it++)
			(*it)->render(trans);

		// reset matrix if one of these components changed it
		if(drawAfterCursor.size())
			Renderer::setMatrix(trans);
	}

	// draw separators

	bool prevIsGroup = false;

	float y = 0;
	for(unsigned int i = 0; i < mEntries.size(); i++)
	{
		if (prevIsGroup && menuTheme->Group.separatorColor != separatorColor)
			Renderer::drawRect(0.0f, y - 2.0f, mSize.x(), 1.0f, menuTheme->Group.separatorColor);
		else
			Renderer::drawRect(0.0f, y, mSize.x(), 1.0f, separatorColor);

		y += getRowHeight(mEntries.at(i).data);

		prevIsGroup = !mEntries.at(i).data.selectable;
	}

	Renderer::drawRect(0.0f, y, mSize.x(), 1.0f, separatorColor);

	Renderer::popClipRect();

	if (mScrollbar.isEnabled() && mEntries.size() > 0)
	{
		mScrollbar.setContainerBounds(getPosition(), getSize());
		mScrollbar.setRange(0, getTotalRowHeight(), mSize.y());
		mScrollbar.setScrollPosition(mCameraOffset);
		mScrollbar.render(parentTrans);
	}
}

float ComponentList::getRowHeight(const ComponentListRow& row) const
{
	int sz = row.elements.size();
	if (sz == 0)
		return 0;
	else if (sz == 1)
		return row.elements[0].component->getSize().y();

	// returns the highest component height found in the row
	float height = 0;

	for(auto& elem : row.elements)
	{
		float h = elem.component->getSize().y();
		if (h > height)
			height = h;
	}

	return height;
}

float ComponentList::getTotalRowHeight() const
{
	float height = 0;
	for(auto it = mEntries.cbegin(); it != mEntries.cend(); it++)
		height += getRowHeight(it->data);

	return height;
}

void ComponentList::updateElementPosition(const ComponentListRow& row, float yOffset)
{
	if (yOffset < 0)
	{
		yOffset = 0;
		for (auto it = mEntries.cbegin(); it != mEntries.cend() && &it->data != &row; it++)
			yOffset += getRowHeight(it->data);
	}

	// assumes updateElementSize has already been called
	float rowHeight = getRowHeight(row);

	float x = row.selectable ? TOTAL_HORIZONTAL_PADDING_PX / 2 : 0;
	for(unsigned int i = 0; i < row.elements.size(); i++)
	{
		const auto comp = row.elements.at(i).component;
		
		// center vertically
		comp->setPosition(x, (rowHeight - comp->getSize().y()) / 2 + yOffset);
		x += comp->getSize().x();
	}
}

void ComponentList::updateElementSize(const ComponentListRow& row)
{
	float width = mSize.x() - (row.selectable ? TOTAL_HORIZONTAL_PADDING_PX : 0);
	std::vector< std::shared_ptr<GuiComponent> > resizeVec;

	for(auto it = row.elements.cbegin(); it != row.elements.cend(); it++)
	{
		if(it->resize_width)
			resizeVec.push_back(it->component);
		else
			width -= it->component->getSize().x();
	}

	// redistribute the "unused" width equally among the components with resize_width set to true
	width = width / resizeVec.size();
	for(auto it = resizeVec.cbegin(); it != resizeVec.cend(); it++)
	{
		(*it)->setSize(width, (*it)->getSize().y());
	}
}

void ComponentList::textInput(const char* text)
{
	if(!size())
		return;

	mEntries.at(mCursor).data.elements.back().component->textInput(text);
}

std::vector<HelpPrompt> ComponentList::getHelpPrompts()
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

bool ComponentList::moveCursor(int amt)
{
	bool ret = listInput(amt);
	listInput(0);
	return ret;
}
