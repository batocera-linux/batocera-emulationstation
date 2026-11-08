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

#include "../char_stream.private.h"
#include "../frame.private.h"
#include "../frame_header.private.h"
#include "../utils.private.h"
#include "modules/frame_ids.h"

#include "comment_frame.private.h"

ID3v2_CommentFrame* CommentFrame_new(
    const char* flags,
    const char* lang,
    const char* short_desc,
    const char* comment
)
{
    ID3v2_CommentFrame* frame = (ID3v2_CommentFrame*) malloc(sizeof(ID3v2_CommentFrame));

    frame->data = CommentFrameData_new(lang, short_desc, comment);

    const int short_desc_size = ID3v2_strlent(short_desc);
    const int frame_size = frame->data->size + short_desc_size + ID3v2_FRAME_ENCODING_LENGTH +
                           ID3v2_COMMENT_FRAME_LANGUAGE_LENGTH;

    frame->header = FrameHeader_new(ID3v2_COMMENT_FRAME_ID, flags, frame_size);

    return frame;
}

ID3v2_CommentFrame* CommentFrame_parse(CharStream* frame_cs, const int id3_major_version)
{
    ID3v2_FrameHeader* header = FrameHeader_parse(frame_cs, id3_major_version);

    CharStream_seek(frame_cs, ID3v2_FRAME_ENCODING_LENGTH, SEEK_CUR); // skip encoding

    char lang[ID3v2_COMMENT_FRAME_LANGUAGE_LENGTH];
    CharStream_read(frame_cs, lang, ID3v2_COMMENT_FRAME_LANGUAGE_LENGTH);

    const int short_desc_size = ID3v2_strlent(CharStream_get_cur(frame_cs));
    char* short_desc = (char*)calloc(short_desc_size, sizeof(char));
    CharStream_read(frame_cs, short_desc, short_desc_size);

    const int comment_size = ID3v2_strlent(CharStream_get_cur(frame_cs));
    char* comment = (char*)calloc(comment_size, sizeof(char));
    CharStream_read(frame_cs, comment, comment_size);

    ID3v2_CommentFrame* frame = CommentFrame_new(header->flags, lang, short_desc, comment);

    FrameHeader_free(header); // we only needed the header to parse the data

    free(short_desc);
    free(comment);

    return frame;
}

CharStream* CommentFrame_to_char_stream(ID3v2_CommentFrame* frame)
{
    if (frame == NULL) return NULL;

    CharStream* frame_header_cs = FrameHeader_to_char_stream(frame->header);
    CharStream* frame_cs = CharStream_new(frame->header->size + ID3v2_FRAME_HEADER_LENGTH);

    // Header
    CharStream_write(frame_cs, frame_header_cs->stream, frame_header_cs->size);
    CharStream_free(frame_header_cs);

    // Data
    CharStream_write(frame_cs, &frame->data->encoding, ID3v2_FRAME_ENCODING_LENGTH);
    CharStream_write(frame_cs, frame->data->language, ID3v2_COMMENT_FRAME_LANGUAGE_LENGTH);
    CharStream_write(
        frame_cs,
        frame->data->short_description,
        ID3v2_strlent(frame->data->short_description)
    );
    CharStream_write(frame_cs, frame->data->comment, frame->data->size);

    return frame_cs;
}

void CommentFrame_free(ID3v2_CommentFrame* frame)
{
    FrameHeader_free(frame->header);
    free(frame->data->comment);
    free(frame->data->short_description);
    free(frame->data);
    free(frame);
}

ID3v2_CommentFrameData* CommentFrameData_new(
    const char* lang,
    const char* short_desc,
    const char* comment
)
{
    const char encoding = string_has_bom(comment) ? ID3v2_ENCODING_UNICODE : ID3v2_ENCODING_ISO;
    const int comment_size = ID3v2_strlent(comment);
    const int short_desc_size = ID3v2_strlent(short_desc);

    ID3v2_CommentFrameData* data = (ID3v2_CommentFrameData*) malloc(sizeof(ID3v2_CommentFrameData));

    data->encoding = encoding;

    data->size = comment_size;

    memcpy(data->language, lang, ID3v2_COMMENT_FRAME_LANGUAGE_LENGTH);

    data->comment = (char*) malloc(comment_size * sizeof(char));
    memcpy(data->comment, comment, comment_size);

    data->short_description = (char*) malloc(short_desc_size * sizeof(char));
    memcpy(data->short_description, short_desc, short_desc_size);

    return data;
}
