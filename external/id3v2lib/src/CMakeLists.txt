INCLUDE_DIRECTORIES(${id3v2lib_SOURCE_DIR}/include ${id3v2lib_SOURCE_DIR}/include/id3v2lib)

SET(id3v2_src frame.c header.c id3v2lib.c types.c utils.c)
SET(id3v2_headers_directory ${id3v2lib_SOURCE_DIR}/include/id3v2lib)

ADD_LIBRARY(id3v2 STATIC ${id3v2_src})

INSTALL(TARGETS id3v2 DESTINATION lib)
INSTALL(DIRECTORY ${id3v2_headers_directory} DESTINATION include)
INSTALL(FILES ${id3v2lib_SOURCE_DIR}/include/id3v2lib.h DESTINATION include)