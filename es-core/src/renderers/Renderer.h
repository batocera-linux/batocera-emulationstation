#pragma once
#ifndef ES_CORE_RENDERER_RENDERER_H
#define ES_CORE_RENDERER_RENDERER_H

#include <vector>
#include <map>
#include <cstring>
#include "math/Vector2f.h"
#include "math/Vector3f.h"

class  Transform4x4f;
class  Vector2i;
struct SDL_Window;

namespace Renderer
{
	namespace Blend
	{
		enum Factor
		{
			ZERO                = 0,
			ONE                 = 1,
			SRC_COLOR           = 2,
			ONE_MINUS_SRC_COLOR = 3,
			SRC_ALPHA           = 4,
			ONE_MINUS_SRC_ALPHA = 5,
			DST_COLOR           = 6,
			ONE_MINUS_DST_COLOR = 7,
			DST_ALPHA           = 8,
			ONE_MINUS_DST_ALPHA = 9

		}; // Factor

	} // Blend::

	namespace Texture
	{
		enum Type
		{
			RGBA  = 0,
			ALPHA = 1

		}; // Type

	} // Texture::

	struct Rect
	{
		Rect() : x(0), y(0), w(0), h(0) { }
		Rect(const int _x, const int _y, const int _w, const int _h) : x(_x), y(_y), w(_w), h(_h) { }

		int x;
		int y;
		int w;
		int h;

		inline bool contains(int px, int py) { return px >= x && px <= x + w && py >= y && py <= y + h; };

	}; // Rect

	struct ShaderInfo
	{
		std::string path;
		std::map<std::string, std::string> parameters;
	};

	struct Vertex
	{
		Vertex() 
			: col(0)			
			, saturation(1.0f)
			, cornerRadius(0.0f)
			, customShader(nullptr)
		{

		}

		Vertex(const Vector2f& _pos, const Vector2f& _tex, const unsigned int _col) 
			: pos(_pos)
			, tex(_tex)
			, col(_col) 
			, saturation(1.0f)
			, cornerRadius(0.0f)
			, customShader(nullptr)
		{ 

		}

		Vector2f     pos;
		Vector2f     tex;
		unsigned int col;

		float saturation;
		float cornerRadius;
		ShaderInfo* customShader;

	}; // Vertex

	class IRenderer
	{
	public:
		virtual std::string getDriverName() = 0;

		virtual std::vector<std::pair<std::string, std::string>> getDriverInformation() = 0;

		virtual unsigned int getWindowFlags() = 0;
		virtual void         setupWindow() = 0;

		virtual void         createContext() = 0;
		virtual void         destroyContext() = 0;

		virtual void         resetCache() = 0;

		virtual unsigned int createTexture(const Texture::Type _type, const bool _linear, const bool _repeat, const unsigned int _width, const unsigned int _height, void* _data) = 0;
		virtual void         destroyTexture(const unsigned int _texture) = 0;
		virtual void         updateTexture(const unsigned int _texture, const Texture::Type _type, const unsigned int _x, const unsigned _y, const unsigned int _width, const unsigned int _height, void* _data) = 0;
		virtual void         bindTexture(const unsigned int _texture) = 0;

		virtual void         drawLines(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor = Blend::SRC_ALPHA, const Blend::Factor _dstBlendFactor = Blend::ONE_MINUS_SRC_ALPHA) = 0;
		virtual void         drawTriangleStrips(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor = Blend::SRC_ALPHA, const Blend::Factor _dstBlendFactor = Blend::ONE_MINUS_SRC_ALPHA, bool verticesChanged = true) = 0;
		virtual void		 drawTriangleFan(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor = Blend::SRC_ALPHA, const Blend::Factor _dstBlendFactor = Blend::ONE_MINUS_SRC_ALPHA) = 0;

		virtual void		 drawSolidRectangle(const float _x, const float _y, const float _w, const float _h, const unsigned int _fillColor, const unsigned int _borderColor, float borderWidth = 1, float cornerRadius = 0) = 0;

		virtual void         setProjection(const Transform4x4f& _projection) = 0;
		virtual void         setMatrix(const Transform4x4f& _matrix) = 0;
		virtual void         setViewport(const Rect& _viewport) = 0;
		virtual void         setScissor(const Rect& _scissor) = 0;

		virtual void         setStencil(const Vertex* _vertices, const unsigned int _numVertices) = 0;
		virtual void		 disableStencil() = 0;

		virtual void         setSwapInterval() = 0;
		virtual void         swapBuffers() = 0;
		
		virtual void		 postProcessShader(const std::string& path, const float _x, const float _y, const float _w, const float _h, const std::map<std::string, std::string>& parameters, unsigned int* data = nullptr) { };

		virtual size_t		 getTotalMemUsage() { return (size_t) -1; };

		virtual bool		 supportShaders() { return false; }
		virtual bool		 shaderSupportsCornerSize(const std::string& shader) { return false; };
	};
	
	class ScreenSettings
	{
	public:
		static bool  isSmallScreen();
		static bool  fullScreenMenus();
		static float fontScale();
		static float menuFontScale();
	};


	std::vector<std::string> getRendererNames();

