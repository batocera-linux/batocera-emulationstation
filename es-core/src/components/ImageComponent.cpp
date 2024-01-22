#include "components/ImageComponent.h"

#include "resources/TextureResource.h"
#include "Log.h"
#include "Settings.h"
#include "ThemeData.h"
#include "LocaleES.h"
#include "utils/FileSystemUtil.h"
#include "playlists/AnimatedGifPlaylist.h"
#include "playlists/M3uPlaylist.h"
#include "utils/StringUtil.h"

Vector2i ImageComponent::getTextureSize() const
{
	if(mTexture)
		return mTexture->getSize();
	else
		return Vector2i::Zero();
}

Vector2f ImageComponent::getSize() const
{
	if (mTargetIsMax && mPadding != Vector4f::Zero())
	{
		auto targetSize = mTargetSize - mPadding.xy() - mPadding.zw();

		if (mSize.x() == targetSize.x())
			return Vector2f(mSize.x() + mPadding.x() + mPadding.z(), mSize.y());
		else if (mSize.y() == targetSize.y())
			return Vector2f(mSize.x(), mSize.y() + mPadding.y() + mPadding.w());
	}

	return GuiComponent::getSize() * (mBottomRightCrop - mTopLeftCrop);
}

ImageComponent::ImageComponent(Window* window, bool forceLoad, bool dynamic) : GuiComponent(window),
	mTargetIsMax(false), mTargetIsMin(false), mFlipX(false), mFlipY(false), mTargetSize(0, 0), mColorShift(0xFFFFFFFF),
	mColorShiftEnd(0xFFFFFFFF), mColorGradientHorizontal(true), mForceLoad(forceLoad), mDynamic(dynamic),
	mFadeOpacity(0), mFading(false), mRotateByTargetSize(false), mTopLeftCrop(0.0f, 0.0f), mBottomRightCrop(1.0f, 1.0f),
	mReflection(0.0f, 0.0f), mSharedTexture(true)
{
	mSaturation = 1.0f;
	mScaleOrigin = Vector2f::Zero();
	mCheckClipping = true;

	mLinear = false;
	mHorizontalAlignment = ALIGN_CENTER;
	mVerticalAlignment = ALIGN_CENTER;
	mReflectOnBorders = false;
	mAllowFading = true;
	mRoundCorners = 0.0f;
	
	mPlaylistTimer = 0;
	updateColors();
}

ImageComponent::~ImageComponent()
{
	if (mTexture != nullptr)
		mTexture->setRequired(false);
}

void ImageComponent::setSize(float w, float h)
{
	if (!mTexture)
	{
		GuiComponent::setSize(w, h);
		return;
	}

	auto clientSize = getClientRect();

	mTargetSize = Vector2f(w, h);
	mSize = mTargetSize;

	resize();
	onSizeChanged();

	if (mChildren.size() && clientSize  != getClientRect())
		recalcChildrenLayout();
}

