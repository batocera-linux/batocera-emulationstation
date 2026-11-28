#include "ImageIO.h"

#include "Log.h"
#include <FreeImage.h>
#include <string.h>
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <sstream>
#include <fstream>
#include <map>
#include <unordered_map>
#include <mutex>
#include "renderers/Renderer.h"
#include "Paths.h"
#include "math/Vector4f.h"
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>

#define STB_IMAGE_RESIZE_STATIC
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "utils/stb_image_resize2.h"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "utils/stb_image.h"

const MaxSizeInfo MaxSizeInfo::Empty;

MaxSizeInfo ImageIO::getTextureDecodeSize(
	const MaxSizeInfo& requested, const Vector2i& screenSize, bool optimizeVRAM)
{
	if (optimizeVRAM && !requested.empty() && requested.x() < screenSize.x() && requested.y() < screenSize.y())
	{
		return requested;
	}

	return MaxSizeInfo(screenSize.x(), screenSize.y(), false);
}

Vector2i ImageIO::getRasterOutputSize(
	const Vector2i& sourceSize, const MaxSizeInfo* maxSize, const Vector2i& screenSize)
{
	if (maxSize == nullptr)
		return sourceSize;

	const int maxX = (int)Math::round(maxSize->x());
	const int maxY = (int)Math::round(maxSize->y());

	if (maxX <= 0 || maxY <= 0 || (sourceSize.x() <= maxX && sourceSize.y() <= maxY))
	{
		return sourceSize;
	}

	Vector2i output = adjustPictureSize(sourceSize, Vector2i(maxX, maxY), maxSize->externalZoom());

	if (output.x() > screenSize.x() || output.y() > screenSize.y())
		output = adjustPictureSize(output, screenSize, false);

	return output;
}

namespace
{
	uint32_t readPngUint32(const unsigned char* p)
	{
		return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
			(uint32_t(p[2]) << 8) | p[3];
	}

	// The gamma tag comes before the palette and image data.
	uint32_t pngGamma(const unsigned char* data, size_t size)
	{
		for (size_t pos = 8; size - pos >= 12; )
		{
			const auto length = readPngUint32(data + pos);
			const auto type = data + pos + 4;
			if (length > size - pos - 12 || !memcmp(type, "IDAT", 4) || !memcmp(type, "PLTE", 4))
				break;
			if (!memcmp(type, "gAMA", 4) && length == 4)
				return readPngUint32(type + 4);
			pos += length + 12;
		}
		return 0;
	}

	void copyGammaCorrected(unsigned char* dest, const unsigned char* src,
		size_t pixels, const unsigned char* table)
	{
		for (size_t i = 0; i < pixels; ++i, dest += 4, src += 4)
		{
			dest[0] = table[src[0]];
			dest[1] = table[src[1]];
			dest[2] = table[src[2]];
			dest[3] = src[3];
		}
	}

