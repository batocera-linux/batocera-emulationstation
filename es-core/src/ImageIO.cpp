#include "ImageIO.h"

#include "Log.h"
#include <string.h>
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <sstream>
#include <fstream>
#include <map>
#include <mutex>
#include "renderers/Renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stbimage/stb_image.h"
#include "stbimage/stb_image_resize.h"
#include "stbimage/stb_image_write.h"

//you can pass 0 for width or height to keep aspect ratio
bool ImageIO::resizeImage(const std::string& path, int maxWidth, int maxHeight)
{
	LOG(LogDebug) << "ImageIO::resizeImage " << path << " max=" << maxWidth << "x" << maxHeight;

	// nothing to do
	if(maxWidth == 0 && maxHeight == 0)
		return true;
	
	//detect the filetype
	int imgSizeX, imgSizeY, imgChannels;
	if (stbi_info(path.c_str(), &imgSizeX, &imgSizeY, &imgChannels) != 1)
	{
		LOG(LogError) << "Error - could not detect filetype for image \"" << path << "\"!";
		return false;
	}

	//make sure we can read this filetype first, then load it
	stbi_set_flip_vertically_on_load(1);
	stbi_uc* image = stbi_load(path.c_str(), &imgSizeX, &imgSizeY, &imgChannels, imgChannels);
	if (image == nullptr)
	{
		LOG(LogError) << "Error - file format reading not supported for image \"" << path << "\"!";
		return false;
	}

	if (imgSizeX == 0 || imgSizeY == 0)
	{
		stbi_image_free(image);
		return true;
	}

	float width = imgSizeX;
	float height = imgSizeY;

	if(maxWidth == 0)
		maxWidth = (int)((maxHeight / height) * width);
	else if(maxHeight == 0)
		maxHeight = (int)((maxWidth / width) * height);
	
	if (width <= maxWidth && height <= maxHeight)
	{
		stbi_image_free(image);
		return true;
	}
	
	// Rescale through stb_image_resize (FreeImage was using FILTER_BILINEAR)
	unsigned char* stbiResizedBitmap = new unsigned char[maxWidth * maxHeight * imgChannels];
	if (stbir_resize_uint8(image, imgSizeX, imgSizeY, 0, stbiResizedBitmap, maxWidth, maxHeight, 0, imgChannels) != 1)
	{
		LOG(LogError) << "Error - Failed to resize image from memory!";
		delete[] stbiResizedBitmap;
		return false;
	}
	stbi_image_free(image);
		
	bool saved = false;
	
	try
	{
		if (imgChannels == 4)
			saved = (stbi_write_png(path.c_str(), maxWidth, maxHeight, imgChannels, stbiResizedBitmap, 0) == 1);
		else
			saved = (stbi_write_jpg(path.c_str(),maxWidth, maxHeight, imgChannels, stbiResizedBitmap, 90) == 1);
	}
	catch(...) { }

	delete[] stbiResizedBitmap;

	if(!saved)
		LOG(LogError) << "Failed to save resized image!";

	return saved;
}

