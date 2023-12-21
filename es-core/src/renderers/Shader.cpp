#include "Shader.h"
#include "Log.h"
#include "renderers/Renderer.h"
#include "resources/ResourceManager.h"
#include "utils/StringUtil.h"
#include "utils/HtmlColor.h"

#include <set>

namespace Renderer
{
	std::string SHADER_VERSION_STRING;

	Shader Shader::createShader(GLenum type, const std::string& source)
	{
		const GLuint shaderId = glCreateShader(type);

		Shader ret;
		ret.compile(shaderId, source.c_str(), type);
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
		GL_CHECK_ERROR(glShaderSource(id, 1, &source, nullptr));
		GL_CHECK_ERROR(glCompileShader(id));

		// Check compile status (ok, warning, error)
		GLint isCompiled = GL_FALSE;
		GLint maxLength = 0;
		GL_CHECK_ERROR(glGetShaderiv(id, GL_COMPILE_STATUS, &isCompiled));
		GL_CHECK_ERROR(glGetShaderiv(id, GL_INFO_LOG_LENGTH, &maxLength));

		// Read log if any
		if (maxLength > 1)
		{
			char* infoLog = new char[maxLength + 1];

			GL_CHECK_ERROR(glGetShaderInfoLog(id, maxLength, &maxLength, infoLog));

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
		mId(-1),
		mPositionAttribute(-1), mColorAttribute(-1), mTexCoordAttribute(-1), mvpUniform(-1),
		linkStatus(false),
		mOutputSize(-1), mOutputOffset(-1), mInputSize(-1), mTextureSize(-1), mResolution(-1),
		mSaturation(-1), mCornerRadius(-1),
		mFrameCount(-1), mFrameDirection(-1)
	{
	}

	void ShaderProgram::deleteProgram()
	{
		if (mId >= 0)
		{
			for (auto shader : mAttachedShaders)
				shader.deleteShader();

			mAttachedShaders.clear();

			GL_CHECK_ERROR(glDeleteProgram(mId));
			mId = -1;
		}
	}

	static std::string appendVersionAndType(const std::string& shaderCode, const std::string& customDefines)
	{
		auto pos = shaderCode.find("#version");
		if (pos != std::string::npos)
		{
			auto next = shaderCode.find("\n");
			if (next != std::string::npos && next + 1 < shaderCode.length())
			{
				std::string ret = shaderCode.substr(0, next + 1) + customDefines + "\n" + shaderCode.substr(next + 1);
				return ret;
			}
		}

		return SHADER_VERSION_STRING + customDefines + "\n" + shaderCode;
	}

	bool ShaderProgram::loadFromFile(const std::string& path)
	{
		if (!ResourceManager::getInstance()->fileExists(path))
			return false;
		
		// This will load the entire GLSL source code into the string variable.
		const ResourceData& shaderData = ResourceManager::getInstance()->getFileData(path);

		std::string shaderCode;
		shaderCode.assign(reinterpret_cast<const char*>(shaderData.ptr.get()), shaderData.length);

		std::string versionString = SHADER_VERSION_STRING;

		Shader vertex = Shader::createShader(GL_VERTEX_SHADER, appendVersionAndType(shaderCode, "#define VERTEX"));
		if (!vertex.compileStatus)
		{
			LOG(LogError) << "Failed to compile GLSL VERTEX shader : " << path;
			return false;
		}

		Shader fragment = Shader::createShader(GL_FRAGMENT_SHADER, appendVersionAndType(shaderCode, "#define FRAGMENT"));
		if (!fragment.compileStatus)
		{
			LOG(LogError) << "Failed to compile GLSL FRAGMENT shader : " << path;
			return false;
		}

		return createShaderProgram(vertex, fragment);
	}

	bool ShaderProgram::createShaderProgram(Shader &vertexShader, Shader &fragmentShader)
	{
		GLuint programId = glCreateProgram();

		GL_CHECK_ERROR(glAttachShader(programId, vertexShader.id));
		GL_CHECK_ERROR(glAttachShader(programId, fragmentShader.id));

		GL_CHECK_ERROR(glLinkProgram(programId));

		GLint isCompiled = GL_FALSE;
		GLint maxLength = 0;

		GL_CHECK_ERROR(glGetProgramiv(programId, GL_LINK_STATUS, &isCompiled));
		GL_CHECK_ERROR(glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &maxLength));

		if (maxLength > 1)
		{
			char* infoLog = new char[maxLength + 1];

			GL_CHECK_ERROR(glGetProgramInfoLog(programId, maxLength, &maxLength, infoLog));

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

		GLint texUniform = glGetUniformLocation(mId, "u_tex");		
		if (texUniform == -1)
			texUniform = glGetUniformLocation(mId, "textureSampler");
		if (texUniform == -1)
			texUniform = glGetUniformLocation(mId, "Texture");
		
		mFrameCount = glGetUniformLocation(mId, "FrameCount");
		mFrameDirection = glGetUniformLocation(mId, "FrameDirection");

		GLint numUniforms = 0;
		GL_CHECK_ERROR(glGetProgramiv(mId, GL_ACTIVE_UNIFORMS, &numUniforms));

		for (int i = 0; i < numUniforms; ++i) 
		{
			GLenum type;
			GLsizei length;
			GLint size;

			char buffer[256];
			GL_CHECK_ERROR(glGetActiveUniform(mId, i, (GLsizei)sizeof(buffer), &length, &size, &type, buffer));

			std::string uniformName = buffer;
			if (builtInUniforms.find(uniformName) != builtInUniforms.cend())
				continue;

			UniformInfo info;
			info.location = glGetUniformLocation(mId, uniformName.c_str());
			info.type = type;

			mCustomUniforms[uniformName] = info;
		}

		if (texUniform != -1)
		{
			GL_CHECK_ERROR(glUseProgram(mId));
			GL_CHECK_ERROR(glUniform1i(texUniform, 0));
		}
	}

	void ShaderProgram::setMatrix(Transform4x4f& mvpMatrix)
	{
		if (mvpUniform != -1 && mvpUniform != GL_INVALID_VALUE && mvpUniform != GL_INVALID_OPERATION)
			GL_CHECK_ERROR(glUniformMatrix4fv(mvpUniform, 1, GL_FALSE, (float*)&mvpMatrix));
	}

	void ShaderProgram::setSaturation(GLfloat saturation)
	{
		if (mSaturation != -1)
			GL_CHECK_ERROR(glUniform1f(mSaturation, saturation));
	}

	void ShaderProgram::setCornerRadius(GLfloat radius)
	{
		if (mCornerRadius != -1)
			GL_CHECK_ERROR(glUniform1f(mCornerRadius, radius));
	}	

	void ShaderProgram::setTextureSize(const Vector2f& size)
	{
		if (mTextureSize != -1)
			GL_CHECK_ERROR(glUniform2f(mTextureSize, size.x(), size.y()));
	}
	
	void ShaderProgram::setInputSize(const Vector2f& size)
	{
		if (mInputSize != -1)
			GL_CHECK_ERROR(glUniform2f(mInputSize, size.x(), size.y()));
	}

	void ShaderProgram::setOutputSize(const Vector2f& size)
	{
		if (mOutputSize != -1)
			GL_CHECK_ERROR(glUniform2f(mOutputSize, size.x(), size.y()));
	}

	void ShaderProgram::setOutputOffset(const Vector2f& size)
	{
		if (mOutputOffset != -1)
			GL_CHECK_ERROR(glUniform2f(mOutputOffset, size.x(), size.y()));
	}

	void ShaderProgram::setResolution()
	{
		if (mResolution != -1)
			GL_CHECK_ERROR(glUniform2f(mResolution, getScreenWidth(), getScreenHeight()));
	}

	void ShaderProgram::setFrameCount(int frame)
	{
		if (mFrameDirection != -1)
			GL_CHECK_ERROR(glUniform1i(mFrameCount, 1));

		if (mFrameCount != -1)
			GL_CHECK_ERROR(glUniform1i(mFrameCount, frame));
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
				GL_CHECK_ERROR(glUniform1i(item.second.location, 0));
				break;
			case GL_FLOAT:
				GL_CHECK_ERROR(glUniform1f(item.second.location, 0.0f));
				break;
			case GL_FLOAT_VEC2:
				GL_CHECK_ERROR(glUniform2f(item.second.location, 0.0f, 0.0f));
				break;
			case GL_FLOAT_VEC4:
				GL_CHECK_ERROR(glUniform4f(item.second.location, 0.0f, 0.0f, 0.0f, 0.0f));
				break;
			case GL_BOOL:
				GL_CHECK_ERROR(glUniform1i(item.second.location, GL_FALSE));
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
			GL_CHECK_ERROR(glUniform1i(location, Utils::String::toInteger(value)));
			break;
		case GL_FLOAT:
			GL_CHECK_ERROR(glUniform1f(location, Utils::String::toFloat(value)));
			break;
		case GL_BOOL:
			GL_CHECK_ERROR(glUniform1i(location, Utils::String::toBoolean(value) ? GL_TRUE : GL_FALSE));
			break;
		case GL_FLOAT_VEC2:
			{
				auto size = Vector2f::parseString(value);
				GL_CHECK_ERROR(glUniform2f(location, size.x(), size.y()));
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

				GL_CHECK_ERROR(glUniform4f(location, red / 255.0f, green / 255.0f, blue / 255.0f, alpha / 255.0f));
			}
			else 
			{
				// Coordinates
				auto size = Vector4f::parseString(value);
				GL_CHECK_ERROR(glUniform4f(location, size.x(), size.y(), size.z(), size.w()));
			}

		}
		break;
		}
	}

