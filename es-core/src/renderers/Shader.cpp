#include "Shader.h"
#include "Log.h"
#include "renderers/Renderer.h"
#include "resources/ResourceManager.h"

namespace Renderer
{
	std::string SHADER_VERSION_STRING;

	Shader Shader::createShader(GLenum type, const std::string& source)
	{
		const GLuint shaderId = glCreateShader(type);

		Shader ret;
		ret.compile(shaderId, source.c_str());
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

	bool Shader::compile(GLuint id, const char* source)
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

			if (isCompiled == GL_FALSE)
			{
				LOG(LogError) << "GLSL Vertex Compile Error\n" << infoLog;
				delete[] infoLog;
				return false;
			}
			else
			{
				if (strstr(infoLog, "WARNING") || strstr(infoLog, "warning") || strstr(infoLog, "Warning"))
					LOG(LogWarning) << "GLSL Vertex Compile Warning\n" << infoLog;
				else
					LOG(LogInfo) << "GLSL Vertex Compile Message\n" << infoLog;
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
		mPositionAttribute(-1),
		mColorAttribute(-1),
		mTexCoordAttribute(-1),
		mvpUniform(-1),
		mSaturation(-1),
		linkStatus(false),
		mId(-1),
		mOutputSize(-1),
		mTextureSize(-1)	
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

	bool ShaderProgram::loadFromFile(const std::string& path)
	{
		if (!ResourceManager::getInstance()->fileExists(path))
			return false;
		
		// This will load the entire GLSL source code into the string variable.
		const ResourceData& shaderData = ResourceManager::getInstance()->getFileData(path);

		std::string shaderCode;
		shaderCode.assign(reinterpret_cast<const char*>(shaderData.ptr.get()), shaderData.length);

		std::string versionString = SHADER_VERSION_STRING;

		Shader vertex = Shader::createShader(GL_VERTEX_SHADER, versionString + "#define VERTEX\n" + shaderCode);
		if (vertex.id < 0)
			return false;

		Shader fragment = Shader::createShader(GL_FRAGMENT_SHADER, versionString + "#define FRAGMENT\n" + shaderCode);
		if (fragment.id < 0)
			return false;

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

		mSaturation = glGetUniformLocation(mId, "saturation");

		GLint texUniform = glGetUniformLocation(mId, "u_tex");		
		if (texUniform == -1)
			texUniform = glGetUniformLocation(mId, "textureSampler");
		
		if (texUniform != -1)
		{
			GL_CHECK_ERROR(glUseProgram(mId));
			GL_CHECK_ERROR(glUniform1i(texUniform, 0));
		}
	}

	void ShaderProgram::setSaturation(GLfloat saturation)
	{
		if (mSaturation != -1)
			GL_CHECK_ERROR(glUniform1f(mSaturation, saturation));
	}

	void ShaderProgram::setTextureSize(const Vector2f& size)
	{
		if (mTextureSize != -1)
			GL_CHECK_ERROR(glUniform2f(mTextureSize, size.x(), size.y()));
	}
	
	void ShaderProgram::setOutputSize(const Vector2f& size)
	{
		if (mOutputSize != -1)
			GL_CHECK_ERROR(glUniform2f(mOutputSize, size.x(), size.y()));
	}

	void ShaderProgram::setMatrix(Transform4x4f& mvpMatrix)
	{
		if (mvpUniform != -1 && mvpUniform != GL_INVALID_VALUE && mvpUniform != GL_INVALID_OPERATION)
			GL_CHECK_ERROR(glUniformMatrix4fv(mvpUniform, 1, GL_FALSE, (float*)&mvpMatrix));
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

		if (mTexCoordAttribute != -1);
			GL_CHECK_ERROR(glDisableVertexAttribArray(mTexCoordAttribute));
	}
}