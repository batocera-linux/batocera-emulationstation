/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_frame_list_h
#define id3v2lib_frame_list_h

typedef struct _ID3v2_Frame ID3v2_Frame;

typedef struct _ID3v2_FrameList
{
    ID3v2_Frame* frame;
    struct _ID3v2_FrameList* start;
    struct _ID3v2_FrameList* next;
} ID3v2_FrameList;

/**
 * Frees list nodes and the frames within.
 */
void ID3v2_FrameList_free(ID3v2_FrameList* list);

/**
 * This only frees the list nodes leaving freeing the frames
 * to the calling routine.
 */
void ID3v2_FrameList_unlink(ID3v2_FrameList* list);

#endif
