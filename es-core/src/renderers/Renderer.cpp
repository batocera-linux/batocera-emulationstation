#include "renderers/Renderer.h"

#include "math/Transform4x4f.h"
#include "math/Vector2i.h"
#include "math/Vector2f.h"
#include "resources/ResourceManager.h"
#include "ImageIO.h"
#include "Log.h"
#include "Settings.h"

#include <stack>

namespace Renderer
{
    static Transform4x4f mProjection;
    static Transform4x4f mMvp;
    static Vector3f       mTranslate;

    void setProjection(const Transform4x4f& _projection)
    {
        mProjection = _projection;
    } // setProjection

    void setMatrix(const Transform4x4f& _matrix)
    {
        mMvp = _matrix;
        mMvp.round();
        mTranslate = mMvp.translation();
    } // setMatrix

	static std::stack<Rect> clipStack;
	static std::stack<Rect> nativeClipStack;

	static SDL_Window*      sdlWindow          = nullptr;
	static SDL_Renderer*	sdlRenderer        = nullptr;
	static int              windowWidth        = 0;
	static int              windowHeight       = 0;
	static int              screenWidth        = 0;
	static int              screenHeight       = 0;
	static int              screenOffsetX      = 0;
	static int              screenOffsetY      = 0;
	static int              screenRotate       = 0;
	static bool             initialCursorState = 1;

	static Vector2i			sdlWindowPosition = Vector2i(SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED);

	static void setIcon()
	{
		size_t                     width   = 0;
		size_t                     height  = 0;
		ResourceData               resData = ResourceManager::getInstance()->getFileData(":/window_icon_256.png");
		SDL_Surface*               surface = ImageIO::loadSurfaceFromMemoryRGBA32(resData.ptr.get(), resData.length, width, height);

		if(surface != nullptr)
		{
            SDL_SetWindowIcon(sdlWindow, surface);
            SDL_FreeSurface(surface);
		}

	} // setIcon

