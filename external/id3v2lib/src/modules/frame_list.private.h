/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_frame_list_private_h
#define id3v2lib_frame_list_private_h

#include "modules/frame_list.h"

ID3v2_FrameList* FrameList_new();

void FrameList_add_frame(ID3v2_FrameList* list, ID3v2_Frame* frame);

ID3v2_Frame* FrameList_get_frame_by_id(ID3v2_FrameList* list, const char* frame_id);
ID3v2_FrameList* FrameList_get_frames_by_id(ID3v2_FrameList* list, const char* frame_id);

ID3v2_Frame* FrameList_remove_frame_by_id(ID3v2_FrameList* list, const char* frame_id);
ID3v2_Frame* FrameList_remove_frame(ID3v2_FrameList* list, ID3v2_Frame* to_remove);
void FrameList_replace_frame(ID3v2_FrameList* list, ID3v2_Frame* old_frame, ID3v2_Frame* new_frame);

#endif
