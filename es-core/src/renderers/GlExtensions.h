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
	extern PFNGLUNIFORM1FPROC glUniform1f;
	extern PFNGLUNIFORM2FPROC glUniform2f;
	extern PFNGLUNIFORM4FPROC glUniform4f;	
	extern PFNGLDELETEPROGRAMPROC glDeleteProgram;
	extern PFNGLDELETESHADERPROC glDeleteShader;

	extern PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
	extern PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebuffer;
	extern PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer;
	extern PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
	extern PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;		

	extern PFNGLGETACTIVEUNIFORMPROC glGetActiveUniform;
};

using namespace glext;

#define OPENGL_EXTENSIONS 1

#endif

#define GL_CHECK_ERROR(Function) (Function, _GLCheckError(#Function))
bool _GLCheckError(const char* _funcName);
