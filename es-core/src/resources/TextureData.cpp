#include "resources/TextureData.h"

#include "math/Misc.h"
#include "renderers/Renderer.h"
#include "resources/ResourceManager.h"
#include "resources/TextureDiskCache.h"
#include "ImageIO.h"
#include "Log.h"
#include <nanosvg/nanosvg.h>
#include <nanosvg/nanosvgrast.h>
#include <string.h>
#include <algorithm>
#include <vlc/vlc.h>
#include <memory>

#include "Settings.h"
#include "utils/ZipFile.h"
#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringListLock.h"
#include "Paths.h"

#define DPI 96

#define OPTIMIZEVRAM Settings::getInstance()->getBool("OptimizeVRAM")

IPdfHandler* TextureData::PdfHandler = nullptr;

TextureData::TextureData(bool tile, bool linear) : 
	mTile(tile), mLinear(linear), mTextureID(0), mDataRGBA(nullptr), mScalable(false), mDynamic(true), mReloadable(false),
#if defined(USE_OPENGLES_30)
	mMipmapped(false),
#endif
	mSize(Vector2i::Zero()), mPhysicalSize(Vector2f::Zero()), mMaxSize(MaxSizeInfo::Empty)
{
	mIsExternalDataRGBA = false;
	mRequired = false;
}

TextureData::~TextureData()
{
	releaseVRAM();
	releaseRAM();
}

#if defined(USE_OPENGLES_30)
size_t TextureData::getMemoryUsage(MemoryUsageType type)
{
	std::unique_lock<std::mutex> lock(mMutex);

	const size_t width = mSize.x() > 0 ? static_cast<size_t>(mSize.x()) : 0;
	const size_t height = mSize.y() > 0 ? static_cast<size_t>(mSize.y()) : 0;
	const size_t baseSize = width * height * 4;

	bool accountForMipmaps = mMipmapped;
	if (type == MemoryUsageType::Estimated && !accountForMipmaps)
	{
		accountForMipmaps = mReloadable && !mIsExternalDataRGBA && mLinear && !mTile &&
			!Utils::FileSystem::isVideo(mPath) && (mSize.x() >= 256 || mSize.y() >= 256);
	}

	size_t textureSize = baseSize;
	if (accountForMipmaps && width > 0 && height > 0)
	{
		textureSize = 0;
		size_t levelWidth = width;
		size_t levelHeight = height;
		while (true)
		{
			textureSize += levelWidth * levelHeight * 4;
			if (levelWidth == 1 && levelHeight == 1)
				break;
			levelWidth = std::max<size_t>(1, levelWidth / 2);
			levelHeight = std::max<size_t>(1, levelHeight / 2);
		}
	}

	if (type == MemoryUsageType::RAM)
		return mDataRGBA != nullptr ? baseSize : 0;
	if (type == MemoryUsageType::VRAM)
		return mTextureID != 0 ? textureSize : 0;
	if (type == MemoryUsageType::Estimated)
		return textureSize;
	return mTextureID != 0 ? textureSize : (mDataRGBA != nullptr ? baseSize : 0);
}
#endif

void TextureData::initFromPath(const std::string& path)
{
	// Just set the path. It will be loaded later
	mPath = path;
	// Only textures with paths are reloadable
	mReloadable = true;
}

