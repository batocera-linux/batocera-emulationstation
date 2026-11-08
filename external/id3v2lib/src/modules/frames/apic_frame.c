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
#include "modules/frame_ids.h"
#include "modules/picture_types.h"
#include "modules/utils.private.h"

#include "apic_frame.private.h"

ID3v2_ApicFrame* ApicFrame_new(
    const char* flags,
    const char* description,
    const char picture_type,
    const char* mime_type,
    const int picture_size,
    const char* data
)
{
    ID3v2_ApicFrame* frame = (ID3v2_ApicFrame*) malloc(sizeof(ID3v2_ApicFrame));

    frame->data = ApicFrameData_new(description, picture_type, mime_type, picture_size, data);

    const int description_size = ID3v2_strlent(description);
    const int mime_type_size = ID3v2_strlent(mime_type);
    const int frame_size = ID3v2_FRAME_ENCODING_LENGTH + mime_type_size +
                           ID3v2_APIC_FRAME_PICTURE_TYPE_LENGTH + description_size +
                           frame->data->picture_size;

    frame->header = FrameHeader_new(ID3v2_ALBUM_COVER_FRAME_ID, flags, frame_size);

    return frame;
}

ID3v2_ApicFrame* ApicFrame_parse(CharStream* frame_cs, const int id3_major_version)
{
    ID3v2_FrameHeader* header = FrameHeader_parse(frame_cs, id3_major_version);

    CharStream_seek(frame_cs, ID3v2_FRAME_ENCODING_LENGTH, SEEK_CUR); // skip encoding

    const int mime_type_size = ID3v2_strlent(CharStream_get_cur(frame_cs));
    char* mime_type = (char*)calloc(mime_type_size, sizeof(char));
    CharStream_read(frame_cs, mime_type, mime_type_size);

    const char picture_type = CharStream_getc(frame_cs);

    const int description_size = ID3v2_strlent(CharStream_get_cur(frame_cs));
    char* description = (char*)calloc(description_size, sizeof(char));
    CharStream_read(frame_cs, description, description_size);

    const int pic_size = header->size - ID3v2_FRAME_ENCODING_LENGTH - mime_type_size -
                         ID3v2_APIC_FRAME_PICTURE_TYPE_LENGTH - description_size;
    
    char* pic_data = (char*)calloc(pic_size, sizeof(char));
    CharStream_read(frame_cs, pic_data, pic_size);

    ID3v2_ApicFrame* frame =
        ApicFrame_new(header->flags, description, picture_type, mime_type, pic_size, pic_data);

    FrameHeader_free(header); // we only needed the header to parse the data

    free(mime_type);
    free(description);
    free(pic_data);

    return frame;
}

CharStream* ApicFrame_to_char_stream(ID3v2_ApicFrame* frame)
{
    if (frame == NULL) return NULL;

    CharStream* frame_header_cs = FrameHeader_to_char_stream(frame->header);
    CharStream* frame_cs = CharStream_new(frame->header->size + ID3v2_FRAME_HEADER_LENGTH);

    // Header
    CharStream_write(frame_cs, frame_header_cs->stream, frame_header_cs->size);
    CharStream_free(frame_header_cs);

    // Data
    CharStream_write(frame_cs, &frame->data->encoding, ID3v2_FRAME_ENCODING_LENGTH);
    CharStream_write(frame_cs, frame->data->mime_type, ID3v2_strlent(frame->data->mime_type));
    CharStream_write(frame_cs, &frame->data->picture_type, ID3v2_APIC_FRAME_PICTURE_TYPE_LENGTH);
    CharStream_write(frame_cs, frame->data->description, ID3v2_strlent(frame->data->description));
    CharStream_write(frame_cs, frame->data->data, frame->data->picture_size);

    return frame_cs;
}

void ApicFrame_free(ID3v2_ApicFrame* frame)
{
    FrameHeader_free(frame->header);
    free(frame->data->data);
    free(frame->data->description);
    free(frame->data->mime_type);
    free(frame->data);
    free(frame);
}

ID3v2_ApicFrameData* ApicFrameData_new(
    const char* description,
    const char picture_type,
    const char* mime_type,
    const int picture_size,
    const char* picture_data
)
{
    ID3v2_ApicFrameData* data = (ID3v2_ApicFrameData*) malloc(sizeof(ID3v2_ApicFrameData));

    const char encoding = string_has_bom(description) ? ID3v2_ENCODING_UNICODE : ID3v2_ENCODING_ISO;
    data->encoding = encoding;

    const int desc_size = ID3v2_strlent(description);
    data->description = (char*) malloc(desc_size * sizeof(char));
    memcpy(data->description, description, desc_size);

    const int mime_type_size = ID3v2_strlent(mime_type);
    data->mime_type = (char*) malloc(mime_type_size * sizeof(char));
    memcpy(data->mime_type, mime_type, mime_type_size);

    data->picture_type = picture_type;

    data->picture_size = picture_size;

    data->data = (char*) malloc(picture_size * sizeof(char));
    memcpy(data->data, picture_data, picture_size);

    return data;
}