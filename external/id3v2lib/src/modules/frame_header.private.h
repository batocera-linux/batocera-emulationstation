/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_frame_header_private_h
#define id3v2lib_frame_header_private_h

#include <stdbool.h>

#include "modules/frame_header.h"

typedef struct _CharStream CharStream;

ID3v2_FrameHeader* FrameHeader_new(const char* id, const char* flags, const int size);
ID3v2_FrameHeader* FrameHeader_parse(CharStream* header_cs, const int id3_major_version);

CharStream* FrameHeader_to_char_stream(ID3v2_FrameHeader* header);

bool FrameHeader_isTextFrame(ID3v2_FrameHeader* header);
bool FrameHeader_isCommentFrame(ID3v2_FrameHeader* header);
bool FrameHeader_isApicFrame(ID3v2_FrameHeader* header);

void FrameHeader_free(ID3v2_FrameHeader* header);

#endif
