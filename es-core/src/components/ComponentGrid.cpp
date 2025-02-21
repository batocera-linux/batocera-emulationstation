#include "components/ComponentGrid.h"

#include "Settings.h"
#include "LocaleES.h"
#include "ThemeData.h"

using namespace GridFlags;

ComponentGrid::ComponentGrid(Window* window, const Vector2i& gridDimensions) : GuiComponent(window), 
	mGridSize(gridDimensions), mCursor(0, 0)
{
	assert(gridDimensions.x() > 0 && gridDimensions.y() > 0);

	mSeparatorColor = ThemeData::getMenuTheme()->Text.separatorColor;
	mCells.reserve(gridDimensions.x() * gridDimensions.y());

	for(int x = 0; x < gridDimensions.x(); x++)
		mColWidths.push_back(new GridSizeInfo());

	for (int y = 0; y < gridDimensions.y(); y++)
		mRowHeights.push_back(new GridSizeInfo());
}

ComponentGrid::~ComponentGrid()
{
	for (auto ptr : mRowHeights)
		delete ptr;

	for (auto ptr : mColWidths)
		delete ptr;	
}

float ComponentGrid::getColWidth(int col)
{
	if (col < 0 || col >= mColWidths.size())
		return 0;

	if (mColWidths[col]->value != 0 || mColWidths[col]->absolute)
		return mColWidths[col]->compute(mSize.x());

	// calculate automatic width
	float freeWidth = mSize.x();

	int autoSizeCols = 0;
	for (int x = 0; x < mGridSize.x(); x++)
	{
		if (mColWidths[x]->value == 0 && !mColWidths[x]->absolute)
			autoSizeCols++;
		else
			freeWidth -= mColWidths[x]->compute(mSize.x());
	}

	return freeWidth / (float)std::max(1, autoSizeCols);
}

float ComponentGrid::getRowHeight(int row)
{
	if (row < 0 || row >= mRowHeights.size())
		return 0;

	if (mRowHeights[row]->value != 0 || mRowHeights[row]->absolute)
		return mRowHeights[row]->compute(mSize.y());

	// calculate automatic height
	float freeHeight = mSize.y();

	int autoSizeRows = 0;
	for (int y = 0; y < mGridSize.y(); y++)
	{
		if (mRowHeights[y]->value == 0 && !mRowHeights[y]->absolute)
			autoSizeRows++;
		else
			freeHeight -= mRowHeights[y]->compute(mSize.y());
	}

	return freeHeight / (float) std::max(1, autoSizeRows);
}

void ComponentGrid::setColWidthPerc(int col, float width, bool update)
{
	assert(width >= 0 && width <= 1);
	assert(col >= 0 && col < mGridSize.x());

	mColWidths[col]->value = width;
	mColWidths[col]->absolute = false;

	if (update)
		onSizeChanged();
}

void ComponentGrid::setRowHeightPerc(int row, float height, bool update)
{
	assert(height >= 0 && height <= 1);
	assert(row >= 0 && row < mGridSize.y());

	mRowHeights[row]->value = height;
	mRowHeights[row]->absolute = false;

	if (update)
		onSizeChanged();
}

void ComponentGrid::setColWidth(int col, float width, bool update)
{
//	assert(width >= 0 && width <= 1);
	assert(col >= 0 && col < mGridSize.x());

	mColWidths[col]->value = width;
	mColWidths[col]->absolute = true;

	if (update)
		onSizeChanged();
}

void ComponentGrid::setRowHeight(int row, float height, bool update)
{
//	assert(height >= 0 && height <= 1);
	assert(row >= 0 && row < mGridSize.y());

	mRowHeights[row]->value = height;
	mRowHeights[row]->absolute = true;

	if (update)
		onSizeChanged();
}

