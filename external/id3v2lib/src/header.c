/*
 * This file is part of the id3v2lib library
 *
 * Copyright (c) 2013, Lorenzo Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "header.h"
#include "utils.h"

int has_id3v2tag(ID3v2_header* tag_header)
{
    if(memcmp(tag_header->tag, "ID3", 3) == 0)
    {
        return 1;
    }

    return 0;
}

int _has_id3v2tag(char* raw_header)
{
    if(memcmp(raw_header, "ID3", 3) == 0)
    {
        return 1;
    }

    return 0;
}

ID3v2_header* get_tag_header(const char* file_name)
{
    char buffer[ID3_HEADER];
    FILE* file = fopen(file_name, "rb");
    if(file == NULL)
    {
        perror("Error opening file");
        return NULL;
    }

    size_t bytesRead = fread(buffer, ID3_HEADER, 1, file);
    if (bytesRead != 1)
    {
        perror("Error reading file");
        fclose(file);
        return NULL;
    }

    fclose(file);
    return get_tag_header_with_buffer(buffer, ID3_HEADER);
}
ID3v2_header* get_tag_header_with_buffer(char *buffer, int length)
{
    int position = 0;
    ID3v2_header *tag_header;

    if(length < ID3_HEADER) {
        return NULL;
    }
    if( ! _has_id3v2tag(buffer))
    {
        return NULL;
    }
    tag_header = new_header();

    memcpy(tag_header->tag, buffer, ID3_HEADER_TAG);
    tag_header->major_version = buffer[position += ID3_HEADER_TAG];
    tag_header->minor_version = buffer[position += ID3_HEADER_VERSION];
    tag_header->flags = buffer[position += ID3_HEADER_REVISION];
    tag_header->tag_size = syncint_decode(btoi(buffer, ID3_HEADER_SIZE, position += ID3_HEADER_FLAGS));

    if(tag_header->flags&(1<<6)==(1<<6))
    {
      // an extended header exists, so we retrieve the actual size of it and save it into the struct
      tag_header->extended_header_size = syncint_decode(btoi(buffer, ID3_EXTENDED_HEADER_SIZE, position += ID3_HEADER_SIZE));
    }
    else
    {
      // no extended header existing
      tag_header->extended_header_size = 0;
    }

    return tag_header;
}

int get_tag_version(ID3v2_header* tag_header)
{
    if(tag_header->major_version == 3)
    {
        return ID3v23;
    }
    else if(tag_header->major_version == 4)
    {
        return ID3v24;
    }
    else
    {
        return NO_COMPATIBLE_TAG;
    }
}