	unsigned char* loadStbRGBA(
		const unsigned char* data, size_t size,
		size_t& width, size_t& height,
		MaxSizeInfo* maxSize, Vector2i* baseSize, Vector2i* packedSize)
	{
		const bool isPng =
			size >= 8 &&
			data[0] == 0x89 && data[1] == 'P' &&
			data[2] == 'N' && data[3] == 'G' &&
			data[4] == 0x0D && data[5] == 0x0A &&
			data[6] == 0x1A && data[7] == 0x0A;

		if (!isPng || size > static_cast<size_t>(std::numeric_limits<int>::max()))
			return nullptr;

		const auto gamma = pngGamma(data, size);
		const double displayGamma = 2.2 * gamma / 100000.0;
		// Match libpng's five-decimal exponent and 5% threshold.
		const double exponent = gamma ? std::round(100000.0 / displayGamma) / 100000.0 : 1.0;
		const bool correctGamma = gamma && (displayGamma < 0.95 || exponent < 0.95);
		unsigned char gammaTable[256];
		if (correctGamma)
		{
			// Use FreeImage when this 8-bit table would lose precision.
			if (size < 33 || data[24] != 8 || gamma < 1000 || gamma > 1000000)
				return nullptr;

			for (int i = 0; i < 256; ++i)
				gammaTable[i] = static_cast<unsigned char>(std::lround(255 * std::pow(i / 255.0, exponent)));
		}

		int sourceWidth = 0;
		int sourceHeight = 0;
		int channels = 0;

		std::unique_ptr<unsigned char, decltype(&stbi_image_free)>
			pixels(nullptr, &stbi_image_free);

		pixels.reset(stbi_load_from_memory(
			data, static_cast<int>(size),
			&sourceWidth, &sourceHeight, &channels, 4));

		if (!pixels)
			return nullptr;

		const int maxStride = std::numeric_limits<int>::max();
		if (sourceWidth <= 0 || sourceHeight <= 0 || sourceWidth > maxStride / 4)
			return nullptr;

		const Vector2i target = ImageIO::getRasterOutputSize(
			Vector2i(sourceWidth, sourceHeight), maxSize,
			Vector2i(Renderer::getScreenWidth(), Renderer::getScreenHeight()));

		if (target.x() <= 0 || target.y() <= 0 || target.x() > maxStride / 4)
			return nullptr;

		const size_t rowBytes = static_cast<size_t>(target.x()) * 4;
		if (static_cast<size_t>(target.y()) >
			std::numeric_limits<size_t>::max() / rowBytes)
			return nullptr;

		const bool resized = target.x() != sourceWidth || target.y() != sourceHeight;

		std::unique_ptr<unsigned char[]> output(new (std::nothrow) unsigned char[rowBytes * target.y()]);
		if (!output)
			return nullptr;

		if (resized)
		{
			if (correctGamma)
				copyGammaCorrected(pixels.get(), pixels.get(), size_t(sourceWidth) * sourceHeight, gammaTable);

			// ES stores the bottom row first.
			void* result = stbir_resize(
				pixels.get(), sourceWidth, sourceHeight, sourceWidth * 4,
				output.get() + rowBytes * (target.y() - 1),
				target.x(), target.y(), -static_cast<int>(rowBytes),
				STBIR_4CHANNEL, STBIR_TYPE_UINT8,
				STBIR_EDGE_CLAMP, STBIR_FILTER_BOX);

			if (result == nullptr)
				return nullptr;
		}
		else
		{
			for (int y = 0; y < sourceHeight; ++y)
			{
				auto dest = output.get() + rowBytes * (sourceHeight - 1 - y);
				auto src = pixels.get() + rowBytes * y;
				if (correctGamma)
					copyGammaCorrected(dest, src, sourceWidth, gammaTable);
				else
					memcpy(dest, src, rowBytes);
			}
		}

		width = target.x();
		height = target.y();

		if (baseSize != nullptr)
			*baseSize = Vector2i(sourceWidth, sourceHeight);

		if (packedSize != nullptr)
			*packedSize = resized ? target : Vector2i(0, 0);

		return output.release();
	}

	FIBITMAP* resizeBitmap(FIBITMAP* source, int width, int height)
	{
		const unsigned int bpp = FreeImage_GetBPP(source);
		const unsigned int sourceWidth = FreeImage_GetWidth(source);
		const unsigned int sourceHeight = FreeImage_GetHeight(source);
		const unsigned int sourcePitch = FreeImage_GetPitch(source);
		const unsigned int intMax =
			static_cast<unsigned int>(std::numeric_limits<int>::max());

		if (FreeImage_GetImageType(source) == FIT_BITMAP &&
			(bpp == 24 || bpp == 32) &&
			sourceWidth > 0 && sourceHeight > 0 &&
			sourceWidth <= intMax && sourceHeight <= intMax &&
			sourcePitch <= intMax &&
			width > 0 && height > 0)
		{
			FIBITMAP* result = FreeImage_Allocate(width, height, bpp);

			if (result != nullptr)
			{
				const unsigned int destinationPitch = FreeImage_GetPitch(result);

				if (destinationPitch <= intMax)
				{
					// Resize alpha like a color channel.
					const stbir_pixel_layout layout = bpp == 32 ? STBIR_4CHANNEL : STBIR_RGB;

					void* resized = stbir_resize(
						FreeImage_GetBits(source),
						static_cast<int>(sourceWidth),
						static_cast<int>(sourceHeight),
						static_cast<int>(sourcePitch),
						FreeImage_GetBits(result),
						width, height,
						static_cast<int>(destinationPitch),
						layout, STBIR_TYPE_UINT8,
						STBIR_EDGE_CLAMP, STBIR_FILTER_BOX);

					if (resized != nullptr)
						return result;
				}

				FreeImage_Unload(result);
			}
		}

		return FreeImage_Rescale(source, width, height, FILTER_BOX);
	}
}