void ImageComponent::resize()
{
	if (!mTexture)
		return;

	const Vector2f textureSize = mTexture->getPhysicalSize();
	if (textureSize == Vector2f::Zero())
		return;

	auto targetSize = mTargetSize - mPadding.xy() - mPadding.zw();

	if (mTexture->isTiled())
	{
		uncrop();
		mSize = targetSize;
	}
	else
	{
		// SVG rasterization is determined by height (see SVGResource.cpp), and rasterization is done in terms of pixels
		// if rounding is off enough in the rasterization step (for images with extreme aspect ratios), it can cause cutoff when the aspect ratio breaks
		// so, we always make sure the resultant height is an integer to make sure cutoff doesn't happen, and scale width from that 
		// (you'll see this scattered throughout the function)
		// this is probably not the best way, so if you're familiar with this problem and have a better solution, please make a pull request!

		if (mTargetIsMax)
		{
			uncrop();
			mSize = textureSize;

			Vector2f resizeScale((targetSize.x() / mSize.x()), (targetSize.y() / mSize.y()));

			if (resizeScale.x() < resizeScale.y())
			{
				mSize[0] *= resizeScale.x(); // this will be mTargetSize.x(). We can't exceed it, nor be lower than it.
				// we need to make sure we're not creating an image larger than max size
				//mSize[1] = Math::min(Math::round(mSize[1] *= resizeScale.x()), mTargetSize.y());
				mSize[1] = Math::min(mSize[1] *= resizeScale.x(), targetSize.y());
			}
			else
			{
				//mSize[1] = Math::round(mSize[1] * resizeScale.y()); // this will be mTargetSize.y(). We can't exceed it.
				mSize[1] = mSize[1] * resizeScale.y(); // this will be mTargetSize.y(). We can't exceed it.

				// for SVG rasterization, always calculate width from rounded height (see comment above)
				// we need to make sure we're not creating an image larger than max size
				mSize[0] = Math::min((mSize[1] / textureSize.y()) * textureSize.x(), targetSize.x());
			}
		}
		else if (mTargetIsMin)
		{
			// mSize = ImageIO::getPictureMinSize(textureSize, mTargetSize);			
			mSize = textureSize;

			Vector2f resizeScale((targetSize.x() / mSize.x()), (targetSize.y() / mSize.y()));

			if (resizeScale.x() > resizeScale.y())
			{
				mSize[0] *= resizeScale.x();
				mSize[1] *= resizeScale.x();

				float cropPercent = (mSize.y() - targetSize.y()) / (mSize.y() * 2);
				crop(0, cropPercent, 0, cropPercent);
			}
			else
			{
				mSize[0] *= resizeScale.y();
				mSize[1] *= resizeScale.y();

				float cropPercent = (mSize.x() - targetSize.x()) / (mSize.x() * 2);
				crop(cropPercent, 0, cropPercent, 0);
			}

			// for SVG rasterization, always calculate width from rounded height (see comment above)
			// we need to make sure we're not creating an image smaller than min size
			// mSize[1] = Math::max(Math::round(mSize[1]), mTargetSize.y());
			// mSize[0] = Math::max((mSize[1] / textureSize.y()) * textureSize.x(), mTargetSize.x());
		}
		else
		{
			uncrop();
			// if both components are set, we just stretch
			// if no components are set, we don't resize at all
			mSize = targetSize == Vector2f::Zero() ? textureSize : targetSize;

			// if only one component is set, we resize in a way that maintains aspect ratio
			// for SVG rasterization, we always calculate width from rounded height (see comment above)
			if (!targetSize.x() && targetSize.y())
			{
				//mSize[1] = Math::round(mTargetSize.y());
				mSize[1] = targetSize.y();
				mSize[0] = (mSize.y() / textureSize.y()) * textureSize.x();
			}
			else if (targetSize.x() && !targetSize.y())
			{
				//mSize[1] = Math::round((mTargetSize.x() / textureSize.x()) * textureSize.y());
				mSize[1] = (targetSize.x() / textureSize.x()) * textureSize.y();
				mSize[0] = (mSize.y() / textureSize.y()) * textureSize.x();
			}
		}
	}

	//mSize[0] = Math::round(mSize.x());
	//mSize[1] = Math::round(mSize.y());	
	//mTexture->rasterizeAt((size_t)mSize.x(), (size_t)mSize.y());

	mTexture->rasterizeAt((size_t)Math::round(mSize.x()), (size_t)Math::round(mSize.y()));
	onSizeChanged();
}

