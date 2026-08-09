#pragma once
#ifndef ES_CORE_RENDERER_SHADER_H
#define ES_CORE_RENDERER_SHADER_H

#include "GlExtensions.h"
#include "math/Transform4x4f.h"
// Vector2f is held by value in the uniform cache below.
#include "math/Vector2f.h"

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

#if defined(USE_OPENGLES_30)
		void select(GLuint vertexBuffer, GLuint indexBuffer);
#else
		void select();
#endif
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
#if defined(USE_OPENGLES_30)
		GLint mSamplerUniform;
		bool mSamplerInitialized;
		GLuint mVertexArray;

		// Anything that resets uniform state must call invalidateUniformCache().
		struct UniformCache
		{
			bool     matrixValid = false;
			float    matrix[16] = { 0 };

			bool     saturationValid = false;
			GLfloat  saturation = 0.0f;

			bool     cornerRadiusValid = false;
			GLfloat  cornerRadius = 0.0f;

			bool     textureSizeValid = false;
			Vector2f textureSize;

			bool     inputSizeValid = false;
			Vector2f inputSize;

			bool     outputSizeValid = false;
			Vector2f outputSize;

			bool     outputOffsetValid = false;
			Vector2f outputOffset;

			bool     resolutionValid = false;
			Vector2f resolution;

			bool     frameCountValid = false;
			int      frameCount = -1;
		};

		UniformCache mUniformCache;

		void invalidateUniformCache() { mUniformCache = UniformCache(); }
#endif
		
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
