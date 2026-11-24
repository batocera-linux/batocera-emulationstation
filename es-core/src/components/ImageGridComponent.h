#pragma once
#ifndef ES_CORE_COMPONENTS_IMAGE_GRID_COMPONENT_H
#define ES_CORE_COMPONENTS_IMAGE_GRID_COMPONENT_H

#include "Log.h"
#include "components/IList.h"
#include "resources/TextureResource.h"
#include "GridTileComponent.h"
#include "animations/LambdaAnimation.h"
#include "Settings.h"
#include "Sound.h"
#include <algorithm>
#include "LocaleES.h"
#include "components/ScrollbarComponent.h"
#include "components/TextComponent.h"
#include <set>
#include <map>
#include <numeric>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <unordered_map>
#include "InputManager.h"
#include "utils/Delegate.h"
#include "BindingManager.h"

#define EXTRAITEMS 2
#define ALLOWANIMATIONS (Settings::PowerSaverMode() != "instant" && Settings::TransitionStyle() != "instant")
#define HOLD_TIME 1000

enum ScrollDirection
{
	SCROLL_VERTICALLY,
	SCROLL_HORIZONTALLY
};

enum ImageSource
{
	THUMBNAIL,
	IMAGE,
	MARQUEE,
	MARQUEEORTEXT,
	FANART,
	TITLESHOT,
	BOXART,
	CARTRIDGE,
	BOXBACK,
	MIX
};

enum CenterSelection
{
	FULL,
	PARTIAL,
	NEVER
};


struct ImageGridData
{
	std::shared_ptr<GridTileComponent> tile;
	std::string texturePath;
	int sourceIndex = 0;						// original insertion order (restored when ungrouping)
};

// Group of consecutive entries sharing the same property value (see setGroupBy)
struct ImageGridGroup
{
	std::string label;
	int firstIndex = 0;						 // index of the group's first entry
	int count = 0;
	float y = 0;								// top of the header (before padding / camera)
	std::shared_ptr<TextComponent> text;		// created on demand, themed via <text name="gridgroup">
	int scrollCol = 0;						  // band mode: first visible column of the band (target)
	float scrollX = 0;						  // band mode: current horizontal offset in px (animated)
};

// Sort value of an entry (sortBy): numeric if the whole text is a number, otherwise natural comparison
struct ImageGridSortKey
{
	std::string text;
	double number = 0;
	bool isNumber = false;
};

inline ImageGridSortKey makeImageGridSortKey(const std::string& text)
{
	ImageGridSortKey key;
	key.text = text;

	if (!text.empty() && (std::isdigit((unsigned char) text[0]) || text[0] == '-' || text[0] == '+' || text[0] == '.'))
	{
		char* end = nullptr;
		double value = std::strtod(text.c_str(), &end);
		if (end != text.c_str() && *end == '\0')
		{
			key.number = value;
			key.isNumber = true;
		}
	}

	return key;
}

// "Natural" order: "Game 2" < "Game 10", case-insensitive (ASCII)
inline int compareImageGridNatural(const std::string& a, const std::string& b)
{
	size_t i = 0, j = 0;
	while (i < a.size() && j < b.size())
	{
		if (std::isdigit((unsigned char) a[i]) && std::isdigit((unsigned char) b[j]))
		{
			size_t si = i, sj = j;
			while (si < a.size() && a[si] == '0') si++;
			while (sj < b.size() && b[sj] == '0') sj++;

			size_t ei = si, ej = sj;
			while (ei < a.size() && std::isdigit((unsigned char) a[ei])) ei++;
			while (ej < b.size() && std::isdigit((unsigned char) b[ej])) ej++;

			const size_t li = ei - si, lj = ej - sj;
			if (li != lj)
				return li < lj ? -1 : 1;

			int c = a.compare(si, li, b, sj, lj);
			if (c != 0)
				return c < 0 ? -1 : 1;

			i = ei;
			j = ej;
		}
		else
		{
			int ca = std::toupper((unsigned char) a[i]);
			int cb = std::toupper((unsigned char) b[j]);
			if (ca != cb)
				return ca < cb ? -1 : 1;

			i++;
			j++;
		}
	}

	if (i < a.size()) return 1;
	if (j < b.size()) return -1;
	return 0;
}

inline int compareImageGridSortKeys(const ImageGridSortKey& a, const ImageGridSortKey& b)
{
	if (a.isNumber && b.isNumber)
		return a.number < b.number ? -1 : (a.number > b.number ? 1 : 0);

	return compareImageGridNatural(a.text, b.text);
}

// Type trait to check if a type derives from IBindable
template <typename T>
struct is_bindable : std::is_base_of<IBindable, std::remove_pointer_t<T>> {};

template<typename T>
class ImageGridComponent : public IList<ImageGridData, T>
{
protected:
	using IList<ImageGridData, T>::mEntries;
	using IList<ImageGridData, T>::mScrollTier;
	using IList<ImageGridData, T>::listUpdate;
	using IList<ImageGridData, T>::listInput;
	using IList<ImageGridData, T>::listRenderTitleOverlay;
	using IList<ImageGridData, T>::getTransform;
	using IList<ImageGridData, T>::mSize;
	using IList<ImageGridData, T>::mCursor;
	using IList<ImageGridData, T>::mShowing;
	using IList<ImageGridData, T>::mVisible;
	using IList<ImageGridData, T>::Entry;
	using IList<ImageGridData, T>::mWindow;

public:
	using IList<ImageGridData, T>::size;
	using IList<ImageGridData, T>::isScrolling;
	using IList<ImageGridData, T>::stopScrolling;
	using IList<ImageGridData, T>::updateBindings;

	ImageGridComponent(Window* window);

	std::string getThemeTypeName() override { return "imagegrid"; }

	void add(const std::string& name, const std::string& imagePath, const T& obj);
	virtual void clear();

	// force: made again even when the path is the same (the file appeared or changed since)
	void setImage(const std::string& imagePath, const T& obj, bool force = false);
	std::string getImage(const T& obj);

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;
	virtual void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

	void onSizeChanged() override;
	inline void setCursorChangedCallback(const std::function<void(CursorState state)>& func) { mCursorChangedCallback = func; }

	void setThemeName(std::string name) { mName = name; };

	virtual void topWindow(bool isTop);
	virtual void onShow();
	virtual void onHide();
	virtual void onScreenSaverActivate();
	virtual void onScreenSaverDeactivate();

	virtual void setOpacity(unsigned char opacity);

	ImageSource		getImageSource() { return mImageSource; };

	void setGridSizeOverride(Vector2f size);

	std::shared_ptr<GridTileComponent> getSelectedTile();

	void resetLastCursor() { mLastCursor = -1; mLastCursorState = CursorState::CURSOR_STOPPED; }
	int getLastCursor() { return mLastCursor; }

	virtual bool onMouseClick(int button, bool pressed, int x, int y) override;
	virtual void onMouseMove(int x, int y) override;
	virtual bool onMouseWheel(int delta) override;
	virtual bool hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult = nullptr) override;

	virtual int getCursorIndex() override
	{
		if (mEntriesReordered && mCursor >= 0 && mCursor < (int)mEntries.size())
			return mEntries[mCursor].data.sourceIndex;

		return mCursor;
	}

	virtual void setCursorIndex(int index, bool force = false) override
	{
		if (mEntriesReordered && index >= 0 && index < (int)mEntries.size())
		{
			int current = -1;

			if (mEntries[index].data.sourceIndex == index) // common case: entry stayed in place
				current = index;
			else
			{
				for (int i = 0; i < (int)mEntries.size(); i++)
				{
					if (mEntries[i].data.sourceIndex == index)
					{
						current = i;
						break;
					}
				}
			}

			if (current < 0)
				return;

			index = current;
		}

		IList<ImageGridData, T>::setCursorIndex(index, force);
	}

	Vector3f getCameraOffset() 
	{ 
		if (isVertical())
			return Vector3f(0, mCameraOffset, 0);

		return Vector3f(mCameraOffset,  0, 0);
	}

	Delegate<ILongMouseClickEvent> longMouseClick;

	void		preloadTiles();

	// Groups entries according to IBindable::getProperty(property). Empty = no groups.
	// Entries are physically reordered (stable sort) so that each group is contiguous; the original order
	// is restored when ungrouping. sortByLabel=true (default): groups in alphabetical order (case-insensitive);
	// false: groups in order of first appearance.
	void setGroupBy(const std::string& property, bool sortByLabel = true)
	{
		if (mGroupBy == property && mGroupSortByLabel == sortByLabel)
			return;

		mGroupBy = property;
		mGroupSortByLabel = sortByLabel;

		for (auto& entry : mEntries)
			entry.data.tile = nullptr;

		resetGrid();
	}

	const std::string& getGroupBy() const { return mGroupBy; }

	// Sorts entries according to IBindable::getProperty(property) (within each group if groups are active).
	// Numbers are compared numerically, text in case-insensitive natural order, missing values last.
	// Empty = original insertion order. Theme: <sortBy>name</sortBy> and <sortDescending>true</sortDescending>
	void setSortBy(const std::string& property, bool descending = false)
	{
		if (mSortBy == property && mSortDescending == descending)
			return;

		mSortBy = property;
		mSortDescending = descending;

		for (auto& entry : mEntries)
			entry.data.tile = nullptr;

		resetGrid();
	}

	const std::string& getSortBy() const { return mSortBy; }

	// Groups always placed at the end, in the given order, regardless of how groups are sorted (e.g. {"collections"}).
	// Case-insensitive comparison with the value of the groupBy property. To be called by the code that fills the grid.
	void setGroupsAtEnd(const std::vector<std::string>& labels)
	{
		if (mGroupsAtEnd == labels)
			return;

		mGroupsAtEnd = labels;

		for (auto& entry : mEntries)
			entry.data.tile = nullptr;

		resetGrid();
	}

	const std::vector<std::string>& getGroupsAtEnd() const { return mGroupsAtEnd; }

	// Groups always placed at the start, in the given order (e.g. {"favorites"}), before all the others.
	// Case-insensitive comparison with the value of the groupBy property.
	// Theme: <groupsAtStart>favorites;recent</groupsAtStart> and <groupsAtEnd>collections;other</groupsAtEnd>
	void setGroupsAtStart(const std::vector<std::string>& labels)
	{
		if (mGroupsAtStart == labels)
			return;

		mGroupsAtStart = labels;

		for (auto& entry : mEntries)
			entry.data.tile = nullptr;

		resetGrid();
	}

	const std::vector<std::string>& getGroupsAtStart() const { return mGroupsAtStart; }

	// Band mode (Netflix / Prime Video style): each group fits on a single row that scrolls horizontally,
	// the bands scroll vertically. No effect if groups are not active. Theme: <groupBands>true</groupBands>
	void setGroupBands(bool bands)
	{
		if (mGroupBands == bands)
			return;

		mGroupBands = bands;

		for (auto& entry : mEntries)
			entry.data.tile = nullptr;

		resetGrid();
	}

	bool getGroupBands() const { return mGroupBands; }

protected:
	virtual void onCursorChanged(const CursorState& state) override;
	virtual void onScroll(int /*amt*/) { if (!mScrollSound.empty()) Sound::get(mScrollSound)->play(); }

