#pragma once
#ifndef ES_CORE_RENDERER_SHADER_H
#define ES_CORE_RENDERER_SHADER_H

#include "GlExtensions.h"

namespace Renderer
{
	class Shader
	{
	public:
		GLuint id;
		bool compileStatus;

		// Compile a shader
		// id should be a valid shader id created by glCreateShader with GL_VERTEX_SHADER or GL_FRAGMENT_SHADER type
		bool compile(GLuint id, const char* source);
	};

	class ShaderProgram
	{
	public:
		GLuint id;
		bool linkStatus;
		GLint posAttrib;
		GLint colAttrib;
		GLint texAttrib;
		GLint mvpUniform;

		// Links vertex and fragment shaders together to make a GLSL program
		bool linkShaderProgram(Shader &vertexShader, Shader &fragmentShader);
	};

} // Renderer::

#endif // ES_CORE_RENDERER_SHADER_H
