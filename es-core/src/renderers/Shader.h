#pragma once
#ifndef ES_CORE_RENDERER_SHADER_H
#define ES_CORE_RENDERER_SHADER_H

namespace Renderer
{

	struct Shader
	{
		//const char* source;
		GLuint id;
		bool compileStatus;
	}; // Shader

	struct ShaderProgram
	{
		GLuint id;
		bool linkStatus;
		GLint posAttrib;
		GLint colAttrib;
		GLint texAttrib;
		GLint projUniform;
        GLint worldUniform;
	}; // ShaderProgram


 	//Load some shader source code from an external file
 	//const char* loadShader(const char* file);

	// Compile a shader
	// id should be a valid shader id created by glCreateShader with GL_VERTEX_SHADER or GL_FRAGMENT_SHADER type
 	bool compileShader(Shader& shader, GLuint id, const char* source);

 	// Links vertex and fragment shaders together to make a GLSL program
 	bool linkShaderProgram(Shader &vertexShader, Shader &fragmentShader, ShaderProgram &shaderProgram);

} // Renderer::

#endif // ES_CORE_RENDERER_SHADER_H
