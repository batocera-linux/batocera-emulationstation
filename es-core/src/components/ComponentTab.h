#pragma once
#ifndef ES_CORE_COMPONENTS_COMPONENT_TAB_H
#define ES_CORE_COMPONENTS_COMPONENT_TAB_H

#include "IList.h"
#include "LocaleES.h"

struct ComponentTabElement
{
	ComponentTabElement(const std::shared_ptr<GuiComponent>& cmp = nullptr, bool resize_h = true, bool inv = true)
		: component(cmp), resize_height(resize_h), invert_when_selected(inv) { };

	std::shared_ptr<GuiComponent> component;
	bool resize_height;
	bool invert_when_selected;
};

struct ComponentTabItem
{
	std::vector<ComponentTabElement> elements;

	inline void addElement(const std::shared_ptr<GuiComponent>& component, bool resize_width, bool invert_when_selected = true)
	{
		if (EsLocale::isRTL())
			elements.insert(elements.begin(), ComponentTabElement(component, resize_width, invert_when_selected));
		else
			elements.push_back(ComponentTabElement(component, resize_width, invert_when_selected));
	}
};

class ComponentTab : public IList<ComponentTabItem, std::string>
{
public:
	ComponentTab(Window* window);

	void addTab(const std::string label, const std::string value = "", bool setCursorHere = false);
	void addTab(const ComponentTabItem& row, const std::string value = "", bool setCursorHere = false);

	void textInput(const char* text) override;
	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;
	virtual std::vector<HelpPrompt> getHelpPrompts() override;

	void onSizeChanged() override;
	void onFocusGained() override;
	void onFocusLost() override;

	bool moveCursor(int amt);
	inline int getCursorId() const { return mCursor; }
	
	float getTotalTabWidth() const;
	inline float getTabWidth(int row) const { return getTabWidth(mEntries.at(row).data); }

	inline void setCursorChangedCallback(const std::function<void(CursorState state)>& callback) { mCursorChangedCallback = callback; };
	inline const std::function<void(CursorState state)>& getCursorChangedCallback() const { return mCursorChangedCallback; };

protected:
	void onCursorChanged(const CursorState& state) override;

private:
	bool mFocused;

	void updateCameraOffset();
	void updateElementPosition(const ComponentTabItem& row);
	void updateElementSize(const ComponentTabItem& row);
	
	float getTabWidth(const ComponentTabItem& row) const;

	float mSelectorBarOffset;
	float mCameraOffset;

	std::function<void(CursorState state)> mCursorChangedCallback;
};

#endif // ES_CORE_COMPONENTS_COMPONENT_LIST_H
