#include "components/ImageComponent.h"

#include "resources/TextureResource.h"
#include "Log.h"
#include "Settings.h"
#include "ThemeData.h"
#include "LocaleES.h"
#include "utils/FileSystemUtil.h"

Vector2i ImageComponent::getTextureSize() const
{
	if(mTexture)
		return mTexture->getSize();
	else
		return Vector2i::Zero();
}

Vector2f ImageComponent::getSize() const
{
	return GuiComponent::getSize() * (mBottomRightCrop - mTopLeftCrop);
}

ImageComponent::ImageComponent(Window* window, bool forceLoad, bool dynamic) : GuiComponent(window),
	mTargetIsMax(false), mTargetIsMin(false), mFlipX(false), mFlipY(false), mTargetSize(0, 0), mColorShift(0xFFFFFFFF),
	mColorShiftEnd(0xFFFFFFFF), mColorGradientHorizontal(true), mForceLoad(forceLoad), mDynamic(dynamic),
	mFadeOpacity(0), mFading(false), mRotateByTargetSize(false), mTopLeftCrop(0.0f, 0.0f), mBottomRightCrop(1.0f, 1.0f),
	mReflection(0.0f, 0.0f), mPadding(Vector4f(0, 0, 0, 0))
{
	mLinear = false;
	mHorizontalAlignment = ALIGN_CENTER;
	mVerticalAlignment = ALIGN_CENTER;
	mReflectOnBorders = false;
	mAllowFading = true;
	mRoundCorners = 0.0f;
	mShowing = false;
	mPlaylistTimer = 0;
	updateColors();
}

ImageComponent::~ImageComponent()
{
	if (mTexture != nullptr)
		mTexture->setRequired(false);
}

void ImageComponent::resize()
{
	if(!mTexture)
		return;

	const Vector2f textureSize = mTexture->getSourceImageSize();
	if(textureSize == Vector2f::Zero())
		return;

	if(mTexture->isTiled())
	{
		uncrop();
		mSize = mTargetSize;
	}
	else
	{
		// SVG rasterization is determined by height (see SVGResource.cpp), and rasterization is done in terms of pixels
		// if rounding is off enough in the rasterization step (for images with extreme aspect ratios), it can cause cutoff when the aspect ratio breaks
		// so, we always make sure the resultant height is an integer to make sure cutoff doesn't happen, and scale width from that 
		// (you'll see this scattered throughout the function)
		// this is probably not the best way, so if you're familiar with this problem and have a better solution, please make a pull request!

		if(mTargetIsMax)
		{
			uncrop();
			mSize = textureSize;

			Vector2f resizeScale((mTargetSize.x() / mSize.x()), (mTargetSize.y() / mSize.y()));

			if(resizeScale.x() < resizeScale.y())
			{
				mSize[0] *= resizeScale.x(); // this will be mTargetSize.x(). We can't exceed it, nor be lower than it.
				// we need to make sure we're not creating an image larger than max size
				mSize[1] = Math::min(Math::round(mSize[1] *= resizeScale.x()), mTargetSize.y());
			}else{
				mSize[1] = Math::round(mSize[1] * resizeScale.y()); // this will be mTargetSize.y(). We can't exceed it.
				
				// for SVG rasterization, always calculate width from rounded height (see comment above)
				// we need to make sure we're not creating an image larger than max size
				mSize[0] = Math::min((mSize[1] / textureSize.y()) * textureSize.x(), mTargetSize.x());
			}
		}else if(mTargetIsMin)
		{
			mSize = textureSize;

			Vector2f resizeScale((mTargetSize.x() / mSize.x()), (mTargetSize.y() / mSize.y()));

			if(resizeScale.x() > resizeScale.y())
			{
				mSize[0] *= resizeScale.x();
				mSize[1] *= resizeScale.x();

				float cropPercent = (mSize.y() - mTargetSize.y()) / (mSize.y() * 2);
				crop(0, cropPercent, 0, cropPercent);
			}else{
				mSize[0] *= resizeScale.y();
				mSize[1] *= resizeScale.y();

				float cropPercent = (mSize.x() - mTargetSize.x()) / (mSize.x() * 2);
				crop(cropPercent, 0, cropPercent, 0);
			}

			// for SVG rasterization, always calculate width from rounded height (see comment above)
			// we need to make sure we're not creating an image smaller than min size
			mSize[1] = Math::max(Math::round(mSize[1]), mTargetSize.y());
			mSize[0] = Math::max((mSize[1] / textureSize.y()) * textureSize.x(), mTargetSize.x());

		}
		else
		{
			uncrop();
			// if both components are set, we just stretch
			// if no components are set, we don't resize at all
			mSize = mTargetSize == Vector2f::Zero() ? textureSize : mTargetSize;

			// if only one component is set, we resize in a way that maintains aspect ratio
			// for SVG rasterization, we always calculate width from rounded height (see comment above)
			if(!mTargetSize.x() && mTargetSize.y())
			{
				mSize[1] = Math::round(mTargetSize.y());
				mSize[0] = (mSize.y() / textureSize.y()) * textureSize.x();
			}else if(mTargetSize.x() && !mTargetSize.y())
			{
				mSize[1] = Math::round((mTargetSize.x() / textureSize.x()) * textureSize.y());
				mSize[0] = (mSize.y() / textureSize.y()) * textureSize.x();
			}
		}
	}

	mSize[0] = Math::round(mSize.x());
	mSize[1] = Math::round(mSize.y());
	// mSize.y() should already be rounded
	mTexture->rasterizeAt((size_t)mSize.x(), (size_t)mSize.y());

	onSizeChanged();
}