	static bool createWindow()
	{
		LOG(LogInfo) << "Creating window...";

		if(SDL_Init(SDL_INIT_VIDEO) != 0)
		{
			LOG(LogError) << "Error initializing SDL!\n	" << SDL_GetError();
			return false;
		}

		initialCursorState = (SDL_ShowCursor(0) != 0);

		SDL_DisplayMode dispMode;
		SDL_GetDesktopDisplayMode(0, &dispMode);
		windowWidth   = Settings::getInstance()->getInt("WindowWidth")   ? Settings::getInstance()->getInt("WindowWidth")   : dispMode.w;
		windowHeight  = Settings::getInstance()->getInt("WindowHeight")  ? Settings::getInstance()->getInt("WindowHeight")  : dispMode.h;
		screenWidth   = Settings::getInstance()->getInt("ScreenWidth")   ? Settings::getInstance()->getInt("ScreenWidth")   : windowWidth;
		screenHeight  = Settings::getInstance()->getInt("ScreenHeight")  ? Settings::getInstance()->getInt("ScreenHeight")  : windowHeight;
		screenOffsetX = Settings::getInstance()->getInt("ScreenOffsetX") ? Settings::getInstance()->getInt("ScreenOffsetX") : 0;
		screenOffsetY = Settings::getInstance()->getInt("ScreenOffsetY") ? Settings::getInstance()->getInt("ScreenOffsetY") : 0;
		screenRotate  = Settings::getInstance()->getInt("ScreenRotate")  ? Settings::getInstance()->getInt("ScreenRotate")  : 0;
		
		/*
		if ((screenRotate == 1 || screenRotate == 3) && !Settings::getInstance()->getBool("Windowed"))
		{
			int tmp = screenWidth;
			screenWidth = screenHeight;
			screenHeight = tmp;
		}
		else */if ((screenRotate == 1 || screenRotate == 3) && Settings::getInstance()->getBool("Windowed"))
		{
			int tmp = screenWidth;
			screenWidth = screenHeight;
			screenHeight = tmp;
		}
		
		int monitorId = Settings::getInstance()->getInt("MonitorID");
		if (monitorId >= 0 && sdlWindowPosition == Vector2i(SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED))
		{
			int displays = SDL_GetNumVideoDisplays();
			if (displays > monitorId)
			{
				SDL_Rect rc;
				SDL_GetDisplayBounds(monitorId, &rc);
				
				sdlWindowPosition = Vector2i(rc.x, rc.y);

				if (Settings::getInstance()->getBool("Windowed") && (Settings::getInstance()->getInt("WindowWidth") || Settings::getInstance()->getInt("ScreenWidth")))
				{
					if (windowWidth != rc.w || windowHeight != rc.h)
					{
						sdlWindowPosition = Vector2i(
							rc.x + (rc.w - windowWidth) / 2,
							rc.y + (rc.h - windowHeight) / 2
						);
					}
				}
			/*	else
				{
					windowWidth = rc.w;
					windowHeight = rc.h;
					screenWidth = rc.w;
					screenHeight = rc.h;
				}*/
			}
		}
		
		setupWindow();

		unsigned int windowFlags = (Settings::getInstance()->getBool("Windowed") ? 0 : (Settings::getInstance()->getBool("FullscreenBorderless") ? SDL_WINDOW_BORDERLESS : SDL_WINDOW_FULLSCREEN)) | getWindowFlags();

#if WIN32
		if (Settings::getInstance()->getBool("AlwaysOnTop"))
			windowFlags |= SDL_WINDOW_ALWAYS_ON_TOP;

		windowFlags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif

		if((sdlWindow = SDL_CreateWindow("EmulationStation", sdlWindowPosition.x(), sdlWindowPosition.y(), windowWidth, windowHeight, windowFlags)) == nullptr)
		{
			LOG(LogError) << "Error creating SDL window!\n\t" << SDL_GetError();
			return false;
		}

		LOG(LogInfo) << "Created window successfully.";

		// Create renderer taking V-Sync setting into account
		if(Settings::getInstance()->getBool("VSync"))
		{
    		sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_TARGETTEXTURE | SDL_RENDERER_ACCELERATED/* | SDL_RENDERER_PRESENTVSYNC*/);
			if (sdlRenderer)
    		{
				// SDL_GL_SetSwapInterval(0) for immediate updates (no vsync, default), 
				// 1 for updates synchronized with the vertical retrace, 
				// or -1 for late swap tearing.
				// SDL_GL_SetSwapInterval returns 0 on success, -1 on error.
				// if vsync is requested, try normal vsync; if that doesn't work, try late swap tearing
				// if that doesn't work, report an error
				if(SDL_GL_SetSwapInterval(1) != 0 && SDL_GL_SetSwapInterval(-1) != 0)
					LOG(LogWarning) << "Tried to enable vsync, but failed! (" << SDL_GetError() << ")";
    		}
		}

