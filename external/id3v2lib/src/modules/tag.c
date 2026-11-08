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

#include "frames/apic_frame.private.h"
#include "frames/comment_frame.private.h"
#include "frames/text_frame.private.h"
#include "modules/char_stream.private.h"
#include "modules/frame.private.h"
#include "modules/frame_header.private.h"
#include "modules/frame_ids.h"
#include "modules/frame_list.private.h"
#include "modules/picture_types.h"
#include "modules/tag_header.private.h"
#include "modules/utils.private.h"

#include "tag.private.h"

ID3v2_Tag* ID3v2_Tag_new(ID3v2_TagHeader* header, const int padding_size)
{
    ID3v2_Tag* tag = (ID3v2_Tag*) malloc(sizeof(ID3v2_Tag));
    tag->header = header == NULL ? TagHeader_new_empty() : header;
    tag->frames = FrameList_new();
    tag->padding_size = 0;

    return tag;
}

ID3v2_Tag* ID3v2_Tag_new_empty()
{
    return ID3v2_Tag_new(NULL, 0);
}

ID3v2_Tag* Tag_parse(CharStream* tag_cs)
{
    ID3v2_TagHeader* header = TagHeader_parse(tag_cs);
    if (header == NULL) return NULL;

    ID3v2_Tag* tag = ID3v2_Tag_new(header, 0);

    if (header->extended_header_size > 0)
    {
        // An extended header exists, skip it
        CharStream_seek(tag_cs, header->extended_header_size, SEEK_SET);
    }

    ID3v2_Frame* current_frame;
    while (tag_cs->cursor < tag->header->tag_size)
    {
        current_frame = Frame_parse(tag_cs, tag->header->major_version);
        if (current_frame == NULL) break;
        FrameList_add_frame(tag->frames, current_frame);
    }

    tag->padding_size = tag_cs->size - tag_cs->cursor;

    return tag;
}

CharStream* Tag_to_char_stream(ID3v2_Tag* tag)
{
    CharStream* tag_cs = CharStream_new(tag->header->tag_size + ID3v2_TAG_HEADER_LENGTH);

    // Write header
    CharStream_write(tag_cs, tag->header->identifier, ID3v2_TAG_HEADER_IDENTIFIER_LENGTH);
    CharStream_write(tag_cs, &tag->header->major_version, ID3v2_TAG_HEADER_MAJOR_VERSION_LENGTH);
    CharStream_write(tag_cs, &tag->header->minor_version, ID3v2_TAG_HEADER_MINOR_VERSION_LENGTH);
    CharStream_write(tag_cs, &tag->header->flags, ID3v2_TAG_HEADER_FLAGS_LENGTH);

    char* size_bytes = itob(syncint_encode(tag->header->tag_size));
    CharStream_write(tag_cs, size_bytes, ID3v2_TAG_HEADER_TAG_SIZE_LENGTH);
    free(size_bytes);

    // Write frames
    ID3v2_FrameList* frames = tag->frames;

    if (frames->frame == NULL)
    {
        // The tag doesn't have any frames
        return tag_cs;
    }

    while (frames != NULL)
    {
        CharStream* frame_cs = Frame_to_char_stream(frames->frame);

        if (frame_cs == NULL) exit(1);

        CharStream_write(tag_cs, frame_cs->stream, frame_cs->size);
        CharStream_free(frame_cs);

        frames = frames->next;
    }

    return tag_cs;
}

/**
 * Getter functions
 */
ID3v2_Frame* ID3v2_Tag_get_frame(ID3v2_Tag* tag, const char* frame_id)
{
    if (tag == NULL) return NULL;
    return FrameList_get_frame_by_id(tag->frames, frame_id);
}

ID3v2_FrameList* ID3v2_Tag_get_frames(ID3v2_Tag* tag, const char* frame_id)
{
    if (tag == NULL) return NULL;
    return FrameList_get_frames_by_id(tag->frames, frame_id);
}

