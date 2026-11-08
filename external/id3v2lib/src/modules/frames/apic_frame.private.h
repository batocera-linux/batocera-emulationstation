/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_apic_frame_private_h
#define id3v2lib_apic_frame_private_h

#include "modules/frames/apic_frame.h"

typedef struct _CharStream CharStream;

ID3v2_ApicFrame* ApicFrame_new(
    const char* flags,
    const char* description,
    const char picture_type,
    const char* mime_type,
    const int picture_size,
    const char* data
);
ID3v2_ApicFrame* ApicFrame_parse(CharStream* frame_cs, const int id3_major_version);
CharStream* ApicFrame_to_char_stream(ID3v2_ApicFrame* frame);

void ApicFrame_free(ID3v2_ApicFrame* frame);

ID3v2_ApicFrameData* ApicFrameData_new(
    const char* description,
    const char picture_type,
    const char* mime_type,
    const int picture_size,
    const char* picture_data
);

#endif