private:
	void		resetGrid();
	void		calcGridDimension();
	
	void		ensureVisibleTileExist();
	Vector2i	getVisibleRange();
	void		loadTile(std::shared_ptr<GridTileComponent> tile, typename IList<ImageGridData, T>::Entry& entry);
	std::shared_ptr<GridTileComponent> createTile(int i, int dimOpposite, Vector2f tileDistance, Vector2f startPosition);

	inline bool isVertical() { return mScrollDirection == SCROLL_VERTICALLY; };

	// GROUPS (vertical scrolling only, without centerSelection)
	inline bool hasGroups() { return !mGroupBy.empty() && isVertical() && mCenterSelection == CenterSelection::NEVER; }

	std::string getGroupKey(const typename IList<ImageGridData, T>::Entry& entry);
	void computeLayout();
	float getGroupedCameraOffset();
	int moveByRows(int rows);
	void ensureGroupText(ImageGridGroup& group);

	bool arrangeEntries(std::vector<std::string>& groupKeys, bool grouped);
	bool restoreEntryOrder();
	std::string getSortValue(const typename IList<ImageGridData, T>::Entry& entry);

	std::string mSortBy;
	bool mSortDescending;

	std::vector<std::string> mGroupsAtStart;
	std::vector<std::string> mGroupsAtEnd;
	static int findGroupLabel(const std::vector<std::string>& labels, const std::string& label);
	int getGroupAtStartIndex(const std::string& label);
	int getGroupAtEndIndex(const std::string& label);
	void applyEntryOrder(const std::vector<int>& order);

	bool mGroupSortByLabel;
	bool mEntriesReordered;					 // mEntries has been permuted by grouping
	int mNextSourceIndex;

	// BANDS
	inline bool hasBands() { return hasGroups() && mGroupBands; }

	int remapBandMove(int from, int axis, int step);
	void scrollBandToCursor(bool animate);
	float getBandScrollX(const std::shared_ptr<GridTileComponent>& tile);
	bool isInBandWindow(int i);
	void animateBandTo(int band, float targetX, bool animate);
	int getBandAtScreenY(int y);
	void dragBand(int dx);
	void endBandDrag();

	int mDragAxis;							  // mouse drag in band mode: 0 = undecided, 1 = horizontal, 2 = vertical
	int mDragBand;							  // band grabbed at press time
	int mScreenY;							   // top of the component on screen (updated at render)
	float mScreenScaleY;

	bool mGroupBands;
	int mBandAxis;							  // keyboard-driven movement: 0 = none, 1 = horizontal, 2 = vertical
	int mBandStep;							  // number of bands per vertical step (page up/down)
	int mWantedSlot;							// visual column kept during vertical moves
	int mWantedSlotCursor;					  // cursor to which mWantedSlot applies

	std::string mGroupBy;
	float mGroupHeaderHeight;				   // px
	float mGroupSpacing;						// px between the end of a group and the next header
	bool mLayoutDirty;
	float mGroupCamera;						 // camera target (independent of the animation in progress)

	std::vector<ImageGridGroup> mGroups;
	std::vector<Vector2i> mCells;			   // (col, row) of each entry
	std::vector<float> mRowTop;				 // y of the top of each row
	std::vector<float> mRowReveal;			  // y to make visible in order to show the row (header if first row of the group)
	std::vector<int> mRowFirst;				 // index of the first entry of each row

	bool mEntriesDirty;

	int mLastCursor;
	CursorState mLastCursorState;

	std::string mDefaultGameTexture;
	std::string mDefaultFolderTexture;
	std::string mDefaultLogoBackgroundTexture;

	std::vector<std::shared_ptr<GridTileComponent>> mVisibleTiles;

	// TILES
	bool mLastRowPartial;
	bool mAnimateSelection;
	Vector2f mAutoLayout;
	float mAutoLayoutZoom;
	Vector4f mPadding;
	Vector2f mMargin;
	Vector2f mTileSize;
	Vector2i mGridDimension;
	Vector2f mGridSizeOverride;

	std::string mScrollSound;

	std::shared_ptr<ThemeData> mTheme;

	std::string mName;

	int mStartPosition;

	bool mAllowVideo;
	float mVideoDelay;

	// MISCELLANEOUS
	CenterSelection mCenterSelection;

	bool mScrollLoop;

	ScrollDirection mScrollDirection;
	ImageSource		mImageSource;

	ScrollbarComponent mScrollbar;

	std::function<void(CursorState state)> mCursorChangedCallback;

	// Mouse
	int		  mPressedCursor;
	Vector2i  mPressedPoint;
	bool	  mIsDragging;
	int		  mTimeHoldingButton;

	float mTotalHeight;
	float mCameraOffset;

	std::map<int, std::shared_ptr<GridTileComponent>> mScrollLoopTiles;

	// Handle pointer types derived from IBindable
	template <typename U = T>
	typename std::enable_if<is_bindable<U>::value, IBindable*>::type
		getBindable(const typename IList<ImageGridData, T>::Entry& entry)
	{
		IBindable* bindingPtr = static_cast<IBindable*>(entry.object);
		return bindingPtr;
	}

	// Handle other types
	template <typename U = T>
	typename std::enable_if<!is_bindable<U>::value, IBindable*>::type
		getBindable(const typename IList<ImageGridData, T>::Entry& entry)
	{
		return nullptr;
	}
};



template<typename T>
std::string ImageGridComponent<T>::getGroupKey(const typename IList<ImageGridData, T>::Entry& entry)
{
	IBindable* bindable = getBindable(entry);
	if (bindable == nullptr)
		return "";

	return bindable->getProperty(mGroupBy).toString();
}

// Reorders mEntries: order[k] = old index of the entry that takes position k. The cursor follows its entry.
template<typename T>
void ImageGridComponent<T>::applyEntryOrder(const std::vector<int>& order)
{
	const int count = (int)mEntries.size();

	std::vector<int> newPosition(count);
	std::vector<typename IList<ImageGridData, T>::Entry> sorted;
	sorted.reserve(count);

	for (int k = 0; k < count; k++)
	{
		newPosition[order[k]] = k;
		sorted.push_back(std::move(mEntries[order[k]]));
		sorted.back().data.tile = nullptr; // tile positions are stale: they will be recreated
	}

	mEntries = std::move(sorted);

	mVisibleTiles.clear();
	mScrollLoopTiles.clear();

	if (mCursor >= 0 && mCursor < count)
		mCursor = newPosition[mCursor];

	mLastCursor = -1;
	mLastCursorState = CursorState::CURSOR_STOPPED;
	mEntriesDirty = true;
}

// Position of the label in a list of pinned groups (case-insensitive), -1 if it is not in the list
template<typename T>
int ImageGridComponent<T>::findGroupLabel(const std::vector<std::string>& labels, const std::string& label)
{
	for (int i = 0; i < (int)labels.size(); i++)
	{
		const std::string& pinned = labels[i];
		if (pinned.size() != label.size())
			continue;

		bool same = true;
		for (size_t c = 0; c < label.size() && same; c++)
			same = std::toupper((unsigned char) pinned[c]) == std::toupper((unsigned char) label[c]);

		if (same)
			return i;
	}

	return -1;
}

template<typename T>
int ImageGridComponent<T>::getGroupAtStartIndex(const std::string& label)
{
	return findGroupLabel(mGroupsAtStart, label);
}

template<typename T>
int ImageGridComponent<T>::getGroupAtEndIndex(const std::string& label)
{
	return findGroupLabel(mGroupsAtEnd, label);
}

template<typename T>
std::string ImageGridComponent<T>::getSortValue(const typename IList<ImageGridData, T>::Entry& entry)
{
	IBindable* bindable = getBindable(entry);
	if (bindable == nullptr)
		return "";

	return bindable->getProperty(mSortBy).toString();
}

// Orders mEntries: contiguous groups (according to mGroupBy), then inner sort according to mSortBy, then original insertion order
// (stable sort, total order: the result does not depend on the current order). groupKeys[i] = group key of entry i,
// ignored if !grouped; it is permuted along with the entries. Returns true if the order changed.
template<typename T>
bool ImageGridComponent<T>::arrangeEntries(std::vector<std::string>& groupKeys, bool grouped)
{
	const int count = (int)mEntries.size();

	std::vector<int> ranks(count, 0);

	if (grouped)
	{
		// Entries traversed in original insertion order, so that "first appearance" has a stable meaning
		std::vector<int> source(count);
		std::iota(source.begin(), source.end(), 0);
		std::stable_sort(source.begin(), source.end(), [this](int a, int b) { return mEntries[a].data.sourceIndex < mEntries[b].data.sourceIndex; });

		std::unordered_map<std::string, int> rankByKey;
		std::vector<std::string> distinct;
		for (int idx : source)
			if (rankByKey.emplace(groupKeys[idx], (int)distinct.size()).second)
				distinct.push_back(groupKeys[idx]);

		if (mGroupSortByLabel)
		{
			// Case-insensitive alphabetical (ASCII); on ties, binary order to stay deterministic
			std::sort(distinct.begin(), distinct.end(), [](const std::string& a, const std::string& b)
			{
				const size_t n = std::min(a.size(), b.size());
				for (size_t i = 0; i < n; i++)
				{
					int ca = std::toupper((unsigned char) a[i]);
					int cb = std::toupper((unsigned char) b[i]);
					if (ca != cb)
						return ca < cb;
				}

				return a.size() != b.size() ? a.size() < b.size() : a < b;
			});
		}

		// Pinned groups: setGroupsAtStart first, then all the others, then setGroupsAtEnd (each in the order given by the caller).
		// A label present in both lists stays at the start.
		auto endOfStart = std::stable_partition(distinct.begin(), distinct.end(), [this](const std::string& key) { return getGroupAtStartIndex(key) >= 0; });
		std::stable_sort(distinct.begin(), endOfStart, [this](const std::string& a, const std::string& b) { return getGroupAtStartIndex(a) < getGroupAtStartIndex(b); });

		auto firstAtEnd = std::stable_partition(endOfStart, distinct.end(), [this](const std::string& key) { return getGroupAtEndIndex(key) < 0; });
		std::stable_sort(firstAtEnd, distinct.end(), [this](const std::string& a, const std::string& b) { return getGroupAtEndIndex(a) < getGroupAtEndIndex(b); });

		// distinct is now in the final group order
		for (int g = 0; g < (int)distinct.size(); g++)
			rankByKey[distinct[g]] = g;

		for (int i = 0; i < count; i++)
			ranks[i] = rankByKey[groupKeys[i]];
	}

	std::vector<ImageGridSortKey> sortKeys;
	if (!mSortBy.empty())
	{
		sortKeys.reserve(count);
		for (int i = 0; i < count; i++)
			sortKeys.push_back(makeImageGridSortKey(getSortValue(mEntries[i])));
	}

	std::vector<int> order(count);
	std::iota(order.begin(), order.end(), 0);
	std::stable_sort(order.begin(), order.end(), [this, &ranks, &sortKeys](int a, int b)
	{
		if (ranks[a] != ranks[b])
			return ranks[a] < ranks[b];

		if (!sortKeys.empty())
		{
			const ImageGridSortKey& ka = sortKeys[a];
			const ImageGridSortKey& kb = sortKeys[b];

			if (ka.text.empty() != kb.text.empty())
				return !ka.text.empty(); // missing values last, regardless of direction

			int c = compareImageGridSortKeys(ka, kb);
			if (c != 0)
				return mSortDescending ? c > 0 : c < 0;
		}

		return mEntries[a].data.sourceIndex < mEntries[b].data.sourceIndex;
	});

	if (std::is_sorted(order.begin(), order.end())) // already in the wanted order, nothing moves
		return false;

	applyEntryOrder(order);
	mEntriesReordered = true;

	if (grouped)
	{
		std::vector<std::string> sortedKeys;
		sortedKeys.reserve(count);
		for (int k = 0; k < count; k++)
			sortedKeys.push_back(std::move(groupKeys[order[k]]));

		groupKeys = std::move(sortedKeys);
	}

	return true;
}

