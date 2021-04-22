#include "GridTileComponent.h"

#include "resources/TextureResource.h"
#include "ThemeData.h"

#include <algorithm>

#include "animations/LambdaAnimation.h"
#include "ImageIO.h"

#ifdef _RPI_
#include "components/VideoPlayerComponent.h"
#endif
#include "components/VideoVlcComponent.h"
#include "utils/FileSystemUtil.h"

#include "Settings.h"
#include "ImageGridComponent.h"

#define VIDEODELAY	100

GridTileComponent::GridTileComponent(Window* window) : GuiComponent(window), mBackground(window), mLabel(window), mVideo(nullptr), mVideoPlaying(false)
{
	mHasStandardMarquee = false;
	mSelectedZoomPercent = 1.0f;
	mAnimPosition = Vector3f(0, 0);
	mVideo = nullptr;
	mMarquee = nullptr;
	mFavorite = nullptr;
	mCheevos = nullptr;
	mImageOverlay = nullptr;
	mIsDefaultImage = false;
	
	mLabelMerged = false;

	resetProperties();

	mImage = new ImageComponent(mWindow);
	mImage->setOrigin(0.5f, 0.5f);

	mLabel.setFont(Font::get(FONT_SIZE_SMALL));
	mLabel.setDefaultZIndex(10);

	addChild(&mBackground);
	addChild(&(*mImage));
	addChild(&mLabel);

	setSelected(false);
	setVisible(true);
}

void GridTileComponent::resetProperties()
{
	mDefaultProperties.Size = getDefaultTileSize();
	mDefaultProperties.Padding = Vector4f(16.0f, 16.0f, 16.0f, 16.0f);

	mSelectedProperties.Size = getSelectedTileSize();
	mSelectedProperties.Padding = mDefaultProperties.Padding;

	mDefaultProperties.Label = mSelectedProperties.Label = GridTextProperties();	
	mDefaultProperties.Image = mSelectedProperties.Image = GridImageProperties();	
	mDefaultProperties.Marquee = mSelectedProperties.Marquee = GridImageProperties();
	mDefaultProperties.Favorite = mSelectedProperties.Favorite = GridImageProperties();
	mDefaultProperties.Cheevos = mSelectedProperties.Cheevos = GridImageProperties();
	mDefaultProperties.Background = mSelectedProperties.Background = GridNinePatchProperties();

	mDefaultProperties.Background.centerColor = mDefaultProperties.Background.edgeColor = 0xAAAAEEFF;
	mDefaultProperties.Image.color = mDefaultProperties.Image.colorEnd = 0xFFFFFFDD;

	mVideoPlayingProperties = mSelectedProperties;
}

void GridTileComponent::forceSize(Vector2f size, float selectedZoom)
{
	mSize = size;
	mDefaultProperties.Size = size;
	mSelectedProperties.Size = size * selectedZoom;
	mVideoPlayingProperties.Size = mSelectedProperties.Size;
}

GridTileComponent::~GridTileComponent()
{
	if (mImage != nullptr)
		delete mImage;

	if (mImageOverlay != nullptr)
		delete mImageOverlay;

	if (mFavorite != nullptr)
		delete mFavorite;

	if (mCheevos != nullptr)
		delete mCheevos;

	if (mMarquee != nullptr)
		delete mMarquee;

	if (mVideo != nullptr)
		delete mVideo;

	mFavorite = nullptr;
    mCheevos = nullptr;
	mMarquee = nullptr;
	mImage = nullptr;
	mVideo = nullptr;
	mImageOverlay = nullptr;
}

std::shared_ptr<TextureResource> GridTileComponent::getTexture(bool marquee) 
{ 
	if (marquee && mMarquee  != nullptr)
		return mMarquee->getTexture();
	else if (!marquee && mImage != nullptr)
		return mImage->getTexture();

	return nullptr; 
}

