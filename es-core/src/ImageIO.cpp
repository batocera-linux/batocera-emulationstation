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

const MaxSizeInfo MaxSizeInfo::Empty;

unsigned char* ImageIO::loadFromMemoryRGBA32(const unsigned char * data, const size_t size, size_t & width, size_t & height, MaxSizeInfo* maxSize, Vector2i* baseSize, Vector2i* packedSize, int subImageIndex)
{
	LOG(LogDebug) << "ImageIO::loadFromMemoryRGBA32";

	if (baseSize != nullptr)
		*baseSize = Vector2i(0, 0);

	if (baseSize != nullptr)
		*packedSize = Vector2i(0, 0);

	std::vector<unsigned char> rawData;
	width = 0;
	height = 0;
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

					size_t maxX = maxSize == nullptr ? 0 : (size_t) Math::round(maxSize->x());
					size_t maxY = maxSize == nullptr ? 0 : (size_t) Math::round(maxSize->y());

					if (maxSize != nullptr && maxX > 0 && maxY > 0 && (width > maxX || height > maxY))
					{
						Vector2i sz = adjustPictureSize(Vector2i(width, height), Vector2i(maxX, maxY), maxSize->externalZoom());

						if (sz.x() > Renderer::getScreenWidth() || sz.y() > Renderer::getScreenHeight())
							sz = adjustPictureSize(sz, Vector2i(Renderer::getScreenWidth(), Renderer::getScreenHeight()), false);
						
						if (sz.x() != width || sz.y() != height)
						{
							LOG(LogDebug) << "ImageIO : rescaling image from " << std::string(std::to_string(width) + "x" + std::to_string(height)).c_str() << " to " << std::string(std::to_string(sz.x()) + "x" + std::to_string(sz.y())).c_str();

							FIBITMAP* imageRescaled = FreeImage_Rescale(fiBitmap, sz.x(), sz.y(), FILTER_BOX);

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
