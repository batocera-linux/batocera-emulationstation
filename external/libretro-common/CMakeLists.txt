project("libretro-common")

set(LIBRETRO_COMMON_HEADERS

	${CMAKE_CURRENT_SOURCE_DIR}/include/retro_miscellaneous.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/retro_endianness.h	
	${CMAKE_CURRENT_SOURCE_DIR}/include/retro_common_api.h	
	${CMAKE_CURRENT_SOURCE_DIR}/include/retro_environment.h		
	${CMAKE_CURRENT_SOURCE_DIR}/include/retro_inline.h	
	${CMAKE_CURRENT_SOURCE_DIR}/include/retro_assert.h	
	${CMAKE_CURRENT_SOURCE_DIR}/include/retro_math.h	
	${CMAKE_CURRENT_SOURCE_DIR}/include/retro_timers.h	
	${CMAKE_CURRENT_SOURCE_DIR}/include/retro_dirent.h	
	${CMAKE_CURRENT_SOURCE_DIR}/include/libretro.h	
	${CMAKE_CURRENT_SOURCE_DIR}/include/boolean.h
	
	${CMAKE_CURRENT_SOURCE_DIR}/include/memalign.h	
	${CMAKE_CURRENT_SOURCE_DIR}/include/memmap.h

	${CMAKE_CURRENT_SOURCE_DIR}/include/file/file_path.h
	
	${CMAKE_CURRENT_SOURCE_DIR}/include/formats/cdfs.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/formats/m3u_file.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/streams/chd_stream.h	
	${CMAKE_CURRENT_SOURCE_DIR}/include/streams/file_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/streams/file_stream_transforms.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/streams/interface_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/streams/memory_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/streams/rzip_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/streams/stdin_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/streams/trans_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libchdr/bitstream.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libchdr/cdrom.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libchdr/chd.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libchdr/coretypes.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libchdr/huffman.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libchdr/libchdr_zlib.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libchdr/lzma.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libchdr/minmax.h
	
	${CMAKE_CURRENT_SOURCE_DIR}/include/string/stdstring.h
	
	${CMAKE_CURRENT_SOURCE_DIR}/include/lists/dir_list.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/lists/string_list.h
	
	${CMAKE_CURRENT_SOURCE_DIR}/include/cdrom/cdrom.h
	
	${CMAKE_CURRENT_SOURCE_DIR}/include/time/rtime.h
	
	${CMAKE_CURRENT_SOURCE_DIR}/include/vfs/vfs.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/vfs/vfs_implementation.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/vfs/vfs_implementation_cdrom.h
	
	${CMAKE_CURRENT_SOURCE_DIR}/include/compat/msvc.h	
	
	${CMAKE_CURRENT_SOURCE_DIR}/include/encodings/crc32.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/encodings/utf.h
	
	${CMAKE_CURRENT_SOURCE_DIR}/7zip/LzmaDec.h
	${CMAKE_CURRENT_SOURCE_DIR}/7zip/LzmaEnc.h
)

