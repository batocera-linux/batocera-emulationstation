#pragma once
#ifndef ES_CORE_IMAGE_IO
#define ES_CORE_IMAGE_IO

#include <stdlib.h>
#include <vector>
#include "math/Vector2f.h"
#include "math/Vector2i.h"


class MaxSizeInfo
{
public:
	static const MaxSizeInfo Empty;

	MaxSizeInfo(float x, float y) : mSize(Vector2f(x, y)), mExternalZoom(false), mExternalZoomKnown(false) { }
	MaxSizeInfo(Vector2f size) : mSize(size), mExternalZoom(false), mExternalZoomKnown(false) { }

	MaxSizeInfo(float x, float y, bool externalZoom) : mSize(Vector2f(x, y)), mExternalZoom(externalZoom), mExternalZoomKnown(true) { }
	MaxSizeInfo(Vector2f size, bool externalZoom) : mSize(size), mExternalZoom(externalZoom), mExternalZoomKnown(true) { }

	const bool empty() const { return mSize.x() <= 1 && mSize.y() <= 1; }

	const float x() const { return mSize.x(); }
	const float y() const { return mSize.y(); }
	const Vector2f& size() const { return mSize; }

	const bool externalZoom() const { return mExternalZoom; }
	const bool isExternalZoomKnown() const { return mExternalZoomKnown; }

private:
	MaxSizeInfo() : mSize(Vector2f(0, 0)), mExternalZoom(false), mExternalZoomKnown(false) { }

	Vector2f mSize;
	bool	 mExternalZoom;
	bool	 mExternalZoomKnown;
};

class ImageIO
{
public:
	static unsigned char*  loadFromMemoryRGBA32(const unsigned char * data, const size_t size, size_t & width, size_t & height, MaxSizeInfo* maxSize = nullptr, Vector2i* baseSize = nullptr, Vector2i* packedSize = nullptr, int subImageIndex = -1);
	static void flipPixelsVert(unsigned char* imagePx, const size_t& width, const size_t& height);
	
	static Vector2f getPictureMinSize(Vector2f imageSize, Vector2f maxSize);
	
	static Vector2i adjustPictureSize(Vector2i imageSize, Vector2i maxSize, bool externSize = false);
	
	static Vector2f adjustPictureSizeF(float cxDIB, float cyDIB, float iMaxX, float iMaxY, bool externSize = false);
	static Vector2f adjustPictureSizeF(Vector2f imageSize, Vector2f maxSize, bool externSize = false);

	static bool		loadImageSize(const std::string& fn, unsigned int *x, unsigned int *y);

	static void		removeImageCache(const std::string& fn);
	static void		updateImageCache(const std::string& fn, int sz, int x, int y);
	static void		loadImageCache();
	static void		saveImageCache();
	static void		clearImageCache();

	static bool		getMultiBitmapInformation(const std::string& path, int& totalFrames, int& frameTime);
};

#endif // ES_CORE_IMAGE_IO
