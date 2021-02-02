#include "GlExtensions.h"
#include <SDL.h>

#ifdef OPENGL_EXTENSIONS
namespace glext
{
	PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
	PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
	PFNGLGENBUFFERSPROC	glGenBuffers = nullptr;		
	PFNGLBINDBUFFERPROC glBindBuffer = nullptr;
	PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
	PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
	PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;
	PFNGLATTACHSHADERPROC glAttachShader = nullptr;
	PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
	PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
	PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;
	PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
	PFNGLUNIFORM1IPROC glUniform1i = nullptr;
	PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
	PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation = nullptr;
	PFNGLBUFFERDATAPROC glBufferData = nullptr;
	PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = nullptr;
	PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
	PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = nullptr;
	PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = nullptr;
	PFNGLCREATESHADERPROC glCreateShader = nullptr;
	PFNGLACTIVETEXTUREPROC glActiveTexture_ = nullptr;

	bool initializeGlExtensions()
	{
		if (SDL_GL_ExtensionSupported("GL_ARB_shader_objects") &&
			SDL_GL_ExtensionSupported("GL_ARB_shading_language_100") &&
			SDL_GL_ExtensionSupported("GL_ARB_vertex_shader") &&
			SDL_GL_ExtensionSupported("GL_ARB_fragment_shader"))
		{
			glCreateShader = (PFNGLCREATESHADERPROC)SDL_GL_GetProcAddress("glCreateShader");
			glCompileShader = (PFNGLCOMPILESHADERPROC)SDL_GL_GetProcAddress("glCompileShader");
			glCreateProgram = (PFNGLCREATEPROGRAMOBJECTARBPROC)SDL_GL_GetProcAddress("glCreateProgram");
			glGenBuffers = (PFNGLGENBUFFERSPROC)SDL_GL_GetProcAddress("glGenBuffers");
			glBindBuffer = (PFNGLBINDBUFFERPROC)SDL_GL_GetProcAddress("glBindBuffer");
			glShaderSource = (PFNGLSHADERSOURCEPROC)SDL_GL_GetProcAddress("glShaderSource");
			glGetShaderiv = (PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv");
			glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)SDL_GL_GetProcAddress("glGetShaderInfoLog");
			glAttachShader = (PFNGLATTACHSHADERPROC)SDL_GL_GetProcAddress("glAttachShader");
			glLinkProgram = (PFNGLLINKPROGRAMPROC)SDL_GL_GetProcAddress("glLinkProgram");
			glGetProgramiv = (PFNGLGETPROGRAMIVPROC)SDL_GL_GetProcAddress("glGetProgramiv");
			glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)SDL_GL_GetProcAddress("glGetProgramInfoLog");
			glUseProgram = (PFNGLUSEPROGRAMPROC)SDL_GL_GetProcAddress("glUseProgram");
			glUniform1i = (PFNGLUNIFORM1IPROC)SDL_GL_GetProcAddress("glUniform1i");
			glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)SDL_GL_GetProcAddress("glGetUniformLocation");
			glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)SDL_GL_GetProcAddress("glGetAttribLocation");
			glBufferData = (PFNGLBUFFERDATAPROC)SDL_GL_GetProcAddress("glBufferData");
			glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)SDL_GL_GetProcAddress("glVertexAttribPointer");
			glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)SDL_GL_GetProcAddress("glEnableVertexAttribArray");
			glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)SDL_GL_GetProcAddress("glDisableVertexAttribArray");
			glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)SDL_GL_GetProcAddress("glUniformMatrix4fv");
			glActiveTexture_ = (PFNGLACTIVETEXTUREPROC)SDL_GL_GetProcAddress("glActiveTexture");

			return glCreateShader != nullptr;
		}

		return false;
	}
}
#endif

#if defined(_DEBUG)
#include "Log.h"

void _GLCheckError(const char* _funcName)
{
	const GLenum errorCode = glGetError();

	if (errorCode != GL_NO_ERROR)
		LOG(LogError) << "GL error: " << _funcName << " failed with error code: " << errorCode;
}
#endif
