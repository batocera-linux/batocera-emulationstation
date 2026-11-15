cmake_minimum_required(VERSION 3.25)

# C++23 everywhere (GCC 14+ on Armbian trixie, GCC 15 / Clang on Arch)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Graphics API : one GL 3 renderer, built either for desktop OpenGL 3.3 core
# (default on x86_64) or for OpenGL ES 3.x (default on ARM, Raspberry Pi 4+).
option(GL "Build the desktop OpenGL 3.3 core renderer" OFF)
option(GLES3 "Build the OpenGL ES 3.x renderer" OFF)

if(GL AND GLES3)
    message(FATAL_ERROR "GL and GLES3 are mutually exclusive; enable only one graphics API")
endif()

option(CEC "CEC" ON)


option(ENABLE_FILEMANAGER "Set to ON to enable f1 shortcut for filesystem" OFF)
option(DISABLE_KODI "Set to ON to disable kodi in menu" OFF)
option(ENABLE_PULSE "Set to ON to enable pulse audio (versus alsa)" OFF)
option(ENABLE_TTS "Set to ON to enable text to speech" OFF)
option(USE_SYSTEM_PUGIXML "Set to ON to use system-wide pugixml library" OFF)

project(emulationstation-all)

# Faster builds. ccache is used when it is installed (unless a launcher is already given), and
# es-core and es-app precompile the headers that almost every file includes. Turn either off
# with -DES_CCACHE=OFF or -DES_PCH=OFF (the clang-tidy and CodeQL workflows do without PCH).
option(ES_CCACHE "Compile through ccache when it is installed" ON)
option(ES_PCH "Precompile the common headers of es-core and es-app" ON)

if(ES_CCACHE AND NOT CMAKE_CXX_COMPILER_LAUNCHER AND NOT CMAKE_C_COMPILER_LAUNCHER)
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        message("-- Using ccache: ${CCACHE_PROGRAM}")
        # pch_defines and time_macros let ccache cache the files built with a precompiled header,
        # include_file_ctime/mtime let it hit right after a checkout (CI) touched every file
        set(ES_CCACHE_LAUNCHER ${CMAKE_COMMAND} -E env
            CCACHE_SLOPPINESS=pch_defines,time_macros,include_file_ctime,include_file_mtime
            ${CCACHE_PROGRAM})
        set(CMAKE_C_COMPILER_LAUNCHER ${ES_CCACHE_LAUNCHER})
        set(CMAKE_CXX_COMPILER_LAUNCHER ${ES_CCACHE_LAUNCHER})
    endif()
endif()

# Lets `ctest --test-dir build` find the tests that ES_TESTS and ES_LAUNCHER_TESTS add in the subfolders
enable_testing()

include(GNUInstallDirs)

# Version shown in ES and given to the packages. Release builds pass -DES_VERSION=<tag without v>;
# otherwise it comes from `git describe` (v1.2.0-3-gabcdef -> 1.2.0-3-gabcdef).
set(ES_VERSION "" CACHE STRING "EmulationStation-RE version (default: from git describe)")
if(NOT ES_VERSION)
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --match "v[0-9]*" --dirty
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE ES_GIT_DESCRIBE OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    endif()
    if(ES_GIT_DESCRIBE)
        string(REGEX REPLACE "^v" "" ES_VERSION_RESOLVED "${ES_GIT_DESCRIBE}")
    else()
        set(ES_VERSION_RESOLVED "0.0.0-dev")
    endif()
else()
    string(REGEX REPLACE "^v" "" ES_VERSION_RESOLVED "${ES_VERSION}")
endif()
message("-- EmulationStation-RE version: ${ES_VERSION_RESOLVED}")
# Set on es-app/src/BuildInfo.cpp only (see es-app/CMakeLists.txt): a new commit changes the
# version, and a definition given to every file would rebuild them all and defeat ccache.
# Installed files are also looked up in $XDG_DATA_DIRS, this is the fallback when the prefix is not listed there
add_compile_definitions(ES_INSTALL_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}" ES_INSTALL_LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")

