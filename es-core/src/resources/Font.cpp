#include "resources/Font.h"

#include "renderers/Renderer.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include "math/Misc.h"
#include "LocaleES.h"

#include "ResourceManager.h"
#include "TextureResource.h"
#include "Settings.h"
#include "ImageIO.h"
#include <algorithm>

#ifdef WIN32
#include <Windows.h>
#endif

FT_Library Font::sLibrary = NULL;

int Font::getSize() const { return mSize; }

std::map< std::pair<std::string, int>, std::weak_ptr<Font> > Font::sFontMap;
static std::map<unsigned int, std::string> substituableChars;

Font::FontFace::FontFace(ResourceData&& d, int size) : data(d)
{
	int err = FT_New_Memory_Face(sLibrary, data.ptr.get(), (FT_Long)data.length, 0, &face);
	if (!err)
		FT_Set_Pixel_Sizes(face, 0, size);
}

Font::FontFace::~FontFace()
{
	if(face)
		FT_Done_Face(face);
}

void Font::initLibrary()
{
	if (sLibrary != nullptr)
		return;

	if(FT_Init_FreeType(&sLibrary))
	{
		sLibrary = NULL;
		LOG(LogError) << "Error initializing FreeType!";
	}
}

size_t Font::getMemUsage() const
{
	size_t memUsage = 0;
	
	for(auto tex : mTextures)
		memUsage += (tex->textureId != 0 ? tex->textureSize.x() * tex->textureSize.y() * 4 : 0);

	for(auto it = mFaceCache.cbegin(); it != mFaceCache.cend(); it++)
		memUsage += it->second->data.length;

	return memUsage;
}

size_t Font::getTotalMemUsage()
{
	size_t total = 0;

	auto it = sFontMap.cbegin();
	while(it != sFontMap.cend())
	{
		if(it->second.expired())
		{
			it = sFontMap.erase(it);
			continue;
		}

		total += it->second.lock()->getMemUsage();
		it++;
	}

	return total;
}

Font::Font(int size, const std::string& path) : mSize(size), mPath(path)
{
	mSize = size;
	//if(mSize > 160) mSize = 160; // maximize the font size while it is causing issues on linux

	// GPI
	if (Renderer::isSmallScreen())
	{
		float sz = Math::min(Renderer::getScreenWidth(), Renderer::getScreenHeight());
		if (sz >= 320) // ODROID 480x320;
			mSize = size * 1.31;
		else // GPI 320x240
			mSize = size * 1.5;
	}

	if (mSize == 0)
		mSize = 2;

	mLoaded = true;
	mMaxGlyphHeight = 0;

	if(!sLibrary)
		initLibrary();

	for (unsigned int i = 0; i < 255; i++)
		mGlyphCacheArray[i] = NULL;

	// always initialize ASCII characters
	for(unsigned int i = 32; i < 128; i++)
		getGlyph(i);

	clearFaceCache();
}

Font::~Font()
{
	unload();

	for (auto tex : mTextures)
		delete tex;

	mTextures.clear();
}

void Font::reload()
{
	if (mLoaded)
		return;
	
	rebuildTextures();
	clearFaceCache();
	Renderer::bindTexture(0);

	mLoaded = true;
}

bool Font::unload()
{
	if (mLoaded)
	{		
		for (auto tex : mTextures)
			tex->deinitTexture();

		clearFaceCache();

		mLoaded = false;
		return true;
	}

	return false;
}

std::shared_ptr<Font> Font::get(int size, const std::string& path)
{
	const std::string canonicalPath = Utils::FileSystem::getCanonicalPath(path);

	std::pair<std::string, int> def(canonicalPath.empty() ? getDefaultPath() : canonicalPath, size);
	auto foundFont = sFontMap.find(def);
	if(foundFont != sFontMap.cend())
	{
		if(!foundFont->second.expired())
			return foundFont->second.lock();
	}

	std::shared_ptr<Font> font = std::shared_ptr<Font>(new Font(def.second, def.first));
	sFontMap[def] = std::weak_ptr<Font>(font);
	ResourceManager::getInstance()->addReloadable(font);
	return font;
}

