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

#include "tag_header.private.h"

ID3v2_TagHeader* TagHeader_new(
    const int major_version,
    const int minor_version,
    const int flags,
    const int tag_size,
    const int extended_header_size
)
{
    ID3v2_TagHeader* tag_header = (ID3v2_TagHeader*) malloc(sizeof(ID3v2_TagHeader));

    if (tag_header != NULL)
    {
        memcpy(tag_header->identifier, "ID3", ID3v2_TAG_HEADER_IDENTIFIER_LENGTH);
        tag_header->major_version = major_version;
        tag_header->minor_version = minor_version;
        tag_header->flags = flags;
        tag_header->tag_size = tag_size;
        tag_header->extended_header_size = extended_header_size;
    }

    return tag_header;
}

ID3v2_TagHeader* TagHeader_new_empty()
{
    return TagHeader_new(3, 0, 0, 0, 0);
}

ID3v2_TagHeader* TagHeader_parse(CharStream* header_cs)
{
    bool has_id3v2_tag = memcmp(header_cs->stream, "ID3", ID3v2_TAG_HEADER_IDENTIFIER_LENGTH) == 0;
    if (!has_id3v2_tag) return NULL;

    char id[ID3v2_TAG_HEADER_IDENTIFIER_LENGTH];
    CharStream_read(header_cs, id, ID3v2_TAG_HEADER_IDENTIFIER_LENGTH);

    const int major_version = CharStream_getc(header_cs);
    const int minor_version = CharStream_getc(header_cs);

    if (major_version != 3 && major_version != 4)
    {
        // No supported id3 tag found
        return NULL;
    }

    const int flags = CharStream_getc(header_cs);

    char raw_size[ID3v2_TAG_HEADER_TAG_SIZE_LENGTH];
    CharStream_read(header_cs, raw_size, ID3v2_TAG_HEADER_TAG_SIZE_LENGTH);
    const int tag_size = syncint_decode(btoi(raw_size, ID3v2_TAG_HEADER_TAG_SIZE_LENGTH));

    int extended_header_size = 0;

    if ((flags & (1 << 6)) == (1 << 6))
    {
        // An extended header exists, retrieve its size
        char raw_eh_size[ID3v2_EXTENDED_HEADER_SIZE_LENGTH];
        CharStream_read(header_cs, raw_eh_size, ID3v2_EXTENDED_HEADER_SIZE_LENGTH);
        extended_header_size = syncint_decode(btoi(raw_eh_size, ID3v2_EXTENDED_HEADER_SIZE_LENGTH));
    }

    return TagHeader_new(major_version, minor_version, flags, tag_size, extended_header_size);
}

void ID3v2_TagHeader_free(ID3v2_TagHeader* header)
{
    free(header);
}