void ImageComponent::onSizeChanged()
{
	updateVertices();
}

void ImageComponent::setDefaultImage(std::string path)
{
	mDefaultPath = path;
}

void ImageComponent::setImage(std::string path, bool tile, MaxSizeInfo maxSize)
{
	std::string canonicalPath = Utils::FileSystem::getCanonicalPath(path);
	if (mPath == canonicalPath)
		return;
	
	mPath = canonicalPath;

	if (mTexture != nullptr)
		mTexture->setRequired(false);

	// If the previous image is in the async queue, remove it
	TextureResource::cancelAsync(mLoadingTexture);
	TextureResource::cancelAsync(mTexture);
	mLoadingTexture.reset();

	if (mPath.empty() || !ResourceManager::getInstance()->fileExists(mPath))
	{
		if(mDefaultPath.empty() || !ResourceManager::getInstance()->fileExists(mDefaultPath))
			mTexture.reset();
		else
			mTexture = TextureResource::get(mDefaultPath, tile, mLinear, mForceLoad, mDynamic, true, maxSize.empty() ? nullptr : &maxSize);
	} 
	else
	{
		std::shared_ptr<TextureResource> texture = TextureResource::get(mPath, tile, mLinear, mForceLoad, mDynamic, true, maxSize.empty() ? nullptr : &maxSize);

		if (!mForceLoad && mDynamic && !mAllowFading && texture != nullptr && !texture->isLoaded())
			mLoadingTexture = texture;
		else
			mTexture = texture;
	}

	if (mShowing && mTexture != nullptr)
		mTexture->setRequired(true);

	if (mLoadingTexture == nullptr)
		resize();
}

void ImageComponent::setImage(const char* path, size_t length, bool tile)
{
	if (mTexture != nullptr)
		mTexture->setRequired(false);

	mTexture.reset();

	mTexture = TextureResource::get("", tile);
	mTexture->initFromMemory(path, length);
	
	resize();
}

void ImageComponent::setImage(const std::shared_ptr<TextureResource>& texture)
{
	if (mTexture != nullptr)
		mTexture->setRequired(false);

	mTexture = texture;

	if (mShowing && mTexture != nullptr)
		mTexture->setRequired(true);

	resize();
}

void ImageComponent::setResize(float width, float height)
{
	if (mSize.x() != 0 && mSize.y() != 0 && !mTargetIsMax && !mTargetIsMin && mTargetSize.x() == width && mTargetSize.y() == height)
		return;

	mTargetSize = Vector2f(width, height);
	mTargetIsMax = false;
	mTargetIsMin = false;
	resize();
}

void ImageComponent::setMaxSize(float width, float height)
{
	if (mSize.x() != 0 && mSize.y() != 0 && mTargetIsMax && !mTargetIsMin && mTargetSize.x() == width && mTargetSize.y() == height)
		return;

	mTargetSize = Vector2f(width, height);
	mTargetIsMax = true;
	mTargetIsMin = false;
	resize();
}