Font::FontTexture::FontTexture()
{
	textureId = 0;
	textureSize = Vector2i(2048, 512);
	writePos = Vector2i::Zero();
	rowHeight = 0;
}

Font::FontTexture::~FontTexture()
{
	deinitTexture();
}

bool Font::FontTexture::findEmpty(const Vector2i& size, Vector2i& cursor_out)
{
	if(size.x() >= textureSize.x() || size.y() >= textureSize.y())
		return false;

	if(writePos.x() + size.x() >= textureSize.x() && writePos.y() + rowHeight + size.y() + 1 < textureSize.y())
	{
		// row full, but it should fit on the next row
		// move cursor to next row
		writePos = Vector2i(0, writePos.y() + rowHeight + 1); // leave 1px of space between glyphs
		rowHeight = 0;
	}

	if(writePos.x() + size.x() >= textureSize.x() || writePos.y() + size.y() >= textureSize.y())
	{
		// nope, still won't fit
		return false;
	}

	cursor_out = writePos;
	writePos[0] += size.x() + 1; // leave 1px of space between glyphs

	if(size.y() > rowHeight)
		rowHeight = size.y();

	return true;
}

void Font::FontTexture::initTexture()
{
	if (textureId == 0)
	{
		textureId = Renderer::createTexture(Renderer::Texture::ALPHA, true, false, textureSize.x(), textureSize.y(), nullptr);
		if (textureId == 0)
			LOG(LogError) << "FontTexture::initTexture() failed to create texture " << textureSize.x() << "x" << textureSize.y();
	}
}

void Font::FontTexture::deinitTexture()
{
	if(textureId != 0)
	{
		Renderer::destroyTexture(textureId);
		textureId = 0;
	}
}

void Font::getTextureForNewGlyph(const Vector2i& glyphSize, FontTexture*& tex_out, Vector2i& cursor_out)
{
	if(mTextures.size())
	{
		// check if the most recent texture has space
		tex_out = mTextures.back();

		// will this one work?
		if(tex_out->findEmpty(glyphSize, cursor_out))
			return; // yes

		LOG(LogDebug) << "Glyph texture cache full, creating a new texture cache for " << Utils::FileSystem::getFileName(mPath) << " " << mSize << "pt";
	}

	// current textures are full,
	// make a new one
	FontTexture* tex = new FontTexture();

	int x = Math::min(2048, mSize * 64);
	int y = Math::min(2048, Math::max(glyphSize.y(), mSize) + 2) * 1.2;

	tex->textureSize = Vector2i(x, y);
	tex->initTexture();

	tex_out = tex;

	mTextures.push_back(tex);
	
	bool ok = tex_out->findEmpty(glyphSize, cursor_out);
	if(!ok)
	{
		LOG(LogError) << "Glyph too big to fit on a new texture (glyph size > " << tex_out->textureSize.x() << ", " << tex_out->textureSize.y() << ")!";
		delete tex;
		tex_out = NULL;
	}
}

std::vector<std::string> getFallbackFontPaths()
{
	std::vector<std::string> fallbackFonts = 
	{
		":/fontawesome-webfont.ttf",
		":/DroidSansFallbackFull.ttf",// japanese, chinese, present on Debian
		":/NanumMyeongjo.ttf", // korean font		
		":/Cairo.ttf" // arabic
	};

	std::vector<std::string> paths;

	for (auto font : fallbackFonts)
		if (ResourceManager::getInstance()->fileExists(font))
			paths.push_back(font);

	paths.shrink_to_fit();
	return paths;
}