ID3v2_TextFrame* ID3v2_Tag_get_artist_frame(ID3v2_Tag* tag)
{
    return (ID3v2_TextFrame*) ID3v2_Tag_get_frame(tag, ID3v2_ARTIST_FRAME_ID);
}

ID3v2_TextFrame* ID3v2_Tag_get_album_frame(ID3v2_Tag* tag)
{
    return (ID3v2_TextFrame*) ID3v2_Tag_get_frame(tag, ID3v2_ALBUM_FRAME_ID);
}

ID3v2_TextFrame* ID3v2_Tag_get_title_frame(ID3v2_Tag* tag)
{
    return (ID3v2_TextFrame*) ID3v2_Tag_get_frame(tag, ID3v2_TITLE_FRAME_ID);
}

ID3v2_TextFrame* ID3v2_Tag_get_track_frame(ID3v2_Tag* tag)
{
    return (ID3v2_TextFrame*) ID3v2_Tag_get_frame(tag, ID3v2_TRACK_FRAME_ID);
}

ID3v2_TextFrame* ID3v2_Tag_get_album_artist_frame(ID3v2_Tag* tag)
{
    return (ID3v2_TextFrame*) ID3v2_Tag_get_frame(tag, ID3v2_ALBUM_ARTIST_FRAME_ID);
}

ID3v2_TextFrame* ID3v2_Tag_get_genre_frame(ID3v2_Tag* tag)
{
    return (ID3v2_TextFrame*) ID3v2_Tag_get_frame(tag, ID3v2_GENRE_FRAME_ID);
}

ID3v2_TextFrame* ID3v2_Tag_get_year_frame(ID3v2_Tag* tag)
{
    return (ID3v2_TextFrame*) ID3v2_Tag_get_frame(tag, ID3v2_YEAR_FRAME_ID);
}

ID3v2_TextFrame* ID3v2_Tag_get_disc_number_frame(ID3v2_Tag* tag)
{
    return (ID3v2_TextFrame*) ID3v2_Tag_get_frame(tag, ID3v2_DISC_NUMBER_FRAME_ID);
}

ID3v2_TextFrame* ID3v2_Tag_get_composer_frame(ID3v2_Tag* tag)
{
    return (ID3v2_TextFrame*) ID3v2_Tag_get_frame(tag, ID3v2_COMPOSER_FRAME_ID);
}

/**
 * Since the ID3v2 spec allows defining multiple COMM frames in a
 * single tag, this gets the first COMM frame found. To retrieve every
 * COMM frame found use ID3v2_Tag_get_comment_frames(tag) instead.
 */
ID3v2_CommentFrame* ID3v2_Tag_get_comment_frame(ID3v2_Tag* tag)
{
    return (ID3v2_CommentFrame*) ID3v2_Tag_get_frame(tag, ID3v2_COMMENT_FRAME_ID);
}

/**
 * Since the ID3v2 spec allows defining multiple COMM frames in a
 * single tag, this gets every COMM frame found. To retrieve only the
 * first COMM frame found use ID3v2_Tag_get_comment_frame(tag) instead.
 */
ID3v2_FrameList* ID3v2_Tag_get_comment_frames(ID3v2_Tag* tag)
{
    return ID3v2_Tag_get_frames(tag, ID3v2_COMMENT_FRAME_ID);
}

ID3v2_ApicFrame* ID3v2_Tag_get_album_cover_frame(ID3v2_Tag* tag)
{
    return (ID3v2_ApicFrame*) ID3v2_Tag_get_frame(tag, ID3v2_ALBUM_COVER_FRAME_ID);
}

ID3v2_FrameList* ID3v2_Tag_get_apic_frames(ID3v2_Tag* tag)
{
    return ID3v2_Tag_get_frames(tag, ID3v2_ALBUM_COVER_FRAME_ID);
}

/**
 * Setter functions
 */
