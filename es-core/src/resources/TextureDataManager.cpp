#include "resources/TextureDataManager.h"

#include "resources/TextureData.h"
#include "resources/TextureResource.h"
#include "Settings.h"
#include "Log.h"
#include <algorithm>

TextureDataManager::TextureDataManager()
{
	unsigned char data[5 * 5 * 4];
	mBlank = std::make_shared<TextureData>(false, false);
	for (int i = 0; i < (5 * 5); ++i)
	{
		data[i * 4] = 0; // (i % 2) * 255;
		data[i*4+1] = 0; // (i % 2) * 255;
		data[i*4+2] = 0; // (i % 2) * 255;
		data[i*4+3] = 0;
	}
	mBlank->initFromRGBA(data, 5, 5);
	mLoader = new TextureLoader(this);
}

TextureDataManager::~TextureDataManager()
{
	delete mLoader;
}

void TextureDataManager::onTextureLoaded(std::shared_ptr<TextureData> tex)
{
	std::unique_lock<std::mutex> lock(mMutex);

	for (auto it = mTextureLookup.cbegin(); it != mTextureLookup.cend(); it++)
	{
		std::shared_ptr<TextureData> texture = *(*it).second;
		if (texture == tex)
		{
			const TextureResource* pResource = it->first;
			((TextureResource*)pResource)->onTextureLoaded(tex);
		}
	}
}

std::shared_ptr<TextureData> TextureDataManager::add(const TextureResource* key, bool tiled, bool linear)
{	
	std::unique_lock<std::mutex> lock(mMutex);

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
	std::unique_lock<std::mutex> lock(mMutex);

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
	std::unique_lock<std::mutex> lock(mMutex);

	auto it = mTextureLookup.find(key);
	if (it != mTextureLookup.cend())
		mLoader->remove(*(*it).second);
}

