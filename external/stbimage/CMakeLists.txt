project("stbimage")

set(STBIMAGE_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/stb_image.h
	${CMAKE_CURRENT_SOURCE_DIR}/stb_image_resize.h
	${CMAKE_CURRENT_SOURCE_DIR}/stb_image_write.h
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
