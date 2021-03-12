#if defined(USE_OPENGLES_20) || defined (USE_OPENGL_21)

#include "renderers/Renderer.h"
#include "math/Transform4x4f.h"
#include "Log.h"
#include "Settings.h"

#include <vector>

#include "GlExtensions.h"
#include "Shader.h"

namespace Renderer
{
//////////////////////////////////////////////////////////////////////////

	static SDL_GLContext sdlContext       = nullptr;
	
	static Transform4x4f projectionMatrix = Transform4x4f::Identity();
	static Transform4x4f worldViewMatrix  = Transform4x4f::Identity();
	static Transform4x4f mvpMatrix		  = Transform4x4f::Identity();

	static Shader  	vertexShaderTexture;
	static Shader  	fragmentShaderColorTexture;
	static ShaderProgram    shaderProgramColorTexture;

	static Shader  	vertexShaderNoTexture;
	static Shader  	fragmentShaderColorNoTexture;
	static ShaderProgram    shaderProgramColorNoTexture;

	static GLuint        vertexBuffer     = 0;

//////////////////////////////////////////////////////////////////////////

	#define SHADER_VERSION_STRING "#version 100\n"

	static void setupShaders()
	{
		bool result = false;

		// vertex shader (no texture)
		const GLchar* vertexSourceNoTexture =
			SHADER_VERSION_STRING
			"uniform   mat4 u_mvp; \n"
			"attribute vec2 a_pos; \n"
			"attribute vec4 a_col; \n"
			"varying   vec4 v_col; \n"
			"void main(void)                                     \n"
			"{                                                   \n"
			"    gl_Position = u_mvp * vec4(a_pos.xy, 0.0, 1.0); \n"
			"    v_col       = a_col;                            \n"
			"}                                                   \n";

		// fragment shader (no texture)
		const GLchar* fragmentSourceNoTexture =
			SHADER_VERSION_STRING
			"precision highp float;     \n"
			"varying   vec4  v_col;     \n"
			"void main(void)            \n"
			"{                          \n"
			"    gl_FragColor = v_col;  \n"
			"}                          \n";


		// Compile each shader, link them to make a full program
		const GLuint vertexShaderColorTextureId = glCreateShader(GL_VERTEX_SHADER);
		result = vertexShaderNoTexture.compile(vertexShaderColorTextureId, vertexSourceNoTexture);
		const GLuint fragmentShaderNoTextureId = glCreateShader(GL_FRAGMENT_SHADER);
		result = fragmentShaderColorNoTexture.compile(fragmentShaderNoTextureId, fragmentSourceNoTexture);
		result = shaderProgramColorNoTexture.linkShaderProgram(vertexShaderNoTexture, fragmentShaderColorNoTexture);


		// Set shader active, retrieve attributes and uniforms locations
		GL_CHECK_ERROR(glUseProgram(shaderProgramColorNoTexture.id));
		shaderProgramColorNoTexture.posAttrib = glGetAttribLocation(shaderProgramColorNoTexture.id, "a_pos");
		shaderProgramColorNoTexture.colAttrib = glGetAttribLocation(shaderProgramColorNoTexture.id, "a_col");
		shaderProgramColorNoTexture.mvpUniform = glGetUniformLocation(shaderProgramColorNoTexture.id, "u_mvp");
		shaderProgramColorNoTexture.texAttrib = -1;

		// vertex shader (texture)
		const GLchar* vertexSourceTexture =
			SHADER_VERSION_STRING
			"uniform   mat4 u_mvp; \n"
			"attribute vec2 a_pos; \n"
			"attribute vec2 a_tex; \n"
			"attribute vec4 a_col; \n"
			"varying   vec2 v_tex; \n"
			"varying   vec4 v_col; \n"
			"void main(void)                                     \n"
			"{                                                   \n"
			"    gl_Position = u_mvp * vec4(a_pos.xy, 0.0, 1.0); \n"
			"    v_tex       = a_tex;                            \n"
			"    v_col       = a_col;                            \n"
			"}                                                   \n";

		// fragment shader (texture)
		const GLchar* fragmentSourceTexture =
			SHADER_VERSION_STRING
			"precision highp float;       \n"
#if defined(USE_OPENGLES_20)
			"precision mediump sampler2D; \n"
#endif
			"varying   vec4      v_col; \n"
			"varying   vec2      v_tex; \n"
			"uniform   sampler2D u_tex; \n"
			"void main(void)                                     \n"
			"{                                                   \n"
			"    gl_FragColor = texture2D(u_tex, v_tex) * v_col; \n"
			"}                                                   \n";
		
		// Compile each shader, link them to make a full program
		const GLuint vertexShaderColorNoTextureId = glCreateShader(GL_VERTEX_SHADER);
		result = vertexShaderTexture.compile(vertexShaderColorNoTextureId, vertexSourceTexture);

		const GLuint fragmentShaderTextureId = glCreateShader(GL_FRAGMENT_SHADER);
		result = fragmentShaderColorTexture.compile(fragmentShaderTextureId, fragmentSourceTexture);
		result = shaderProgramColorTexture.linkShaderProgram(vertexShaderTexture, fragmentShaderColorTexture);
		
		// Set shader active, retrieve attributes and uniforms locations
		GL_CHECK_ERROR(glUseProgram(shaderProgramColorTexture.id));
		shaderProgramColorTexture.posAttrib = glGetAttribLocation(shaderProgramColorTexture.id, "a_pos");
		shaderProgramColorTexture.colAttrib = glGetAttribLocation(shaderProgramColorTexture.id, "a_col");
		shaderProgramColorTexture.texAttrib = glGetAttribLocation(shaderProgramColorTexture.id, "a_tex");
		shaderProgramColorTexture.mvpUniform = glGetUniformLocation(shaderProgramColorTexture.id, "u_mvp");
		GLint texUniform = glGetUniformLocation(shaderProgramColorTexture.id, "u_tex");
		GL_CHECK_ERROR(glUniform1i(texUniform, 0));

	} // setupShaders

//////////////////////////////////////////////////////////////////////////