void ID3v2_Tag_set_text_frame(ID3v2_Tag* tag, ID3v2_TextFrameInput* input)
{
    ID3v2_TextFrame* new_frame = TextFrame_new(input->id, input->flags, input->text);
    ID3v2_TextFrame* existing_frame =
        (ID3v2_TextFrame*) FrameList_get_frame_by_id(tag->frames, input->id);

    if (existing_frame == NULL)
    {
        FrameList_add_frame(tag->frames, (ID3v2_Frame*) new_frame);
        tag->header->tag_size += new_frame->header->size;
    }
    else
    {
        FrameList_replace_frame(
            tag->frames,
            (ID3v2_Frame*) existing_frame,
            (ID3v2_Frame*) new_frame
        );
        tag->header->tag_size += (new_frame->header->size - existing_frame->header->size);
        ID3v2_Frame_free((ID3v2_Frame*) existing_frame);
    }
}

void ID3v2_Tag_set_artist(ID3v2_Tag* tag, const char* artist)
{
    ID3v2_Tag_set_text_frame(
        tag,
        &(ID3v2_TextFrameInput){
            .id = ID3v2_ARTIST_FRAME_ID,
            .flags = "\0\0",
            .text = artist,
        }
    );
}

void ID3v2_Tag_set_album(ID3v2_Tag* tag, const char* album)
{
    ID3v2_Tag_set_text_frame(
        tag,
        &(ID3v2_TextFrameInput){
            .id = ID3v2_ALBUM_FRAME_ID,
            .flags = "\0\0",
            .text = album,
        }
    );
}

void ID3v2_Tag_set_title(ID3v2_Tag* tag, const char* title)
{
    ID3v2_Tag_set_text_frame(
        tag,
        &(ID3v2_TextFrameInput){
            .id = ID3v2_TITLE_FRAME_ID,
            .flags = "\0\0",
            .text = title,
        }
    );
}

void ID3v2_Tag_set_track(ID3v2_Tag* tag, const char* track)
{
    ID3v2_Tag_set_text_frame(
        tag,
        &(ID3v2_TextFrameInput){
            .id = ID3v2_TRACK_FRAME_ID,
            .flags = "\0\0",
            .text = track,
        }
    );
}

void ID3v2_Tag_set_album_artist(ID3v2_Tag* tag, const char* album_artist)
{
    ID3v2_Tag_set_text_frame(
        tag,
        &(ID3v2_TextFrameInput){
            .id = ID3v2_ALBUM_ARTIST_FRAME_ID,
            .flags = "\0\0",
            .text = album_artist,
        }
    );
}

void ID3v2_Tag_set_genre(ID3v2_Tag* tag, const char* genre)
{
    ID3v2_Tag_set_text_frame(
        tag,
        &(ID3v2_TextFrameInput){
            .id = ID3v2_GENRE_FRAME_ID,
            .flags = "\0\0",
            .text = genre,
        }
    );
}

void ID3v2_Tag_set_year(ID3v2_Tag* tag, const char* year)
{
    ID3v2_Tag_set_text_frame(
        tag,
        &(ID3v2_TextFrameInput){
            .id = ID3v2_YEAR_FRAME_ID,
            .flags = "\0\0",
            .text = year,
        }
    );
}

void ID3v2_Tag_set_disc_number(ID3v2_Tag* tag, const char* disc_number)
{
    ID3v2_Tag_set_text_frame(
        tag,
        &(ID3v2_TextFrameInput){
            .id = ID3v2_DISC_NUMBER_FRAME_ID,
            .flags = "\0\0",
            .text = disc_number,
        }
    );
}

void ID3v2_Tag_set_composer(ID3v2_Tag* tag, const char* composer)
{
    ID3v2_Tag_set_text_frame(
        tag,
        &(ID3v2_TextFrameInput){
            .id = ID3v2_COMPOSER_FRAME_ID,
            .flags = "\0\0",
            .text = composer,
        }
    );
}

/**
 * This only sets the first COMM frame found.
 */
