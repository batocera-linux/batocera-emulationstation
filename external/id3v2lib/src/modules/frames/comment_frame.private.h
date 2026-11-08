/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_comment_frame_private_h
#define id3v2lib_comment_frame_private_h

#include "modules/frames/comment_frame.h"

typedef struct _CharStream CharStream;

ID3v2_CommentFrame* CommentFrame_new(
    const char* flags,
    const char* lang,
    const char* short_desc,
    const char* comment
);
ID3v2_CommentFrame* CommentFrame_parse(CharStream* frame_cs, const int id3_major_version);
CharStream* CommentFrame_to_char_stream(ID3v2_CommentFrame* frame);

void CommentFrame_free(ID3v2_CommentFrame* frame);

ID3v2_CommentFrameData* CommentFrameData_new(
    const char* lang,
    const char* short_desc,
    const char* comment
);

#endif