void ImageComponent::setMinSize(float width, float height)
{
	if (mSize.x() != 0 && mSize.y() != 0 && mTargetIsMin && !mTargetIsMax && mTargetSize.x() == width && mTargetSize.y() == height)
		return;

	mTargetSize = Vector2f(width, height);
	mTargetIsMax = false;
	mTargetIsMin = true;
	resize();
}

Vector2f ImageComponent::getRotationSize() const
{
	return mRotateByTargetSize ? mTargetSize : mSize;
}

void ImageComponent::setRotateByTargetSize(bool rotate)
{
	mRotateByTargetSize = rotate;
}

void ImageComponent::cropLeft(float percent)
{
	assert(percent >= 0.0f && percent <= 1.0f);
	mTopLeftCrop.x() = percent;
}

void ImageComponent::cropTop(float percent)
{
	assert(percent >= 0.0f && percent <= 1.0f);
	mTopLeftCrop.y() = percent;
}

void ImageComponent::cropRight(float percent)
{
	assert(percent >= 0.0f && percent <= 1.0f);
	mBottomRightCrop.x() = 1.0f - percent;
}

void ImageComponent::cropBot(float percent)
{
	assert(percent >= 0.0f && percent <= 1.0f);
	mBottomRightCrop.y() = 1.0f - percent;
}

void ImageComponent::crop(float left, float top, float right, float bot)
{
	cropLeft(left);
	cropTop(top);
	cropRight(right);
	cropBot(bot);
}

void ImageComponent::uncrop()
{
	crop(0, 0, 0, 0);
}

void ImageComponent::setFlipX(bool flip)
{
	mFlipX = flip;
	updateVertices();
}

void ImageComponent::setFlipY(bool flip)
{
	mFlipY = flip;
	updateVertices();
}

void ImageComponent::setColorShift(unsigned int color)
{
	mColorShift = color;
	mColorShiftEnd = color;
	updateColors();
}

void ImageComponent::setColorShiftEnd(unsigned int color)
{
	mColorShiftEnd = color;
	updateColors();
}

void ImageComponent::setColorGradientHorizontal(bool horizontal)
{
	mColorGradientHorizontal = horizontal;
	updateColors();
}

void ImageComponent::setOpacity(unsigned char opacity)
{
	mOpacity = opacity;	
	updateColors();
}

void ImageComponent::updateVertices()
{
	if(!mTexture)
		return;

	// we go through this mess to make sure everything is properly rounded
	// if we just round vertices at the end, edge cases occur near sizes of 0.5
	const Vector2f     topLeft     = { mSize * mTopLeftCrop };
	const Vector2f     bottomRight = { mSize * mBottomRightCrop };
	const float        px          = mTexture->isTiled() ? mSize.x() / getTextureSize().x() : 1.0f;
	const float        py          = mTexture->isTiled() ? mSize.y() / getTextureSize().y() : 1.0f;
	const unsigned int color       = Renderer::convertColor(mColorShift);
	const unsigned int colorEnd    = Renderer::convertColor(mColorShiftEnd);
	

	mVertices[0] = { { topLeft.x() + mPadding.x(),     topLeft.y() + mPadding.y()     },
		{ mTopLeftCrop.x(),          py   - mTopLeftCrop.y()     }, color };

	mVertices[1] = { { topLeft.x() + mPadding.x(),     bottomRight.y() - mPadding.w() }, 
		{ mTopLeftCrop.x(),          1.0f - mBottomRightCrop.y() }, mColorGradientHorizontal ? colorEnd : color };

	mVertices[2] = { { bottomRight.x() - mPadding.z(), topLeft.y() + mPadding.y()	},
		{ mBottomRightCrop.x() * px, py   - mTopLeftCrop.y()     }, mColorGradientHorizontal ? color : colorEnd };

	mVertices[3] = { { bottomRight.x() - mPadding.z(), bottomRight.y() - mPadding.w() }, 
	{ mBottomRightCrop.x() * px, 1.0f - mBottomRightCrop.y() }, color };

	// round vertices
	for(int i = 0; i < 4; ++i)
		mVertices[i].pos.round();

	if(mFlipX)
	{
		for(int i = 0; i < 4; ++i)
			mVertices[i].tex[0] = px - mVertices[i].tex[0];
	}

	if(mFlipY)
	{
		for(int i = 0; i < 4; ++i)
			mVertices[i].tex[1] = py - mVertices[i].tex[1];
	}

	updateColors();
}

