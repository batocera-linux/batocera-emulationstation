/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "modules/utils.private.h"

#include "char_stream.private.h"

CharStream* CharStream_new(const int size)
{
    CharStream* cs = (CharStream*) malloc(sizeof(CharStream));
    cs->stream = (char*) calloc(size, sizeof(char));
    cs->cursor = 0;
    cs->size = size;
    return cs;
}

CharStream* CharStream_from_buffer(const char* buffer, const int size)
{
    CharStream* cs = (CharStream*) malloc(sizeof(CharStream));
    cs->stream = (char*) calloc(size, sizeof(char));
    memcpy(cs->stream, buffer, size);
    cs->cursor = 0;
    cs->size = size;
    return cs;
}

char* CharStream_get_cur(CharStream* cs)
{
    return cs->stream + cs->cursor;
}

void CharStream_write(CharStream* cs, const char* data, const int size)
{
    memcpy(CharStream_get_cur(cs), data, size);
    cs->cursor += size;
}

int CharStream_read(CharStream* cs, char* dest, const int size)
{
    const int final_size = clamp_int(size, 0, cs->size - cs->cursor);
    memcpy(dest, CharStream_get_cur(cs), final_size);
    cs->cursor += final_size;
    return final_size;
}

int CharStream_getc(CharStream* cs)
{
    if (cs->cursor >= cs->size) return EOF;
    int c = cs->stream[cs->cursor];
    cs->cursor++;
    return c;
}

char* CharStream_slice(CharStream* cs, const int n)
{
    char* result = CharStream_get_cur(cs);
    cs->cursor += n;
    return result;
}

void CharStream_seek(CharStream* cs, const int offset, const int whence)
{
    switch (whence)
    {
        case SEEK_SET:
            cs->cursor = offset;
            break;
        case SEEK_CUR:
            cs->cursor = clamp_int(cs->cursor + offset, 0, cs->size);
            break;
        default:
            cs->cursor = clamp_int(cs->size + offset, 0, cs->size);
            break;
    }
}

void CharStream_free(CharStream* cs)
{
    free(cs->stream);
    free(cs);
}