	static void setupVertexBuffer()
	{
		GL_CHECK_ERROR(glGenBuffers(1, &vertexBuffer));
		GL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer));

	} // setupVertexBuffer

//////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////

	static GLenum convertTextureType(const Texture::Type _type)
	{
		switch(_type)
		{
			case Texture::RGBA:  { return GL_RGBA;            } break;
			case Texture::ALPHA: { return GL_LUMINANCE_ALPHA; } break;
			default:             { return GL_ZERO;            }
		}

	} // convertTextureType

//////////////////////////////////////////////////////////////////////////

	unsigned int convertColor(const unsigned int _color)
	{
		// convert from rgba to abgr
		const unsigned char r = ((_color & 0xff000000) >> 24) & 255;
		const unsigned char g = ((_color & 0x00ff0000) >> 16) & 255;
		const unsigned char b = ((_color & 0x0000ff00) >>  8) & 255;
		const unsigned char a = ((_color & 0x000000ff)      ) & 255;

		return ((a << 24) | (b << 16) | (g << 8) | (r));

	} // convertColor

//////////////////////////////////////////////////////////////////////////

	unsigned int getWindowFlags()
	{
		return SDL_WINDOW_OPENGL;

	} // getWindowFlags

//////////////////////////////////////////////////////////////////////////

	void setupWindow()
	{
#if OPENGL_EXTENSIONS
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#else
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,       1);
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE,           8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,         8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,          8);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,        24);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,       1);
		SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

	} // setupWindow

