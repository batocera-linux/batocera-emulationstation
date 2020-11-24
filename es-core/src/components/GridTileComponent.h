#pragma once
#ifndef ES_CORE_COMPONENTS_GRID_TILE_COMPONENT_H
#define ES_CORE_COMPONENTS_GRID_TILE_COMPONENT_H

#include "NinePatchComponent.h"
#include "ImageComponent.h"
#include "TextComponent.h"
#include "ThemeData.h"
#include "resources/TextureResource.h"

class VideoComponent;

struct GridImageProperties
{
public:
	GridImageProperties()
	{
		Loaded = false;
		Visible  = false;

		reflexion = Vector2f::Zero();
		pos = Vector2f(0.5f, 0.5f);
		size = Vector2f(1.0f, 1.0f);
		origin = Vector2f(0.5f, 0.5f);
		color = colorEnd = 0xFFFFFFFF;		
		sizeMode = "maxSize";
		roundCorners = 0;
	}

	void mixProperties(GridImageProperties& def, GridImageProperties& sel, float percent);
	bool applyTheme(const ThemeData::ThemeElement* elem);

	void updateImageComponent(ImageComponent* image, Vector2f offsetPos, Vector2f parentSize, bool disableSize = false)
	{
		if (image == nullptr)
			return;

		image->setPosition(offsetPos.x() + pos.x() * parentSize.x(), offsetPos.y() + pos.y() * parentSize.y());
		
		if (!disableSize && sizeMode == "size")
			image->setSize(size.x() * parentSize.x(), size.y() * parentSize.y());
		else if (sizeMode == "minSize")
			image->setMinSize(size.x() * parentSize.x(), size.y() * parentSize.y());
		else
			image->setMaxSize(size.x() * parentSize.x(), size.y() * parentSize.y());

		image->setOrigin(origin);
		image->setColorShift(color);
		image->setColorShiftEnd(colorEnd);
		image->setMirroring(reflexion);
		image->setRoundCorners(roundCorners);
	}

	bool Loaded;
	bool Visible;

	Vector2f pos;
	Vector2f size;
	Vector2f origin;
	Vector2f reflexion;
	
	unsigned int color;
	unsigned int colorEnd;

	std::string  sizeMode;

	float roundCorners;
};

struct GridTextProperties
{
public:
	GridTextProperties()
	{
		Loaded = false;
		Visible = false;
		
		pos = Vector2f(-1, -1);
		size = Vector2f(1.0f, 0.30f);		
		color = 0xFFFFFFFF;
		backColor = 0;		
		fontSize = 0;
		glowColor = 0;
		glowSize = 0;
		padding = Vector4f::Zero();
	}

	void mixProperties(GridTextProperties& def, GridTextProperties& sel, float percent);
	bool applyTheme(const ThemeData::ThemeElement* elem);

	void updateTextComponent(TextComponent* text, Vector2f parentSize, bool disableSize = false)
	{
		if (text == nullptr)
			return;

		text->setPosition(pos.x() * parentSize.x(), pos.y() * parentSize.y());
		text->setSize(size.x() * parentSize.x(), size.y() * parentSize.y());
	//	text->setPadding(padding);
		text->setColor(color);
		text->setBackgroundColor(backColor);
		text->setGlowColor(glowColor);
		text->setGlowSize(glowSize);
		text->setAutoScroll(autoScroll);
		text->setFont(fontPath, fontSize * Math::min(Renderer::getScreenHeight(), Renderer::getScreenWidth()));
	}
	
	bool Loaded;
	bool Visible;

	Vector2f pos;
	Vector2f size;

	unsigned int color;
	unsigned int backColor;

	unsigned int glowColor;
	float glowSize;

	std::string  fontPath;
	float fontSize;
	bool autoScroll;
	Vector4f padding;
};

struct GridNinePatchProperties
{
public:
	GridNinePatchProperties()
	{
		Loaded = false;
		Visible = false;

		edgeColor = centerColor = animateColor = 0xFFFFFFFF;
		cornerSize = Vector2f(16, 16);
		path = ":/frame.png";
		animateTime = 0;
		padding = Vector4f::Zero();
		mTexture = TextureResource::get(path, false, true);
	}

