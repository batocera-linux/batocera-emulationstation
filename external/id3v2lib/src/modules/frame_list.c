/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include <stdlib.h>
#include <string.h>

#include "modules/frame.private.h"
#include "modules/frame_header.private.h"
#include "modules/frame_list.private.h"

#include "frame_list.private.h"

ID3v2_FrameList* FrameList_new()
{
    ID3v2_FrameList* list = (ID3v2_FrameList*) malloc(sizeof(ID3v2_FrameList));

    if (list != NULL)
    {
        list->frame = NULL;
        list->next = NULL;
        list->start = NULL;
    }

    return list;
}

void FrameList_add_frame(ID3v2_FrameList* list, ID3v2_Frame* frame)
{
    // If the list is empty
    if (list->start == NULL)
    {
        list->start = list;
        list->frame = frame;
    }
    else
    {
        ID3v2_FrameList* current = FrameList_new();
        current->frame = frame;
        current->start = list->start;

        ID3v2_FrameList* last = list;
        while (last->next != NULL)
        {
            last = last->next;
        }

        last->next = current;
    }
}

/**
 * Returns the first frame matching frame_id
 */
ID3v2_Frame* FrameList_get_frame_by_id(ID3v2_FrameList* list, const char* frame_id)
{
    while (list != NULL && list->frame != NULL)
    {
        if (strncmp(list->frame->header->id, frame_id, 4) == 0)
        {
            return list->frame;
        }

        list = list->next;
    }

    return NULL;
}

/**
 * Returns all the frames matching frame_id in a sublist
 */
ID3v2_FrameList* FrameList_get_frames_by_id(ID3v2_FrameList* list, const char* frame_id)
{
    ID3v2_FrameList* sublist = FrameList_new();

    while (list != NULL && list->frame != NULL)
    {
        if (strncmp(list->frame->header->id, frame_id, 4) == 0)
        {
            FrameList_add_frame(sublist, list->frame);
        }

        list = list->next;
    }

    return sublist;
}

void ID3v2_FrameList_free(ID3v2_FrameList* list)
{
    while (list != NULL)
    {
        ID3v2_Frame_free(list->frame);
        ID3v2_FrameList* next_head = list->next;
        free(list);
        list = next_head;
    }
}

void ID3v2_FrameList_unlink(ID3v2_FrameList* list)
{
    while (list != NULL)
    {
        ID3v2_FrameList* next_head = list->next;
        free(list);
        list = next_head;
    }
}

ID3v2_Frame* FrameList_remove_frame_by_id(ID3v2_FrameList* list, const char* frame_id)
{
    if (list == NULL) return NULL;

    while (list != NULL)
    {
        if (strncmp(list->frame->header->id, frame_id, 4) == 0)
        {
            ID3v2_Frame* removed = list->frame;
            ID3v2_FrameList* new_head_old_ptr = list->next != NULL ? list->next : FrameList_new();
            *list = *new_head_old_ptr;
            free(new_head_old_ptr);
            return removed;
        }

        list = list->next;
    }

    return NULL;
}

ID3v2_Frame* FrameList_remove_frame(ID3v2_FrameList* list, ID3v2_Frame* to_remove)
{
    if (list == NULL) return NULL;

    while (list != NULL)
    {
        if (list->frame == to_remove)
        {
            ID3v2_FrameList* new_head_old_ptr = list->next != NULL ? list->next : FrameList_new();
            *list = *new_head_old_ptr;
            free(new_head_old_ptr);
            return to_remove;
        }

        list = list->next;
    }

    return NULL;
}

/**
 * This does not free the replaced frame. That's responsibility
 * of the calling routine.
 */
void FrameList_replace_frame(ID3v2_FrameList* list, ID3v2_Frame* old_frame, ID3v2_Frame* new_frame)
{
    if (list == NULL) return;

    while (list != NULL)
    {
        if (list->frame == old_frame)
        {
            list->frame = new_frame;
            return;
        }

        list = list->next;
    }
}