// No groups and no sort: recovers the original insertion order. Returns true if the order changed.
template<typename T>
bool ImageGridComponent<T>::restoreEntryOrder()
{
	if (!mEntriesReordered)
		return false;

	mEntriesReordered = false;

	std::vector<int> order(mEntries.size());
	std::iota(order.begin(), order.end(), 0);
	std::stable_sort(order.begin(), order.end(), [this](int a, int b) { return mEntries[a].data.sourceIndex < mEntries[b].data.sourceIndex; });

	if (std::is_sorted(order.begin(), order.end()))
		return false;

	applyEntryOrder(order);
	return true;
}

template<typename T>
void ImageGridComponent<T>::computeLayout()
{
	mLayoutDirty = false;

	// Band mode: keep each band's horizontal scroll across a recomputation
	std::unordered_map<std::string, int> oldScroll;
	if (hasBands())
		for (const auto& g : mGroups)
			oldScroll[g.label] = g.scrollCol;

	mGroups.clear();
	mCells.clear();
	mRowTop.clear();
	mRowReveal.clear();
	mRowFirst.clear();

	if (!hasGroups())
	{
		std::vector<std::string> noKeys;
		const bool changed = mSortBy.empty() ? restoreEntryOrder() : arrangeEntries(noKeys, false);

		if (changed)
		{
			// Without groups the camera relies on mStartPosition: restart from the beginning and realign on the cursor
			mStartPosition = 0;
			mCameraOffset = 0;
			onCursorChanged(CURSOR_STOPPED);
		}

		return;
	}

	// Group keys, then physical grouping of entries with the same key
	std::vector<std::string> keys(mEntries.size());
	for (int i = 0; i < (int)mEntries.size(); i++)
		keys[i] = getGroupKey(mEntries[i]);

	const bool reordered = arrangeEntries(keys, true);
	if (reordered)
	{
		mGroupCamera = 0;
		mCameraOffset = 0;
	}

	const bool bands = hasBands();
	const int dimOpposite = bands ? INT_MAX : Math::max(1, mGridDimension.x()); // in bands mode, a single row per group
	const Vector2f tileDistance = mTileSize + mMargin;

	mCells.assign(mEntries.size(), Vector2i::Zero());

	int col = 0;
	float y = 0;
	bool first = true;
	bool groupStart = false;
	std::string lastKey;

	auto closeRow = [&]()
	{
		if (col != 0)
		{
			col = 0;
			y += tileDistance.y();
		}
	};

	for (int i = 0; i < (int)mEntries.size(); i++)
	{
		const std::string& key = keys[i];
		if (first || key != lastKey)
		{
			closeRow(); // finishes the last (partial) row of the previous group

			if (!first)
				y += mGroupSpacing;

			ImageGridGroup group;
			group.label = key;
			group.firstIndex = i;
			group.y = y;

			if (bands)
			{
				auto it = oldScroll.find(key);
				if (it != oldScroll.end())
					group.scrollCol = it->second;
			}

			mGroups.push_back(group);

			y += mGroupHeaderHeight;

			lastKey = key;
			first = false;
			groupStart = true;
		}

		mGroups.back().count++;

		if (col == 0)
		{
			mRowTop.push_back(y);
			mRowReveal.push_back(groupStart ? mGroups.back().y : y);
			mRowFirst.push_back(i);
			groupStart = false;
		}

		mCells[i] = Vector2i(col, (int)mRowTop.size() - 1);

		if (++col == dimOpposite)
			closeRow();
	}

	closeRow();

	// Same semantics as the historical calculation: top of the second-to-last row
	mTotalHeight = mRowTop.empty() ? 0.0f : Math::max(0.0f, mRowTop.back() - tileDistance.y());

	if (bands)
	{
		const int visibleCols = Math::max(1, mGridDimension.x());
		for (auto& g : mGroups)
		{
			g.scrollCol = Math::max(0, Math::min(g.scrollCol, Math::max(0, g.count - visibleCols)));
			g.scrollX = g.scrollCol * tileDistance.x();
		}

		scrollBandToCursor(false);
	}

	// The cursor may have changed row: realign the camera (mLayoutDirty is already false, no recursion)
	if (reordered)
		mCameraOffset = getGroupedCameraOffset();
}

// Target camera (NEVER mode): keeps the selected tile fully visible,
// and the group header when on its first row
template<typename T>
float ImageGridComponent<T>::getGroupedCameraOffset()
{
	if (mLayoutDirty)
		computeLayout();

	if (mCursor < 0 || mCursor >= (int)mCells.size())
		return mGroupCamera;

	const int row = mCells[mCursor].y();
	const float viewHeight = mSize.y() - mPadding.y() - mPadding.w();

	const float top = mRowReveal[row];
	const float bottom = mRowTop[row] + mTileSize.y();

	float camera = mGroupCamera;

	if (top < camera)
		camera = top;
	else if (bottom > camera + viewHeight)
		camera = bottom - viewHeight;

	mGroupCamera = Math::max(0.0f, camera);
	return mGroupCamera;
}

// Vertical movement in rows: same column, or last tile if the target row is partial
// Band mode: movement starting from entry "from". axis 1 = horizontal (bounded to the band),
// axis 2 = vertical (step bands, keeping the same visual column)
template<typename T>
int ImageGridComponent<T>::remapBandMove(int from, int axis, int step)
{
	if (from < 0 || from >= (int)mCells.size() || mGroups.empty())
		return from;

	const Vector2i cell = mCells[from];
	const ImageGridGroup& current = mGroups[cell.y()];

	if (axis == 1)
		return Math::max(current.firstIndex, Math::min(from + step, current.firstIndex + current.count - 1));

	const int visibleCols = Math::max(1, mGridDimension.x());

	int slot = (mWantedSlotCursor == from && mWantedSlot >= 0) ? mWantedSlot : cell.x() - current.scrollCol;
	slot = Math::max(0, Math::min(slot, visibleCols - 1));

	const int band = Math::max(0, Math::min(cell.y() + step, (int)mGroups.size() - 1));
	if (band == cell.y())
		return from;

	const ImageGridGroup& target = mGroups[band];
	const int col = Math::max(0, Math::min(target.scrollCol + slot, target.count - 1));

	mWantedSlot = slot;
	mWantedSlotCursor = target.firstIndex + col;

	return mWantedSlotCursor;
}

// Band mode: keeps the selected tile within the horizontal window of its band
template<typename T>
void ImageGridComponent<T>::scrollBandToCursor(bool animate)
{
	if (!hasBands() || mCursor < 0 || mCursor >= (int)mCells.size() || mGroups.empty())
		return;

	const int band = mCells[mCursor].y();
	const int col = mCells[mCursor].x();
	ImageGridGroup& group = mGroups[band];

	const int visibleCols = Math::max(1, mGridDimension.x());
	const float distance = mTileSize.x() + mMargin.x();

	int target = group.scrollCol;
	if (col < target)
		target = col;
	else if (col >= target + visibleCols)
		target = col - visibleCols + 1;

	target = Math::max(0, Math::min(target, Math::max(0, group.count - visibleCols)));
	group.scrollCol = target;

	const float targetX = target * distance;
	if (group.scrollX == targetX)
		return;

	animateBandTo(band, targetX, animate);
}

// Brings the band to its target offset (animated or not)
template<typename T>
void ImageGridComponent<T>::animateBandTo(int band, float targetX, bool animate)
{
	if (band < 0 || band >= (int)mGroups.size())
		return;

	ImageGridGroup& group = mGroups[band];
	const float distance = mTileSize.x() + mMargin.x();

	// Only one band animates at a time: the others are snapped to their target
	for (int b = 0; b < (int)mGroups.size(); b++)
		if (b != band)
			mGroups[b].scrollX = mGroups[b].scrollCol * distance;

	if (!animate || group.scrollX == targetX)
	{
		group.scrollX = targetX;
		mEntriesDirty = true;
		return;
	}

	const float fromX = group.scrollX;

	auto func = [this, band, fromX, targetX](float t)
	{
		if (band >= (int)mGroups.size())
			return;

		t -= 1; // cubic ease out
		float pct = Math::lerp(0, 1, t*t*t + 1);

		mGroups[band].scrollX = targetX * pct + fromX * (1.0 - pct);
		mEntriesDirty = true;
	};

	((GuiComponent*)this)->setAnimation(new LambdaAnimation(func, 200), 0, [this, band, targetX] {
		if (band < (int)mGroups.size())
			mGroups[band].scrollX = targetX;

		mEntriesDirty = true;
	}, false, 3);
}

// Band located under a screen y coordinate (mouse)
template<typename T>
int ImageGridComponent<T>::getBandAtScreenY(int y)
{
	if (mGroups.empty())
		return -1;

	const float localY = (y - mScreenY) / (mScreenScaleY > 0 ? mScreenScaleY : 1.0f);
	const float worldY = localY + mCameraOffset - mPadding.y(); // same reference frame as group.y

	int band = 0;
	for (int b = 0; b < (int)mGroups.size(); b++)
	{
		if (mGroups[b].y > worldY)
			break;

		band = b;
	}

	return band;
}

// Horizontal drag: the band follows the finger / mouse, bounded at its ends
template<typename T>
void ImageGridComponent<T>::dragBand(int dx)
{
	if (mDragBand < 0 || mDragBand >= (int)mGroups.size())
		return;

	ImageGridGroup& group = mGroups[mDragBand];

	const float distance = mTileSize.x() + mMargin.x();
	const int visibleCols = Math::max(1, mGridDimension.x());
	const float maxX = Math::max(0, group.count - visibleCols) * distance;

	group.scrollX = Math::max(0.0f, Math::min(group.scrollX + dx, maxX));
	mEntriesDirty = true;
}

// End of horizontal drag: snaps the band to the nearest column, and brings the selection back into the window
template<typename T>
void ImageGridComponent<T>::endBandDrag()
{
	if (mDragBand < 0 || mDragBand >= (int)mGroups.size())
		return;

	ImageGridGroup& group = mGroups[mDragBand];

	const float distance = mTileSize.x() + mMargin.x();
	const int visibleCols = Math::max(1, mGridDimension.x());
	const int maxScroll = Math::max(0, group.count - visibleCols);

	int col = (int)(group.scrollX / distance + 0.5f);
	col = Math::max(0, Math::min(col, maxScroll));

	group.scrollCol = col;
	animateBandTo(mDragBand, col * distance, ALLOWANIMATIONS);

	if (mCursor >= 0 && mCursor < (int)mCells.size() && mCells[mCursor].y() == mDragBand)
	{
		const int cursorCol = mCells[mCursor].x();
		const int clamped = Math::max(col, Math::min(cursorCol, Math::min(col + visibleCols - 1, group.count - 1)));

		if (clamped != cursorCol)
		{
			mCursor = group.firstIndex + clamped;
			onCursorChanged(CURSOR_STOPPED);
		}
	}
}

// Band mode: is the tile within the horizontal window (+ margin) of its band? If not, no need to create it
template<typename T>
bool ImageGridComponent<T>::isInBandWindow(int i)
{
	if (!hasBands() || i < 0 || i >= (int)mCells.size() || mCells[i].y() >= (int)mGroups.size())
		return true;

	const float distance = mTileSize.x() + mMargin.x();
	const ImageGridGroup& group = mGroups[mCells[i].y()];

	const float x = mCells[i].x() * distance;
	const float viewWidth = mSize.x();

	return x + mTileSize.x() >= group.scrollX - EXTRAITEMS * distance && x <= group.scrollX + viewWidth + EXTRAITEMS * distance;
}

