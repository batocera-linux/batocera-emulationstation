#if defined(USE_OPENGL_21_OLD)

#include "renderers/Renderer.h"
#include "math/Transform4x4f.h"
#include "Log.h"
#include "Settings.h"

#include <SDL_opengl.h>
#include <SDL.h>
#include <vector>

namespace Renderer
{
	static SDL_GLContext sdlContext = nullptr;

	static GLenum convertBlendFactor(const Blend::Factor _blendFactor)
	{
		switch(_blendFactor)
		{
			case Blend::ZERO:                { return GL_ZERO;                } break;
			case Blend::ONE:                 { return GL_ONE;                 } break;
			case Blend::SRC_COLOR:           { return GL_SRC_COLOR;           } break;
			case Blend::ONE_MINUS_SRC_COLOR: { return GL_ONE_MINUS_SRC_COLOR; } break;
			case Blend::SRC_ALPHA:           { return GL_SRC_ALPHA;           } break;
			case Blend::ONE_MINUS_SRC_ALPHA: { return GL_ONE_MINUS_SRC_ALPHA; } break;
			case Blend::DST_COLOR:           { return GL_DST_COLOR;           } break;
			case Blend::ONE_MINUS_DST_COLOR: { return GL_ONE_MINUS_DST_COLOR; } break;
			case Blend::DST_ALPHA:           { return GL_DST_ALPHA;           } break;
			case Blend::ONE_MINUS_DST_ALPHA: { return GL_ONE_MINUS_DST_ALPHA; } break;
			default:                         { return GL_ZERO;                }
		}

	} // convertBlendFactor

	static GLenum convertTextureType(const Texture::Type _type)
	{
		switch(_type)
		{
			case Texture::RGBA:  { return GL_RGBA;  } break;
			case Texture::ALPHA: { return GL_ALPHA; } break;
			default:             { return GL_ZERO;  }
		}

	} // convertTextureType

	unsigned int convertColor(const unsigned int _color)
	{
		// convert from rgba to abgr
		unsigned char r = ((_color & 0xff000000) >> 24) & 255;
		unsigned char g = ((_color & 0x00ff0000) >> 16) & 255;
		unsigned char b = ((_color & 0x0000ff00) >>  8) & 255;
		unsigned char a = ((_color & 0x000000ff)      ) & 255;

		return ((a << 24) | (b << 16) | (g << 8) | (r));

	} // convertColor

	unsigned int getWindowFlags()
	{
		return SDL_WINDOW_OPENGL;

	} // getWindowFlags

	void setupWindow()
	{
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

		SDL_GL_SetAttribute(SDL_GL_RED_SIZE,     8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,   8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,    8);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,  24);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