		if (sdlRenderer == nullptr)
		{
    		sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED);
			SDL_GL_SetSwapInterval(0);
		}

        setIcon();

        return true;

	} // createWindow

	static void destroyWindow()
	{
		if (Settings::getInstance()->getBool("Windowed") && Settings::getInstance()->getInt("WindowWidth") && Settings::getInstance()->getInt("WindowHeight"))
		{
			int x; int y;
			SDL_GetWindowPosition(sdlWindow, &x, &y);
			sdlWindowPosition = Vector2i(x, y); // Save position to restore it later
		}

		SDL_DestroyRenderer(sdlRenderer);
		sdlRenderer = nullptr;

		SDL_DestroyWindow(sdlWindow);
		sdlWindow = nullptr;

		SDL_ShowCursor(initialCursorState);

		SDL_Quit();

	} // destroyWindow

	void activateWindow()
	{
		SDL_RestoreWindow(sdlWindow);
		SDL_RaiseWindow(sdlWindow);

		if (Settings::getInstance()->getBool("Windowed"))
		{
			int h; int w;
			SDL_GetWindowSize(sdlWindow, &w, &h);

			SDL_DisplayMode DM;
			SDL_GetCurrentDisplayMode(0, &DM);

			if (w == DM.w && h == DM.h)
				SDL_SetWindowPosition(sdlWindow, 0, 0);
		}
		
		SDL_SetWindowInputFocus(sdlWindow);		
	}

	// TODO move this
	static bool rendererInited = false;

	bool init()
	{
	    if (rendererInited)
	        return true;

		if(!createWindow())
			return false;

		Transform4x4f projection = Transform4x4f::Identity();
		Rect          viewport   = {0, 0, 0, 0};

		switch(screenRotate)
		{
			case 0:
			{
				viewport.x = screenOffsetX;
				viewport.y = screenOffsetY;
				viewport.w = screenWidth;
				viewport.h = screenHeight;

				projection.orthoProjection(0, screenWidth, screenHeight, 0, -1.0, 1.0);
			}
			break;

			case 1:
			{
				viewport.x = windowWidth - screenOffsetY - screenHeight;
				viewport.y = screenOffsetX;
				viewport.w = screenHeight;
				viewport.h = screenWidth;

				projection.orthoProjection(0, screenHeight, screenWidth, 0, -1.0, 1.0);
				projection.rotate((float)ES_DEG_TO_RAD(90), {0, 0, 1});
				projection.translate({0, screenHeight * -1.0f, 0});
			}
			break;

			case 2:
			{
				viewport.x = windowWidth  - screenOffsetX - screenWidth;
				viewport.y = windowHeight - screenOffsetY - screenHeight;
				viewport.w = screenWidth;
				viewport.h = screenHeight;

				projection.orthoProjection(0, screenWidth, screenHeight, 0, -1.0, 1.0);
				projection.rotate((float)ES_DEG_TO_RAD(180), {0, 0, 1});
				projection.translate({screenWidth * -1.0f, screenHeight * -1.0f, 0});
			}
			break;

			case 3:
			{
				viewport.x = screenOffsetY;
				viewport.y = windowHeight - screenOffsetX - screenWidth;
				viewport.w = screenHeight;
				viewport.h = screenWidth;

				projection.orthoProjection(0, screenHeight, screenWidth, 0, -1.0, 1.0);
				projection.rotate((float)ES_DEG_TO_RAD(270), {0, 0, 1});
				projection.translate({screenWidth * -1.0f, 0, 0});
			}
			break;
		}

		setViewport(viewport);
		setProjection(projection);
		
        swapBuffers();

        rendererInited = true;
		return true;

	} // init

	void deinit()
	{
		destroyWindow();

	} // deinit

	void pushClipRect(const Vector2i& _pos, const Vector2i& _size)
	{
		Rect box = { _pos.x(), _pos.y(), _size.x(), _size.y() };

		if(box.w == 0) box.w = screenWidth  - box.x;
		if(box.h == 0) box.h = screenHeight - box.y;

		switch(screenRotate)
		{
			case 0: { box = { screenOffsetX + box.x,                       screenOffsetY + box.y,                        box.w, box.h }; } break;
			case 1: { box = { windowWidth - screenOffsetY - box.y - box.h, screenOffsetX + box.x,                        box.h, box.w }; } break;
			case 2: { box = { windowWidth - screenOffsetX - box.x - box.w, windowHeight - screenOffsetY - box.y - box.h, box.w, box.h }; } break;
			case 3: { box = { screenOffsetY + box.y,                       windowHeight - screenOffsetX - box.x - box.w, box.h, box.w }; } break;
		}

		// make sure the box fits within clipStack.top(), and clip further accordingly
		if(clipStack.size())
		{
			const Rect& top = clipStack.top();
			if( top.x          >  box.x)          box.x = top.x;
			if( top.y          >  box.y)          box.y = top.y;
			if((top.x + top.w) < (box.x + box.w)) box.w = (top.x + top.w) - box.x;
			if((top.y + top.h) < (box.y + box.h)) box.h = (top.y + top.h) - box.y;
		}

		if(box.w < 0) box.w = 0;
		if(box.h < 0) box.h = 0;

		clipStack.push(box);
		Rect tmp = { _pos.x(), _pos.y(), _size.x(), _size.y() };
		nativeClipStack.push(tmp);

		setScissor(box);

	} // pushClipRect

	void popClipRect()
	{
		if(clipStack.empty())
		{
			LOG(LogError) << "Tried to popClipRect while the stack was empty!";
			return;
		}

		clipStack.pop();
		nativeClipStack.pop();

		if(clipStack.empty()) SDL_RenderSetClipRect(sdlRenderer, NULL);
		else                  setScissor(clipStack.top());

	} // popClipRect

	void drawRect(const float _x, const float _y, const float _w, const float _h, const unsigned int _color, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
	{
        // TODO : proper blending handling (_srcBlendFactor, _dstBlendFactor)
		
		// Round coordinates (might need to be adjusted)
		Rect rect;
		rect.x = (int)(_x + 0.5f);
		rect.y = (int)(_y + 0.5f);
		rect.w = (int)(_w + 0.5f);
		rect.h = (int)(_h + 0.5f);

		// Matrix translation
        rect.x += (int)mTranslate.x();
        rect.y += (int)mTranslate.y();

        // Convert color
		Uint8 r,g,b,a;
		a = ( _color     ) & 0xFF;
		b = (_color >>  8) & 0xFF;
		g = (_color >> 16) & 0xFF;
		r = (_color >> 24) & 0xFF;
		SDL_SetRenderDrawColor(sdlRenderer, r, g, b, a);

		// Set blending mode (TODO handle properly srcBlendFactor and dstBlendFactor)
		SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);

		// Draw
		SDL_RenderFillRect(sdlRenderer, &rect);

	} // drawRect

	void drawGradientRect(const float _x, const float _y, const float _w, const float _h, const unsigned int _color, const unsigned int _colorEnd, bool horizontalGradient, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
	{
		// Are we really drawing a gradient ?
		if (horizontalGradient==false)
		{
			drawRect(_x, _y, _w, _h, _color, _srcBlendFactor, _dstBlendFactor);
			return;
		}

		// Round coordinates (might need to be adjusted)
		Rect rect;
		rect.x = (int)(_x + 0.5f);
		rect.y = (int)(_y + 0.5f);
		rect.w = (int)(_w + 0.5f);
		rect.h = (int)(_h + 0.5f);

        // Matrix translation
        rect.x += (int)mTranslate.x();
        rect.y += (int)mTranslate.y();

        // Set blending mode (TODO handle properly srcBlendFactor and dstBlendFactor)
		SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);

		// Force renderer to scale smoothly (SDL hint)
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

		// Create a 2x1 surface with gradient colors
		Uint32 pixelData[2];
		pixelData[0] = _color;
		pixelData[1] = _colorEnd;
		SDL_Surface* sdlSurfaceGradient = SDL_CreateRGBSurfaceWithFormatFrom(pixelData, 2, 1, 32, 2*sizeof(Uint32), SDL_PIXELFORMAT_ARGB8888);

		// Create a texture from the surface, destroy surface as its' not needed anymore
		SDL_Texture* sdlTextureGradient = SDL_CreateTextureFromSurface(sdlRenderer, sdlSurfaceGradient);
		SDL_FreeSurface(sdlSurfaceGradient);

		// Copy (stretch) to current render target
		SDL_RenderCopy(sdlRenderer, sdlTextureGradient, NULL, &rect);

		// Disable high squality scaling
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

		/*
		const unsigned int color    = convertColor(_color);
		const unsigned int colorEnd = convertColor(_colorEnd);

		Vertex             vertices[4];

		vertices[0] = { { _x     ,_y      }, { 0.0f, 0.0f }, color };
		vertices[1] = { { _x     ,_y + _h }, { 0.0f, 0.0f }, horizontalGradient ? colorEnd : color };
		vertices[2] = { { _x + _w,_y      }, { 0.0f, 0.0f }, horizontalGradient ? color : colorEnd };
		vertices[3] = { { _x + _w,_y + _h }, { 0.0f, 0.0f }, colorEnd };

		// round vertices
		for(int i = 0; i < 4; ++i)
			vertices[i].pos.round();

		bindTexture(0);
		drawTriangleStrips(vertices, 4, _srcBlendFactor, _dstBlendFactor);*/

	} // drawRect