void GridTileComponent::resize()
{
	auto currentProperties = getCurrentProperties();

	Vector2f size = currentProperties.Size;
	if (mSize != size)
		setSize(size);

	bool isDefaultImage = mIsDefaultImage; // && (mCurrentPath == ":/folder.svg" || mCurrentPath == ":/cartridge.svg");

	float height = (int) (size.y() * currentProperties.Label.size.y());
	float labelHeight = height;

	if (!currentProperties.Label.Visible || mLabelMerged || currentProperties.Label.size.x() == 0)
		height = 0;

	float topPadding = currentProperties.Padding.y();
	float bottomPadding = std::max(currentProperties.Padding.w(), height);

	Vector2f imageOffset = Vector2f(currentProperties.Padding.x(), currentProperties.Padding.y());
	Vector2f imageSize(size.x() - currentProperties.Padding.x() - currentProperties.Padding.z(), size.y() - topPadding - bottomPadding);

	// Image
	if (currentProperties.Image.Loaded)
	{
		if (isDefaultImage)
		{
			imageOffset.x() += imageSize.x() * 0.05;
			imageSize.x() *= 0.90;
		}

		currentProperties.Image.updateImageComponent(mImage, imageOffset, imageSize, false);

		if (mImage != nullptr && isDefaultImage)
			mImage->setRoundCorners(0);

		if (mImage != nullptr && currentProperties.Image.sizeMode != "maxSize" && isDefaultImage)
			mImage->setMaxSize(imageSize.x(), imageSize.y());
	}	
	else if (mImage != nullptr)
	{
		// Retrocompatibility : imagegrid.image is not defined
		mImage->setOrigin(0.5f, 0.5f);
		mImage->setPosition(imageOffset.x() + imageSize.x() / 2.0f, imageOffset.y() + imageSize.y() / 2.0f);
		mImage->setColorShift(currentProperties.Image.color);
		mImage->setMirroring(currentProperties.Image.reflexion);

		if (currentProperties.Image.sizeMode == "minSize" && !isDefaultImage)
			mImage->setMinSize(imageSize.x(), imageSize.y());
		else if (currentProperties.Image.sizeMode == "size")
			mImage->setSize(imageSize.x(), imageSize.x());
		else
			mImage->setMaxSize(imageSize.x(), imageSize.y());

//		imageOffset = Vector2f::Zero();
	}
	
	// Recompute final image size if necessary
	if (mImage != nullptr && currentProperties.Image.sizeMode == "maxSize")
	{
		auto origin = mImage->getOrigin();
		auto pos = mImage->getPosition();
		imageSize = mImage->getSize();
		imageOffset = Vector2f(pos.x() - imageSize.x() * origin.x(), pos.y() - imageSize.y() * origin.y());
	}
	
	// Text	
	mLabel.setVisible(!mLabel.getText().empty() && (currentProperties.Label.Visible || mIsDefaultImage));

	if (currentProperties.Label.Visible)
	{
		auto szRef = mLabelMerged ? mSize - imageOffset : mSize;

		currentProperties.Label.updateTextComponent(&mLabel, szRef);
		
		// Automatic layout for not merged labels 
		if (currentProperties.Label.pos.x() < 0)
		{			
			if (currentProperties.Padding.x() == 0 && !mLabelMerged)
			{
				mLabel.setPosition(mImage->getPosition().x() - mImage->getSize().x() / 2, mImage->getSize().y());
				mLabel.setSize(mImage->getSize().x(), labelHeight);
			}
			else
			{
				mLabel.setPosition(0, szRef.y() - labelHeight);
				mLabel.setSize(size.x(), labelHeight);
			}
		}
	}
	else if (mIsDefaultImage)
	{
		mLabel.setColor(0xFFFFFFFF);
		mLabel.setGlowColor(0x00000010);
		mLabel.setGlowSize(2);
		mLabel.setOpacity(255);
		mLabel.setPosition(mSize.x() * 0.1, mSize.y() * 0.2);
		mLabel.setSize(mSize.x() - mSize.x() * 0.2, mSize.y() - mSize.y() * 0.3);
	}

	// Other controls ( Favorite / Marquee / Overlay )
	if (currentProperties.Favorite.Loaded)
		currentProperties.Favorite.updateImageComponent(mFavorite, imageOffset, imageSize, false);
	
    if (currentProperties.Cheevos.Loaded)
		currentProperties.Cheevos.updateImageComponent(mCheevos, imageOffset, imageSize, false);

	if (currentProperties.Marquee.Loaded)
		currentProperties.Marquee.updateImageComponent(mMarquee, imageOffset, imageSize, true);

	if (currentProperties.ImageOverlay.Loaded)
		currentProperties.ImageOverlay.updateImageComponent(mImageOverlay, imageOffset, imageSize, false);

	// Video
	if (mVideo != nullptr && mVideo->isPlaying())
	{
		if (currentProperties.Image.sizeMode != "size")
		{
			mVideo->setOrigin(0.5, 0.5);			
			mVideo->setPosition(imageOffset.x() + imageSize.x() / 2.0f, imageOffset.y() + imageSize.y() / 2.0f);
			mVideo->setMinSize(imageSize.x(), imageSize.y());

			if (mImage != nullptr)
				mVideo->setRoundCorners(mImage->getRoundCorners());
		}
		else
		{
			mVideo->setOrigin(0.5f, 0.5f);
			mVideo->setPosition(size.x() / 2.0f, (size.y() - height) / 2.0f);

			if (currentProperties.Image.sizeMode == "size")
				mVideo->setSize(imageSize.x(), size.y() - topPadding - bottomPadding);
			else
				mVideo->setMaxSize(imageSize.x(), size.y() - topPadding - bottomPadding);
		}
	}

	// Background when SelectionMode == "image"
	Vector3f bkPosition = Vector3f::Zero();
	Vector2f bkSize = size;

	if (mImage != NULL && currentProperties.SelectionMode == "image" && mImage->getSize() != Vector2f(0, 0))
	{
		if (currentProperties.Image.sizeMode == "minSize")
		{
			if (!mLabelMerged && currentProperties.Label.Visible)
				bkSize = Vector2f(size.x(), size.y() - bottomPadding + topPadding);
		}
		else
		{
			bkPosition = Vector3f(imageOffset.x() - mSelectedProperties.Padding.x(), imageOffset.y() - mSelectedProperties.Padding.y(), 0);
			bkSize = Vector2f(imageSize.x() + 2 * mSelectedProperties.Padding.x(), imageSize.y() + 2 * mSelectedProperties.Padding.y());
		}
	}
	
	// Background when animating
	if (mSelectedZoomPercent != 1.0f && mAnimPosition.x() != 0 && mAnimPosition.y() != 0 && mSelected)
	{
		float x = mPosition.x() + bkPosition.x();
		float y = mPosition.y() + bkPosition.y();

		x = mAnimPosition.x() * (1.0 - mSelectedZoomPercent) + x * mSelectedZoomPercent;
		y = mAnimPosition.y() * (1.0 - mSelectedZoomPercent) + y * mSelectedZoomPercent;

		bkPosition = Vector3f(x - mPosition.x(), y - mPosition.y(), 0);
	}	
	
	mBackground.setPosition(bkPosition);
	mBackground.setSize(bkSize);

	currentProperties.Background.updateNinePatchComponent(&mBackground);
	/*
	mBackground.setCornerSize(currentProperties.Background.cornerSize);
	mBackground.setCenterColor(currentProperties.Background.centerColor);
	mBackground.setEdgeColor(currentProperties.Background.edgeColor);
	mBackground.setImagePath(currentProperties.Background.path);
	*/
	if (mSelected && mAnimPosition == Vector3f(0, 0, 0) && mSelectedZoomPercent != 1.0)
		mBackground.setOpacity(mSelectedZoomPercent * 255);
	else
		mBackground.setOpacity(255);
}

void GridTileComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mVideo != nullptr && mVideo->isPlaying() && mVideo->isFading())
		resize();
}

void GridTileComponent::renderBackground(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	Transform4x4f trans = parentTrans * getTransform();
	mBackground.render(trans);
}

bool GridTileComponent::hasMarquee()
{
	return mHasStandardMarquee;
}

bool GridTileComponent::isMinSizeTile()
{
	auto currentProperties = getCurrentProperties(false);
	return (currentProperties.Image.sizeMode == "minSize");
}

void GridTileComponent::renderContent(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	Transform4x4f trans = parentTrans * getTransform();

	Vector2f clipPos(trans.translation().x(), trans.translation().y());
	if (!Renderer::isVisibleOnScreen(clipPos.x(), clipPos.y(), mSize.x() * trans.r0().x(), mSize.y() * trans.r1().y()))
		return;

	auto currentProperties = getCurrentProperties(false);

	float padding = currentProperties.Padding.x();
	float topPadding = currentProperties.Padding.y();
	float bottomPadding = topPadding;

	if (currentProperties.Label.Visible && !mLabelMerged)
		bottomPadding = std::max((int)topPadding, (int)(mSize.y() * currentProperties.Label.size.y()));

	Vector2i pos((int)Math::round(trans.translation()[0] + padding), (int)Math::round(trans.translation()[1] + topPadding));
	Vector2i size((int)Math::round(mSize.x()* trans.r0().x() - 2 * padding), (int)Math::round(mSize.y()* trans.r1().y() - topPadding - bottomPadding));
	
	bool isDefaultImage = mIsDefaultImage;
	bool isMinSize = !isDefaultImage && currentProperties.Image.sizeMode == "minSize";

	if (isMinSize)
		Renderer::pushClipRect(pos, size);
		
	if (mImage != NULL)
	{		
		if (!isMinSize || !mSelected || mVideo == nullptr || !(mVideo->isPlaying() && !mVideo->isFading()))
			mImage->render(trans);
	}

	if (mSelected && !mVideoPath.empty() && mVideo != nullptr)
		mVideo->render(trans);
	
	if (!mLabelMerged && isMinSize)
		Renderer::popClipRect();
	
	std::vector<GuiComponent*> zOrdered;

	if (mMarquee != nullptr && mMarquee->hasImage())
		zOrdered.push_back(mMarquee);
	else
		zOrdered.push_back(&mLabel);

	if (mFavorite != nullptr && mFavorite->hasImage() && mFavorite->isVisible())
		zOrdered.push_back(mFavorite);

	if (mCheevos != nullptr && mCheevos->hasImage() && mCheevos->isVisible())
		zOrdered.push_back(mCheevos);

	if (mImageOverlay != nullptr && mImageOverlay->hasImage() && mImageOverlay->isVisible())
		zOrdered.push_back(mImageOverlay);

	std::stable_sort(zOrdered.begin(), zOrdered.end(), [](GuiComponent* a, GuiComponent* b) { return b->getZIndex() > a->getZIndex(); });
	
	for (auto comp : zOrdered)
		comp->render(trans);
		
	if (mLabelMerged && isMinSize)
		Renderer::popClipRect();
}

void GridTileComponent::render(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	renderBackground(parentTrans);
	renderContent(parentTrans);
}

void GridTileComponent::createMarquee()
{
	if (mMarquee != nullptr)
		return;

	mMarquee = new ImageComponent(mWindow);
	mMarquee->setOrigin(0.5f, 0.5f);
	mMarquee->setDefaultZIndex(20);
	addChild(mMarquee);
}

void GridTileComponent::createFavorite()
{
	if (mFavorite != nullptr)
		return;

	mFavorite = new ImageComponent(mWindow);
	mFavorite->setOrigin(0.5f, 0.5f);
	mFavorite->setDefaultZIndex(15);
	mFavorite->setVisible(false);
	
	addChild(mFavorite);
}

void GridTileComponent::createCheevos()
{
	if (mCheevos != nullptr)
		return;

	mCheevos = new ImageComponent(mWindow);
	mCheevos->setOrigin(0.5f, 0.5f);
	mCheevos->setDefaultZIndex(15);
	mCheevos->setVisible(false);
	
	addChild(mCheevos);
}

void GridTileComponent::createImageOverlay()
{
	if (mImageOverlay != nullptr)
		return;

	mImageOverlay = new ImageComponent(mWindow);
	mImageOverlay->setOrigin(0.5f, 0.5f);
	mImageOverlay->setDefaultZIndex(25);
	mImageOverlay->setVisible(false);

	addChild(mImageOverlay);
}

