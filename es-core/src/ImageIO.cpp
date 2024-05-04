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
				if (FreeImage_GetBPP(fiBitmap) != 32)
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

					int w = (int)width;

					for (int y = (int)height; --y >= 0; )
					{
						unsigned int* argb = (unsigned int*)FreeImage_GetScanLine(fiBitmap, y);
						unsigned int* abgr = (unsigned int*)(tempData + (y * width * 4));
						for (int x = w; --x >= 0;)
						{
							unsigned int c = argb[x];
							abgr[x] = (c & 0xFF00FF00) | ((c & 0xFF) << 16) | ((c >> 16) & 0xFF);
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

struct CachedFileInfo
{
	CachedFileInfo(int sz, int sx, int sy)
	{
		size = sz;
		x = sx;
		y = sy;		
	};

	CachedFileInfo()
	{
		size = 0;
		x = 0;
		y = 0;		
	};

	int size;
	int x;
	int y;	
};

static std::unordered_map<std::string, CachedFileInfo> sizeCache;
static bool sizeCacheDirty = false;

std::string getImageCacheFilename()
{
	return Paths::getUserEmulationStationPath() + "/imagecache.db";
}

void ImageIO::clearImageCache()
{
	std::string fname = getImageCacheFilename();
	Utils::FileSystem::removeFile(fname);
	sizeCache.clear();
}

void ImageIO::loadImageCache()
{
	std::string fname = getImageCacheFilename();

	std::ifstream f(fname.c_str());
	if (f.fail())
		return;

	sizeCache.clear();

	std::string relativeTo = Paths::getRootPath();

	std::vector<std::string> splits;

	std::string line;
	while (std::getline(f, line))
	{
		splits.clear();

		const char* src = line.c_str();

		while (true)
		{
			const char* d = strchr(src, '|');
			size_t len = (d) ? d - src : strlen(src);

			if (len)
				splits.push_back(std::string(src, len)); // capture token

			if (d) src += len + 1; else break;
		}

		if (splits.size() == 4)
		{
			std::string file = Utils::FileSystem::resolveRelativePath(splits[0], relativeTo, true);

			CachedFileInfo fi;
			fi.size = Utils::String::toInteger(splits[1]);
			fi.x = Utils::String::toInteger(splits[2]);
			fi.y = Utils::String::toInteger(splits[3]);

			sizeCache[file] = fi;
		}
	}

	f.close();
}

static bool _isCachablePath(const std::string& path)
{
	return 
		path.find("/themes/") == std::string::npos && 
		path.find("/tmp/") == std::string::npos &&
		path.find("/emulationstation.tmp/") == std::string::npos &&
		path.find("/pdftmp/") == std::string::npos && 
		path.find("/saves/") == std::string::npos;
}

void ImageIO::saveImageCache()
{
	if (!sizeCacheDirty)
		return;

	std::string fname = getImageCacheFilename();
	std::ofstream f(fname.c_str(), std::ios::binary);
	if (f.fail())
		return;

	std::string relativeTo = Paths::getRootPath();

	for (auto it : sizeCache)
	{
		if (it.second.size < 0)
			continue;

		if (!_isCachablePath(it.first))
			continue;

		std::string path = Utils::FileSystem::createRelativePath(it.first, relativeTo, true); 

		f << path;
		f << "|";
		f << std::to_string(it.second.size);
		f << "|";
		f << std::to_string(it.second.x);
		f << "|";
		f << std::to_string(it.second.y);
		f << "\n";
	}

	f.close();
}

static std::mutex sizeCacheLock;

void ImageIO::removeImageCache(const std::string& fn)
{
	std::unique_lock<std::mutex> lock(sizeCacheLock);

	auto it = sizeCache.find(fn);
	if (it != sizeCache.cend())
		sizeCache.erase(fn);
}

void ImageIO::updateImageCache(const std::string& fn, int sz, int x, int y)
{
	std::unique_lock<std::mutex> lock(sizeCacheLock);

	auto it = sizeCache.find(fn);
	if (it != sizeCache.cend())
	{
		if (x != it->second.x || y != it->second.y || sz != it->second.size)
		{
			auto& item = it->second;

			item.x = x;
			item.y = y;
			item.size = sz;

			if (sz > 0 && x > 0 && _isCachablePath(fn))
				sizeCacheDirty = true;
		}
	}
	else
	{
		sizeCache[fn] = CachedFileInfo(sz, x, y);

		if (sz > 0 && x > 0 && _isCachablePath(fn))
			sizeCacheDirty = true;
	}
}

static bool extractSvgSize(const std::string& svgFilePath, float& width, float& height)
{
	width = -1;
	height = -1;

	std::ifstream svgFile(svgFilePath);

	if (!svgFile.is_open())
		return false;

	bool started = false;

	std::string line;
	while (std::getline(svgFile, line))
	{
		if (!started)
			started = line.find("<svg") != std::string::npos;

		if (!started)
			continue;

		// Find the width attribute within the <svg> element
		size_t widthPos = line.find("width=\"");
		if (widthPos != std::string::npos)
		{
			widthPos += 7;

			size_t widthEnd = line.find("\"", widthPos);
			if (widthEnd != std::string::npos)
			{
				std::string widthStr = line.substr(widthPos, widthEnd - widthPos);
				width = Utils::String::toFloat(widthStr);
			}
		}

		// Find the height attribute within the <svg> element
		size_t heightPos = line.find("height=\"");
		if (heightPos != std::string::npos)
		{
			heightPos += 8;

			// Extract the height value		
			size_t heightEnd = line.find("\"", heightPos);
			if (heightEnd != std::string::npos)
			{
				std::string heightStr = line.substr(heightPos, heightEnd - heightPos);
				height = Utils::String::toFloat(heightStr);
			}
		}

		// Break the loop once width and height are found
		if (width > 0 && height > 0)
			break;

		size_t viewBoxPos = line.find("viewBox=\"");
		if (viewBoxPos != std::string::npos)
		{
			viewBoxPos += 9;

			// Extract the viewBox value

			size_t viewBoxEnd = line.find("\"", viewBoxPos);
			if (viewBoxEnd != std::string::npos)
			{
				std::string viewBoxStr = line.substr(viewBoxPos, viewBoxEnd - viewBoxPos);

				auto vec = Vector4f::parseString(viewBoxStr);
				width = vec.z();
				height = vec.w();
				break;
			}
		}

		if (line.find(">") != std::string::npos)
			break;
	}

	svgFile.close();

	return width > 0 && height > 0;
}

static int get16bit(const uint8_t* data, int index) 
{
	return (data[index] & 0xFF) | ((data[index + 1] & 0xFF) << 8);
}

static int get24bit(const uint8_t* data, int index)
{
	return get16bit(data, index) | ((data[index + 2] & 0xFF) << 16);
}

bool ImageIO::loadImageSize(const std::string& fn, unsigned int *x, unsigned int *y)
{
	{
		std::unique_lock<std::mutex> lock(sizeCacheLock);

		auto it = sizeCache.find(fn);
		if (it != sizeCache.cend())
		{
			if (it->second.size < 0)
				return false;

			*x = it->second.x;
			*y = it->second.y;
			return true;
		}
	}

	LOG(LogDebug) << "ImageIO::loadImageSize " << fn;

	auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(fn));
	
	if (ext == ".svg")
	{
		float width, height;
		if (!extractSvgSize(fn, width, height))
		{
			updateImageCache(fn, -1, -1, -1);
			return false;
		}

		if (width == (int)width && height == (int)height)
		{
			auto size = Utils::FileSystem::getFileSize(fn);

			*x = width;
			*y = height;
			updateImageCache(fn, size, *x, *y);

			return true;
		}

		updateImageCache(fn, -1, -1, -1);
		return false;
	}
	
	if (ext != ".jpg" && ext != ".png" && ext != ".jpeg" && ext != ".gif" && ext != ".webp")
	{
		LOG(LogWarning) << "ImageIO::loadImageSize\tUnknown file type";
		return false;
	}

	auto size = Utils::FileSystem::getFileSize(fn);

#if WIN32
	FILE* f = _wfopen(Utils::String::convertToWideString(fn).c_str(), L"rb");
#else
	FILE *f = fopen(fn.c_str(), "rb");
#endif

	if (f == 0)
	{
		LOG(LogWarning) << "ImageIO::loadImageSize\tUnable to open file";
		updateImageCache(fn, -1, -1, -1);
		return false;
	}

	// Strategy:
	// reading GIF dimensions requires the first 10 bytes of the file
	// reading PNG dimensions requires the first 24 bytes of the file
	// reading JPEG dimensions requires scanning through jpeg chunks
	// In all formats, the file is at least 24 bytes big, so we'll read that always

	// We need 30 bytes to read WebP dimensions
#define BUFSIZE 30

	unsigned char buf[BUFSIZE];
	if (fread(buf, 1, BUFSIZE, f) != BUFSIZE)
	{
		updateImageCache(fn, -1, -1, -1);
		return false;
	}

	// WebP file
	if (buf[0] == 'R' && buf[1] == 'I' && buf[2] == 'F' && buf[3] == 'F' && buf[12] == 'V' && buf[13] == 'P' && buf[14] == '8')
	{
		switch (buf[15])
		{
		case ' ':			
			*x = get16bit(buf, 26) & 0x3FFF;
			*y = get16bit(buf, 28) & 0x3FFF;
			updateImageCache(fn, size, *x, *y);
			return true;			
		case 'X':
			*x = 1 + (get24bit(buf, 24));
			*y = 1 + (get24bit(buf, 27));
			updateImageCache(fn, size, *x, *y);
			return true;
		case 'L':
			{
				int firstBytes = get16bit(buf, 21);
				int width = 1 + (firstBytes & 0x3FFF);
				int lastTwoDigits = (firstBytes & 0xC000) >> 14;
				int height = 1 + (((get16bit(buf, 23) & 0xFFF) << 2) | lastTwoDigits);

				*y = height;
				*x = width;
				updateImageCache(fn, size, *x, *y);
				return true;
			}
		}

		updateImageCache(fn, -1, -1, -1);
		return false;
	}

	// For JPEGs, we need to read the first 12 bytes of each chunk.
	// We'll read those 12 bytes at buf+2...buf+14, i.e. overwriting the existing buf.
	bool jfif = false;

	if ((buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF && buf[3] == 0xE0 && buf[6] == 'J' && buf[7] == 'F' && buf[8] == 'I' && buf[9] == 'F') ||
		(buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF && buf[3] == 0xE1 && buf[6] == 'E' && buf[7] == 'x' && buf[8] == 'i' && buf[9] == 'f'))
	{
		jfif = true;

		long pos = 2;
		while (buf[2] == 0xFF)
		{
			if (buf[3] == 0xC0 || buf[3] == 0xC1 || buf[3] == 0xC2 || buf[3] == 0xC3 || buf[3] == 0xC9 || buf[3] == 0xCA || buf[3] == 0xCB)
				break;

			pos += 2 + (buf[4] << 8) + buf[5];
		
			if (fseek(f, pos, SEEK_SET) != 0)
				break;

			if (fread(buf + 2, 1, 12, f) != 12)
				break;
		}
	}

	fclose(f);

	// JPEG: (first two bytes of buf are first two bytes of the jpeg file; rest of buf is the DCT frame
	if (jfif && buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF)
	{
		*y = (buf[7] << 8) + buf[8];
		*x = (buf[9] << 8) + buf[10];
		
		LOG(LogDebug) << "ImageIO::loadImageSize\tJPG size " << std::string(std::to_string(*x) + "x" + std::to_string(*y)).c_str();

		if (*x > 5000) // security ?
		{
			updateImageCache(fn, -1, -1, -1);
			return false;
		}

		updateImageCache(fn, size, *x, *y);
		return true;
	}

	// GIF: first three bytes say "GIF", next three give version number. Then dimensions
	if (buf[0] == 'G' && buf[1] == 'I' && buf[2] == 'F')
	{
		*x = buf[6] + (buf[7] << 8);
		*y = buf[8] + (buf[9] << 8);

		LOG(LogDebug) << "ImageIO::loadImageSize\tGIF size " << std::string(std::to_string(*x) + "x" + std::to_string(*y)).c_str();

		updateImageCache(fn, size, *x, *y);
		return true;
	}

	// PNG: the first frame is by definition an IHDR frame, which gives dimensions
	if (buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G' && buf[4] == 0x0D && buf[5] == 0x0A && buf[6] == 0x1A && buf[7] == 0x0A && buf[12] == 'I' && buf[13] == 'H' && buf[14] == 'D' && buf[15] == 'R')
	{
		*x = (buf[16] << 24) + (buf[17] << 16) + (buf[18] << 8) + (buf[19] << 0);
		*y = (buf[20] << 24) + (buf[21] << 16) + (buf[22] << 8) + (buf[23] << 0);

		LOG(LogDebug) << "ImageIO::loadImageSize\tPNG size " << std::string(std::to_string(*x) + "x" + std::to_string(*y)).c_str();

		updateImageCache(fn, size, *x, *y);
		return true;
	}

	updateImageCache(fn, -1, -1, -1);
	LOG(LogWarning) << "ImageIO::loadImageSize\tUnable to extract size";
	return false;
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
