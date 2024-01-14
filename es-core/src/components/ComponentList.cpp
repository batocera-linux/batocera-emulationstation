#include "components/ComponentList.h"
#include "LocaleES.h"
#include "TextToSpeech.h"

#include "components/TextComponent.h"
#include "components/SwitchComponent.h"
#include "components/SliderComponent.h"
#include "components/OptionListComponent.h"
#include "InputManager.h"

#define TOTAL_HORIZONTAL_PADDING_PX 20

ComponentList::ComponentList(Window* window) : IList<ComponentListRow, std::string>(window, LIST_SCROLL_STYLE_SLOW, LIST_NEVER_LOOP), mScrollbar(window)
{
	mHotRow = -1;
	mPressedRow = -1;
	mPressedPoint = Vector2i(-1, -1);
	mIsDragging = false;

	mSelectorBarOffset = 0;
	mCameraOffset = 0;
	mFocused = false;
	mOldCursor = -1; 

	mScrollbar.loadFromMenuTheme();	
}

void ComponentList::addRow(const ComponentListRow& row, bool setCursorHere, bool updateSize, const std::string& userData)
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
	else if (mFocused && mCursor == 0 && mEntries.size() == 1)
		onCursorChanged(CURSOR_STOPPED);
}

void ComponentList::removeLastRowIfGroup()
{
	int index = mEntries.size() - 1;
	if (index < 0)
		return;

	if (mEntries.at(index).data.group)
		mEntries.erase(mEntries.begin() + index);
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

	row.addElement(group, true);
	row.selectable = false;
	row.group = true;
	addRow(row);
}

void ComponentList::onSizeChanged()
{
	IList::onSizeChanged();

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

	if (size())
	{
		for (auto it = mEntries.cbegin(); it != mEntries.cend(); it++)
			for (auto elt : it->data.elements)
				elt.component->onFocusLost();
	}
}

void ComponentList::onFocusGained()
{
	mFocused = true;

	if (size())
	{
		for (auto it = mEntries.cbegin(); it != mEntries.cend(); it++)
			for (auto elt : it->data.elements)
				elt.component->onFocusLost();

		for (auto elt : mEntries.at(mCursor).data.elements)
			elt.component->onFocusGained();
	}
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

	}else if(config->isMappedTo("pageup", input))
	{
		return listInput(input.value != 0 ? -6 : 0);
	}else if(config->isMappedTo("pagedown", input)){
		return listInput(input.value != 0 ? 6 : 0);
	}

	return false;
}

void ComponentList::update(int deltaTime)
{
	mScrollbar.update(deltaTime);

	listUpdate(deltaTime);

	if(size())
	{
		if (mUpdateType == ComponentListFlags::UpdateType::UPDATE_ALWAYS)
		{
			for (auto& entry : mEntries)
				for (auto it = entry.data.elements.cbegin(); it != entry.data.elements.cend(); it++)
					it->component->update(deltaTime);
		}
		else if (mUpdateType == ComponentListFlags::UpdateType::UPDATE_WHEN_SELECTED)
		{
			// update our currently selected row
			for (auto it = mEntries.at(mCursor).data.elements.cbegin(); it != mEntries.at(mCursor).data.elements.cend(); it++)
				it->component->update(deltaTime);
		}
	}
}

void ComponentList::onCursorChanged(const CursorState& state)
{
	mScrollbar.onCursorChanged();

	// update the selector bar position
	// in the future this might be animated
	mSelectorBarOffset = 0;
	for (int i = 0; i < mCursor; i++)
		mSelectorBarOffset += getRowHeight(mEntries.at(i).data);

	updateCameraOffset();

	// this is terribly inefficient but we don't know what we came from so...
	if (mFocused && mCursor >= 0 && mCursor < size())
	{
		for (auto it = mEntries.cbegin(); it != mEntries.cend(); it++)
			for (auto elt : it->data.elements)
				elt.component->onFocusLost();

		for (auto elt : mEntries.at(mCursor).data.elements)
			elt.component->onFocusGained();
	}

	if (mCursorChangedCallback)
		mCursorChangedCallback(state);

	updateHelpPrompts();

	// tts
	if (state == CURSOR_STOPPED && mOldCursor != mCursor)
		saySelectedLine();
}

