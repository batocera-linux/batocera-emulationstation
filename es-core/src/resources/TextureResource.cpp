#include "resources/TextureResource.h"

#include "utils/FileSystemUtil.h"
#include "resources/TextureData.h"
#include <cstring>
#include "Settings.h"
#include "PowerSaver.h"
#include "Log.h"
#include "renderers/Renderer.h"

TextureDataManager		TextureResource::sTextureDataManager;
std::map< TextureResource::TextureKeyType, std::weak_ptr<TextureResource> > TextureResource::sTextureMap;
std::set<TextureResource*> 	TextureResource::sNonDynamicTextureResources;

TextureResource::TextureResource(const std::string& path, bool tile, bool linear, bool dynamic, bool allowAsync, const MaxSizeInfo* maxSize) : mTextureData(nullptr), mForceLoad(false)
{
	// Create a texture data object for this texture
	if (!path.empty())
	{
		// If there is a path then the 'dynamic' flag tells us whether to use the texture
		// data manager to manage loading/unloading of this texture
		if (dynamic)
		{
			std::shared_ptr<TextureData> data = sTextureDataManager.add(this, tile, linear);
			if (maxSize != nullptr)
				data->setMaxSize(*maxSize);

			data->initFromPath(path);

			if (allowAsync && Settings::getInstance()->getBool("AsyncImages")) // && ImageIO::loadImageSize(ResourceManager::getInstance()->getResourcePath(path), &width, &height))
				data->setScalable(Utils::FileSystem::isSVG(path));
			else				
				sTextureDataManager.load(data, true); // Force the texture manager to load it using a blocking load
		}
		else
		{
			sTextureDataManager.cleanupVRAM();

			mTextureData = std::make_shared<TextureData>(tile, linear);

			if (maxSize != nullptr)
				mTextureData->setMaxSize(*maxSize);

			mTextureData->setDynamic(false);
			mTextureData->initFromPath(path);
			mTextureData->load();			
		}
	}
	else
	{
		// Create a texture managed by this class because it cannot be dynamically loaded and unloaded
		mTextureData = std::make_shared<TextureData>(tile, linear);
		mTextureData->setDynamic(false);
	}

	if (mTextureData != nullptr && sNonDynamicTextureResources.find(this) == sNonDynamicTextureResources.end())
		sNonDynamicTextureResources.insert(this);
}

TextureResource::~TextureResource()
{
	LOG(LogDebug) << "~TextureResource";
	
	if (mTextureData == nullptr)
		sTextureDataManager.remove(this);
	else if (sNonDynamicTextureResources.size() > 0)
	{
		auto pthis = sNonDynamicTextureResources.find(this);
		if (pthis != sNonDynamicTextureResources.end())
			sNonDynamicTextureResources.erase(pthis);
	}
}

void TextureResource::cleanupTextureResourceCache()
{
	std::vector<TextureKeyType> toRemove;

	for (auto item : sTextureMap)
		if (item.second.expired())
			toRemove.push_back(item.first);

	for (auto tmp : toRemove)
		sTextureMap.erase(tmp);
}

std::shared_ptr<TextureResource> TextureResource::get(const std::string& path, bool tile, bool linear, bool forceLoad, bool dynamic, bool asReloadable, const MaxSizeInfo* maxSize, const std::string& shareId)
{
	std::shared_ptr<ResourceManager>& rm = ResourceManager::getInstance();

	const std::string canonicalPath = Utils::FileSystem::getCanonicalPath(path);
	if (canonicalPath.empty())
	{
		std::shared_ptr<TextureResource> tex = std::make_shared<TextureResource>("", tile, linear, false, false);
		rm->addReloadable(tex); //make sure we get properly deinitialized even though we do nothing on reinitialization
		return tex;
	}

	// internal resources should not be dynamic
	if (canonicalPath.length() > 0 && canonicalPath[0] == ':')
		dynamic = false;

	TextureKeyType key(canonicalPath, tile, linear, shareId);

	//TextureKeyType key(canonicalPath, tile, linear);
	auto foundTexture = sTextureMap.find(key);
	if (foundTexture != sTextureMap.cend())
	{
		if (!foundTexture->second.expired())
		{
			std::shared_ptr<TextureResource> rc = foundTexture->second.lock();

			if (maxSize != nullptr && !maxSize->empty() && Settings::getInstance()->getBool("OptimizeVRAM"))
			{
				std::shared_ptr<TextureData> dt;
				if (rc->mTextureData != nullptr)
					dt = rc->mTextureData;
				else
					dt = sTextureDataManager.get(rc.get(), TextureLoadMode::NOLOAD);

				if (dt != nullptr)
				{
					dt->setMaxSize(*maxSize);

					if (dt->isLoaded() && !dt->isMaxSizeValid())
					{
						dt->releaseVRAM();
						dt->releaseRAM();
						dt->load();
					}
				}
			}

			return rc;
		}
		else if (!asReloadable)
			sTextureMap.erase(foundTexture);
	}

	// need to create it
	std::shared_ptr<TextureResource> tex;
	tex = std::make_shared<TextureResource>(std::get<0>(key), tile, linear, dynamic, !forceLoad, maxSize);

	auto loadMode = forceLoad ? TextureLoadMode::STANDARD: TextureLoadMode::NOLOAD;
	std::shared_ptr<TextureData> data = sTextureDataManager.get(tex.get(), loadMode);

	if (asReloadable)
	{
		sTextureMap[key] = std::weak_ptr<TextureResource>(tex);

		// Add it to the reloadable list
		rm->addReloadable(tex);
	}

	if (data != nullptr && maxSize != nullptr)
		data->setMaxSize(*maxSize);

	// Force load it if necessary. Note that it may get dumped from VRAM if we run low
	if (forceLoad)
	{
		tex->mForceLoad = forceLoad;

		if (data != nullptr && !data->isLoaded())
			data->load();
	}

	return tex;
}