void GridTileComponent::createVideo()
{
	if (mVideo != nullptr)
		return;

	mVideo = new VideoVlcComponent(mWindow, "");

	// video
	mVideo->setOrigin(0.5f, 0.5f);
	mVideo->setStartDelay(VIDEODELAY);
	mVideo->setDefaultZIndex(11);
	addChild(mVideo);
}

void GridTileComponent::applyThemeToProperties(const ThemeData::ThemeElement* elem, GridTileProperties& properties)
{
	if (elem == nullptr)
		return;

	Vector2f screen = Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	if (elem->has("size"))
		properties.Size = elem->get<Vector2f>("size") * screen;

	if (elem->has("padding"))
	{
		properties.Padding = elem->get<Vector4f>("padding");
		
		if (abs(properties.Padding.x()) < 1 && abs(properties.Padding.y()) < 1 && abs(properties.Padding.w()) < 1 && abs(properties.Padding.y()) < 1)
			properties.Padding *= screen;
	}

	if (elem && elem->has("selectionMode"))
		properties.SelectionMode = elem->get<std::string>("selectionMode");		

	// Retrocompatibility for Background properties
	if (elem->has("backgroundImage"))
		properties.Background.setImagePath(elem->get<std::string>("backgroundImage"));

	if (elem->has("backgroundCornerSize"))
		properties.Background.cornerSize = elem->get<Vector2f>("backgroundCornerSize");

	if (elem->has("backgroundColor"))
	{
		properties.Background.centerColor = elem->get<unsigned int>("backgroundColor");
		properties.Background.edgeColor = elem->get<unsigned int>("backgroundColor");
	}

	if (elem->has("backgroundCenterColor"))
		properties.Background.centerColor = elem->get<unsigned int>("backgroundCenterColor");

	if (elem->has("backgroundEdgeColor"))
		properties.Background.edgeColor = elem->get<unsigned int>("backgroundEdgeColor");

	// Retrocompatibility for Image properties
	if (elem && elem->has("reflexion"))
		properties.Image.reflexion = elem->get<Vector2f>("reflexion");

	if (elem->has("imageColor"))
		properties.Image.color = properties.Image.colorEnd = elem->get<unsigned int>("imageColor");

	if (elem && elem->has("imageSizeMode"))
		properties.Image.sizeMode = elem->get<std::string>("imageSizeMode");
}

bool GridImageProperties::applyTheme(const ThemeData::ThemeElement* elem)
{
	if (!elem)
		return false;

	Loaded = true;
	Visible = true;

	if (elem && elem->has("visible"))
		Visible = elem->get<bool>("visible");

	if (elem && elem->has("origin"))
		origin = elem->get<Vector2f>("origin");

	if (elem && elem->has("pos"))
		pos = elem->get<Vector2f>("pos");

	if (elem && elem->has("size"))
	{
		sizeMode = "size";
		size = elem->get<Vector2f>("size");
	}
	else if (elem && elem->has("minSize"))
	{
		sizeMode = "minSize";
		size = elem->get<Vector2f>("minSize");
	}
	else if (elem && elem->has("maxSize"))
	{
		sizeMode = "maxSize";
		size = elem->get<Vector2f>("maxSize");
	}

	if (elem && elem->has("color"))
		color = colorEnd = elem->get<unsigned int>("color");

	if (elem && elem->has("colorEnd"))
		colorEnd = elem->get<unsigned int>("colorEnd");

	if (elem && elem->has("reflexion"))
		reflexion = elem->get<Vector2f>("reflexion");

	if (elem && elem->has("roundCorners"))
		roundCorners = elem->get<float>("roundCorners");

	return true;
}

bool GridTextProperties::applyTheme(const ThemeData::ThemeElement* elem)
{
	if (!elem)
	{
		Visible = false;
		return false;
	}

	Loaded = true;
	Visible = true;

	if (elem && elem->has("visible"))
		Visible = elem->get<bool>("visible");

	if (elem && elem->has("pos"))
		pos = elem->get<Vector2f>("pos");

	if (elem && elem->has("size"))
	{
		size = elem->get<Vector2f>("size");
		if (size.y() == 0)
			Visible = false;
	}

	if (elem && elem->has("color"))
		color = elem->get<unsigned int>("color");

	if (elem && elem->has("backgroundColor"))
		backColor = elem->get<unsigned int>("backgroundColor");

	if (elem && elem->has("glowColor"))
		glowColor = elem->get<unsigned int>("glowColor");

	if (elem && elem->has("glowSize"))
		glowSize = elem->get<float>("glowSize");

	if (elem && elem->has("fontSize"))
		fontSize = elem->get<float>("fontSize");

	if (elem && elem->has("fontPath"))
		fontPath = elem->get<std::string>("fontPath");

	if (elem && elem->has("padding"))
		padding = elem->get<Vector4f>("padding");

	if (elem->has("singleLineScroll"))
		autoScroll = elem->get<bool>("singleLineScroll");

	return true;
}

