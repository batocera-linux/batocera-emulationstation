#include "Shader.h"
#include "Log.h"
#include "renderers/Renderer.h"
#include "resources/ResourceManager.h"
#include "utils/StringUtil.h"
#include "utils/HtmlColor.h"

#include <set>
#include <cstring>

#if defined(USE_OPENGLES_30) && defined(NDEBUG)
#define SHADER_GL_CALL(Function) (Function)
#else
#define SHADER_GL_CALL(Function) GL_CHECK_ERROR(Function)
#endif

namespace Renderer
{
	std::string SHADER_VERSION_STRING;

	Shader Shader::createShader(GLenum type, const std::string& source)
	{
		const GLuint shaderId = glCreateShader(type);

		Shader ret;
		if (!ret.compile(shaderId, source.c_str(), type))
			glDeleteShader(shaderId);
		return ret;
	}

	void Shader::deleteShader()
	{
		if (id >= 0 && compileStatus)
		{
			glDeleteShader(id);
			compileStatus = false;
			id = -1;
		}
	}

	bool Shader::compile(GLuint id, const char* source, GLenum type)
	{
		// Try to compile GLSL source code
		SHADER_GL_CALL(glShaderSource(id, 1, &source, nullptr));
		SHADER_GL_CALL(glCompileShader(id));

		// Check compile status (ok, warning, error)
		GLint isCompiled = GL_FALSE;
		GLint maxLength = 0;
		SHADER_GL_CALL(glGetShaderiv(id, GL_COMPILE_STATUS, &isCompiled));
		SHADER_GL_CALL(glGetShaderiv(id, GL_INFO_LOG_LENGTH, &maxLength));

		// Read log if any
		if (maxLength > 1)
		{
			char* infoLog = new char[maxLength + 1];

			SHADER_GL_CALL(glGetShaderInfoLog(id, maxLength, &maxLength, infoLog));

			std::string shaderType = (type == GL_FRAGMENT_SHADER) ? "Fragment" : "Vertex";

			if (isCompiled == GL_FALSE)
			{
				LOG(LogError) << "GLSL " << shaderType << " Compile Error\n" << infoLog;
				delete[] infoLog;
				return false;
			}
			else
			{
				if (strstr(infoLog, "WARNING") || strstr(infoLog, "warning") || strstr(infoLog, "Warning"))
					LOG(LogWarning) << "GLSL " << shaderType << " Compile Warning\n" << infoLog;
				else
					LOG(LogInfo) << "GLSL " << shaderType << " Compile Message\n" << infoLog;

				delete[] infoLog;
			}
		}

		// Compile OK ? Affect shader id
		compileStatus = isCompiled;
		if (compileStatus == GL_TRUE)
		{
			this->id = id;
			return true;
		}

		return false;
	}

	ShaderProgram::ShaderProgram() :
		mId(-1), linkStatus(false),
		mPositionAttribute(-1), mColorAttribute(-1), mTexCoordAttribute(-1), mvpUniform(-1),
		mSaturation(-1), mTextureSize(-1), mOutputSize(-1), mOutputOffset(-1), mInputSize(-1),
		mResolution(-1), mCornerRadius(-1), mFrameCount(-1), mFrameDirection(-1)
#if defined(USE_OPENGLES_30)
		, mSamplerUniform(-1), mSamplerInitialized(false), mVertexArray(0)
#endif
	{
	}

	void ShaderProgram::deleteProgram()
	{
#if defined(USE_OPENGLES_30)
		if (mVertexArray != 0)
		{
			SHADER_GL_CALL(glDeleteVertexArrays(1, &mVertexArray));
			mVertexArray = 0;
		}
#endif

		if (mId >= 0)
		{
			for (auto shader : mAttachedShaders)
				shader.deleteShader();

			mAttachedShaders.clear();

			SHADER_GL_CALL(glDeleteProgram(mId));
			mId = -1;
		}
	}