	void setImagePath(const std::string _path)
	{
		path = _path;
		mTexture = TextureResource::get(path, false, true);
	}

	bool applyTheme(const ThemeData::ThemeElement* elem);

	void updateNinePatchComponent(NinePatchComponent* ctl)
	{
		if (ctl == nullptr)
			return;

		ctl->setCenterColor(centerColor);
		ctl->setEdgeColor(edgeColor);
		ctl->setCornerSize(cornerSize);
		ctl->setAnimateTiming(animateTime);
		ctl->setAnimateColor(animateColor);
		ctl->setImagePath(path);
		ctl->setPadding(padding);
	}

	bool Loaded;
	bool Visible;

	Vector2f	 cornerSize;
	unsigned int centerColor;
	unsigned int edgeColor;
	std::string  path;
	Vector4f	 padding;

	unsigned int animateColor;
	float animateTime;

	// Store texture to avoid unloading & flickering
	std::shared_ptr<TextureResource> mTexture;
};

struct GridTileProperties
{
	Vector2f				Size;
	Vector4f				Padding;
	std::string				SelectionMode;

	GridNinePatchProperties Background;

	GridTextProperties		Label;
	GridImageProperties		Image;
	GridImageProperties		Marquee;
	GridImageProperties		Favorite;
	GridImageProperties		ImageOverlay;
};

class GridTileComponent : public GuiComponent
{
public:
	GridTileComponent(Window* window);
	~GridTileComponent();

	void render(const Transform4x4f& parentTrans) override;

	virtual void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties);

	// Made this a static function because the ImageGridComponent need to know the default tile max size
	// to calculate the grid dimension before it instantiate the GridTileComponents
	static Vector2f getDefaultTileSize();
	Vector2f getSelectedTileSize() const;
	bool isSelected() const;

	void resetImages();

	void setLabel(std::string name);
	void setVideo(const std::string& path, float defaultDelay = -1.0);

	void setImage(const std::string& path, bool isDefaultImage = false);
	void setMarquee(const std::string& path);
	
	void setFavorite(bool favorite);
	bool hasFavoriteMedia() { return mFavorite != nullptr; }

	void setSelected(bool selected, bool allowAnimation = true, Vector3f* pPosition = NULL, bool force = false);	

	void forceSize(Vector2f size, float selectedZoom = 1.0);

	void renderBackground(const Transform4x4f& parentTrans);
	void renderContent(const Transform4x4f& parentTrans);

	bool shouldSplitRendering() { return isAnimationPlaying(3); };

	Vector3f getBackgroundPosition();

	virtual void onShow();
	virtual void onHide();
	virtual void update(int deltaTime);
	virtual void onScreenSaverActivate();
	virtual void onScreenSaverDeactivate();

	bool isMinSizeTile();
	bool hasMarquee();
	void setIsDefaultImage(bool value = true) { mIsDefaultImage = value; }

	void forceMarquee(const std::string& path);

	std::shared_ptr<TextureResource> getTexture(bool marquee = false);

private:
	void	resetProperties();
	void	createVideo();
	void	createMarquee();
	void	createFavorite();
	void	createImageOverlay();
	void	startVideo();
	void	stopVideo();

	void resize();

	static void applyThemeToProperties(const ThemeData::ThemeElement* elem, GridTileProperties& properties);

	GridTileProperties getCurrentProperties(bool mixValues = true);

	TextComponent mLabel;

	// bool mLabelVisible;
	bool mLabelMerged;

	NinePatchComponent mBackground;

	GridTileProperties mDefaultProperties;
	GridTileProperties mSelectedProperties;
	GridTileProperties mVideoPlayingProperties;

	std::string mCurrentMarquee;
	std::string mCurrentPath;
	std::string mVideoPath;

	void setSelectedZoom(float percent);

	float mSelectedZoomPercent;
	bool mSelected;	

	bool mIsDefaultImage;

	Vector3f mAnimPosition;

	VideoComponent* mVideo;
	ImageComponent* mImage;
	ImageComponent* mMarquee;
	ImageComponent* mFavorite;
	ImageComponent* mImageOverlay;

	bool mVideoPlaying;	
	bool mHasStandardMarquee;
};

#endif // ES_CORE_COMPONENTS_GRID_TILE_COMPONENT_H