bool TextureData::initSVGFromMemory(const unsigned char* fileData, size_t length)
{
	// Use a separate load lock so VRAM cleanup can still run.
	std::unique_lock<std::mutex> imageLoadLock(mImageLoadMutex);

	{
		std::unique_lock<std::mutex> lock(mMutex);
		if (mDataRGBA || mTextureID != 0)
			return true;
	}

	// nsvgParse needs a writable string with a trailing null.
	char* copy = (char*)malloc(length + 1);
	if (copy == nullptr)
		return false;

	memcpy(copy, fileData, length);
	copy[length] = '\0';

	std::unique_ptr<NSVGimage, decltype(&nsvgDelete)> svgImage(nsvgParse(copy, "px", DPI), nsvgDelete);
	free(copy);

	if (!svgImage)
	{
		LOG(LogError) << "Error parsing SVG image.";
		return false;
	}

	if (svgImage->width == 0 || svgImage->height == 0)
		return false;

	// Retry if the requested size changes while loading.
	for (;;)
	{
		Vector2f scalableMinimumSize;
		MaxSizeInfo maxSize = MaxSizeInfo::Empty;

		{
			std::unique_lock<std::mutex> lock(mMutex);
			if (mDataRGBA || mTextureID != 0)
				return true;

			scalableMinimumSize = mScalableMinimumSize;
			maxSize = mMaxSize;
		}

		float sourceWidth = svgImage->width;
		float sourceHeight = svgImage->height;

		if (scalableMinimumSize.empty())
		{
			if (!maxSize.empty() &&
				sourceWidth < maxSize.x() && sourceHeight < maxSize.y())
			{
				auto sz = ImageIO::adjustPictureSizeF(
					sourceWidth, sourceHeight,
					maxSize.x(), maxSize.y(), maxSize.externalZoom());

				sourceHeight = sz.y();
				sourceWidth = (sourceHeight * svgImage->width) / svgImage->height;
			}
		}
		else
		{
			sourceHeight = scalableMinimumSize.y();
			sourceWidth = (sourceHeight * svgImage->width) / svgImage->height;
		}

		size_t width = (size_t)Math::round(sourceWidth);
		size_t height = (size_t)Math::round(sourceHeight);

		if (width == 0)
		{
			width = (size_t)Math::round(((float)height / svgImage->height) * svgImage->width);
		}
		else if (height == 0)
		{
			height = (size_t)Math::round(((float)width / svgImage->width) * svgImage->height);
		}

		if (!maxSize.empty() &&
			(width > maxSize.x() || height > maxSize.y()))
		{
			auto imageSize = Vector2i(width, height);
			auto displaySize = Vector2i((int)Math::round(maxSize.x()), (int)Math::round(maxSize.y()));

			Vector2i sz = ImageIO::adjustPictureSize(imageSize, displaySize, maxSize.externalZoom());

			if (sz.x() == displaySize.x())
			{
				width = sz.x();
				height = Math::round((width * svgImage->height) / svgImage->width);
			}
			else
			{
				height = sz.y();
				width = Math::round((height * svgImage->width) / svgImage->height);
			}
		}

		const int screenWidth = Renderer::getScreenWidth();
		const int screenHeight = Renderer::getScreenHeight();

		if (screenWidth > 0 && screenHeight > 0 &&
			(width > (size_t)screenWidth || height > (size_t)screenHeight))
		{
			const double ratio = std::min(
				(double)screenWidth / (double)width,
				(double)screenHeight / (double)height);

			width = std::max<size_t>(1, (size_t)Math::round((float)(width * ratio)));
			height = std::max<size_t>(1, (size_t)Math::round((float)(height * ratio)));
		}

		if (width == 0 || height == 0)
		{
			LOG(LogError) << "Error parsing SVG image size.";
			return false;
		}

		std::unique_ptr<unsigned char[]> dataRGBA(new unsigned char[width * height * 4]);

		double scale = ((float)((int)height)) / svgImage->height;
		double scaleV = ((float)((int)width)) / svgImage->width;
		if (scaleV < scale)
			scale = scaleV;

		std::unique_ptr<NSVGrasterizer, decltype(&nsvgDeleteRasterizer)> rast(
			nsvgCreateRasterizer(), nsvgDeleteRasterizer);
		if (!rast)
			return false;

		nsvgRasterize(rast.get(), svgImage.get(), 0, 0, scale,
			dataRGBA.get(), (int)width, (int)height, (int)width * 4);

		rast.reset();
		ImageIO::flipPixelsVert(dataRGBA.get(), width, height);

		std::unique_lock<std::mutex> lock(mMutex);
		if (mDataRGBA || mTextureID != 0)
			return true;

		if (mScalableMinimumSize.x() != scalableMinimumSize.x() ||
			mScalableMinimumSize.y() != scalableMinimumSize.y() ||
			mMaxSize.x() != maxSize.x() || mMaxSize.y() != maxSize.y() ||
			mMaxSize.externalZoom() != maxSize.externalZoom())
			continue;

		mScalableMinimumSize = Vector2f(sourceWidth, sourceHeight);
		mPhysicalSize = Vector2f(sourceWidth, sourceHeight);
		mSize = Vector2i(width, height);
		mDataRGBA = dataRGBA.release();
		return true;
	}
}

bool TextureData::initImageFromMemory(const unsigned char* fileData, size_t length, int subImageIndex)
{
	return initImage(fileData, length, subImageIndex, false, false);
}