	static std::string appendVersionAndType(const std::string& shaderCode, const std::string& customDefines, const std::string& defaultVersion)
	{
		auto pos = shaderCode.find("#version");
		if (pos != std::string::npos)
		{
			auto next = shaderCode.find("\n", pos);
			if (next != std::string::npos && next + 1 < shaderCode.length())
				return shaderCode.substr(0, next + 1) + customDefines + "\n" + shaderCode.substr(next + 1);
		}

		return defaultVersion + customDefines + "\n" + shaderCode;
	}

	bool ShaderProgram::loadFromFile(const std::string& path)
	{
		if (!ResourceManager::getInstance()->fileExists(path))
			return false;
		
		// This will load the entire GLSL source code into the string variable.
		const ResourceData& shaderData = ResourceManager::getInstance()->getFileData(path);

		std::string shaderCode;
		shaderCode.assign(reinterpret_cast<const char*>(shaderData.ptr.get()), shaderData.length);

		Shader vertex = Shader::createShader(GL_VERTEX_SHADER, appendVersionAndType(shaderCode, "#define VERTEX", SHADER_VERSION_STRING));
		Shader fragment;
		if (vertex.compileStatus)
			fragment = Shader::createShader(GL_FRAGMENT_SHADER, appendVersionAndType(shaderCode, "#define FRAGMENT", SHADER_VERSION_STRING));

#if defined(USE_OPENGLES_30)
		// Theme shaders written for GLES2 often omit #version. Prefer native GLSL ES
		// 3.00, but retain compatibility with those shaders when they use legacy syntax.
		if ((!vertex.compileStatus || !fragment.compileStatus) && shaderCode.find("#version") == std::string::npos)
		{
			vertex.deleteShader();
			fragment.deleteShader();
			LOG(LogWarning) << "Retrying legacy GLSL ES 1.00 shader under OpenGL ES 3.0: " << path;
			const std::string legacyVersion = "#version 100\n";
			vertex = Shader::createShader(GL_VERTEX_SHADER, appendVersionAndType(shaderCode, "#define VERTEX", legacyVersion));
			if (vertex.compileStatus)
				fragment = Shader::createShader(GL_FRAGMENT_SHADER, appendVersionAndType(shaderCode, "#define FRAGMENT", legacyVersion));
		}
#endif

		if (!vertex.compileStatus)
		{
			LOG(LogError) << "Failed to compile GLSL VERTEX shader : " << path;
			return false;
		}

		if (!fragment.compileStatus)
		{
			vertex.deleteShader();
			LOG(LogError) << "Failed to compile GLSL FRAGMENT shader : " << path;
			return false;
		}

		return createShaderProgram(vertex, fragment);
	}

	bool ShaderProgram::createShaderProgram(Shader &vertexShader, Shader &fragmentShader)
	{
		GLuint programId = glCreateProgram();

		SHADER_GL_CALL(glAttachShader(programId, vertexShader.id));
		SHADER_GL_CALL(glAttachShader(programId, fragmentShader.id));

		SHADER_GL_CALL(glLinkProgram(programId));

		GLint isCompiled = GL_FALSE;
		GLint maxLength = 0;

		SHADER_GL_CALL(glGetProgramiv(programId, GL_LINK_STATUS, &isCompiled));
		SHADER_GL_CALL(glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &maxLength));

		if (maxLength > 1)
		{
			char* infoLog = new char[maxLength + 1];

			SHADER_GL_CALL(glGetProgramInfoLog(programId, maxLength, &maxLength, infoLog));

			if (isCompiled == GL_FALSE)
			{
				LOG(LogError) << "GLSL Link (Texture) Error\n" << infoLog;
				delete[] infoLog;
				return false;
			}
			else
			{
				if (strstr(infoLog, "WARNING") || strstr(infoLog, "warning") || strstr(infoLog, "Warning"))
					LOG(LogWarning) << "GLSL Link (Texture) Warning\n" << infoLog;
				else
					LOG(LogInfo) << "GLSL Link (Texture) Message\n" << infoLog;
				delete[] infoLog;
			}
		}

