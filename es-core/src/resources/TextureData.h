#pragma once
#ifndef ES_CORE_RESOURCES_TEXTURE_DATA_H
#define ES_CORE_RESOURCES_TEXTURE_DATA_H

#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>
#include "ImageIO.h"
#include "TextureDataManager.h"

class TextureResource;

class IPdfHandler
{
public:
	virtual int getPdfPageCount(const std::string& fileName) = 0;
	virtual std::vector<std::string> extractPdfImages(const std::string& fileName, int pageIndex = -1, int pageCount = 1, int quality = 0) = 0;
};

class TextureData
{
public:
	static IPdfHandler* PdfHandler;

	TextureData(bool tile, bool linear);
	~TextureData();

	// These functions populate mDataRGBA but do not upload the texture to VRAM

	//!!!! Needs to be canonical path. Caller should check for duplicates before calling this
	void initFromPath(const std::string& path);
	bool initSVGFromMemory(const unsigned char* fileData, size_t length);
	bool initImageFromMemory(const unsigned char* fileData, size_t length, int subImageIndex = -1);
	bool initFromRGBA(unsigned char* dataRGBA, size_t width, size_t height, bool copyData = true);

	// Read the data into memory if necessary
	bool load(bool allowCacheWrite = false);
	bool loadFromCbz();
	bool loadFromPdf(int pageIndex = 1);
	bool loadFromVideo();

	bool isLoaded();

	// Upload the texture to VRAM if necessary and bind. Returns true if bound ok or
	// false if either not loaded
	bool uploadAndBind();

	// Release the texture from VRAM
	void releaseVRAM();

	// Release the texture from conventional RAM
	void releaseRAM();

	// Get the amount of memory used by this texture.
#if defined(USE_OPENGLES_30)
	size_t getMemoryUsage(MemoryUsageType type = MemoryUsageType::Allocated);
#else
	size_t getMemoryUsage(MemoryUsageType type = MemoryUsageType::Allocated)
	{
		std::unique_lock<std::mutex> lock(mMutex);

		if (type == MemoryUsageType::RAM)
			return mDataRGBA != nullptr ? mSize.x() * mSize.y() * 4 : 0;

		if (type == MemoryUsageType::VRAM)
			return mTextureID != 0 ? mSize.x() * mSize.y() * 4 : 0;

		if (type == MemoryUsageType::Estimated)
			return mSize.x() * mSize.y() * 4;

		return mTextureID != 0 || mDataRGBA != nullptr ? mSize.x() * mSize.y() * 4 : 0;
	}
#endif

	Vector2i getSize() const
	{
		std::unique_lock<std::mutex> lock(mMutex);
		return mSize;
	}

	Vector2f getPhysicalSize() const
	{
		std::unique_lock<std::mutex> lock(mMutex);
		return mPhysicalSize;
	}

	bool rasterizeAt(float width, float height);

	bool tiled() { return mTile; }

	unsigned char* getDataRGBA() { return mDataRGBA; }

	void setMaxSize(const MaxSizeInfo& maxSize);
	bool isMaxSizeValid();

	inline const std::string& getPath() { return mPath; };

	bool updateFromExternalRGBA(unsigned char* dataRGBA, size_t width, size_t height);

	bool isRequired() { return mRequired || !mRequiredOwners.empty(); }

	void setRequired(bool value, const void* owner = nullptr)
	{
		if (owner == nullptr)
			mRequired = value;
		else if (value)
			mRequiredOwners.insert(owner);
		else
			mRequiredOwners.erase(owner);
	}

	inline bool isDynamic() { return mDynamic; };
	void setDynamic(bool value) { mDynamic = value; };

	inline bool isReloadable() { return mReloadable; };
	
	bool isScalable()
	{
		std::unique_lock<std::mutex> lock(mMutex);
		return mScalable;
	}

	void setScalable(bool value)
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mScalable = value;
	}

private:
	bool	initImage(const unsigned char* fileData, size_t length, int subImageIndex, bool fileBacked, bool allowCacheWrite);
	bool			mRequired;
	std::unordered_set<const void*>	mRequiredOwners;

	mutable std::mutex	mMutex;
	std::mutex		mImageLoadMutex;
	bool			mTile;
	bool			mLinear;
	std::string		mPath;
	unsigned int	mTextureID;
	unsigned char*	mDataRGBA;
	bool			mReloadable;
	bool			mDynamic;
#if defined(USE_OPENGLES_30)
	bool			mMipmapped;
#endif

	MaxSizeInfo		mMaxSize;

	Vector2i		mSize;
	Vector2f		mPhysicalSize;

	bool			mScalable;
	Vector2f		mScalableMinimumSize;

	bool			mIsExternalDataRGBA;
};

#endif // ES_CORE_RESOURCES_TEXTURE_DATA_H