void ImageComponent::updateVertices()
{
	if (!mTexture)
		return;

	Vector2f     topLeft = mSize * mTopLeftCrop;
	Vector2f     bottomRight = mSize * mBottomRightCrop;

	Vector2f paddingOffset;

	if (mPadding != Vector4f::Zero())
	{
		paddingOffset = mPadding.xy() - (mPadding.xy() + mPadding.zw()) * mOrigin;
		topLeft += paddingOffset;
		bottomRight += paddingOffset;
	}

	const float        px = mTexture->isTiled() ? mSize.x() / getTextureSize().x() : 1.0f;
	const float        py = mTexture->isTiled() ? mSize.y() / getTextureSize().y() : 1.0f;
	const unsigned int color = Renderer::convertColor(mColorShift);
	const unsigned int colorEnd = Renderer::convertColor(mColorShiftEnd);

	mVertices[0] = {
		{ topLeft.x(),					topLeft.y()  },
		{ mTopLeftCrop.x(),				py - mTopLeftCrop.y()     }, color };

	mVertices[1] = {
		{ topLeft.x(),					bottomRight.y() },
		{ mTopLeftCrop.x(),				1.0f - mBottomRightCrop.y() }, mColorGradientHorizontal ? colorEnd : color };

	mVertices[2] = {
		{ bottomRight.x(),				topLeft.y()	},
		{ mBottomRightCrop.x() * px,	py - mTopLeftCrop.y()     }, mColorGradientHorizontal ? color : colorEnd };

	mVertices[3] = {
		{ bottomRight.x(),				bottomRight.y() },
		{ mBottomRightCrop.x() * px,    1.0f - mBottomRightCrop.y() }, color };

	// Fix vertices for min Target
	if (mTargetIsMin)
	{
		auto targetSize = mTargetSize - mPadding.xy() - mPadding.zw();
		Vector2f targetSizePos = (mSize - targetSize) * mOrigin + paddingOffset;

		float x = targetSizePos.x();
		float y = targetSizePos.y();
		float r = x + targetSize.x();
		float b = y + targetSize.y();

		mVertices[0].pos[0] = x;
		mVertices[0].pos[1] = y;

		mVertices[1].pos[0] = x;
		mVertices[1].pos[1] = b;

		mVertices[2].pos[0] = r;
		mVertices[2].pos[1] = y;

		mVertices[3].pos[0] = r;
		mVertices[3].pos[1] = b;
	}
	
	if (mTexture && mTexture->isScalable())
	{
		// For SVG images we need to round vertices in order to be precisely aligned
		for (int i = 0; i < 4; ++i)
			mVertices[i].pos.round();
	}

	if (mFlipX)
	{
		for (int i = 0; i < 4; ++i)
			mVertices[i].tex[0] = px - mVertices[i].tex[0];
	}

	if (mFlipY)
	{
		for (int i = 0; i < 4; ++i)
			mVertices[i].tex[1] = py - mVertices[i].tex[1];
	}

	updateColors();
	updateRoundCorners();
}

void ImageComponent::onSizeChanged()
{
	GuiComponent::onSizeChanged();	
	updateVertices();
	recalcChildrenLayout();
}

void ImageComponent::setDefaultImage(std::string path)
{
	mDefaultPath = path;
}

void ImageComponent::setImage(const std::string&  path, bool tile, const MaxSizeInfo& maxSize, bool checkFileExists, bool allowMultiImagePlaylist)
{
	std::string canonicalPath = (path[0] == '{' ? "" : Utils::FileSystem::getCanonicalPath(path));
	if (!mPath.empty() && mPath == canonicalPath)
		return;
	
	if (allowMultiImagePlaylist && !canonicalPath.empty())
	{
		auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(canonicalPath));
		if (ext == ".gif" || ext == ".apng")
		{
			int totalFrames, frameTime;
			if (ImageIO::getMultiBitmapInformation(canonicalPath, totalFrames, frameTime) && totalFrames > 0)
			{
				mPath = "";
				mForceLoad = true; // Disable async for animated gifs

				setAllowFading(false);
				setPlaylist(std::make_shared<AnimatedGifPlaylist>(canonicalPath, totalFrames, frameTime));
				if (!mPath.empty())
					return;
			}
		}
		else if (ext == ".m3u")
		{
			mPath = "";
			setAllowFading(false);
			setPlaylist(std::make_shared<M3uPlaylist>(canonicalPath));
			if (!mPath.empty())
				return;
		}
	}

	mPath = canonicalPath;

	if (mTexture != nullptr)
		mTexture->setRequired(false);

	// If the previous image is in the async queue, remove it
	if (mLoadingTexture && mLoadingTexture.use_count() == 1 && !mLoadingTexture->isLoaded())
		TextureResource::cancelAsync(mLoadingTexture);

	if (mTexture && mTexture.use_count() == 1 && !mTexture->isLoaded())
		TextureResource::cancelAsync(mTexture);

	mLoadingTexture.reset();

	MaxSizeInfo defMaxSize = MaxSizeInfo::Empty;
	MaxSizeInfo* pDefaultMaxSize = nullptr;
	if (maxSize.empty())
	{
		defMaxSize = getMaxSizeInfo();
		pDefaultMaxSize = defMaxSize.empty() ? nullptr : &defMaxSize;
	}

	std::string shareId;

	if (!mSharedTexture)
	{
		shareId = getTag(); // Use tag ( element name ) as the share id -> It can be shared only with elements with same exact name

		if (shareId.empty())
		{
			uintptr_t intAddress = reinterpret_cast<uintptr_t>(this);
			shareId = std::to_string(intAddress);
		}
	}

	if (mPath.empty() || (checkFileExists && !ResourceManager::getInstance()->fileExists(mPath)))
	{
		if (mDefaultPath.empty() || !ResourceManager::getInstance()->fileExists(mDefaultPath))
			mTexture.reset();
		else
			mTexture = TextureResource::get(mDefaultPath, tile, mLinear, mForceLoad, mDynamic, true, maxSize.empty() ? pDefaultMaxSize : &maxSize, shareId);
	} 
	else
	{
		if (mPlaylist != nullptr && mPlaylistCache.find(mPath) != mPlaylistCache.cend())
			mTexture = mPlaylistCache[mPath];
		else
		{
			std::shared_ptr<TextureResource> texture = TextureResource::get(mPath, tile, mLinear, mForceLoad, mDynamic, true, maxSize.empty() ? pDefaultMaxSize : &maxSize, shareId);

			if (mPlaylist != nullptr)
				mPlaylistCache[mPath] = texture;

			if (mShowing && !mForceLoad && mDynamic && !mAllowFading && texture != nullptr && !texture->isLoaded())
			{
				mLoadingTexture = texture;
				mLoadingTexture->reload();
			}
			else
				mTexture = texture;
		}
	}

	if (mShowing && mTexture != nullptr)
	{
		mTexture->reload();
		mTexture->setRequired(true);
	}

	if (mLoadingTexture == nullptr && !mTargetSize.empty())
		resize();
}