		mAttachedShaders.push_back(vertexShader);
		mAttachedShaders.push_back(fragmentShader);

		// Compile OK ? Affect program id
		this->linkStatus = isCompiled;
		if (this->linkStatus == GL_TRUE)
		{
			this->mId = programId;
			findAttribsAndUniforms();
			return true;
		}

		return false;
	}

	static std::set<std::string> builtInUniforms =
	{
		"MVPMatrix", 
		"VertexCoord", "positionVertex", 
		"TexCoord", "texCoordVertex", 
		"COLOR", "colorVertex", 
		"TextureSize", "textureSize", 
		"OutputSize", "outputSize", 
		"OutputOffset", "outputOffset",
		"InputSize", "inputSize", 
		"Resolution", "resolution", 
		"saturation", "es_cornerRadius",
		"FrameCount", "FrameDirection",
		"u_tex", "textureSampler", "Texture"
	};

	void ShaderProgram::findAttribsAndUniforms()
	{
#if defined(USE_OPENGLES_30)
		invalidateUniformCache();
#endif

		// Matrix
		mvpUniform = glGetUniformLocation(mId, "MVPMatrix");

		// Attribs
		mPositionAttribute = glGetAttribLocation(mId, "VertexCoord");
		if (mPositionAttribute == -1)
			mPositionAttribute = glGetAttribLocation(mId, "positionVertex");

		mTexCoordAttribute = glGetAttribLocation(mId, "TexCoord");
		if (mTexCoordAttribute == -1)
			mTexCoordAttribute = glGetAttribLocation(mId, "texCoordVertex");
		
		mColorAttribute = glGetAttribLocation(mId, "COLOR");
		if (mColorAttribute == -1)
			mColorAttribute = glGetAttribLocation(mId, "colorVertex");
		
		// Uniforms
		mTextureSize = glGetUniformLocation(mId, "TextureSize");
		if (mTextureSize == -1)
			mTextureSize = glGetUniformLocation(mId, "textureSize");
		
		mOutputSize = glGetUniformLocation(mId, "OutputSize");
		if (mOutputSize == -1)
			mOutputSize = glGetUniformLocation(mId, "outputSize");

		mOutputOffset = glGetUniformLocation(mId, "OutputOffset");
		if (mOutputOffset == -1)
			mOutputOffset = glGetUniformLocation(mId, "outputOffset");

		mInputSize = glGetUniformLocation(mId, "InputSize");
		if (mInputSize == -1)
			mInputSize = glGetUniformLocation(mId, "inputSize");

		mResolution = glGetUniformLocation(mId, "Resolution");
		if (mResolution == -1)
			mResolution = glGetUniformLocation(mId, "resolution");

		mSaturation = glGetUniformLocation(mId, "saturation");
		mCornerRadius = glGetUniformLocation(mId, "es_cornerRadius");

#if defined(USE_OPENGLES_30)
		mSamplerUniform = glGetUniformLocation(mId, "u_tex");
		mSamplerInitialized = false;
		if (mSamplerUniform == -1)
			mSamplerUniform = glGetUniformLocation(mId, "textureSampler");
		if (mSamplerUniform == -1)
			mSamplerUniform = glGetUniformLocation(mId, "Texture");
#else
		GLint texUniform = glGetUniformLocation(mId, "u_tex");
		if (texUniform == -1)
			texUniform = glGetUniformLocation(mId, "textureSampler");
		if (texUniform == -1)
			texUniform = glGetUniformLocation(mId, "Texture");
#endif
		
		mFrameCount = glGetUniformLocation(mId, "FrameCount");
		mFrameDirection = glGetUniformLocation(mId, "FrameDirection");

		GLint numUniforms = 0;
		SHADER_GL_CALL(glGetProgramiv(mId, GL_ACTIVE_UNIFORMS, &numUniforms));

		for (int i = 0; i < numUniforms; ++i) 
		{
			GLenum type;
			GLsizei length;
			GLint size;

			char buffer[256];
			SHADER_GL_CALL(glGetActiveUniform(mId, i, (GLsizei)sizeof(buffer), &length, &size, &type, buffer));

			std::string uniformName = buffer;
			if (builtInUniforms.find(uniformName) != builtInUniforms.cend())
				continue;

			UniformInfo info;
			info.location = glGetUniformLocation(mId, uniformName.c_str());
			info.type = type;

			mCustomUniforms[uniformName] = info;
		}

#if !defined(USE_OPENGLES_30)
		if (texUniform != -1)
		{
			SHADER_GL_CALL(glUseProgram(mId));
			SHADER_GL_CALL(glUniform1i(texUniform, 0));
		}
#endif
	}

	// Skips redundant uniform writes on ES 3.0. Other backends keep the original
	// unconditional behaviour.
