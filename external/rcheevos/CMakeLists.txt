project("rcheevos")

set(CHEEVOS_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/include/rurl.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/rcheevos.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/rconsoles.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/rhash.h		
)

set(CHEEVOS_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/rhash/hash.c	
	${CMAKE_CURRENT_SOURCE_DIR}/src/rhash/md5.c	
	${CMAKE_CURRENT_SOURCE_DIR}/src/rcheevos/alloc.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/rcheevos/compat.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/rcheevos/compat.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/rcheevos/condition.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/rcheevos/condset.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/rcheevos/consoleinfo.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/rcheevos/format.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/rcheevos/internal.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/rcheevos/lboard.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/rcheevos/memref.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/rcheevos/operand.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/rcheevos/richpresence.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/rcheevos/runtime.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/rcheevos/runtime_progress.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/rcheevos/trigger.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/rcheevos/value.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/rurl/url.c	
	
)

add_definitions("-DRC_DISABLE_LUA")

include_directories(${COMMON_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(rcheevos STATIC ${CHEEVOS_SOURCES} ${CHEEVOS_HEADERS})
target_link_libraries(rcheevos ${COMMON_LIBRARIES})