// Band mode: horizontal offset of the band containing this tile (found by its vertical position)
template<typename T>
float ImageGridComponent<T>::getBandScrollX(const std::shared_ptr<GridTileComponent>& tile)
{
	if (mGroups.empty() || mRowTop.empty())
		return 0;

	const float rowTop = tile->getPosition().y() - mTileSize.y() / 2 - mPadding.y();
	const float half = (mTileSize.y() + mMargin.y()) / 2;

	int band = (int)(std::upper_bound(mRowTop.begin(), mRowTop.end(), rowTop + half) - mRowTop.begin()) - 1;
	band = Math::max(0, Math::min(band, (int)mGroups.size() - 1));

	return mGroups[band].scrollX;
}

template<typename T>
int ImageGridComponent<T>::moveByRows(int rows)
{
	if (mLayoutDirty)
		computeLayout();

	if (hasBands())
		return remapBandMove(mCursor, 2, rows);

	if (mCursor < 0 || mCursor >= (int)mCells.size() || mRowTop.empty())
		return mCursor;

	const Vector2i cell = mCells[mCursor];
	const int row = Math::max(0, Math::min(cell.y() + rows, (int)mRowTop.size() - 1));

	const int first = mRowFirst[row];
	const int last = (row + 1 < (int)mRowFirst.size() ? mRowFirst[row + 1] : (int)mEntries.size()) - 1;

	return Math::min(first + cell.x(), last);
}

template<typename T>
void ImageGridComponent<T>::ensureGroupText(ImageGridGroup& group)
{
	group.text = std::make_shared<TextComponent>(mWindow);
	group.text->setVerticalAlignment(ALIGN_CENTER); // default, overridable by the theme

	if (mTheme)
		group.text->applyTheme(mTheme, mName, "gridgroup", ThemeFlags::ALL & ~(ThemeFlags::POSITION | ThemeFlags::SIZE));

	group.text->setPosition(mPadding.x(), group.y + mPadding.y());
	group.text->setSize(mSize.x() - mPadding.x() - mPadding.z(), mGroupHeaderHeight);
	group.text->setText(group.label);
}

template<typename T>
Vector2i ImageGridComponent<T>::getVisibleRange()
{
	if (hasGroups())
	{
		if (mLayoutDirty)
			computeLayout();

		if (mRowTop.empty())
			return Vector2i(0, -1);

		const Vector2f tileDistance = mTileSize + mMargin;

		const float top = mCameraOffset - EXTRAITEMS * tileDistance.y() - mTileSize.y();
		const float bottom = mCameraOffset + mSize.y() + EXTRAITEMS * tileDistance.y();

		const int r0 = (int)(std::lower_bound(mRowTop.begin(), mRowTop.end(), top) - mRowTop.begin());
		const int r1 = (int)(std::upper_bound(mRowTop.begin(), mRowTop.end(), bottom) - mRowTop.begin());

		const int start = r0 < (int)mRowFirst.size() ? mRowFirst[r0] : (int)mEntries.size();
		const int end = r1 < (int)mRowFirst.size() ? mRowFirst[r1] - 1 : (int)mEntries.size() - 1;

		return Vector2i(start, end);
	}

	int dimExt = isVertical() ? mGridDimension.y() : mGridDimension.x();
	int dimOpposite = Math::max(1, isVertical() ? mGridDimension.x() : mGridDimension.y());

	Vector2f tileDistance = mTileSize + mMargin;

	int position = mCameraOffset / (isVertical() ? tileDistance.y() : tileDistance.x()) * dimOpposite;

	int startIndex = position - EXTRAITEMS * dimOpposite;
	int endIndex = startIndex + dimExt * dimOpposite;

	return Vector2i(startIndex, endIndex);
}

template<typename T>
std::shared_ptr<GridTileComponent> ImageGridComponent<T>::createTile(int i, int dimOpposite, Vector2f tileDistance, Vector2f startPosition)
{
	// Create tiles
	auto tile = std::make_shared<GridTileComponent>(mWindow);

	int X = i % (int)dimOpposite;
	int Y = i / (int)dimOpposite;

	if (i < 0)
	{
		X = (i + mEntries.size() - 1) % (int)dimOpposite;
		Y = ((i + mEntries.size() - 1) / (int)dimOpposite) - ((mEntries.size() - 1) / (int)dimOpposite);
	}

	if (!isVertical())
		std::swap(X, Y);

	tile->setOrigin(0.5f, 0.5f);
	if (hasGroups() && i >= 0 && i < (int)mCells.size())
		tile->setPosition(mCells[i].x() * tileDistance.x() + startPosition.x(), mRowTop[mCells[i].y()] + startPosition.y());
	else
		tile->setPosition(X * tileDistance.x() + startPosition.x(), Y * tileDistance.y() + startPosition.y());
	tile->setSize(mTileSize);

	if (mTheme)
		tile->applyTheme(mTheme, mName, "gridtile", ThemeFlags::ALL);

	if (mAutoLayout.x() != 0 && mAutoLayout.y() != 0)
		tile->forceSize(mTileSize, mAutoLayoutZoom);

	return tile;
}

template<typename T>
void ImageGridComponent<T>::preloadTiles()
{
	if (mLayoutDirty)
		computeLayout();

	int dimOpposite = Math::max(1, isVertical() ? mGridDimension.x() : mGridDimension.y());

	Vector2f startPosition = mTileSize / 2;
	startPosition += Vector2f(mPadding.x(), mPadding.y());

	Vector2f tileDistance = mTileSize + mMargin;

	for (int i = 0; i < mEntries.size(); i++)
	{
		typename IList<ImageGridData, T>::Entry& entry = mEntries[i];
		if (entry.data.tile != nullptr)
			continue;

		auto tile = createTile(i, dimOpposite, tileDistance, startPosition);
		tile->setVisible(false);

		loadTile(tile, entry);
		
		entry.data.tile = tile;
	}
}

template<typename T>
void ImageGridComponent<T>::ensureVisibleTileExist()
{
	if (mEntries.size() == 0 || mGridDimension.y() == 0 || mGridDimension.x() == 0)
		return;

	int dimOpposite = Math::max(1, isVertical() ? mGridDimension.x() : mGridDimension.y());

	Vector2f tileDistance = mTileSize + mMargin;

	if (mLayoutDirty)
		computeLayout();

	auto range = getVisibleRange();
	int startIndex = range.x(); // position - EXTRAITEMS * dimOpposite;
	int endIndex = range.y(); // startIndex + dimExt * dimOpposite;

	if (startIndex < 0)
		startIndex = 0;

	if (endIndex > mEntries.size() - 1)
		endIndex = mEntries.size() - 1;

	if (!hasGroups()) // with groups, mTotalHeight is computed by computeLayout()
	{
		int totalCols = Math::max(0, mEntries.size() - 1) / dimOpposite;
		mTotalHeight = Math::max(0.0f, (float) (isVertical() ? tileDistance.y() : tileDistance.x()) * (totalCols - 1));
	}

	Vector2f startPosition = mTileSize / 2;
	startPosition += Vector2f(mPadding.x(), mPadding.y());

	auto oldScrollLoopTiles = mScrollLoopTiles;	
	mScrollLoopTiles.clear();

	int from = mScrollLoop ? Math::min(range.x(), 0) : 0;
	int to = mScrollLoop ? Math::max(range.y(), mEntries.size()) : mEntries.size();

	for (int idx = from; idx < to; idx++)
	{
		int i = idx;

		if (mScrollLoop)
		{
			int min = dimOpposite == 1 ? 0 : (mEntries.size() % dimOpposite) - dimOpposite;
			int max = mEntries.size() - min;

			if (i < min)
			{
				while (i < min)
					i += max;
			}
			else if (i >= max)
			{
				while (i >= max)
					i -= max;
			}

			if (i < 0 || i >= mEntries.size())
				continue;
		}

		typename IList<ImageGridData, T>::Entry& entry = mEntries[i];

		if ((mScrollLoop && idx >= range.x() && idx <= range.y()) || (!mScrollLoop && i >= startIndex && i <= endIndex && isInBandWindow(i)))
		{
			if (entry.data.tile == nullptr)
			{
				// Create tiles
				auto tile = createTile(i, dimOpposite, tileDistance, startPosition);
				loadTile(tile, entry);

				entry.data.tile = tile;

				if (tile->isVisible())
					mVisibleTiles.push_back(tile);

				if (mCursor == i)
				{
					auto curTile = getSelectedTile();
					while (curTile != nullptr)
					{
						curTile->setSelected(false, false, nullptr, true);
						curTile = getSelectedTile();
					}

					mLastCursor = mCursor;
					tile->setSelected(true, true, nullptr, true);
				}

				if (mShowing)
					tile->onShow();
			}
			else if (!entry.data.tile->isVisible())
			{
				loadTile(entry.data.tile, entry);
				entry.data.tile->setVisible(true);

				mVisibleTiles.push_back(entry.data.tile);

				if (mShowing)
					entry.data.tile->onShow();
			}

			if (mScrollLoop && (i < startIndex || i > endIndex))
			{
				auto tile = createTile(idx, dimOpposite, tileDistance, startPosition);
				loadTile(tile, entry);
				mScrollLoopTiles[idx] = tile;
			}
		}
		else if (entry.data.tile != nullptr)
		{
			if (entry.data.tile->isVisible())
				entry.data.tile->setVisible(false);

			auto it = std::find(mVisibleTiles.cbegin(), mVisibleTiles.cend(), entry.data.tile);
			if (it != mVisibleTiles.cend())
				mVisibleTiles.erase(it);

			if (!mShowing)
			{				
				entry.data.tile->resetImages();
				entry.data.tile = nullptr;
			}
			else if (entry.data.tile->isShowing())
				entry.data.tile->onHide();
		}
	}
}

template<typename T>
ImageGridComponent<T>::ImageGridComponent(Window* window) : IList<ImageGridData, T>(window), mScrollbar(window)
{
	Vector2f screen = Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	mCameraOffset = 0;
	mTotalHeight = 0;

	mGroupHeaderHeight = 0;
	mGroupSpacing = 0;
	mGroupCamera = 0;
	mLayoutDirty = true;
	mGroupSortByLabel = true;
	mSortDescending = false;
	mEntriesReordered = false;
	mNextSourceIndex = 0;
	mGroupBands = false;
	mBandAxis = 0;
	mBandStep = 1;
	mWantedSlot = -1;
	mWantedSlotCursor = -1;
	mDragAxis = 0;
	mDragBand = -1;
	mScreenY = 0;
	mScreenScaleY = 1;

	mPressedCursor = -1;
	mPressedPoint = Vector2i(-1, -1);
	mIsDragging = false;
	mTimeHoldingButton = -1;

	mGridSizeOverride = Vector2f::Zero();
	mAutoLayout = Vector2f::Zero();
	mAutoLayoutZoom = 1.0;

	mVideoDelay = 750;
	mAllowVideo = false;
	mName = "grid";
	mStartPosition = 0;
	mEntriesDirty = true;

	mLastCursor = -1;
	mLastCursorState = CursorState::CURSOR_STOPPED;

	mDefaultGameTexture = ":/cartridge.svg";
	mDefaultFolderTexture = ":/folder.svg";
	mDefaultLogoBackgroundTexture = "";

	mAnimateSelection = true;
	mSize = screen * 0.80f;
	mMargin = screen * 0.07f;
	mPadding = Vector4f::Zero();
	mTileSize = GridTileComponent::getDefaultTileSize();

	mImageSource = THUMBNAIL;

	mCenterSelection = CenterSelection::NEVER;
	mScrollLoop = false;
	mScrollDirection = SCROLL_VERTICALLY;

	mCursorChangedCallback = nullptr;
}