void ImageComponent::setImage(const char* path, size_t length, bool tile)
{
	mPath = "";
	if (mTexture != nullptr)
		mTexture->setRequired(false);

	mTexture.reset();

	if (path != nullptr)
	{
		mTexture = TextureResource::get("", tile);
		mTexture->initFromMemory(path, length);
	}

	resize();
}

void ImageComponent::setImage(const std::shared_ptr<TextureResource>& texture)
{
	if (mTexture != nullptr)
		mTexture->setRequired(false);

	mTexture = texture;

	if (isShowing() && mTexture != nullptr)
		mTexture->setRequired(true);

	resize();
}

void ImageComponent::setResize(float width, float height)
{
	if (mSize.x() != 0 && mSize.y() != 0 && !mTargetIsMax && !mTargetIsMin && mTargetSize.x() == width && mTargetSize.y() == height)
		return;

	mTargetSize = Vector2f(width, height);
	mSize = mTargetSize;
	mTargetIsMax = false;
	mTargetIsMin = false;
	resize();
}

void ImageComponent::setMaxSize(float width, float height)
{
	if (mSize.x() != 0 && mSize.y() != 0 && mTargetIsMax && !mTargetIsMin && mTargetSize.x() == width && mTargetSize.y() == height)
		return;
	
	mTargetSize = Vector2f(width, height);
	mSize = mTargetSize;
	mTargetIsMax = true;
	mTargetIsMin = false;
	resize();
}

void ImageComponent::setMinSize(float width, float height)
{
	if (mSize.x() != 0 && mSize.y() != 0 && mTargetIsMin && !mTargetIsMax && mTargetSize.x() == width && mTargetSize.y() == height)
		return;

	mTargetSize = Vector2f(width, height);
	mSize = mTargetSize;
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
	if (mRotateByTargetSize == rotate)
		return;

	mRotateByTargetSize = rotate;
	onRotationChanged();
}

void ImageComponent::cropLeft(float percent)
{
	if (percent < 0.0f) percent = 0.0f; else if (percent > 1.0f) percent = 1.0f;
	mTopLeftCrop.x() = percent;
}

void ImageComponent::cropTop(float percent)
{
	if (percent < 0.0f) percent = 0.0f; else if (percent > 1.0f) percent = 1.0f;
	mTopLeftCrop.y() = percent;
}

void ImageComponent::cropRight(float percent)
{
	if (percent < 0.0f) percent = 0.0f; else if (percent > 1.0f) percent = 1.0f;
	mBottomRightCrop.x() = 1.0f - percent;
}

void ImageComponent::cropBot(float percent)
{
	if (percent < 0.0f) percent = 0.0f; else if (percent > 1.0f) percent = 1.0f;
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
	if (mFlipX == flip)
		return;

	mFlipX = flip;
	updateVertices();
}

void ImageComponent::setFlipY(bool flip)
{
	if (mFlipY == flip)
		return;

	mFlipY = flip;
	updateVertices();
}

void ImageComponent::setColorShift(unsigned int color)
{
	if (mColorShift == color && mColorShiftEnd == color)
		return;

	mColorShift = color;
	mColorShiftEnd = color;
	updateColors();
}

