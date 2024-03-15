#include "Renderer_GLES20.h"

#ifdef RENDERER_GLES_20

#include "Renderer_GLES20.h"
#include "renderers/Renderer.h"
#include "math/Transform4x4f.h"
#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"
#include "Log.h"
#include "Settings.h"

#include <vector>
#include <set>
#include <fstream>

#include "GlExtensions.h"
#include "Shader.h"

#include "resources/ResourceManager.h"

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

	static std::map<std::string, ShaderProgram*> _customShaders;

	static ShaderProgram* getShaderProgram(const char* shaderFile)
	{
		if (shaderFile == nullptr || strlen(shaderFile) == 0)
			return nullptr;

		auto it = _customShaders.find(shaderFile);
		if (it != _customShaders.cend())
			return it->second;

		ShaderProgram* customShader = new ShaderProgram();
		if (!customShader->loadFromFile(shaderFile))
		{
			delete customShader;
			customShader = nullptr;
		}

		_customShaders[shaderFile] = customShader;

		return customShader;	
	}


	class ShaderBatch : public std::vector<ShaderProgram*>
	{
	public:
		static ShaderBatch* getShaderBatch(const char* shaderFile);

		std::map<std::string, std::string> parameters;
	};

	static std::map<std::string, ShaderBatch*> _customShaderBatch;

	ShaderBatch* ShaderBatch::getShaderBatch(const char* shaderFile)
	{
		if (shaderFile == nullptr)
			return nullptr;

		auto it = _customShaderBatch.find(shaderFile);
		if (it != _customShaderBatch.cend())
			return it->second;

		std::string fullPath = ResourceManager::getInstance()->getResourcePath(shaderFile);

		ShaderBatch* ret = new ShaderBatch();

		std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(fullPath));
		if (ext == ".glslp")
		{
			std::string path = Utils::FileSystem::getParent(fullPath);

			std::map<std::string, std::string> confMap;

			std::string line;
			std::ifstream systemConf(fullPath);
			if (systemConf && systemConf.is_open())
			{
				while (std::getline(systemConf, line))
				{
					int idx = line.find("=");
					if (idx == std::string::npos || line.find("#") == 0 || line.find(";") == 0)
						continue;

					std::string key = Utils::String::trim(line.substr(0, idx));
					std::string value = Utils::String::trim(Utils::String::replace(line.substr(idx + 1), "\"", ""));
					if (!key.empty() && !value.empty())
						confMap[key] = value;

				}
				systemConf.close();
			}

			int count = 0;

			auto it = confMap.find("shaders");
			if (it != confMap.cend())
				count = Utils::String::toInteger(it->second);

			for (int i = 0; i < count; i++)
			{
				auto name = "shader" + std::to_string(i);

				it = confMap.find(name);
				if (it == confMap.cend())
					continue;

				std::string relative = it->second;
				if (!Utils::String::startsWith(relative, ":") && !Utils::String::startsWith(relative, "/") && !Utils::String::startsWith(relative, "."))
					relative = "./" + relative;

				std::string full = Utils::FileSystem::resolveRelativePath(relative, path, true);

				ShaderProgram* customShader = getShaderProgram(full.c_str());
				if (customShader != nullptr)
					ret->push_back(customShader);
			}

			it = confMap.find("parameters");
			if (it != confMap.cend())
			{
				for (auto prm : Utils::String::split(it->second, ';', true))
				{
					it = confMap.find(prm);
					if (it != confMap.cend())
						ret->parameters[prm] = it->second;
				}
			}
		}
		else
		{
			ShaderProgram* customShader = getShaderProgram(fullPath.c_str());
			if (customShader != nullptr)
				ret->push_back(customShader);
		}

		_customShaderBatch[shaderFile] = ret;
		return ret;
	};

	static int getAvailableVideoMemory();

	static void setupDefaultShaders()
	{
#if defined(USE_OPENGLES_20)
		SHADER_VERSION_STRING = "#version 100\n";
#else 
		SHADER_VERSION_STRING = "#version 120\n";

		std::string shaders = Utils::String::trim(glGetString(GL_SHADING_LANGUAGE_VERSION) ? (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) : "");

		auto sep = shaders.find_first_of(" -");
		if (sep != std::string::npos)
			shaders = shaders.substr(0, sep);

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
			#ifdef GL_ES
			precision mediump float;
			#endif

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
			varying   vec2 v_pos;

			void main(void)                                    
			{                                                  
			    gl_Position = MVPMatrix * vec4(VertexCoord.xy, 0.0, 1.0);
			    v_tex       = TexCoord;                           
			    v_col       = COLOR;  
				v_pos       = VertexCoord;                         
			}
			)=====";

		// fragment shader (texture)
		std::string fragmentSourceTexture =
			SHADER_VERSION_STRING +
			R"=====(
			#ifdef GL_ES
			precision mediump float;
			precision mediump sampler2D;
			#endif		

			varying   vec4      v_col;
			varying   vec2      v_tex;
			varying   vec2      v_pos;

			uniform   sampler2D u_tex;
			uniform   vec2      outputSize;
			uniform   vec2      outputOffset;
			uniform   float		saturation;
			uniform   float     es_cornerRadius;

			void main(void)                                    
			{                                                  
			    vec4 clr = texture2D(u_tex, v_tex);
		
			    if (saturation != 1.0) {
			    	vec3 gray = vec3(dot(clr.rgb, vec3(0.34, 0.55, 0.11)));
			    	vec3 blend = mix(gray, clr.rgb, saturation);
			    	clr = vec4(blend, clr.a);
			    }

				if (es_cornerRadius != 0.0) {

					vec2 pos = abs(v_pos - outputOffset);
					vec2 middle = vec2(abs(outputSize.x), abs(outputSize.y)) / 2.0;
					vec2 center = abs(v_pos - outputOffset - middle);
					vec2 q = center - middle + es_cornerRadius;
					float distance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - es_cornerRadius;	

					if (distance > 0.0) {
						discard;
					} 
					else if (pos.x >= 1.0 && pos.y >= 1.0 && pos.x <= outputSize.x - 1.0 && pos.y <= outputSize.y - 1.0)
					{
						float pixelValue = 1.0 - smoothstep(-0.75, 0.5, distance);
						clr.a *= pixelValue;						
					}
				}
			
			    gl_FragColor = clr * v_col;
			}
			)=====";

		// Compile each shader, link them to make a full program
		auto vertexShaderTexture = Shader::createShader(GL_VERTEX_SHADER, vertexSourceTexture);
		auto fragmentShaderColorTexture = Shader::createShader(GL_FRAGMENT_SHADER, fragmentSourceTexture);
		shaderProgramColorTexture.createShaderProgram(vertexShaderTexture, fragmentShaderColorTexture);
		
		// fragment shader (alpha texture)
		std::string fragmentSourceAlpha =
			SHADER_VERSION_STRING +
			R"=====(
			#ifdef GL_ES
			precision mediump float;
			precision mediump sampler2D;
			#endif		

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

	#ifndef GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX
	#define GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX 0x9049
	#endif

	static int getAvailableVideoMemory()
	{	
		/*
		const std::string extensions = glGetString(GL_EXTENSIONS) ? (const char*)glGetString(GL_EXTENSIONS) : "";
		if (extensions.find("GL_NVX_gpu_memory_info") != std::string::npos)
		{
			GLint totalMemoryKb = 0;
			glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &totalMemoryKb);
			if (totalMemoryKb != 0)
				return totalMemoryKb / 1024;
		}
		*/
		float total = 0;

		float megabytes = 4.0;
		int sz = sqrtf(megabytes * 1024.0 * 1024.0 / 4.0f);

		std::vector<unsigned int> textures;
		textures.reserve(1000);

		while (true)
		{
			unsigned int textureId = 0;
			glGenTextures(1, &textureId);

			if (textureId == 0 || glGetError() != GL_NO_ERROR)
				break;

			textures.push_back(textureId);

			bindTexture(textureId);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sz, sz, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			if (glGetError() != GL_NO_ERROR)
				break;

			total += megabytes;
		}

		while (glGetError() != GL_NO_ERROR)
			;

		for (auto tx : textures)
			glDeleteTextures(1, &tx);

		return total;
	}

