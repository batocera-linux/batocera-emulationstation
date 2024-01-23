#include "renderers/Renderer.h"

#include "Renderer_GL21.h"
#include "Renderer_GLES10.h"
#include "Renderer_GLES20.h"

#include "math/Transform4x4f.h"
#include "math/Vector2i.h"
#include "resources/ResourceManager.h"
#include "ImageIO.h"
#include "Log.h"
#include "Settings.h"
#include "utils/StringUtil.h"

#include <SDL.h>
#include <stack>

#if WIN32
#include <Windows.h>
#include <SDL_syswm.h>

#include <dwmapi.h>
#pragma comment(lib, "Dwmapi.lib")

#include "utils/Platform.h"
#endif

namespace Renderer
{
	static std::stack<Rect> clipStack;
	static std::stack<Rect> nativeClipStack;

	static SDL_Window*      sdlWindow          = nullptr;
	static int              windowWidth        = 0;
	static int              windowHeight       = 0;
	static int              screenWidth        = 0;
	static int              screenHeight       = 0;
	static int              screenOffsetX      = 0;
	static int              screenOffsetY      = 0;
	static int              screenRotate       = 0;
	static bool             initialCursorState = 1;
	static Vector2i         screenMargin;
	static Rect				viewPort;

	static Vector2i			sdlWindowPosition = Vector2i(SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED);

	static int              currentFrame = 0;

	int  getCurrentFrame() { return currentFrame; }

	static Rect screenToviewport(const Rect& rect)
	{
		Rect rc = rect;

		float dx = (screenWidth - 2 * screenMargin.x()) / (float)screenWidth;
		float dy = (screenHeight - 2 * screenMargin.y()) / (float)screenHeight;

		rc.x = screenMargin.x() + rc.x * dx;
		rc.y = screenMargin.y() + rc.y * dy;
		rc.w = (float) rc.w * dx;
		rc.h = (float) rc.h * dy;

		return rc;
	}

	Rect		getScreenRect(const Transform4x4f& transform, const Vector3f& size, bool viewPort)
	{
		return getScreenRect(transform, Vector2f(size.x(), size.y()), viewPort);
	}


	Rect		getScreenRect(const Transform4x4f& transform, const Vector2f& size, bool viewPort)
	{
		auto rc = Rect(
			(int)Math::round(transform.translation().x()),
			(int)Math::round(transform.translation().y()),
			(int)Math::round(size.x() * transform.r0().x()),
			(int)Math::round(size.y() * transform.r1().y()));

		if (viewPort && screenMargin.x() != 0 && screenMargin.y() != 0)
			return screenToviewport(rc);

		return rc;
	}

	Vector2i  physicalScreenToRotatedScreen(int x, int y)
	{
		int mx = x;
		int my = y;

		switch (Renderer::getScreenRotate())
		{
		case 1:
			mx = Renderer::getScreenHeight() - mx;
			std::swap(mx, my);
			break;
		case 2:
			mx = Renderer::getScreenWidth() - mx;
			my = Renderer::getScreenHeight() - my;
			break;
		case 3:
			my = Renderer::getScreenWidth() - my;
			std::swap(mx, my);
			break;
		}

		return Vector2i(mx, my);
	}