unsigned char* ImageIO::loadFromMemoryRGBA32(const unsigned char * data, const size_t size, size_t & width, size_t & height, MaxSizeInfo* maxSize, Vector2i* baseSize, Vector2i* packedSize, int subImageIndex)
{
	LOG(LogDebug) << "ImageIO::loadFromMemoryRGBA32";

	if (baseSize != nullptr)
		*baseSize = Vector2i(0, 0);

	if (packedSize != nullptr)
		*packedSize = Vector2i(0, 0);

	std::vector<unsigned char> rawData;
	width = 0;
	height = 0;

	if (data == nullptr || size == 0)
		return nullptr;

	if (subImageIndex < 0)
	{
		unsigned char* result = loadStbRGBA(data, size, width, height, maxSize, baseSize, packedSize);

		if (result != nullptr)
			return result;
	}

	FIMEMORY * fiMemory = FreeImage_OpenMemory((BYTE *)data, (DWORD)size);

	if (fiMemory != nullptr) 
	{
		//detect the filetype from data
		FREE_IMAGE_FORMAT format = FreeImage_GetFileTypeFromMemory(fiMemory);
		if (format != FIF_UNKNOWN && FreeImage_FIFSupportsReading(format))
		{			
			// file type is supported. load image
			FIMULTIBITMAP* fiMultiBitmap = nullptr;
			FIBITMAP* fiBitmap = nullptr;

			if (subImageIndex < 0)
				fiBitmap = FreeImage_LoadFromMemory(format, fiMemory);
			else 
			{
				fiMultiBitmap = FreeImage_LoadMultiBitmapFromMemory(format, fiMemory, GIF_PLAYBACK);
				if (fiMultiBitmap == nullptr)
					fiBitmap = FreeImage_LoadFromMemory(format, fiMemory);
				else
				{
					fiBitmap = FreeImage_LockPage(fiMultiBitmap, subImageIndex);
					if (fiBitmap == nullptr)
						FreeImage_CloseMultiBitmap(fiMultiBitmap);				
				}
			}
			
			if (fiBitmap != nullptr)
			{
				//loaded. convert to 32bit if necessary
				const int bitsPerPixel = FreeImage_GetBPP(fiBitmap);
				if ((fiMultiBitmap != nullptr && bitsPerPixel != 32) || (fiMultiBitmap == nullptr && bitsPerPixel != 32 && bitsPerPixel != 24 && bitsPerPixel != 8))
				{
					FIBITMAP * fiConverted = FreeImage_ConvertTo32Bits(fiBitmap);
					if (fiConverted != nullptr)
					{
						//free original bitmap data
						if (fiMultiBitmap != nullptr)
						{
							FreeImage_UnlockPage(fiMultiBitmap, fiBitmap, false);
							FreeImage_CloseMultiBitmap(fiMultiBitmap);
							fiMultiBitmap = nullptr;
						}
						else
							FreeImage_Unload(fiBitmap);

						fiBitmap = fiConverted;
					}
				}

				if (fiBitmap != nullptr)
				{
					width = FreeImage_GetWidth(fiBitmap);
					height = FreeImage_GetHeight(fiBitmap);

					if (baseSize != nullptr)
						*baseSize = Vector2i(width, height);

					const Vector2i sz = getRasterOutputSize(Vector2i(width, height), maxSize, Vector2i(Renderer::getScreenWidth(), Renderer::getScreenHeight()));
						
					if (sz.x() != width || sz.y() != height)
					{
						LOG(LogDebug) << "ImageIO : rescaling image from " << std::string(std::to_string(width) + "x" + std::to_string(height)).c_str() << " to " << std::string(std::to_string(sz.x()) + "x" + std::to_string(sz.y())).c_str();

						FIBITMAP* imageRescaled = resizeBitmap(fiBitmap, sz.x(), sz.y());

						if (imageRescaled == nullptr)
						{
							if (fiMultiBitmap != nullptr)
							{
								FreeImage_UnlockPage(fiMultiBitmap, fiBitmap, false);
								FreeImage_CloseMultiBitmap(fiMultiBitmap);
							}
							else
								FreeImage_Unload(fiBitmap);

							FreeImage_CloseMemory(fiMemory);
							return nullptr;
						}

						if (fiMultiBitmap != nullptr)
						{
							FreeImage_UnlockPage(fiMultiBitmap, fiBitmap, false);
							FreeImage_CloseMultiBitmap(fiMultiBitmap);
							fiMultiBitmap = nullptr;
						}
						else
							FreeImage_Unload(fiBitmap);

						fiBitmap = imageRescaled;

						width = FreeImage_GetWidth(fiBitmap);
						height = FreeImage_GetHeight(fiBitmap);

						if (packedSize != nullptr)
							*packedSize = Vector2i(width, height);
					}

					unsigned char* tempData = new unsigned char[width * height * 4];

					const size_t w = width;
					const size_t h = height;
					const int pitch = FreeImage_GetPitch(fiBitmap);
					const unsigned char* bits = FreeImage_GetBits(fiBitmap);
					const int bpp = FreeImage_GetBPP(fiBitmap);

					if (bpp == 32)
					{
						for (size_t y = 0; y < h; y++)
						{
							const unsigned int* argb = (const unsigned int*)(bits + y * pitch);
							unsigned int* abgr = (unsigned int*)(tempData + y * w * 4);

							for (size_t x = 0; x < w; x++)
							{
								const unsigned int c = argb[x];
								abgr[x] = (c & 0xFF00FF00) | ((c & 0xFF) << 16) | ((c >> 16) & 0xFF);
							}
						}
					}
					else if (bpp == 24)
					{
						for (size_t y = 0; y < h; y++)
						{
							const unsigned char* src = bits + y * pitch;
							unsigned int* abgr = (unsigned int*)(tempData + y * w * 4);
							for (size_t x = 0; x < w; x++)
							{
								const unsigned char r = src[x * 3 + 0];
								const unsigned char g = src[x * 3 + 1];
								const unsigned char b = src[x * 3 + 2];
								abgr[x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
							}
						}
					}
					else if (bpp == 8)
					{
						RGBQUAD* palette = FreeImage_GetPalette(fiBitmap);
						BOOL hasTransparency = FreeImage_IsTransparent(fiBitmap);
						BYTE* transTable = FreeImage_GetTransparencyTable(fiBitmap); // alpha par index de palette

						for (size_t y = 0; y < h; y++)
						{
							const unsigned char* src = bits + y * pitch;
							unsigned int* abgr = (unsigned int*)(tempData + y * w * 4);
							for (size_t x = 0; x < w; x++)
							{
								const BYTE idx = src[x];
								const RGBQUAD& color = palette[idx];
								const BYTE alpha = (hasTransparency && transTable) ? transTable[idx] : 0xFF;
								abgr[x] = (alpha << 24) | (color.rgbBlue << 16) | (color.rgbGreen << 8) | color.rgbRed;
							}
						}
					}

					if (fiMultiBitmap)
					{
						FreeImage_UnlockPage(fiMultiBitmap, fiBitmap, false);
						FreeImage_CloseMultiBitmap(fiMultiBitmap);
					}
					else
						FreeImage_Unload(fiBitmap);

					FreeImage_CloseMemory(fiMemory);

					return tempData;
				}
			}
			else
			{
				LOG(LogError) << "Error - Failed to load image from memory!";
			}
		}
		else
		{
			LOG(LogError) << "Error - File type " << (format == FIF_UNKNOWN ? "unknown" : "unsupported") << "!";
		}
		//free FIMEMORY again
		FreeImage_CloseMemory(fiMemory);
	}

	return nullptr;
}

void ImageIO::flipPixelsVert(unsigned char* imagePx, const size_t& width, const size_t& height)
{
	unsigned int temp;
	unsigned int* arr = (unsigned int*)imagePx;
	for(size_t y = 0; y < height / 2; y++)
	{
		for(size_t x = 0; x < width; x++)
		{
			temp = arr[x + (y * width)];
			arr[x + (y * width)] = arr[x + (height * width) - ((y + 1) * width)];
			arr[x + (height * width) - ((y + 1) * width)] = temp;
		}
	}
}

Vector2f ImageIO::adjustPictureSizeF(Vector2f imageSize, Vector2f maxSize, bool externSize)
{
	return adjustPictureSizeF(imageSize.x(), imageSize.y(), maxSize.x(), maxSize.y(), externSize);
}

Vector2f ImageIO::adjustPictureSizeF(float cxDIB, float cyDIB, float iMaxX, float iMaxY, bool externSize)
{
	if (externSize)
		return getPictureMinSize(Vector2f(cxDIB, cyDIB), Vector2f(iMaxX, iMaxY));

	if (cxDIB == 0 || cyDIB == 0)
		return Vector2f(cxDIB, cyDIB);

	float xCoef = iMaxX / cxDIB;
	float yCoef = iMaxY / cyDIB;

	cyDIB = cyDIB * std::max(xCoef, yCoef);
	cxDIB = cxDIB * std::max(xCoef, yCoef);

	if (cxDIB > iMaxX)
	{
		cyDIB = cyDIB * iMaxX / cxDIB;
		cxDIB = iMaxX;
	}

	if (cyDIB > iMaxY)
	{
		cxDIB = cxDIB * iMaxY / cyDIB;
		cyDIB = iMaxY;
	}

	return Vector2f(cxDIB, cyDIB);
}

Vector2i ImageIO::adjustPictureSize(Vector2i imageSize, Vector2i maxSize, bool externSize)
{
	if (externSize)
	{
		Vector2f szf = getPictureMinSize(Vector2f(imageSize.x(), imageSize.y()), Vector2f(maxSize.x(), maxSize.y()));
		return Vector2i(szf.x(), szf.y());
	}

	int cxDIB = imageSize.x();
	int cyDIB = imageSize.y();

	if (cxDIB == 0 || cyDIB == 0)
		return imageSize;

	int iMaxX = maxSize.x();
	int iMaxY = maxSize.y();

	double xCoef = (double)iMaxX / (double)cxDIB;
	double yCoef = (double)iMaxY / (double)cyDIB;

#if WIN32
	cyDIB = (int)((double)cyDIB * std::fmax(xCoef, yCoef));
	cxDIB = (int)((double)cxDIB * std::fmax(xCoef, yCoef));
#else
	cyDIB = (int)((double)cyDIB * std::max(xCoef, yCoef));
	cxDIB = (int)((double)cxDIB * std::max(xCoef, yCoef));
#endif

	if (cxDIB > iMaxX)
	{
		cyDIB = (int)((double)cyDIB * (double)iMaxX / (double)cxDIB);
		cxDIB = iMaxX;
	}

	if (cyDIB > iMaxY)
	{
		cxDIB = (int)((double)cxDIB * (double)iMaxY / (double)cyDIB);
		cyDIB = iMaxY;
	}

	return Vector2i(cxDIB, cyDIB);
}

Vector2f ImageIO::getPictureMinSize(Vector2f imageSize, Vector2f maxSize)
{
	if (imageSize.x() == 0 || imageSize.y() == 0)
		return imageSize;

	float cxDIB = maxSize.x();
	float cyDIB = maxSize.y();

	float xCoef = maxSize.x() / imageSize.x();
	float yCoef = maxSize.y() / imageSize.y();

	if (imageSize.x() * yCoef < maxSize.x())
		cyDIB = imageSize.y() * xCoef;
	else
		cxDIB = imageSize.x() * yCoef;

	return Vector2f(cxDIB, cyDIB);
}

bool ImageIO::getMultiBitmapInformation(const std::string& path, int& totalFrames, int& frameTime)
{	
	totalFrames = 1;
	frameTime = 0;

	FREE_IMAGE_FORMAT fileFormat;

#if WIN32
	fileFormat = FreeImage_GetFileTypeU(Utils::String::convertToWideString(path).c_str());
#else
	fileFormat = FreeImage_GetFileType(path.c_str());
#endif

	if (fileFormat != FIF_GIF && fileFormat != FIF_PNG)
		return false;

	if (!FreeImage_FIFSupportsReading(fileFormat))
		return false;

	auto size = Utils::FileSystem::getFileSize(path);
	if (size < 4)
		return false;

	unsigned char* data = new unsigned char[size]();
	if (data == nullptr)
		return false;

#if WIN32
	std::ifstream stream(Utils::String::convertToWideString(path), std::ios::binary);
#else
	std::ifstream stream(path, std::ios::binary);
#endif

	stream.read((char*)data, size);
	stream.close();

	bool result = false;

	FIMEMORY* fiMemory = FreeImage_OpenMemory((BYTE *)data, (DWORD)size);
	if (fiMemory != nullptr)
	{
		auto fiMultiBitmap = FreeImage_LoadMultiBitmapFromMemory(fileFormat, fiMemory);
		if (fiMultiBitmap != nullptr)
		{
			auto fiBitmap = FreeImage_LockPage(fiMultiBitmap, 0);
			if (fiBitmap != nullptr)
			{
				totalFrames = FreeImage_GetPageCount(fiMultiBitmap);
				if (totalFrames > 1)
				{
					FITAG* tagFrameTime = nullptr;
					FreeImage_GetMetadata(FIMD_ANIMATION, fiBitmap, "FrameTime", &tagFrameTime);
					if (tagFrameTime != nullptr && FreeImage_GetTagCount(tagFrameTime))
						frameTime = *static_cast<const uint32_t*>(FreeImage_GetTagValue(tagFrameTime));

					if (frameTime == 0)
						frameTime = 100;

					result = true;
				}

				FreeImage_UnlockPage(fiMultiBitmap, fiBitmap, false);
			}

			FreeImage_CloseMultiBitmap(fiMultiBitmap, 0);
		}

		FreeImage_CloseMemory(fiMemory);
	}

	delete[] data;

	return result;
}