//////////////////////////////////////////////////////////////////////////

	void drawColoredLines(const SDL_Point* _points, const unsigned int _numPoints, const Uint32 _color, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
	{
		// Convert color
		Uint8 r,g,b,a;
		a = ( _color     ) & 0xFF;
		b = (_color >>  8) & 0xFF;
		g = (_color >> 16) & 0xFF;
		r = (_color >> 24) & 0xFF;
		SDL_SetRenderDrawColor(sdlRenderer, r, g, b, a);

		// Set blending mode (TODO handle properly srcBlendFactor and dstBlendFactor)
		SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);

		// Draw
		SDL_RenderDrawLines(sdlRenderer, _points, _numPoints);
	}

	/*void drawLines(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
	{
		// Pass buffer data
		GL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * _numVertices, _vertices, GL_DYNAMIC_DRAW));

		// Setup shader (always NOT textured)
		GL_CHECK_ERROR(glUseProgram(shaderProgramColorNoTexture.id));

		GL_CHECK_ERROR(glVertexAttribPointer(shaderProgramColorNoTexture.posAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, pos)));
		GL_CHECK_ERROR(glEnableVertexAttribArray(shaderProgramColorNoTexture.posAttrib));

		GL_CHECK_ERROR(glVertexAttribPointer(shaderProgramColorNoTexture.colAttrib, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(Vertex), (const void*)offsetof(Vertex, col)));
		GL_CHECK_ERROR(glEnableVertexAttribArray(shaderProgramColorNoTexture.colAttrib));

		// Do rendering
		GL_CHECK_ERROR(glEnable(GL_BLEND));
		GL_CHECK_ERROR(glBlendFunc(convertBlendFactor(_srcBlendFactor), convertBlendFactor(_dstBlendFactor)));
		GL_CHECK_ERROR(glDrawArrays(GL_LINES, 0, _numVertices));
		GL_CHECK_ERROR(glDisable(GL_BLEND));

		// Restore context
		GL_CHECK_ERROR(glDisableVertexAttribArray(shaderProgramColorNoTexture.posAttrib));
		GL_CHECK_ERROR(glDisableVertexAttribArray(shaderProgramColorNoTexture.colAttrib));

	} // drawLines*/

