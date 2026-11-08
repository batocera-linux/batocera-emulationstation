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
#include "modules/frame_header.private.h"
#include "modules/frame_ids.h"
#include "modules/frames/apic_frame.private.h"
#include "modules/frames/comment_frame.private.h"
#include "modules/frames/text_frame.private.h"
#include "modules/utils.private.h"

#include "frame.private.h"

ID3v2_Frame* Frame_parse(CharStream* frame_cs, int id3_major_version)
{
    ID3v2_FrameHeader* header = FrameHeader_parse(frame_cs, id3_major_version);

    if (header == NULL) return NULL; // no valid frame found

    if (FrameHeader_isTextFrame(header))
    {
        // Rewind the stream back so the frame can correctly parse the header
        CharStream_seek(frame_cs, -ID3v2_FRAME_HEADER_LENGTH, SEEK_CUR);
        FrameHeader_free(header); // we don't need it, the frame will parse its own header
        return (ID3v2_Frame*) TextFrame_parse(frame_cs, id3_major_version);
    }
    else if (FrameHeader_isCommentFrame(header))
    {
        CharStream_seek(frame_cs, -ID3v2_FRAME_HEADER_LENGTH, SEEK_CUR);
        FrameHeader_free(header);
        return (ID3v2_Frame*) CommentFrame_parse(frame_cs, id3_major_version);
    }
    else if (FrameHeader_isApicFrame(header))
    {
        CharStream_seek(frame_cs, -ID3v2_FRAME_HEADER_LENGTH, SEEK_CUR);
        FrameHeader_free(header);
        return (ID3v2_Frame*) ApicFrame_parse(frame_cs, id3_major_version);
    }

    // Unknown frame type, simply copy the raw data into the data property
    ID3v2_Frame* frame = (ID3v2_Frame*) malloc(sizeof(ID3v2_Frame));
    frame->header = header;
    frame->data = (char*) malloc(header->size * sizeof(char));
    CharStream_read(frame_cs, frame->data, header->size);

    return frame;
}

CharStream* Frame_to_char_stream(ID3v2_Frame* frame)
{
    if (frame == NULL) return NULL;

    if (FrameHeader_isTextFrame(frame->header))
    {
        return TextFrame_to_char_stream((ID3v2_TextFrame*) frame);
    }
    else if (FrameHeader_isCommentFrame(frame->header))
    {
        return CommentFrame_to_char_stream((ID3v2_CommentFrame*) frame);
    }
    else if (FrameHeader_isApicFrame(frame->header))
    {
        return ApicFrame_to_char_stream((ID3v2_ApicFrame*) frame);
    }
    else
    {
        // Unknown frame type, dump whatever we have in memory for that frame
        CharStream* frame_cs = CharStream_new(frame->header->size + ID3v2_FRAME_HEADER_LENGTH);
        CharStream* header_cs = FrameHeader_to_char_stream(frame->header);
        CharStream_write(frame_cs, header_cs->stream, ID3v2_FRAME_HEADER_LENGTH);
        CharStream_write(frame_cs, frame->data, frame->header->size);
        return frame_cs;
    }
}

void ID3v2_Frame_free(ID3v2_Frame* frame)
{
    if (frame == NULL) return;

    if (FrameHeader_isTextFrame(frame->header))
    {
        TextFrame_free((ID3v2_TextFrame*) frame);
    }
    else if (FrameHeader_isCommentFrame(frame->header))
    {
        CommentFrame_free((ID3v2_CommentFrame*) frame);
    }
    else if (FrameHeader_isApicFrame(frame->header))
    {
        ApicFrame_free((ID3v2_ApicFrame*) frame);
    }
    else
    {
        // Unknown frame id, naively try our best to free it
        free(frame->header);
        free(frame->data);
    }
}