template<typename T>
void ImageGridComponent<T>::add(const std::string& name, const std::string& imagePath, const T& obj)
{
	typename IList<ImageGridData, T>::Entry entry;
	entry.name = name;
	entry.object = obj;
	entry.data.texturePath = imagePath;
	entry.data.sourceIndex = mNextSourceIndex++;

	static_cast<IList< ImageGridData, T >*>(this)->add(entry);

	mEntriesDirty = true;
	mLayoutDirty = true;
}

template<typename T>
void ImageGridComponent<T>::clear()
{	
	IList<ImageGridData, T>::clear();
	mNextSourceIndex = 0;
	mEntriesReordered = false;
	resetGrid();
}

template<typename T>
std::string ImageGridComponent<T>::getImage(const T& obj)
{
	IList<ImageGridData, T>* list = static_cast<IList< ImageGridData, T >*>(this);
	auto entry = list->findEntry(obj);
	if (entry != list->end())
		return (*entry).data.texturePath;

	return "";
}

template<typename T>
void ImageGridComponent<T>::setImage(const std::string& imagePath, const T& obj, bool force)
{
	IList<ImageGridData, T>* list = static_cast<IList< ImageGridData, T >*>(this);
	auto entry = list->findEntry(obj);
	if (entry != list->end() && (force || (*entry).data.texturePath != imagePath))
	{
		(*entry).data.texturePath = imagePath;

		// The tile is made again (with its selected state) at the next update. The old one leaves the
		// visible tiles: left there, it was drawn over the new one (the selected tile is drawn last)
		// and never freed.
		if ((*entry).data.tile != nullptr)
		{
			auto it = std::find(mVisibleTiles.cbegin(), mVisibleTiles.cend(), (*entry).data.tile);
			if (it != mVisibleTiles.cend())
				mVisibleTiles.erase(it);

			(*entry).data.tile = nullptr;
		}

		mEntriesDirty = true;
	}
}

template<typename T>
bool ImageGridComponent<T>::input(InputConfig* config, Input input)
{
	if (input.value != 0)
	{
		int idx = isVertical() ? 0 : 1;
		float dimScrollable = isVertical() ? mGridDimension.y() - 2 * EXTRAITEMS : mGridDimension.x() - 2 * EXTRAITEMS;

		Vector2i dir = Vector2i::Zero();

		if (config->isMappedLike("up", input))
			dir[1 ^ idx] = -1;
		else if (config->isMappedLike("down", input))
			dir[1 ^ idx] = 1;
		else if (config->isMappedLike("left", input))
			dir[0 ^ idx] = -1;
		else if (config->isMappedLike("right", input))
			dir[0 ^ idx] = 1;
		else  if (config->isMappedTo("pageup", input))
		{
			if (isVertical())
				dir[1 ^ idx] = -dimScrollable;
			else
				dir[0 ^ idx] = -dimScrollable;
		}
		else if (config->isMappedTo("pagedown", input))
		{
			if (isVertical())
				dir[1 ^ idx] = dimScrollable;
			else
				dir[0 ^ idx] = dimScrollable;
		}

		if (dir != Vector2i::Zero())
		{
			if (hasBands())
			{
				// Speed ±1: IList replays this step on every tick, onCursorChanged converts it
				// (bounded to the band horizontally, band jump vertically)
				mBandAxis = (dir.y() != 0) ? 2 : 1;

				int amount = (dir.y() != 0) ? dir.y() : dir.x();
				mBandStep = Math::max(1, std::abs(amount));

				listInput(amount < 0 ? -1 : 1);
			}
			else if (hasGroups())
			{
				// Partial rows at the end of a group break index arithmetic: we navigate by rows
				int delta = dir.x();
				if (dir.y() != 0)
				{
					int target = moveByRows(dir.y()); // may reorder entries: mCursor is read after the call
					delta = target - mCursor;
				}

				if (delta != 0)
					listInput(delta);
			}
			else if (isVertical())
				listInput(dir.x() + dir.y() * mGridDimension.x());
			else
				listInput(dir.x() + dir.y() * mGridDimension.y());

			return true;
		}
	}
	else
	{
		if (config->isMappedLike("up", input) || config->isMappedLike("down", input) ||
			config->isMappedLike("left", input) || config->isMappedLike("right", input) ||
			config->isMappedTo("pagedown", input) || config->isMappedTo("pageup", input))
		{
			mBandAxis = 0;
			stopScrolling();
		}
	}

	return GuiComponent::input(config, input);
}

template<typename T>
void ImageGridComponent<T>::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mTimeHoldingButton >= 0)
	{
		mTimeHoldingButton += deltaTime;

		if (mTimeHoldingButton >= HOLD_TIME)
		{
			mTimeHoldingButton = -1;
			mPressedCursor = -1;
			mPressedPoint = Vector2i(-1, -1);

			longMouseClick.invoke([&](ILongMouseClickEvent* c) { c->onLongMouseClick(this); });
		}
	}

	mScrollbar.update(deltaTime);

	listUpdate(deltaTime);

	if (mEntriesDirty)
	{
		ensureVisibleTileExist();
		mEntriesDirty = false;
	}

	for (auto tile : mVisibleTiles)
		tile->update(deltaTime);
	/*
	for (auto entry : mEntries)
		if (entry.data.tile != nullptr && entry.data.tile->isVisible())
			entry.data.tile->update(deltaTime);
	*/
}

template<typename T>
void ImageGridComponent<T>::topWindow(bool isTop)
{
	GuiComponent::topWindow(isTop);

	for (auto entry : mEntries)
		if (entry.data.tile != nullptr)
			entry.data.tile->topWindow(isTop);
}

template<typename T>
void ImageGridComponent<T>::setOpacity(unsigned char opacity)
{
	GuiComponent::setOpacity(opacity);

	for (auto entry : mEntries)
		if (entry.data.tile != nullptr)
			entry.data.tile->setOpacity(opacity);
}

template<typename T>
void ImageGridComponent<T>::onShow()
{
	GuiComponent::onShow();

	for (auto entry : mEntries)
		if (entry.data.tile != nullptr)
			entry.data.tile->onShow();
}

template<typename T>
void ImageGridComponent<T>::onHide()
{
	GuiComponent::onHide();

	for (auto entry : mEntries)
		if (entry.data.tile != nullptr)
			entry.data.tile->onHide();

	ensureVisibleTileExist();
	mEntriesDirty = false;
}

template<typename T>
void ImageGridComponent<T>::onScreenSaverActivate()
{
	GuiComponent::onScreenSaverActivate();

	for (auto entry : mEntries)
		if (entry.data.tile != nullptr)
			entry.data.tile->onScreenSaverActivate();
}

template<typename T>
void ImageGridComponent<T>::onScreenSaverDeactivate()
{
	GuiComponent::onScreenSaverDeactivate();

	for (auto entry : mEntries)
		if (entry.data.tile != nullptr)
			entry.data.tile->onScreenSaverDeactivate();
}


template<typename T>
void ImageGridComponent<T>::setGridSizeOverride(Vector2f size)
{
	mGridSizeOverride = size;
}

template<typename T>
std::shared_ptr<GridTileComponent> ImageGridComponent<T>::getSelectedTile()
{
	for (auto tile : mVisibleTiles)
		if (tile->isSelected())
			return tile;

	for (auto entry : mEntries)
		if (entry.data.tile != nullptr)
			if (entry.data.tile->isSelected())
				return entry.data.tile;

	return nullptr;
}

template<typename T>
void ImageGridComponent<T>::render(const Transform4x4f& parentTrans)
{
	if (!mVisible)
		return;

	Transform4x4f trans = parentTrans * getTransform();
	Transform4x4f tileTrans = trans;

	auto rect = Renderer::getScreenRect(trans, mSize);
	if (!Renderer::isVisibleOnScreen(rect))
		return;

	mScreenY = rect.y;
	mScreenScaleY = mSize.y() > 0 ? (float)rect.h / mSize.y() : 1.0f;

	if (Settings::DebugGrid())
	{
		Renderer::setMatrix(trans);
		Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), 0xFF000033);

		for (auto tile : mVisibleTiles)
		{
			auto tt = trans * tile->getTransform();
			tt.translate(isVertical() ? 0 : -mCameraOffset, isVertical() ? -mCameraOffset : 0);

			Renderer::setMatrix(tt);
			Renderer::drawRect(0.0, 0.0, tile->getSize().x(), tile->getSize().y(), 0x00FF0033);
		}

		Renderer::setMatrix(parentTrans);
	}

	bool splittedRendering = (mAnimateSelection && mAutoLayout.x() != 0);

	Vector2i pos(rect.x, rect.y);
	Vector2i size(rect.w, rect.h);

	if (mAutoLayout == Vector2f::Zero() && mLastRowPartial) // If the last row is partial in Retropie, extend clip to show the entire last row 
		size.y() = (mTileSize + mMargin).y() * (mGridDimension.y() - 2 * EXTRAITEMS);

	Renderer::pushClipRect(pos, size);

	float dimScrollable = isVertical() ? mGridDimension.y() - 2 * EXTRAITEMS : mGridDimension.x() - 2 * EXTRAITEMS;
	float dimOpposite = Math::max(1, isVertical() ? mGridDimension.x() : mGridDimension.y());
	
	tileTrans.translate(isVertical() ? 0 : -mCameraOffset, isVertical() ? -mCameraOffset : 0);

	std::shared_ptr<GridTileComponent> selectedTile;

	for (auto scrollLoopTile : mScrollLoopTiles)
		scrollLoopTile.second->render(tileTrans);

	// Group labels (styled by <text name="gridgroup">), under the tiles so that the zoomed selected tile passes over them
	if (hasGroups())
	{
		for (auto& group : mGroups)
		{
			float groupTop = group.y + mPadding.y();
			if (groupTop + mGroupHeaderHeight < mCameraOffset || groupTop > mCameraOffset + mSize.y())
				continue;

			if (group.text == nullptr)
				ensureGroupText(group);

			group.text->render(tileTrans);
		}
	}

	// Band mode: each band has its own horizontal offset
	const bool bands = hasBands();
	auto tileTransFor = [&](const std::shared_ptr<GridTileComponent>& tile) -> Transform4x4f
	{
		Transform4x4f t = tileTrans;
		if (bands)
			t.translate(-getBandScrollX(tile), 0.0f);

		return t;
	};

	for (auto tile : mVisibleTiles)
	{
		if (tile->isSelected())
		{
			selectedTile = tile;
			if (splittedRendering && tile->shouldSplitRendering())
				tile->renderBackground(tileTransFor(tile));
		}
		else
			tile->render(tileTransFor(tile));
	}

	// Render the selected image content on top of the others
	if (selectedTile != nullptr)
	{
		if (splittedRendering && selectedTile->shouldSplitRendering())
			selectedTile->renderContent(tileTransFor(selectedTile));
		else
			selectedTile->render(tileTransFor(selectedTile));
	}

	Renderer::popClipRect();

	listRenderTitleOverlay(trans);
	GuiComponent::renderChildren(trans);

	if (mScrollbar.isEnabled() && !mScrollLoop)
	{
		Vector3f pos = GuiComponent::getPosition();
		Vector2f sz = GuiComponent::getSize();

		if (isVertical())
		{
			pos.y() += mPadding.y();
			sz.y() -= mPadding.y() + mPadding.w();
		}
		else
		{
			pos.x() += mPadding.x();
			sz.x() -= mPadding.x() + mPadding.z();
		}

		auto scrollableSize = mTileSize + mMargin;

		mScrollbar.setContainerBounds(pos, sz, isVertical());

		float ss = isVertical() ? scrollableSize.y() : scrollableSize.x();
		mScrollbar.setRange(0, mTotalHeight + ss, ss);
		mScrollbar.setScrollPosition(mCameraOffset);
		mScrollbar.render(parentTrans);
	}
}