void ComponentList::saySelectedLine()
{
	int n = 0;

	if (!(mCursor >= 0 && mCursor < mEntries.size())) 
		return;

	mOldCursor = mCursor;
	for (auto& element : mEntries.at(mCursor).data.elements)
	{
		if (element.component->isKindOf<TextComponent>())
		{
			TextToSpeech::getInstance()->say(element.component->getValue(), n > 0);
			n++;
		}
		if (element.component->isKindOf<SliderComponent>())
		{
			SliderComponent* slider = dynamic_cast<SliderComponent*>(element.component.get());
			if (slider == nullptr)
				continue;

			float v = slider->getValue();
			char strval[32];
			snprintf(strval, 32, "%.0f", v);
			TextToSpeech::getInstance()->say(std::string(strval) + _(slider->getSuffix().c_str()), n > 0);
			n++;
		}
		if (element.component->isKindOf<SwitchComponent>())
		{
			TextToSpeech::getInstance()->say(((SwitchComponent*)element.component.get())->getState() ? _("ENABLED") : _("DISABLED"), n > 0);
			n++;
		}
		if (element.component->isKindOf<OptionListComponent<std::string>>())
		{
			OptionListComponent<std::string>* optionList = dynamic_cast<OptionListComponent<std::string>*>(element.component.get());
			if (optionList == nullptr)
				continue;

			if (optionList->IsMultiSelect())
				TextToSpeech::getInstance()->say(Utils::String::join(optionList->getSelectedObjects(), ", "));
			else
				TextToSpeech::getInstance()->say(optionList->getSelectedName(), n > 0);

			n++;
		}
	}
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
	if (!size())
		return;

	float opacity = getOpacity() / 255.0;
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

	Renderer::pushClipRect(trans.translation().x(), trans.translation().y(), Math::round(dim.x()), Math::round(dim.y() + 1));

	// scroll the camera
	trans.translate(Vector3f(0, -Math::round(mCameraOffset), 0));

	float y = 0;

	std::map<unsigned int, float> rowHeights;
	std::vector<GuiComponent*> drawAfterCursor;

	for (unsigned int i = 0; i < mEntries.size(); i++)
	{
		auto& entry = mEntries.at(i);

		float rowHeight = getRowHeight(entry.data);
		rowHeights[i] = rowHeight;

		if (y - mCameraOffset + rowHeight >= 0)
		{
			if (mFocused && entry.data.selectable && i == mCursor)
			{
				Renderer::setMatrix(trans);

				if (selectorColor != bgColor && (selectorColor & 0xFF) != 0)
				{
					Renderer::setMatrix(trans);
					Renderer::drawRect(0.0f, mSelectorBarOffset, mSize.x(), rowHeight,
						selectorColor & 0xFFFFFF00 | (unsigned char)((selectorColor & 0xFF) * opacity),
						selectorGradientColor & 0xFFFFFF00 | (unsigned char)((selectorGradientColor & 0xFF) * opacity),
						selectorGradientHorz);
				}
			}

			if (i == mHotRow)
			{
				Renderer::setMatrix(trans);

				float hotOpacity = 0.1f;
				Renderer::drawRect(0.0f, y, mSize.x(), rowHeight,
					selectorColor & 0xFFFFFF00 | (unsigned char)((selectorColor & 0xFF) * opacity * hotOpacity),
					selectorGradientColor & 0xFFFFFF00 | (unsigned char)((selectorGradientColor & 0xFF) * opacity * hotOpacity),
					selectorGradientHorz);

			//	Renderer::drawRect(0.0f, y, mSize.x(), rowHeight, 0x00000010, 0x00000010);
			}

			for (auto& element : entry.data.elements)
			{				
				if (Settings::DebugMouse() && i == mHotRow)
					element.component->setColor(0xFFFF00FF);
				else 
				if (entry.data.group)
					element.component->setColor(menuTheme->Group.color);
				else
				{
					if (entry.data.selectable && mFocused && i == mCursor)
						element.component->setColor(selectedColor);
					else
						element.component->setColor(textColor);
				}
					
				if (element.component->isKindOf<TextComponent>())
					drawAfterCursor.push_back(element.component.get());
				else
					element.component->render(trans);
			}
		}
		
		y += rowHeight;
		if (y - mCameraOffset > mSize.y())
			break;
	}

	for (auto it : drawAfterCursor)
		it->render(trans);

	// draw separators
	if (separatorColor != 0 ||  menuTheme->Group.separatorColor != 0)
	{		
		Renderer::setMatrix(trans);

		bool prevIsGroup = false;

		y = 0;
		for (unsigned int i = 0; i < mEntries.size(); i++)
		{
			auto it = rowHeights.find(i);
			if (it != rowHeights.cend())
			{
				if (y - mCameraOffset + it->second >= 0)
				{
					if (prevIsGroup && menuTheme->Group.separatorColor != separatorColor)
						Renderer::drawRect(0.0f, y - 2.0f, mSize.x(), 1.0f, menuTheme->Group.separatorColor & 0xFFFFFF00 | (unsigned char)((menuTheme->Group.separatorColor & 0xFF) * opacity));
					else if (separatorColor != 0)
						Renderer::drawRect(0.0f, y, mSize.x(), 1.0f, separatorColor & 0xFFFFFF00 | (unsigned char)((separatorColor & 0xFF) * opacity));
				}

				y += it->second; // getRowHeight(mEntries.at(i).data);
				if (y - mCameraOffset > mSize.y())
					break;
			}

			prevIsGroup = mEntries[i].data.group;
		}

		Renderer::drawRect(0.0f, y, mSize.x(), 1.0f, separatorColor & 0xFFFFFF00 | (unsigned char)((separatorColor & 0xFF) * opacity));
	}

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
			prompts.push_back(HelpPrompt(_("up/down"), _("CHOOSE")));
	}

	return prompts;
}