	void ShaderProgram::select()
	{
		GL_CHECK_ERROR(glUseProgram(mId));

		if (mPositionAttribute != -1)
		{
			GL_CHECK_ERROR(glVertexAttribPointer(mPositionAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, pos)));
			GL_CHECK_ERROR(glEnableVertexAttribArray(mPositionAttribute));
		}

		if (mColorAttribute != -1)
		{
			GL_CHECK_ERROR(glVertexAttribPointer(mColorAttribute, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (const void*)offsetof(Vertex, col)));
			GL_CHECK_ERROR(glEnableVertexAttribArray(mColorAttribute));
		}

		if (mTexCoordAttribute != -1)
		{
			GL_CHECK_ERROR(glVertexAttribPointer(mTexCoordAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, tex)));
			GL_CHECK_ERROR(glEnableVertexAttribArray(mTexCoordAttribute));
		}
	}

	void ShaderProgram::unSelect()
	{
		GL_CHECK_ERROR(glUseProgram(0));

		if (mPositionAttribute != -1)
			GL_CHECK_ERROR(glDisableVertexAttribArray(mPositionAttribute));

		if (mColorAttribute != -1)
			GL_CHECK_ERROR(glDisableVertexAttribArray(mColorAttribute));

		if (mTexCoordAttribute != -1)
			GL_CHECK_ERROR(glDisableVertexAttribArray(mTexCoordAttribute));
	}
}