//////////////////////////////////////////////////////////////////////////

	SDL_Texture* createStaticTexture(const Texture::Type _type, const bool _linear, const bool _repeat, const unsigned int _width, const unsigned int _height, void* _data)
	{
        SDL_Texture* texture = nullptr;
        if (_data == nullptr)
        {
	        texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, _width, _height);
        }
        else
        {
			SDL_Surface* sdlSurfaceData = SDL_CreateRGBSurfaceWithFormatFrom(_data, _width, _height, 32, _width*sizeof(Uint32), SDL_PIXELFORMAT_ARGB8888);

			// Create a texture from the surface, destroy surface as its' not needed anymore
			texture = SDL_CreateTextureFromSurface(sdlRenderer, sdlSurfaceData);
			SDL_FreeSurface(sdlSurfaceData);
        }

        return texture;
	}

//////////////////////////////////////////////////////////////////////////

	SDL_Texture* createStreamingTexture(const Texture::Type _type, const bool _linear, const bool _repeat, const unsigned int _width, const unsigned int _height, void* _data)
	{
        SDL_Texture* texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, _width, _height);
        void* pixels = nullptr;
        int pitch;
        if (_data)
        {
        	if (SDL_LockTexture(texture, NULL, &pixels, &pitch) == 0)
        	{
        		for (int y = 0 ; y < _height ; y++)
        			memcpy(pixels+y*pitch, _data+y*_height*sizeof(Uint32), _width*sizeof(Uint32));

        		SDL_UnlockTexture(texture);
        	}
        }
        return texture;
	}

//////////////////////////////////////////////////////////////////////////

	void destroyTexture(SDL_Texture* _texture)
	{
		SDL_DestroyTexture(_texture);
	} // destroyTexture

//////////////////////////////////////////////////////////////////////////

	static SDL_Texture* boundTexture = 0;

	void bindTexture(SDL_Texture* _texture)
	{
		if (boundTexture == _texture)
			return;

		boundTexture = _texture;
	} // bindTexture

//////////////////////////////////////////////////////////////////////////

	void updateTexture(SDL_Texture* _texture, const Texture::Type _type, const unsigned int _x, const unsigned _y, const unsigned int _width, const unsigned int _height, void* _data)
	{
        Uint32* pixels = nullptr;
        Uint32* data32 = (Uint32*)_data;
        int pitch;
        if (_data)
        {
	        SDL_Rect rect = { _x, _y, _width, _height };
        	if (SDL_LockTexture(_texture, nullptr/*&rect*/, (void**)&pixels, &pitch) == 0)
        	{
        		for (int y = 0 ; y < _height ; y++)
        			memcpy(pixels+y*(pitch/sizeof(Uint32)), data32+((_y+y)*_width)+_x, _width*sizeof(Uint32));

        		SDL_UnlockTexture(_texture);
        	}
        }
	} // updateTexture

