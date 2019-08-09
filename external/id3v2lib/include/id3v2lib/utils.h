/*
 * This file is part of the id3v2lib library
 *
 * Copyright (c) 2013, Lorenzo Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_utils_h
#define id3v2lib_utils_h

// this piece of code makes this header usable under MSVC
// without downloading msinttypes
#ifndef _MSC_VER
  #include <inttypes.h>
#else
  typedef unsigned short uint16_t;
#endif

#include "types.h"

unsigned int btoi(char* bytes, int size, int offset);
char* itob(int integer);
int syncint_encode(int value);
int syncint_decode(int value);
void add_to_list(ID3v2_frame_list* list, ID3v2_frame* frame);
ID3v2_frame* get_from_list(ID3v2_frame_list* list, char* frame_id);
void free_tag(ID3v2_tag* tag);
char* get_mime_type_from_filename(const char* filename);

void genre_num_string(char* dest, char *genre_data);
char* convert_genre_number(int number);

// String functions
int has_bom(uint16_t* string);
uint16_t* char_to_utf16(char* string, int size);
void println_utf16(uint16_t* string, int size);
char* get_path_to_file(const char* file);

#endif