std::shared_ptr<TextureData> TextureDataManager::get(const TextureResource* key, bool enableLoading)
{
	std::unique_lock<std::mutex> lock(mMutex);
	
	// If it's in the cache then we want to remove it from it's current location and
	// move it to the top
	std::shared_ptr<TextureData> tex;
	auto it = mTextureLookup.find(key);
	if (it != mTextureLookup.cend())
	{
		tex = *(*it).second;

		if (!enableLoading)
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
		if (enableLoading && !tex->isLoaded())
		{
			lock.unlock();
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
		mBlank->uploadAndBind();
	return bound;
}

size_t TextureDataManager::getTotalSize()
{
	std::unique_lock<std::mutex> lock(mMutex);

	size_t total = 0;
	for (auto tex : mTextures)
		total += tex->width() * tex->height() * 4;

	return total;
}

size_t TextureDataManager::getCommittedSize()
{
	std::unique_lock<std::mutex> lock(mMutex);

	size_t total = 0;
	for (auto tex : mTextures)
		total += tex->getVRAMUsage();

	return total;
}

size_t TextureDataManager::getQueueSize()
{
	return mLoader->getQueueSize();
}

bool compareTextures(const std::shared_ptr<TextureData>& first, const std::shared_ptr<TextureData>& second)
{
	bool isResource = first->getPath().rfind(":/") == 0;
	bool secondIsResource = second->getPath().rfind(":/") == 0;
	if (isResource && !secondIsResource)
		return true;

	return false;
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

		mLoader->remove(tex);
		block = true; // Reload instantly or other instances will fade again
	}

	// Not loaded. Make sure there is room
	size_t size = TextureResource::getTotalMemUsage();
	size_t max_texture = (size_t)Settings::getInstance()->getInt("MaxVRAM") * 1024 * 1024;

	if (size >= max_texture)
	{
		LOG(LogDebug) << "Cleanup VRAM\tCurrent VRAM : " << std::to_string(size / 1024.0 / 1024.0).c_str() << " MB";

		std::unique_lock<std::mutex> lock(mMutex);

		std::list<std::shared_ptr<TextureData>> orderedTextures(mTextures);
		orderedTextures.sort(compareTextures);

		for (auto it = orderedTextures.crbegin(); it != orderedTextures.crend(); ++it)
		{
			if (size < max_texture)
				break;

			if ((*it) == tex)
				continue;

			bool changed = false;

			if ((*it)->isLoaded())
			{
				LOG(LogDebug) << "Cleanup VRAM\tReleased : " << (*it)->getPath().c_str();

				(*it)->releaseVRAM();
				(*it)->releaseRAM();

				changed = true;
			}

			// It may be already in the loader queue. In this case it wouldn't have been using
			// any VRAM yet but it will be. Remove it from the loader queue
			if (mLoader->remove(*it))
			{
				LOG(LogDebug) << "Cleanup VRAM\tRemoved from queue : " << (*it)->getPath().c_str();
				changed = true;
			}

			if (changed)
			{
				lock.unlock();
				size = TextureResource::getTotalMemUsage();
				lock.lock();
			}
		}
	}

	if (!block)
		mLoader->load(tex);
	else
	{
		mLoader->remove(tex);
		tex->load();
	}
}

TextureLoader::TextureLoader(TextureDataManager* mgr) : mManager(mgr), mExit(false)
{
	int num_threads = std::thread::hardware_concurrency() / 2;
	if (num_threads == 0)
		num_threads = 1;

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

void TextureLoader::threadProc()
{
	while (true)
	{		
		// Wait for an event to say there is something in the queue
		std::unique_lock<std::mutex> lock(mLoaderLock);
		mEvent.wait(lock, [this]() { return mExit || !mTextureDataQ.empty(); });

		if (mExit)
			break;

		if (!mTextureDataQ.empty())
		{
			std::shared_ptr<TextureData> textureData = mTextureDataQ.front();
			mTextureDataQ.pop_front();

			mProcessingTextureDataQ.push_back(textureData);

			lock.unlock();

			if (textureData && !textureData->isLoaded())
			{
				//LOG(LogDebug) << "TextureLoader::Thread\tLoading " << textureData->getPath().c_str();
				std::this_thread::yield();

				textureData->load(true);
				//mManager->onTextureLoaded(textureData);				

				lock.lock();
				mProcessingTextureDataQ.remove(textureData);			
				lock.unlock();
			}

			std::this_thread::yield();
		}		
	}
}

void TextureLoader::load(std::shared_ptr<TextureData> textureData)
{
	std::unique_lock<std::mutex> lock(mLoaderLock);

	// Make sure it's not already loaded
	if (textureData->isLoaded())
		return;

	// If is is currently loading, don't add again
	if (std::find(mProcessingTextureDataQ.cbegin(), mProcessingTextureDataQ.cend(), textureData) != mProcessingTextureDataQ.cend())
		return;

	// Remove it from the queue if it is already there
	auto tx = std::find(mTextureDataQ.cbegin(), mTextureDataQ.cend(), textureData);
	if (tx != mTextureDataQ.cend())
		mTextureDataQ.erase(tx);

	// Put it on the start of the queue as we want the newly requested textures to load first
	mTextureDataQ.push_front(textureData);
	mEvent.notify_one();
}

bool TextureLoader::remove(std::shared_ptr<TextureData> textureData)
{
	// Just remove it from the queue so we don't attempt to load it
	std::unique_lock<std::mutex> lock(mLoaderLock);

	auto tx = std::find(mTextureDataQ.cbegin(), mTextureDataQ.cend(), textureData);
	if (tx != mTextureDataQ.cend())
	{
		mTextureDataQ.erase(tx);
		return true;
	}

	return false;
}

size_t TextureLoader::getQueueSize()
{
	std::unique_lock<std::mutex> lock(mLoaderLock);

	// Gets the amount of video memory that will be used once all textures in
	// the queue are loaded
	size_t mem = 0;
	for (auto tex : mTextureDataQ)
		mem += tex->width() * tex->height() * 4;

	return mem;
}

void TextureLoader::clearQueue()
{
	std::unique_lock<std::mutex> lock(mLoaderLock);

	// Just abort any waiting texture
	mTextureDataQ.clear();	
}

void TextureDataManager::clearQueue()
{
	if (mLoader != nullptr)
		mLoader->clearQueue();
}