void ComponentGrid::setEntry(const std::shared_ptr<GuiComponent>& comp, const Vector2i& pos, bool canFocus, bool resize, const Vector2i& size,
	unsigned int border, GridFlags::UpdateType updateType)
{
	assert(pos.x() >= 0 && pos.x() < mGridSize.x() && pos.y() >= 0 && pos.y() < mGridSize.y());
	assert(comp != nullptr);
	assert(comp->getParent() == NULL);

	GridEntry entry(pos, size, comp, canFocus, resize, updateType, border);
	mCells.push_back(entry);

	addChild(comp.get());

	if(!cursorValid() && canFocus)
	{
		auto origCursor = mCursor;
		mCursor = pos;
		onCursorMoved(origCursor, mCursor);
	}

	updateCellComponent(mCells.back());
	updateSeparators();
}

bool ComponentGrid::removeEntry(const std::shared_ptr<GuiComponent>& comp)
{
	for(auto it = mCells.cbegin(); it != mCells.cend(); it++)
	{
		if(it->component == comp)
		{
			removeChild(comp.get());
			mCells.erase(it);
			return true;
		}
	}

	return false;
}

void ComponentGrid::updateCellComponent(const GridEntry& cell)
{
	// size
	Vector2f size(0, 0);
	for(int x = cell.pos.x(); x < cell.pos.x() + cell.dim.x(); x++)
		size[0] += getColWidth(x);
	for(int y = cell.pos.y(); y < cell.pos.y() + cell.dim.y(); y++)
		size[1] += getRowHeight(y);

	if(cell.resize)
		cell.component->setSize(size);

	// position
	// find top left corner
	Vector3f pos(0, 0, 0);
	for(int x = 0; x < cell.pos.x(); x++)
		pos[0] += getColWidth(x);
	for(int y = 0; y < cell.pos.y(); y++)
		pos[1] += getRowHeight(y);

	// center component

	if (cell.component->getThemeTypeName() == "image")
	{
		pos[0] = pos.x() + size.x() * cell.component->getOrigin().x();
		pos[1] = pos.y() + size.y() * cell.component->getOrigin().y();
	}
	else
	{
		pos[0] = pos.x() + (size.x() - cell.component->getSize().x()) / 2;
		pos[1] = pos.y() + (size.y() - cell.component->getSize().y()) / 2;
	}
	
	cell.component->setPosition(pos);
}

void ComponentGrid::updateSeparators()
{
	mLines.clear();

	unsigned int color = Renderer::convertColor(mSeparatorColor);

	bool drawAll = Settings::DebugGrid();
	if (drawAll)
		color = 0xFF00FFFF;

	Vector2f pos;
	Vector2f size;
	for(auto it = mCells.cbegin(); it != mCells.cend(); it++)
	{
		if(!it->border && !drawAll)
			continue;

		// find component position + size
		pos = Vector2f(0, 0);
		size = Vector2f(0, 0);
		for(int x = 0; x < it->pos.x(); x++)
			pos[0] += getColWidth(x);
		for(int y = 0; y < it->pos.y(); y++)
			pos[1] += getRowHeight(y);
		for(int x = it->pos.x(); x < it->pos.x() + it->dim.x(); x++)
			size[0] += getColWidth(x);
		for(int y = it->pos.y(); y < it->pos.y() + it->dim.y(); y++)
			size[1] += getRowHeight(y);

		if(it->border & BORDER_TOP || drawAll)
		{
			mLines.push_back( { { pos.x(),               pos.y()               }, { 0.0f, 0.0f }, color } );
			mLines.push_back( { { pos.x() + size.x(),    pos.y()               }, { 0.0f, 0.0f }, color } );
		}
		if(it->border & BORDER_BOTTOM || drawAll)
		{
			mLines.push_back( { { pos.x(),               pos.y() + size.y()    }, { 0.0f, 0.0f }, color } );
			mLines.push_back( { { pos.x() + size.x(),    mLines.back().pos.y() }, { 0.0f, 0.0f }, color } );
		}
		if(it->border & BORDER_LEFT || drawAll)
		{
			mLines.push_back( { { pos.x(),               pos.y()               }, { 0.0f, 0.0f }, color } );
			mLines.push_back( { { pos.x(),               pos.y() + size.y()    }, { 0.0f, 0.0f }, color } );
		}
		if(it->border & BORDER_RIGHT || drawAll)
		{
			mLines.push_back( { { pos.x() + size.x(),    pos.y()               }, { 0.0f, 0.0f }, color } );
			mLines.push_back( { { mLines.back().pos.x(), pos.y() + size.y()    }, { 0.0f, 0.0f }, color } );
		}
	}
}

