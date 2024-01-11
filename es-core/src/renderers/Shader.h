#pragma once
#ifndef ES_CORE_RENDERER_SHADER_H
#define ES_CORE_RENDERER_SHADER_H

#include "GlExtensions.h"
#include "math/Transform4x4f.h"

#include <string>
#include <vector>
#include <map>

namespace Renderer
{
	class Shader
	{
	public:
		Shader()
		{
			id = -1;
			compileStatus = false;
		}

		static Shader createShader(GLenum type, const std::string& source);
		void deleteShader();

		GLuint id;
		bool compileStatus;

	private:
		// Compile a shader
		// id should be a valid shader id created by glCreateShader with GL_VERTEX_SHADER or GL_FRAGMENT_SHADER type
		bool compile(GLuint id, const char* source, GLenum type);
	};

	class ShaderProgram
	{
	public:
		ShaderProgram();

		bool loadFromFile(const std::string& path);

		// Links vertex and fragment shaders together to make a GLSL program
		bool createShaderProgram(Shader &vertexShader, Shader &fragmentShader);

		void select();
		void unSelect();

		void setMatrix(Transform4x4f& mvpMatrix);
		void setSaturation(GLfloat saturation);
		void setTextureSize(const Vector2f& size);
		void setInputSize(const Vector2f& size);
		void setOutputSize(const Vector2f& size);
		void setOutputOffset(const Vector2f& size);
		void setCornerRadius(GLfloat radius);
		void setResolution();
		void setFrameCount(int frame);

		void setCustomUniformsParameters(const std::map<std::string, std::string>& parameters);

		bool supportsTextureSize() { return mTextureSize != -1; }
		bool supportsCornerRadius() { return mCornerRadius != -1; }

		void deleteProgram();

	private:
		void setUniformEx(const std::string& name, const std::string value);

		GLuint mId;
		bool linkStatus;
		GLint mPositionAttribute;
		GLint mColorAttribute;
		GLint mTexCoordAttribute;
		GLint mvpUniform;

		GLint mSaturation;		
		GLint mTextureSize;
		GLint mOutputSize;
		GLint mOutputOffset;
		GLint mInputSize;
		GLint mResolution;
		GLint mCornerRadius;
		GLint mFrameCount;
		GLint mFrameDirection;
		
		struct UniformInfo
		{
			GLint location;
			GLenum type;			
		};

		std::map<std::string, UniformInfo> mCustomUniforms;
		std::vector<Shader> mAttachedShaders;

	private:
		void findAttribsAndUniforms();
	};

} // Renderer::

#endif // ES_CORE_RENDERER_SHADER_H