FT_Face Font::getFaceForChar(unsigned int id)
{
	static const std::vector<std::string> fallbackFonts = getFallbackFontPaths();

	// look through our current font + fallback fonts to see if any have the glyph we're looking for
	for(unsigned int i = 0; i < fallbackFonts.size() + 1; i++)
	{
		auto fit = mFaceCache.find(i);

		if(fit == mFaceCache.cend()) // doesn't exist yet
		{
			// i == 0 -> mPath
			// otherwise, take from fallbackFonts
			const std::string& path = (i == 0 ? mPath : fallbackFonts.at(i - 1));
			ResourceData data = ResourceManager::getInstance()->getFileData(path);
			mFaceCache[i] = std::unique_ptr<FontFace>(new FontFace(std::move(data), mSize));
			fit = mFaceCache.find(i);
		}

		if(FT_Get_Char_Index(fit->second->face, id) != 0)
			return fit->second->face;
	}

	// nothing has a valid glyph - return the "real" face so we get a "missing" character
	return mFaceCache.cbegin()->second->face;
}

void Font::clearFaceCache()
{
	mFaceCache.clear();
}

Font::Glyph* Font::getGlyph(unsigned int id)
{
	if (id < 255)
	{
		// FCA Optimisation : array is faster than a map
		// When computing & displaying long descriptions in gamelist views, it can come here textsize*2 times per frame
		Glyph* fastCache = mGlyphCacheArray[id];
		if (fastCache != NULL)
			return fastCache;
	}
	else
	{
		// is it already loaded?
		auto it = mGlyphMap.find(id);
		if (it != mGlyphMap.cend())
			return it->second;
	}

	// nope, need to make a glyph
	FT_Face face = getFaceForChar(id);
	if(!face)
	{
		LOG(LogError) << "Could not find appropriate font face for character " << id << " for font " << mPath;
		return NULL;
	}

	FT_GlyphSlot g = face->glyph;

	if(FT_Load_Char(face, id, FT_LOAD_RENDER))
	{
		LOG(LogError) << "Could not find glyph for character " << id << " for font " << mPath << ", size " << mSize << "!";
		return NULL;
	}

	Vector2i glyphSize(g->bitmap.width, g->bitmap.rows);

	FontTexture* tex = NULL;
	Vector2i cursor;
	getTextureForNewGlyph(glyphSize, tex, cursor);

	// getTextureForNewGlyph can fail if the glyph is bigger than the max texture size (absurdly large font size)
	if(tex == NULL)
	{
		LOG(LogError) << "Could not create glyph for character " << id << " for font " << mPath << ", size " << mSize << " (no suitable texture found)!";
		return NULL;
	}

	// create glyph
	Glyph* pGlyph = new Glyph();
	
	pGlyph->texture = tex;
	pGlyph->texPos = Vector2f((float)cursor.x() / (float)tex->textureSize.x(), (float)cursor.y() / (float)tex->textureSize.y());
	pGlyph->texSize = Vector2f((float)glyphSize.x() / (float)tex->textureSize.x(), (float)glyphSize.y() / (float)tex->textureSize.y());
	pGlyph->advance = Vector2f((float)g->metrics.horiAdvance / 64.0f, (float)g->metrics.vertAdvance / 64.0f);
	pGlyph->bearing = Vector2f((float)g->metrics.horiBearingX / 64.0f, (float)g->metrics.horiBearingY / 64.0f);	
	pGlyph->cursor = cursor;
	pGlyph->glyphSize = glyphSize;

	// upload glyph bitmap to texture
	Renderer::updateTexture(tex->textureId, Renderer::Texture::ALPHA, cursor.x(), cursor.y(), glyphSize.x(), glyphSize.y(), g->bitmap.buffer);

	// update max glyph height
	if(glyphSize.y() > mMaxGlyphHeight)
		mMaxGlyphHeight = glyphSize.y();

	mGlyphMap[id] = pGlyph;

	if (id < 255)
		mGlyphCacheArray[id] = pGlyph;

	// done
	return pGlyph;
}