template<typename T>
void ImageGridComponent<T>::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	// Keep the theme pointer to apply it on the tiles later on
	mTheme = nullptr;

	// Apply theme to GuiComponent but not size property, which will be applied at the end of this function
	GuiComponent::applyTheme(theme, view, element, properties ^ ThemeFlags::SIZE);

	Vector2f screen = Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	mScrollbar.fromTheme(theme, view, element, "imagegrid");

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "imagegrid");
	if (elem)
	{
		if (elem->has("margin"))
		{
			auto margin = elem->get<Vector2f>("margin");
			if (abs(margin.x()) < 1 && abs(margin.y()) < 1)
				mMargin = margin * screen;
			else
				mMargin = margin; // Pixel size
		}

		if (elem->has("padding"))
		{
			auto padding = elem->get<Vector4f>("padding");
			if (abs(padding.x()) < 1 && abs(padding.y()) < 1 && abs(padding.z()) < 1 && abs(padding.w()) < 1)
				mPadding = padding * Vector4f(screen.x(), screen.y(), screen.x(), screen.y());
			else 
				mPadding = padding; // Pixel size
		}

		if (elem->has("autoLayout"))
		{
			mAutoLayout = elem->get<Vector2f>("autoLayout");

			float screenProportion = (float)Renderer::getScreenWidth() / (float)Renderer::getScreenHeight();

			if (properties & ThemeFlags::SIZE && elem->has("size"))
			{
				Vector2f scale = GuiComponent::getParent() ? GuiComponent::getParent()->getSize() : Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
				auto size = elem->get<Vector2f>("size") * scale;
				if (size.y() != 0)
					screenProportion = size.x() / size.y();
			}

			float cellProportion = 1;
			if (elem->has("cellProportion"))
			{
				cellProportion = elem->get<float>("cellProportion");
				if (cellProportion == 0)
					cellProportion = 1;
			}

			if (mAutoLayout.y() == 0)
				mAutoLayout.y() = Math::round(mAutoLayout.x() / screenProportion * cellProportion);
			else if (mAutoLayout.x() == 0)
				mAutoLayout.x() = Math::round(mAutoLayout.y() * screenProportion / cellProportion);
		}

		if (elem->has("animateSelection"))
			mAnimateSelection = elem->get<bool>("animateSelection");

		if (elem->has("autoLayoutSelectedZoom"))
			mAutoLayoutZoom = elem->get<float>("autoLayoutSelectedZoom");

		if (elem->has("imageSource"))
		{
			auto direction = elem->get<std::string>("imageSource");
			if (direction == "image")
				mImageSource = IMAGE;
			else if (direction == "marquee")
				mImageSource = MARQUEE;
			else if (direction == "marqueeortext")
				mImageSource = MARQUEEORTEXT;
			else if (direction == "fanart")
				mImageSource = FANART;
			else if (direction == "titleshot")
				mImageSource = TITLESHOT;
			else if (direction == "boxart")
				mImageSource = BOXART;
			else if (direction == "cartridge")
				mImageSource = CARTRIDGE;
			else if (direction == "boxback")
				mImageSource = BOXBACK;
			else if (direction == "mix")
				mImageSource = MIX;
			else
				mImageSource = THUMBNAIL;
		}
		else
			mImageSource = THUMBNAIL;

		if (elem->has("scrollDirection"))
		{
			auto direction = elem->get<std::string>("scrollDirection");
			if (direction == "horizontal")
			{
				mCenterSelection = CenterSelection::PARTIAL;
				mScrollDirection = SCROLL_HORIZONTALLY;
			}
			else if (direction == "horizontalCenter")
			{
				mCenterSelection = CenterSelection::FULL;
				mScrollDirection = SCROLL_HORIZONTALLY;
			}
			else if (direction == "verticalCenter")
			{
				mCenterSelection = CenterSelection::FULL;
				mScrollDirection = SCROLL_VERTICALLY;
			}
			else
			{
				mCenterSelection = CenterSelection::NEVER;
				mScrollDirection = SCROLL_VERTICALLY;
			}
		}

		if (elem->has("showVideoAtDelay"))
		{
			mVideoDelay = elem->get<float>("showVideoAtDelay");
			mAllowVideo = (mVideoDelay >= 0);
		}
		else
			mAllowVideo = false;

		if (elem->has("centerSelection"))
		{
			if (!(elem->get<std::string>("centerSelection").compare("true")))
				mCenterSelection = CenterSelection::FULL;
			else if (!(elem->get<std::string>("centerSelection").compare("partial")))
				mCenterSelection = CenterSelection::PARTIAL;
			else
				mCenterSelection = CenterSelection::NEVER;
		}

		if (mCenterSelection != CenterSelection::NEVER && elem->has("scrollLoop"))
			mScrollLoop = (elem->get<bool>("scrollLoop"));
		else
			mScrollLoop = false;

		if (elem->has("gameImage"))
		{
			std::string path = elem->get<std::string>("gameImage");

			if (!ResourceManager::getInstance()->fileExists(path))
				LOG(LogWarning) << "Could not replace default game image, check path: " << path;
			else
			{
				std::string oldDefaultGameTexture = mDefaultGameTexture;
				mDefaultGameTexture = path;

				// mEntries are already loaded at this point,
				// so we need to update them with new game image texture
				for (auto it = mEntries.begin(); it != mEntries.end(); it++)
				{
					if ((*it).data.texturePath == oldDefaultGameTexture)
						(*it).data.texturePath = mDefaultGameTexture;
				}
			}
		}

		if (elem->has("folderImage"))
		{
			std::string path = elem->get<std::string>("folderImage");

			if (!ResourceManager::getInstance()->fileExists(path))
				LOG(LogWarning) << "Could not replace default folder image, check path: " << path;
			else
			{
				std::string oldDefaultFolderTexture = mDefaultFolderTexture;
				mDefaultFolderTexture = path;

				// mEntries are already loaded at this point,
				// so we need to update them with new folder image texture
				for (auto it = mEntries.begin(); it != mEntries.end(); it++)
				{
					if ((*it).data.texturePath == oldDefaultFolderTexture)
						(*it).data.texturePath = mDefaultFolderTexture;
				}
			}
		}

		if (elem->has("logoBackgroundImage"))
		{
			std::string path = elem->get<std::string>("logoBackgroundImage");

			if (!ResourceManager::getInstance()->fileExists(path))
				LOG(LogWarning) << "Could not replace default folder image, check path: " << path;
			else
			{
				std::string oldDefaultFolderTexture = mDefaultLogoBackgroundTexture;
				mDefaultLogoBackgroundTexture = path;

				// mEntries are already loaded at this point,
				// so we need to update them with new folder image texture
				for (auto it = mEntries.begin(); it != mEntries.end(); it++)
					if ((*it).data.texturePath == oldDefaultFolderTexture)
						(*it).data.texturePath = mDefaultLogoBackgroundTexture;
			}
		}

		if (elem->has("scrollSound"))
			mScrollSound = elem->get<std::string>("scrollSound");

		if (elem->has("groupBy"))
			mGroupBy = elem->get<std::string>("groupBy");

		if (elem->has("groupHeight"))
		{
			float height = elem->get<float>("groupHeight");
			mGroupHeaderHeight = (abs(height) < 1) ? height * screen.y() : height; // same convention as margin
		}

		if (elem->has("groupSpacing"))
		{
			float spacing = elem->get<float>("groupSpacing");
			mGroupSpacing = (abs(spacing) < 1) ? spacing * screen.y() : spacing;
		}

		if (elem->has("groupBands"))
			mGroupBands = elem->get<bool>("groupBands");

		if (elem->has("sortDescending"))
			mSortDescending = elem->get<bool>("sortDescending");

		if (elem->has("groupsAtStart"))
			mGroupsAtStart = Utils::String::split(elem->get<std::string>("groupsAtStart"), ';', true);

		if (elem->has("groupsAtEnd"))
			mGroupsAtEnd = Utils::String::split(elem->get<std::string>("groupsAtEnd"), ';', true);

		if (elem->has("sortBy"))
			mSortBy = elem->get<std::string>("sortBy");

		mLayoutDirty = true;
	}

	// We still need to manually get the grid tile size here,
	// so we can recalculate the new grid dimension, and THEN (re)build the tiles
	elem = theme->getElement(view, "default", "gridtile");

	mTileSize = elem && elem->has("size") ?
		elem->get<Vector2f>("size") * screen :
		GridTileComponent::getDefaultTileSize();

	// Apply size property, will trigger a call to onSizeChanged() which will build the tiles
	GuiComponent::applyTheme(theme, view, element, ThemeFlags::SIZE | ThemeFlags::Z_INDEX);

	// Keep the theme pointer to apply it on the tiles later on
	mTheme = theme;

	// Trigger the call manually if the theme have no "imagegrid" element
	resetGrid();

	if (mEntries.size())
	{
		for (auto& entry : mEntries)
		{
			if (entry.data.tile != nullptr && entry.data.tile->isVisible())
				mVisibleTiles.push_back(entry.data.tile);
		}
	}
}

template<typename T>
void ImageGridComponent<T>::onSizeChanged()
{
	IList<ImageGridData, T>::onSizeChanged();

	if (mTheme == nullptr || mSize.x() <= 0 || mSize.y() <= 0)
		return;

	calcGridDimension();
	mEntriesDirty = true;
	mLayoutDirty = true;
}

