#include "Renderer_GLES20.h"

#ifdef RENDERER_GLES_20

#include "Renderer_GLES20.h"
#include "renderers/Renderer.h"
#include "math/Transform4x4f.h"
#include "Log.h"
#include "Settings.h"

#include <vector>
#include <set>

#include "GlExtensions.h"
#include "Shader.h"

namespace Renderer
{

//////////////////////////////////////////////////////////////////////////

	struct TextureInfo
	{
		GLenum type;
		Vector2f size;
	};

	static SDL_GLContext	sdlContext       = nullptr;
	
	static Transform4x4f	projectionMatrix = Transform4x4f::Identity();
	static Transform4x4f	worldViewMatrix  = Transform4x4f::Identity();
	static Transform4x4f	mvpMatrix		 = Transform4x4f::Identity();

	static ShaderProgram    shaderProgramColorTexture;
	static ShaderProgram    shaderProgramColorNoTexture;
	static ShaderProgram    shaderProgramAlpha;

	static GLuint			vertexBuffer     = 0;

	static std::map<unsigned int, TextureInfo*> _textures;

	static unsigned int		boundTexture = 0;

	extern std::string SHADER_VERSION_STRING;

//////////////////////////////////////////////////////////////////////////

	static ShaderProgram* currentProgram = nullptr;
	
	static void useProgram(ShaderProgram* program)
	{
		if (program == currentProgram)
		{
			if (currentProgram != nullptr)
				currentProgram->setMatrix(mvpMatrix);

			return;
		}
		
		if (program == nullptr && currentProgram != nullptr)
			currentProgram->unSelect();

		currentProgram = program;
		
		if (currentProgram != nullptr)
		{
			currentProgram->select();
			currentProgram->setMatrix(mvpMatrix);
		}
	}

	static std::map<std::string, ShaderProgram*> customShaders;

	static ShaderProgram* getShaderProgram(char* shaderFile)
	{
		if (shaderFile == nullptr)
			return nullptr;

		auto it = customShaders.find(shaderFile);
		if (it != customShaders.cend())
			return it->second;

		ShaderProgram* customShader = new ShaderProgram();
		if (!customShader->loadFromFile(shaderFile))
		{
			delete customShader;
			customShader = nullptr;
		}

		customShaders[shaderFile] = customShader;

		return customShader;	
	}

	static void setupDefaultShaders()
	{
#if defined(USE_OPENGLES_20)
		SHADER_VERSION_STRING = "#version 100\n";
#else 
		SHADER_VERSION_STRING = "#version 120\n";

		const std::string shaders = glGetString(GL_SHADING_LANGUAGE_VERSION) ? (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) : "";

		if (shaders.find("1.0") != std::string::npos)
			SHADER_VERSION_STRING = "#version 100\n";
		else if (shaders.find("1.1") != std::string::npos)
			SHADER_VERSION_STRING = "#version 110\n";
#endif

		LOG(LogInfo) << "GLSL version preprocessor :     " << SHADER_VERSION_STRING;

		// vertex shader (no texture)
		std::string vertexSourceNoTexture =
			SHADER_VERSION_STRING +
			R"=====(
			uniform   mat4 MVPMatrix;
			attribute vec2 VertexCoord;
			attribute vec4 COLOR;
			varying   vec4 v_col;
			void main(void)
			{
			    gl_Position = MVPMatrix * vec4(VertexCoord.xy, 0.0, 1.0);
			    v_col       = COLOR;
			}
			)=====";