 	bool        init            ();
 	void        deinit          ();
	void        pushClipRect    (const Vector2i& _pos, const Vector2i& _size);
	void		pushClipRect	(int x, int y, int w, int h);
	void		pushClipRect	(Rect rect);
	void        popClipRect     ();
	void        drawRect        (const float _x, const float _y, const float _w, const float _h, const unsigned int _color, const Blend::Factor _srcBlendFactor = Blend::SRC_ALPHA, const Blend::Factor _dstBlendFactor = Blend::ONE_MINUS_SRC_ALPHA);
	void        drawRect        (const float _x, const float _y, const float _w, const float _h, const unsigned int _color, const unsigned int _colorEnd, bool horizontalGradient = false, const Blend::Factor _srcBlendFactor = Blend::SRC_ALPHA, const Blend::Factor _dstBlendFactor = Blend::ONE_MINUS_SRC_ALPHA);

	SDL_Window* getSDLWindow    ();
	int         getWindowWidth  ();
	int         getWindowHeight ();
	int         getScreenWidth  ();
	int         getScreenHeight ();
	int         getScreenOffsetX();
	int         getScreenOffsetY();
	int         getScreenRotate ();
	float		getScreenProportion();
	std::string getAspectRatio();
	bool		isVerticalScreen();

	Vector2i    physicalScreenToRotatedScreen(int x, int y);

	// API specific
	inline static unsigned int convertColor (const unsigned int _color) { return ((_color & 0xFF000000) >> 24) | ((_color & 0x00FF0000) >> 8) | ((_color & 0x0000FF00) << 8) | ((_color & 0x000000FF) << 24); } 
	// convertColor

	unsigned int getWindowFlags    ();
	void         setupWindow       ();
	void         createContext     ();
	void         destroyContext    ();
	void         resetCache        ();
	unsigned int createTexture     (const Texture::Type _type, const bool _linear, const bool _repeat, const unsigned int _width, const unsigned int _height, void* _data);
	void         destroyTexture    (const unsigned int _texture);
	void         updateTexture     (const unsigned int _texture, const Texture::Type _type, const unsigned int _x, const unsigned _y, const unsigned int _width, const unsigned int _height, void* _data);
	void         bindTexture       (const unsigned int _texture);
	void         drawLines         (const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor = Blend::SRC_ALPHA, const Blend::Factor _dstBlendFactor = Blend::ONE_MINUS_SRC_ALPHA);
	void         drawTriangleStrips(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor = Blend::SRC_ALPHA, const Blend::Factor _dstBlendFactor = Blend::ONE_MINUS_SRC_ALPHA, bool verticesChanged = true);
	void		 drawSolidRectangle(const float _x, const float _y, const float _w, const float _h, const unsigned int _fillColor, const unsigned int _borderColor, float borderWidth = 1, float cornerRadius = 0);
	void         setProjection     (const Transform4x4f& _projection);
	void         setMatrix         (const Transform4x4f& _matrix);
	void         setViewport       (const Rect& _viewport);
	Rect&        getViewport	   ();
	void         setScissor        (const Rect& _scissor);
	void         setSwapInterval   ();
	void         swapBuffers       ();

	void		 blurBehind		   (const float _x, const float _y, const float _w, const float _h, const float blurSize = 4.0f);
	void		 postProcessShader (const std::string& path, const float _x, const float _y, const float _w, const float _h, const std::map<std::string, std::string>& parameters, unsigned int* data = nullptr);

	size_t		 getTotalMemUsage  ();

	bool		 supportShaders();
	bool		 shaderSupportsCornerSize(const std::string& shader);

	std::string  getDriverName();
	std::vector<std::pair<std::string, std::string>> getDriverInformation();

	bool         isClippingEnabled  ();
	bool         isVisibleOnScreen  (float x, float y, float w, float h);
	inline bool  isVisibleOnScreen  (const Rect& rect) { return isVisibleOnScreen(rect.x, rect.y, rect.w, rect.h); }
	bool         isSmallScreen      ();
	unsigned int mixColors(unsigned int first, unsigned int second, float percent);

	void		drawRoundRect(float x, float y, float w, float h, float radius, unsigned int color, const Blend::Factor _srcBlendFactor = Blend::SRC_ALPHA, const Blend::Factor _dstBlendFactor = Blend::ONE_MINUS_SRC_ALPHA);
	void		enableRoundCornerStencil(float x, float y, float size_x, float size_y, float radius);
	
	void		drawTriangleFan(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor = Blend::SRC_ALPHA, const Blend::Factor _dstBlendFactor = Blend::ONE_MINUS_SRC_ALPHA);

	void		setStencil(const Vertex* _vertices, const unsigned int _numVertices);
	void		disableStencil();

	std::vector<Vertex> createRoundRect(float x, float y, float width, float height, float radius, unsigned int color = 0xFFFFFFFF);

	Rect		getScreenRect(const Transform4x4f& transform, const Vector3f& size, bool viewPort = false);
	Rect		getScreenRect(const Transform4x4f& transform, const Vector2f& size, bool viewPort = false);
	
	void		activateWindow();

	Vector2i	setScreenMargin(int marginX, int marginY);

	int         getCurrentFrame();

} // Renderer::

#endif // ES_CORE_RENDERER_RENDERER_H