bool GridNinePatchProperties::applyTheme(const ThemeData::ThemeElement* elem)
{
	if (!elem)
	{
		Visible = false;
		return false;
	}

	Loaded = true;
	Visible = true;

	if (elem && elem->has("visible"))
		Visible = elem->get<bool>("visible");
	/*
	if (elem && elem->has("pos"))
		pos = elem->get<Vector2f>("pos");

	if (elem && elem->has("size"))
		size = elem->get<Vector2f>("size");
		*/
	if (elem && elem->has("color"))
		centerColor = edgeColor = elem->get<unsigned int>("color");

	if (elem && elem->has("centerColor"))
		centerColor = elem->get<unsigned int>("centerColor");

	if (elem && elem->has("edgeColor"))
		edgeColor = elem->get<unsigned int>("edgeColor");

	if (elem && elem->has("cornerSize"))
		cornerSize = elem->get<Vector2f>("cornerSize");

	if (elem && elem->has("path"))
		setImagePath(elem->get<std::string>("path"));

	if (elem && elem->has("animateColor"))
		animateColor = elem->get<unsigned int>("animateColor");

	if (elem && elem->has("animateColorTime"))
		animateTime = elem->get<float>("animateColorTime");

	if (elem && elem->has("padding"))
		padding = elem->get<Vector4f>("padding");

	return true;
}

void GridTileComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	if (mSize == Vector2f::Zero())
		setSize(getDefaultTileSize());

	resetProperties();

	const ThemeData::ThemeElement* grid = theme->getElement(view, "gamegrid", "imagegrid");
	if (grid && grid->has("showVideoAtDelay"))
	{
		createVideo();

		if (theme->getElement(view, "gridtile", "video"))
			mVideo->applyTheme(theme, view, "gridtile", ThemeFlags::ALL ^ (ThemeFlags::PATH));
		else if (theme->getElement(view, "gridtile.video", "video"))
			mVideo->applyTheme(theme, view, "gridtile.video", ThemeFlags::ALL ^ (ThemeFlags::PATH));
	}
	else if (mVideo != nullptr)
	{
		removeChild(mVideo);
		delete mVideo;
		mVideo = nullptr;
	}

	// Apply theme to the default gridtile
	const ThemeData::ThemeElement* elem = theme->getElement(view, "default", "gridtile");
	if (elem)
	{
		applyThemeToProperties(elem, mDefaultProperties);
		applyThemeToProperties(elem, mSelectedProperties);		
	}

	// Apply theme to the selected gridtile
	elem = theme->getElement(view, "selected", "gridtile");
	if (elem)
		applyThemeToProperties(elem, mSelectedProperties);
		

	// Apply theme to the <image name="gridtile.image"> element
	elem = theme->getElement(view, "gridtile.image", "image");
	if (elem)
	{
		mImage->applyTheme(theme, view, "gridtile.image", ThemeFlags::ALL ^ (ThemeFlags::PATH));

		mDefaultProperties.Image.applyTheme(elem);
		mSelectedProperties.Image.applyTheme(elem);
	}	

	// Apply theme to the <image name="gridtile.image:selected"> element
	elem = theme->getElement(view, "gridtile.image:selected", "image");
	if (elem)
		mSelectedProperties.Image.applyTheme(elem);
	

	// Apply theme to the <image name="gridtile.marquee"> element
	elem = theme->getElement(view, "gridtile.marquee", "image");
	if (elem)
	{
		mHasStandardMarquee = true;
		createMarquee();
		mMarquee->applyTheme(theme, view, "gridtile.marquee", ThemeFlags::ALL ^ (ThemeFlags::PATH));

		mDefaultProperties.Marquee.applyTheme(elem);
		mSelectedProperties.Marquee = mDefaultProperties.Marquee;

		// Apply theme to the <image name="gridtile.marquee:selected"> element
		elem = theme->getElement(view, "gridtile.marquee:selected", "image");
		if (elem)
			mSelectedProperties.Marquee.applyTheme(elem);
	}
	else if (mMarquee != nullptr)
	{
		mHasStandardMarquee = false;
		removeChild(mMarquee);
		delete mMarquee;
		mMarquee = nullptr;
	}


	// Apply theme to the <image name="gridtile.marquee"> element
	elem = theme->getElement(view, "gridtile.favorite", "image");
	if (elem)
	{
		createFavorite();
		mFavorite->applyTheme(theme, view, "gridtile.favorite", ThemeFlags::ALL);

		mDefaultProperties.Favorite.sizeMode = "size";
		mDefaultProperties.Favorite.applyTheme(elem);
		mSelectedProperties.Favorite = mDefaultProperties.Favorite;

		// Apply theme to the <image name="gridtile.favorite:selected"> element
		elem = theme->getElement(view, "gridtile.favorite:selected", "image");
		if (elem)
			mSelectedProperties.Favorite.applyTheme(elem);
	}
	else if (mFavorite != nullptr)
	{
		removeChild(mFavorite);
		delete mFavorite;
		mFavorite = nullptr;
	}
    
	// Apply theme to the <image name="gridtile.cheevos"> element
	elem = theme->getElement(view, "gridtile.cheevos", "image");
	if (elem)
	{
		createCheevos();
		mCheevos->applyTheme(theme, view, "gridtile.cheevos", ThemeFlags::ALL);

		mDefaultProperties.Cheevos.sizeMode = "size";
		mDefaultProperties.Cheevos.applyTheme(elem);
		mSelectedProperties.Cheevos = mDefaultProperties.Cheevos;

		// Apply theme to the <image name="gridtile.cheevos:selected"> element
		elem = theme->getElement(view, "gridtile.cheevos:selected", "image");
		if (elem)
			mSelectedProperties.Cheevos.applyTheme(elem);
	}
	else if (mCheevos != nullptr)
	{
		removeChild(mCheevos);
		delete mCheevos;
		mCheevos = nullptr;
	}


	// Apply theme to the <image name="gridtile.overlay"> element
	elem = theme->getElement(view, "gridtile.overlay", "image");
	if (elem)
	{
		createImageOverlay();
		mImageOverlay->applyTheme(theme, view, "gridtile.overlay", ThemeFlags::ALL);

		mDefaultProperties.ImageOverlay.sizeMode = "size";
		mDefaultProperties.ImageOverlay.applyTheme(elem);
		mSelectedProperties.ImageOverlay = mDefaultProperties.ImageOverlay;

		// Apply theme to the <image name="gridtile.favorite:selected"> element
		elem = theme->getElement(view, "gridtile.overlay:selected", "image");
		if (elem)
			mSelectedProperties.ImageOverlay.applyTheme(elem);
	}
	else if (mImageOverlay != nullptr)
	{
		removeChild(mImageOverlay);
		delete mImageOverlay;
		mImageOverlay = nullptr;
	}
	

	// Apply theme to the <text name="gridtile"> element
	elem = theme->getElement(view, "gridtile", "text");
	if (elem == nullptr) // Apply theme to the <text name="gridtile.text"> element		
		elem = theme->getElement(view, "gridtile.text", "text");

	if (elem != NULL)
	{
		mLabel.applyTheme(theme, view, element, properties);

		mDefaultProperties.Label.applyTheme(elem);
		mSelectedProperties.Label.applyTheme(elem);		

		bool hasVisible = elem->has("visible");
		mLabelMerged = elem->has("pos");
		if (!mLabelMerged && elem->has("size"))
			mLabelMerged = mDefaultProperties.Label.size.x() == 0;

		// Apply theme to the <text name="gridtile:selected"> element
		elem = theme->getElement(view, "gridtile_selected", "text");
		if (elem == nullptr)
			elem = theme->getElement(view, "gridtile:selected", "text");
		if (elem == nullptr) // Apply theme to the <text name="gridtile.text:selected"> element
			elem = theme->getElement(view, "gridtile.text:selected", "text");

		if (elem)
		{
			mSelectedProperties.Label.applyTheme(elem);
			if (hasVisible && !elem->has("visible"))
				mSelectedProperties.Label.Visible = mDefaultProperties.Label.Visible;
		}
	}

	// Apply theme to the <ninepatch name="gridtile"> element
	elem = theme->getElement(view, "gridtile", "ninepatch");
	if (elem == nullptr) // Apply theme to the <ninepatch name="gridtile.background"> element		
		elem = theme->getElement(view, "gridtile.background", "ninepatch");

	if (elem != NULL)
	{
		mBackground.applyTheme(theme, view, element, properties);
		mDefaultProperties.Background.applyTheme(elem);		
		mSelectedProperties.Background.applyTheme(elem);
	}

	// Apply theme to the <ninepatch name="gridtile:selected"> element
	elem = theme->getElement(view, "gridtile:selected", "ninepatch");
	if (elem == nullptr) // Apply theme to the <ninepatch name="gridtile.background:selected"> element
		elem = theme->getElement(view, "gridtile.background:selected", "ninepatch");

	if (elem)
		mSelectedProperties.Background.applyTheme(elem);

	mVideoPlayingProperties = mSelectedProperties;

	if (!mVideoPlayingProperties.Label.applyTheme(theme->getElement(view, "gridtile:videoplaying", "text")))
		mVideoPlayingProperties.Label.applyTheme(theme->getElement(view, "gridtile.text:videoplaying", "text"));

	mVideoPlayingProperties.Image.applyTheme(theme->getElement(view, "gridtile.image:videoplaying", "image"));
	mVideoPlayingProperties.Marquee.applyTheme(theme->getElement(view, "gridtile.marquee:videoplaying", "image"));
	mVideoPlayingProperties.Favorite.applyTheme(theme->getElement(view, "gridtile.favorite:selected", "image"));
	mVideoPlayingProperties.Cheevos.applyTheme(theme->getElement(view, "gridtile.cheevos:selected", "image"));
	mVideoPlayingProperties.ImageOverlay.applyTheme(theme->getElement(view, "gridtile.overlay:videoplaying", "image"));
}