		// fragment shader (no texture)
		std::string fragmentSourceNoTexture =
			SHADER_VERSION_STRING +
			R"=====(
			precision mediump float;
			varying   vec4  v_col;   
			void main(void)          
			{                        
			    gl_FragColor = v_col;
			}                        
			)=====";

		// Compile each shader, link them to make a full program
		auto vertexShaderNoTexture = Shader::createShader(GL_VERTEX_SHADER, vertexSourceNoTexture);
		auto fragmentShaderColorNoTexture = Shader::createShader(GL_FRAGMENT_SHADER, fragmentSourceNoTexture);

		shaderProgramColorNoTexture.createShaderProgram(vertexShaderNoTexture, fragmentShaderColorNoTexture);
		
		// vertex shader (texture)
		std::string vertexSourceTexture =
			SHADER_VERSION_STRING +
			R"=====(
			uniform   mat4 MVPMatrix;
			attribute vec2 VertexCoord;
			attribute vec2 TexCoord;
			attribute vec4 COLOR;
			varying   vec2 v_tex;
			varying   vec4 v_col;
			void main(void)                                    
			{                                                  
			    gl_Position = MVPMatrix * vec4(VertexCoord.xy, 0.0, 1.0);
			    v_tex       = TexCoord;                           
			    v_col       = COLOR;                           
			}
			)=====";

		// fragment shader (texture)
		std::string fragmentSourceTexture =
			SHADER_VERSION_STRING +
#if defined(USE_OPENGLES_20)
			"precision mediump sampler2D; \n"+
#endif
			R"=====(
			precision mediump float;
			varying   vec4      v_col;
			varying   vec2      v_tex;
			uniform   sampler2D u_tex;
			uniform   float saturation;
			void main(void)                                    
			{                                                  
			    vec4 clr = texture2D(u_tex, v_tex) * v_col;
		
			    if (saturation != 1.0) {
			    	vec3 gray = vec3(dot(clr.rgb, vec3(0.34, 0.55, 0.11)));
			    	vec3 blend = mix(gray, clr.rgb, saturation);
			    	clr = vec4(blend, clr.a);
			    }
			
			    gl_FragColor = clr;
			}
			)=====";

		// Compile each shader, link them to make a full program
		auto vertexShaderTexture = Shader::createShader(GL_VERTEX_SHADER, vertexSourceTexture);
		auto fragmentShaderColorTexture = Shader::createShader(GL_FRAGMENT_SHADER, fragmentSourceTexture);
		shaderProgramColorTexture.createShaderProgram(vertexShaderTexture, fragmentShaderColorTexture);
		
		// fragment shader (alpha texture)
		std::string fragmentSourceAlpha =
			SHADER_VERSION_STRING +
#if defined(USE_OPENGLES_20)
			"precision mediump sampler2D; \n" +
#endif
			R"=====(
			precision mediump float;  
			varying   vec4      v_col;
			varying   vec2      v_tex;
			uniform   sampler2D u_tex;
			void main(void)           
			{                         
			    vec4 a = vec4(1.0, 1.0, 1.0, texture2D(u_tex, v_tex).a);
			    gl_FragColor = a * v_col; 
			}
			)=====";


		auto vertexShaderAlpha = Shader::createShader(GL_VERTEX_SHADER, vertexSourceTexture);
		auto fragmentShaderAlpha = Shader::createShader(GL_FRAGMENT_SHADER, fragmentSourceAlpha);

		shaderProgramAlpha.createShaderProgram(vertexShaderAlpha, fragmentShaderAlpha);
		
		useProgram(nullptr);
	} // setupDefaultShaders

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
#if defined(USE_OPENGLES_20)
			case Texture::ALPHA: { return GL_ALPHA; } break;
#else
			case Texture::ALPHA: { return GL_LUMINANCE_ALPHA; } break;
#endif
			default:             { return GL_ZERO;            }
		}

	} // convertTextureType

//////////////////////////////////////////////////////////////////////////

	static int getAvailableVideoMemory()
	{
		float total = 0;

		float megabytes = 10.0;
		int sz = sqrtf(megabytes * 1024.0 * 1024.0 / 4.0f);

		std::vector<unsigned int> textures;
		textures.reserve(1000000);

		while (true)
		{
			unsigned int textureId;
			glGenTextures(1, &textureId);
			if (glGetError() != GL_NO_ERROR)
				break;

			textures.push_back(textureId);

			bindTexture(textureId);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sz, sz, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			if (glGetError() != GL_NO_ERROR)
				break;

			textures.push_back(textureId);
			total += megabytes;
		}

		for (auto tx : textures)
			Renderer::destroyTexture(tx);

		return total;
	}