bool TextureData::initImage(const unsigned char* fileData, size_t length, int subImageIndex, bool fileBacked, bool allowCacheWrite)
{
	if (!fileBacked && (fileData == nullptr || length == 0))
		return false;

	std::unique_lock<std::mutex> imageLoadLock(mImageLoadMutex);

	for (;;)
	{
		const int screenWidth = Renderer::getScreenWidth();
		const int screenHeight = Renderer::getScreenHeight();
		const bool optimizeVRAM = OPTIMIZEVRAM;
		const Vector2i screenSize(screenWidth, screenHeight);

		// Caller holds mMutex.
		auto getDecodeSize = [&]() -> MaxSizeInfo
		{
			return ImageIO::getTextureDecodeSize(mMaxSize, screenSize, optimizeVRAM);
		};

		MaxSizeInfo maxSize = MaxSizeInfo::Empty;

		{
			std::unique_lock<std::mutex> lock(mMutex);

			if (mDataRGBA || mTextureID != 0)
				return true;

			maxSize = getDecodeSize();
		}

		TextureDiskCache::Image image;
		uint64_t key = 0;
		bool hit = false;

		if (fileBacked && TextureDiskCache::enabled())
		{
			key = TextureDiskCache::makeKey(mPath, maxSize.x(), maxSize.y(), maxSize.externalZoom(), screenWidth, screenHeight);
			hit = TextureDiskCache::read(key, image);
		}

		if (!hit)
		{
			std::shared_ptr<unsigned char> source;
			const unsigned char* input = fileData;
			size_t inputLength = length;

			if (fileBacked)
			{
				const ResourceData data = ResourceManager::getInstance()->getFileData(mPath);
				source = data.ptr;
				input = source.get();
				inputLength = data.length;
			}

			if (!input || !inputLength)
				return false;

			size_t width = 0;
			size_t height = 0;
			Vector2i physicalSize = Vector2i::Zero();

			image.pixels.reset(ImageIO::loadFromMemoryRGBA32(input, inputLength, width, height, &maxSize, &physicalSize, nullptr, subImageIndex));

			if (!image.pixels)
			{
				LOG(LogError) << "Could not initialize texture from memory: " << mPath;
				return false;
			}

			image.width = static_cast<uint32_t>(width);
			image.height = static_cast<uint32_t>(height);
			image.originalWidth = static_cast<uint32_t>(physicalSize.x());
			image.originalHeight = static_cast<uint32_t>(physicalSize.y());

			source.reset();

			if (key && allowCacheWrite)
				TextureDiskCache::write(key, image);
		}

		std::unique_lock<std::mutex> lock(mMutex);

		if (mDataRGBA || mTextureID != 0)
			return true;

		const MaxSizeInfo currentSize = getDecodeSize();

		if (currentSize.x() != maxSize.x() || currentSize.y() != maxSize.y() ||
			currentSize.externalZoom() != maxSize.externalZoom() ||
			Renderer::getScreenWidth() != screenWidth || Renderer::getScreenHeight() != screenHeight)
		{
			continue;
		}

		mPhysicalSize = Vector2f(image.originalWidth, image.originalHeight);
		mSize = Vector2i(image.width, image.height);
		mScalable = false;
		mIsExternalDataRGBA = false;
		mDataRGBA = image.pixels.release();

		return true;
	}
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

	mSize = Vector2i(width, height);

	if (copyData)
		mPhysicalSize = Vector2f(mSize.x(), mSize.y());

	return true;
}

bool TextureData::updateFromExternalRGBA(unsigned char* dataRGBA, size_t width, size_t height)
{
	std::unique_lock<std::mutex> lock(mMutex);

	if (!mIsExternalDataRGBA && mDataRGBA != nullptr)
		delete[] mDataRGBA;

#if defined(USE_OPENGLES_30)
	const bool dimensionsChanged = mSize.x() != static_cast<int>(width) || mSize.y() != static_cast<int>(height);
#endif

	mIsExternalDataRGBA = true;
	mDataRGBA = dataRGBA;
	mSize = Vector2i(width, height);
	mPhysicalSize = Vector2f(width, height);

	if (mTextureID != 0)
	{
#if defined(USE_OPENGLES_30)
		if (dimensionsChanged || mMipmapped)
		{
			Renderer::destroyTexture(mTextureID);
			mTextureID = Renderer::createTexture(Renderer::Texture::RGBA, mLinear, mTile, width, height, mDataRGBA);
			mMipmapped = false;
		}
		else
#endif
			Renderer::updateTexture(mTextureID, Renderer::Texture::RGBA, 0, 0, width, height, mDataRGBA);
	}

	return true;
}

