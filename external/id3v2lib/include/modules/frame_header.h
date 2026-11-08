/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_frame_header_h
#define id3v2lib_frame_header_h

#define ID3v2_FRAME_HEADER_LENGTH 10
#define ID3v2_FRAME_HEADER_ID_LENGTH 4
#define ID3v2_FRAME_HEADER_SIZE_LENGTH 4
#define ID3v2_FRAME_HEADER_FLAGS_LENGTH 2

typedef struct _ID3v2_FrameHeader
{
    char id[ID3v2_FRAME_HEADER_ID_LENGTH];
    int size;
    char flags[ID3v2_FRAME_HEADER_FLAGS_LENGTH];
} ID3v2_FrameHeader;

#endif