//////////////////////////////////////////////////////////////////////////

	unsigned int GLES20Renderer::getWindowFlags()
	{
		return SDL_WINDOW_OPENGL;

	} // getWindowFlags

//////////////////////////////////////////////////////////////////////////

	void GLES20Renderer::setupWindow()
	{
#if OPENGL_EXTENSIONS
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#else
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
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
	std::string GLES20Renderer::getDriverName()
	{
#if OPENGL_EXTENSIONS
		return "OPENGL 2.1 / GLSL";
#else 
		return "OPENGL ES 2.0";
#endif
	}

	std::vector<std::pair<std::string, std::string>> GLES20Renderer::getDriverInformation()
	{
		std::vector<std::pair<std::string, std::string>> info;

		info.push_back(std::pair<std::string, std::string>("GRAPHICS API", getDriverName()));

		const std::string vendor = glGetString(GL_VENDOR) ? (const char*)glGetString(GL_VENDOR) : "";
		if (!vendor.empty())
			info.push_back(std::pair<std::string, std::string>("VENDOR", vendor));

		const std::string renderer = glGetString(GL_RENDERER) ? (const char*)glGetString(GL_RENDERER) : "";
		if (!renderer.empty())
			info.push_back(std::pair<std::string, std::string>("RENDERER", renderer));

		const std::string version = glGetString(GL_VERSION) ? (const char*)glGetString(GL_VERSION) : "";
		if (!version.empty())
			info.push_back(std::pair<std::string, std::string>("VERSION", version));

		const std::string shaders = glGetString(GL_SHADING_LANGUAGE_VERSION) ? (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) : "";
		if (!shaders.empty())
			info.push_back(std::pair<std::string, std::string>("SHADERS", shaders));

		return info;
	}

	void GLES20Renderer::createContext()
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

		setupDefaultShaders();
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

	void GLES20Renderer::resetCache()
	{
		for (auto customShader : customShaders)
			if (customShader.second != nullptr)
				customShader.second->deleteProgram();

		customShaders.clear();
	}

	void GLES20Renderer::destroyContext()
	{
		resetCache();

		SDL_GL_DeleteContext(sdlContext);
		sdlContext = nullptr;

	} // destroyContext

//////////////////////////////////////////////////////////////////////////

	unsigned int GLES20Renderer::createTexture(const Texture::Type _type, const bool _linear, const bool _repeat, const unsigned int _width, const unsigned int _height, void* _data)
	{
		const GLenum type = convertTextureType(_type);
		unsigned int texture;

		glGenTextures(1, &texture);
		if (glGetError() != GL_NO_ERROR)
		{
			LOG(LogError) << "CreateTexture error: glGenTextures failed";
			return 0;
		}

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

			glTexImage2D(GL_TEXTURE_2D, 0, type, _width, _height, 0, type, GL_UNSIGNED_BYTE, la_data);
			delete[] la_data;

			if (glGetError() != GL_NO_ERROR)
			{
				LOG(LogError) << "CreateTexture error: glTexImage2D failed";
				destroyTexture(texture);
				return 0;
			}
		}
		else
		{
			glTexImage2D(GL_TEXTURE_2D, 0, type, _width, _height, 0, type, GL_UNSIGNED_BYTE, _data);
			if (glGetError() != GL_NO_ERROR)
			{
				LOG(LogError) << "CreateTexture error: glTexImage2D failed";
				destroyTexture(texture);
				return 0;
			}
		}

		if (texture != 0)
		{
			auto it = _textures.find(texture);
			if (it != _textures.cend())
			{
				it->second->type = type;
				it->second->size = Vector2f(_width, _height);
			}
			else
			{
				auto info = new TextureInfo();
				info->type = type;
				info->size = Vector2f(_width, _height);
				_textures[texture] = info;
			}
		}

		return texture;

	} // createTexture