//////////////////////////////////////////////////////////////////////////

	void createContext()
	{
		sdlContext = SDL_GL_CreateContext(getSDLWindow());
		SDL_GL_MakeCurrent(getSDLWindow(), sdlContext);

		const std::string vendor     = glGetString(GL_VENDOR)     ? (const char*)glGetString(GL_VENDOR)     : "";
		const std::string renderer   = glGetString(GL_RENDERER)   ? (const char*)glGetString(GL_RENDERER)   : "";
		const std::string version    = glGetString(GL_VERSION)    ? (const char*)glGetString(GL_VERSION)    : "";
		const std::string extensions = glGetString(GL_EXTENSIONS) ? (const char*)glGetString(GL_EXTENSIONS) : "";
		const std::string shaders    = glGetString(GL_SHADING_LANGUAGE_VERSION) ? (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) : "";
		
		LOG(LogInfo) << "GL vendor:   " << vendor;
		LOG(LogInfo) << "GL renderer: " << renderer;
		LOG(LogInfo) << "GL version:  " << version;
		LOG(LogInfo) << "GL shading:  " << shaders;
		LOG(LogInfo) << "GL exts:     " << extensions;

		LOG(LogInfo) << " ARB_texture_non_power_of_two: " << (extensions.find("ARB_texture_non_power_of_two") != std::string::npos ? "ok" : "MISSING");

#if OPENGL_EXTENSIONS
		initializeGlExtensions();
#endif

		setupShaders();
		setupVertexBuffer();

		GL_CHECK_ERROR(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));

#if OPENGL_EXTENSIONS
		GL_CHECK_ERROR(glActiveTexture_(GL_TEXTURE0));
#else
		GL_CHECK_ERROR(glActiveTexture(GL_TEXTURE0));
#endif

		GL_CHECK_ERROR(glPixelStorei(GL_PACK_ALIGNMENT, 1));
		GL_CHECK_ERROR(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

	} // createContext

//////////////////////////////////////////////////////////////////////////

	void destroyContext()
	{
		SDL_GL_DeleteContext(sdlContext);
		sdlContext = nullptr;

	} // destroyContext

//////////////////////////////////////////////////////////////////////////

	unsigned int createTexture(const Texture::Type _type, const bool _linear, const bool _repeat, const unsigned int _width, const unsigned int _height, void* _data)
	{
		const GLenum type = convertTextureType(_type);
		unsigned int texture;

		GL_CHECK_ERROR(glGenTextures(1, &texture));
		bindTexture(texture);

		GL_CHECK_ERROR(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, _repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE));
		GL_CHECK_ERROR(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, _repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE));

		GL_CHECK_ERROR(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		GL_CHECK_ERROR(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, _linear ? GL_LINEAR : GL_NEAREST));

		// Regular GL_ALPHA textures are black + alpha in shaders
		// Create a GL_LUMINANCE_ALPHA texture instead so its white + alpha
		
		if (type == GL_LUMINANCE_ALPHA && _data != nullptr)
		{
			uint8_t* a_data  = (uint8_t*)_data;
			uint8_t* la_data = new uint8_t[_width * _height * 2];
			memset(la_data, 255, _width * _height * 2);
			if (a_data)
			{
				for(uint32_t i=0; i<(_width * _height); ++i)
					la_data[(i * 2) + 1] = a_data[i];
			}

			GL_CHECK_ERROR(glTexImage2D(GL_TEXTURE_2D, 0, type, _width, _height, 0, type, GL_UNSIGNED_BYTE, la_data));

			delete[] la_data;
		}
		else
			GL_CHECK_ERROR(glTexImage2D(GL_TEXTURE_2D, 0, type, _width, _height, 0, type, GL_UNSIGNED_BYTE, _data));

		return texture;

	} // createTexture

//////////////////////////////////////////////////////////////////////////

	void destroyTexture(const unsigned int _texture)
	{
		GL_CHECK_ERROR(glDeleteTextures(1, &_texture));

	} // destroyTexture

//////////////////////////////////////////////////////////////////////////

	void updateTexture(const unsigned int _texture, const Texture::Type _type, const unsigned int _x, const unsigned _y, const unsigned int _width, const unsigned int _height, void* _data)
	{
		const GLenum type = convertTextureType(_type);

		bindTexture(_texture);

		// Regular GL_ALPHA textures are black + alpha in shaders
		// Create a GL_LUMINANCE_ALPHA texture instead so its white + alpha
		if(type == GL_LUMINANCE_ALPHA)
		{
			uint8_t* a_data  = (uint8_t*)_data;
			uint8_t* la_data = new uint8_t[_width * _height * 2];
			memset(la_data, 255, _width * _height * 2);
			if (a_data)
			{
				for(uint32_t i=0; i<(_width * _height); ++i)
					la_data[(i * 2) + 1] = a_data[i];				
			}

			GL_CHECK_ERROR(glTexSubImage2D(GL_TEXTURE_2D, 0, _x, _y, _width, _height, type, GL_UNSIGNED_BYTE, la_data));
			delete[] la_data;
		}
		else
			GL_CHECK_ERROR(glTexSubImage2D(GL_TEXTURE_2D, 0, _x, _y, _width, _height, type, GL_UNSIGNED_BYTE, _data));

		bindTexture(0);

	} // updateTexture

