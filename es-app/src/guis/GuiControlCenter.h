#pragma once
#ifndef ES_APP_GUIS_GUI_CONTROL_CENTER_H
#define ES_APP_GUIS_GUI_CONTROL_CENTER_H

#include "GuiComponent.h"
#include "renderers/Renderer.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class CcShell;
class Font;
class ImageComponent;
struct TextCache;

namespace pugi { class xml_node; }

// Quick-settings card rendering of a batocera-controlcenter XML file.
class GuiControlCenter : public GuiComponent
{
public:
	// refHeight: height of the output the card is on. fitContent: card sized to its content.
	GuiControlCenter(Window* window, const std::string& xmlPath, float refHeight, bool fitContent);
	~GuiControlCenter();

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;
	void onSizeChanged() override;

	bool hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult = nullptr) override;
	bool onMouseClick(int button, bool pressed, int x, int y) override;
	void onMouseMove(int x, int y) override;
	bool onMouseWheel(int delta) override;

	std::vector<HelpPrompt> getHelpPrompts() override;
	HelpStyle getHelpStyle() override;

	void open();
	void close();
	bool isClosed() const { return mClosed; }
	// the afterclick="bcc_close" actions, run once the card is gone so their keys reach the game
	std::string takeCloseActions();
	bool isClosing() const { return mClosing; }

	bool consumeDirty();

	static std::string defaultXmlPath();

	// the process that samples while hidden saves the monitor history for the others
	void setSavesHistory(bool save) { mSavesHistory = save; }

	void setOnClosed(const std::function<void()>& func) { mOnClosed = func; }
	void setOnRescreen(const std::function<void()>& func) { mOnRescreen = func; }

	struct Binding;
	struct Item;
	struct Tab;

private:
	enum class Drag { None, Pending, Scroll, Slider, Dismiss };

	void load(const std::string& xmlPath);
	void buildContainer(const pugi::xml_node& node, int tab, std::vector<Item*>& target, std::vector<std::shared_ptr<Binding>> conds);
	void buildControls(const std::string& label, const std::vector<pugi::xml_node>& controls, int tab, std::vector<Item*>& target, const std::vector<std::shared_ptr<Binding>>& conds);
	std::shared_ptr<Binding> bind(const std::string& tmpl, float refresh, int tab, int ttlMs = 1000);
	void rebuildGallery(Item* gallery, const std::string& json);
	std::shared_ptr<Binding> bindCondition(const pugi::xml_node& node, float refresh, int tab);
	void registerId(const pugi::xml_node& node, Item* item);

	bool isVisible(Item* item, int depth = 0);
	bool isVisible(Tab* tab);
	bool conditionsPass(const std::vector<std::shared_ptr<Binding>>& conds, int depth = 0);
	std::vector<Tab*> visibleTabs();
	void selectTab(int delta);
	void selectTabIndex(int index);

	void layout();
	void ensureFocusVisible();
	std::vector<Item*> focusables();
	void moveFocusVertical(int dir);
	void moveFocusHorizontal(int dir);
	std::vector<Item*> headerButtons();
	void setHeaderFocus(int index);

	void activate(Item* item);
	void openDoc(Item* item);
	void showDoc(Item* item, const std::string& path);
	void adjust(Item* item, int dir);
	void runAction(Item* item, const std::string& cmd, const std::string& afterclick, const std::vector<std::string>& updateIds);
	void afterAction(Item* item, const std::string& afterclick, const std::vector<std::string>& updateIds);
	void request(const std::shared_ptr<Binding>& binding);
	void refreshItem(Item* item);
	void refreshAll();
	void setSliderTarget(Item* item, float value);
	void commitCycler(Item* item);

	void openSheet(Item* item);
	void closeSheet();
	bool sheetInput(InputConfig* config, Input input);
	void applySheet(int index);

	Item* itemAt(float x, float y);
	int tabAt(float x, float y);
	int sheetIndexAt(float x, float y);
	Vector2f toLocal(int x, int y);

	void drawText(const std::shared_ptr<Font>& font, const std::string& text, float x, float y, float w, float h, unsigned int color, int align, const Transform4x4f& trans);
	float drawIcon(const std::shared_ptr<ImageComponent>& icon, float x, float y, float h, unsigned int color, const Transform4x4f& trans);
	void drawSelector(float x, float y, float w, float h, const Transform4x4f& trans);
	float viewHeight();
	void sliderTrack(Item* item, float& x, float& w);
	Renderer::Rect sheetRect(int& visibleRows);
	void renderItem(Item* item, const Transform4x4f& trans, float ox, float oy);
	void renderSheet(const Transform4x4f& trans);
	float sliderFraction(Item* item);

	CcShell* mShell;
	float mRefHeight;
	float mUnit;
	float mRowH;
	float mPad;
	float mSliderLabelW = 0;
	bool mFitContent;

	std::vector<Item*> mItems;
	std::vector<Tab*> mTabs;
	std::vector<Item*> mHeader;
	std::vector<Item*> mFooter;
	std::vector<std::shared_ptr<Binding>> mBindings;
	std::map<std::string, Item*> mIds;

	int mTab;
	Item* mFocus;
	bool mFocusTabs;
	int mFocusHeader;
	bool mShowFocus;
	bool mLayoutDirty;
	bool mDirty;
	bool mTabChosen;

	Renderer::Rect mCard;
	float mTitleH;
	float mHeaderH, mTabsH, mFooterH, mHelpH, mHandleH;
	float mContentH;
	float mScroll;
	std::vector<Renderer::Rect> mTabRects;

	// 0 hidden, 1 fully shown
	float mShown;
	bool mClosing;
	bool mClosed;
	std::vector<std::string> mCloseActions;

	void sampleHiddenGraphs(int deltaTime);
	void loadHistory();
	void saveHistory();

	bool mSavesHistory;
	bool mHistoryDirty;
	int mHistoryMs;
	int mHistorySaveMs;
	float mDragOffset;

	Drag mDrag;
	Vector2f mDownPos;
	float mDownScroll;
	float mDownSheetScroll;
	Item* mDownItem;

	int mRepeatDir;
	int mRepeatTime;

	Item* mSheetItem;
	std::vector<std::pair<std::string, std::string>> mSheetOptions;
	int mSheetCursor;
	float mSheetScroll;
	bool mSheetLoading;

	std::shared_ptr<Font> mFontTitle;
	std::shared_ptr<Font> mFontText;
	std::shared_ptr<Font> mFontSmall;
	std::shared_ptr<Font> mFontFooter;
	std::shared_ptr<Font> mFontSub;

	unsigned int mColorSeparator;

	float mThemeWidth;
	float mThemeHeight;
	unsigned int mColorBg, mColorText, mColorTextDim, mColorSelector, mColorSelectorEnd, mColorSelected, mColorTrack, mColorTitle, mColorFooter, mColorScrollbar;
	bool mSelectorHorizontal;

	std::shared_ptr<ImageComponent> mIconOn, mIconOff, mIconArrow, mIconOptionArrow, mIconOptionArrowLeft, mIconKnob;

	std::map<std::string, std::unique_ptr<TextCache>> mTextCache;

	std::function<void()> mOnClosed;
	std::function<void()> mOnRescreen;
};

#endif