// Made this a static function because the ImageGridComponent need to know the default tile size
// to calculate the grid dimension before it instantiate the GridTileComponents
Vector2f GridTileComponent::getDefaultTileSize()
{
	Vector2f screen = Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	return screen * 0.22f;
}

Vector2f GridTileComponent::getSelectedTileSize() const
{
	return mDefaultProperties.Size * 1.2f;
}

bool GridTileComponent::isSelected() const
{
	return mSelected;
}

void GridTileComponent::setImage(const std::string& path, bool isDefaultImage)
{
	if (path == ":/folder.svg" || path == ":/cartridge.svg")
		mIsDefaultImage = true;
	else
		mIsDefaultImage = isDefaultImage;

	if (mCurrentPath == path)
		return;

	mCurrentPath = path;
	
	if (mSelectedProperties.Size.x() > mSize.x())
		mImage->setImage(path, false, MaxSizeInfo(mSelectedProperties.Size, mSelectedProperties.Image.sizeMode != "maxSize"), false);
	else
		mImage->setImage(path, false, MaxSizeInfo(mSize, mSelectedProperties.Image.sizeMode != "maxSize"), false);

	resize();
}

void GridTileComponent::setMarquee(const std::string& path)
{
	if (mMarquee == nullptr)
		return;

	if (mCurrentMarquee == path)
		return;

	mCurrentMarquee = path;

	if (mSelectedProperties.Size.x() > mSize.x())
		mMarquee->setImage(path, false, MaxSizeInfo(mSelectedProperties.Size), false);
	else
		mMarquee->setImage(path, false, MaxSizeInfo(mSize), false);

	resize();
}

