#pragma once
#ifndef ES_CORE_COMPONENTS_COMPONENT_LIST_H
#define ES_CORE_COMPONENTS_COMPONENT_LIST_H

#include "IList.h"
#include "LocaleES.h"
#include "components/ScrollbarComponent.h"
#include "math/Vector2i.h"

namespace ComponentListFlags
{
	enum UpdateType
	{
		UPDATE_ALWAYS,
		UPDATE_WHEN_SELECTED,
		UPDATE_NEVER
	};
};

struct ComponentListElement
{
	ComponentListElement(const std::shared_ptr<GuiComponent>& cmp = nullptr, bool resize_w = true)
		: component(cmp), resize_width(resize_w) { };

	std::shared_ptr<GuiComponent> component;
	bool resize_width;	
};

struct ComponentListRow
{
	ComponentListRow() 
	{
		selectable = true;
		group = false;
	};

	bool group;
	bool selectable;
	std::vector<ComponentListElement> elements;

	// The input handler is called when the user enters any input while this row is highlighted (including up/down).
	// Return false to let the list try to use it or true if the input has been consumed.
	// If no input handler is supplied (input_handler == nullptr), the default behavior is to forward the input to 
	// the rightmost element in the currently selected row.
	std::function<bool(InputConfig*, Input)> input_handler;
	
	inline void addElement(const std::shared_ptr<GuiComponent>& component, bool resize_width)
	{
		if (EsLocale::isRTL())
			elements.insert(elements.begin(), ComponentListElement(component, resize_width));
		else
			elements.push_back(ComponentListElement(component, resize_width));
	}

	// Utility method for making an input handler for "when the users presses A on this, do func."
	inline void makeAcceptInputHandler(const std::function<void()>& func, bool onButtonRelease = false)
	{
		if (func == nullptr)
			return;

		input_handler = [func, onButtonRelease](InputConfig* config, Input input) -> bool 
		{
			if(config->isMappedTo(BUTTON_OK, input) && (onButtonRelease ? input.value == 0 : input.value != 0)) 
			{
				func();
				return true;
			}
			return false;
		};
	}
};

class ComponentList : public IList<ComponentListRow, std::string>
{
public:
	ComponentList(Window* window);

	void addRow(const ComponentListRow& row, bool setCursorHere = false, bool updateSize = true, const std::string& userData = "");
	void addGroup(const std::string& label, bool forceVisible = false);
	void removeLastRowIfGroup();

	void textInput(const char* text) override;
	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;
	virtual std::vector<HelpPrompt> getHelpPrompts() override;

	void onSizeChanged() override;
	void onFocusGained() override;
	void onFocusLost() override;

	void setUpdateType(ComponentListFlags::UpdateType updateType) { mUpdateType = updateType;  }

	bool moveCursor(int amt);
	inline int getCursorId() const { return mCursor; }

	std::string getSelectedUserData();
	
	float getTotalRowHeight() const;
	inline float getRowHeight(int row) const { return getRowHeight(mEntries.at(row).data); }

	inline void setCursorChangedCallback(const std::function<void(CursorState state)>& callback) { mCursorChangedCallback = callback; };
	inline const std::function<void(CursorState state)>& getCursorChangedCallback() const { return mCursorChangedCallback; };

	void saySelectedLine();

	virtual bool onMouseClick(int button, bool pressed, int x, int y) override;
	virtual void onMouseMove(int x, int y) override;
	virtual bool onMouseWheel(int delta) override;
	virtual bool hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult = nullptr) override;

protected:
	void onCursorChanged(const CursorState& state) override;

	virtual int onBeforeScroll(int cursor, int direction) override
	{ 
		int looped = cursor;
		while (cursor >= 0 && cursor < mEntries.size() && !mEntries[cursor].data.selectable)
		{
			cursor += direction;
			if (cursor == looped || cursor < 0 || cursor >= mEntries.size())
				return mCursor;
		}
		
		return cursor; 
	}

private:
	bool mFocused;
	int mOldCursor;

	void updateCameraOffset();
	void updateElementPosition(const ComponentListRow& row, float yOffset = -1.0);
	void updateElementSize(const ComponentListRow& row);
	
	float getRowHeight(const ComponentListRow& row) const;

	float mSelectorBarOffset;
	float mCameraOffset;

	ComponentListFlags::UpdateType mUpdateType;

	std::function<void(CursorState state)> mCursorChangedCallback;

	ScrollbarComponent mScrollbar;
	int		  mHotRow;
	int		  mPressedRow;
	Vector2i  mPressedPoint;
	bool	  mIsDragging;
};

#endif // ES_CORE_COMPONENTS_COMPONENT_LIST_H