void ImageComponent::setColorShiftEnd(unsigned int color)
{
	if (mColorShiftEnd == color)
		return;

	mColorShiftEnd = color;
	updateColors();
}

void ImageComponent::setColorGradientHorizontal(bool horizontal)
{
	if (mColorGradientHorizontal == horizontal)
		return;

	mColorGradientHorizontal = horizontal;
	updateColors();
}

void ImageComponent::onOpacityChanged()
{
	updateColors();
}

void ImageComponent::onPaddingChanged()
{ 
	GuiComponent::onPaddingChanged();
	resize();
	updateVertices(); 
}

void ImageComponent::updateColors()
{
	float opacity = (getOpacity() * (mFading ? mFadeOpacity / 255.0 : 1.0)) / 255.0;

	const unsigned int color = Renderer::convertColor(mColorShift & 0xFFFFFF00 | (unsigned char)((mColorShift & 0xFF) * opacity));
	const unsigned int colorEnd = Renderer::convertColor(mColorShiftEnd & 0xFFFFFF00 | (unsigned char)((mColorShiftEnd & 0xFF) * opacity));
	
	mVertices[0].col = color;
	mVertices[1].col = mColorGradientHorizontal ? colorEnd : color;
	mVertices[2].col = mColorGradientHorizontal ? color : colorEnd;
	mVertices[3].col = colorEnd;
}

void ImageComponent::updateRoundCorners()
{
	if (mRoundCorners <= 0 || Renderer::shaderSupportsCornerSize(mCustomShader.path))
	{
		mRoundCornerStencil.clear();
		return;
	}

	float x = 0;
	float y = 0;
	float size_x = mSize.x();
	float size_y = mSize.y();
	
	if (mTargetIsMin)
	{
		auto targetSize = mTargetSize - mPadding.xy() - mPadding.zw();

		Vector2f targetSizePos = (targetSize - mSize) * mOrigin * -1;

		x = targetSizePos.x();
		y = targetSizePos.y();
		size_x = targetSize.x();
		size_y = targetSize.y();
	}

	float radius = mRoundCorners < 1 ? Math::max(size_x, size_y) * mRoundCorners : mRoundCorners;
	mRoundCornerStencil = Renderer::createRoundRect(x, y, size_x, size_y, radius);
}

