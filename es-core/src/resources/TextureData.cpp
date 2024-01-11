#include "resources/TextureData.h"

#include "math/Misc.h"
#include "renderers/Renderer.h"
#include "resources/ResourceManager.h"
#include "ImageIO.h"
#include "Log.h"
#include <nanosvg/nanosvg.h>
#include <nanosvg/nanosvgrast.h>
#include <string.h>
#include <algorithm>
#include <vlc/vlc.h>

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

	float sourceWidth = svgImage->width;
	float sourceHeight = svgImage->height;

	if (mScalableMinimumSize.empty())
	{
		sourceWidth = svgImage->width;
		sourceHeight = svgImage->height;
		
		if (!mMaxSize.empty() && sourceWidth < mMaxSize.x() && sourceHeight < mMaxSize.y())
		{
			auto sz = ImageIO::adjustPictureSizeF(sourceWidth, sourceHeight, mMaxSize.x(), mMaxSize.y(), mMaxSize.externalZoom());
			sourceHeight = sz.y();
			sourceWidth = (sourceHeight * svgImage->width) / svgImage->height; // FCA : Always compute width using source aspect ratio
		}
	}
	else
	{
		sourceHeight = mScalableMinimumSize.y();
		sourceWidth = (sourceHeight * svgImage->width) / svgImage->height; // FCA : Always compute width using source aspect ratio
	}

	mScalableMinimumSize = Vector2f(sourceWidth, sourceHeight);
	mPhysicalSize = Vector2f(sourceWidth, sourceHeight);

	size_t width = (size_t)Math::round(sourceWidth);
	size_t height = (size_t)Math::round(sourceHeight);

	if (width == 0)
	{
		// auto scale width to keep aspect
		width = (size_t)Math::round(((float)height / svgImage->height) * svgImage->width);
	}
	else if (height == 0)
	{
		// auto scale height to keep aspect
		height = (size_t)Math::round(((float)width / svgImage->width) * svgImage->height);
	}


	if (OPTIMIZEVRAM && !mMaxSize.empty() && (width > mMaxSize.x() || height > mMaxSize.y()))
	{
		auto imageSize = Vector2i(width, height);
		auto displaySize = Vector2i((int)Math::round(mMaxSize.x()), (int)Math::round(mMaxSize.y()));

		Vector2i sz = ImageIO::adjustPictureSize(imageSize, displaySize, mMaxSize.externalZoom());
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
	
	mSize = Vector2i(width, height);

	if (width * height <= 0)
	{
		LOG(LogError) << "Error parsing SVG image size.";
		return false;
	}

	unsigned char* dataRGBA = new unsigned char[width * height * 4];

	double scale = ((float)((int)height)) / svgImage->height;
	double scaleV = ((float)((int)width)) / svgImage->width;
	if (scaleV < scale)
		scale = scaleV;

	NSVGrasterizer* rast = nsvgCreateRasterizer();
	nsvgRasterize(rast, svgImage, 0, 0, scale, dataRGBA, (int)width, (int)height, (int)width * 4);
	nsvgDeleteRasterizer(rast);
	nsvgDelete(svgImage);

	ImageIO::flipPixelsVert(dataRGBA, width, height);

	mDataRGBA = dataRGBA;

	return true;
}