void GridTileComponent::setCheevos(bool cheevos)
{
	if (mCheevos == nullptr)
		return;

	mCheevos->setVisible(cheevos);
	resize();
}

void GridTileComponent::setFavorite(bool favorite)
{
	if (mFavorite == nullptr)
		return;

	mFavorite->setVisible(favorite);
	resize();
}

void GridTileComponent::resetImages()
{
	setLabel("");
	setImage("");
	setMarquee("");
	stopVideo();
}

void GridTileComponent::setLabel(std::string name)
{
	if (mLabel.getText() == name)
		return;

	mLabel.setText(name);
	resize();
}

void GridTileComponent::setVideo(const std::string& path, float defaultDelay)
{
	if (mVideoPath == path)
		return;

	mVideoPath = path;

	if (mVideo != nullptr)
	{
		if (defaultDelay >= 0.0)
			mVideo->setStartDelay(defaultDelay);

		if (mVideoPath.empty())
			stopVideo();
	}

	resize();
}

void GridTileComponent::onShow()
{
	GuiComponent::onShow();	
	resize();
}

void GridTileComponent::onHide()
{
	GuiComponent::onHide();	
}

void GridTileComponent::startVideo()
{
	if (mVideo != nullptr)
	{
		// Inform video component about size before staring in order to be able to use OptimizeVideo parameter
		if (mSelectedProperties.Image.sizeMode == "minSize")
			mVideo->setMinSize(mSelectedProperties.Size);
		else
			mVideo->setResize(mSelectedProperties.Size);

		mVideo->setVideo(mVideoPath);
	}
}

void GridTileComponent::stopVideo()
{
	if (mVideo != nullptr)
		mVideo->setVideo("");
}

void GridTileComponent::setSelected(bool selected, bool allowAnimation, Vector3f* pPosition, bool force)
{
	if (!isShowing() || !ALLOWANIMATIONS)
		allowAnimation = false;

	if (mSelected == selected && !force)
	{
		if (mSelected)
			startVideo();

		return;
	}

	mSelected = selected;

	if (!mSelected)
	{
		mBackground.setAnimateTiming(0);
		stopVideo();
	}

	if (selected)
	{
		if (pPosition == NULL || !allowAnimation)
		{
			cancelAnimation(3);

			this->setSelectedZoom(1);
			mAnimPosition = Vector3f(0, 0, 0);
			startVideo();

			resize();
		}
		else
		{
			if (pPosition == NULL)
				mAnimPosition = Vector3f(0, 0, 0);
			else
				mAnimPosition = Vector3f(pPosition->x(), pPosition->y(), pPosition->z());

			auto func = [this](float t)
			{
				t -= 1; // cubic ease out
				float pct = Math::lerp(0, 1, t*t*t + 1);

				this->setSelectedZoom(pct);
			};

			cancelAnimation(3);
			setAnimation(new LambdaAnimation(func, 250), 0, [this] {
				this->setSelectedZoom(1);
				mAnimPosition = Vector3f(0, 0, 0);
				startVideo();
			}, false, 3);
		}
	}
	else // if (!selected)
	{
		if (!allowAnimation)
		{
			cancelAnimation(3);
			this->setSelectedZoom(0);
			stopVideo();
			resize();
		}
		else
		{
			this->setSelectedZoom(1);
			stopVideo();

			auto func = [this](float t)
			{
				t -= 1; // cubic ease out
				float pct = Math::lerp(0, 1, t*t*t + 1);
				this->setSelectedZoom(1.0 - pct);
			};

			cancelAnimation(3);
			setAnimation(new LambdaAnimation(func, 250), 0, [this] {
				this->setSelectedZoom(0);
			}, false, 3);
		}
	}
}

void GridTileComponent::setSelectedZoom(float percent)
{
	if (mSelectedZoomPercent == percent)
		return;

	mSelectedZoomPercent = percent;
	resize();
}

Vector3f GridTileComponent::getBackgroundPosition()
{
	return Vector3f(mBackground.getPosition().x() + mPosition.x(), mBackground.getPosition().y() + mPosition.y(), 0);
}

static Vector2f mixVectors(const Vector2f& def, const Vector2f& sel, float percent)
{
	if (def == sel || percent == 0)
		return def;
		
	if (percent == 1)
		return sel;

	float x = def.x() * (1.0 - percent) + sel.x() * percent;
	float y = def.y() * (1.0 - percent) + sel.y() * percent;
	return Vector2f(x, y);	
}

static Vector4f mixVectors(const Vector4f& def, const Vector4f& sel, float percent)
{
	if (def == sel || percent == 0)
		return def;

	if (percent == 1)
		return sel;

	float x = def.x() * (1.0 - percent) + sel.x() * percent;
	float y = def.y() * (1.0 - percent) + sel.y() * percent;
	float z = def.z() * (1.0 - percent) + sel.z() * percent;
	float w = def.w() * (1.0 - percent) + sel.w() * percent;
	return Vector4f(x, y, z, w);
}