void ImageComponent::render(const Transform4x4f& parentTrans)
{
	if (!mVisible)
		return;

	if (mLoadingTexture != nullptr && mLoadingTexture->isLoaded())
	{
		if (mTexture != nullptr)
			mTexture->setRequired(false);

		mTexture = mLoadingTexture;

		if (isShowing() && mTexture != nullptr)
			mTexture->setRequired(true);

		mLoadingTexture.reset();
		resize();
		updateColors();
	}

	Transform4x4f trans = parentTrans * getTransform();
	
	// Don't use soft clip if rotation applied : let renderer do the work
	if (mCheckClipping && mRotation == 0 && trans.r0().y() == 0)
	{
		auto rect = Renderer::getScreenRect(trans, mSize);
		if (!Renderer::isVisibleOnScreen(rect))
			return;
	}

	if (mColorShift == 0)
	{
		GuiComponent::renderChildren(trans);
		return;
	}

	if (Settings::DebugMouse() && mIsMouseOver)
	{
		Renderer::setMatrix(trans);
		Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), 0xFF000033, 0xFF000033);
	}
	else if (Settings::DebugImage() || (Settings::DebugMouse() && mIsMouseOver))
	{
		Renderer::setMatrix(trans);
		Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), 0x00000033, 0x00000033);
	}

	if (mTexture && getOpacity() > 0)
	{
		Vector2f targetSizePos = (mTargetSize - mSize) * mOrigin * -1;

		if(Settings::DebugImage())
			Renderer::drawRect(targetSizePos.x(), targetSizePos.y(), mTargetSize.x(), mTargetSize.y(), 0xFF000033, 0xFF000033);

		// actually draw the image
		// The bind() function returns false if the texture is not currently loaded. A blank
		// texture is bound in this case but we want to handle a fade so it doesn't just 'jump' in
		// when it finally loads
		if (!mTexture->bind())
		{
			fadeIn(false);
			return;
		}

		beginCustomClipRect();

		// Align left
		if (mVerticalAlignment == ALIGN_TOP)
			trans.translate(0, targetSizePos.y());
		else if (mVerticalAlignment == ALIGN_BOTTOM)
			trans.translate(targetSizePos.x(), targetSizePos.y() + mTargetSize.y() - mSize.y());

		if (mHorizontalAlignment == ALIGN_LEFT)
			trans.translate(targetSizePos.x(), 0);
		else if (mHorizontalAlignment == ALIGN_RIGHT)
			trans.translate(targetSizePos.x() + mTargetSize.x() - mSize.x(), targetSizePos.y());

		Renderer::setMatrix(trans);

		fadeIn(true);

		mVertices->saturation = mSaturation;
		mVertices->customShader = mCustomShader.path.empty() ? nullptr : &mCustomShader;						

		if (mRoundCorners > 0 && mRoundCornerStencil.size() > 0)
		{
			Renderer::setStencil(mRoundCornerStencil.data(), mRoundCornerStencil.size());
			Renderer::drawTriangleStrips(&mVertices[0], 4);
			Renderer::disableStencil();
		}
		else
		{
			mVertices->cornerRadius = mRoundCorners < 1 ? Math::max(mSize.x(), mSize.y()) * mRoundCorners : mRoundCorners;			
			Renderer::drawTriangleStrips(&mVertices[0], 4);
		}

		if (mReflection.x() != 0 || mReflection.y() != 0)
		{
			float baseOpacity = (getOpacity() * (mFading ? mFadeOpacity / 255.0 : 1.0)) / 255.0;

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

		GuiComponent::renderChildren(trans);

		endCustomClipRect();
	}
	else
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

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, getThemeTypeName());
	if (!elem)
		return;

	if (properties & ThemeFlags::SIZE)
	{
		Vector4f clientRectangle = getParent() ? getParent()->getClientRect() : Vector4f(0, 0, (float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
		Vector2f scale = Vector2f(clientRectangle.z(), clientRectangle.w());

		if (elem->has("size"))
		{
			auto sz = mSourceBounds.zw() = elem->get<Vector2f>("size");
			setResize(elem->get<Vector2f>("size") * scale);
		}
		else if (elem->has("maxSize"))
		{
			auto sz = mSourceBounds.zw() = elem->get<Vector2f>("maxSize");
			setMaxSize(elem->get<Vector2f>("maxSize") * scale);
		}
		else if (elem->has("minSize"))
		{
			auto sz = mSourceBounds.zw() = elem->get<Vector2f>("minSize");
			setMinSize(elem->get<Vector2f>("minSize") * scale);
		}

		if (elem->has("w"))
		{
			auto w = mSourceBounds.z() = elem->get<float>("w");
			mTargetSize = Vector2f(elem->get<float>("w") * scale.x(), mTargetSize.y());
			resize();
		}

		if (elem->has("h"))
		{
			auto h = mSourceBounds.w() = elem->get<float>("h");
			mTargetSize = Vector2f(mTargetSize.x(), elem->get<float>("h") * scale.y());
			resize();
		}
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

		if (elem->has("linearSmooth"))
			mLinear = elem->get<bool>("linearSmooth");

		if (elem->has("saturation"))
			setSaturation(Math::clamp(elem->get<float>("saturation"), 0.0f, 1.0f));

		if (ThemeData::parseCustomShader(elem, &mCustomShader))
			updateRoundCorners();
	}

	if (elem->has("shared"))
		mSharedTexture = elem->get<bool>("shared");

	if (properties & ThemeFlags::ROTATION && elem->has("flipX"))
		setFlipX(elem->get<bool>("flipX"));

	if (properties & ThemeFlags::ROTATION && elem->has("flipY"))
		setFlipY(elem->get<bool>("flipY"));

	if (elem->has("autoFade"))
		setAllowFading(elem->get<bool>("autoFade"));

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
		setRoundCorners(elem->get<float>("roundCorners"));

	GuiComponent::applyTheme(theme, view, element, properties & ~ThemeFlags::SIZE);

	if (elem->has("default"))
		setDefaultImage(elem->get<std::string>("default"));

	if (properties & PATH)
	{
		if (elem->has("path"))
		{
			auto path = elem->get<std::string>("path");

			if (!path.empty() && path[0] != '{')
			{
				mPath = "";

				if (mPlaylist == nullptr)
				{
					bool tile = (elem->has("tile") && elem->get<bool>("tile"));
					setImage(path, tile);
				}
			}
		}		
		else if (!mDefaultPath.empty() && mPlaylist == nullptr)
			setImage("");
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
	mPlaylistCache.clear();
	mPlaylist = playList;
	if (mPlaylist == nullptr)
		return;

	auto image = mPlaylist->getNextItem();
	if (!image.empty())
		setImage(image);
	else if (!mDefaultPath.empty())
		setImage("");
}

void ImageComponent::onShow()
{
	mPlaylistTimer = 0;

	if (!isShowing() && mPlaylist != nullptr && !mPath.empty() && mPlaylist->getRotateOnShow())
	{
		auto item = mPlaylist->getNextItem();
		if (!item.empty())
			setImage(item); // , false, getMaxSizeInfo(), true, false);
	}

	GuiComponent::onShow();	

	if (mTexture != nullptr)
	{
		mTexture->reload();			
		mTexture->setRequired(true);
	}
}

void ImageComponent::onHide()
{
	if (mTexture)
		mTexture->setRequired(false);

	if (mShowing)
	{
		if (mLoadingTexture)
		{
			int count = mLoadingTexture.use_count();
			if (count == 1 && !mLoadingTexture->isLoaded())
				TextureResource::cancelAsync(mLoadingTexture);
		}

		if (mTexture)
		{
			int count = mTexture.use_count();
			if (count == 1 && !mTexture->isLoaded())
				TextureResource::cancelAsync(mTexture);
		}
	}

	GuiComponent::onHide();
}

void ImageComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mPlaylist && isShowing())
	{
		mPlaylistTimer += deltaTime;

		if (mPlaylistTimer >= mPlaylist->getDelay())
		{
			auto item = mPlaylist->getNextItem();
			if (!item.empty())
			{
				// LOG(LogDebug) << "getNextItem: " << item;
				setImage(item); // , false, getMaxSizeInfo(), true, false);
			}

			mPlaylistTimer = 0.0;
		}
	}
}

