#include "resources/TextureDataManager.h"

#include "resources/TextureData.h"
#include "resources/TextureResource.h"
#include "Settings.h"
#include "Log.h"
#include "utils/Platform.h"
#include <algorithm>
#include <SDL.h>

TextureDataManager::TextureDataManager()
{
	mLoader = new TextureLoader(this);
}

TextureDataManager::~TextureDataManager()
{
	delete mLoader;
}

std::shared_ptr<TextureData> TextureDataManager::add(const TextureResource* key, bool tiled, bool linear)
{
	std::unique_lock<std::recursive_mutex> lock(mMutex);

	// Find the entry in the list
	auto it = mTextureLookup.find(key);
	if (it != mTextureLookup.cend())
	{
		mLoader->remove(*(*it).second);
		// Remove the list entry
		mTextures.erase((*it).second);
		// And the lookup
		mTextureLookup.erase(it);
	}

	std::shared_ptr<TextureData> data = std::make_shared<TextureData>(tiled, linear);
	mTextures.push_front(data);
	mTextureLookup[key] = mTextures.cbegin();

	return data;
}

void TextureDataManager::remove(const TextureResource* key)
{
	std::unique_lock<std::recursive_mutex> lock(mMutex);

	// Find the entry in the list
	auto it = mTextureLookup.find(key);
	if (it != mTextureLookup.cend())
	{
		mLoader->remove(*(*it).second);
		// Remove the list entry
		mTextures.erase((*it).second);
		// And the lookup
		mTextureLookup.erase(it);
	}
}

void TextureDataManager::cancelAsync(const TextureResource* key)
{
	std::unique_lock<std::recursive_mutex> lock(mMutex);

	auto it = mTextureLookup.find(key);
	if (it != mTextureLookup.cend())
		mLoader->remove(*(*it).second);
}

std::shared_ptr<TextureData> TextureDataManager::get(const TextureResource* key, TextureLoadMode enableLoading)
{
	std::unique_lock<std::recursive_mutex> lock(mMutex);

	// If it's in the cache then we want to remove it from it's current location and
	// move it to the top
	std::shared_ptr<TextureData> tex;
	auto it = mTextureLookup.find(key);
	if (it != mTextureLookup.cend())
	{
		tex = *(*it).second;

		if (enableLoading == TextureLoadMode::NOLOAD)
			return tex;

		if (enableLoading != TextureLoadMode::LOADNOMOVETOTOP && mTextures.cbegin() != it->second)
		{
			// splice keeps the stored iterator valid.
			mTextures.splice(mTextures.cbegin(), mTextures, it->second);
		}

		// Make sure it's loaded or queued for loading
		if (enableLoading != TextureLoadMode::MOVETOTOPONLY && (enableLoading == TextureLoadMode::BLOCKING || !tex->isLoaded()))
		{
			load(tex, enableLoading == TextureLoadMode::BLOCKING, enableLoading != TextureLoadMode::LOADNOMOVETOTOP);
		}
	}

	return tex;
}

bool TextureDataManager::bind(const TextureResource* key)
{
	std::shared_ptr<TextureData> tex = get(key);
	bool bound = false;
	if (tex != nullptr)
		bound = tex->uploadAndBind();
	if (!bound)
		getBlankTexture()->uploadAndBind();
	return bound;
}

size_t TextureDataManager::getTotalMemoryUsage(MemoryUsageType type)
{
	std::unique_lock<std::recursive_mutex> lock(mMutex);

	size_t total = 0;

	for (const auto& tex : mTextures)
		total += tex->getMemoryUsage(type);

	return total;
}

bool compareTextures(const std::shared_ptr<TextureData>& first, const std::shared_ptr<TextureData>& second)
{
	bool isResource = first->getPath().rfind(":/") == 0;
	bool secondIsResource = second->getPath().rfind(":/") == 0;
	if (isResource && !secondIsResource)
		return true;

	return (second->isRequired() && !first->isRequired());
}