bool TextureData::initImageFromMemory(const unsigned char* fileData, size_t length, int subImageIndex)
{
	// If already initialised then don't read again
	if (isLoaded())
		return true;

	// Don't load images greater than screen resolution
	MaxSizeInfo maxSize(Renderer::getScreenWidth(), Renderer::getScreenHeight(), false);
	if (!mMaxSize.empty() && mMaxSize.x() < maxSize.x() && mMaxSize.y() < maxSize.y())
		maxSize = mMaxSize;
		
	auto oldSize = mSize;

	size_t width, height;
	Vector2i size;
	Vector2i physicalSize;
	unsigned char* imageRGBA = ImageIO::loadFromMemoryRGBA32((const unsigned char*)(fileData), length, width, height, &maxSize, &physicalSize, &size, subImageIndex);
	
	mPhysicalSize = Vector2f(physicalSize.x(), physicalSize.y());
	mSize = size.empty() ? physicalSize : size;
	mScalable = false;

	if (imageRGBA == nullptr)
	{
		LOG(LogError) << "Could not initialize texture from memory, invalid data!  (file path: " << mPath << ", data ptr: " << (size_t)fileData << ", reported size: " << length << ")";
		return false;
	}

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

	mSize = Vector2i(width, height);

	if (copyData)
		mPhysicalSize = Vector2f(mSize.x(), mSize.y());

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

	mSize = Vector2i(width, height);
	mPhysicalSize = Vector2f(width, height);

	if (mTextureID != 0)
		Renderer::updateTexture(mTextureID, Renderer::Texture::RGBA, 0, 0, width, height, mDataRGBA);

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
		while (time <= ms)
			time = libvlc_media_player_get_time(vlcMediaPlayer);

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

		if (initImageFromMemory((const unsigned char*)data.ptr.get(), data.length))
		{
			ImageIO::updateImageCache(mPath, Utils::FileSystem::getFileSize(mPath), Math::round((int)mPhysicalSize.x()), Math::round((int)mPhysicalSize.y()));
			return true;
		}
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

	if (!mMaxSize.empty())
		dpi = (int) Math::clamp(mMaxSize.y() / 6, 32, 300);

	auto files = PdfHandler->extractPdfImages(mPath, pageIndex, pageIndex, dpi);
	if (files.size() > 0)
	{
		std::shared_ptr<ResourceManager>& rm = ResourceManager::getInstance();
		const ResourceData& data = rm->getFileData(files[0]);
		Utils::FileSystem::removeFile(files[0]);

		retval = initImageFromMemory((const unsigned char*)data.ptr.get(), data.length);

		if (retval)
			ImageIO::updateImageCache(mPath, Utils::FileSystem::getFileSize(mPath), Math::round((int)mPhysicalSize.x()), Math::round((int)mPhysicalSize.y()));
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
			ImageIO::updateImageCache(mPath, Utils::FileSystem::getFileSize(mPath), Math::round((int)mPhysicalSize.x()), Math::round((int)mPhysicalSize.y()));
	}

	return retval;
}

bool TextureData::load(bool updateCache)
{
	// Need to load. See if there is a file
	if (mPath.empty())
		return false;

	LOG(LogDebug) << "TextureData::load " << mPath;
	mScalable = false;

	std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(mPath));

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

	const ResourceData& data = ResourceManager::getInstance()->getFileData(path);

	// is it an SVG?
	if (ext == ".svg")
	{
		mScalable = true;
		return initSVGFromMemory((const unsigned char*)data.ptr.get(), data.length);
	}

	bool retval = initImageFromMemory((const unsigned char*)data.ptr.get(), data.length, subImageIndex);

	if (updateCache && retval)
		ImageIO::updateImageCache(mPath, data.length, Math::round((int)mPhysicalSize.x()), Math::round((int)mPhysicalSize.y()));

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
		if (mSize.empty() || mDataRGBA == nullptr)
		{
			Renderer::bindTexture(mTextureID);
			return false;
		}

		// Upload texture
		mTextureID = Renderer::createTexture(Renderer::Texture::RGBA, mLinear, mTile, mSize.x(), mSize.y(), mDataRGBA);
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

void TextureData::setStoredSize(float width, float height)
{
	mSize = Vector2i(width, height);
}

void TextureData::setMaxSize(const MaxSizeInfo& maxSize)
{
	if (!OPTIMIZEVRAM)
		return;

	if (mPhysicalSize.empty())
		mMaxSize = maxSize;
	else
	{
		auto imageSize = Vector2i((int)Math::round(mPhysicalSize.x()), Math::round(mPhysicalSize.y()));

		Vector2i value = ImageIO::adjustPictureSize(imageSize, Vector2i(mMaxSize.x(), mMaxSize.y()), mMaxSize.externalZoom());
		Vector2i newVal = ImageIO::adjustPictureSize(imageSize, Vector2i(maxSize.x(), maxSize.y()), mMaxSize.externalZoom());

		if (newVal.x() > value.x() || newVal.y() > value.y())
			mMaxSize = maxSize;
	}
}

bool TextureData::isMaxSizeValid()
{
	if (!OPTIMIZEVRAM)
		return true;

	if (mSize.empty() || mPhysicalSize.empty())
		return true;

	if (mMaxSize.empty())
		return true;

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

			if (!isLoaded())
				return true;

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

			if (!isLoaded())
				return true;

			releaseVRAM();
			releaseRAM();
			load();

			return isLoaded();
		}
	}

	return false;
}
