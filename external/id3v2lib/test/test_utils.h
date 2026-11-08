/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_test_utils_h
#define id3v2lib_test_utils_h

#include <stdbool.h>

#include "id3v2lib.h"

// This makes MSVC happy without having to download msinttypes
#ifndef _MSC_VER
    #include <inttypes.h>
#else
typedef unsigned short uint16_t;
#endif

bool has_bom(const char* string);
void print_text_frame_text(const char* text, const int size);
void print_text_frame(ID3v2_TextFrame* frame);
void print_comment_frame(ID3v2_CommentFrame* frame);
void print_comment_frames(ID3v2_FrameList* frames);
void save_apic_frame(ID3v2_ApicFrame* frame, char* dir_path);
uint16_t* char_to_utf16(const char* string, int size);
void println_utf16(uint16_t* string, int size);
char* to_unicode(char* string);
void clone_file(const char* src, const char* dest);

#endif
