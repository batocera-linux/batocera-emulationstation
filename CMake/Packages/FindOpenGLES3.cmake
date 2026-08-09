# FindOpenGLES3
# ---------------
# Finds the OpenGL ES 3 headers and the GLESv2 ABI library that exposes
# OpenGL ES 2.0 and 3.x entry points on EGL platforms.
#
# This defines:
#
# OPENGLES3_FOUND
# OPENGLES3_INCLUDE_DIRS
# OPENGLES3_LIBRARIES

if(NOT HINT_GLES_LIBNAME)
    set(HINT_GLES_LIBNAME GLESv2)
endif()

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_OPENGLES3 QUIET glesv2)
endif()

find_path(OPENGLES3_INCLUDE_DIR GLES3/gl3.h
    PATHS "${CMAKE_FIND_ROOT_PATH}/usr/include"
    HINTS ${HINT_GLES_INCDIR} ${PC_OPENGLES3_INCLUDE_DIRS}
)

find_library(OPENGLES3_gl_LIBRARY
    NAMES ${HINT_GLES_LIBNAME}
    HINTS ${HINT_GLES_LIBDIR} ${PC_OPENGLES3_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenGLES3
    REQUIRED_VARS OPENGLES3_gl_LIBRARY OPENGLES3_INCLUDE_DIR
)

if(OpenGLES3_FOUND)
    set(OPENGLES3_FOUND TRUE)
    set(OPENGLES3_LIBRARIES ${OPENGLES3_gl_LIBRARY})
    set(OPENGLES3_INCLUDE_DIRS ${OPENGLES3_INCLUDE_DIR})
    mark_as_advanced(OPENGLES3_INCLUDE_DIR OPENGLES3_gl_LIBRARY)
endif()