#-------------------------------------------------------------------------------
#add local find scripts to CMAKE path
LIST(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/CMake/Utils
    ${CMAKE_CURRENT_SOURCE_DIR}/CMake/Packages
)

# disable kodi menu
if(DISABLE_KODI)
  MESSAGE("Kodi disabled")
else()
  add_definitions(-D_ENABLE_KODI_)
endif()

# batocera / file manager f1 button
# disable file manager
if(ENABLE_FILEMANAGER)
  MESSAGE("file manager enabled")
  add_definitions(-D_ENABLE_FILEMANAGER_)
endif()

#-------------------------------------------------------------------------------
# pick the graphics API
if(NOT GL AND NOT GLES3)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64)")
        set(GLES3 ON)
    else()
        set(GL ON)
    endif()
endif()

if(GLES3)
    MESSAGE("-- Graphics API: OpenGL ES 3.x")
else()
    MESSAGE("-- Graphics API: OpenGL 3.3 core")
endif()

#finding necessary packages
#-------------------------------------------------------------------------------
if(GLES3)
    find_package(OpenGLES3 REQUIRED)
else()
    find_package(OpenGL REQUIRED)
    # Prefer glvnd's libOpenGL (no GLX dependency); SDL creates the context itself
    if(TARGET OpenGL::OpenGL)
        set(ES_OPENGL_TARGET OpenGL::OpenGL)
    else()
        set(ES_OPENGL_TARGET OpenGL::GL)
    endif()
endif()
find_package(Freetype REQUIRED)
find_package(SDL3 3.2.0 REQUIRED CONFIG COMPONENTS SDL3)
find_package(SDL3_image 3.2.0 REQUIRED CONFIG)  # image decoding (PNG, JPEG, animated GIF, WebP...)
find_package(CURL REQUIRED)
find_package(VLC REQUIRED)
find_package(RapidJSON REQUIRED)

# FriBidi is only used to reorder right-to-left scripts (Arabic, Hebrew) at render time.
# It is optional on purpose: when it is missing, Font.cpp falls back on tryFastBidi(), which
# is what every build used before RTL support was added. pkg-config is a Unix mechanism and
# the Windows dependency package does not ship FriBidi, so this must never be REQUIRED.
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
	pkg_check_modules(FRIBIDI QUIET fribidi)
endif()

if(FRIBIDI_FOUND)
	MESSAGE("-- FriBidi found, using it for right-to-left text reordering")
	add_definitions(-DHAVE_FRIBIDI)
else()
	MESSAGE("-- FriBidi not found, falling back on the built-in right-to-left reordering")
endif()

if(ENABLE_PULSE)
  MESSAGE("pulse audio enabled")
  find_package(PulseAudio REQUIRED)
  add_definitions(-D_ENABLE_PULSE_)
else()
  MESSAGE("pulse audio disabled")
endif()

find_package(udev)
if(UDEV_FOUND)
  MESSAGE("udev found. guns enabled")
  add_definitions(-DHAVE_UDEV)
else()
  MESSAGE("udev not found. guns will be disabled")
endif()

if(ENABLE_TTS)
  MESSAGE("tts enabled")
  find_library(LIBESPEAK_LIBRARIES NAMES espeak REQUIRED)
  if("${LIBESPEAK_LIBRARIES}" STREQUAL "LIBESPEAK_LIBRARIES-NOTFOUND")
	MESSAGE(FATAL_ERROR "espeak not found")
  endif()
  add_definitions(-D_ENABLE_TTS_)
else()
  MESSAGE("tts disabled")
endif()

if(CEC)
  MESSAGE("CEC enabled")
  find_package(libCEC)
else()
  MESSAGE("CEC disabled")
endif()

#add ALSA for Linux
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    find_package(ALSA REQUIRED)
endif()