	static void setIcon()
	{
		size_t                     width   = 0;
		size_t                     height  = 0;
		ResourceData               resData = ResourceManager::getInstance()->getFileData(":/window_icon_256.png");
		unsigned char*             rawData = ImageIO::loadFromMemoryRGBA32(resData.ptr.get(), resData.length, width, height);

		if(rawData != nullptr)
		{
			ImageIO::flipPixelsVert(rawData, width, height);

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			unsigned int rmask = 0xFF000000;
			unsigned int gmask = 0x00FF0000;
			unsigned int bmask = 0x0000FF00;
			unsigned int amask = 0x000000FF;
#else
			unsigned int rmask = 0x000000FF;
			unsigned int gmask = 0x0000FF00;
			unsigned int bmask = 0x00FF0000;
			unsigned int amask = 0xFF000000;
#endif
			// try creating SDL surface from logo data
			SDL_Surface* logoSurface = SDL_CreateRGBSurfaceFrom((void*)rawData, (int)width, (int)height, 32, (int)(width * 4), rmask, gmask, bmask, amask);
			
			if(logoSurface != nullptr)
			{
				SDL_SetWindowIcon(sdlWindow, logoSurface);
				SDL_FreeSurface(logoSurface);
			}

			delete[] rawData;
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

		static SDL_DisplayMode dispMode;

		initialCursorState = (SDL_ShowCursor(0) != 0);
		if (windowWidth == 0)
		{
			SDL_GetDesktopDisplayMode(0, &dispMode);
		}

		windowWidth   = Settings::getInstance()->getInt("WindowWidth")   ? Settings::getInstance()->getInt("WindowWidth")   : dispMode.w;
		windowHeight  = Settings::getInstance()->getInt("WindowHeight")  ? Settings::getInstance()->getInt("WindowHeight")  : dispMode.h;
		screenWidth   = Settings::getInstance()->getInt("ScreenWidth")   ? Settings::getInstance()->getInt("ScreenWidth")   : windowWidth;
		screenHeight  = Settings::getInstance()->getInt("ScreenHeight")  ? Settings::getInstance()->getInt("ScreenHeight")  : windowHeight;
		screenOffsetX = Settings::getInstance()->getInt("ScreenOffsetX") ? Settings::getInstance()->getInt("ScreenOffsetX") : 0;
		screenOffsetY = Settings::getInstance()->getInt("ScreenOffsetY") ? Settings::getInstance()->getInt("ScreenOffsetY") : 0;
		screenRotate  = Settings::getInstance()->getInt("ScreenRotate")  ? Settings::getInstance()->getInt("ScreenRotate")  : 0;

		if (screenRotate == 1 || screenRotate == 3)
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

				if (!Settings::getInstance()->getBool("Windowed") && !Settings::getInstance()->getBool("WindowWidth"))
				{
					windowWidth = screenWidth = rc.w;
					windowHeight = screenHeight = rc.h;

					if (screenRotate == 1 || screenRotate == 3)
					{
						int tmp = screenWidth;
						screenWidth = screenHeight;
						screenHeight = tmp;
					}
				}

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

		createContext();
		setIcon();
		setSwapInterval();
		swapBuffers();

#if WIN32
		if (windowFlags & SDL_WINDOW_BORDERLESS)
		{
			int x;
			int y;
			SDL_GetWindowPosition(sdlWindow, &x, &y);

			SDL_SetWindowBordered(sdlWindow, SDL_bool::SDL_TRUE);
			
			SDL_SysWMinfo wmInfo;
			SDL_VERSION(&wmInfo.version);
			SDL_GetWindowWMInfo(sdlWindow, &wmInfo);
			HWND hWnd = wmInfo.info.win.window;
			if (hWnd != NULL)
			{
				LONG lStyle = GetWindowLong(hWnd, GWL_STYLE);
				lStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
				SetWindowLong(hWnd, GWL_STYLE, lStyle);

				LONG lExStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
				lExStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
				SetWindowLong(hWnd, GWL_EXSTYLE, lExStyle);

				// Check if the major and minor versions indicate Windows 11 or newer
				if (Utils::Platform::isWindows11()) 
				{
					BOOL isComposited;
					HRESULT result = ::DwmIsCompositionEnabled(&isComposited);
					if (SUCCEEDED(result) && isComposited)
					{
						int preference = 1; // DWM_WINDOW_CORNER_PREFERENCE::DWMWCP_DONOTROUND;
						HRESULT ret = ::DwmSetWindowAttribute(hWnd, 33, &preference, sizeof(preference)); // DWMWA_WINDOW_CORNER_PREFERENCE
					}
				}

				SetWindowPos(hWnd, NULL,
					x, y,
					windowWidth, windowHeight,
					SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOOWNERZORDER);
			}
			else
			{
				SDL_SetWindowBordered(sdlWindow, SDL_bool::SDL_FALSE);
				SDL_SetWindowPosition(sdlWindow, x, y);
			}
		}
#endif

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

		destroyContext();

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


	void updateProjection()
	{
		Transform4x4f projection = Transform4x4f::Identity();
		Rect          viewport;

		switch(screenRotate)
		{
			case 0:
			{
				viewport.x = screenOffsetX + screenMargin.x();
				viewport.y = screenOffsetY + screenMargin.y();
				viewport.w = screenWidth - 2 * screenMargin.x();
				viewport.h = screenHeight - 2 * screenMargin.y();

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
	}

	Vector2i	setScreenMargin(int marginX, int marginY)
	{
		auto oldMargin = screenMargin;

		if (screenMargin.x() == marginX && screenMargin.y() == marginY)
			return oldMargin;
		
		screenMargin = Vector2i(marginX, marginY);

		Rect          viewport;
		viewport.x = screenOffsetX + screenMargin.x();
		viewport.y = screenOffsetY + screenMargin.y();
		viewport.w = screenWidth - 2 * screenMargin.x();
		viewport.h = screenHeight - 2 * screenMargin.y();

		setViewport(viewport);
		return oldMargin;
	}


	bool init()
	{
		if (!createWindow())
			return false;

		updateProjection();
		swapBuffers();
		return true;
	}

	void deinit()
	{
		destroyWindow();

	} // deinit

	void pushClipRect(const Vector2i& _pos, const Vector2i& _size)
	{
		pushClipRect(_pos.x(), _pos.y(), _size.x(), _size.y());
	}

	void pushClipRect(Rect rect)
	{
		pushClipRect(rect.x, rect.y, rect.w, rect.h);
	}

	void pushClipRect(int x, int y, int w, int h)	
	{
		Rect box(x, y, w, h);

		if(box.w == 0) box.w = screenWidth  - box.x;
		if(box.h == 0) box.h = screenHeight - box.y;

		switch(screenRotate)
		{
			case 0: { box = Rect(screenOffsetX + box.x,                       screenOffsetY + box.y,                        box.w, box.h); } break;
			case 1: { box = Rect(windowWidth - screenOffsetY - box.y - box.h, screenOffsetX + box.x,                        box.h, box.w); } break;
			case 2: { box = Rect(windowWidth - screenOffsetX - box.x - box.w, windowHeight - screenOffsetY - box.y - box.h, box.w, box.h); } break;
			case 3: { box = Rect(screenOffsetY + box.y,                       windowHeight - screenOffsetX - box.x - box.w, box.h, box.w); } break;
		}

		// make sure the box fits within clipStack.top(), and clip further accordingly
		if(clipStack.size())
		{
			const Rect& top = clipStack.top();
			if (top.x > box.x)
			{
				box.w += (box.x - top.x);
				box.x = top.x;
			}
			if (top.y > box.y)
			{
				box.h += (box.y - top.y);
				box.y = top.y;				
			}
			if((top.x + top.w) < (box.x + box.w)) 
				box.w = (top.x + top.w) - box.x;
			if((top.y + top.h) < (box.y + box.h)) 
				box.h = (top.y + top.h) - box.y;
		}

		if(box.w < 0) box.w = 0;
		if(box.h < 0) box.h = 0;

		clipStack.push(box);
		nativeClipStack.push(Rect(x, y, w, h));

		if (screenMargin.x() != 0 && screenMargin.y() != 0)
			box = screenToviewport(box);

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

		if (clipStack.empty())
		{
			static 	Rect EmptyRect = Rect(0, 0, 0, 0);
			setScissor(EmptyRect);
		}
		else
		{
			Rect box = clipStack.top();

			if (screenMargin.x() != 0 && screenMargin.y() != 0)
				box = screenToviewport(box);

			setScissor(box);
		}

	} // popClipRect

	void drawRect(const float _x, const float _y, const float _w, const float _h, const unsigned int _color, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
	{
		drawRect(_x, _y, _w, _h, _color, _color, true, _srcBlendFactor, _dstBlendFactor);
	} // drawRect

	void drawRect(const float _x, const float _y, const float _w, const float _h, const unsigned int _color, const unsigned int _colorEnd, bool horizontalGradient, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
	{
		const unsigned int color    = convertColor(_color);
		const unsigned int colorEnd = convertColor(_colorEnd);
		Vertex             vertices[4];

		vertices[0] = { { _x     ,_y      }, { 0.0f, 0.0f }, color };
		vertices[1] = { { _x     ,_y + _h }, { 0.0f, 0.0f }, horizontalGradient ? colorEnd : color };
		vertices[2] = { { _x + _w,_y      }, { 0.0f, 0.0f }, horizontalGradient ? color : colorEnd };
		vertices[3] = { { _x + _w,_y + _h }, { 0.0f, 0.0f }, colorEnd };

		// round vertices
		//for(int i = 0; i < 4; ++i)
		//	vertices[i].pos.round();

		bindTexture(0);
		drawTriangleStrips(vertices, 4, _srcBlendFactor, _dstBlendFactor);

	} // drawRect

	SDL_Window* getSDLWindow()     { return sdlWindow; }
	int         getWindowWidth()   { return windowWidth; }
	int         getWindowHeight()  { return windowHeight; }
	int         getScreenWidth()   { return screenWidth; }
	int         getScreenHeight()  { return screenHeight; }
	int         getScreenOffsetX() { return screenOffsetX; }
	int         getScreenOffsetY() { return screenOffsetY; }
	int         getScreenRotate()  { return screenRotate; }
	bool		isVerticalScreen() { return screenHeight > screenWidth; }

	float		getScreenProportion() 
	{ 
		if (screenHeight == 0)
			return 1.0;

		return (float) screenWidth / (float) screenHeight;
	}

	std::map<std::string, float> ratios =
	{
		{ "4/3",		   4.0f / 3.0f },
		{ "16/9",          16.0f / 9.0f },
		{ "16/10",         16.0f / 10.0f },
		{ "16/15",         16.0f / 15.0f },
		{ "21/9",          21.0f / 9.0f },
		{ "1/1",           1 / 1 },
		{ "2/1",           2.0f / 1.0f },
		{ "3/2",           3.0f / 2.0f },
		{ "3/4",           3.0f / 4.0f },
		{ "4/1",           4.0f / 1.0f },
		{ "9/16",          9.0f / 16.0f },
		{ "5/4",           5.0f / 4.0f },
		{ "6/5",           6.0f / 5.0f },
		{ "7/9",           7.0f / 9.0f },
		{ "8/3",           8.0f / 3.0f },
		{ "8/7",           8.0f / 7.0f },
		{ "19/12",         19.0f / 12.0f },
		{ "19/14",         19.0f / 14.0f },
		{ "30/17",         30.0f / 17.0f },
		{ "32/9",          32.0f / 9.0f }
	};

	std::string  getAspectRatio()
	{
		float nearDist = 9999999;
		std::string nearName = "";

		float prop = Renderer::getScreenProportion();

		for (auto ratio : ratios)
		{
			float dist = abs(prop - ratio.second);
			if (dist < nearDist)
			{
				nearDist = dist;
				nearName = ratio.first;
			}
		}

		return nearName;
	}


	bool        isSmallScreen()    
	{ 		
		return ScreenSettings::isSmallScreen();
		//return screenWidth <= 480 || screenHeight <= 480; 
	};

	bool isClippingEnabled() { return !clipStack.empty(); }

	inline bool valueInRange(int value, int min, int max)
	{
		return (value >= min) && (value <= max);
	}

	bool rectOverlap(Rect &A, Rect &B)
	{
		bool xOverlap = valueInRange(A.x, B.x, B.x + B.w) ||
			valueInRange(B.x, A.x, A.x + A.w);

		bool yOverlap = valueInRange(A.y, B.y, B.y + B.h) ||
			valueInRange(B.y, A.y, A.y + A.h);

		return xOverlap && yOverlap;
	}

	bool isVisibleOnScreen(float x, float y, float w, float h)
	{
		static Rect screen = Rect(0, 0, Renderer::getScreenWidth(), Renderer::getScreenHeight());

		if (w > 0 && x + w <= 0)
			return false;

		if (h > 0 && y + h <= 0)
			return false;
		
		if (x == screen.w || y == screen.h)
			return false;

		Rect box = Rect(x, y, w, h);

		if (!rectOverlap(box, screen))
			return false;
			
		if (clipStack.empty())
			return true;

		if (nativeClipStack.empty())
		{
			LOG(LogDebug) << "Renderer::isVisibleOnScreen used without any clip stack!";
			return true;
		}

		return rectOverlap(nativeClipStack.top(), box);
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


	//////////////////////////////////////////////////////////////////////////

	std::vector<std::string> getRendererNames()
	{
		std::vector<std::string> ret;
	
#ifdef RENDERER_GLES_20
		{
			GLES20Renderer rd;				
			ret.push_back(rd.getDriverName());
		}
#endif

#ifdef RENDERER_OPENGL_21
		{
			OpenGL21Renderer rd;
			ret.push_back(rd.getDriverName());
		}
#endif

#ifdef RENDERER_OPENGLES_10
		{
			GLES10Renderer rd;
			ret.push_back(rd.getDriverName());
		}
#endif
		return ret;
	}

	IRenderer* getRendererFromName(const std::string& name)
	{
		if (name.empty())
			return nullptr;

#ifdef RENDERER_GLES_20
		{
			GLES20Renderer rd;
			if (rd.getDriverName() == name)
				return new GLES20Renderer();
		}
#endif

#ifdef RENDERER_OPENGL_21
		{
			OpenGL21Renderer rd;
			if (rd.getDriverName() == name)
				return new OpenGL21Renderer();
		}
#endif

#ifdef RENDERER_OPENGLES_10
		{
			GLES10Renderer rd;
			if (rd.getDriverName() == name)
				return new GLES10Renderer();
		}
#endif

		return nullptr;
	}

	static IRenderer* createRenderer()
	{
		IRenderer* instance = getRendererFromName(Settings::getInstance()->getString("Renderer"));
		if (instance == nullptr)
		{
#ifdef RENDERER_GLES_20
			instance = new GLES20Renderer();
#elif RENDERER_OPENGL_21
			instance = new OpenGL21Renderer();
#elif RENDERER_OPENGLES_10
			instance = new GLES10Renderer();
#endif
		}

		return instance;
	}

	static IRenderer* _instance = nullptr;

	static inline IRenderer* Instance()
	{
		if (_instance == nullptr)
			_instance = createRenderer();

		return _instance;
	}

	//////////////////////////////////////////////////////////////////////////
	std::string getDriverName()
	{
		return Instance()->getDriverName();
	}

	std::vector<std::pair<std::string, std::string>> getDriverInformation()
	{
		return Instance()->getDriverInformation();
	}

	unsigned int getWindowFlags()
	{
		return Instance()->getWindowFlags();
	}

	void setupWindow()
	{
		return Instance()->setupWindow();
	}

	void createContext() 
	{
		Instance()->createContext();
	}

	void resetCache()
	{
		Instance()->resetCache();
	}
	
	void destroyContext()
	{
		Instance()->destroyContext();
	}

	unsigned int createTexture(const Texture::Type _type, const bool _linear, const bool _repeat, const unsigned int _width, const unsigned int _height, void* _data)
	{
		return Instance()->createTexture(_type, _linear, _repeat, _width, _height, _data);
	}

	void  destroyTexture(const unsigned int _texture)
	{
		Instance()->destroyTexture(_texture);
	}

	void updateTexture(const unsigned int _texture, const Texture::Type _type, const unsigned int _x, const unsigned _y, const unsigned int _width, const unsigned int _height, void* _data)
	{
		Instance()->updateTexture(_texture, _type, _x, _y, _width, _height, _data);
	}

	void bindTexture(const unsigned int _texture)
	{
		Instance()->bindTexture(_texture);
	}

	void drawLines(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
	{
		Instance()->drawLines(_vertices, _numVertices, _srcBlendFactor, _dstBlendFactor);
	}

	void drawTriangleStrips(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor, bool verticesChanged)
	{
		Instance()->drawTriangleStrips(_vertices, _numVertices, _srcBlendFactor, _dstBlendFactor, verticesChanged);
	}

	void drawSolidRectangle(const float _x, const float _y, const float _w, const float _h, const unsigned int _fillColor, const unsigned int _borderColor, float borderWidth, float cornerRadius)
	{
		Instance()->drawSolidRectangle(_x, _y, _w, _h, _fillColor, _borderColor, borderWidth, cornerRadius);
	}

	void drawTriangleFan(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
	{
		Instance()->drawTriangleFan(_vertices, _numVertices, _srcBlendFactor, _dstBlendFactor);
	}

	bool shaderSupportsCornerSize(const std::string& shader)
	{
		return Instance()->shaderSupportsCornerSize(shader);
	}

	bool supportShaders()
	{
		return Instance()->supportShaders();
	}

	void setProjection(const Transform4x4f& _projection)
	{
		Instance()->setProjection(_projection);
	}

	void setMatrix(const Transform4x4f& _matrix)
	{
		Instance()->setMatrix(_matrix);
	}

	void blurBehind(const float _x, const float _y, const float _w, const float _h, const float blurSize)
	{
		std::map<std::string, std::string> map;
		map["blur"] = std::to_string(blurSize);
		Instance()->postProcessShader(":/shaders/blur.glsl", _x, _y, _w, _h, map);		
	}

	void postProcessShader(const std::string& path, const float _x, const float _y, const float _w, const float _h, const std::map<std::string, std::string>& parameters, unsigned int* data)
	{
		Instance()->postProcessShader(path, _x, _y, _w, _h, parameters, data);
	}

	Rect& getViewport()
	{
		return viewPort;
	}

	void setViewport(const Rect& _viewport)
	{
		viewPort = _viewport;
		Instance()->setViewport(_viewport);
	}

	void setScissor(const Rect& _scissor)
	{
		Instance()->setScissor(_scissor);
	}

	void setStencil(const Vertex* _vertices, const unsigned int _numVertices)
	{
		Instance()->setStencil(_vertices, _numVertices);
	}

	void disableStencil()
	{
		Instance()->disableStencil();
	}

	void setSwapInterval()
	{
		Instance()->setSwapInterval();
	}

	void swapBuffers() 
	{
		currentFrame++;
		Instance()->swapBuffers();
	}

	size_t getTotalMemUsage()
	{
		return Instance()->getTotalMemUsage();
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool  ScreenSettings::isSmallScreen()
	{
		auto menus = Settings::getInstance()->getString("ForceSmallScreen");
		if (!menus.empty())
			return menus == "true";

		return screenWidth <= 480 || screenHeight <= 480;
	}

	bool  ScreenSettings::fullScreenMenus()
	{
		auto menus = Settings::getInstance()->getString("FullScreenMenu");
		if (!menus.empty())
			return menus == "true";

		//return true;
		return isSmallScreen();
	}

	float ScreenSettings::menuFontScale()
	{
		auto scale = Settings::getInstance()->getString("MenuFontScale");
		if (!scale.empty())
		{
			auto val = Utils::String::toFloat(scale);
			if (val > 0 && val < 4)
				return val;
		}

		float sz = Math::min(Renderer::getScreenWidth(), Renderer::getScreenHeight());
		if (sz < 320)
			return 1.5f;  // GPI 320x240

		if (sz >= 320 && sz < 720) // ODROID 480x320;
			return 1.31f;

		return 1.0f;
	}

	float ScreenSettings::fontScale()
	{
		auto scale = Settings::getInstance()->getString("FontScale");
		if (!scale.empty())
		{
			auto val = Utils::String::toFloat(scale);
			if (val > 0 && val < 4)
				return val;
		}

		float sz = Math::min(Renderer::getScreenWidth(), Renderer::getScreenHeight());
		if (sz < 320) 
			return 1.5f;  // GPI 320x240

		if (sz >= 320 && sz < 720) // ODROID 480x320;
			return 1.31f;

		return 1.0f;
	}


} // Renderer::