void ID3v2_Tag_set_comment_frame(ID3v2_Tag* tag, ID3v2_CommentFrameInput* input)
{
    ID3v2_CommentFrame* new_frame =
        CommentFrame_new(input->flags, input->language, input->short_description, input->comment);
    ID3v2_CommentFrame* existing_frame = ID3v2_Tag_get_comment_frame(tag);

    if (existing_frame == NULL)
    {
        FrameList_add_frame(tag->frames, (ID3v2_Frame*) new_frame);
        tag->header->tag_size += new_frame->header->size;
    }
    else
    {
        FrameList_replace_frame(
            tag->frames,
            (ID3v2_Frame*) existing_frame,
            (ID3v2_Frame*) new_frame
        );
        tag->header->tag_size += (new_frame->header->size - existing_frame->header->size);
        ID3v2_Frame_free((ID3v2_Frame*) existing_frame);
    }
}

void ID3v2_Tag_add_comment_frame(ID3v2_Tag* tag, ID3v2_CommentFrameInput* input)
{
    ID3v2_CommentFrame* new_frame =
        CommentFrame_new(input->flags, input->language, input->short_description, input->comment);
    FrameList_add_frame(tag->frames, (ID3v2_Frame*) new_frame);
    tag->header->tag_size += new_frame->header->size;
}

void ID3v2_Tag_set_comment(ID3v2_Tag* tag, const char* lang, const char* comment)
{
    ID3v2_Tag_set_comment_frame(
        tag,
        &(ID3v2_CommentFrameInput){
            .flags = "\0\0",
            .language = lang,
            .short_description = EMPTY_UNICODE_STR,
            .comment = comment,
        }
    );
}

/**
 * This only sets the first APIC frame found.
 */
void ID3v2_Tag_set_apic_frame(ID3v2_Tag* tag, ID3v2_ApicFrameInput* input)
{
    ID3v2_ApicFrame* new_frame = ApicFrame_new(
        input->flags,
        input->description,
        input->picture_type,
        input->mime_type,
        input->picture_size,
        input->data
    );
    ID3v2_ApicFrame* existing_frame = ID3v2_Tag_get_album_cover_frame(tag);

    if (existing_frame == NULL)
    {
        FrameList_add_frame(tag->frames, (ID3v2_Frame*) new_frame);
        tag->header->tag_size += new_frame->header->size;
    }
    else
    {
        FrameList_replace_frame(
            tag->frames,
            (ID3v2_Frame*) existing_frame,
            (ID3v2_Frame*) new_frame
        );
        tag->header->tag_size += (new_frame->header->size - existing_frame->header->size);
        ID3v2_Frame_free((ID3v2_Frame*) existing_frame);
    }
}

void ID3v2_Tag_add_apic_frame(ID3v2_Tag* tag, ID3v2_ApicFrameInput* input)
{
    ID3v2_ApicFrame* new_frame = ApicFrame_new(
        input->flags,
        input->description,
        input->picture_type,
        input->mime_type,
        input->picture_size,
        input->data
    );
    FrameList_add_frame(tag->frames, (ID3v2_Frame*) new_frame);
    tag->header->tag_size += new_frame->header->size;
}

/**
 * This only sets the front album cover (picture_type = 0x03)
 */
void ID3v2_Tag_set_album_cover(
    ID3v2_Tag* tag,
    const char* mime_type,
    const int size,
    const char* data
)
{
    ID3v2_Tag_set_apic_frame(
        tag,
        &(ID3v2_ApicFrameInput){
            .flags = "\0\0",
            .mime_type = mime_type,
            .description = EMPTY_UNICODE_STR,
            .picture_size = size,
            .picture_type = ID3v2_PIC_TYPE_FRONT_COVER,
            .data = data,
        }
    );
}

void ID3v2_Tag_free(ID3v2_Tag* tag)
{
    ID3v2_TagHeader_free(tag->header);
    ID3v2_FrameList_free(tag->frames);
    free(tag);
}