// Avoid multiple extraction in the same file at the same time
static Utils::StringListLockType mImageExtractorLock;

#if WIN32
extern void _checkUpgradedVlcVersion();
#endif

bool TextureData::loadFromVideo()
{
	Utils::StringListLock lock(mImageExtractorLock, mPath);

	auto val = Utils::FileSystem::createRelativePath(Utils::FileSystem::changeExtension(mPath, ".jpg"), Paths::getHomePath(), true);
	val = Utils::String::replace(val, "~/../", "./");

	std::string localFile = Utils::FileSystem::resolveRelativePath(val, Paths::getUserEmulationStationPath() + "/tmp/videothumbs/", true);

	if (Utils::FileSystem::exists(localFile))
	{
		auto date = Utils::FileSystem::getFileCreationDate(localFile);
		auto duration = Utils::Time::DateTime::now().elapsedSecondsSince(date);
		if (duration > 62 * 86400) // 2 months
			Utils::FileSystem::removeFile(localFile);
	}

	if (!Utils::FileSystem::exists(localFile))
	{
		Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(localFile));

		libvlc_instance_t *vlcInstance = nullptr;
		libvlc_media_t *vlcMedia = nullptr;
		libvlc_media_player_t *vlcMediaPlayer = nullptr;

		std::vector<std::string> cmdline;
		cmdline.push_back("--quiet");
		cmdline.push_back("--rate=1");
		cmdline.push_back("--video-filter=scene");
		cmdline.push_back("--intf=dummy");
		cmdline.push_back("--vout=dummy");
		cmdline.push_back("--scene-format=jpeg");
		cmdline.push_back("--scene-ratio=1");
		cmdline.push_back("--no-video-title-show");

		const char** vlcArgs = new const char*[cmdline.size()];

		for (int i = 0; i < cmdline.size(); i++)
			vlcArgs[i] = cmdline[i].c_str();

#if WIN32
		_checkUpgradedVlcVersion();
#endif

		vlcInstance = libvlc_new(cmdline.size(), vlcArgs);
		if (vlcInstance == nullptr)
			return false;

		vlcMedia = libvlc_media_new_path(vlcInstance, Utils::FileSystem::getPreferredPath(mPath).c_str());
		if (vlcMedia == nullptr)
		{
			libvlc_release(vlcInstance);
			return false;
		}

		libvlc_media_add_option(vlcMedia, ":no-audio");
		libvlc_media_add_option(vlcMedia, ":start-time=1.5");

		vlcMediaPlayer = libvlc_media_player_new_from_media(vlcMedia);
		if (vlcMediaPlayer == nullptr)
		{
			libvlc_media_release(vlcMedia);
			libvlc_release(vlcInstance);
			return false;
		}

		int ms = 1500;

		libvlc_media_player_set_rate(vlcMediaPlayer, 1);
		libvlc_audio_set_mute(vlcMediaPlayer, 1);
		libvlc_media_player_play(vlcMediaPlayer);
		libvlc_media_player_set_time(vlcMediaPlayer, ms);

		auto time = libvlc_media_player_get_time(vlcMediaPlayer);
		int n = 100; // avoid infinite loop
		while (time <= ms && n > 0) {
			time = libvlc_media_player_get_time(vlcMediaPlayer);
			n--;
		}

		int result = libvlc_video_take_snapshot(vlcMediaPlayer, 0, localFile.c_str(), 0, 0);

		libvlc_media_player_stop(vlcMediaPlayer);
		libvlc_media_player_release(vlcMediaPlayer);
		libvlc_media_release(vlcMedia);
		libvlc_release(vlcInstance);
	}

	if (Utils::FileSystem::exists(localFile))
	{
		std::shared_ptr<ResourceManager>& rm = ResourceManager::getInstance();
		const ResourceData& data = rm->getFileData(localFile);
		return initImageFromMemory((const unsigned char*)data.ptr.get(), data.length);
	}

	return false;
}

