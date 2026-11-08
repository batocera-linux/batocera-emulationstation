/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_tag_h
#define id3v2lib_tag_h

#define ID3v2_TAG_DEFAULT_PADDING_LENGTH 2048

typedef struct _ID3v2_TagHeader ID3v2_TagHeader;
typedef struct _ID3v2_FrameList ID3v2_FrameList;
typedef struct _ID3v2_TextFrame ID3v2_TextFrame;
typedef struct _ID3v2_CommentFrame ID3v2_CommentFrame;
typedef struct _ID3v2_ApicFrame ID3v2_ApicFrame;

typedef struct _ID3v2_Tag
{
    ID3v2_TagHeader* header;
    ID3v2_FrameList* frames;
    int padding_size;
} ID3v2_Tag;

ID3v2_Tag* ID3v2_Tag_new(ID3v2_TagHeader* header, const int padding_size);
ID3v2_Tag* ID3v2_Tag_new_empty();
void ID3v2_Tag_free(ID3v2_Tag* tag);

/**
 * Getter functions
 */

/**
 * Returns the first frame found with the provided id. If you need
 * more than the first occurrence, then, use ID3v2_Tag_get_frames(tag, frame_id);
 */
ID3v2_Frame* ID3v2_Tag_get_frame(ID3v2_Tag* tag, const char* frame_id);

/**
 * Returns all the frames matching frame_id in a sublist. If you only need
 * the first occurrence, then, use ID3v2_Tag_get_frame(tag, frame_id);
 */
ID3v2_FrameList* ID3v2_Tag_get_frames(ID3v2_Tag* tag, const char* frame_id);

ID3v2_TextFrame* ID3v2_Tag_get_artist_frame(ID3v2_Tag* tag);
ID3v2_TextFrame* ID3v2_Tag_get_album_frame(ID3v2_Tag* tag);
ID3v2_TextFrame* ID3v2_Tag_get_title_frame(ID3v2_Tag* tag);
ID3v2_TextFrame* ID3v2_Tag_get_track_frame(ID3v2_Tag* tag);
ID3v2_TextFrame* ID3v2_Tag_get_album_artist_frame(ID3v2_Tag* tag);
ID3v2_TextFrame* ID3v2_Tag_get_genre_frame(ID3v2_Tag* tag);
ID3v2_TextFrame* ID3v2_Tag_get_year_frame(ID3v2_Tag* tag);
ID3v2_TextFrame* ID3v2_Tag_get_disc_number_frame(ID3v2_Tag* tag);
ID3v2_TextFrame* ID3v2_Tag_get_composer_frame(ID3v2_Tag* tag);
ID3v2_CommentFrame* ID3v2_Tag_get_comment_frame(ID3v2_Tag* tag);
ID3v2_FrameList* ID3v2_Tag_get_comment_frames(ID3v2_Tag* tag);
ID3v2_ApicFrame* ID3v2_Tag_get_album_cover_frame(ID3v2_Tag* tag);
ID3v2_FrameList* ID3v2_Tag_get_apic_frames(ID3v2_Tag* tag);

/**
 * Setter functions
 */
typedef struct _ID3v2_TextFrameInput
{
    const char* id;
    const char* flags;
    const char* text;
} ID3v2_TextFrameInput;

void ID3v2_Tag_set_text_frame(ID3v2_Tag* tag, ID3v2_TextFrameInput* input);

void ID3v2_Tag_set_artist(ID3v2_Tag* tag, const char* artist);
void ID3v2_Tag_set_album(ID3v2_Tag* tag, const char* album);
void ID3v2_Tag_set_title(ID3v2_Tag* tag, const char* title);
void ID3v2_Tag_set_track(ID3v2_Tag* tag, const char* track);
void ID3v2_Tag_set_album_artist(ID3v2_Tag* tag, const char* album_artist);
void ID3v2_Tag_set_genre(ID3v2_Tag* tag, const char* genre);
void ID3v2_Tag_set_year(ID3v2_Tag* tag, const char* year);
void ID3v2_Tag_set_disc_number(ID3v2_Tag* tag, const char* disc_number);
void ID3v2_Tag_set_composer(ID3v2_Tag* tag, const char* composer);

typedef struct _ID3v2_CommentFrameInput
{
    const char* flags;
    const char* language;
    const char* short_description;
    const char* comment;
} ID3v2_CommentFrameInput;

void ID3v2_Tag_set_comment_frame(ID3v2_Tag* tag, ID3v2_CommentFrameInput* input);
void ID3v2_Tag_add_comment_frame(ID3v2_Tag* tag, ID3v2_CommentFrameInput* input);

void ID3v2_Tag_set_comment(ID3v2_Tag* tag, const char* language, const char* comment);

typedef struct _ID3v2_ApicFrameInput
{
    const char* flags;
    const char* mime_type;
    const char* description;
    const char picture_type;
    const int picture_size;
    const char* data;
} ID3v2_ApicFrameInput;

void ID3v2_Tag_add_apic_frame(ID3v2_Tag* tag, ID3v2_ApicFrameInput* input);
void ID3v2_Tag_set_apic_frame(ID3v2_Tag* tag, ID3v2_ApicFrameInput* input);

void ID3v2_Tag_set_album_cover(
    ID3v2_Tag* tag,
    const char* mime_type,
    const int size,
    const char* data
);

/**
 * Delete functions
 */
void ID3v2_Tag_delete_frame(ID3v2_Tag* tag, const char* frame_id);

void ID3v2_Tag_delete_artist(ID3v2_Tag* tag);
void ID3v2_Tag_delete_album(ID3v2_Tag* tag);
void ID3v2_Tag_delete_title(ID3v2_Tag* tag);
void ID3v2_Tag_delete_track(ID3v2_Tag* tag);
void ID3v2_Tag_delete_album_artist(ID3v2_Tag* tag);
void ID3v2_Tag_delete_genre(ID3v2_Tag* tag);
void ID3v2_Tag_delete_year(ID3v2_Tag* tag);
void ID3v2_Tag_delete_disc_number(ID3v2_Tag* tag);
void ID3v2_Tag_delete_composer(ID3v2_Tag* tag);

void ID3v2_Tag_delete_comment_frame(ID3v2_Tag* tag, const int index);
void ID3v2_Tag_delete_comment(ID3v2_Tag* tag);

void ID3v2_Tag_delete_apic_frame(ID3v2_Tag* tag, const int index);
void ID3v2_Tag_delete_album_cover(ID3v2_Tag* tag);

#endif