void ID3v2_Tag_delete_frame(ID3v2_Tag* tag, const char* frame_id)
{
    ID3v2_Frame* deleted = FrameList_remove_frame_by_id(tag->frames, frame_id);
    tag->header->tag_size -= deleted->header->size + ID3v2_FRAME_HEADER_LENGTH;
    // Maybe we should just return the frame instead of taking the responsibility
    // of freeing it?
    ID3v2_Frame_free(deleted);
}

void ID3v2_Tag_delete_artist(ID3v2_Tag* tag)
{
    ID3v2_Tag_delete_frame(tag, ID3v2_ARTIST_FRAME_ID);
}

void ID3v2_Tag_delete_album(ID3v2_Tag* tag)
{
    ID3v2_Tag_delete_frame(tag, ID3v2_ALBUM_FRAME_ID);
}

void ID3v2_Tag_delete_title(ID3v2_Tag* tag)
{
    ID3v2_Tag_delete_frame(tag, ID3v2_TITLE_FRAME_ID);
}

void ID3v2_Tag_delete_track(ID3v2_Tag* tag)
{
    ID3v2_Tag_delete_frame(tag, ID3v2_TRACK_FRAME_ID);
}

void ID3v2_Tag_delete_album_artist(ID3v2_Tag* tag)
{
    ID3v2_Tag_delete_frame(tag, ID3v2_ALBUM_ARTIST_FRAME_ID);
}

void ID3v2_Tag_delete_genre(ID3v2_Tag* tag)
{
    ID3v2_Tag_delete_frame(tag, ID3v2_GENRE_FRAME_ID);
}

void ID3v2_Tag_delete_year(ID3v2_Tag* tag)
{
    ID3v2_Tag_delete_frame(tag, ID3v2_YEAR_FRAME_ID);
}

void ID3v2_Tag_delete_disc_number(ID3v2_Tag* tag)
{
    ID3v2_Tag_delete_frame(tag, ID3v2_DISC_NUMBER_FRAME_ID);
}

void ID3v2_Tag_delete_composer(ID3v2_Tag* tag)
{
    ID3v2_Tag_delete_frame(tag, ID3v2_COMPOSER_FRAME_ID);
}

void ID3v2_Tag_delete_comment_frame(ID3v2_Tag* tag, const int index)
{
    int i = 0;
    ID3v2_FrameList* head = ID3v2_Tag_get_comment_frames(tag);

    while (head != NULL)
    {
        ID3v2_CommentFrame* to_delete = (ID3v2_CommentFrame*) head->frame;

        if (i == index)
        {
            FrameList_remove_frame(tag->frames, (ID3v2_Frame*) to_delete);
            ID3v2_Frame_free((ID3v2_Frame*) to_delete);
            break;
        }

        i++;
        head = head->next;
    }

    ID3v2_FrameList_unlink(head);
}

void ID3v2_Tag_delete_comment(ID3v2_Tag* tag)
{
    ID3v2_Tag_delete_frame(tag, ID3v2_COMMENT_FRAME_ID);
}

void ID3v2_Tag_delete_apic_frame(ID3v2_Tag* tag, const int index)
{
    int i = 0;
    ID3v2_FrameList* head = ID3v2_Tag_get_apic_frames(tag);

    while (head != NULL)
    {
        ID3v2_ApicFrame* to_delete = (ID3v2_ApicFrame*) head->frame;

        if (i == index)
        {
            FrameList_remove_frame(tag->frames, (ID3v2_Frame*) to_delete);
            ID3v2_Frame_free((ID3v2_Frame*) to_delete);
            break;
        }

        i++;
        head = head->next;
    }

    ID3v2_FrameList_unlink(head);
}

void ID3v2_Tag_delete_album_cover(ID3v2_Tag* tag)
{
    ID3v2_Tag_delete_frame(tag, ID3v2_ALBUM_COVER_FRAME_ID);
}