bool TextureData::loadFromPdf(int pageIndex)
{
	if (PdfHandler == nullptr)
		return false;
	
	bool retval = false;

	Utils::StringListLock lock(mImageExtractorLock, mPath);
	
	int dpi = 48;

	if (OPTIMIZEVRAM && !mMaxSize.empty())
		dpi = (int)Math::clamp(mMaxSize.y() / 6, 32, 300);

	auto files = PdfHandler->extractPdfImages(mPath, pageIndex, pageIndex, dpi);
	if (files.size() > 0)
	{
		std::shared_ptr<ResourceManager>& rm = ResourceManager::getInstance();
		const ResourceData& data = rm->getFileData(files[0]);
		Utils::FileSystem::removeFile(files[0]);

		initImageFromMemory((const unsigned char*)data.ptr.get(), data.length);
	}

	return retval;
}

bool TextureData::loadFromCbz()
{
	bool retval = false;

	Utils::StringListLock lock(mImageExtractorLock, mPath);

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

		Utils::Zip::zip_callback func = [](void *pOpaque, uint64_t ofs, const void *pBuf, size_t n)
		{
			unsigned char* pSource = (unsigned char*)pBuf;
			unsigned char* pDest = (unsigned char*)pOpaque;

			memcpy(pDest + ofs, pSource, n);

			return n;
		};

		zipFile.readBuffered(files[0].filename, func, buffer);		
		retval = initImageFromMemory(buffer, size);
	}

	return retval;
}

bool TextureData::load(bool allowCacheWrite)
{
	// Need to load. See if there is a file
	if (mPath.empty())
		return false;

	LOG(LogDebug) << "TextureData::load " << mPath;

	std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(mPath));

	setScalable(ext == ".svg");

	if (ext == ".cbz")
		return loadFromCbz();

	if (ext == ".pdf")
		return PdfHandler != nullptr ? loadFromPdf() : false;

	if (Utils::FileSystem::isVideo(mPath))
		return loadFromVideo();

	std::string path = mPath;
	int subImageIndex = -1;

	if (ext.find(',') != std::string::npos)
	{
		auto idx = path.rfind(',');
		subImageIndex = Utils::String::toInteger(mPath.substr(idx + 1));
		path = mPath.substr(0, idx);
	}

	if (ext != ".svg" && subImageIndex == -1 && !path.empty() && path[0] == '/')
		return initImage(nullptr, 0, -1, true, allowCacheWrite);

	const ResourceData& data = ResourceManager::getInstance()->getFileData(path);
	if (data.length == 0)
		return false;

	// is it an SVG?
	if (ext == ".svg")
	{
		return initSVGFromMemory((const unsigned char*)data.ptr.get(), data.length);
	}

	return initImageFromMemory((const unsigned char*)data.ptr.get(), data.length, subImageIndex);
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
		if (mSize.empty() || mDataRGBA == nullptr)
		{
			Renderer::bindTexture(mTextureID);
			return false;
		}

#if defined(USE_OPENGLES_30)
		// Mipmaps reduce 4K artwork minification cost and aliasing, but add about 33%
		// VRAM. Restrict them to stable, path-backed linear artwork that will not be
		// updated every frame (video/external pixels, fonts, and render targets never enter here).
		const bool useMipmaps = mReloadable && !mIsExternalDataRGBA && mLinear && !mTile &&
			!Utils::FileSystem::isVideo(mPath) && (mSize.x() >= 256 || mSize.y() >= 256);
		mTextureID = useMipmaps
			? Renderer::createMipmappedTexture(Renderer::Texture::RGBA, mLinear, mTile, mSize.x(), mSize.y(), mDataRGBA)
			: Renderer::createTexture(Renderer::Texture::RGBA, mLinear, mTile, mSize.x(), mSize.y(), mDataRGBA);
#else
		// Legacy renderers retain their original single-level texture path.
		mTextureID = Renderer::createTexture(Renderer::Texture::RGBA, mLinear, mTile, mSize.x(), mSize.y(), mDataRGBA);
#endif
		if (mTextureID == 0)
			return false;
#if defined(USE_OPENGLES_30)
		mMipmapped = useMipmaps;
#endif

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
#if defined(USE_OPENGLES_30)
		mMipmapped = false;
#endif
	}
}

