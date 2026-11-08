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

#include "modules/char_stream.private.h"
#include "modules/frame.private.h"
#include "modules/frame_header.private.h"
#include "modules/utils.private.h"

#include "text_frame.private.h"

ID3v2_TextFrame* TextFrame_new(const char* id, const char* flags, const char* text)
{
    ID3v2_TextFrame* frame = (ID3v2_TextFrame*) malloc(sizeof(ID3v2_TextFrame));

    frame->data = TextFrameData_new(text);

    const int frame_size = ID3v2_FRAME_ENCODING_LENGTH + frame->data->size;
    frame->header = FrameHeader_new(id, flags, frame_size);

    return frame;
}

ID3v2_TextFrame* TextFrame_parse(CharStream* frame_cs, const int id3_major_version)
{
    ID3v2_FrameHeader* header = FrameHeader_parse(frame_cs, id3_major_version);

    CharStream_seek(frame_cs, ID3v2_FRAME_ENCODING_LENGTH, SEEK_CUR); // skip encoding

    const int text_size = header->size - ID3v2_FRAME_ENCODING_LENGTH;
    char* text = (char*)calloc(text_size, sizeof(char));
    CharStream_read(frame_cs, text, text_size);

    ID3v2_TextFrame* frame = TextFrame_new(header->id, header->flags, text);

    FrameHeader_free(header); // we only needed the header to parse the data
    free(text);

    return frame;
}

CharStream* TextFrame_to_char_stream(ID3v2_TextFrame* frame)
{
    if (frame == NULL) return NULL;

    CharStream* frame_header_cs = FrameHeader_to_char_stream(frame->header);
    CharStream* frame_cs = CharStream_new(frame->header->size + ID3v2_FRAME_HEADER_LENGTH);

    // Header
    CharStream_write(frame_cs, frame_header_cs->stream, frame_header_cs->size);
    CharStream_free(frame_header_cs);

    // Data
    CharStream_write(frame_cs, &frame->data->encoding, ID3v2_FRAME_ENCODING_LENGTH);
    CharStream_write(frame_cs, frame->data->text, frame->data->size);

    return frame_cs;
}

void TextFrame_free(ID3v2_TextFrame* frame)
{
    FrameHeader_free(frame->header);
    free(frame->data->text);
    free(frame->data);
    free(frame);
}

ID3v2_TextFrameData* TextFrameData_new(const char* text)
{
    const char encoding = string_has_bom(text) ? ID3v2_ENCODING_UNICODE : ID3v2_ENCODING_ISO;
    const int str_termination_size = encoding == ID3v2_ENCODING_ISO ? 1 : 2;
    const int size = ID3v2_strlen(text) + str_termination_size;

    ID3v2_TextFrameData* data = (ID3v2_TextFrameData*) malloc(sizeof(ID3v2_TextFrameData));
    data->text = (char*) malloc(size * sizeof(char));

    data->encoding = encoding;
    data->size = size;
    memcpy(data->text, text, size);

    return data;
}
