/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_frame_h
#define id3v2lib_frame_h

#define ID3v2_FRAME_ENCODING_LENGTH 1

#define ID3v2_ENCODING_ISO 0
#define ID3v2_ENCODING_UNICODE 1

typedef struct _ID3v2_FrameHeader ID3v2_FrameHeader;

typedef struct _ID3v2_Frame
{
    ID3v2_FrameHeader* header;
    void* data;
} ID3v2_Frame;

void ID3v2_Frame_free(ID3v2_Frame* frame);

#endif