#if defined(USE_OPENGLES_30)
	#define SHADER_UNIFORM_CACHED(field, expression)                 \
		if (mUniformCache.field##Valid && mUniformCache.field == (expression)) \
			return;                                                  \
		mUniformCache.field = (expression);                          \
		mUniformCache.field##Valid = true;
#else
	#define SHADER_UNIFORM_CACHED(field, expression) (void)0;
#endif

	void ShaderProgram::setMatrix(Transform4x4f& mvpMatrix)
	{
		if (mvpUniform == -1 || mvpUniform == GL_INVALID_VALUE || mvpUniform == GL_INVALID_OPERATION)
			return;

#if defined(USE_OPENGLES_30)
		const float* values = (const float*)&mvpMatrix;
		if (mUniformCache.matrixValid && memcmp(mUniformCache.matrix, values, sizeof(mUniformCache.matrix)) == 0)
			return;

		memcpy(mUniformCache.matrix, values, sizeof(mUniformCache.matrix));
		mUniformCache.matrixValid = true;
#endif

		SHADER_GL_CALL(glUniformMatrix4fv(mvpUniform, 1, GL_FALSE, (float*)&mvpMatrix));
	}

	void ShaderProgram::setSaturation(GLfloat saturation)
	{
		if (mSaturation == -1)
			return;

		SHADER_UNIFORM_CACHED(saturation, saturation)
		SHADER_GL_CALL(glUniform1f(mSaturation, saturation));
	}

	void ShaderProgram::setCornerRadius(GLfloat radius)
	{
		if (mCornerRadius == -1)
			return;

		SHADER_UNIFORM_CACHED(cornerRadius, radius)
		SHADER_GL_CALL(glUniform1f(mCornerRadius, radius));
	}	

	void ShaderProgram::setTextureSize(const Vector2f& size)
	{
		if (mTextureSize == -1)
			return;

		SHADER_UNIFORM_CACHED(textureSize, size)
		SHADER_GL_CALL(glUniform2f(mTextureSize, size.x(), size.y()));
	}
	
	void ShaderProgram::setInputSize(const Vector2f& size)
	{
		if (mInputSize == -1)
			return;

		SHADER_UNIFORM_CACHED(inputSize, size)
		SHADER_GL_CALL(glUniform2f(mInputSize, size.x(), size.y()));
	}

	void ShaderProgram::setOutputSize(const Vector2f& size)
	{
		if (mOutputSize == -1)
			return;

		SHADER_UNIFORM_CACHED(outputSize, size)
		SHADER_GL_CALL(glUniform2f(mOutputSize, size.x(), size.y()));
	}

	void ShaderProgram::setOutputOffset(const Vector2f& size)
	{
		if (mOutputOffset == -1)
			return;

		SHADER_UNIFORM_CACHED(outputOffset, size)
		SHADER_GL_CALL(glUniform2f(mOutputOffset, size.x(), size.y()));
	}

	void ShaderProgram::setResolution()
	{
		if (mResolution == -1)
			return;

		const Vector2f resolution((float)getScreenWidth(), (float)getScreenHeight());

		SHADER_UNIFORM_CACHED(resolution, resolution)
		SHADER_GL_CALL(glUniform2f(mResolution, resolution.x(), resolution.y()));
	}

	void ShaderProgram::setFrameCount(int frame)
	{
#if defined(USE_OPENGLES_30)
		if (mUniformCache.frameCountValid && mUniformCache.frameCount == frame)
			return;

		mUniformCache.frameCount = frame;
		mUniformCache.frameCountValid = true;
#endif

		if (mFrameDirection != -1)
#if defined(USE_OPENGLES_30)
			SHADER_GL_CALL(glUniform1i(mFrameDirection, 1));
#else
			SHADER_GL_CALL(glUniform1i(mFrameCount, 1));
#endif

		if (mFrameCount != -1)
			SHADER_GL_CALL(glUniform1i(mFrameCount, frame));
	}

	void ShaderProgram::setCustomUniformsParameters(const std::map<std::string, std::string>& parameters)
	{
		// Reset values of custom uniforms that are not present in the parameters
		for (auto item : mCustomUniforms)
		{
			if (parameters.find(item.first) != parameters.cend())
				continue;

			switch (item.second.type)
			{
			case GL_INT:
				SHADER_GL_CALL(glUniform1i(item.second.location, 0));
				break;
			case GL_FLOAT:
				SHADER_GL_CALL(glUniform1f(item.second.location, 0.0f));
				break;
			case GL_FLOAT_VEC2:
				SHADER_GL_CALL(glUniform2f(item.second.location, 0.0f, 0.0f));
				break;
			case GL_FLOAT_VEC4:
				SHADER_GL_CALL(glUniform4f(item.second.location, 0.0f, 0.0f, 0.0f, 0.0f));
				break;
			case GL_BOOL:
				SHADER_GL_CALL(glUniform1i(item.second.location, GL_FALSE));
				break;
			default:
				break;
			}
		}

		for (auto param : parameters)
			setUniformEx(param.first, param.second);
	}

	void ShaderProgram::setUniformEx(const std::string& name, const std::string value)
	{
		auto it = mCustomUniforms.find(name);
		if (it == mCustomUniforms.cend())
			return;

		GLint location = it->second.location;

		switch (it->second.type)
		{
		case GL_INT:
			SHADER_GL_CALL(glUniform1i(location, Utils::String::toInteger(value)));
			break;
		case GL_FLOAT:
			SHADER_GL_CALL(glUniform1f(location, Utils::String::toFloat(value)));
			break;
		case GL_BOOL:
			SHADER_GL_CALL(glUniform1i(location, Utils::String::toBoolean(value) ? GL_TRUE : GL_FALSE));
			break;
		case GL_FLOAT_VEC2:
			{
				auto size = Vector2f::parseString(value);
				SHADER_GL_CALL(glUniform2f(location, size.x(), size.y()));
			}
			break;
		case GL_FLOAT_VEC4:
		{
			if (value != "0" && value != "1" && value.find(" ") == std::string::npos && value.find(".") == std::string::npos)
			{
				// It's a color
				auto clr = Utils::HtmlColor::parse(value);

				unsigned char red = (clr >> 24) & 0xFF;
				unsigned char green = (clr >> 16) & 0xFF;
				unsigned char blue = (clr >> 8) & 0xFF;
				unsigned char alpha = clr & 0xFF;

				SHADER_GL_CALL(glUniform4f(location, red / 255.0f, green / 255.0f, blue / 255.0f, alpha / 255.0f));
			}
			else  if (value.find(".00") != std::string::npos)
			{
				// It's a float
				auto clr = (int) Utils::String::toDouble(value);

				unsigned char red = (clr >> 24) & 0xFF;
				unsigned char green = (clr >> 16) & 0xFF;
				unsigned char blue = (clr >> 8) & 0xFF;
				unsigned char alpha = clr & 0xFF;

				SHADER_GL_CALL(glUniform4f(location, red / 255.0f, green / 255.0f, blue / 255.0f, alpha / 255.0f));
			}
			else
			{
				// Coordinates
				auto size = Vector4f::parseString(value);
				SHADER_GL_CALL(glUniform4f(location, size.x(), size.y(), size.z(), size.w()));
			}

		}
		break;
		}
	}

#if defined(USE_OPENGLES_30)
	void ShaderProgram::select(GLuint vertexBuffer, GLuint indexBuffer)
#else
	void ShaderProgram::select()
#endif
	{
		SHADER_GL_CALL(glUseProgram(mId));

#if defined(USE_OPENGLES_30)
		if (mSamplerUniform != -1 && !mSamplerInitialized)
		{
			SHADER_GL_CALL(glUniform1i(mSamplerUniform, 0));
			mSamplerInitialized = true;
		}

		if (mVertexArray == 0)
		{
			SHADER_GL_CALL(glGenVertexArrays(1, &mVertexArray));
			SHADER_GL_CALL(glBindVertexArray(mVertexArray));
			SHADER_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer));

			// Element array bindings are vertex array state. Zero when unused.
			SHADER_GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer));

			if (mPositionAttribute != -1)
			{
				SHADER_GL_CALL(glVertexAttribPointer(mPositionAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (const void*)offsetof(GpuVertex, x)));
				SHADER_GL_CALL(glEnableVertexAttribArray(mPositionAttribute));
			}

			if (mColorAttribute != -1)
			{
				SHADER_GL_CALL(glVertexAttribPointer(mColorAttribute, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GpuVertex), (const void*)offsetof(GpuVertex, col)));
				SHADER_GL_CALL(glEnableVertexAttribArray(mColorAttribute));
			}

			if (mTexCoordAttribute != -1)
			{
				SHADER_GL_CALL(glVertexAttribPointer(mTexCoordAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (const void*)offsetof(GpuVertex, u)));
				SHADER_GL_CALL(glEnableVertexAttribArray(mTexCoordAttribute));
			}
		}
		else
			SHADER_GL_CALL(glBindVertexArray(mVertexArray));