void TextureDataManager::cleanupVRAM()
{
	std::unique_lock<std::recursive_mutex> lock(mMutex);

	size_t maxVRAM = (size_t)Settings::getInstance()->getInt("MaxVRAM") * 1024 * 1024;
	size_t size = TextureResource::getTotalMemoryUsage(MemoryUsageType::Allocated);

	if (size < maxVRAM)
		return;

	// Perform cleanup on textures
	for (auto it = mTextures.crbegin(); it != mTextures.crend(); ++it)
	{
		auto tex = *it;
		if (!tex->isReloadable() || tex->isRequired() || !tex->isLoaded())
			continue;

		auto textureSize = tex->getMemoryUsage(MemoryUsageType::Allocated); // pass == 0 ? MemoryUsageType::RAM : MemoryUsageType::VRAM);
		if (textureSize == 0)
			continue;

		LOG(LogDebug) << "Cleanup VRAM\tReleased : " << tex->getPath().c_str() << ", " << std::to_string(tex->getSize().x()) << "x" << std::to_string(tex->getSize().y());

		tex->releaseVRAM();
		tex->releaseRAM();

		size -= textureSize;
		if (size <= maxVRAM)
			break;
	}

	// On x86 platforms, perform cleanup on textures stored in RAM, limit to 250Mb
	if (sizeof(void*) == 4)
	{
		size_t maxRAM = 250 * 1024 * 1024;
		size = TextureResource::getTotalMemoryUsage(MemoryUsageType::RAM);

		if (size > maxRAM)
		{
			for (auto it = mTextures.crbegin(); it != mTextures.crend(); ++it)
			{
				auto tex = *it;
				if (!tex->isReloadable() || tex->isRequired() || !tex->isLoaded())
					continue;

				auto textureSize = tex->getMemoryUsage(MemoryUsageType::RAM);
				if (textureSize == 0)
					continue;

				LOG(LogDebug) << "Cleanup RAM\tReleased : " << tex->getPath().c_str() << ", " << std::to_string(tex->getSize().x()) << "x" << std::to_string(tex->getSize().y());

				tex->releaseRAM();

				size -= textureSize;
				if (size <= maxRAM)
					break;
			}
		}
	}
}

void TextureDataManager::load(std::shared_ptr<TextureData> tex, bool block, bool prioritize)
{
	std::unique_lock<std::recursive_mutex> lock(mMutex);
	if (mSuspended)
		return;

	if (block)
	{
		mLoader->remove(tex);
		mLoader->waitFor(tex);
	}

	if (tex->isLoaded() && !tex->isMaxSizeValid() && tex->getMemoryUsage(MemoryUsageType::Allocated) > 0)
	{
		mLoader->remove(tex);
		mLoader->waitFor(tex);
		tex->releaseVRAM();
		tex->releaseRAM();
		block = true;
	}

	if (!block)
		mLoader->load(tex, prioritize);
	else if (!tex->isLoaded())
		tex->load();
}

TextureLoader::TextureLoader(TextureDataManager* mgr) : mManager(mgr), mExit(false)
{
	int num_threads = std::thread::hardware_concurrency() / 2;
	if (num_threads < 2)
		num_threads = 2;

	for (size_t i = 0; i < num_threads; i++)
		mThreads.push_back(std::thread(&TextureLoader::threadProc, this));
}

TextureLoader::~TextureLoader()
{
	{
		std::unique_lock<std::mutex> lock(mLoaderLock);
		mExit = true;
		mQueuedTextures.clear();
		mTextureDataQ.clear();
	}

	mEvent.notify_all();

	for (std::thread& t : mThreads)
		t.join();
}

#if WIN32
#include <Windows.h>
#endif

void TextureLoader::threadProc()
{
#if WIN32
	if (Utils::Platform::isWindows10())
		SetThreadDescription(GetCurrentThread(), L"TextureLoader::threadProc");
#endif

	while (true)
	{
		// Wait for an event to say there is something in the queue
		std::unique_lock<std::mutex> lock(mLoaderLock);
		mEvent.wait(lock, [this]() {
			return mExit || (!mSuspended && !mPaused && !mTextureDataQ.empty());
		});

		if (mExit)
			break;

		if (!mTextureDataQ.empty())
		{
			std::shared_ptr<TextureData> textureData = mTextureDataQ.front();

			mQueuedTextures.erase(textureData);
			mTextureDataQ.pop_front();

			if (textureData && !textureData->isLoaded())
			{
				mProcessingTextureDataQ.insert(textureData);

				lock.unlock();

				try { textureData->load(true); }
				catch (...) { }

				lock.lock();

				mProcessingTextureDataQ.erase(textureData);
				// Drop the worker's reference before waking cleanup.
				textureData.reset();
				mIdle.notify_all();
			}

			lock.unlock();
		}
	}
}

