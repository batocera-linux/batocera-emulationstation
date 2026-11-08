/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_text_frame_private_h
#define id3v2lib_text_frame_private_h

#include "modules/frames/text_frame.h"

typedef struct _CharStream CharStream;

ID3v2_TextFrame* TextFrame_new(const char* id, const char* flags, const char* text);
ID3v2_TextFrame* TextFrame_parse(CharStream* frame_cs, const int id3_major_version);
CharStream* TextFrame_to_char_stream(ID3v2_TextFrame* frame);

void TextFrame_free(ID3v2_TextFrame* frame);

ID3v2_TextFrameData* TextFrameData_new(const char* text);

#endif