// completely recreate the texture data for all textures based on mGlyphs information
void Font::rebuildTextures()
{
	// recreate OpenGL textures
	for(auto tex : mTextures)
		tex->initTexture();

	// reupload the texture data
	for(auto it = mGlyphMap.cbegin(); it != mGlyphMap.cend(); it++)
	{
		FT_Face face = getFaceForChar(it->first);
		FT_GlyphSlot glyphSlot = face->glyph;

		// load the glyph bitmap through FT
		FT_Load_Char(face, it->first, FT_LOAD_RENDER);

		Glyph* glyph = it->second;
		
		// upload to texture
		Renderer::updateTexture(glyph->texture->textureId, Renderer::Texture::ALPHA,
			glyph->cursor.x(), glyph->cursor.y(),
			glyph->glyphSize.x(), glyph->glyphSize.y(),
			glyphSlot->bitmap.buffer);
	}
}

void Font::renderTextCache(TextCache* cache)
{
	if(cache == NULL)
	{
		LOG(LogError) << "Attempted to draw NULL TextCache!";
		return;
	}

	int tex = -1;

	for(auto& vertex : cache->vertexLists)
	{		
		if (vertex.textureIdPtr == nullptr)
			continue;
		
		if (tex != *vertex.textureIdPtr)
		{
			tex = *vertex.textureIdPtr;
			Renderer::bindTexture(tex);			
		}

		if (tex != 0)
			Renderer::drawTriangleStrips(&vertex.verts[0], vertex.verts.size());
	}

	if (cache->renderingGlow)
		return;

	for (auto sub : cache->imageSubstitutes)
	{
		if (sub.texture && sub.texture->bind())
		{
			if (Settings::DebugImage)
				Renderer::drawRect(
					sub.vertex[0].pos.x(), 
					sub.vertex[0].pos.y(), 
					sub.vertex[3].pos.x() - sub.vertex[0].pos.x(),
					sub.vertex[3].pos.x() - sub.vertex[0].pos.x(), 
					0xFF000033, 0xFF000033);

			Renderer::drawTriangleStrips(&sub.vertex[0], 4);
		}
	}
}

void Font::renderGradientTextCache(TextCache* cache, unsigned int colorTop, unsigned int colorBottom, bool horz)
{
	if (cache == NULL)
	{
		LOG(LogError) << "Attempted to draw NULL TextCache!";
		return;
	}

	for (auto it = cache->vertexLists.cbegin(); it != cache->vertexLists.cend(); it++)
	{
		if (*it->textureIdPtr == 0)
			continue;

		std::vector<Renderer::Vertex> vxs;
		vxs.resize(it->verts.size());

		float maxY = -1;

		for (int i = 0; i < it->verts.size(); i += 6)
			if (maxY == -1 || maxY < it->verts[i + 2].pos.y())
				maxY = it->verts[i + 2].pos.y();

		for (int i = 0; i < it->verts.size(); i += 6)
		{
			float topOffset = it->verts[i + 1].pos.y();
			float bottomOffset = it->verts[i + 2].pos.y();
			
			float topPercent = (maxY == 0 ? 1.0 : topOffset / maxY);
			float bottomPercent = (maxY == 0 ? 1.0 : bottomOffset / maxY);

			const unsigned int colorT = Renderer::mixColors(colorTop, colorBottom, topPercent);
			const unsigned int colorB = Renderer::mixColors(colorTop, colorBottom, bottomPercent);
		
			vxs[i + 1] = it->verts[i + 1];
			vxs[i + 1].col = colorT;

			vxs[i + 2] = it->verts[i + 2];
			vxs[i + 2].col = colorB;

			vxs[i + 3] = it->verts[i + 3];
			vxs[i + 3].col = colorT;

			vxs[i + 4] = it->verts[i + 4];
			vxs[i + 4].col = colorB;

			// make duplicates of first and last vertex so this can be rendered as a triangle strip
			vxs[i + 0] = vxs[i + 1];
			vxs[i + 5] = vxs[i + 4];
		}

		Renderer::bindTexture(*it->textureIdPtr);
		Renderer::drawTriangleStrips(&vxs[0], vxs.size());		
	}
}

