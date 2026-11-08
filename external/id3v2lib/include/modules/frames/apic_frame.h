/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_apic_frame_h
#define id3v2lib_apic_frame_h

#define ID3v2_APIC_FRAME_PICTURE_TYPE_LENGTH 1

#define ID3v2_MIME_TYPE_JPG "image/jpeg"
#define ID3v2_MIME_TYPE_PNG "image/png"

typedef struct _ID3v2_FrameHeader ID3v2_FrameHeader;

typedef struct _ID3v2_ApicFrameData
{
    char encoding;
    char* mime_type;
    char picture_type;
    char* description;
    char* data;
    int picture_size;
} ID3v2_ApicFrameData;

typedef struct _ID3v2_ApicFrame
{
    ID3v2_FrameHeader* header;
    ID3v2_ApicFrameData* data;
} ID3v2_ApicFrame;

#endif
