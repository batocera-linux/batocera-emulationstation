cmake_minimum_required(VERSION 3.1...3.22)

set(ID3V2_PUBLIC_HEADERS
  "${CMAKE_CURRENT_LIST_DIR}/../include/modules/frames/apic_frame.h"
  "${CMAKE_CURRENT_LIST_DIR}/../include/modules/frames/comment_frame.h"
  "${CMAKE_CURRENT_LIST_DIR}/../include/modules/frames/text_frame.h"
  "${CMAKE_CURRENT_LIST_DIR}/../include/modules/frame_header.h"
  "${CMAKE_CURRENT_LIST_DIR}/../include/modules/frame_ids.h"
  "${CMAKE_CURRENT_LIST_DIR}/../include/modules/frame_list.h"
  "${CMAKE_CURRENT_LIST_DIR}/../include/modules/frame.h"
  "${CMAKE_CURRENT_LIST_DIR}/../include/modules/picture_types.h"
  "${CMAKE_CURRENT_LIST_DIR}/../include/modules/tag_header.h"
  "${CMAKE_CURRENT_LIST_DIR}/../include/modules/tag.h"
  "${CMAKE_CURRENT_LIST_DIR}/../include/modules/utils.h"
  "${CMAKE_CURRENT_LIST_DIR}/../include/id3v2lib.h"
  "${CMAKE_CURRENT_LIST_DIR}/../include/id3v2lib.compat.h"
)

set(ID3V2_HEADERS
  ${ID3V2_PUBLIC_HEADERS}
  "${CMAKE_CURRENT_LIST_DIR}/modules/frames/apic_frame.private.h"
  "${CMAKE_CURRENT_LIST_DIR}/modules/frames/comment_frame.private.h"
  "${CMAKE_CURRENT_LIST_DIR}/modules/frames/text_frame.private.h"
  "${CMAKE_CURRENT_LIST_DIR}/modules/char_stream.private.h"
  "${CMAKE_CURRENT_LIST_DIR}/modules/frame_header.private.h"
  "${CMAKE_CURRENT_LIST_DIR}/modules/frame_list.private.h"
  "${CMAKE_CURRENT_LIST_DIR}/modules/frame.private.h"
  "${CMAKE_CURRENT_LIST_DIR}/modules/tag_header.private.h"
  "${CMAKE_CURRENT_LIST_DIR}/modules/tag.private.h"
  "${CMAKE_CURRENT_LIST_DIR}/modules/utils.private.h"
)

set(ID3V2_SRC
  "${CMAKE_CURRENT_LIST_DIR}/modules/frames/apic_frame.c"
  "${CMAKE_CURRENT_LIST_DIR}/modules/frames/comment_frame.c"
  "${CMAKE_CURRENT_LIST_DIR}/modules/frames/text_frame.c"
  "${CMAKE_CURRENT_LIST_DIR}/modules/char_stream.c"
  "${CMAKE_CURRENT_LIST_DIR}/modules/frame_header.c"
  "${CMAKE_CURRENT_LIST_DIR}/modules/frame_list.c"
  "${CMAKE_CURRENT_LIST_DIR}/modules/frame.c"
  "${CMAKE_CURRENT_LIST_DIR}/modules/tag_header.c"
  "${CMAKE_CURRENT_LIST_DIR}/modules/tag.c"
  "${CMAKE_CURRENT_LIST_DIR}/modules/utils.c"
  "${CMAKE_CURRENT_LIST_DIR}/id3v2lib.c"
  "${CMAKE_CURRENT_LIST_DIR}/id3v2lib.compat.c"
)

add_library(id3v2lib ${ID3V2_SRC} ${ID3V2_HEADERS})

target_include_directories(id3v2lib
  PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/../include"
)

set_target_properties(id3v2lib PROPERTIES C_STANDARD 99)

target_compile_options(id3v2lib PUBLIC -Wall)
target_compile_options(id3v2lib PUBLIC -g)

install(
  TARGETS id3v2lib
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}-${PROJECT_VERSION}"
)

install(
  DIRECTORY "${CMAKE_SOURCE_DIR}/include/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}-${PROJECT_VERSION}"
  FILES_MATCHING
  PATTERN "*.h"
)
