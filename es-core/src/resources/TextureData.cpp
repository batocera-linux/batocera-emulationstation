#include "resources/TextureData.h"

#include "math/Misc.h"
#include "renderers/Renderer.h"
#include "resources/ResourceManager.h"
#include "ImageIO.h"
#include "Log.h"
#include <nanosvg/nanosvg.h>
#include <nanosvg/nanosvgrast.h>
#include <string.h>
#include "Settings.h"

#include <algorithm>
#include "utils/ZipFile.h"
#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"

#define DPI 96

#define OPTIMIZEVRAM Settings::getInstance()->getBool("OptimizeVRAM")

TextureData::TextureData(bool tile, bool linear) : mTile(tile), mLinear(linear), mTextureID(0), mDataRGBA(nullptr), mScalable(false),
									  mWidth(0), mHeight(0), mSourceWidth(0.0f), mSourceHeight(0.0f),
									  mPackedSize(Vector2i(0, 0)), mBaseSize(Vector2i(0, 0))
{
	mIsExternalDataRGBA = false;
	mRequired = false;
}

TextureData::~TextureData()
{
	releaseVRAM();
	releaseRAM();
}

void TextureData::initFromPath(const std::string& path)
{
	// Just set the path. It will be loaded later
	mPath = path;
	// Only textures with paths are reloadable
	mReloadable = true;
}

bool TextureData::initSVGFromMemory(const unsigned char* fileData, size_t length)
{
	// If already initialised then don't read again
	std::unique_lock<std::mutex> lock(mMutex);
	if (mDataRGBA || (mTextureID != 0))
		return true;

	// nsvgParse excepts a modifiable, null-terminated string
	char* copy = (char*)malloc(length + 1);
	if (copy == NULL)
		return false;
	
	memcpy(copy, fileData, length);
	copy[length] = '\0';

	NSVGimage* svgImage = nsvgParse(copy, "px", DPI);
	free(copy);
	if (!svgImage)
	{
		LOG(LogError) << "Error parsing SVG image.";
		return false;
	}

	if (svgImage->width == 0 || svgImage->height == 0)
		return false;

	// We want to rasterise this texture at a specific resolution. If the source size
	// variables are set then use them otherwise set them from the parsed file
	if ((mSourceWidth == 0.0f) && (mSourceHeight == 0.0f))
	{
		mSourceWidth = svgImage->width;
		mSourceHeight = svgImage->height;

		if (!mMaxSize.empty() && mSourceWidth < mMaxSize.x() && mSourceHeight < mMaxSize.y())
		{
			auto sz = ImageIO::adjustPictureSize(Vector2i(mSourceWidth, mSourceHeight), Vector2i(mMaxSize.x(), mMaxSize.y()));
			mSourceWidth = sz.x();
			mSourceHeight = sz.y();
		}
	}
	else
		mSourceWidth = (mSourceHeight * svgImage->width) / svgImage->height; // FCA : Always compute width using source aspect ratio


	mWidth = (size_t)Math::round(mSourceWidth);
	mHeight = (size_t)Math::round(mSourceHeight);

	if (mWidth == 0)
	{
		// auto scale width to keep aspect
		mWidth = (size_t)Math::round(((float)mHeight / svgImage->height) * svgImage->width);
	}
	else if (mHeight == 0)
	{
		// auto scale height to keep aspect
		mHeight = (size_t)Math::round(((float)mWidth / svgImage->width) * svgImage->height);
	}

	mBaseSize = Vector2i(mWidth, mHeight);

	if (OPTIMIZEVRAM && !mMaxSize.empty())
	{
		if (mHeight < mMaxSize.y() && mWidth < mMaxSize.x()) // FCATMP
		{
			Vector2i sz = ImageIO::adjustPictureSize(Vector2i(mWidth, mHeight), Vector2i(mMaxSize.x(), mMaxSize.y()), mMaxSize.externalZoom());
			mHeight = sz.y();
			mWidth = Math::round((mHeight * svgImage->width) / svgImage->height);
		}

		if (!mMaxSize.empty() && (mWidth > mMaxSize.x() || mHeight > mMaxSize.y()))
		{
			Vector2i sz = ImageIO::adjustPictureSize(Vector2i(mWidth, mHeight), Vector2i(mMaxSize.x(), mMaxSize.y()), mMaxSize.externalZoom());
			mHeight = sz.y();
			mWidth = Math::round((mHeight * svgImage->width) / svgImage->height);
			
			mPackedSize = Vector2i(mWidth, mHeight);
		}
		else
			mPackedSize = Vector2i(0, 0);
	}
	else
		mPackedSize = Vector2i(0, 0);

	if (mWidth * mHeight <= 0)
	{
		LOG(LogError) << "Error parsing SVG image size.";
		return false;
	}

	unsigned char* dataRGBA = new unsigned char[mWidth * mHeight * 4];

	double scale = ((float)((int)mHeight)) / svgImage->height;
	double scaleV = ((float)((int)mWidth)) / svgImage->width;
	if (scaleV < scale)
		scale = scaleV;

	NSVGrasterizer* rast = nsvgCreateRasterizer();
	nsvgRasterize(rast, svgImage, 0, 0, scale, dataRGBA, (int)mWidth, (int)mHeight, (int)mWidth * 4);
	nsvgDeleteRasterizer(rast);
	nsvgDelete(svgImage);

	ImageIO::flipPixelsVert(dataRGBA, mWidth, mHeight);

	mDataRGBA = dataRGBA;

	return true;
}