void ComponentGrid::onSizeChanged()
{
	GuiComponent::onSizeChanged();

	for(auto it = mCells.cbegin(); it != mCells.cend(); it++)
		updateCellComponent(*it);

	updateSeparators();
}

const ComponentGrid::GridEntry* ComponentGrid::getCellAt(int x, int y) const
{
	assert(x >= 0 && x < mGridSize.x() && y >= 0 && y < mGridSize.y());
	
	for(auto it = mCells.cbegin(); it != mCells.cend(); it++)
	{
		int xmin = it->pos.x();
		int xmax = xmin + it->dim.x();
		int ymin = it->pos.y();
		int ymax = ymin + it->dim.y();

		if(x >= xmin && y >= ymin && x < xmax && y < ymax)
			return &(*it);
	}

	return NULL;
}

bool ComponentGrid::input(InputConfig* config, Input input)
{
	const GridEntry* cursorEntry = getCellAt(mCursor);
	if(cursorEntry && cursorEntry->component->input(config, input))
		return true;

	if(!input.value)
		return false;

	bool result = false;

	if (config->isMappedLike("down", input))
		result = moveCursor(Vector2i(0, 1));
	if (config->isMappedLike("up", input))
		result = moveCursor(Vector2i(0, -1));
	if (config->isMappedLike("left", input))
		result = moveCursor(Vector2i(-1, 0));
	if (config->isMappedLike("right", input))
		result = moveCursor(Vector2i(1, 0));

	if (!result && mUnhandledInputCallback)
		return mUnhandledInputCallback(config, input);	

	return result;
}

void ComponentGrid::resetCursor()
{
	if(!mCells.size())
		return;

	for(auto it = mCells.cbegin(); it != mCells.cend(); it++)
	{
		if(it->canFocus)
		{
			Vector2i origCursor = mCursor;
			mCursor = it->pos;
			onCursorMoved(origCursor, mCursor);
			break;
		}
	}
}

Vector2i ComponentGrid::getCursor()
{
	return mCursor;
}

bool ComponentGrid::moveCursor(Vector2i dir)
{
	assert(dir.x() || dir.y());

	const Vector2i origCursor = mCursor;

	const GridEntry* currentCursorEntry = getCellAt(mCursor);

	Vector2i searchAxis(dir.x() == 0, dir.y() == 0);
	
	while(mCursor.x() >= 0 && mCursor.y() >= 0 && mCursor.x() < mGridSize.x() && mCursor.y() < mGridSize.y())
	{
		mCursor = mCursor + dir;

		Vector2i curDirPos = mCursor;

		const GridEntry* cursorEntry;
		//spread out on search axis+
		while(mCursor.x() < mGridSize.x() && mCursor.y() < mGridSize.y() && mCursor.x() >= 0 && mCursor.y() >= 0)
		{
			cursorEntry = getCellAt(mCursor);
			if(cursorEntry && cursorEntry->canFocus && cursorEntry != currentCursorEntry)
			{
				onCursorMoved(origCursor, mCursor);
				return true;
			}

			mCursor += dir;
		}
		/*
		//now again on search axis-
		mCursor = curDirPos;
		while(mCursor.x() >= 0 && mCursor.y() >= 0 && mCursor.x() < mGridSize.x() && mCursor.y() < mGridSize.y())
		{
			cursorEntry = getCellAt(mCursor);
			if(cursorEntry && cursorEntry->canFocus && cursorEntry != currentCursorEntry)
			{
				onCursorMoved(origCursor, mCursor);
				return true;
			}

			mCursor -= dir;
		}
		*/
		mCursor = curDirPos;
	}

	//failed to find another focusable element in this direction
	mCursor = origCursor;
	return false;
}