std::string tryFastBidi(const std::string& text)
{
	std::string ret = "";

	size_t lastCursor = 0;
	size_t i = 0;
	while (i < text.length())
	{
		const char&  c = text[i];
		
		if ((c & 0xE0) == 0xC0)
		{
			if ((c & 0x10) == 0x10)
			{
				ret.insert(lastCursor, text.substr(i + 1, 1));
				ret.insert(lastCursor, text.substr(i, 1));
				i += 2;
			}
			else
			{
				ret += text[i++];
				ret += text[i++];
				lastCursor = i;
			}
		}
		else if ((c & 0xF0) == 0xE0)
		{
			ret += text[i++];
			ret += text[i++];
			ret += text[i++];
			lastCursor = i;
		}
		else if ((c & 0xF8) == 0xF0)
		{
			ret += text[i++];
			ret += text[i++];
			ret += text[i++];
			ret += text[i++];
			lastCursor = i;
		}
		else
		{
			if (c == 32 && 
				i + 1 < text.length() && (text[i+1] & 0xE0) == 0xC0 && (text[i + 1] & 0x10) == 0x10 && 
				i > 0 && (text[i - 1] & 0xE0) != 0xC0 && (text[i -1 ] & 0x10) != 0x10)
			{
				ret.insert(lastCursor, text.substr(i, 1));
				i++;
			}
			else
			{
				ret += text[i++];
				lastCursor = i;
			}
		}
	}

	return ret;
}

Vector2f Font::sizeText(std::string text, float lineSpacing)
{
	float lineWidth = 0.0f;
	float highestWidth = 0.0f;

	const float lineHeight = getHeight(lineSpacing);

	float y = lineHeight;

	size_t i = 0;
	while(i < text.length())
	{
		unsigned int character = Utils::String::chars2Unicode(text, i); // advances i

		if (substituableChars.find(character) != substituableChars.cend())
		{
			lineWidth += lineHeight;
			continue;
		}

		if(character == '\n')
		{
			if(lineWidth > highestWidth)
				highestWidth = lineWidth;

			lineWidth = 0.0f;
			y += lineHeight;
		}

		Glyph* glyph = getGlyph(character);
		if(glyph)
			lineWidth += glyph->advance.x();
	}

	if(lineWidth > highestWidth)
		highestWidth = lineWidth;

	return Vector2f(highestWidth, y);
}

float Font::getHeight(float lineSpacing) const
{
	return mMaxGlyphHeight * lineSpacing;
}

float Font::getLetterHeight()
{
	Glyph* glyph = getGlyph('S');
	if (glyph != nullptr)
		return glyph->glyphSize.y();

	return mSize;
}

//the worst algorithm ever written
//breaks up a normal string with newlines to make it fit xLen
std::string Font::wrapText(std::string text, float xLen)
{
	std::string out;

	std::string line, word, temp;
	size_t space;

	Vector2f textSize;

	while(text.length() > 0) //while there's text or we still have text to render
	{
		space = text.find_first_of(" \t\n");
		if(space == std::string::npos)
			space = text.length() - 1;

		word = text.substr(0, space + 1);
		text.erase(0, space + 1);

		temp = line + word;

		textSize = sizeText(temp);

		// if the word will fit on the line, add it to our line, and continue
		if(textSize.x() <= xLen)
		{
			line = temp;
			continue;
		}else{
			// the next word won't fit, so break here
			out += line + '\n';
			line = word;
		}
	}

	// whatever's left should fit
	out += line;

	return out;
}

Vector2f Font::sizeWrappedText(std::string text, float xLen, float lineSpacing)
{
	text = wrapText(text, xLen);
	return sizeText(text, lineSpacing);
}