//////////////////////////////////////////////////////////////////////////

	void blit(SDL_Renderer* renderer, SDL_Texture* _texture, SDL_Rect* srcRect, SDL_Rect* dstRect, Uint32 flipFlags)
	{
        dstRect->x += (int)mTranslate.x();
        dstRect->y += (int)mTranslate.y();

	    if (flipFlags == 0)
		    SDL_RenderCopy(renderer, _texture, srcRect, dstRect);
	    else
        {
            int w, h;
            SDL_GetRendererOutputSize(renderer, &w, &h);
            SDL_RenderCopyEx(renderer, _texture, srcRect, dstRect, 0., NULL, (SDL_RendererFlip)flipFlags);
        }
	}

	SDL_Window*     getSDLWindow()      { return sdlWindow; }
    SDL_Renderer*   getWindowRenderer() { return sdlRenderer; }
    int             getWindowWidth()    { return windowWidth; }
	int             getWindowHeight()   { return windowHeight; }
	int             getScreenWidth()    { return screenWidth; }
	int             getScreenHeight()   { return screenHeight; }
	int             getScreenOffsetX()  { return screenOffsetX; }
	int             getScreenOffsetY()  { return screenOffsetY; }
	int             getScreenRotate()   { return screenRotate; }

	SDL_Renderer* createTextureRenderer()
    {
	    return sdlRenderer;
    }

	bool        isSmallScreen()    
	{ 		
		return screenWidth < 400 || screenHeight < 400; 
	};

	bool isClippingEnabled() { return !clipStack.empty(); }

	bool rectOverlap(SDL_Rect* A, SDL_Rect* B)
	{
		SDL_Rect rectInter;
		return SDL_IntersectRect(A, B, &rectInter);
	}

	bool isVisibleOnScreen(float x, float y, float w, float h)
	{
		SDL_Rect screen, box;

		if (w > 0 && x + w <= 0)
			return false;

		if (h > 0 && y + h <= 0)
			return false;

		// Fill screen rect		
		screen.x = 0;
		screen.y = 0;
		screen.w = Renderer::getScreenWidth();
		screen.h = Renderer::getScreenHeight();

		if (x == screen.w || y == screen.h)
			return false;
		
		// Fill box rect	
		// TODO Should we round here ????
		box.x = (int)x;
		box.y = (int)y;
		box.w = (int)w;
		box.h = (int)h;

		if (!rectOverlap(&box, &screen))
			return false;
			
		if (clipStack.empty())
			return true;

		if (nativeClipStack.empty())
		{
			LOG(LogDebug) << "Renderer::isVisibleOnScreen used without any clip stack!";
			return true;
		}

		screen = nativeClipStack.top();
		return rectOverlap(&screen, &box);
	}

	unsigned int mixColors(unsigned int first, unsigned int second, float percent)
	{
		unsigned char alpha0 = (first >> 24) & 0xFF;
		unsigned char blue0 = (first >> 16) & 0xFF;
		unsigned char green0 = (first >> 8) & 0xFF;
		unsigned char red0 = first & 0xFF;

		unsigned char alpha1 = (second >> 24) & 0xFF;
		unsigned char blue1 = (second >> 16) & 0xFF;
		unsigned char green1 = (second >> 8) & 0xFF;
		unsigned char red1 = second & 0xFF;

		unsigned char alpha = (unsigned char)(alpha0 * (1.0 - percent) + alpha1 * percent);
		unsigned char blue = (unsigned char)(blue0 * (1.0 - percent) + blue1 * percent);
		unsigned char green = (unsigned char)(green0 * (1.0 - percent) + green1 * percent);
		unsigned char red = (unsigned char)(red0 * (1.0 - percent) + red1 * percent);

		return (alpha << 24) | (blue << 16) | (green << 8) | red;
	}