void ComponentGrid::setCursorTo(Vector2i pos)
{
	assert(pos.x() >= 0 && pos.x() < mGridSize.x() && pos.y() >= 0 && pos.y() < mGridSize.y());

	const Vector2i origCursor = mCursor;
	mCursor = pos;

	onCursorMoved(origCursor, pos);
}

void ComponentGrid::onFocusLost()
{
	const GridEntry* cursorEntry = getCellAt(mCursor);
	if(cursorEntry)
		cursorEntry->component->onFocusLost();
}

void ComponentGrid::onFocusGained()
{
	const GridEntry* cursorEntry = getCellAt(mCursor);
	if(cursorEntry)
		cursorEntry->component->onFocusGained();
}

bool ComponentGrid::cursorValid()
{
	const GridEntry* e = getCellAt(mCursor);
	return (e != NULL && e->canFocus);
}

void ComponentGrid::update(int deltaTime)
{
	// update ALL THE THINGS
	const GridEntry* cursorEntry = getCellAt(mCursor);
	for(auto it = mCells.cbegin(); it != mCells.cend(); it++)
	{
		if(it->updateType == UPDATE_ALWAYS || (it->updateType == UPDATE_WHEN_SELECTED && cursorEntry == &(*it)))
			it->component->update(deltaTime);
	}
}

void ComponentGrid::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();

	auto rect = Renderer::getScreenRect(trans, mSize);
	if (!Renderer::isVisibleOnScreen(rect))
		return;

	renderChildren(trans);
	
	// draw cell separators
	if(mLines.size())
	{
		Renderer::setMatrix(trans);
		Renderer::bindTexture(0);
		Renderer::drawLines(&mLines[0], mLines.size());
	}
}

void ComponentGrid::textInput(const char* text)
{
	const GridEntry* selectedEntry = getCellAt(mCursor);
	if(selectedEntry != NULL && selectedEntry->canFocus)
		selectedEntry->component->textInput(text);
}

void ComponentGrid::onCursorMoved(Vector2i from, Vector2i to)
{
	const GridEntry* cell = getCellAt(from);
	if(cell)
		cell->component->onFocusLost();

	cell = getCellAt(to);
	if(cell)
		cell->component->onFocusGained();

	updateHelpPrompts();
}

bool ComponentGrid::isCursorTo(const std::shared_ptr<GuiComponent>& comp)
{
	for (auto it = mCells.cbegin(); it != mCells.cend(); it++)
		if (it->component == comp && mCursor == it->pos)
			return true;

	return false;
}

void ComponentGrid::setCursorTo(const std::shared_ptr<GuiComponent>& comp)
{
	for(auto it = mCells.cbegin(); it != mCells.cend(); it++)
	{
		if(it->component == comp)
		{
			Vector2i oldCursor = mCursor;
			mCursor = it->pos;
			onCursorMoved(oldCursor, mCursor);
			return;
		}
	}
}

std::vector<HelpPrompt> ComponentGrid::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	const GridEntry* e = getCellAt(mCursor);
	if(e)
		prompts = e->component->getHelpPrompts();
	
	bool canScrollVert = mGridSize.y() > 1;
	bool canScrollHoriz = mGridSize.x() > 1;
	for(auto it = prompts.cbegin(); it != prompts.cend(); it++)
	{
		if(it->first == "up/down/left/right")
		{
			canScrollHoriz = false;
			canScrollVert = false;
			break;
		}else if(it->first == "up/down")
		{
			canScrollVert = false;
		}else if(it->first == "left/right")
		{
			canScrollHoriz = false;
		}
	}

	if(canScrollHoriz && canScrollVert)
	  prompts.push_back(HelpPrompt("up/down/left/right", _("CHOOSE"))); 
	else if(canScrollHoriz)
	  prompts.push_back(HelpPrompt("left/right", _("CHOOSE"))); 
	else if(canScrollVert)
	  prompts.push_back(HelpPrompt("up/down", _("CHOOSE"))); 

	return prompts;
}