//////////////////////////////////////////////////////////////////////////
	GLES20Renderer::GLES20Renderer() : mFrameBuffer(-1)
	{

	}

	unsigned int GLES20Renderer::getWindowFlags()
	{
		return SDL_WINDOW_OPENGL;

	} // getWindowFlags

//////////////////////////////////////////////////////////////////////////

	void GLES20Renderer::setupWindow()
	{
#if OPENGL_EXTENSIONS
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

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
		return "OPENGL 3.0 / GLSL";
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

		/*
		int videoMem = getTotalMemUsage() / 1024.0 / 1024.0;
		info.push_back(std::pair<std::string, std::string>("USED VRAM", std::to_string(videoMem) + " MB"));

		videoMem = getAvailableVideoMemory();
		info.push_back(std::pair<std::string, std::string>("FREE VRAM", std::to_string(videoMem) + " MB"));
		*/
		return info;
	}

	void GLES20Renderer::createContext()
	{
		sdlContext = SDL_GL_CreateContext(getSDLWindow());
		
#if OPENGL_EXTENSIONS
		if (sdlContext == nullptr)
		{
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

			sdlContext = SDL_GL_CreateContext(getSDLWindow());
			if (sdlContext == nullptr)
			{
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

				sdlContext = SDL_GL_CreateContext(getSDLWindow());
			}
		}
#endif

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
		bindTexture(0);

		for (auto customShader : _customShaderBatch)
		{
			if (customShader.second != nullptr)
			{
				customShader.second->clear();
				delete customShader.second;
			}
		}

		_customShaderBatch.clear();

		for (auto customShader : _customShaders)
		{
			if (customShader.second != nullptr)
			{
				customShader.second->deleteProgram();
				delete customShader.second;
			}
		}

		_customShaders.clear();

		if (mFrameBuffer != -1)
		{
			GL_CHECK_ERROR(glDeleteFramebuffers(1, &mFrameBuffer));
			mFrameBuffer = -1;
		}
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

		unsigned int texture = -1;
		GL_CHECK_ERROR(glGenTextures(1, &texture));

		if (texture == -1)
		{
			LOG(LogError) << "CreateTexture error: glGenTextures failed ";
			return 0;
		}
		
		bindTexture(0);
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

		//	while (glGetError() != GL_NO_ERROR);

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
		//	while (glGetError() != GL_NO_ERROR);

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
	void GLES20Renderer::drawSolidRectangle(const float _x, const float _y, const float _w, const float _h, const unsigned int _fillColor, const unsigned int _borderColor, float borderWidth, float cornerRadius)
	{
		if (cornerRadius == 0.0f)
		{
			if (_fillColor != 0)
				drawRect(_x + borderWidth, _y + borderWidth, _w - borderWidth - borderWidth, _h - borderWidth - borderWidth, _fillColor);

			if (_borderColor != 0 && borderWidth > 0)
			{
				drawRect(_x, _y, _w, borderWidth, _borderColor);
				drawRect(_x + _w - borderWidth, _y + borderWidth, borderWidth, _h - borderWidth, _borderColor);
				drawRect(_x, _y + _h - borderWidth, _w - borderWidth, borderWidth, _borderColor);
				drawRect(_x, _y + borderWidth, borderWidth, _h - borderWidth - borderWidth, _borderColor);
			}
			return;
		}

		bindTexture(0);
		useProgram(&shaderProgramColorNoTexture);

		GL_CHECK_ERROR(glEnable(GL_BLEND));
		GL_CHECK_ERROR(glBlendFunc(convertBlendFactor(Blend::SRC_ALPHA), convertBlendFactor(Blend::ONE_MINUS_SRC_ALPHA)));

		auto inner = createRoundRect(_x + borderWidth, _y + borderWidth, _w - borderWidth - borderWidth, _h - borderWidth - borderWidth, cornerRadius, _fillColor);

		if ((_fillColor) & 0xFF)
		{
			GL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * inner.size(), inner.data(), GL_DYNAMIC_DRAW));
			GL_CHECK_ERROR(glDrawArrays(GL_TRIANGLE_FAN, 0, inner.size()));
		}

		if ((_borderColor) & 0xFF && borderWidth > 0)
		{
			auto outer = createRoundRect(_x, _y, _w, _h, cornerRadius, _borderColor);

			setStencil(inner.data(), inner.size());
			GL_CHECK_ERROR(glStencilFunc(GL_NOTEQUAL, 1, ~0));

			GL_CHECK_ERROR(glEnable(GL_BLEND));
			GL_CHECK_ERROR(glBlendFunc(convertBlendFactor(Blend::SRC_ALPHA), convertBlendFactor(Blend::ONE_MINUS_SRC_ALPHA)));

			GL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * outer.size(), outer.data(), GL_DYNAMIC_DRAW));
			GL_CHECK_ERROR(glDrawArrays(GL_TRIANGLE_FAN, 0, outer.size()));
			
			disableStencil();
		}

		GL_CHECK_ERROR(glDisable(GL_BLEND));
	}

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

				if (_vertices->customShader != nullptr && !_vertices->customShader->path.empty())
				{
					ShaderProgram* customShader = getShaderProgram(_vertices->customShader->path.c_str());
					if (customShader != nullptr)
						shader = customShader;
				}

				useProgram(shader);

				// Update Shader Uniforms				
				shader->setSaturation(_vertices->saturation);
				shader->setCornerRadius(_vertices->cornerRadius);
				shader->setResolution();
				shader->setFrameCount(Renderer::getCurrentFrame());

				if (shader->supportsTextureSize() && it != _textures.cend() && it->second != nullptr)
				{
					shader->setInputSize(it->second->size);
					shader->setTextureSize(it->second->size);
				}
				
				if (_numVertices > 0)
				{
					Vector2f vec = _vertices[_numVertices - 1].pos;
					if (_numVertices == 4)
					{
						vec.x() -= _vertices[0].pos.x();
						vec.y() -= _vertices[0].pos.y();
					}

					// Inverted rendering
					if (_vertices[_numVertices - 1].tex.y() == 1 && _vertices[0].tex.y() == 0)
						vec.y() = -vec.y();

					shader->setOutputSize(vec);						
					shader->setOutputOffset(_vertices[0].pos);
				}

				if (_vertices->customShader != nullptr && !_vertices->customShader->path.empty())
					shader->setCustomUniformsParameters(_vertices->customShader->parameters);
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
		// worldViewMatrix.round();
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
			if (SDL_GL_SetSwapInterval(1) != 0 && SDL_GL_SetSwapInterval(-1) != 0)
				LOG(LogWarning) << "Tried to enable vsync, but failed! (" << SDL_GetError() << ")";
		}
		else
			SDL_GL_SetSwapInterval(0);

	} // setSwapInterval

