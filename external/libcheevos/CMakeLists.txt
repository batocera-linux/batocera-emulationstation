project("libcheevos")

set(CHEEVOS_HEADERS

	${CMAKE_CURRENT_SOURCE_DIR}/cheevos.h

	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/include/rurl.h
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/include/rcheevos.h
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/include/rconsoles.h
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/include/rhash.h		
)

set(CHEEVOS_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/cheevos.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rhash/hash.c	
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rhash/md5.c	
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rcheevos/alloc.c
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rcheevos/compat.c
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rcheevos/compat.h
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rcheevos/condition.c
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rcheevos/condset.c
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rcheevos/consoleinfo.c
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rcheevos/format.c
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rcheevos/internal.h
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rcheevos/lboard.c
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rcheevos/memref.c
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rcheevos/operand.c
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rcheevos/richpresence.c
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rcheevos/runtime.c
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rcheevos/runtime_progress.c
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rcheevos/trigger.c
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rcheevos/value.c
	${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src/rurl/url.c	
	
)

set(LIBRETRO_COMMON_HEADERS

	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/retro_miscellaneous.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/retro_endianness.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/retro_common.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/retro_common_api.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/retro_environment.h		
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/retro_inline.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/retro_assert.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/retro_math.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/retro_timers.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/retro_dirent.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/libretro.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/boolean.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/memalign.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/memmap.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/formats/cdfs.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/formats/m3u_file.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/streams/chd_stream.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/streams/file_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/streams/file_stream_transforms.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/streams/interface_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/streams/memory_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/streams/rzip_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/streams/stdin_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/streams/trans_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/libchdr/bitstream.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/libchdr/cdrom.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/libchdr/chd.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/libchdr/coretypes.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/libchdr/huffman.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/libchdr/libchdr_zlib.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/libchdr/lzma.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/libchdr/minmax.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/libchdr/flac.h		
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/file/file_path.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/string/stdstring.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/lists/dir_list.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/lists/string_list.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/cdrom/cdrom.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/time/rtime.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/vfs/vfs.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/vfs/vfs_implementation.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/vfs/vfs_implementation_cdrom.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/compat/msvc.h		
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/encodings/crc32.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include/encodings/utf.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/7zip/LzmaDec.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/7zip/LzmaEnc.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/7zip/7z.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/7zip/7zTypes.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/7zip/7zFile.h	
)

set(LIBRETRO_COMMON_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/file/file_path.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/file/file_path_io.c	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/file/retro_dirent.c	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/compat/compat_strl.c	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/compat/compat_posix_string.c	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/compat/fopen_utf8.c		
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/memmap/memalign.c	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/memmap/memmap.c		
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/formats/cdfs/cdfs.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/formats/libchdr/libchdr_bitstream.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/formats/libchdr/libchdr_cdrom.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/formats/libchdr/libchdr_chd.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/formats/libchdr/libchdr_huffman.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/formats/libchdr/libchdr_lzma.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/formats/libchdr/libchdr_zlib.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/formats/libchdr/libchdr_flac.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/formats/libchdr/libchdr_flac_codec.c	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/formats/m3u/m3u_file.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/streams/chd_stream.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/streams/file_stream.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/streams/file_stream_transforms.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/streams/interface_stream.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/streams/memory_stream.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/streams/rzip_stream.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/streams/stdin_stream.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/streams/trans_stream.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/streams/trans_stream_pipe.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/streams/trans_stream_zlib.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/string/stdstring.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/vfs/vfs_implementation.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/vfs/vfs_implementation_cdrom.c	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/lists/dir_list.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/lists/string_list.c	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/time/rtime.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/cdrom/cdrom.c	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/encodings/encoding_crc32.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/encodings/encoding_utf.c
	                           
	# 7zip                     
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/7zip/LzmaDec.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/7zip/LzmaEnc.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/7zip/7zBuf.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/7zip/7zCrc.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/7zip/7zDec.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/7zip/7zFile.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/7zip/7zIn.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/7zip/7zStream.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/7zip/Bcj2.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/7zip/Bra.c	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/7zip/LzFind.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/7zip/Lzma2Dec.c
                                
	# zip                      
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libz/adler32.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libz/compress.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libz/deflate.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libz/gzclose.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libz/gzlib.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libz/gzread.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libz/gzwrite.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libz/inffast.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libz/inflate.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libz/inftrees.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libz/libz-crc32.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libz/trees.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libz/uncompr.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libz/zutil.c
	                            
	# flac                     
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libFLAC/bitmath.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libFLAC/bitreader.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libFLAC/cpu.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libFLAC/crc.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libFLAC/fixed.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libFLAC/float.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libFLAC/format.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libFLAC/lpc.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libFLAC/md5.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libFLAC/memory.c
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libFLAC/stream_decoder.c	

	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libFLAC/include/FLAC/assert.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libFLAC/include/FLAC/callback.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libFLAC/include/FLAC/export.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libFLAC/include/FLAC/format.h	
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libFLAC/include/FLAC/metadata.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libFLAC/include/FLAC/ordinals.h
	${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/src/libFLAC/include/FLAC/stream_decoder.h
)

# Keep Directory structure in Visual Studio
if(MSVC)
	file(
		 GLOB_RECURSE source_list RELATIVE
		 "${CMAKE_CURRENT_SOURCE_DIR}"
		 *.cpp *.c
		)
	foreach(source IN LISTS source_list)
		get_filename_component(source_path "${source}" PATH)
		string(REPLACE "/" "\\" source_path_msvc "${source_path}")
		string(REPLACE "src" "" source_path_final "${source_path_msvc}")
		source_group("${source_path_final}" FILES "${source}")
	endforeach()  	
endif()


add_definitions("-DRC_DISABLE_LUA")
add_definitions("-DHAVE_CDROM -DHAVE_CHD -DWANT_SUBCODE -DWANT_RAW_DATA_SECTOR -DHAVE_ZLIB -DHAVE_7ZIP -DHAVE_FLAC -DFLAC__NO_DLL -DFLAC__HAS_OGG=0")

if(NOT DEFINED MSVC)
	add_definitions("-DHAVE_LROUND")
endif()

include_directories(${COMMON_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/include)
include_directories(${COMMON_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include)
add_library(libcheevos STATIC ${CHEEVOS_SOURCES} ${CHEEVOS_HEADERS} ${LIBRETRO_COMMON_HEADERS} ${LIBRETRO_COMMON_SOURCES})