void ImageComponent::updateColors()
{
	float opacity = (mOpacity * (mFading ? mFadeOpacity / 255.0 : 1.0)) / 255.0;

	const unsigned int color = Renderer::convertColor(mColorShift & 0xFFFFFF00 | (unsigned char)((mColorShift & 0xFF) * opacity));
	const unsigned int colorEnd = Renderer::convertColor(mColorShiftEnd & 0xFFFFFF00 | (unsigned char)((mColorShiftEnd & 0xFF) * opacity));
	
	mVertices[0].col = color;
	mVertices[1].col = mColorGradientHorizontal ? colorEnd : color;
	mVertices[2].col = mColorGradientHorizontal ? color : colorEnd;
	mVertices[3].col = colorEnd;
}

void ImageComponent::render(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	if (mLoadingTexture != nullptr && mLoadingTexture->isLoaded())
	{
		if (mTexture != nullptr)
			mTexture->setRequired(false);

		mTexture = mLoadingTexture;

		if (mShowing && mTexture != nullptr)
			mTexture->setRequired(true);

		mLoadingTexture.reset();
		resize();
		updateColors();
	}

	Transform4x4f trans = parentTrans * getTransform();
	
	// Don't use soft clip if rotation applied : let renderer do the work
	if (mRotation == 0 && !Renderer::isVisibleOnScreen(trans.translation().x(), trans.translation().y(), mSize.x(), mSize.y()))
		return;

	Renderer::setMatrix(trans);

	if(mTexture && mOpacity > 0)
	{
		Vector2f targetSizePos = (mTargetSize - mSize) * mOrigin * -1;

		if(Settings::getInstance()->getBool("DebugImage")) 
		{
			Renderer::drawRect(targetSizePos.x(), targetSizePos.y(), mTargetSize.x(), mTargetSize.y(), 0xFF000033, 0xFF000033);
			Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), 0x00000033, 0x00000033);
		}

		// actually draw the image
		// The bind() function returns false if the texture is not currently loaded. A blank
		// texture is bound in this case but we want to handle a fade so it doesn't just 'jump' in
		// when it finally loads
		if (!mTexture->bind())
		{
			fadeIn(false);
			return;
		}

		// Align left
		//trans.translate(Vector3f(targetSizePos.x(), 0, 0.0f));
		
		if (mVerticalAlignment == ALIGN_TOP)
			trans.translate(Vector3f(0, targetSizePos.y(), 0.0f));
		else if (mVerticalAlignment == ALIGN_BOTTOM)
			trans.translate(Vector3f(targetSizePos.x(), targetSizePos.y() + mTargetSize.y() - mSize.y(), 0.0f));

		if (mHorizontalAlignment == ALIGN_LEFT)
			trans.translate(Vector3f(targetSizePos.x(), 0, 0.0f));
		else if (mHorizontalAlignment == ALIGN_RIGHT)
			trans.translate(Vector3f(targetSizePos.x() + mTargetSize.x() - mSize.x(), targetSizePos.y(), 0.0f));

		Renderer::setMatrix(trans);

		fadeIn(true);

		if (mRoundCorners > 0)
		{
			float x = 0;
			float y = 0;
			float size_x = mSize.x();
			float size_y = mSize.y();


			if (mTargetIsMin)
			{
				x = targetSizePos.x();
				y = targetSizePos.y();
				size_x = mTargetSize.x();
				size_y = mTargetSize.y();
			}

			float radius = Math::max(size_x, size_y) * mRoundCorners;

			Renderer::enableRoundCornerStencil(x, y, size_x, size_y, radius);

			mTexture->bind();
		}

		Renderer::drawTriangleStrips(&mVertices[0], 4);

		if (mRoundCorners > 0)
			Renderer::disableStencil();

		if (mReflection.x() != 0 || mReflection.y() != 0)
		{
			float baseOpacity = (mOpacity * (mFading ? mFadeOpacity / 255.0 : 1.0)) / 255.0;

			float alpha = baseOpacity * ((mColorShift & 0x000000ff)) / 255.0;
			float alpha2 = baseOpacity * alpha * mReflection.y();

			alpha *= mReflection.x();

			const unsigned int colorT = Renderer::convertColor((mColorShift & 0xffffff00) + (unsigned char)(255.0*alpha));
			const unsigned int colorB = Renderer::convertColor((mColorShift & 0xffffff00) + (unsigned char)(255.0*alpha2));

			int h = mVertices[1].pos.y() - mVertices[0].pos.y();

			if (mReflectOnBorders)
				h = mTargetSize.y();

			Renderer::Vertex mirrorVertices[4];

			mirrorVertices[0] = {
				{ mVertices[0].pos.x(), mVertices[0].pos.y() + h },
				{ mVertices[0].tex.x(), mVertices[1].tex.y() },
				colorT };

			mirrorVertices[1] = {
				{ mVertices[1].pos.x(), mVertices[1].pos.y() + h },
				{ mVertices[1].tex.x(), mVertices[0].tex.y() },
				colorB };

			mirrorVertices[2] = {
				{ mVertices[2].pos.x(), mVertices[2].pos.y() + h },
				{ mVertices[2].tex.x(), mVertices[3].tex.y() },
				colorT };

			mirrorVertices[3] = {
				{ mVertices[3].pos.x(), mVertices[3].pos.y() + h },
				{ mVertices[3].tex.x(), mVertices[2].tex.y() },
				colorB };

			Renderer::drawTriangleStrips(&mirrorVertices[0], 4);
		}

		Renderer::bindTexture(0);
	}

	GuiComponent::renderChildren(trans);
}