template<typename T>
void ImageGridComponent<T>::onCursorChanged(const CursorState& state)
{
	if (mEntries.size() == 0)
		return;

	if (mLayoutDirty)
		computeLayout(); // may reorder entries and move the cursor

	// Keyboard navigation in band mode: IList moved the cursor by ±1 entry, we reinterpret this step
	if (hasBands() && mBandAxis != 0 && mLastCursor >= 0 && mLastCursor < (int)mEntries.size() && std::abs(mCursor - mLastCursor) == 1)
		mCursor = remapBandMove(mLastCursor, mBandAxis, (mCursor - mLastCursor) * mBandStep);

	if (mCursor == mLastCursor && state == mLastCursorState)
		return;

	mLastCursorState = state;

	if (mLastCursor == mCursor)
	{
		if (state == CURSOR_STOPPED && mCursorChangedCallback)
			mCursorChangedCallback(state);

		return;
	}

	mScrollbar.onCursorChanged();

	bool direction = mCursor >= mLastCursor;
	bool isScrollLooping = false;

	if (mScrollLoop)
	{
		int diff = direction ? mCursor - mLastCursor : mLastCursor - mCursor;
		if (mScrollLoop && diff == mEntries.size() - 1)
		{
			direction = !direction;
			isScrollLooping = true;
		}
	}

	int oldStart = mStartPosition;

	float dimScrollable = isVertical() ? mGridDimension.y() - 2 * EXTRAITEMS : mGridDimension.x() - 2 * EXTRAITEMS;
	float dimOpposite = Math::max(1, isVertical() ? mGridDimension.x() : mGridDimension.y());
	
	int centralCol = (int)(dimScrollable - 0.5) / 2;
	int maxCentralCol = (int)(dimScrollable) / 2;

	int oldCol = (mLastCursor / dimOpposite);
	int col = (mCursor / dimOpposite);

	int lastCol = ((mEntries.size() - 1) / dimOpposite);

	int lastScroll = std::max(0, (int)(lastCol + 1 - dimScrollable));

	float startPos = 0;
	float endPos = 1;

	Vector3f oldPos = Vector3f::Zero();

	if (mLastCursor >= 0 && mLastCursor < mEntries.size() && mEntries[mLastCursor].data.tile != nullptr)
	{
		if (ALLOWANIMATIONS && mAnimateSelection)
			oldPos = mEntries[mLastCursor].data.tile->getBackgroundPosition();
	}

	if (mCursor >= 0 && mCursor < mEntries.size() && mEntries[mCursor].data.tile != nullptr)
	{
		if (!mEntries[mCursor].data.tile->isSelected())
		{
			auto selectedTile = getSelectedTile();
			while (selectedTile != nullptr)
			{
				selectedTile->setSelected(false, true, nullptr, true);
				selectedTile = getSelectedTile();
			}

			if (!mAnimateSelection)
			{
				oldPos = Vector3f(0, 0);
				mEntries[mCursor].data.tile->setSelected(true, true, &oldPos, true);
			}
			else
				mEntries[mCursor].data.tile->setSelected(true, true, oldPos == Vector3f::Zero() ? nullptr : &oldPos, true);

			auto selected = mEntries[mCursor].data.tile;
			auto old = mLastCursor >= 0 && mLastCursor < mEntries.size() ? mEntries[mLastCursor].data.tile : nullptr;

			for (auto tile : mVisibleTiles)
				if (tile != selected && tile != old)
					if (tile->hasItemTemplate())
						tile->setSelected(false);
		}
		else if (mLastCursor == -9999)
			mEntries[mCursor].data.tile->startVideo();
	}

	int firstVisibleCol = mStartPosition / dimOpposite;

	auto oldStartPosition = mStartPosition;

	if (mCenterSelection == CenterSelection::NEVER)
	{
		if (col == 0)
			mStartPosition = 0;
		if (col < firstVisibleCol)
			mStartPosition = col * dimOpposite;
		else if (col >= firstVisibleCol + dimScrollable)
			mStartPosition = (col - dimScrollable + 1) * dimOpposite;
	}
	else
	{
		if ((col < centralCol || (col == 0 && col == centralCol)) && mCenterSelection == CenterSelection::PARTIAL)
			mStartPosition = 0;
		else if ((col - centralCol) > lastScroll && mCenterSelection == CenterSelection::PARTIAL && !mScrollLoop)
			mStartPosition = lastScroll * dimOpposite;
		else if (maxCentralCol != centralCol && col == firstVisibleCol + maxCentralCol || col == firstVisibleCol + centralCol)
		{
			if (col == firstVisibleCol + maxCentralCol)
				mStartPosition = (col - maxCentralCol) * dimOpposite;
			else
				mStartPosition = (col - centralCol) * dimOpposite;
		}
		else
		{
			if (oldCol == firstVisibleCol + maxCentralCol)
				mStartPosition = (col - maxCentralCol) * dimOpposite;
			else
				mStartPosition = (col - centralCol) * dimOpposite;
		}
	}

	float newCameraOffset = mCameraOffset;

	if (hasGroups())
		newCameraOffset = getGroupedCameraOffset();
	else if (mStartPosition != oldStartPosition)
	{
		auto tileDistance = mTileSize + mMargin;
		newCameraOffset = (isVertical() ? tileDistance.y() : tileDistance.x()) * (mStartPosition / dimOpposite);
	}

	if (hasBands())
		scrollBandToCursor(mLastCursor >= 0 && mLastCursor != -9999 && ALLOWANIMATIONS);

	auto lastCursor = mLastCursor;

	if (lastCursor != -9999 && (lastCursor < 0 || !ALLOWANIMATIONS))
	{
		mCameraOffset = newCameraOffset;
		mEntriesDirty = true;

		if (mCursorChangedCallback)
			mCursorChangedCallback(state);

		mLastCursor = mCursor;

		return;
	}

	if (lastCursor != -9999)
	{
		if (mCursorChangedCallback)
			mCursorChangedCallback(state);
	}

	if (isScrollLooping)
	{
		Vector2f tileDistance = mTileSize + mMargin;
		int totalCols = ((Math::max(0, mEntries.size() - 1)) / dimOpposite);
		float totalSize = (isVertical() ? tileDistance.y() : tileDistance.x()) * (totalCols + 1);

		if (direction)
			mCameraOffset -= totalSize;
		else
			mCameraOffset += totalSize;

		mEntriesDirty = true;
	}

	mLastCursor = mCursor;

	if (newCameraOffset != mCameraOffset)
	{
		if (ALLOWANIMATIONS)
		{
			float oldCameraoffset = mCameraOffset;

			auto func = [this, newCameraOffset, oldCameraoffset](float t)
			{
				t -= 1; // cubic ease out
				float pct = Math::lerp(0, 1, t*t*t + 1);

				mCameraOffset = newCameraOffset * pct + oldCameraoffset * (1.0 - pct);
				mEntriesDirty = true;
			};

			((GuiComponent*)this)->setAnimation(new LambdaAnimation(func, 250), 0, [this, newCameraOffset] {
				mCameraOffset = newCameraOffset;
				mEntriesDirty = true;
			}, false, 2);
		}
		else
		{
			mCameraOffset = newCameraOffset;
			mEntriesDirty = true;
		}
	}
}

template<typename T>
void ImageGridComponent<T>::loadTile(std::shared_ptr<GridTileComponent> tile, typename IList<ImageGridData, T>::Entry& entry)
{
	std::string name = entry.name;

	IBindable* bindable = getBindable(entry);
	
	// tile->setFavorite(entry.data.favorite); //	tile->setCheevos(entry.data.cheevos);
	bool favorite = bindable ? bindable->getProperty("favorite").toBoolean() : false;

	// Label
	if (!favorite || tile->hasFavoriteMedia())
	{
		// Remove favorite text glyph
		if (Utils::String::startsWith(name, _U("\uF006 ")))
			tile->setLabel(name.substr(4));
		else
			tile->setLabel(name);
	}
	else
		tile->setLabel(name);

	if (bindable != nullptr)
		tile->updateBindings(bindable);
	
	if (tile->hasItemTemplate())
		return;

	std::string imagePath = entry.data.texturePath;
	std::string marqueePath = bindable ? bindable->getProperty("marquee").toString() : ""; // entry.data.marqueePath;
	std::string videoPath = bindable ? bindable->getProperty("video").toString() : ""; // entry.data.marqueePath;
	
	if (Utils::FileSystem::isAudio(imagePath) && videoPath.empty())
	{
		videoPath = imagePath;

		if (ResourceManager::getInstance()->fileExists(":/mp3.jpg"))
			imagePath = ":/mp3.jpg";
	}
	else if (Utils::FileSystem::isVideo(imagePath) && videoPath.empty())
	{
		videoPath = imagePath;
		imagePath = "";
	}

	bool cheevos = bindable ? bindable->getProperty("cheevos").toBoolean() : false;
	bool folder = bindable ? bindable->getProperty("folder").toBoolean() : false;
	bool virtualFolder = bindable ? bindable->getProperty("virtualfolder").toBoolean() : false; 

	bool preloadMedias = Settings::PreloadMedias();

	bool setMarquee = true;

	// Image
	if ((preloadMedias && !imagePath.empty()) || (!preloadMedias && ResourceManager::getInstance()->fileExists(imagePath)))
	{
		if (virtualFolder)
		{
			tile->setLabel("");

			if (!mDefaultLogoBackgroundTexture.empty() && tile->isMinSizeTile())
			{
				tile->setImage(mDefaultLogoBackgroundTexture);
				tile->forceMarquee(imagePath);
				setMarquee = false;
			}
			else
				tile->setImage(imagePath, true);
		}
		else
			tile->setImage(imagePath, false);

		if (mImageSource == MARQUEEORTEXT)
			tile->setLabel("");
	}
	else if (mImageSource == MARQUEEORTEXT)
		tile->setImage("");
	else if (folder)
		tile->setImage(mDefaultFolderTexture, mDefaultFolderTexture == ":/folder.svg");
	else
	{
		if (!mDefaultLogoBackgroundTexture.empty() && tile->hasMarquee() && !marqueePath.empty() && ResourceManager::getInstance()->fileExists(marqueePath))
			tile->setImage(mDefaultLogoBackgroundTexture);
		else
			tile->setImage(mDefaultGameTexture, mDefaultGameTexture == ":/cartridge.svg");
	}

	if (setMarquee)
	{
		if (!mDefaultLogoBackgroundTexture.empty() && tile->isMinSizeTile())
			tile->forceMarquee("");

		// Marquee		
		if (tile->hasMarquee())
		{
			if ((preloadMedias && !marqueePath.empty()) || (!preloadMedias && ResourceManager::getInstance()->fileExists(marqueePath)))
				tile->setMarquee(marqueePath);
			else
				tile->setMarquee("");
		}
	}

	tile->setFavorite(favorite);
	tile->setCheevos(cheevos);

	// Video
	if (mAllowVideo)
	{
		if ((preloadMedias && !videoPath.empty()) || (!preloadMedias && ResourceManager::getInstance()->fileExists(videoPath)))
			tile->setVideo(videoPath, mVideoDelay);
		else
			tile->setVideo("");
	}
	else
		tile->setVideo("");
}

template<typename T>
void ImageGridComponent<T>::resetGrid()
{
	mVisibleTiles.clear();
	mLayoutDirty = true;
	mGroupCamera = 0;

	if (mGridSizeOverride.x() != 0 && mGridSizeOverride.y() != 0)
		mAutoLayout = mGridSizeOverride;

	calcGridDimension();

	float dimOpposite = Math::max(1, isVertical() ? mGridDimension.x() : mGridDimension.y());

	mStartPosition = 0;
	if (mCenterSelection != CenterSelection::NEVER)
	{
		int dimScrollable = (isVertical() ? mGridDimension.y() : mGridDimension.x()) - 2 * EXTRAITEMS;
		mStartPosition -= (int)Math::floorf(dimScrollable / 2.0f);
		
		int centralCol = (int)(dimScrollable - 0.5) / 2;
		int maxCentralCol = (int)(dimScrollable) / 2;
		int firstVisibleCol = 0 / dimOpposite; // mStartPosition

		int lastCol = ((mEntries.size() - 1) / dimOpposite);
		int lastScroll = std::max(0, (int)(lastCol + 1 - dimScrollable));

		int oldCol = 0;
		int col = 0;

		if ((col < centralCol || (col == 0 && col == centralCol)) && mCenterSelection == CenterSelection::PARTIAL)
			mStartPosition = 0;
		else if ((col - centralCol) > lastScroll && mCenterSelection == CenterSelection::PARTIAL && !mScrollLoop)
			mStartPosition = lastScroll * dimOpposite;
		else if (maxCentralCol != centralCol && col == firstVisibleCol + maxCentralCol || col == firstVisibleCol + centralCol)
		{
			if (col == firstVisibleCol + maxCentralCol)
				mStartPosition = (col - maxCentralCol) * dimOpposite;
			else
				mStartPosition = (col - centralCol) * dimOpposite;
		}
		else
		{
			if (oldCol == firstVisibleCol + maxCentralCol)
				mStartPosition = (col - maxCentralCol) * dimOpposite;
			else
				mStartPosition = (col - centralCol) * dimOpposite;
		}
	}
	
	auto tileDistance = mTileSize + mMargin;
	mCameraOffset = (isVertical() ? tileDistance.y() : tileDistance.x()) * (mStartPosition / dimOpposite);
	mEntriesDirty = true;

	mLastCursor = -1;
	mLastCursorState = CursorState::CURSOR_STOPPED;
	onCursorChanged(CURSOR_STOPPED);
}