unsigned char* ImageIO::loadFromMemoryRGBA32(const unsigned char * data, const size_t size, size_t & width, size_t & height, MaxSizeInfo* maxSize, Vector2i* baseSize, Vector2i* packedSize)
{
	LOG(LogDebug) << "ImageIO::loadFromMemoryRGBA32";

	if (baseSize != nullptr)
		*baseSize = Vector2i(0, 0);

	if (baseSize != nullptr)
		*packedSize = Vector2i(0, 0);

	unsigned char* stbiResizedBitmap = nullptr;
	width = 0;
	height = 0;

	// Check image is supported by stb_image
	int imgSizeX, imgSizeY, imgChannels;
	if (stbi_info_from_memory(data, size, &imgSizeX, &imgSizeY, &imgChannels) != 1)
	{
			LOG(LogError) << "Error - Failed to decode image from memory!";
			return nullptr;
	}
	
	// Do the load through stb_image
	stbi_set_flip_vertically_on_load(1);
	stbi_uc* stbiBitmap = stbi_load_from_memory(data, size, &imgSizeX, &imgSizeY, &imgChannels, 4);
	if (stbiBitmap != nullptr) 
	{
		width = imgSizeX;
		height = imgSizeY;

		if (baseSize != nullptr)
			*baseSize = Vector2i(width, height);

		if (maxSize != nullptr && maxSize->x() > 0 && maxSize->y() > 0 && (width > maxSize->x() || height > maxSize->y()))
		{
			Vector2i sz = adjustPictureSize(Vector2i(width, height), Vector2i(maxSize->x(), maxSize->y()), maxSize->externalZoom());

			if (sz.x() > Renderer::getScreenWidth() || sz.y() > Renderer::getScreenHeight())
				sz = adjustPictureSize(sz, Vector2i(Renderer::getScreenWidth(), Renderer::getScreenHeight()), false);
			
			if (sz.x() != width || sz.y() != height)
			{
				LOG(LogDebug) << "ImageIO : rescaling image from " << std::string(std::to_string(width) + "x" + std::to_string(height)).c_str() << " to " << std::string(std::to_string(sz.x()) + "x" + std::to_string(sz.y())).c_str();

				// Rescale through stb_image_resize (FreeImage was using FILTER_BOX)
				stbiResizedBitmap = new unsigned char[sz.x() * sz.y() * 4];
				if (stbir_resize_uint8(stbiBitmap, width, height, 0, stbiResizedBitmap, sz.x(), sz.y(), 0, 4) != 1)
				{
					LOG(LogError) << "Error - Failed to resize image from memory!";
					delete[] stbiResizedBitmap;
					return nullptr;
				}
				stbi_image_free(stbiBitmap);

				width = sz.x();
				height = sz.y();
				
				if (packedSize != nullptr)
					*packedSize = Vector2i(width, height);
				
				stbiBitmap = stbiResizedBitmap;
			}
		}

		LOG(LogDebug) << "ImageIO : returning decoded image ";

		/*unsigned char* tempData = new unsigned char[width * height * 4];

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

		FreeImage_Unload(fiBitmap);
		FreeImage_CloseMemory(fiMemory);*/

		return stbiBitmap;

	}
	else
	{
			LOG(LogError) << "Error - Failed to load image from memory!";
			return nullptr;
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

static std::map<std::string, CachedFileInfo> sizeCache;
static bool sizeCacheDirty = false;

std::string getImageCacheFilename()
{
	return Utils::FileSystem::getEsConfigPath() + "/imagecache.db";
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

#if WIN32
	std::string relativeTo = Utils::FileSystem::getParent(Utils::FileSystem::getHomePath());
#else
	std::string relativeTo = "/userdata";	
#endif

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
		path.find("/tmp/") != std::string::npos &&
		path.find("/emulationstation.tmp/") != std::string::npos &&
		path.find("/pdftmp/") == std::string::npos && 
		path.find("/saves/") != std::string::npos;
}

void ImageIO::saveImageCache()
{
	if (!sizeCacheDirty)
		return;

	std::string fname = getImageCacheFilename();
	std::ofstream f(fname.c_str(), std::ios::binary);
	if (f.fail())
		return;

#if WIN32
	std::string relativeTo = Utils::FileSystem::getParent(Utils::FileSystem::getHomePath());
#else
	std::string relativeTo = "/userdata";
#endif

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

void ImageIO::removeImageCache(const std::string fn)
{
	std::unique_lock<std::mutex> lock(sizeCacheLock);

	auto it = sizeCache.find(fn);
	if (it != sizeCache.cend())
		sizeCache.erase(fn);
}

void ImageIO::updateImageCache(const std::string fn, int sz, int x, int y)
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


bool ImageIO::loadImageSize(const char *fn, unsigned int *x, unsigned int *y)
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
	if (ext != ".jpg" && ext != ".png" && ext != ".jpeg" && ext != ".gif")
	{
		LOG(LogWarning) << "ImageIO::loadImageSize\tUnknown file type";
		return false;
	}

	auto size = Utils::FileSystem::getFileSize(fn);

#if WIN32
	FILE *f = _fsopen(fn, "rb", _SH_DENYNO);
#else
	FILE *f = fopen(fn, "rb");
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
	unsigned char buf[24]; 
	if (fread(buf, 1, 24, f) != 24)
	{
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