void ImageComponent::fadeIn(bool textureLoaded)
{
	if (!mAllowFading)
		return;

	if (!mForceLoad)
	{
		if (!textureLoaded)
		{
			// Start the fade if this is the first time we've encountered the unloaded texture
			if (!mFading)
			{
				// Start with a zero opacity and flag it as fading
				mFadeOpacity = 0;
				mFading = true;
				
				updateColors();
			}
		}
		else if (mFading && textureLoaded)
		{
			// The texture is loaded and we need to fade it in. The fade is based on the frame rate
			// and is 1/4 second if running at 60 frames per second although the actual value is not
			// that important
			int opacity = mFadeOpacity + 255 / 10;
			// See if we've finished fading
			if (opacity >= 255)
			{
				mFadeOpacity = 255;
				mFading = false;
			}
			else
			{
				mFadeOpacity = (unsigned char)opacity;
			}

			// Apply the combination of the target opacity and current fade			
			updateColors();
		}
	}
}

bool ImageComponent::hasImage()
{
	return (bool)mTexture;
}

void ImageComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	using namespace ThemeFlags;

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "image");
	if (!elem)
	{
		return;
	}

	if (elem->has("linearSmooth"))
		mLinear = elem->get<bool>("linearSmooth");

	Vector2f scale = getParent() ? getParent()->getSize() : Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	if (properties & POSITION && elem->has("pos"))
	{
		Vector2f denormalized = elem->get<Vector2f>("pos") * scale;
		setPosition(Vector3f(denormalized.x(), denormalized.y(), 0));
	}

	if (properties & ThemeFlags::SIZE)
	{
		if (elem->has("size"))
		{
			auto sz = elem->get<Vector2f>("size");
			if (sz.x() == 0 && sz.y() != 0 && Settings::getInstance()->getInt("ScreenRotate") != 0)
			{
				sz.x() = sz.y();
				setMinSize(sz * scale);
			} 
			else if (sz.y() == 0 && sz.x() != 0 && Settings::getInstance()->getInt("ScreenRotate") != 0)
			{
				sz.y() = sz.x();
				setMinSize(sz * scale);
			}
			else
				setResize(sz * scale);
		}
		else if (elem->has("maxSize"))
			setMaxSize(elem->get<Vector2f>("maxSize") * scale);
		else if (elem->has("minSize"))
			setMinSize(elem->get<Vector2f>("minSize") * scale);
	}

	// position + size also implies origin
	if ((properties & ORIGIN || (properties & POSITION && properties & ThemeFlags::SIZE)) && elem->has("origin"))
		setOrigin(elem->get<Vector2f>("origin"));

	if (elem->has("default")) {
		setDefaultImage(elem->get<std::string>("default"));
	}

	if (properties & COLOR)
	{
		if (elem->has("color"))
		{
			setColorShift(elem->get<unsigned int>("color"));
			setColorShiftEnd(elem->get<unsigned int>("color"));
		}

		if (elem->has("colorEnd"))
			setColorShiftEnd(elem->get<unsigned int>("colorEnd"));

		if (elem->has("gradientType"))
			setColorGradientHorizontal(elem->get<std::string>("gradientType").compare("horizontal"));
		
		if (elem->has("reflexion"))
			mReflection = elem->get<Vector2f>("reflexion");
		else
			mReflection = Vector2f::Zero();

		if (elem->has("reflexionOnFrame"))
			mReflectOnBorders = elem->get<bool>("reflexionOnFrame");
		else
			mReflectOnBorders = false;

		if (elem->has("opacity"))
			setOpacity((unsigned char) (elem->get<float>("opacity") * 255.0));		
	}	

	if(properties & ThemeFlags::ROTATION) 
	{
		if(elem->has("rotation"))
			setRotationDegrees(elem->get<float>("rotation"));

		if(elem->has("rotationOrigin"))
			setRotationOrigin(elem->get<Vector2f>("rotationOrigin"));

		if (elem->has("flipX"))
			setFlipX(elem->get<bool>("flipX"));

		if (elem->has("flipY"))
			setFlipY(elem->get<bool>("flipY"));
	}

	if (properties & ALIGNMENT && elem->has("horizontalAlignment"))
	{
		std::string str = elem->get<std::string>("horizontalAlignment");
		if (str == "left")
			setHorizontalAlignment(ALIGN_LEFT);
		else if (str == "right")
			setHorizontalAlignment(ALIGN_RIGHT);
		else
			setHorizontalAlignment(ALIGN_CENTER);		
	}

	if (properties & ALIGNMENT && elem->has("verticalAlignment"))
	{
		std::string str = elem->get<std::string>("verticalAlignment");
		if (str == "top")
			setVerticalAlignment(ALIGN_TOP);
		else if (str == "bottom")
			setVerticalAlignment(ALIGN_BOTTOM);
		else
			setVerticalAlignment(ALIGN_CENTER);
	}

	if (properties & ALIGNMENT && elem->has("roundCorners"))
		mRoundCorners = elem->get<float>("roundCorners");

	if(properties & ThemeFlags::Z_INDEX && elem->has("zIndex"))
		setZIndex(elem->get<float>("zIndex"));
	else
		setZIndex(getDefaultZIndex());

	if(properties & ThemeFlags::VISIBLE && elem->has("visible"))
		setVisible(elem->get<bool>("visible"));
	else
		setVisible(true);

	if (properties & PATH && elem->has("path"))
	{
		auto path = elem->get<std::string>("path");
		if (ResourceManager::getInstance()->fileExists(path))
		{
			bool tile = (elem->has("tile") && elem->get<bool>("tile"));
			if(tile)
				setImage(path, true);
			else
			{
				auto sz = getMaxSizeInfo();
				if (!mLinear && sz.x() > 32 && sz.y() > 32)
				{
					mPath = "";
					setImage(path, tile, sz);
				}
				else
					setImage(path, false);
			}
		}
	}
}