void TextureLoader::setPaused(bool paused)
{
	{
		std::lock_guard<std::mutex> lock(mLoaderLock);
		mPaused = paused;
	}

	if (!paused)
		mEvent.notify_all();
}

void TextureLoader::load(std::shared_ptr<TextureData> textureData, bool prioritize)
{
	std::unique_lock<std::mutex> lock(mLoaderLock);

	if (mSuspended || mExit || textureData->isLoaded())
		return;

	if (mProcessingTextureDataQ.count(textureData) != 0)
		return;

	auto queued = mQueuedTextures.find(textureData);
	if (queued != mQueuedTextures.end())
	{
		if (prioritize && queued->second != mTextureDataQ.begin())
		{
			mTextureDataQ.splice(mTextureDataQ.begin(), mTextureDataQ, queued->second);
		}

		return;
	}

	auto position = mTextureDataQ.insert(prioritize ? mTextureDataQ.begin() : mTextureDataQ.end(), textureData);

	try
	{
		mQueuedTextures.emplace(textureData, position);
	}
	catch (...)
	{
		mTextureDataQ.erase(position);
		throw;
	}

	mEvent.notify_one();
}

bool TextureLoader::remove(std::shared_ptr<TextureData> textureData)
{
	std::unique_lock<std::mutex> lock(mLoaderLock);

	auto queued = mQueuedTextures.find(textureData);
	if (queued == mQueuedTextures.end())
		return false;

	mTextureDataQ.erase(queued->second);
	mQueuedTextures.erase(queued);
	return true;
}

void TextureLoader::suspend()
{
	std::unique_lock<std::mutex> lock(mLoaderLock);

	mSuspended = true;
	mQueuedTextures.clear();
	mTextureDataQ.clear();
	mIdle.wait(lock, [this]() { return mProcessingTextureDataQ.empty(); });
}

void TextureLoader::waitFor(const std::shared_ptr<TextureData>& textureData)
{
	std::unique_lock<std::mutex> lock(mLoaderLock);
	mIdle.wait(lock, [this, &textureData]() {
		return mProcessingTextureDataQ.count(textureData) == 0;
	});
}

void TextureLoader::resume()
{
	std::unique_lock<std::mutex> lock(mLoaderLock);
	mSuspended = false;
	mEvent.notify_all();
}

int TextureLoader::getQueueSize()
{
	std::unique_lock<std::mutex> lock(mLoaderLock);
	return static_cast<int>(mTextureDataQ.size() + mProcessingTextureDataQ.size());
}

bool TextureLoader::isLoadingIdle()
{
	std::unique_lock<std::mutex> lock(mLoaderLock, std::try_to_lock);

	return lock.owns_lock() && mTextureDataQ.empty() && mProcessingTextureDataQ.empty();
}

void TextureDataManager::setLoadingPaused(bool paused)
{
	mLoader->setPaused(paused);
}

void TextureDataManager::suspend()
{
	{
		std::unique_lock<std::recursive_mutex> lock(mMutex);
		mSuspended = true;
	}

	// Workers may still hold textures that left the cache.
	mLoader->suspend();
	mBlank.reset();
}

void TextureDataManager::resume()
{
	std::unique_lock<std::recursive_mutex> lock(mMutex);
	mLoader->resume();
	mSuspended = false;
}

int TextureDataManager::getQueueSize()
{
	return mLoader ? mLoader->getQueueSize() : 0;
}

bool TextureDataManager::isLoadingIdle()
{
	return mLoader && mLoader->isLoadingIdle();
}

std::shared_ptr<TextureData> TextureDataManager::getBlankTexture()
{
	if (mBlank == nullptr)
	{
		mBlank = std::make_shared<TextureData>(false, false);

		int size = 8;
		unsigned char* data = new unsigned char[size * size * 4];
		for (int i = 0; i < size * size; ++i)
		{
			data[i * 4] = 0;
			data[i * 4 + 1] = 0;
			data[i * 4 + 2] = 0;
			data[i * 4 + 3] = 0;
		}

		mBlank->initFromRGBA(data, size, size);
	}

	return mBlank;
}