void TextureResource::updateFromExternalPixels(unsigned char* dataRGBA, size_t width, size_t height)
{
	// This is only valid if we have a local texture data object
	if (mTextureData == nullptr)
		return;

	mTextureData->updateFromExternalRGBA(dataRGBA, width, height);
}

void TextureResource::initFromPixels(unsigned char* dataRGBA, size_t width, size_t height)
{
	// This is only valid if we have a local texture data object
	if (mTextureData == nullptr)
		return;

	mTextureData->releaseVRAM();

	// FCA optimisation, if streamed image size is already the same, don't free/reallocate memory (which is slow), just copy bytes
	if (mTextureData->getDataRGBA() != nullptr && getSize().x() == width && getSize().y() == height)
	{
		memcpy(mTextureData->getDataRGBA(), dataRGBA, width * height * 4);
		return;
	}

	mTextureData->releaseRAM();
	mTextureData->initFromRGBA(dataRGBA, width, height);
}

void TextureResource::initFromMemory(const char* data, size_t length)
{
	// This is only valid if we have a local texture data object
	if (mTextureData == nullptr)
		return;

	mTextureData->releaseVRAM();
	mTextureData->releaseRAM();
	mTextureData->initImageFromMemory((const unsigned char*)data, length);
}

bool TextureResource::isTiled() const
{
	auto data = mTextureData ? mTextureData : sTextureDataManager.get(this, TextureLoadMode::NOLOAD);
	return data ? data->tiled() : false;
}

bool TextureResource::isScalable() const
{
	auto data = mTextureData ? mTextureData : sTextureDataManager.get(this, TextureLoadMode::NOLOAD);
	return data ? data->isScalable() : false;
}

void TextureResource::prioritize() const
{
	if (mTextureData == nullptr)
		sTextureDataManager.get(this, TextureLoadMode::MOVETOTOPONLY);
}

void TextureResource::setRequired(bool value) const
{
	if (mTextureData != nullptr)
		return;
	
	auto data = sTextureDataManager.get(this, TextureLoadMode::NOLOAD);
	if (data != nullptr)
		data->setRequired(value);	
}

bool TextureResource::bind()
{
	if (mTextureData != nullptr)
	{
		mTextureData->uploadAndBind();
		return true;
	}

	return sTextureDataManager.bind(this);	
}

void TextureResource::cancelAsync(std::shared_ptr<TextureResource> texture)
{
	if (texture != nullptr)
		sTextureDataManager.cancelAsync(texture.get());
}

// For scalable source images in textures we want to set the resolution to rasterize at
void TextureResource::rasterizeAt(size_t width, size_t height)
{
	// Avoids crashing if a theme (in grids) defines negative sizes
	if (width < 0) width = -width;
	if (height < 0) height = -height;

	auto data = mTextureData ? mTextureData : sTextureDataManager.get(this, TextureLoadMode::NOLOAD);
	if (data != nullptr)
		data->rasterizeAt((float)width, (float)height);
}

bool TextureResource::isLoaded() const
{
	if (mTextureData != nullptr)
		return mTextureData->isLoaded();

	auto data = sTextureDataManager.get(this, TextureLoadMode::NOLOAD);
	if (data != nullptr)
		return data->isLoaded();

	return true;
}

size_t TextureResource::getTotalMemoryUsage(MemoryUsageType type)
{
	size_t total = 0;

	// Count up all textures that manage their own texture data
	for (auto tex : sNonDynamicTextureResources)
		total += tex->mTextureData->getMemoryUsage(type);

	// Now get the committed memory from the manager
	total += sTextureDataManager.getTotalMemoryUsage(type);

	return total;
}

size_t TextureResource::getMemoryUsage(MemoryUsageType type)
{
	auto data = mTextureData ? mTextureData : sTextureDataManager.get(this, TextureLoadMode::NOLOAD);
	return data != nullptr ? data->getMemoryUsage(type) : 0;
}

bool TextureResource::unload()
{
	// Release the texture's resources
	auto data = mTextureData ? mTextureData : sTextureDataManager.get(this, TextureLoadMode::NOLOAD);
	if (data != nullptr && data->isLoaded())
	{
		data->releaseVRAM();
		data->releaseRAM();

		return true;
	}

	return false;
}

void TextureResource::reload()
{
	// For dynamically loaded textures the texture manager will load them on demand.
	// For manually loaded textures we have to reload them here
	if (mTextureData)
	{
		if (!mTextureData->isLoaded())
			mTextureData->load();
	}
	else
		sTextureDataManager.get(this, TextureLoadMode::LOADNOMOVETOTOP);
}

void TextureResource::clearQueue()
{
	sTextureDataManager.clearQueue();
}

const Vector2i TextureResource::getSize() const
{ 	
	auto data = mTextureData ? mTextureData : sTextureDataManager.get(this, TextureLoadMode::NOLOAD);
	return data && data->isLoaded() ? data->getSize() : Vector2i::Zero(); //	mSize;
}

const Vector2f TextureResource::getPhysicalSize() const 
{ 
	auto data = mTextureData ? mTextureData : sTextureDataManager.get(this, TextureLoadMode::NOLOAD);
	return data && data->isLoaded() ? data->getPhysicalSize() : Vector2f::Zero(); // mPhysicalSize;
}