bool TextureData::initImageFromMemory(const unsigned char* fileData, size_t length, int subImageIndex)
{
	size_t width, height;

	// If already initialised then don't read again
	{
		std::unique_lock<std::mutex> lock(mMutex);
		if (mDataRGBA || (mTextureID != 0))
			return true;
	}

	MaxSizeInfo maxSize(Renderer::getScreenWidth(), Renderer::getScreenHeight(), false);
	if (!mMaxSize.empty())
		maxSize = mMaxSize;
	
	unsigned char* imageRGBA = nullptr;
	
	if (subImageIndex >= 0)
		imageRGBA = ImageIO::loadFromMemoryRGBA32((const unsigned char*)(fileData), length, width, height, &maxSize, &mBaseSize, &mPackedSize, subImageIndex);
	else
		imageRGBA = ImageIO::loadFromMemoryRGBA32((const unsigned char*)(fileData), length, width, height, &maxSize, &mBaseSize, &mPackedSize);

	if (imageRGBA == nullptr)
	{
		LOG(LogError) << "Could not initialize texture from memory, invalid data!  (file path: " << mPath << ", data ptr: " << (size_t)fileData << ", reported size: " << length << ")";
		return false;
	}

	mSourceWidth = (float) width;
	mSourceHeight = (float) height;
	mScalable = false;

	return initFromRGBA(imageRGBA, width, height, false);
}

bool TextureData::initFromRGBA(unsigned char* dataRGBA, size_t width, size_t height, bool copyData)
{
	// If already initialised then don't read again
	std::unique_lock<std::mutex> lock(mMutex);

	if (mIsExternalDataRGBA)
	{
		mIsExternalDataRGBA = false;
		mDataRGBA = nullptr;
	}

	if (mDataRGBA)
		return true;

	if (copyData)
	{
		// Take a copy
		mDataRGBA = new unsigned char[width * height * 4];
		memcpy(mDataRGBA, dataRGBA, width * height * 4);
	}
	else
		mDataRGBA = dataRGBA;

	mWidth = width;
	mHeight = height;
	return true;
}

bool TextureData::updateFromExternalRGBA(unsigned char* dataRGBA, size_t width, size_t height)
{
	// If already initialised then don't read again
	std::unique_lock<std::mutex> lock(mMutex);

	if (!mIsExternalDataRGBA && mDataRGBA != nullptr)
		delete[] mDataRGBA;

	mIsExternalDataRGBA = true;
	mDataRGBA = dataRGBA;
	mWidth = width;
	mHeight = height;

	if (mTextureID != 0)
		Renderer::updateTexture(mTextureID, Renderer::Texture::RGBA, 0, 0, mWidth, mHeight, mDataRGBA);

	return true;
}

static std::mutex mCbzMutex;

bool TextureData::loadFromCbz()
{
	std::unique_lock<std::mutex> lock(mCbzMutex);

	bool retval = false;

	std::vector<Utils::Zip::ZipInfo> files;

	Utils::Zip::ZipFile zipFile;
	if (zipFile.load(mPath))
	{
		for (auto file : zipFile.infolist())
		{
			auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(file.filename));
			if (ext != ".jpg")
				continue;

			if (Utils::String::startsWith(file.filename, "__"))
				continue;

			files.push_back(file);
		}

		std::sort(files.begin(), files.end(), [](const Utils::Zip::ZipInfo& a, const Utils::Zip::ZipInfo& b) { return Utils::String::toLower(a.filename) < Utils::String::toLower(b.filename); });
	}

	if (files.size() > 0 && files[0].file_size > 0)
	{
		size_t size = files[0].file_size;
		unsigned char* buffer = new unsigned char[size];

		Utils::Zip::zip_callback func = [](void *pOpaque, unsigned long long ofs, const void *pBuf, size_t n)
		{
			unsigned char* pSource = (unsigned char*)pBuf;
			unsigned char* pDest = (unsigned char*)pOpaque;

			memcpy(pDest + ofs, pSource, n);

			return n;
		};

		zipFile.readBuffered(files[0].filename, func, buffer);

		retval = initImageFromMemory(buffer, size);

		if (retval)
			ImageIO::updateImageCache(mPath, Utils::FileSystem::getFileSize(mPath), mBaseSize.x(), mBaseSize.y());
	}

	return retval;
}

