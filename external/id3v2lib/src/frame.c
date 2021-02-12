/*
 * This file is part of the id3v2lib library
 *
 * Copyright (c) 2013, Lorenzo Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "frame.h"
#include "utils.h"
#include "constants.h"

ID3v2_frame* parse_frame(char* bytes, int offset, int version, int bufferSize)
{
    ID3v2_frame* frame = new_frame();
    
    // Parse frame header
    memcpy(frame->frame_id, bytes + offset, ID3_FRAME_ID);
    // Check if we are into padding
    if(memcmp(frame->frame_id, "\0\0\0\0", 4) == 0)
    {
        free(frame);
        return NULL;
    }

    frame->size = btoi(bytes, 4, offset += ID3_FRAME_ID);
    if(version == ID3v24)
    {
        frame->size = syncint_decode(frame->size);
    }

	memcpy(frame->flags, bytes + (offset += ID3_FRAME_SIZE), 2);
    
	// Invalid size !?
	if (frame->size + offset > bufferSize)
	{
		free(frame);
		return NULL;
	}

    // Load frame data
    frame->data = (char*) malloc(frame->size * sizeof(char));
    memcpy(frame->data, bytes + (offset += ID3_FRAME_FLAGS), frame->size);
    
    return frame;
}

int get_frame_type(char* frame_id)
{
    switch(frame_id[0])
    {
        case 'T':
            return TEXT_FRAME;
        case 'C':
            return COMMENT_FRAME;
        case 'A':
            return APIC_FRAME;
        default:
            return INVALID_FRAME;
    }
}

ID3v2_frame_text_content* parse_text_frame_content(ID3v2_frame* frame)
{
    ID3v2_frame_text_content* content;
    if(frame == NULL)
    {
        return NULL;
    }

    content = new_text_content(frame->size);
    content->encoding = frame->data[0];
    content->size = frame->size - ID3_FRAME_ENCODING;
    memcpy(content->data, frame->data + ID3_FRAME_ENCODING, content->size);

    return content;
}

ID3v2_frame_comment_content* parse_comment_frame_content(ID3v2_frame* frame)
{
    ID3v2_frame_comment_content *content;
    if(frame == NULL)
    {
        return NULL;
    }
    
    content = new_comment_content(frame->size);
    
    content->text->encoding = frame->data[0];
    content->text->size = frame->size - ID3_FRAME_ENCODING - ID3_FRAME_LANGUAGE - ID3_FRAME_SHORT_DESCRIPTION;
    memcpy(content->language, frame->data + ID3_FRAME_ENCODING, ID3_FRAME_LANGUAGE);
    content->short_description = "\0"; // Ignore short description
    memcpy(content->text->data, frame->data + ID3_FRAME_ENCODING + ID3_FRAME_LANGUAGE + 1, content->text->size);
    
    return content;
}

char* parse_mime_type(char* data, int* i)
{
    char* mime_type = (char*) malloc(30 * sizeof(char));
    
    while(data[*i] != '\0' && *i < 30)
    {
        mime_type[*i - 1] = data[*i];
        (*i)++;
    }
    
    return mime_type;
}

ID3v2_frame_apic_content* parse_apic_frame_content(ID3v2_frame* frame)
{
    ID3v2_frame_apic_content *content;
    int i = 1; // Skip ID3_FRAME_ENCODING

    if(frame == NULL)
    {
        return NULL;
    }
    
    content = new_apic_content();
    
    content->encoding = frame->data[0];
    
    content->mime_type = parse_mime_type(frame->data, &i);
    content->picture_type = frame->data[++i];
    content->description = &frame->data[++i];

    if (content->encoding == 0x01 || content->encoding == 0x02) {
            /* skip UTF-16 description */
            for ( ; * (uint16_t *) (frame->data + i); i += 2);
            i += 2;
    }
    else {
            /* skip UTF-8 or Latin-1 description */
            for ( ; frame->data[i] != '\0'; i++);
            i += 1;
    }
  
    content->picture_size = frame->size - i;
    content->data = (char*) malloc(content->picture_size);
    memcpy(content->data, frame->data + i, content->picture_size);
    
    return content;
}