//////////////////////////////////////////////////////////////////////////

	void GLES20Renderer::swapBuffers()
	{
		useProgram(nullptr);

#ifdef WIN32		
		glFlush();
		Sleep(0);
#endif
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
				shaderProgramColorTexture.setCornerRadius(0.0f);
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

		glClearStencil(0);
		glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_FALSE);

		glStencilFunc(GL_ALWAYS, 1, ~0);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

		glEnable(GL_BLEND);
		glBlendFunc(convertBlendFactor(Blend::SRC_ALPHA), convertBlendFactor(Blend::ONE_MINUS_SRC_ALPHA));
		glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * _numVertices, _vertices, GL_DYNAMIC_DRAW);
		glDrawArrays(GL_TRIANGLE_FAN, 0, _numVertices);
		glDisable(GL_BLEND);

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_TRUE);

		glStencilFunc(GL_EQUAL, 1, ~0);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	}

	void GLES20Renderer::disableStencil()
	{
		glDisable(GL_STENCIL_TEST);
	}

	size_t GLES20Renderer::getTotalMemUsage()
	{
		size_t total = 0;

		for (auto tex : _textures)
		{
			if (tex.first != 0 && tex.second)
			{
				size_t size = tex.second->size.x() * tex.second->size.y() * (tex.second->type == GL_ALPHA ? 1 : 4);
				total += size;
			}
		}	

		return total;
	}

	bool GLES20Renderer::shaderSupportsCornerSize(const std::string& shader)
	{
		ShaderProgram* customShader = getShaderProgram(shader.c_str());
		if (customShader == nullptr)
			customShader = &shaderProgramColorTexture;
			
		return customShader->supportsCornerRadius();
	}

	void GLES20Renderer::postProcessShader(const std::string& path, const float _x, const float _y, const float _w, const float _h, const std::map<std::string, std::string>& parameters, unsigned int* data)
	{
#if OPENGL_EXTENSIONS
		if (glBlitFramebuffer == nullptr || glFramebufferTexture2D == nullptr)
			return;

		if (getScreenRotate() != 0 && getScreenRotate() != 2)
			return;

		ShaderBatch* shaderBatch = ShaderBatch::getShaderBatch(path.c_str());
		if (shaderBatch == nullptr || shaderBatch->size() == 0)
			return;

		if (mFrameBuffer == -1)
			GL_CHECK_ERROR(glGenFramebuffers(1, &mFrameBuffer));

		if (mFrameBuffer == -1)
			return;

		float textureScale = 1.0f;
		
		// Special hack for single-pass blur shader -> Texture downscaling
		if (path == ":/shaders/blur.glsl" && data == nullptr)
		{
			auto it = parameters.find("blur");
			if (it != parameters.cend())
			{
				float blurSize = Utils::String::toFloat(it->second);

				textureScale = blurSize / 5.0f;
				if (textureScale < 1.0f)
					textureScale = 1.0f;
			}
		}


		int x = _x; int y = _y; int w = _w; int h = _h;

		if (y < 0) 
		{
			h += y; y = 0;
			if (h <= 0)
				return;
		}

		if (x < 0)
		{
			w += x; x = 0;
			if (w <= 0) 
				return;
		}

		int tw = w / textureScale;
		int th = h / textureScale;

		auto nTextureID = createTexture(Renderer::Texture::RGBA, true, false, tw, th, nullptr);
		if (nTextureID > 0)
		{
			int width = getScreenWidth();
			int height = getScreenHeight();

			unsigned int nFrameBuffer2 = -1;
			unsigned int nTexture2 = -1;

			if (shaderBatch->size() > 1 || data != nullptr)
			{
				// Multiple passes ? We need another framebuffer + another texture
				glGenFramebuffers(1, &nFrameBuffer2);
				nTexture2 = createTexture(Renderer::Texture::RGBA, true, false, tw, th, nullptr);
			}

			auto oldProgram = currentProgram;
			auto oldMatrix = worldViewMatrix;

			GLboolean oldCissors;
			GL_CHECK_ERROR(glGetBooleanv(GL_SCISSOR_TEST, &oldCissors));
			GL_CHECK_ERROR(glDisable(GL_SCISSOR_TEST));

			setMatrix(Transform4x4f::Identity());

			Vertex vertices[4];
				
			if (shaderBatch->size() == 1 && data == nullptr)
			{
				vertices[0] = { { (float)x    , (float)y       }, { 0.0f, 1.0f }, 0xFFFFFFFF };
				vertices[1] = { { (float)x    , (float)y + h   }, { 0.0f, 0.0f }, 0xFFFFFFFF };
				vertices[2] = { { (float)x + w, (float)y       }, { 1.0f, 1.0f }, 0xFFFFFFFF };
				vertices[3] = { { (float)x + w, (float)y + h   }, { 1.0f, 0.0f }, 0xFFFFFFFF };

				if (getScreenRotate() == 2)
				{
					vertices[0] = { { (float)x    , (float)y       }, { 1.0f, 0.0f }, 0xFFFFFFFF };
					vertices[1] = { { (float)x    , (float)y + h   }, { 1.0f, 1.0f }, 0xFFFFFFFF };
					vertices[2] = { { (float)x + w, (float)y       }, { 0.0f, 0.0f }, 0xFFFFFFFF };
					vertices[3] = { { (float)x + w, (float)y + h   }, { 0.0f, 1.0f }, 0xFFFFFFFF };
				}
			}
			else
			{			
				vertices[0] = { { (float)0    , (float)height - h }, { 0.0f, 1.0f }, 0xFFFFFFFF };
				vertices[1] = { { (float)0    , (float)height },     { 0.0f, 0.0f }, 0xFFFFFFFF };
				vertices[2] = { { (float)0 + w, (float)height - h }, { 1.0f, 1.0f }, 0xFFFFFFFF };
				vertices[3] = { { (float)0 + w, (float)height  },    { 1.0f, 0.0f }, 0xFFFFFFFF };
			}
			
			// round vertices
			for (int i = 0; i < 4; ++i)
				vertices[i].pos.round();

			GL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * 4, &vertices, GL_DYNAMIC_DRAW));

			for (int i = 0; i < shaderBatch->size(); i++)
			{
				auto customShader = shaderBatch->at(i);

				if (i == 0)
				{
					bindTexture(nTextureID);

					GL_CHECK_ERROR(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mFrameBuffer));
					GL_CHECK_ERROR(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, nTextureID, 0));

					if (getScreenRotate() == 2)
						GL_CHECK_ERROR(glBlitFramebuffer(x, y, x + w, y + h, 0, 0, tw, th, GL_COLOR_BUFFER_BIT, GL_LINEAR));
					else
						GL_CHECK_ERROR(glBlitFramebuffer(x, height - y - h, x + w, height - y, 0, 0, tw, th, GL_COLOR_BUFFER_BIT, GL_LINEAR));

					if (shaderBatch->size() == 1 && data == nullptr)
						GL_CHECK_ERROR(glBindFramebuffer(GL_FRAMEBUFFER, 0));
					else
					{
						GL_CHECK_ERROR(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, nFrameBuffer2));
						GL_CHECK_ERROR(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, nTexture2, 0));
					}
				}
				else
				{
					bindTexture(i % 2 == 1 ? nTexture2 : nTextureID);

					if (i == shaderBatch->size() - 1 && data == nullptr)
					{
						// This is the last shader in the batch. 
						vertices[0] = { { (float)x    , (float)y       }, { 0.0f, 1.0f }, 0xFFFFFFFF };
						vertices[1] = { { (float)x    , (float)y + h   }, { 0.0f, 0.0f }, 0xFFFFFFFF };
						vertices[2] = { { (float)x + w, (float)y       }, { 1.0f, 1.0f }, 0xFFFFFFFF };
						vertices[3] = { { (float)x + w, (float)y + h   }, { 1.0f, 0.0f }, 0xFFFFFFFF };

						if (getScreenRotate() == 2)
						{
							vertices[0] = { { (float)x    , (float)y       }, { 1.0f, 0.0f }, 0xFFFFFFFF };
							vertices[1] = { { (float)x    , (float)y + h   }, { 1.0f, 1.0f }, 0xFFFFFFFF };
							vertices[2] = { { (float)x + w, (float)y       }, { 0.0f, 0.0f }, 0xFFFFFFFF };
							vertices[3] = { { (float)x + w, (float)y + h   }, { 0.0f, 1.0f }, 0xFFFFFFFF };
						}

						for (int i = 0; i < 4; ++i) vertices[i].pos.round();

						GL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * 4, &vertices, GL_DYNAMIC_DRAW));

						GL_CHECK_ERROR(glBindFramebuffer(GL_FRAMEBUFFER, 0));
					}
					else
						GL_CHECK_ERROR(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, i % 2 == 1 ? mFrameBuffer : nFrameBuffer2));
				}

				useProgram(customShader);

				customShader->setSaturation(1.0f);
				customShader->setCornerRadius(0.0f);
				customShader->setTextureSize(Vector2f(tw, th));
				customShader->setInputSize(Vector2f(tw, th));
				customShader->setOutputSize(vertices[3].pos);
				customShader->setOutputOffset(vertices[0].pos);
				customShader->setResolution();
				customShader->setFrameCount(Renderer::getCurrentFrame());

				// Parameters in the glslp
				std::map<std::string, std::string> params = shaderBatch->parameters;

				// Parameters in the theme
				for (const auto& entry : parameters)
					params[entry.first] = entry.second;

				customShader->setCustomUniformsParameters(params);

				GL_CHECK_ERROR(glDisable(GL_BLEND));
				GL_CHECK_ERROR(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
			}

			if (data != nullptr)
			{				
				GL_CHECK_ERROR(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0)); // Detach
				GL_CHECK_ERROR(glBindFramebuffer(GL_FRAMEBUFFER, 0));

				bool takeFirst = (shaderBatch->size() - 1) % 2 == 1;
				*data = takeFirst ? nTextureID : nTexture2;
				destroyTexture(takeFirst ? nTexture2 : nTextureID);
			}
			else
			{
				bindTexture(0);
				useProgram(oldProgram);
				setMatrix(oldMatrix);

				if (oldCissors)
					GL_CHECK_ERROR(glEnable(GL_SCISSOR_TEST));

				destroyTexture(nTextureID);

				if (nTexture2 != -1)
					destroyTexture(nTexture2);
			}

			if (nFrameBuffer2 != -1)
				GL_CHECK_ERROR(glDeleteFramebuffers(1, &nFrameBuffer2));
		}		
#endif
	}
	
} // Renderer::

#endif // USE_OPENGLES_20