#-------------------------------------------------------------------------------
#set up compiler flags and excutable names
if(CEC)
  if(DEFINED libCEC_FOUND)
    add_definitions(-DHAVE_LIBCEC)
  endif()
endif()

# API keys of the web services, compiled into the binary. Each one is read from the CMake
# variable of that name (-DGAMESDB_APIKEY=...), else from the environment variable of that name,
# which is how the release workflow passes its secrets. An empty value means no key: the
# scraper or menu that needs it is left out of the build.
#
# The keys go into a generated ApiKeys.h rather than add_compile_definitions(): CMake drops
# definitions containing '#', and passwords may contain one. The header is not installed.
set(ES_API_KEYS_HEADER "// Generated by CMake from the API key variables/environment. Not installed.\n#pragma once\n\n")
foreach(ES_API_KEY SCREENSCRAPER_SOFTNAME SCREENSCRAPER_DEV_LOGIN GAMESDB_APIKEY CHEEVOS_DEV_LOGIN HFS_DEV_LOGIN)
    if(NOT DEFINED ${ES_API_KEY} AND DEFINED ENV{${ES_API_KEY}})
        set(${ES_API_KEY} "$ENV{${ES_API_KEY}}")
    endif()

    if(NOT "${${ES_API_KEY}}" STREQUAL "")
        message("-- ${ES_API_KEY} set")

        # Escape the value for a C string literal.
        set(ES_API_KEY_VALUE "${${ES_API_KEY}}")
        string(REPLACE "\\" "\\\\" ES_API_KEY_VALUE "${ES_API_KEY_VALUE}")
        string(REPLACE "\"" "\\\"" ES_API_KEY_VALUE "${ES_API_KEY_VALUE}")
        string(REPLACE "\n" "\\n" ES_API_KEY_VALUE "${ES_API_KEY_VALUE}")
        string(REPLACE "\r" "" ES_API_KEY_VALUE "${ES_API_KEY_VALUE}")
        string(APPEND ES_API_KEYS_HEADER "#define ${ES_API_KEY} \"${ES_API_KEY_VALUE}\"\n")
    endif()
endforeach()
file(WRITE ${CMAKE_BINARY_DIR}/ApiKeys.h "${ES_API_KEYS_HEADER}")
file(CHMOD ${CMAKE_BINARY_DIR}/ApiKeys.h PERMISSIONS OWNER_READ OWNER_WRITE)

#-------------------------------------------------------------------------------

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13)
    message(FATAL_ERROR "GCC 13 or newer is required for C++23")
endif()

add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-attributes>)

# Release builds at -O2, like the distribution packages, instead of CMake's -O3: the build is
# about a third faster and the front-end gains nothing measurable from -O3.
foreach(ES_LANG C CXX)
    string(REPLACE "-O3" "-O2" CMAKE_${ES_LANG}_FLAGS_RELEASE "${CMAKE_${ES_LANG}_FLAGS_RELEASE}")
endforeach()

add_definitions(-DUSE_GL3)
if(GLES3)
    add_definitions(-DUSE_GLES)
endif()