Vector2f Font::getWrappedTextCursorOffset(std::string text, float xLen, size_t stop, float lineSpacing)
{
	std::string wrappedText = wrapText(text, xLen);

	float lineWidth = 0.0f;
	float y = 0.0f;

	size_t wrapCursor = 0;
	size_t cursor = 0;
	while(cursor < stop)
	{
		unsigned int wrappedCharacter = Utils::String::chars2Unicode(wrappedText, wrapCursor);
		unsigned int character = Utils::String::chars2Unicode(text, cursor);

		if(wrappedCharacter == '\n' && character != '\n')
		{
			//this is where the wordwrap inserted a newline
			//reset lineWidth and increment y, but don't consume a cursor character
			lineWidth = 0.0f;
			y += getHeight(lineSpacing);

			cursor = Utils::String::prevCursor(text, cursor); // unconsume
			continue;
		}

		if(character == '\n')
		{
			lineWidth = 0.0f;
			y += getHeight(lineSpacing);
			continue;
		}

		Glyph* glyph = getGlyph(character);
		if(glyph)
			lineWidth += glyph->advance.x();
	}

	return Vector2f(lineWidth, y);
}

//=============================================================================================================
//TextCache
//=============================================================================================================

float Font::getNewlineStartOffset(const std::string& text, const unsigned int& charStart, const float& xLen, const Alignment& alignment)
{
	switch(alignment)
	{
	case ALIGN_LEFT:
		return 0;
	case ALIGN_CENTER:
		{
			unsigned int endChar = (unsigned int)text.find('\n', charStart);
			return (xLen - sizeText(text.substr(charStart, endChar != std::string::npos ? endChar - charStart : endChar)).x()) / 2.0f;
		}
	case ALIGN_RIGHT:
		{
			unsigned int endChar = (unsigned int)text.find('\n', charStart);
			return xLen - (sizeText(text.substr(charStart, endChar != std::string::npos ? endChar - charStart : endChar)).x());
		}
	default:
		return 0;
	}
}