//////////////////////////////////////////////////////////////////////////

	void GLES20Renderer::destroyTexture(const unsigned int _texture)
	{
		auto it = _textures.find(_texture);
		if (it != _textures.cend())
		{
			if (it->second != nullptr)
			{
				delete it->second;
				it->second = nullptr;
			}
				
			_textures.erase(it);
		}
		
		GL_CHECK_ERROR(glDeleteTextures(1, &_texture));

	} // destroyTexture

//////////////////////////////////////////////////////////////////////////

	void GLES20Renderer::updateTexture(const unsigned int _texture, const Texture::Type _type, const unsigned int _x, const unsigned _y, const unsigned int _width, const unsigned int _height, void* _data)
	{
		const GLenum type = convertTextureType(_type);

		bindTexture(_texture);

		// Regular GL_ALPHA textures are black + alpha in shaders
		// Create a GL_LUMINANCE_ALPHA texture instead so its white + alpha
		if (type == GL_LUMINANCE_ALPHA)
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

		if (_texture != 0)
		{
			auto it = _textures.find(_texture);
			if (it != _textures.cend())
			{
				it->second->type = type;
				it->second->size = Vector2f(_width, _height);
			}
			else
			{
				auto info = new TextureInfo();
				info->type = type;
				info->size = Vector2f(_width, _height);
				_textures[_texture] = info;
			}
		}

		bindTexture(0);

	} // updateTexture

//////////////////////////////////////////////////////////////////////////

	void GLES20Renderer::bindTexture(const unsigned int _texture)
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

	void GLES20Renderer::drawLines(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
	{
		// Pass buffer data
		GL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * _numVertices, _vertices, GL_DYNAMIC_DRAW));

		useProgram(&shaderProgramColorNoTexture);

		// Do rendering
		if (_srcBlendFactor != Blend::ONE && _dstBlendFactor != Blend::ONE)
		{
			GL_CHECK_ERROR(glEnable(GL_BLEND));
			GL_CHECK_ERROR(glBlendFunc(convertBlendFactor(_srcBlendFactor), convertBlendFactor(_dstBlendFactor)));
			GL_CHECK_ERROR(glDrawArrays(GL_LINES, 0, _numVertices));
			GL_CHECK_ERROR(glDisable(GL_BLEND));
		}
		else
		{
			GL_CHECK_ERROR(glDisable(GL_BLEND));
			GL_CHECK_ERROR(glDrawArrays(GL_LINES, 0, _numVertices));
		}

	} // drawLines

//////////////////////////////////////////////////////////////////////////

	void GLES20Renderer::drawTriangleStrips(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor, bool verticesChanged)
	{
		if (verticesChanged)
			GL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * _numVertices, _vertices, GL_DYNAMIC_DRAW));

		// Setup shader
		if (boundTexture != 0)
		{
			auto it = _textures.find(boundTexture);
			if (it != _textures.cend() && it->second != nullptr && it->second->type == GL_ALPHA)
				useProgram(&shaderProgramAlpha);
			else
			{
				ShaderProgram* shader = &shaderProgramColorTexture;

				if (_vertices->customShader != nullptr)
				{
					ShaderProgram* customShader = getShaderProgram(_vertices->customShader);
					if (customShader != nullptr)
						shader = customShader;
				}

				useProgram(shader);

				// Update Shader Uniforms

				shader->setSaturation(_vertices->saturation);
				
				if (shader->supportsTextureSize() && it != _textures.cend() && it->second != nullptr)
					shader->setTextureSize(it->second->size);
				
				if (_numVertices > 0)
					shader->setOutputSize(_vertices[_numVertices-1].pos);
			}
		}
		else
			useProgram(&shaderProgramColorNoTexture);

		// Do rendering
		if (_srcBlendFactor != Blend::ONE && _dstBlendFactor != Blend::ONE)
		{
			GL_CHECK_ERROR(glEnable(GL_BLEND));
			GL_CHECK_ERROR(glBlendFunc(convertBlendFactor(_srcBlendFactor), convertBlendFactor(_dstBlendFactor)));
			GL_CHECK_ERROR(glDrawArrays(GL_TRIANGLE_STRIP, 0, _numVertices));
			GL_CHECK_ERROR(glDisable(GL_BLEND));
		}
		else
		{
			GL_CHECK_ERROR(glDisable(GL_BLEND));
			GL_CHECK_ERROR(glDrawArrays(GL_TRIANGLE_STRIP, 0, _numVertices));
		}
	} // drawTriangleStrips