void TextureData::releaseRAM()
{
	std::unique_lock<std::mutex> lock(mMutex);

	if (mDataRGBA != nullptr && !mIsExternalDataRGBA)
		delete[] mDataRGBA;

	mDataRGBA = 0;
}

void TextureData::setMaxSize(const MaxSizeInfo& maxSize)
{
	// SVGs need the display size even when optimizevram is off.
	std::unique_lock<std::mutex> lock(mMutex);

	if (mMaxSize.x() == maxSize.x() &&
		mMaxSize.y() == maxSize.y() &&
		mMaxSize.externalZoom() == maxSize.externalZoom() &&
		mMaxSize.isExternalZoomKnown() == maxSize.isExternalZoomKnown())
	{
		return;
	}

	if (mPhysicalSize.empty())
		mMaxSize = maxSize;
	else
	{
		auto imageSize = Vector2i((int)Math::round(mPhysicalSize.x()), Math::round(mPhysicalSize.y()));

		Vector2i value = ImageIO::adjustPictureSize(imageSize, Vector2i(mMaxSize.x(), mMaxSize.y()), mMaxSize.externalZoom());
		Vector2i newVal = ImageIO::adjustPictureSize(imageSize, Vector2i(maxSize.x(), maxSize.y()), maxSize.externalZoom());

		if (newVal.x() > value.x() || newVal.y() > value.y())
			mMaxSize = maxSize;
	}
}

bool TextureData::isMaxSizeValid()
{
	if (!OPTIMIZEVRAM)
		return true;

	std::unique_lock<std::mutex> lock(mMutex);

	if (mSize.empty() || mPhysicalSize.empty())
		return true;

	if (mMaxSize.empty())
		return true;

	if (!mScalable)
	{
		const Vector2i screenSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());

		const MaxSizeInfo decodeSize = ImageIO::getTextureDecodeSize(mMaxSize, screenSize, true);

		const Vector2i sourceSize((int)Math::round(mPhysicalSize.x()), (int)Math::round(mPhysicalSize.y()));

		const Vector2i expectedSize = ImageIO::getRasterOutputSize(sourceSize, &decodeSize, screenSize);

		return mSize.x() >= expectedSize.x() && mSize.y() >= expectedSize.y();
	}

	if ((int) Math::round(mPhysicalSize.x()) == mSize.x() && (int)Math::round(mPhysicalSize.y()) == mSize.y())
		return true;

	if ((int)mMaxSize.x() <= mSize.x() || (int)mMaxSize.y() <= mSize.y())
		return true;

	if (mPhysicalSize.x() <= mSize.x() || mPhysicalSize.y() <= mSize.y())
		return true;

	return false;
}

bool TextureData::rasterizeAt(float width, float height)
{
	std::unique_lock<std::mutex> lock(mMutex);

	if (mSize.empty())
		return false;

	int h = (int)Math::round(height);
	int w = (int)Math::round(width);

	if (mScalable)
	{
		if ((int)Math::round(mScalableMinimumSize.y()) < h && (int)Math::round(mScalableMinimumSize.x()) != w)
		{
			LOG(LogDebug) << "Requested SVG image size too small. Reloading image from (" << mScalableMinimumSize.x() << ", " << mScalableMinimumSize.y() << ") to (" << width << ", " << height << ")";

			mScalableMinimumSize.x() = width;
			mScalableMinimumSize.y() = height;

			if (!mDataRGBA && mTextureID == 0)
				return true;

			lock.unlock();
			releaseVRAM();
			releaseRAM();
			load();

			return isLoaded();
		}
	}
	else if (!mMaxSize.empty() && h < Renderer::getScreenHeight() && w < Renderer::getScreenWidth() && OPTIMIZEVRAM)
	{
		if (mSize.y() < h && mSize.x() < w && (int)Math::round(mPhysicalSize.y()) >= h && (int)Math::round(mPhysicalSize.x()) > w)
		{
			LOG(LogDebug) << "Requested PNG/JPG image size too small. Reloading image from (" << mSize.x() << ", " << mSize.y() << ") to (" << width << ", " << height << ")";

			mMaxSize = MaxSizeInfo(w, h, mMaxSize.externalZoom());

			if (!mDataRGBA && mTextureID == 0)
				return true;

			lock.unlock();
			releaseVRAM();
			releaseRAM();
			load();

			return isLoaded();
		}
	}

	return false;
}
