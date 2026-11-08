/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_frame_private_h
#define id3v2lib_frame_private_h

#include "modules/frame.h"

typedef struct _CharStream CharStream;

ID3v2_Frame* Frame_parse(CharStream* frame_cs, int id3_major_version);

CharStream* Frame_to_char_stream(ID3v2_Frame* frame);

#endif