//////////////////////////////////////////////////////////////////////////

	static unsigned int boundTexture = 0;

	void bindTexture(const unsigned int _texture)
	{
		if (boundTexture == _texture)
			return;

		boundTexture = _texture;

		if(_texture == 0)
		{
			GL_CHECK_ERROR(glBindTexture(GL_TEXTURE_2D, 0));
			boundTexture = 0;
		}
		else
		{
			GL_CHECK_ERROR(glBindTexture(GL_TEXTURE_2D, _texture));
			boundTexture = _texture;
		}

	} // bindTexture

//////////////////////////////////////////////////////////////////////////

	void drawLines(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
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

	} // drawLines

//////////////////////////////////////////////////////////////////////////

	void drawTriangleStrips(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
	{
		// Pass buffer data
		GL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * _numVertices, _vertices, GL_DYNAMIC_DRAW));

		// Setup shader
		if (boundTexture != 0)
		{
			GL_CHECK_ERROR(glUseProgram(shaderProgramColorTexture.id));

			GL_CHECK_ERROR(glVertexAttribPointer(shaderProgramColorTexture.posAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, pos)));
			GL_CHECK_ERROR(glEnableVertexAttribArray(shaderProgramColorTexture.posAttrib));

			GL_CHECK_ERROR(glVertexAttribPointer(shaderProgramColorTexture.colAttrib, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(Vertex), (const void*)offsetof(Vertex, col)));
			GL_CHECK_ERROR(glEnableVertexAttribArray(shaderProgramColorTexture.colAttrib));

			GL_CHECK_ERROR(glVertexAttribPointer(shaderProgramColorTexture.texAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, tex)));
			GL_CHECK_ERROR(glEnableVertexAttribArray(shaderProgramColorTexture.texAttrib));
		}
		else
		{
			GL_CHECK_ERROR(glUseProgram(shaderProgramColorNoTexture.id));

			GL_CHECK_ERROR(glVertexAttribPointer(shaderProgramColorNoTexture.posAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, pos)));
			GL_CHECK_ERROR(glEnableVertexAttribArray(shaderProgramColorNoTexture.posAttrib));

			GL_CHECK_ERROR(glVertexAttribPointer(shaderProgramColorNoTexture.colAttrib, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(Vertex), (const void*)offsetof(Vertex, col)));
			GL_CHECK_ERROR(glEnableVertexAttribArray(shaderProgramColorNoTexture.colAttrib));
		}

		// Do rendering
		GL_CHECK_ERROR(glEnable(GL_BLEND));
		GL_CHECK_ERROR(glBlendFunc(convertBlendFactor(_srcBlendFactor), convertBlendFactor(_dstBlendFactor)));
		GL_CHECK_ERROR(glDrawArrays(GL_TRIANGLE_STRIP, 0, _numVertices));
		GL_CHECK_ERROR(glDisable(GL_BLEND));

		// Restore context
		if (boundTexture != 0)
		{
			GL_CHECK_ERROR(glDisableVertexAttribArray(shaderProgramColorTexture.posAttrib));
			GL_CHECK_ERROR(glDisableVertexAttribArray(shaderProgramColorTexture.colAttrib));
			GL_CHECK_ERROR(glDisableVertexAttribArray(shaderProgramColorTexture.texAttrib));
		}
		else
		{
			GL_CHECK_ERROR(glDisableVertexAttribArray(shaderProgramColorNoTexture.posAttrib));
			GL_CHECK_ERROR(glDisableVertexAttribArray(shaderProgramColorNoTexture.colAttrib));
		}

	} // drawTriangleStrips