bool TextureData::load(bool updateCache)
{
	bool retval = false;

	// Need to load. See if there is a file
	if (!mPath.empty())
	{
		LOG(LogDebug) << "TextureData::load " << mPath;

		if (mPath.substr(mPath.size() - 4, std::string::npos) == ".cbz")
			return loadFromCbz();
		
		std::string path = mPath;
		int subImageIndex = -1;

		if (Utils::FileSystem::getExtension(mPath).find(",") != std::string::npos)
		{
			auto idx = path.rfind(',');
			subImageIndex = Utils::String::toInteger(mPath.substr(idx+1));
			path = mPath.substr(0, idx);			
		}

		std::shared_ptr<ResourceManager>& rm = ResourceManager::getInstance();
		const ResourceData& data = rm->getFileData(path);
		// is it an SVG?
		if (mPath.substr(mPath.size() - 4, std::string::npos) == ".svg")
		{
			mScalable = true;
			retval = initSVGFromMemory((const unsigned char*)data.ptr.get(), data.length);
		}
		else
			retval = initImageFromMemory((const unsigned char*)data.ptr.get(), data.length, subImageIndex);

		if (updateCache && retval)
			ImageIO::updateImageCache(mPath, data.length, mBaseSize.x(), mBaseSize.y());
	}

	return retval;
}

bool TextureData::isLoaded()
{
	std::unique_lock<std::mutex> lock(mMutex);
	if (mDataRGBA || (mTextureID != 0))
		return true;
	return false;
}

bool TextureData::uploadAndBind()
{
	// See if it's already been uploaded
	std::unique_lock<std::mutex> lock(mMutex);

	if (mTextureID != 0)
		Renderer::bindTexture(mTextureID);
	else
	{
		// Make sure we're ready to upload
		if (mWidth == 0 || mHeight == 0 || mDataRGBA == nullptr)
		{
			Renderer::bindTexture(mTextureID);
			return false;
		}

		// Upload texture
		mTextureID = Renderer::createTexture(Renderer::Texture::RGBA, mLinear, mTile, mWidth, mHeight, mDataRGBA);
		if (mTextureID == 0)
			return false;

		if (mDataRGBA != nullptr && !mIsExternalDataRGBA)
			delete[] mDataRGBA;

		mDataRGBA = nullptr;
	}

	return true;
}

void TextureData::releaseVRAM()
{
	std::unique_lock<std::mutex> lock(mMutex);
	if (mTextureID != 0)
	{
		Renderer::destroyTexture(mTextureID);
		mTextureID = 0;
	}
}

void TextureData::releaseRAM()
{
	std::unique_lock<std::mutex> lock(mMutex);

	if (mDataRGBA != nullptr && !mIsExternalDataRGBA)
		delete[] mDataRGBA;

	mDataRGBA = 0;
}

size_t TextureData::width()
{
	if (mWidth == 0)
		load();
	return mWidth;
}

size_t TextureData::height()
{
	if (mHeight == 0)
		load();
	return mHeight;
}

float TextureData::sourceWidth()
{
	if (mSourceWidth == 0)
		load();
	return mSourceWidth;
}

float TextureData::sourceHeight()
{
	if (mSourceHeight == 0)
		load();
	return mSourceHeight;
}

void TextureData::setTemporarySize(float width, float height)
{
	mWidth = width;
	mHeight = height;
	mSourceWidth = width;
	mSourceHeight = height;
}

void TextureData::setSourceSize(float width, float height)
{
	if (mScalable)
	{
		if ((int) mSourceHeight < (int) height && (int) mSourceWidth != (int) width)
		{
			LOG(LogDebug) << "Requested scalable image size too small. Reloading image from (" << mSourceWidth << ", " << mSourceHeight << ") to (" << width << ", " << height << ")";

			mSourceWidth = width;
			mSourceHeight = height;
			releaseVRAM();
			releaseRAM();
			load();
		}
	}
}

size_t TextureData::getVRAMUsage()
{
	if ((mTextureID != 0) || (mDataRGBA != nullptr))
		return mWidth * mHeight * 4;
	else
		return 0;
}

void TextureData::setMaxSize(MaxSizeInfo maxSize)
{
	if (!Settings::getInstance()->getBool("OptimizeVRAM"))
		return;

	if (mSourceWidth == 0 || mSourceHeight == 0)
		mMaxSize = maxSize;
	else
	{
		Vector2i value = ImageIO::adjustPictureSize(Vector2i(mSourceWidth, mSourceHeight), Vector2i(mMaxSize.x(), mMaxSize.y()), mMaxSize.externalZoom());
		Vector2i newVal = ImageIO::adjustPictureSize(Vector2i(mSourceWidth, mSourceHeight), Vector2i(maxSize.x(), maxSize.y()), mMaxSize.externalZoom());

		if (newVal.x() > value.x() || newVal.y() > value.y())
			mMaxSize = maxSize;
	}
}

bool TextureData::isMaxSizeValid()
{
	if (!OPTIMIZEVRAM)
		return true;

	if (mPackedSize == Vector2i(0, 0))
		return true;

	if (mBaseSize == Vector2i(0, 0))
		return true;

	if (mMaxSize.empty())
		return true;

	if ((int)mMaxSize.x() <= mPackedSize.x() || (int)mMaxSize.y() <= mPackedSize.y())
		return true;

	if (mBaseSize.x() <= mPackedSize.x() || mBaseSize.y() <= mPackedSize.y())
		return true;

	return false;
}