TextCache* Font::buildTextCache(const std::string& _text, Vector2f offset, unsigned int color, float xLen, Alignment alignment, float lineSpacing)
{
	float x = offset[0] + (xLen != 0 ? getNewlineStartOffset(_text, 0, xLen, alignment) : 0);
	
	float yTop = getGlyph('S')->bearing.y();
	float yBot = getHeight(lineSpacing);
	float yDecal = (yBot + yTop) / 2.0f;
	float y = offset[1] + (yBot + yTop)/2.0f;

	// vertices by texture
	std::map< FontTexture*, std::vector<Renderer::Vertex> > vertMap;

	std::string text = EsLocale::isRTL() ? tryFastBidi(_text) : _text;

	std::map<int, int> tabStops;
	int tabIndex = 0;

	if (alignment == ALIGN_LEFT && text.find("\t") != std::string::npos)
	{
		for (auto line : Utils::String::split(text, '\n', true))
		{
			if (line.find("\t") == std::string::npos)
				continue;

			tabIndex = 0;
			int curTab = 0;
			int xpos = x;

			size_t pos = 0;
			while (pos < text.length())
			{
				unsigned int character = Utils::String::chars2Unicode(text, pos); // also advances cursor
				if (character == 0 || character == '\r')
					continue;

				if (substituableChars.find(character) != substituableChars.cend())
				{
					x += yBot;
					continue;
				}

				if (character == '\t')
				{
					auto it = tabStops.find(tabIndex);
					if (it != tabStops.cend())
						it->second = Math::max(it->second, xpos);
					else
						tabStops[tabIndex] = xpos;

					curTab = xpos;
					tabIndex++;
				}

				auto glyph = getGlyph(character);
				if (glyph == NULL)
					continue;

				xpos += glyph->advance.x();
			}
		}
	}

	std::vector<TextImageSubstitute> imageSubstitutes;

	tabIndex = 0;
	size_t cursor = 0;
	while(cursor < text.length())
	{
		unsigned int character = Utils::String::chars2Unicode(text, cursor); // also advances cursor

		auto it = substituableChars.find(character);
		if (it != substituableChars.cend() && ResourceManager::getInstance()->fileExists(it->second))
		{
			auto padding = (yTop / 4.0f);

			// MaxSizeInfo mx(yBot - (2.0f * padding), yBot - (2.0f * padding));

			TextImageSubstitute is;
			is.texture = TextureResource::get(it->second, false, true, true, false, true); // , &mx);
			if (is.texture != nullptr)
			{
				Renderer::Rect rect(
					x,
					y - yDecal + padding,
					yBot - (2.0f * padding),
					yBot - padding);

				auto imgSize = is.texture->getSourceImageSize();
				auto sz = ImageIO::adjustPictureSize(Vector2i(imgSize.x(), imgSize.y()), Vector2i(rect.w, rect.h));

				Renderer::Rect rc(
					rect.x + (rect.w / 2.0f) - (sz.x() / 2.0f), 
					rect.y + (rect.h / 2.0f) - (sz.y() / 2.0f),
					sz.x(),
					sz.y());
				
				is.vertex[0] = { { (float) rc.x			, (float) rc.y + rc.h }	, { 0.0f, 0.0f }, 0xFFFFFFFF };
				is.vertex[1] = { { (float) rc.x			, (float) rc.y }		, { 0.0f, 1.0f }, 0xFFFFFFFF };
				is.vertex[2] = { { (float) rc.x + rc.w  , (float) rc.y + rc.h }	, { 1.0f, 0.0f }, 0xFFFFFFFF };
				is.vertex[3] = { { (float) rc.x + rc.w  , (float) rc.y }		, { 1.0f, 1.0f }, 0xFFFFFFFF };

				imageSubstitutes.push_back(is);

				x += yBot - (2.0f*padding);
				continue;
			}
		}

		Glyph* glyph;

		// invalid character
		if (character == 0)
			continue;

		if (character == '\r')
			continue;

		if(character == '\n')
		{
			tabIndex = 0;
			y += getHeight(lineSpacing);
			x = offset[0] + (xLen != 0 ? getNewlineStartOffset(text, (const unsigned int)cursor /* cursor is already advanced */, xLen, alignment) : 0);
			continue;
		}

		if (character == '\t')
		{
			auto it = tabStops.find(tabIndex);
			if (it != tabStops.cend())
			{
				x = it->second + Renderer::getScreenWidth() * 0.01f;
				tabIndex++;
				continue;
			}

			character = ' ';
			tabIndex++;
		}

		glyph = getGlyph(character);
		if(glyph == NULL)
			continue;

		std::vector<Renderer::Vertex>& verts = vertMap[glyph->texture];
		size_t oldVertSize = verts.size();
		verts.resize(oldVertSize + 6);
		Renderer::Vertex* vertices = verts.data() + oldVertSize;

		const float        glyphStartX    = x + glyph->bearing.x();
		const unsigned int convertedColor = Renderer::convertColor(color);

		vertices[1] = { { glyphStartX                                       , y - glyph->bearing.y()                                          }, { glyph->texPos.x(),                      glyph->texPos.y()                      }, convertedColor };
		vertices[2] = { { glyphStartX                                       , y - glyph->bearing.y() + (glyph->glyphSize.y())                 }, { glyph->texPos.x(),                      glyph->texPos.y() + glyph->texSize.y() }, convertedColor };
		vertices[3] = { { glyphStartX + glyph->glyphSize.x()                , y - glyph->bearing.y()                                          }, { glyph->texPos.x() + glyph->texSize.x(), glyph->texPos.y()                      }, convertedColor };
		vertices[4] = { { glyphStartX + glyph->glyphSize.x()                , y - glyph->bearing.y() + (glyph->glyphSize.y())                 }, { glyph->texPos.x() + glyph->texSize.x(), glyph->texPos.y() + glyph->texSize.y() }, convertedColor };

		// round vertices
		for(int i = 1; i < 5; ++i)
			vertices[i].pos.round();

		// make duplicates of first and last vertex so this can be rendered as a triangle strip
		vertices[0] = vertices[1];
		vertices[5] = vertices[4];

		// advance
		x += glyph->advance.x();
	}

	//TextCache::CacheMetrics metrics = { sizeText(text, lineSpacing) };

	TextCache* cache = new TextCache();
	cache->vertexLists.resize(vertMap.size());
	cache->metrics = { sizeText(text, lineSpacing) };
	cache->imageSubstitutes = imageSubstitutes;

	unsigned int i = 0;
	for(auto it = vertMap.cbegin(); it != vertMap.cend(); it++)
	{
		TextCache::VertexList& vertList = cache->vertexLists.at(i);

		vertList.textureIdPtr = &it->first->textureId;
		vertList.verts = it->second;
		i++;
	}

	clearFaceCache();

	return cache;
}

