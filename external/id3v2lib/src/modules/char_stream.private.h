/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_char_stream_private_h
#define id3v2lib_char_stream_private_h

typedef struct _CharStream
{
    int cursor;
    int size;
    char* stream;
} CharStream;

CharStream* CharStream_new(const int size);
CharStream* CharStream_from_buffer(const char* buffer, const int size);

char* CharStream_get_cur(CharStream* cs);
void CharStream_write(CharStream* cs, const char* data, const int size);
int CharStream_read(CharStream* cs, char* dest, const int size);
int CharStream_getc(CharStream* cs);
char* CharStream_slice(CharStream* cs, const int n);

void CharStream_seek(CharStream* cs, const int offset, const int whence);

void CharStream_free(CharStream* cs);

#endif