bool ImageComponent::isTiled()
{ 
	return mTexture != nullptr && mTexture->isTiled(); 
}

ThemeData::ThemeElement::Property ImageComponent::getProperty(const std::string name)
{
	Vector2f scale = getParent() ? getParent()->getSize() : Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	if (name == "size" || name == "maxSize" || name == "minSize")
		return mSize / scale;
	else if (name == "color")
		return mColorShift;
	else if (name == "colorEnd")
		return mColorShiftEnd;
	else if (name == "reflexion")
		return mReflection;
	else if (name == "roundCorners")
		return mRoundCorners;
	else if (name == "path")
		return mPath;
	else if (name == "default")
		return mDefaultPath;
	else if (name == "saturation")
		return mSaturation;
	else if (name == "autoFade")
		return mAllowFading;
	else if (Utils::String::startsWith(name, "shader."))
	{
		auto prop = name.substr(7);

		auto it = mCustomShader.parameters.find(prop);
		if (it != mCustomShader.parameters.cend())
			return Utils::String::toFloat(it->second);

		return 0.0f;
	}
	
	return GuiComponent::getProperty(name);
}

void ImageComponent::setProperty(const std::string name, const ThemeData::ThemeElement::Property& value)
{	
	Vector2f scale = getParent() ? getParent()->getSize() : Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	
	if (value.type == ThemeData::ThemeElement::Property::PropertyType::Float && name == "w")
	{
		mSourceBounds.z() = value.f;
		mTargetSize = Vector2f(value.f * scale.x(), mTargetSize.y());
		resize();
	}
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Float && name == "h")
	{
		mSourceBounds.w() = value.f;
		mTargetSize = Vector2f(mTargetSize.x(), value.f * scale.y());
		resize();
	}	
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Pair && (name == "maxSize" || name == "minSize"))
	{
		mSourceBounds.zw() = value.v;
		mTargetSize = Vector2f(value.v.x() * scale.x(), value.v.y() * scale.y());
		resize();
	}
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Int && name == "color")
	{
		if (mColorShift == mColorShiftEnd)
			setColorShift(value.i);
		else
		{
			mColorShift = value.i;			
			updateColors();
		}
	}
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Bool && name == "autoFade")
		setAllowFading(value.b);
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Int && name == "colorEnd")
		setColorShiftEnd(value.i);
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Pair && name == "reflexion")
		mReflection = value.v;
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Float && name == "roundCorners")
		setRoundCorners(value.f);
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::String && name == "default")
	{
		if (mDefaultPath != value.s && !value.s.empty() && Utils::FileSystem::exists(value.s) && mPath.empty())
		{
			setDefaultImage(value.s);
			setImage("");
		}
		else
			mDefaultPath = value.s;
	}
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::String && name == "path")
		setImage(value.s); // , false, getMaxSizeInfo()
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Float && name == "saturation")
		setSaturation(value.f);
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Float && Utils::String::startsWith(name, "shader."))
	{
		auto prop = name.substr(7);

		auto it = mCustomShader.parameters.find(prop);
		if (it != mCustomShader.parameters.cend())
			mCustomShader.parameters[prop] = std::to_string(value.f);
	}
	else
		GuiComponent::setProperty(name, value);
}