#else
		if (mPositionAttribute != -1)
		{
			SHADER_GL_CALL(glVertexAttribPointer(mPositionAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, pos)));
			SHADER_GL_CALL(glEnableVertexAttribArray(mPositionAttribute));
		}

		if (mColorAttribute != -1)
		{
			SHADER_GL_CALL(glVertexAttribPointer(mColorAttribute, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (const void*)offsetof(Vertex, col)));
			SHADER_GL_CALL(glEnableVertexAttribArray(mColorAttribute));
		}

		if (mTexCoordAttribute != -1)
		{
			SHADER_GL_CALL(glVertexAttribPointer(mTexCoordAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, tex)));
			SHADER_GL_CALL(glEnableVertexAttribArray(mTexCoordAttribute));
		}
#endif
	}

	void ShaderProgram::unSelect()
	{
#if defined(USE_OPENGLES_30)
		SHADER_GL_CALL(glBindVertexArray(0));
		SHADER_GL_CALL(glUseProgram(0));
#else
		SHADER_GL_CALL(glUseProgram(0));

		if (mPositionAttribute != -1)
			SHADER_GL_CALL(glDisableVertexAttribArray(mPositionAttribute));

		if (mColorAttribute != -1)
			SHADER_GL_CALL(glDisableVertexAttribArray(mColorAttribute));

		if (mTexCoordAttribute != -1)
			SHADER_GL_CALL(glDisableVertexAttribArray(mTexCoordAttribute));
#endif
	}
}