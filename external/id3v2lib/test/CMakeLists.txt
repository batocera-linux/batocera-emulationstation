cmake_minimum_required(VERSION 3.1...3.22)

set(TEST_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/assertion_utils.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/compat_test.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/delete_test.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/get_test.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/main_test.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/set_test.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/test_utils.c"
)

set(TEST_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/assertion_utils.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/compat_test.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/delete_test.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/get_test.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/set_test.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/test_utils.h"
)

set(TEST_ASSETS
  "${CMAKE_CURRENT_SOURCE_DIR}/extra/album_cover.png"
  "${CMAKE_CURRENT_SOURCE_DIR}/extra/empty.mp3"
  "${CMAKE_CURRENT_SOURCE_DIR}/extra/file.mp3"
  "${CMAKE_CURRENT_SOURCE_DIR}/extra/no_tag.mp3"
)

add_executable(main_test ${TEST_SRC} ${TEST_HEADERS})

set_target_properties(main_test PROPERTIES C_STANDARD 99)

target_compile_options(main_test PUBLIC -Wall)
target_compile_options(main_test PUBLIC -g)

target_link_libraries(main_test PRIVATE id3v2lib)

file(
  COPY ${TEST_ASSETS}
  DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/extra"
)
