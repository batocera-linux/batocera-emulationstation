#pragma once

#include <SDL.h>

#if USE_OPENGLES_20
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>

namespace glext
{
	bool initializeGlExtensions();

	extern PFNGLCOMPILESHADERPROC glCompileShader;
	extern PFNGLCREATEPROGRAMPROC glCreateProgram;
	extern PFNGLGENBUFFERSPROC	glGenBuffers;
	extern PFNGLBINDBUFFERPROC glBindBuffer;
	extern PFNGLSHADERSOURCEPROC glShaderSource;
	extern PFNGLGETSHADERIVPROC glGetShaderiv;
	extern PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
	extern PFNGLATTACHSHADERPROC glAttachShader;
	extern PFNGLLINKPROGRAMPROC glLinkProgram;
	extern PFNGLGETPROGRAMIVPROC glGetProgramiv;
	extern PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
	extern PFNGLUSEPROGRAMPROC glUseProgram;
	extern PFNGLUNIFORM1IPROC glUniform1i;
	extern PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
	extern PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
	extern PFNGLBUFFERDATAPROC glBufferData;
	extern PFNGLBUFFERSUBDATAARBPROC glBufferSubData;
	extern PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
	extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
	extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
	extern PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
	extern PFNGLCREATESHADERPROC glCreateShader;
	extern PFNGLACTIVETEXTUREPROC glActiveTexture_;
};

using namespace glext;

#define OPENGL_EXTENSIONS 1

#endif

#if defined(_DEBUG)
#define GL_CHECK_ERROR(Function) (Function, _GLCheckError(#Function))
void _GLCheckError(const char* _funcName);
#else
#define GL_CHECK_ERROR(Function) (Function)
#endif