bool ComponentList::moveCursor(int amt)
{
	bool ret = listInput(amt);
	listInput(0);
	return ret;
}

std::string ComponentList::getSelectedUserData()
{
	if (mCursor >= 0 && mCursor < mEntries.size())
		return mEntries[mCursor].object;

	return "";
}

bool ComponentList::hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult)
{
	Transform4x4f trans = parentTransform * getTransform();

	bool ret = false;

	mHotRow = -1;
		
	auto rect = Renderer::getScreenRect(trans, getSize(), true);
	if (x != -1 && y != -1 && rect.contains(x, y))
	{
		ret = true;

		Transform4x4f ti = trans;
		ti.translate(0, -mCameraOffset);

		float ry = 0;

		for (unsigned int i = 0; i < mEntries.size(); i++)
		{
			auto& entry = mEntries.at(i);
			float rowHeight = getRowHeight(entry.data);

			if (entry.data.selectable && ry - mCameraOffset + rowHeight >= 0)
			{
				rect = Renderer::getScreenRect(ti, Vector2f(getSize().x(), rowHeight), true);
				if (rect.contains(x, y))
				{
					mHotRow = i;
					break;
				}
			}

			ti.translate(0, rowHeight);
			ry += rowHeight;
			if (ry - mCameraOffset > mSize.y())
				break;
		}

		if (pResult != nullptr)
			pResult->push_back(this);
	}

	trans.translate(0, -mCameraOffset);

	for (int i = 0; i < getChildCount(); i++)
		ret |= getChild(i)->hitTest(x, y, trans, pResult);

	return ret;
}

bool ComponentList::onMouseClick(int button, bool pressed, int x, int y)
{
	if (button == 1)
	{
		if (pressed)
		{
			if (mHotRow >= 0)
			{
				float camOffset = mCameraOffset;
				mCursor = mHotRow;
				onCursorChanged(CURSOR_STOPPED);
				mCameraOffset = camOffset;
				mPressedRow = mHotRow;
			}

			mIsDragging = false;
			mPressedPoint = Vector2i(x, y);
			mWindow->setMouseCapture(this);
		}
		else if (mWindow->hasMouseCapture(this))
		{
			mWindow->releaseMouseCapture();

			auto row = mPressedRow;
			mPressedRow = -1;
			mPressedPoint = Vector2i(-1, -1);

			auto dragging = mIsDragging;
			mIsDragging = false;

			if (!dragging && row == mHotRow && mCursor == row)
				InputManager::getInstance()->sendMouseClick(mWindow, 1);				
		}

		return true;
	}

	return false;
}

void ComponentList::onMouseMove(int x, int y)
{
	if (mPressedPoint.x() != -1 && mPressedPoint.y() != -1 && mWindow->hasMouseCapture(this))
	{
		mIsDragging = true;
		mHotRow = -1;

		float yOffset = 0;
		for (auto entry : mEntries)
			yOffset += getRowHeight(entry.data);

		yOffset -= getSize().y();
		if (yOffset <= 0)
			return;

		mCameraOffset += mPressedPoint.y() - y;

		if (mCameraOffset < 0 )
			mCameraOffset = 0;

		if (mCameraOffset > yOffset)
			mCameraOffset = yOffset;

		mPressedPoint = Vector2i(x, y);
	}
}

bool ComponentList::onMouseWheel(int delta)
{
	int newCursor = mCursor - delta;
	
	if (newCursor < 0)
		newCursor = 0;

	if (newCursor >= mEntries.size())
		newCursor = mEntries.size() - 1;

	while (newCursor >= 0 && newCursor < mEntries.size() && !mEntries[newCursor].data.selectable)
		newCursor += delta > 0 ? -1 : 1;

	if (newCursor >= 0 && newCursor < mEntries.size() && newCursor != mCursor)
	{
		mCursor = newCursor;
		onCursorChanged(CURSOR_STOPPED);
	}

	return true;
}