static unsigned int mixUnsigned(const unsigned int def, const unsigned int sel, float percent)
{
	if (def == sel || percent == 0)
		return def;

	if (percent == 1)
		return sel;

	return def * (1.0 - percent) + sel * percent;
}

static float mixFloat(const float def, const float sel, float percent)
{
	if (def == sel || percent == 0)
		return def;

	if (percent == 1)
		return sel;

	return def * (1.0 - percent) + sel * percent;
}

void GridImageProperties::mixProperties(GridImageProperties& def, GridImageProperties& sel, float percent)
{
	if (!def.Loaded)
		return;

	using namespace Renderer;

	pos = mixVectors(def.pos, sel.pos, percent);
	size = mixVectors(def.size, sel.size, percent);
	origin = mixVectors(def.origin, sel.origin, percent);
	color = mixColors(def.color, sel.color, percent);
	colorEnd = mixColors(def.colorEnd, sel.colorEnd, percent);
	reflexion = mixVectors(def.reflexion, sel.reflexion, percent);
	roundCorners = mixFloat(def.roundCorners, sel.roundCorners, percent);
}

void GridTextProperties::mixProperties(GridTextProperties& def, GridTextProperties& sel, float percent)
{
	if (!def.Loaded)
		return;

	using namespace Renderer;

	pos = mixVectors(def.pos, sel.pos, percent);
	size = mixVectors(def.size, sel.size, percent);
	color = mixColors(def.color, sel.color, percent);
	backColor = mixColors(def.backColor, sel.backColor, percent);
	glowColor = mixColors(def.glowColor, sel.glowColor, percent);
	glowSize = mixFloat(def.glowSize, sel.glowSize, percent);
	fontSize = mixFloat(def.fontSize, sel.fontSize, percent);
	padding = mixVectors(def.padding, sel.padding, percent);
}

GridTileProperties GridTileComponent::getCurrentProperties(bool mixValues)
{
	GridTileProperties prop = mSelected ? mSelectedProperties : mDefaultProperties;

	if (mSelectedZoomPercent == 0.0f || mSelectedZoomPercent == 1.0f)
		if (!mSelected || (mVideo != nullptr && !mVideo->isPlaying()))
			return prop;

	if (mixValues)
	{
		GridTileProperties* from = &mDefaultProperties;
		GridTileProperties* to = &mSelectedProperties;
		float pc = mSelectedZoomPercent;
		
		if (mSelected && mVideo != nullptr && mVideo->isPlaying())
		{
			if (!mVideo->isFading())
				return mVideoPlayingProperties;

			from = &mSelectedProperties;
			to = &mVideoPlayingProperties;

			float t = mVideo->getFade() - 1; // cubic ease in
			pc = Math::lerp(0, 1, t*t*t + 1);
		}

		prop.Size = mixVectors(from->Size, to->Size, pc);
		prop.Padding = mixVectors(from->Padding, to->Padding, pc);

		prop.Label.mixProperties(from->Label, to->Label, pc);
		prop.Image.mixProperties(from->Image, to->Image, pc);
		prop.Marquee.mixProperties(from->Marquee, to->Marquee, pc);
				
		prop.Favorite.mixProperties(from->Favorite, to->Favorite, pc);
		prop.Cheevos.mixProperties(from->Cheevos, to->Cheevos, pc);
		prop.ImageOverlay.mixProperties(from->ImageOverlay, to->ImageOverlay, pc);
	}
	else if (mSelected && mVideo != nullptr && mVideo->isPlaying() && !mVideo->isFading())
		return mVideoPlayingProperties;

	return prop;
}

void GridTileComponent::onScreenSaverActivate()
{
	GuiComponent::onScreenSaverActivate();

	if (mVideo)
		mVideo->onScreenSaverActivate();
}

void GridTileComponent::onScreenSaverDeactivate()
{
	GuiComponent::onScreenSaverDeactivate();

	if (mVideo)
		mVideo->onScreenSaverDeactivate();
}

void GridTileComponent::forceMarquee(const std::string& path)
{
	if (mHasStandardMarquee)
	{
		setMarquee(path);
		return;
	}

	bool isMarqueeForced = !path.empty();

	if (isMarqueeForced)
	{
		if (mMarquee == nullptr)
		{
			createMarquee();

			mMarquee->setOrigin(0.5, 0.5);
			mMarquee->setPosition(0.5, 0.5);
			mMarquee->setSize(0.5, 0.5);
			mMarquee->setIsLinear(true);

			mDefaultProperties.Marquee = GridImageProperties();
			mDefaultProperties.Marquee.size = Vector2f(0.55f, 0.55f);
			mDefaultProperties.Marquee.Visible = true;
			mDefaultProperties.Marquee.Loaded = true;
			mSelectedProperties.Marquee = mDefaultProperties.Marquee;
			mSelectedProperties.Marquee.size = Vector2f(0.65f, 0.65f);
		}

		setMarquee(path);
	}
	else if (mMarquee != nullptr)
	{
		removeChild(mMarquee);
		delete mMarquee;
		mMarquee = nullptr;

		mCurrentMarquee = "";
	}	
}

Vector3f GridTileComponent::getLaunchTarget()
{
	return Vector3f(getCenter().x(), getCenter().y(), 0);
}