//////////////////////////////////////////////////////////////////////////

	void setProjection(const Transform4x4f& _projection)
	{
		projectionMatrix = _projection;

		mvpMatrix = projectionMatrix * worldViewMatrix;
		glUseProgram(shaderProgramColorTexture.id);
		GL_CHECK_ERROR(glUniformMatrix4fv(shaderProgramColorTexture.mvpUniform, 1, GL_FALSE, (float*)&mvpMatrix));
		glUseProgram(shaderProgramColorNoTexture.id);
		GL_CHECK_ERROR(glUniformMatrix4fv(shaderProgramColorNoTexture.mvpUniform, 1, GL_FALSE, (float*)&mvpMatrix));

	} // setProjection

//////////////////////////////////////////////////////////////////////////

	void setMatrix(const Transform4x4f& _matrix)
	{
		worldViewMatrix = _matrix;
		worldViewMatrix.round();

		mvpMatrix = projectionMatrix * worldViewMatrix;
		glUseProgram(shaderProgramColorTexture.id);
		GL_CHECK_ERROR(glUniformMatrix4fv(shaderProgramColorTexture.mvpUniform, 1, GL_FALSE, (float*)&mvpMatrix));
		glUseProgram(shaderProgramColorNoTexture.id);
		GL_CHECK_ERROR(glUniformMatrix4fv(shaderProgramColorNoTexture.mvpUniform, 1, GL_FALSE, (float*)&mvpMatrix));

	} // setMatrix

//////////////////////////////////////////////////////////////////////////

	void setViewport(const Rect& _viewport)
	{
		// glViewport starts at the bottom left of the window
		GL_CHECK_ERROR(glViewport( _viewport.x, getWindowHeight() - _viewport.y - _viewport.h, _viewport.w, _viewport.h));

	} // setViewport

//////////////////////////////////////////////////////////////////////////

	void setScissor(const Rect& _scissor)
	{
		if((_scissor.x == 0) && (_scissor.y == 0) && (_scissor.w == 0) && (_scissor.h == 0))
		{
			GL_CHECK_ERROR(glDisable(GL_SCISSOR_TEST));
		}
		else
		{
			// glScissor starts at the bottom left of the window
			GL_CHECK_ERROR(glScissor(_scissor.x, getWindowHeight() - _scissor.y - _scissor.h, _scissor.w, _scissor.h));
			GL_CHECK_ERROR(glEnable(GL_SCISSOR_TEST));
		}

	} // setScissor

//////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////

	void swapBuffers()
	{
		SDL_GL_SwapWindow(getSDLWindow());
		GL_CHECK_ERROR(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

	} // swapBuffers

//////////////////////////////////////////////////////////////////////////
	
	void drawTriangleFan(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
	{
		GL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * _numVertices, _vertices, GL_DYNAMIC_DRAW));

		GL_CHECK_ERROR(glUseProgram(shaderProgramColorNoTexture.id));

		GL_CHECK_ERROR(glVertexAttribPointer(shaderProgramColorNoTexture.posAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, pos)));
		GL_CHECK_ERROR(glEnableVertexAttribArray(shaderProgramColorNoTexture.posAttrib));

		GL_CHECK_ERROR(glVertexAttribPointer(shaderProgramColorNoTexture.colAttrib, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (const void*)offsetof(Vertex, col)));
		GL_CHECK_ERROR(glEnableVertexAttribArray(shaderProgramColorNoTexture.colAttrib));

		GL_CHECK_ERROR(glEnable(GL_BLEND));
		GL_CHECK_ERROR(glBlendFunc(convertBlendFactor(_srcBlendFactor), convertBlendFactor(_dstBlendFactor)));
		GL_CHECK_ERROR(glDrawArrays(GL_TRIANGLE_FAN, 0, _numVertices));
		GL_CHECK_ERROR(glDisable(GL_BLEND));

		GL_CHECK_ERROR(glDisableVertexAttribArray(shaderProgramColorNoTexture.posAttrib));
		GL_CHECK_ERROR(glDisableVertexAttribArray(shaderProgramColorNoTexture.colAttrib));
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

#endif // USE_OPENGLES_20