		// Antialias : Not supported on every machine
		// SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		// SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 2);

	} // setupWindow

	std::vector<std::pair<std::string, std::string>> getDriverInformation()
	{
		std::vector<std::pair<std::string, std::string>> info;

		const std::string vendor = glGetString(GL_VENDOR) ? (const char*)glGetString(GL_VENDOR) : "";
		if (!vendor.empty())
			info.push_back(std::pair<std::string, std::string>("GL VENDOR", vendor));

		const std::string renderer = glGetString(GL_RENDERER) ? (const char*)glGetString(GL_RENDERER) : "";
		if (!renderer.empty())
			info.push_back(std::pair<std::string, std::string>("GL RENDERER", renderer));

		const std::string version = glGetString(GL_VERSION) ? (const char*)glGetString(GL_VERSION) : "";
		if (!version.empty())
			info.push_back(std::pair<std::string, std::string>("GL VERSION", version));

		return info;
	}

	void createContext()
	{
		sdlContext = SDL_GL_CreateContext(getSDLWindow());
		SDL_GL_MakeCurrent(getSDLWindow(), sdlContext);

		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

		std::string glExts = (const char*)glGetString(GL_EXTENSIONS);
		LOG(LogInfo) << "Checking available OpenGL extensions...";
		LOG(LogInfo) << " ARB_texture_non_power_of_two: " << (glExts.find("ARB_texture_non_power_of_two") != std::string::npos ? "ok" : "MISSING");

	} // createContext

	void destroyContext()
	{
		SDL_GL_DeleteContext(sdlContext);
		sdlContext = nullptr;

	} // destroyContext

	unsigned int createTexture(const Texture::Type _type, const bool _linear, const bool _repeat, const unsigned int _width, const unsigned int _height, void* _data)
	{
		const GLenum type = convertTextureType(_type);
		unsigned int texture;

		glGenTextures(1, &texture);
		if (glGetError() != GL_NO_ERROR)
			return 0;

		bindTexture(texture);

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, _repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, _repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, _linear ? GL_LINEAR : GL_NEAREST);

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glTexImage2D(GL_TEXTURE_2D, 0, type, _width, _height, 0, type, GL_UNSIGNED_BYTE, _data);

		if (glGetError() != GL_NO_ERROR)
		{
			glDeleteTextures(1, &texture);
			return 0;
		}

		return texture;

	} // createTexture
	
	void destroyTexture(const unsigned int _texture)
	{
		glDeleteTextures(1, &_texture);

	} // destroyTexture

	void updateTexture(const unsigned int _texture, const Texture::Type _type, const unsigned int _x, const unsigned _y, const unsigned int _width, const unsigned int _height, void* _data)
	{
		glBindTexture(GL_TEXTURE_2D, _texture);

		if (_x == -1 && _y == -1)
		{
			const GLenum type = convertTextureType(_type);
			glTexImage2D(GL_TEXTURE_2D, 0, type, _width, _height, 0, type, GL_UNSIGNED_BYTE, _data);
		}
		else 
			glTexSubImage2D(GL_TEXTURE_2D, 0, _x, _y, _width, _height, convertTextureType(_type), GL_UNSIGNED_BYTE, _data);

		glBindTexture(GL_TEXTURE_2D, 0);

	} // updateTexture

	static unsigned int boundTexture = 0;

	void bindTexture(const unsigned int _texture)
	{
		if (boundTexture == _texture)
			return;

		boundTexture = _texture;

		glBindTexture(GL_TEXTURE_2D, _texture);

		if(_texture == 0) glDisable(GL_TEXTURE_2D);
		else              glEnable(GL_TEXTURE_2D);

	} // bindTexture

	void drawLines(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
	{
		glEnable(GL_BLEND);
		glBlendFunc(convertBlendFactor(_srcBlendFactor), convertBlendFactor(_dstBlendFactor));

		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		glVertexPointer(  2, GL_FLOAT,         sizeof(Vertex), &_vertices[0].pos);
		glTexCoordPointer(2, GL_FLOAT,         sizeof(Vertex), &_vertices[0].tex);
		glColorPointer(   4, GL_UNSIGNED_BYTE, sizeof(Vertex), &_vertices[0].col);

		glDrawArrays(GL_LINES, 0, _numVertices);

		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);

		glDisable(GL_BLEND);

	} // drawLines

	void setGrayscale()
	{
		glMatrixMode(GL_COLOR);

		GLfloat grayScale[16] =
		{
			.3f, .3f, .3f, 0.0f,
			.59f, .59f, .59f, 0.0f,
			.11f, .11f, .11f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};
		glLoadMatrixf(grayScale);
	} // setGrayscale


	struct Vertex4f
	{
		Vertex4f() { }
		Vertex4f(const Vector2f& _pos, const Vector2f& _tex, const unsigned int _col) : pos(_pos), tex(_tex), col(_col) { }

		Vector3f		pos;
		Vector4f		tex;

		unsigned int col;

	}; // Vertex

	static void correctQuad(Vertex4f& v1, Vertex4f& v2, Vertex4f& v3, Vertex4f& v4)
	{
		// detects intersection of two diagonal lines
		float divisor = (v4.pos.y() - v3.pos.y()) * (v2.pos.x() - v1.pos.x()) - (v4.pos.x() - v3.pos.x()) * (v2.pos.y() - v1.pos.y());
		float ua = ((v4.pos.x() - v3.pos.x()) * (v1.pos.y() - v3.pos.y()) - (v4.pos.y() - v3.pos.y()) * (v1.pos.x() - v3.pos.x())) / divisor;
		float ub = ((v2.pos.x() - v1.pos.x()) * (v1.pos.y() - v3.pos.y()) - (v2.pos.y() - v1.pos.y()) * (v1.pos.x() - v3.pos.x())) / divisor;

		// calculates the intersection point
		float centerX = v1.pos.x() + ua * (v2.pos.x() - v1.pos.x());
		float centerY = v1.pos.y() + ub * (v2.pos.y() - v1.pos.y());
		Vector3f center(v2.pos.x() - centerX, v2.pos.y() - centerY, 0.5f);

		float d1 = Vector3f(v1.pos - center).length();
		float d2 = Vector3f(v2.pos - center).length();
		float d3 = Vector3f(v3.pos - center).length();
		float d4 = Vector3f(v4.pos - center).length();
	
		// calculates quotients used as w component in uvw texture mapping
		v1.tex *= isnan(d2) || d2 == 0.0f ? 1.0f : (d1 + d2) / d2;
		v2.tex *= isnan(d1) || d1 == 0.0f ? 1.0f : (d2 + d1) / d1;
		v3.tex *= isnan(d4) || d4 == 0.0f ? 1.0f : (d3 + d4) / d4;
		v4.tex *= isnan(d3) || d3 == 0.0f ? 1.0f : (d4 + d3) / d3;
	}

	void drawTriangleStrips(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
	{
		glEnable(GL_BLEND);
		glBlendFunc(convertBlendFactor(_srcBlendFactor), convertBlendFactor(_dstBlendFactor));

		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		/*
		if (_numVertices == 4)
		{
			Vertex4f v[4];
			for (int i = 0; i < 4; i++)
			{
				v[i].pos = Vector3f(_vertices[i].pos, 0);
				v[i].tex = Vector4f(_vertices[i].tex, 0, 1);
				v[i].col = _vertices[i].col;
			}

#define	ES_PI (3.1415926535897932384626433832795028841971693993751058209749445923)
#define	ES_RAD_TO_DEG(_x) ((_x) * (180.0 / ES_PI))
#define	ES_DEG_TO_RAD(_x) ((_x) * (ES_PI / 180.0))


			if (_vertices[2].tex.x() != 0)
				correctQuad(v[0], v[3], v[1], v[2]);
			
			glVertexPointer(3, GL_FLOAT, sizeof(Vertex4f), &v[0].pos);
			glTexCoordPointer(4, GL_FLOAT, sizeof(Vertex4f), &v[0].tex);
			glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex4f), &v[0].col);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, _numVertices);

		}
		else*/
		{
			glVertexPointer(2, GL_FLOAT, sizeof(Vertex), &_vertices[0].pos);
			glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), &_vertices[0].tex);
			glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), &_vertices[0].col);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, _numVertices);
		}

		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);

		glDisable(GL_BLEND);

	} // drawTriangleStrips

	void setProjection(const Transform4x4f& _projection)
	{
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf((GLfloat*)&_projection);

	} // setProjection

	void setMatrix(const Transform4x4f& _matrix)
	{
		Transform4x4f matrix = _matrix;
		matrix.round();
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf((GLfloat*)&matrix);

	} // setMatrix

	void setViewport(const Rect& _viewport)
	{
		// glViewport starts at the bottom left of the window
		glViewport( _viewport.x, getWindowHeight() - _viewport.y - _viewport.h, _viewport.w, _viewport.h);

	} // setViewport

	void setScissor(const Rect& _scissor)
	{
		if((_scissor.x == 0) && (_scissor.y == 0) && (_scissor.w == 0) && (_scissor.h == 0))
		{
			glDisable(GL_SCISSOR_TEST);
		}
		else
		{
			// glScissor starts at the bottom left of the window
			glScissor(_scissor.x, getWindowHeight() - _scissor.y - _scissor.h, _scissor.w, _scissor.h);
			glEnable(GL_SCISSOR_TEST);
		}

	} // setScissor

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

	void swapBuffers()
	{		
#ifdef WIN32		
		glFlush();
		glFinish();
		Sleep(0);
#endif

		SDL_GL_SwapWindow(getSDLWindow());

#ifdef WIN32		
		Sleep(0);
#endif

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	} // swapBuffers


	void drawTriangleFan(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
	{
		glEnable(GL_MULTISAMPLE);

		glEnable(GL_BLEND);
		glBlendFunc(convertBlendFactor(_srcBlendFactor), convertBlendFactor(_dstBlendFactor));

		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		glVertexPointer(2, GL_FLOAT, sizeof(Vertex), &_vertices[0].pos);
		glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), &_vertices[0].tex);
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), &_vertices[0].col);

		glDrawArrays(GL_TRIANGLE_FAN, 0, _numVertices);

		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);

		glDisable(GL_BLEND);
		glDisable(GL_MULTISAMPLE);
	}

	void setStencil(const Vertex* _vertices, const unsigned int _numVertices)
	{
		bool tx = glIsEnabled(GL_TEXTURE_2D);
		glDisable(GL_TEXTURE_2D);

		glClear(GL_DEPTH_BUFFER_BIT);
		glEnable(GL_STENCIL_TEST);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_FALSE);
		glStencilFunc(GL_NEVER, 1, 0xFF);
		glStencilOp(GL_REPLACE, GL_KEEP, GL_KEEP);

		glStencilMask(0xFF);
		glClear(GL_STENCIL_BUFFER_BIT);

		drawTriangleFan(_vertices, _numVertices);

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_TRUE);
		glStencilMask(0x00);
		glStencilFunc(GL_EQUAL, 0, 0xFF);
		glStencilFunc(GL_EQUAL, 1, 0xFF);

		if (tx)
			glEnable(GL_TEXTURE_2D);
	}

	void disableStencil()
	{
		glDisable(GL_STENCIL_TEST);
	}

} // Renderer::

#endif // USE_OPENGL_21