// Calculate how much tiles of size mTileSize we can fit in a grid of size mSize using a margin of size mMargin
template<typename T>
void ImageGridComponent<T>::calcGridDimension()
{
	Vector2f gridDimension = (mSize + mMargin) / (mTileSize + mMargin);
	if (mAutoLayout.x() != 0 && mAutoLayout.y() != 0)
		gridDimension = mAutoLayout;

	mLastRowPartial = Math::floorf(gridDimension.y()) != gridDimension.y();
	mGridDimension = Vector2i((int)gridDimension.x(), Math::ceilf(gridDimension.y()));

	// Grid dimension validation
	if (mGridDimension.x() < 1)
		LOG(LogError) << "Theme defined grid X dimension below 1";
	if (mGridDimension.y() < 1)
		LOG(LogError) << "Theme defined grid Y dimension below 1";

	// Add extra tiles to both sides : Add EXTRAITEMS before, EXTRAITEMS after
	if (isVertical())
		mGridDimension.y() += 2 * EXTRAITEMS;
	else
		mGridDimension.x() += 2 * EXTRAITEMS;

	// Auto layout
	if (mAutoLayout.x() != 0 && mAutoLayout.y() != 0)
	{
		auto x = (mSize.x() - (mMargin.x() * (mAutoLayout.x() - 1)) - mPadding.x() - mPadding.z()) / (int)mAutoLayout.x();
		auto y = (mSize.y() - (mMargin.y() * (mAutoLayout.y() - 1)) - mPadding.y() - mPadding.w()) / (int)mAutoLayout.y();

		if (hasBands())
		{
			// Ratio as it would be without bands (the one wanted by the theme): we keep it
			const float ratio = (y > 0) ? x / y : 1.0f;

			const float rows = (float)(int)mAutoLayout.y();
			const float availableW = mSize.x() - mPadding.x() - mPadding.z();
			const float availableH = mSize.y() - mPadding.y() - mPadding.w();

			// Height: N × (header + tile) + (N-1) × (vertical margin + group spacing)
			y = (availableH - rows * mGroupHeaderHeight - (rows - 1) * (mMargin.y() + mGroupSpacing)) / rows;
			y = Math::max(1.0f, y);

			// Width derived from the height, to keep the tiles' shape
			x = Math::min(y * ratio, availableW);

			// Fully visible columns (used for the bands' horizontal scroll)
			mGridDimension.x() = Math::max(1, (int)Math::floorf((availableW + mMargin.x()) / (x + mMargin.x())));
		}

		mTileSize = Vector2f(x, y);
	}
};

template<typename T>
bool ImageGridComponent<T>::hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult)
{
	auto ret = GuiComponent::hitTest(x, y, parentTransform, pResult);

	Transform4x4f trans = parentTransform * getTransform();
	
	trans.translate(isVertical() ? 0 : -mCameraOffset, isVertical() ? -mCameraOffset : 0);
	
	for (auto tile : mVisibleTiles)
	{
		if (hasBands())
		{
			Transform4x4f bandTrans = trans;
			bandTrans.translate(-getBandScrollX(tile), 0.0f);
			tile->hitTest(x, y, bandTrans, pResult);
		}
		else
			tile->hitTest(x, y, trans, pResult);
	}

	return ret;
}

template<typename T>
bool ImageGridComponent<T>::onMouseWheel(int delta)
{
	float dimOpposite = Math::max(1, isVertical() ? mGridDimension.x() : mGridDimension.y());

	auto newCursor = hasGroups() ? moveByRows(-delta) : mCursor - delta * dimOpposite;
	/*
	if (newCursor < 0)
		newCursor += mEntries.size();
	else if (newCursor >= mEntries.size())
		newCursor -= mEntries.size();
	*/
	if (mScrollLoop)
	{
		if (newCursor < 0)
			newCursor += mEntries.size();
		else if (newCursor >= mEntries.size())
			newCursor -= mEntries.size();
	}
	else
	{
		if (newCursor < 0)
			newCursor = 0;
		else if (newCursor >= mEntries.size())
			newCursor = mEntries.size() - 1;
	}
	
	if (newCursor != mCursor && newCursor >= 0 && newCursor < mEntries.size())
	{
		mCursor = newCursor;
		onCursorChanged(CURSOR_STOPPED);
	}

	return true;
}


template<typename T>
bool ImageGridComponent<T>::onMouseClick(int button, bool pressed, int x, int y)
{
	if (button == 1)
	{
		mTimeHoldingButton = -1;

		if (pressed)
		{
			mPressedCursor = -1;

			std::shared_ptr<GridTileComponent> hotTile;

			int tileIndex = 0;

			for (auto entry : mEntries)
			{
				if (entry.data.tile != nullptr && entry.data.tile->isVisible() && entry.data.tile->isMouseOver())
				{
					hotTile = entry.data.tile;
					break;
				}

				tileIndex++;
			}

			if (hotTile)
			{
				auto cursor = tileIndex;
				if (cursor != mCursor && mCenterSelection == CenterSelection::NEVER)
				{
					mCursor = cursor;

					if (mCursor >= 0 && mCursor < mEntries.size() && mEntries[mCursor].data.tile != nullptr)
					{
						auto selectedTile = getSelectedTile();
						while (selectedTile != nullptr)
						{
							selectedTile->setSelected(false, true, nullptr, true);
							selectedTile = getSelectedTile();
						}

						Vector3f oldPos = Vector3f::Zero();
						mEntries[mCursor].data.tile->setSelected(true, true, mAnimateSelection ? nullptr : &oldPos, true);
					}

					if (mCursorChangedCallback)
						mCursorChangedCallback(CURSOR_STOPPED);

					mLastCursor = mCursor;
				}
				else
					mPressedCursor = mCursor;

				mTimeHoldingButton = 0;
			}

			mIsDragging = false;
			mPressedPoint = Vector2i(x, y);
			mDragAxis = 0;
			mDragBand = hasBands() ? getBandAtScreenY(y) : -1;
			mWindow->setMouseCapture(this);
		}
		else if (mWindow->hasMouseCapture(this))
		{
			mWindow->releaseMouseCapture();

			auto btnDownCursor = mPressedCursor;
			mPressedCursor = -1;
			mPressedPoint = Vector2i(-1, -1);

			if (hasBands() && mDragAxis == 1)
				endBandDrag();

			mDragAxis = 0;

			if (mCenterSelection != CenterSelection::NEVER)
			{
				float dimScrollable = isVertical() ? mGridDimension.y() - 2 * EXTRAITEMS : mGridDimension.x() - 2 * EXTRAITEMS;
				float dimOpposite = Math::max(1, isVertical() ? mGridDimension.x() : mGridDimension.y());

				float tileDist = isVertical() ? (mTileSize + mMargin).y() : (mTileSize + mMargin).x();
				int newCursor = ((mCameraOffset + tileDist * dimScrollable / 2.0f)  * dimOpposite) / tileDist;

				if (mCursor != newCursor && newCursor >= 0 && newCursor < mEntries.size())
				{
					mCursor = newCursor;
					onCursorChanged(CURSOR_STOPPED);
				}
				else
				{
					mLastCursor = -9999;
					mStartPosition = -9999;
					onCursorChanged(CURSOR_STOPPED);
				}
			}

			auto dragging = mIsDragging;
			mIsDragging = false;

			if (!dragging && btnDownCursor == mCursor)
				InputManager::getInstance()->sendMouseClick(mWindow, 1);
		}

		return true;
	}

	return false;
}

template<typename T>
void ImageGridComponent<T>::onMouseMove(int x, int y)
{
	if (mPressedPoint.x() != -1 && mPressedPoint.y() != -1 && mWindow->hasMouseCapture(this))
	{
		if (hasBands())
		{
			// Band mode: the gesture axis is decided after a few pixels
			const int dx = mPressedPoint.x() - x;
			const int dy = mPressedPoint.y() - y;

			if (mDragAxis == 0)
			{
				if (std::abs(dx) < 8 && std::abs(dy) < 8)
					return; // not a drag yet: the click is still possible

				mDragAxis = std::abs(dx) > std::abs(dy) ? 1 : 2;

				if (mDragAxis == 1)
					((GuiComponent*)this)->cancelAnimation(3); // stops the band animation
			}

			if (mDragAxis == 1)
			{
				mIsDragging = true;
				mTimeHoldingButton = -1;

				dragBand(dx);
				mPressedPoint = Vector2i(x, y);
				return;
			}
		}

		mIsDragging = true;
		mTimeHoldingButton = -1;

		if (isVertical())
			mCameraOffset += mPressedPoint.y() - y;
		else
			mCameraOffset += mPressedPoint.x() - x;

		if (mCenterSelection == CenterSelection::NEVER)
		{
			if (mCameraOffset < 0)
				mCameraOffset = 0;

			if (mCameraOffset > mTotalHeight)
				mCameraOffset = mTotalHeight;
		}
		else
		{

			float dimScrollable = isVertical() ? mGridDimension.y() - 2 * EXTRAITEMS : mGridDimension.x() - 2 * EXTRAITEMS;
			float dimOpposite = Math::max(1, isVertical() ? mGridDimension.x() : mGridDimension.y());
			float tileDist = isVertical() ? (mTileSize + mMargin).y() : (mTileSize + mMargin).x();

			float min = tileDist * (float)(int)(dimScrollable / 2);

			int totalToScroll = mTotalHeight + tileDist;

			if (mScrollLoop)
			{
				if (mCameraOffset < -min)
					mCameraOffset += totalToScroll + tileDist;

				if (mCameraOffset > totalToScroll - min)
					mCameraOffset -= totalToScroll + tileDist;
			}
			else
			{
				if (mCameraOffset < -min)
					mCameraOffset = -min;

				if (mCameraOffset > mTotalHeight)
					mCameraOffset = mTotalHeight;
			}

			int newCursor = ((mCameraOffset + tileDist * dimScrollable / 2.0f)  * dimOpposite) / tileDist;

			if (mCursor != newCursor && newCursor >= 0 && newCursor < mEntries.size())
			{
				mCursor = newCursor;
				if (mCursor >= 0 && mCursor < mEntries.size() && mEntries[mCursor].data.tile != nullptr)
				{
					auto selectedTile = getSelectedTile();
					while (selectedTile != nullptr)
					{
						selectedTile->setSelected(false, true, nullptr, true);
						selectedTile = getSelectedTile();
					}

					Vector3f oldPos = Vector3f::Zero();
					mEntries[mCursor].data.tile->setSelected(true, true, mAnimateSelection ? nullptr : &oldPos, true, false);
				}

				if (mCursorChangedCallback)
					mCursorChangedCallback(CURSOR_STOPPED);

				mLastCursor = mCursor;
			}
		}


		mGroupCamera = mCameraOffset;

		mEntriesDirty = true;
		mPressedPoint = Vector2i(x, y);
	}
}

#endif // ES_CORE_COMPONENTS_IMAGE_GRID_COMPONENT_H