void ImageComponent::setRoundCorners(float value) 
{ 
	if (mRoundCorners == value)
		return;
		
	mRoundCorners = value; 
	updateRoundCorners();
}

void ImageComponent::setCustomShader(const Renderer::ShaderInfo& customShader)
{ 
	mCustomShader = customShader; 
	updateRoundCorners();
}

void ImageComponent::setSaturation(float saturation)
{
	mSaturation = saturation;
}

void ImageComponent::recalcLayout()
{
	if (mExtraType != ExtraType::EXTRACHILDREN || !getParent())
		return;

	Vector4f clientRectangle = getParent()->getClientRect();
	Vector2f newPos = mSourceBounds.xy() * clientRectangle.zw() + clientRectangle.xy();
	Vector2f newSize = mSourceBounds.zw() * clientRectangle.zw();

	if (mPosition.v2() != newPos)
		setPosition(newPos.x(), newPos.y());

	if (mSize != newSize)
	{
		if (mTargetIsMax)
			setMaxSize(newSize);
		else if (mTargetIsMin)
			setMinSize(newSize);
		else
			setResize(newSize);
	}
}

const MaxSizeInfo ImageComponent::getMaxSizeInfo()
{
	if (!mSize.empty())
	{
		float maxScale = 1.0f;

		/*
		float maxScale = Math::max(1.0f, mScale);

		GuiComponent* comp = this;
		while (comp != nullptr)
		{
			for (auto storyBoard : comp->getStoryBoards())
			{
				if (storyBoard.second == nullptr)
					continue;

				for (auto anim : storyBoard.second->animations)
				{
					if (anim->propertyName != "scale")
						continue;

					if (comp == this)
					{
						if (anim->from.type == ThemeData::ThemeElement::Property::PropertyType::Float && anim->from.f > maxScale)
							maxScale = anim->from.f;

						if (anim->to.type == ThemeData::ThemeElement::Property::PropertyType::Float && anim->to.f > maxScale)
							maxScale = anim->to.f;
					}
					else
					{
						float localScale = 1.0f;

						if (anim->from.type == ThemeData::ThemeElement::Property::PropertyType::Float && anim->from.f > localScale)
							localScale = anim->from.f;

						if (anim->to.type == ThemeData::ThemeElement::Property::PropertyType::Float && anim->to.f > localScale)
							localScale = anim->to.f;

						maxScale *= localScale;
					}
				}
			}

			comp = comp->getParent();
		}
		*/
		if (mTargetSize.empty())
		{
			auto size = mSize - mPadding.xy() - mPadding.zw();
			size *= maxScale;

			// Make sure it's not bigger that screen res
			if (size.x() > Renderer::getScreenWidth() || size.y() > Renderer::getScreenHeight())
				size = ImageIO::adjustPictureSizeF(size, Vector2f(Renderer::getScreenWidth(), Renderer::getScreenHeight()), mTargetIsMin);

			if (size.x() > 32 && size.y() > 32)
				return MaxSizeInfo(size, mTargetIsMin);

			return MaxSizeInfo::Empty;
		}

		auto targetSize = mTargetSize - mPadding.xy() - mPadding.zw();
		targetSize *= maxScale;

		// Make sure it's not bigger that screen res
		if (targetSize.x() > Renderer::getScreenWidth() || targetSize.y() > Renderer::getScreenHeight())
			targetSize = ImageIO::adjustPictureSizeF(targetSize, Vector2f(Renderer::getScreenWidth(), Renderer::getScreenHeight()), mTargetIsMin);

		if (targetSize.x() > 32 && targetSize.y() > 32)
			return MaxSizeInfo(targetSize, mTargetIsMin);
	}

	return MaxSizeInfo::Empty;
}