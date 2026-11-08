/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_comment_frame_h
#define id3v2lib_comment_frame_h

#define ID3v2_COMMENT_FRAME_LANGUAGE_LENGTH 3

typedef struct _ID3v2_FrameHeader ID3v2_FrameHeader;

typedef struct _ID3v2_CommentFrameData
{
    int size;
    char encoding;
    char language[ID3v2_COMMENT_FRAME_LANGUAGE_LENGTH];
    char* short_description;
    char* comment;
} ID3v2_CommentFrameData;

typedef struct _ID3v2_CommentFrame
{
    ID3v2_FrameHeader* header;
    ID3v2_CommentFrameData* data;
} ID3v2_CommentFrame;

#endif
