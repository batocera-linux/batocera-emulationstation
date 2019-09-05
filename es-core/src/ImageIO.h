#pragma once
#ifndef ES_CORE_IMAGE_IO
#define ES_CORE_IMAGE_IO

#include <stdlib.h>
#include <vector>
#include "math/Vector2f.h"

class ImageIO
{
public:
	static unsigned char*  loadFromMemoryRGBA32(const unsigned char * data, const size_t size, size_t & width, size_t & height);
	static void flipPixelsVert(unsigned char* imagePx, const size_t& width, const size_t& height);

	// batocera
	static Vector2f getPictureMinSize(Vector2f imageSize, Vector2f maxSize);
};

#endif // ES_CORE_IMAGE_IO