#-------------------------------------------------------------------------------
#add include directories
set(COMMON_INCLUDE_DIRS
    ${FREETYPE_INCLUDE_DIRS}
    ${FRIBIDI_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIR}
    ${VLC_INCLUDE_DIR}
    ${RAPIDJSON_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/external
    ${CMAKE_CURRENT_SOURCE_DIR}/es-core/src
    ${CMAKE_BINARY_DIR}
)

if(ENABLE_PULSE)
 LIST(APPEND COMMON_INCLUDE_DIRS
    ${PULSEAUDIO_INCLUDE_DIR}
      )
endif()

if(USE_SYSTEM_PUGIXML)
  MESSAGE("Use system pugixml")
  find_package(pugixml REQUIRED)
endif()

#add libCEC_INCLUDE_DIR
if(CEC)
  if(DEFINED libCEC_FOUND)
    LIST(APPEND COMMON_INCLUDE_DIRS
      ${libCEC_INCLUDE_DIR}
      )
  endif()
endif()

#add ALSA for Linux
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    LIST(APPEND COMMON_INCLUDE_DIRS
        ${ALSA_INCLUDE_DIRS}
    )
endif()

if(GLES3)
    LIST(APPEND COMMON_INCLUDE_DIRS ${OPENGLES3_INCLUDE_DIR})
endif()

#-------------------------------------------------------------------------------
#define libraries and directories
set(COMMON_LIBRARIES
    ${FREETYPE_LIBRARIES}
    ${FRIBIDI_LIBRARIES}
    SDL3::SDL3
    SDL3_image::SDL3_image
    audiodecoders
    ${CURL_LIBRARIES}
    ${VLC_LIBRARIES}
    pugixml
    nanosvg 
    id3v2lib	
	libcheevos	
)
  
if(UDEV_FOUND)
 LIST(APPEND COMMON_LIBRARIES
    ${UDEV_LIBRARY}
      )
endif()

if(ENABLE_PULSE)
 LIST(APPEND COMMON_LIBRARIES
    ${PULSEAUDIO_LIBRARY}
      )
endif()

if(ENABLE_TTS)
 LIST(APPEND COMMON_LIBRARIES
    ${LIBESPEAK_LIBRARIES}
      )
endif()

#add libCEC_LIBRARIES
if(CEC)
  if(DEFINED libCEC_FOUND)
    LIST(APPEND COMMON_LIBRARIES
      dl
      ${libCEC_LIBRARIES}
      )
  endif()
endif()

#add ALSA for Linux
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    LIST(APPEND COMMON_LIBRARIES
        ${ALSA_LIBRARY}
    )
endif()

if(GLES3)
    LIST(APPEND COMMON_LIBRARIES EGL ${OPENGLES3_LIBRARIES})
else()
    LIST(APPEND COMMON_LIBRARIES ${ES_OPENGL_TARGET})
endif()

# batocera / i18n
# i18n
find_program (MSGFMT_EXECUTABLE msgfmt)
find_program (MSGMERGE_EXECUTABLE msgmerge)
find_program (XGETTEXT_EXECUTABLE xgettext)
find_package(Intl)

# gettext itself is part of glibc; msgfmt is only needed to compile the translations
if(Intl_FOUND)
  message (STATUS "Native language support enabled.")
  add_definitions(-DHAVE_INTL)
else()
  message (STATUS "Native language support disabled.")
endif()

find_program (PYTHON3_EXECUTABLE python3)
if(PYTHON3_EXECUTABLE)
  add_custom_target (checkgamesdb ALL COMMENT "Checking guns and wheels games db.")
  add_custom_command (TARGET checkgamesdb COMMAND "${PYTHON3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/resources/checkWheelGunGamesResources.py")
endif()

#-------------------------------------------------------------------------------
# add each component

# Headers that most files of es-core and es-app include, compiled once per target (ES_PCH)
set(ES_PRECOMPILED_HEADERS
    <algorithm>
    <functional>
    <map>
    <memory>
    <mutex>
    <set>
    <sstream>
    <string>
    <thread>
    <unordered_map>
    <vector>
    ${CMAKE_CURRENT_SOURCE_DIR}/es-core/src/GuiComponent.h
    ${CMAKE_CURRENT_SOURCE_DIR}/es-core/src/Window.h
    ${CMAKE_CURRENT_SOURCE_DIR}/es-core/src/Log.h
    ${CMAKE_CURRENT_SOURCE_DIR}/es-core/src/Settings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/es-core/src/LocaleES.h
    ${CMAKE_CURRENT_SOURCE_DIR}/es-core/src/utils/StringUtil.h
)

add_subdirectory("external")
add_subdirectory("es-core")
add_subdirectory("es-launcher")
add_subdirectory("es-app")

if(MSGFMT_EXECUTABLE AND Intl_FOUND)
  add_subdirectory (locale)
endif()