set(LIBRETRO_COMMON_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/file/file_path.c
	${CMAKE_CURRENT_SOURCE_DIR}/file/file_path_io.c
	
	${CMAKE_CURRENT_SOURCE_DIR}/compat/compat_strl.c	
	${CMAKE_CURRENT_SOURCE_DIR}/compat/compat_posix_string.c	
	${CMAKE_CURRENT_SOURCE_DIR}/compat/fopen_utf8.c	
	
	${CMAKE_CURRENT_SOURCE_DIR}/memmap/memalign.c	
	${CMAKE_CURRENT_SOURCE_DIR}/memmap/memmap.c	
	
	${CMAKE_CURRENT_SOURCE_DIR}/formats/cdfs/cdfs.c
	${CMAKE_CURRENT_SOURCE_DIR}/formats/libchdr/libchdr_bitstream.c
	${CMAKE_CURRENT_SOURCE_DIR}/formats/libchdr/libchdr_cdrom.c
	${CMAKE_CURRENT_SOURCE_DIR}/formats/libchdr/libchdr_chd.c
	${CMAKE_CURRENT_SOURCE_DIR}/formats/libchdr/libchdr_huffman.c
	${CMAKE_CURRENT_SOURCE_DIR}/formats/libchdr/libchdr_lzma.c
	${CMAKE_CURRENT_SOURCE_DIR}/formats/libchdr/libchdr_zlib.c
	${CMAKE_CURRENT_SOURCE_DIR}/formats/libchdr/libchdr_flac.c
	${CMAKE_CURRENT_SOURCE_DIR}/formats/libchdr/libchdr_flac_codec.c	
	${CMAKE_CURRENT_SOURCE_DIR}/formats/m3u/m3u_file.c
	${CMAKE_CURRENT_SOURCE_DIR}/streams/chd_stream.c
	${CMAKE_CURRENT_SOURCE_DIR}/streams/file_stream.c
	${CMAKE_CURRENT_SOURCE_DIR}/streams/file_stream_transforms.c
	${CMAKE_CURRENT_SOURCE_DIR}/streams/interface_stream.c
	${CMAKE_CURRENT_SOURCE_DIR}/streams/memory_stream.c
	${CMAKE_CURRENT_SOURCE_DIR}/streams/rzip_stream.c
	${CMAKE_CURRENT_SOURCE_DIR}/streams/stdin_stream.c
	${CMAKE_CURRENT_SOURCE_DIR}/streams/trans_stream.c
	${CMAKE_CURRENT_SOURCE_DIR}/streams/trans_stream_pipe.c
	${CMAKE_CURRENT_SOURCE_DIR}/streams/trans_stream_zlib.c
	${CMAKE_CURRENT_SOURCE_DIR}/string/stdstring.c
	${CMAKE_CURRENT_SOURCE_DIR}/vfs/vfs_implementation.c
	${CMAKE_CURRENT_SOURCE_DIR}/vfs/vfs_implementation_cdrom.c	

	${CMAKE_CURRENT_SOURCE_DIR}/lists/dir_list.c
	${CMAKE_CURRENT_SOURCE_DIR}/lists/string_list.c
	
	${CMAKE_CURRENT_SOURCE_DIR}/time/rtime.c
	${CMAKE_CURRENT_SOURCE_DIR}/cdrom/cdrom.c
	
	${CMAKE_CURRENT_SOURCE_DIR}/encodings/encoding_crc32.c
	${CMAKE_CURRENT_SOURCE_DIR}/encodings/encoding_utf.c
	
	# 7zip
	${CMAKE_CURRENT_SOURCE_DIR}/7zip/LzmaDec.c
	${CMAKE_CURRENT_SOURCE_DIR}/7zip/LzmaEnc.c
	${CMAKE_CURRENT_SOURCE_DIR}/7zip/7zBuf.c
	${CMAKE_CURRENT_SOURCE_DIR}/7zip/7zCrc.c
	${CMAKE_CURRENT_SOURCE_DIR}/7zip/7zDec.c
	${CMAKE_CURRENT_SOURCE_DIR}/7zip/7zFile.c
	${CMAKE_CURRENT_SOURCE_DIR}/7zip/7zIn.c
	${CMAKE_CURRENT_SOURCE_DIR}/7zip/7zStream.c
	${CMAKE_CURRENT_SOURCE_DIR}/7zip/Bcj2.c
	${CMAKE_CURRENT_SOURCE_DIR}/7zip/Bra.c	
	${CMAKE_CURRENT_SOURCE_DIR}/7zip/LzFind.c
	${CMAKE_CURRENT_SOURCE_DIR}/7zip/Lzma2Dec.c

	# zip
	${CMAKE_CURRENT_SOURCE_DIR}/libz/adler32.c
	${CMAKE_CURRENT_SOURCE_DIR}/libz/compress.c
	${CMAKE_CURRENT_SOURCE_DIR}/libz/deflate.c
	${CMAKE_CURRENT_SOURCE_DIR}/libz/gzclose.c
	${CMAKE_CURRENT_SOURCE_DIR}/libz/gzlib.c
	${CMAKE_CURRENT_SOURCE_DIR}/libz/gzread.c
	${CMAKE_CURRENT_SOURCE_DIR}/libz/gzwrite.c
	${CMAKE_CURRENT_SOURCE_DIR}/libz/inffast.c
	${CMAKE_CURRENT_SOURCE_DIR}/libz/inflate.c
	${CMAKE_CURRENT_SOURCE_DIR}/libz/inftrees.c
	${CMAKE_CURRENT_SOURCE_DIR}/libz/libz-crc32.c
	${CMAKE_CURRENT_SOURCE_DIR}/libz/trees.c
	${CMAKE_CURRENT_SOURCE_DIR}/libz/uncompr.c
	${CMAKE_CURRENT_SOURCE_DIR}/libz/zutil.c
	
	# flac
	${CMAKE_CURRENT_SOURCE_DIR}/libFLAC/bitmath.c
	${CMAKE_CURRENT_SOURCE_DIR}/libFLAC/bitreader.c
	${CMAKE_CURRENT_SOURCE_DIR}/libFLAC/cpu.c
	${CMAKE_CURRENT_SOURCE_DIR}/libFLAC/crc.c
	${CMAKE_CURRENT_SOURCE_DIR}/libFLAC/fixed.c
	${CMAKE_CURRENT_SOURCE_DIR}/libFLAC/float.c
	${CMAKE_CURRENT_SOURCE_DIR}/libFLAC/format.c
	${CMAKE_CURRENT_SOURCE_DIR}/libFLAC/lpc.c
	${CMAKE_CURRENT_SOURCE_DIR}/libFLAC/md5.c
	${CMAKE_CURRENT_SOURCE_DIR}/libFLAC/memory.c
	${CMAKE_CURRENT_SOURCE_DIR}/libFLAC/stream_decoder.c	
)

add_definitions("-DHAVE_CDROM -DHAVE_CHD -DHAVE_ZLIB -DHAVE_7ZIP -DHAVE_FLAC -DFLAC__NO_DLL -DFLAC__HAS_OGG=0")

if(NOT DEFINED MSVC)
	add_definitions("-DHAVE_LROUND")
endif()

include_directories(${COMMON_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(libretro-common STATIC ${LIBRETRO_COMMON_SOURCES} ${LIBRETRO_COMMON_HEADERS})