TextCache* Font::buildTextCache(const std::string& text, float offsetX, float offsetY, unsigned int color)
{
	return buildTextCache(text, Vector2f(offsetX, offsetY), color, 0.0f);
}

void TextCache::setColor(unsigned int color)
{
	const unsigned int convertedColor = Renderer::convertColor(color);

	for(auto it = vertexLists.begin(); it != vertexLists.end(); it++)
		for(auto it2 = it->verts.begin(); it2 != it->verts.end(); it2++)
			it2->col = convertedColor;
}

std::shared_ptr<Font> Font::getFromTheme(const ThemeData::ThemeElement* elem, unsigned int properties, const std::shared_ptr<Font>& orig)
{
	using namespace ThemeFlags;
	if(!(properties & FONT_PATH) && !(properties & FONT_SIZE))
		return orig;
	
	std::shared_ptr<Font> font;
	int size = (orig ? orig->mSize : FONT_SIZE_MEDIUM);
	std::string path = (orig ? orig->mPath : getDefaultPath());

	float sh = (float) Math::min(Renderer::getScreenHeight(), Renderer::getScreenWidth());
	if(properties & FONT_SIZE && elem->has("fontSize")) 
	{
		if ((int)(sh * elem->get<float>("fontSize")) > 0)
			size = (int)(sh * elem->get<float>("fontSize"));
	}

	if(properties & FONT_PATH && elem->has("fontPath"))
	{
		std::string tmppath = elem->get<std::string>("fontPath");
		if (!tmppath.empty())
			path = tmppath;
	}

	return get(size, path);
}

void Font::OnThemeChanged()
{
	static std::map<unsigned int, std::string> defaultMap =
	{
		{ 0xF300, ":/flags/au.png" },
		{ 0xF301, ":/flags/br.png" },
		{ 0xF302, ":/flags/ca.png" },
		{ 0xF303, ":/flags/ch.png" },
		{ 0xF304, ":/flags/de.png" },
		{ 0xF305, ":/flags/es.png" },
		{ 0xF306, ":/flags/eu.png" },
		{ 0xF307, ":/flags/fr.png" },
		{ 0xF308, ":/flags/gr.png" },
		{ 0xF309, ":/flags/in.png" },
		{ 0xF30A, ":/flags/it.png" },
		{ 0xF30A, ":/flags/jp.png" },
		{ 0xF30B, ":/flags/kr.png" },
		{ 0xF30C, ":/flags/nl.png" },
		{ 0xF30D, ":/flags/no.png" },
		{ 0xF30E, ":/flags/pt.png" },
		{ 0xF30F, ":/flags/ru.png" },
		{ 0xF310, ":/flags/sw.png" },
		{ 0xF311, ":/flags/uk.png" },
		{ 0xF312, ":/flags/us.png" },
		{ 0xF313, ":/flags/wr.png" }
	};

	substituableChars = defaultMap;

	auto paths = ResourceManager::getInstance()->getResourcePaths();
	std::reverse(paths.begin(), paths.end());

	for (auto testPath : paths)
	{
		auto fontoverrides = Utils::FileSystem::combine(testPath, "fontoverrides");
		for (auto file : Utils::FileSystem::getDirectoryFiles(fontoverrides))
		{
			if (file.directory || file.hidden)
				continue;

			if (Utils::String::toLower(Utils::FileSystem::getExtension(file.path)) != ".png")
				continue;

			auto stem = Utils::FileSystem::getStem(file.path);

			auto val = Utils::String::fromHexString(stem);
			if (val >= 0xF000)
			{
				substituableChars.erase(val);
				substituableChars.insert(std::pair<unsigned int, std::string>(val, file.path));
			}
		}
	}
}