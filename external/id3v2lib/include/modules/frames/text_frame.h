/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_text_frame_h
#define id3v2lib_text_frame_h

typedef struct _ID3v2_FrameHeader ID3v2_FrameHeader;

typedef struct _ID3v2_TextFrameData
{
    int size;
    char encoding;
    char* text;
} ID3v2_TextFrameData;

typedef struct _ID3v2_TextFrame
{
    ID3v2_FrameHeader* header;
    ID3v2_TextFrameData* data;
} ID3v2_TextFrame;

#endif
