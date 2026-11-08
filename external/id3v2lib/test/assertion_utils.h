/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_test_assertion_utils_h
#define id3v2lib_test_assertion_utils_h

#include "id3v2lib.h"

typedef struct _Frame_header_assertion
{
    const char* id;
    int size;
    const char* flags;
} Frame_header_assertion;

void assert_frame_header(ID3v2_Frame* frame, Frame_header_assertion comparison);
void assert_text_frame(ID3v2_TextFrame* frame, ID3v2_TextFrameInput* comparison);
void assert_comment_frame(ID3v2_CommentFrame* frame, ID3v2_CommentFrameInput* comparison);
void assert_apic_frame(ID3v2_ApicFrame* frame, ID3v2_ApicFrameInput* comparison);

#endif
