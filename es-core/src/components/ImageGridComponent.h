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
#include <set>
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
};

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

	void setImage(const std::string& imagePath, const T& obj);
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

	Vector3f getCameraOffset() 
	{ 
		if (isVertical())
			return Vector3f(0, mCameraOffset, 0);

		return Vector3f(mCameraOffset,  0, 0);
	}

	Delegate<ILongMouseClickEvent> longMouseClick;

	void		preloadTiles();

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
Vector2i ImageGridComponent<T>::getVisibleRange()
{
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

	auto range = getVisibleRange();
	int startIndex = range.x(); // position - EXTRAITEMS * dimOpposite;
	int endIndex = range.y(); // startIndex + dimExt * dimOpposite;

	if (startIndex < 0)
		startIndex = 0;

	if (endIndex > mEntries.size() - 1)
		endIndex = mEntries.size() - 1;

	int totalCols = Math::max(0, mEntries.size() - 1) / dimOpposite;
	mTotalHeight = Math::max(0.0f, (float) (isVertical() ? tileDistance.y() : tileDistance.x()) * (totalCols - 1));

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

		if ((mScrollLoop && idx >= range.x() && idx <= range.y()) || (!mScrollLoop && i >= startIndex && i <= endIndex))
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

			if (mScrollLoop && i < startIndex || i > endIndex)
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

	static_cast<IList< ImageGridData, T >*>(this)->add(entry);

	mEntriesDirty = true;
}

template<typename T>
void ImageGridComponent<T>::clear()
{	
	IList<ImageGridData, T>::clear();
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
void ImageGridComponent<T>::setImage(const std::string& imagePath, const T& obj)
{
	IList<ImageGridData, T>* list = static_cast<IList< ImageGridData, T >*>(this);
	auto entry = list->findEntry(obj);
	if (entry != list->end() && (*entry).data.texturePath != imagePath)
	{
		(*entry).data.texturePath = imagePath;
		(*entry).data.tile = nullptr;

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
			if (isVertical())
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

	for (auto tile : mVisibleTiles)
	{
		if (tile->isSelected())
		{
			selectedTile = tile;
			if (splittedRendering && tile->shouldSplitRendering())
				tile->renderBackground(tileTrans);
		}
		else
			tile->render(tileTrans);
	}

	// Render the selected image content on top of the others
	if (selectedTile != nullptr)
	{
		if (splittedRendering && selectedTile->shouldSplitRendering())
			selectedTile->renderContent(tileTrans);
		else
			selectedTile->render(tileTrans);
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
		mScrollbar.setRange(0, mTotalHeight + (isVertical() ? scrollableSize.y() : scrollableSize.y()), isVertical() ? scrollableSize.y() : scrollableSize.y());
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
}

template<typename T>
void ImageGridComponent<T>::onCursorChanged(const CursorState& state)
{
	if (mEntries.size() == 0)
		return;

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
		else if (mLastCursor = -9999)
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

	if (mStartPosition != oldStartPosition)
	{
		auto tileDistance = mTileSize + mMargin;
		newCameraOffset = (isVertical() ? tileDistance.y() : tileDistance.x()) * (mStartPosition / dimOpposite);
	}

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
		tile->hitTest(x, y, trans, pResult);

	return ret;
}

template<typename T>
bool ImageGridComponent<T>::onMouseWheel(int delta)
{
	float dimOpposite = Math::max(1, isVertical() ? mGridDimension.x() : mGridDimension.y());

	auto newCursor = mCursor - delta * dimOpposite;
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
			mWindow->setMouseCapture(this);
		}
		else if (mWindow->hasMouseCapture(this))
		{
			mWindow->releaseMouseCapture();

			auto btnDownCursor = mPressedCursor;
			mPressedCursor = -1;
			mPressedPoint = Vector2i(-1, -1);

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


		mEntriesDirty = true;
		mPressedPoint = Vector2i(x, y);
	}
}

#endif // ES_CORE_COMPONENTS_IMAGE_GRID_COMPONENT_H