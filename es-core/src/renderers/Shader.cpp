#include "Shader.h"
#include "Log.h"

namespace Renderer
{
	bool Shader::compile(GLuint id, const char* source)
	{
		// Try to compile GLSL source code
		compileStatus = false;
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

	bool ShaderProgram::linkShaderProgram(Shader &vertexShader, Shader &fragmentShader)
	{
		// shader program (texture)
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

		// Compile OK ? Affect program id
		this->linkStatus = isCompiled;
		if (this->linkStatus == GL_TRUE)
		{
			this->id = programId;
			return true;
		}

		return false;
	}

}