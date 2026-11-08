/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "modules/char_stream.private.h"
#include "modules/utils.private.h"

#include "frame_header.private.h"

ID3v2_FrameHeader* FrameHeader_new(const char* id, const char* flags, const int size)
{
    ID3v2_FrameHeader* frame_header = (ID3v2_FrameHeader*) malloc(sizeof(ID3v2_FrameHeader));

    memcpy(frame_header->id, id, ID3v2_FRAME_HEADER_ID_LENGTH);
    memcpy(frame_header->flags, flags, ID3v2_FRAME_HEADER_FLAGS_LENGTH);
    frame_header->size = size;

    return frame_header;
}

ID3v2_FrameHeader* FrameHeader_parse(CharStream* header_cs, const int id3_major_version)
{
    char id[ID3v2_FRAME_HEADER_ID_LENGTH];
    CharStream_read(header_cs, id, ID3v2_FRAME_HEADER_ID_LENGTH);

    // If the id is 0000, that means we already reached the end and we're inside the padding
    if (memcmp(id, "\0\0\0\0", ID3v2_FRAME_HEADER_ID_LENGTH) == 0)
    {
        // Rewind the char stream and return NULL
        CharStream_seek(header_cs, -ID3v2_FRAME_HEADER_ID_LENGTH, SEEK_CUR);
        return NULL;
    }

    int size = btoi(
        CharStream_slice(header_cs, ID3v2_FRAME_HEADER_SIZE_LENGTH),
        ID3v2_FRAME_HEADER_SIZE_LENGTH
    );

    if (id3_major_version == 4)
    {
        size = syncint_decode(size);
    }

    char flags[ID3v2_FRAME_HEADER_FLAGS_LENGTH];
    CharStream_read(header_cs, flags, ID3v2_FRAME_HEADER_FLAGS_LENGTH);

    return FrameHeader_new(id, flags, size);
}

CharStream* FrameHeader_to_char_stream(ID3v2_FrameHeader* header)
{
    CharStream* header_cs = CharStream_new(ID3v2_FRAME_HEADER_LENGTH);

    CharStream_write(header_cs, header->id, ID3v2_FRAME_HEADER_ID_LENGTH);
    char* size_bytes = itob(header->size);
    CharStream_write(header_cs, size_bytes, ID3v2_FRAME_HEADER_SIZE_LENGTH);
    free(size_bytes);
    CharStream_write(header_cs, (const char*) &header->flags, ID3v2_FRAME_HEADER_FLAGS_LENGTH);

    return header_cs;
}

bool FrameHeader_isTextFrame(ID3v2_FrameHeader* header)
{
    return header->id[0] == 'T';
}

bool FrameHeader_isCommentFrame(ID3v2_FrameHeader* header)
{
    return header->id[0] == 'C';
}

bool FrameHeader_isApicFrame(ID3v2_FrameHeader* header)
{
    return header->id[0] == 'A';
}

void FrameHeader_free(ID3v2_FrameHeader* header)
{
    free(header);
}