std::vector<HelpPrompt> ImageComponent::getHelpPrompts()
{
	std::vector<HelpPrompt> ret;
	ret.push_back(HelpPrompt(BUTTON_OK, _("SELECT")));
	return ret;
}

void ImageComponent::setPlaylist(std::shared_ptr<IPlaylist> playList)
{
	mPlaylist = playList;
	if (mPlaylist == nullptr)
		return;

	auto image = mPlaylist->getNextItem();
	if (!image.empty())
		setImage(image);
}

void ImageComponent::onShow()
{
	GuiComponent::onShow();

	if (mTexture != nullptr)
		mTexture->setRequired(true);

	if (!mShowing && mPlaylist != nullptr && !mPath.empty() && mPlaylist->getRotateOnShow())
	{
		auto item = mPlaylist->getNextItem();
		if (!item.empty())
			setImage(item, false, getMaxSizeInfo());
	}

	mShowing = true;
}

void ImageComponent::onHide()
{
	GuiComponent::onHide();

	if (mTexture != nullptr)
		mTexture->setRequired(false);

	mShowing = false;
}

void ImageComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mPlaylist != nullptr && mShowing)
	{
		mPlaylistTimer += deltaTime;

		if (mPlaylistTimer >= mPlaylist->getDelay())
		{
			auto item = mPlaylist->getNextItem();
			if (!item.empty())
				setImage(item, false, getMaxSizeInfo());

			mPlaylistTimer = 0.0;
		}
	}
}

bool ImageComponent::isTiled()
{ 
	return mTexture != nullptr && mTexture->isTiled(); 
}