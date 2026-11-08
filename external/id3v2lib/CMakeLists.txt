cmake_minimum_required(VERSION 3.1...3.22)
project(id3v2lib VERSION 2.0)

include(GNUInstallDirs)

option(BUILD_SHARED_LIBS "Build shared libraries instead of static ones." OFF)

add_subdirectory(src)
add_subdirectory(test)
