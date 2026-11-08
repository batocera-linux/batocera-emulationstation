/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_id3v2lib_compat_h
#define id3v2lib_id3v2lib_compat_h

#ifdef __cplusplus
extern "C"
{
#endif

#include "id3v2lib.h"

    // Compat types
    typedef struct
    {
        char tag[ID3v2_TAG_HEADER_IDENTIFIER_LENGTH];
        char major_version;
        char minor_version;
        char flags;
        int tag_size;
        int extended_header_size;
    } ID3v2_header;

    typedef struct
    {
        int size;
        char encoding;
        char* data;
    } ID3v2_frame_text_content;

    typedef struct
    {
        char* language;
        char* short_description;
        ID3v2_frame_text_content* text;
    } ID3v2_frame_comment_content;

    typedef struct
    {
        char encoding;
        char* mime_type;
        char picture_type;
        char* description;
        int picture_size;
        char* data;
    } ID3v2_frame_apic_content;

    typedef struct
    {
        char frame_id[ID3v2_FRAME_HEADER_ID_LENGTH];
        int size;
        char flags[ID3v2_FRAME_HEADER_FLAGS_LENGTH];
        char* data;
        ID3v2_Frame* frame;
    } ID3v2_frame;

    typedef struct _ID3v2_frame_list
    {
        ID3v2_frame* frame;
        struct _ID3v2_frame_list* start;
        struct _ID3v2_frame_list* last;
        struct _ID3v2_frame_list* next;
    } ID3v2_frame_list;

    typedef struct
    {
        char* raw;
        ID3v2_header* tag_header;
        ID3v2_frame_list* frames;
        ID3v2_Tag* tag;
    } ID3v2_tag;

    // Compat functions
    ID3v2_tag* new_tag();
    ID3v2_tag* load_tag(const char* file_name);
    ID3v2_tag* load_tag_with_buffer(const char* buffer, const int length);
    void remove_tag(const char* file_name);
    void set_tag(const char* file_name, ID3v2_tag* tag);
    void free_tag(ID3v2_tag* tag);
    void free_frame(ID3v2_frame* frame);

    // Getter functions
    ID3v2_frame* tag_get_title(ID3v2_tag* tag);
    ID3v2_frame* tag_get_artist(ID3v2_tag* tag);
    ID3v2_frame* tag_get_album(ID3v2_tag* tag);
    ID3v2_frame* tag_get_album_artist(ID3v2_tag* tag);
    ID3v2_frame* tag_get_genre(ID3v2_tag* tag);
    ID3v2_frame* tag_get_track(ID3v2_tag* tag);
    ID3v2_frame* tag_get_year(ID3v2_tag* tag);
    ID3v2_frame* tag_get_comment(ID3v2_tag* tag);
    ID3v2_frame* tag_get_disc_number(ID3v2_tag* tag);
    ID3v2_frame* tag_get_composer(ID3v2_tag* tag);
    ID3v2_frame* tag_get_album_cover(ID3v2_tag* tag);

    ID3v2_frame_text_content* parse_text_frame_content(ID3v2_frame* frame);
    ID3v2_frame_comment_content* parse_comment_frame_content(ID3v2_frame* frame);
    ID3v2_frame_apic_content* parse_apic_frame_content(ID3v2_frame* frame);

    // Setter functions
    void tag_set_title(char* title, char encoding, ID3v2_tag* tag);
    void tag_set_artist(char* artist, char encoding, ID3v2_tag* tag);
    void tag_set_album(char* album, char encoding, ID3v2_tag* tag);
    void tag_set_album_artist(char* album_artist, char encoding, ID3v2_tag* tag);
    void tag_set_genre(char* genre, char encoding, ID3v2_tag* tag);
    void tag_set_track(char* track, char encoding, ID3v2_tag* tag);
    void tag_set_year(char* year, char encoding, ID3v2_tag* tag);
    void tag_set_comment(char* comment, char encoding, ID3v2_tag* tag);
    void tag_set_disc_number(char* disc_number, char encoding, ID3v2_tag* tag);
    void tag_set_composer(char* composer, char encoding, ID3v2_tag* tag);
    void tag_set_album_cover(const char* filename, ID3v2_tag* tag);
    void tag_set_album_cover_from_bytes(
        char* album_cover_bytes,
        char* mimetype,
        int picture_size,
        ID3v2_tag* tag
    );

#ifdef __cplusplus
} // extern "C"
#endif

#endif