//////////////////////////////////////////////////////////////////////////

	void GLES20Renderer::setProjection(const Transform4x4f& _projection)
	{
		projectionMatrix = _projection;
		mvpMatrix = projectionMatrix * worldViewMatrix;
	} // setProjection

//////////////////////////////////////////////////////////////////////////

	void GLES20Renderer::setMatrix(const Transform4x4f& _matrix)
	{
		worldViewMatrix = _matrix;
		worldViewMatrix.round();
		mvpMatrix = projectionMatrix * worldViewMatrix;
	} // setMatrix

//////////////////////////////////////////////////////////////////////////

	void GLES20Renderer::setViewport(const Rect& _viewport)
	{
		// glViewport starts at the bottom left of the window
		GL_CHECK_ERROR(glViewport( _viewport.x, getWindowHeight() - _viewport.y - _viewport.h, _viewport.w, _viewport.h));

	} // setViewport

//////////////////////////////////////////////////////////////////////////

	void GLES20Renderer::setScissor(const Rect& _scissor)
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

	void GLES20Renderer::setSwapInterval()
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

	void GLES20Renderer::swapBuffers()
	{
		useProgram(nullptr);
		SDL_GL_SwapWindow(getSDLWindow());
		GL_CHECK_ERROR(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
	} // swapBuffers

//////////////////////////////////////////////////////////////////////////
	
	void GLES20Renderer::drawTriangleFan(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
	{		
		// Pass buffer data
		GL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * _numVertices, _vertices, GL_DYNAMIC_DRAW));

		// Setup shader
		if (boundTexture != 0)
		{
			auto it = _textures.find(boundTexture);
			if (it != _textures.cend() && it->second != nullptr && it->second->type == GL_ALPHA)
				useProgram(&shaderProgramAlpha);
			else
			{
				useProgram(&shaderProgramColorTexture);
				shaderProgramColorTexture.setSaturation(_vertices->saturation);
			}
		}
		else
			useProgram(&shaderProgramColorNoTexture);

		// Do rendering
		if (_srcBlendFactor != Blend::ONE && _dstBlendFactor != Blend::ONE)
		{
			GL_CHECK_ERROR(glEnable(GL_BLEND));
			GL_CHECK_ERROR(glBlendFunc(convertBlendFactor(_srcBlendFactor), convertBlendFactor(_dstBlendFactor)));
			GL_CHECK_ERROR(glDrawArrays(GL_TRIANGLE_FAN, 0, _numVertices));
			GL_CHECK_ERROR(glDisable(GL_BLEND));
		}
		else
		{
			GL_CHECK_ERROR(glDisable(GL_BLEND));
			GL_CHECK_ERROR(glDrawArrays(GL_TRIANGLE_FAN, 0, _numVertices));			
		}
	}

	void GLES20Renderer::setStencil(const Vertex* _vertices, const unsigned int _numVertices)
	{
		useProgram(&shaderProgramColorNoTexture);

		glEnable(GL_STENCIL_TEST);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_FALSE);
		glStencilFunc(GL_NEVER, 1, 0xFF);
		glStencilOp(GL_REPLACE, GL_KEEP, GL_KEEP);
		glStencilMask(0xFF);
		glClear(GL_STENCIL_BUFFER_BIT);
		
		glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * _numVertices, _vertices, GL_DYNAMIC_DRAW);
		glDrawArrays(GL_TRIANGLE_FAN, 0, _numVertices);

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_TRUE);
		glStencilMask(0x00);
		glStencilFunc(GL_EQUAL, 0, 0xFF);
		glStencilFunc(GL_EQUAL, 1, 0xFF);
	}

	void GLES20Renderer::disableStencil()
	{
		glDisable(GL_STENCIL_TEST);
	}
} // Renderer::

#endif // USE_OPENGLES_20