#define ROUNDING_PIECES 8.0f

	static void addRoundCorner(float x, float y, double sa, double arc, float r, unsigned int color, float pieces, std::vector<Vertex> &vertex)
	{
		// centre of the arc, for clockwise sense
		float cent_x = x + r * Math::cosf(sa + ES_PI / 2.0f);
		float cent_y = y + r * Math::sinf(sa + ES_PI / 2.0f);

		// build up piecemeal including end of the arc
		int n = ceil(pieces * arc / ES_PI * 2.0f);

		float step = arc / (float)n;

		Vertex vx;
		vx.tex = Vector2f::Zero();
		vx.col = color;

		for (int i = 0; i <= n; i++)
		{
			float ang = sa + step * (float)i;

			// compute the next point
			float next_x = cent_x + r * Math::sinf(ang);
			float next_y = cent_y - r * Math::cosf(ang);

			vx.pos[0] = next_x;
			vx.pos[1] = next_y;
			vertex.push_back(vx);
		}
	}

	std::vector<Vertex> createRoundRect(float x, float y, float width, float height, float radius, unsigned int color)
	{
		auto finalColor = convertColor(color);
		float pieces = Math::min(3.0f, Math::max(radius / 3.0f, ROUNDING_PIECES));

		std::vector<Vertex> vertex;
		addRoundCorner(x, y + radius, 3.0f * ES_PI / 2.0f, ES_PI / 2.0f, radius, finalColor, pieces, vertex);
		addRoundCorner(x + width - radius, y, 0.0, ES_PI / 2.0f, radius, finalColor, pieces, vertex);
		addRoundCorner(x + width, y + height - radius, ES_PI / 2.0f, ES_PI / 2.0f, radius, finalColor, pieces, vertex);
		addRoundCorner(x + radius, y + height, ES_PI, ES_PI / 2.0f, radius, finalColor, pieces, vertex);
		return vertex;
	}
	
	void drawRoundRect(float x, float y, float width, float height, float radius, unsigned int color, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
	{
		bindTexture(0);

		std::vector<Vertex> vertex = createRoundRect(x, y, width, height, radius, color);
		drawTriangleFan(vertex.data(), vertex.size(), _srcBlendFactor, _dstBlendFactor);
	}

	void enableRoundCornerStencil(float x, float y, float width, float height, float radius)
	{
		std::vector<Vertex> vertex = createRoundRect(x, y, width, height, radius);
		setStencil(vertex.data(), vertex.size());
	}

	void setSwapInterval()
	{
		// vsync
		if(Settings::getInstance()->getBool("VSync"))
		{
			// SDL_GL_SetSwapInterval(0) for immediate updates (no vsync, default),
			// 1 for updates synchronized with the vertical retrace,
			// or -1 for late swap tearing.
			// SDL_GL_SetSwapInterval returns 0 on success, -1 on error.
			// if vsync is requested, try normal vsync; if that doesn't work, try late swap tearing
			// if that doesn't work, report an error
			if(SDL_GL_SetSwapInterval(1) != 0 && SDL_GL_SetSwapInterval(-1) != 0)
				LOG(LogWarning) << "Tried to enable vsync, but failed! (" << SDL_GetError() << ")";
		}
		else
			SDL_GL_SetSwapInterval(0);

	} // setSwapInterval

	void setViewport(const Rect& _viewport)
	{
		SDL_Rect viewport;
		viewport.x = _viewport.x;
		viewport.y = _viewport.y;
		viewport.w = _viewport.w;
		viewport.h = _viewport.h;
		SDL_RenderSetViewport(sdlRenderer, &viewport);
		
		// glViewport starts at the bottom left of the window
		//GL_CHECK_ERROR(glViewport( _viewport.x, getWindowHeight() - _viewport.y - _viewport.h, _viewport.w, _viewport.h));

	} // setViewport

	void swapBuffers()
	{
		SDL_RenderPresent(sdlRenderer);
        SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(sdlRenderer);
	}

	void setScissor(const Rect& _scissor)
	{
		if((_scissor.x == 0) && (_scissor.y == 0) && (_scissor.w == 0) && (_scissor.h == 0))
		{
			SDL_RenderSetClipRect(sdlRenderer, NULL);
		}
		else
		{
			SDL_Rect rect;
			rect.x = _scissor.x;
			rect.y = _scissor.y;
			rect.w = _scissor.w;
			rect.h = _scissor.h;
			SDL_RenderSetClipRect(sdlRenderer, &rect);
			//// glScissor starts at the bottom left of the window
			//GL_CHECK_ERROR(glScissor(_scissor.x, getWindowHeight() - _scissor.y - _scissor.h, _scissor.w, _scissor.h));
			//GL_CHECK_ERROR(glEnable(GL_SCISSOR_TEST));
		}

	} // setScissor

} // Renderer::
