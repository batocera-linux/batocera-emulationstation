#include "resources/TextureDataManager.h"

#include "resources/TextureData.h"
#include "resources/TextureResource.h"
#include "Settings.h"
#include "Log.h"
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

		if (mTextures.cbegin() != (*it).second)
		{
			// Remove the list entry
			mTextures.erase((*it).second);
			// Put it at the top
			mTextures.push_front(tex);
			// Store it back in the lookup
			mTextureLookup[key] = mTextures.cbegin();
		}

		// Make sure it's loaded or queued for loading
		if (enableLoading != TextureLoadMode::MOVETOTOPONLY && !tex->isLoaded())
		{
			//lock.unlock();
			load(tex);
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

	for (auto tex : mTextures)
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

void TextureDataManager::cleanupVRAM(std::shared_ptr<TextureData> exclude)
{
	std::unique_lock<std::recursive_mutex> lock(mMutex);

	size_t maxVRAM = (size_t)Settings::getInstance()->getInt("MaxVRAM") * 1024 * 1024;
	size_t size = TextureResource::getTotalMemoryUsage(MemoryUsageType::Allocated);

	if (exclude)
		size += exclude->getMemoryUsage(MemoryUsageType::Estimated);

	if (size < maxVRAM)
		return;

	int pass = 0;
	while (true)
	{		
		int maxSize = (pass == 0 && sizeof(void*) == 4) ? 120 * 1024 * 1024 : size; // With 1st pass, on 32bits ES, limit RAM cache usage to 120 mb

		// First Perform cleanup on textures without considering the queue
		for (auto it = mTextures.crbegin(); it != mTextures.crend(); ++it)
		{
			auto tex = *it;
			if (tex == exclude || !tex->isReloadable() || tex->isRequired() || !tex->isLoaded())
				continue;

			auto textureSize = tex->getMemoryUsage(pass == 0 ? MemoryUsageType::RAM : MemoryUsageType::VRAM);
			if (textureSize == 0)
				continue;			

			LOG(LogDebug) << "Cleanup VRAM\tReleased : " << tex->getPath().c_str() << ", " << std::to_string(tex->getSize().x()) << "x" << std::to_string(tex->getSize().y());

			tex->releaseVRAM();
			tex->releaseRAM();

			size -= textureSize;
			maxSize -= textureSize;
			if (maxSize < maxVRAM)
				break;
		}

		pass++;
		if (maxSize < maxVRAM || pass == 2)
			break;
	}
}

void TextureDataManager::load(std::shared_ptr<TextureData> tex, bool block)
{
	// See if it's already loaded
	if (tex->isLoaded())
	{
		if (tex->isMaxSizeValid())
			return;

		tex->releaseVRAM();
		tex->releaseRAM();

		block = true; // Reload instantly or other instances will fade again
	}

	mLoader->remove(tex);	

	if (!block)
		mLoader->load(tex);
	else
	{
		cleanupVRAM(tex);
		tex->load();
	}
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
	// Just abort any waiting texture
	clearQueue();

	// Exit the thread
	mExit = true;
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
	SetThreadDescription(GetCurrentThread(), L"TextureLoader::threadProc");
#endif

	while (true)
	{		
		// Wait for an event to say there is something in the queue
		std::unique_lock<std::mutex> lock(mLoaderLock);
		mEvent.wait(lock, [this]() { return !paused && (mExit || !mTextureDataQ.empty()); });

		if (mExit)
			break;

		if (!mTextureDataQ.empty())
		{
			std::shared_ptr<TextureData> textureData = mTextureDataQ.front();

			mTextureDataQ.pop_front();
			mTextureDataQSet.erase(textureData);

			if (textureData && !textureData->isLoaded())
			{
				mProcessingTextureDataQ.insert(textureData);

				lock.unlock();
				std::this_thread::yield();

				mManager->cleanupVRAM(textureData);

				try
				{
					textureData->load();
				}
				catch (const std::exception& e)
				{
				
				}
				catch (...)
				{

				}

				std::this_thread::yield();
				lock.lock();

				mProcessingTextureDataQ.erase(textureData);
			}

			lock.unlock();
			std::this_thread::yield();
		}		
	}
}

bool TextureLoader::paused = false;

void TextureLoader::load(std::shared_ptr<TextureData> textureData)
{
//	if (paused)
	//	return;

	std::unique_lock<std::mutex> lock(mLoaderLock);

	// Make sure it's not already loaded
	if (textureData->isLoaded())
		return;

	// If is is currently loading, don't add again
	if (mProcessingTextureDataQ.find(textureData) != mProcessingTextureDataQ.cend())
		return;

	// Remove it from the queue if it is already there
	if (mTextureDataQSet.erase(textureData) > 0)
	{
		auto tx = std::find(mTextureDataQ.cbegin(), mTextureDataQ.cend(), textureData);
		if (tx != mTextureDataQ.cend())
			mTextureDataQ.erase(tx);
	}

	// Put it on the start of the queue as we want the newly requested textures to load first
	mTextureDataQ.push_front(textureData);
	mTextureDataQSet.insert(textureData);

	mEvent.notify_one();
}

bool TextureLoader::remove(std::shared_ptr<TextureData> textureData)
{
	// Just remove it from the queue so we don't attempt to load it
	std::unique_lock<std::mutex> lock(mLoaderLock);

	if (mTextureDataQSet.erase(textureData) > 0)
	{
		auto tx = std::find(mTextureDataQ.cbegin(), mTextureDataQ.cend(), textureData);
		if (tx != mTextureDataQ.cend())
			mTextureDataQ.erase(tx);

		return true;
	}

	return false;
}

void TextureLoader::clearQueue()
{
	std::unique_lock<std::mutex> lock(mLoaderLock);

	// Just abort any waiting texture
	mTextureDataQSet.clear();
	mTextureDataQ.clear();	
}

void TextureDataManager::clearQueue()
{
	mBlank = nullptr;

	if (mLoader != nullptr)
		mLoader->clearQueue();
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
