/*
 * This file is part of the id3v2lib library
 *
 * Copyright (c) 2013, Lorenzo Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "id3v2lib.h"


ID3v2_tag* load_tag(const char* file_name)
{
    char *buffer;
    FILE *file;
    int header_size;
    ID3v2_tag *tag;

    // get header size
    ID3v2_header *tag_header = get_tag_header(file_name);
    if(tag_header == NULL) {
        return NULL;
    }
    header_size = tag_header->tag_size + 10;
    free(tag_header);

    // allocate buffer and fetch header
    file = fopen(file_name, "rb");
    if(file == NULL)
    {
        perror("Error opening file");
        return NULL;
    }
    buffer = (char*) malloc((10+header_size) * sizeof(char));
    if(buffer == NULL) {
        perror("Could not allocate buffer");
        fclose(file);
        return NULL;
    }
    //fseek(file, 10, SEEK_SET);
    fread(buffer, header_size+10, 1, file);
    fclose(file);


    //parse free and return
    tag = load_tag_with_buffer(buffer, header_size);
    free(buffer);

    return tag;
}

ID3v2_tag* load_tag_with_buffer(char *bytes, int length)
{
    // Declaration
    ID3v2_frame *frame;
    int offset = 0;
    ID3v2_tag* tag;
    ID3v2_header* tag_header;

    // Initialization
    tag_header = get_tag_header_with_buffer(bytes, length);

    if(tag_header == NULL) // no valid header found
      return NULL;

    if(get_tag_version(tag_header) == NO_COMPATIBLE_TAG)
    {
        // no supported id3 tag found
        free(tag_header);
        return NULL;
    }

    if(length < tag_header->tag_size+10)
    {
        // Not enough bytes provided to parse completely. TODO: how to communicate to the user the lack of bytes?
        free(tag_header);
        return NULL;
    }

    tag = new_tag();

    // Associations
    tag->tag_header = tag_header;

    // move the bytes pointer to the correct position
    bytes+=10; // skip header
    if(tag_header->extended_header_size)
      // an extended header exists, so we skip it too
      bytes+=tag_header->extended_header_size+4; // don't forget to skip the extended header size bytes too
    
    tag->raw = (char*) malloc(tag->tag_header->tag_size * sizeof(char));
    memcpy(tag->raw, bytes, tag_header->tag_size);
    // we use tag_size here to prevent copying too much if the user provides more bytes than needed to this function

    while(offset < tag_header->tag_size)
    {
        frame = parse_frame(tag->raw, offset, get_tag_version(tag_header));

        if(frame != NULL)
        {
            offset += frame->size + 10;
            add_to_list(tag->frames, frame);
        }
        else
        {
            break;
        }
    }

    return tag;
}

void remove_tag(const char* file_name)
{
    int c;
    FILE* file;
    FILE* temp_file;
    ID3v2_header* tag_header;

    file = fopen(file_name, "r+b");
    temp_file = tmpfile();

    tag_header = get_tag_header(file_name);
    if(tag_header == NULL)
    {
        return;
    }

    fseek(file, tag_header->tag_size + 10, SEEK_SET);
    while((c = getc(file)) != EOF)
    {
        putc(c, temp_file);
    }

    // Write temp file data back to original file
    fseek(temp_file, 0, SEEK_SET);
    fseek(file, 0, SEEK_SET);
    while((c = getc(temp_file)) != EOF)
    {
        putc(c, file);
    }
}

void write_header(ID3v2_header* tag_header, FILE* file)
{
    fwrite("ID3", 3, 1, file);
    fwrite(&tag_header->major_version, 1, 1, file);
    fwrite(&tag_header->minor_version, 1, 1, file);
    fwrite(&tag_header->flags, 1, 1, file);
    fwrite(itob(syncint_encode(tag_header->tag_size)), 4, 1, file);
}

void write_frame(ID3v2_frame* frame, FILE* file)
{
    fwrite(frame->frame_id, 1, 4, file);
    fwrite(itob(frame->size), 1, 4, file);
    fwrite(frame->flags, 1, 2, file);
    fwrite(frame->data, 1, frame->size, file);
}

int get_tag_size(ID3v2_tag* tag)
{
    int size = 0;
    ID3v2_frame_list* frame_list = new_frame_list();

    if(tag->frames == NULL)
    {
        return size;
    }

    frame_list = tag->frames->start;
    while(frame_list != NULL)
    {
        size += frame_list->frame->size + 10;
        frame_list = frame_list->next;
    }

    return size;
}

void set_tag(const char* file_name, ID3v2_tag* tag)
{
    int c;
    FILE *file;
    ID3v2_frame_list *frame_list;
    int i;
    int padding = 2048;
    int old_size;
    FILE* temp_file;

    if(tag == NULL)
    {
        return;
    }

    old_size = tag->tag_header->tag_size;

    // Set the new tag header
    tag->tag_header = new_header();
    memcpy(tag->tag_header->tag, "ID3", 3);
    tag->tag_header->major_version = '\x03';
    tag->tag_header->minor_version = '\x00';
    tag->tag_header->flags = '\x00';
    tag->tag_header->tag_size = get_tag_size(tag) + padding;

    // Create temp file and prepare to write
    file = fopen(file_name, "r+b");
    temp_file = tmpfile();

    // Write to file
    write_header(tag->tag_header, temp_file);
    frame_list = tag->frames->start;
    while(frame_list != NULL)
    {
        write_frame(frame_list->frame, temp_file);
        frame_list = frame_list->next;
    }

    // Write padding
    for(i = 0; i < padding; i++)
    {
        putc('\x00', temp_file);
    }

    fseek(file, old_size + 10, SEEK_SET);
    while((c = getc(file)) != EOF)
    {
        putc(c, temp_file);
    }

    // Write temp file data back to original file
    fseek(temp_file, 0, SEEK_SET);
    fseek(file, 0, SEEK_SET);
    while((c = getc(temp_file)) != EOF)
    {
        putc(c, file);
    }

    fclose(file);
    fclose(temp_file);
}

/**
 * Getter functions
 */
ID3v2_frame* tag_get_title(ID3v2_tag* tag)
{
    if(tag == NULL)
    {
        return NULL;
    }

    return get_from_list(tag->frames, "TIT2");
}

ID3v2_frame* tag_get_artist(ID3v2_tag* tag)
{
    if(tag == NULL)
    {
        return NULL;
    }

    return get_from_list(tag->frames, "TPE1");
}

ID3v2_frame* tag_get_album(ID3v2_tag* tag)
{
    if(tag == NULL)
    {
        return NULL;
    }

    return get_from_list(tag->frames, "TALB");
}

ID3v2_frame* tag_get_album_artist(ID3v2_tag* tag)
{
    if(tag == NULL)
    {
        return NULL;
    }

    return get_from_list(tag->frames, "TPE2");
}

ID3v2_frame* tag_get_genre(ID3v2_tag* tag)
{
    if(tag == NULL)
    {
        return NULL;
    }

    return get_from_list(tag->frames, "TCON");
}

ID3v2_frame* tag_get_track(ID3v2_tag* tag)
{
    if(tag == NULL)
    {
        return NULL;
    }

    return get_from_list(tag->frames, "TRCK");
}

ID3v2_frame* tag_get_year(ID3v2_tag* tag)
{
    if(tag == NULL)
    {
        return NULL;
    }

    return get_from_list(tag->frames, "TYER");
}

ID3v2_frame* tag_get_comment(ID3v2_tag* tag)
{
    if(tag == NULL)
    {
        return NULL;
    }

    return get_from_list(tag->frames, "COMM");
}

ID3v2_frame* tag_get_disc_number(ID3v2_tag* tag)
{
    if(tag == NULL)
    {
        return NULL;
    }

    return get_from_list(tag->frames, "TPOS");
}

ID3v2_frame* tag_get_composer(ID3v2_tag* tag)
{
    if(tag == NULL)
    {
        return NULL;
    }

    return get_from_list(tag->frames, "TCOM");
}

ID3v2_frame* tag_get_album_cover(ID3v2_tag* tag)
{
    if(tag == NULL)
    {
        return NULL;
    }

    return get_from_list(tag->frames, "APIC");
}

/**
 * Setter functions
 */
void set_text_frame(char* data, char encoding, char* frame_id, ID3v2_frame* frame)
{
    char *frame_data;
    // Set frame id and size
    memcpy(frame->frame_id, frame_id, 4);
    frame->size = 1 + (int) strlen(data);

    // Set frame data
    // TODO: Make the encoding param relevant.
    frame_data = (char*) malloc(frame->size * sizeof(char));
    frame->data = (char*) malloc(frame->size * sizeof(char));

    sprintf(frame_data, "%c%s", encoding, data);
    memcpy(frame->data, frame_data, frame->size);

    free(frame_data);
}

void set_comment_frame(char* data, char encoding, ID3v2_frame* frame)
{
    char *frame_data;

    memcpy(frame->frame_id, COMMENT_FRAME_ID, 4);
    frame->size = 1 + 3 + 1 + (int) strlen(data); // encoding + language + description + comment

    frame_data = (char*) malloc(frame->size * sizeof(char));
    frame->data = (char*) malloc(frame->size * sizeof(char));

    sprintf(frame_data, "%c%s%c%s", encoding, "eng", '\x00', data);
    memcpy(frame->data, frame_data, frame->size);

    free(frame_data);
}

void set_album_cover_frame(char* album_cover_bytes, char* mimetype, int picture_size, ID3v2_frame* frame)
{
    char *frame_data;
    int offset;

    memcpy(frame->frame_id, ALBUM_COVER_FRAME_ID, 4);
    frame->size = 1 + (int) strlen(mimetype) + 1 + 1 + 1 + picture_size; // encoding + mimetype + 00 + type + description + picture

    frame_data = (char*) malloc(frame->size * sizeof(char));
    frame->data = (char*) malloc(frame->size * sizeof(char));

    offset = 1 + (int) strlen(mimetype) + 1 + 1 + 1;
    sprintf(frame_data, "%c%s%c%c%c", '\x00', mimetype, '\x00', FRONT_COVER, '\x00');
    memcpy(frame->data, frame_data, offset);
    memcpy(frame->data + offset, album_cover_bytes, picture_size);

    free(frame_data);
}

void tag_set_title(char* title, char encoding, ID3v2_tag* tag)
{
    ID3v2_frame* title_frame = NULL;
    if( ! (title_frame = tag_get_title(tag)))
    {
        title_frame = new_frame();
        add_to_list(tag->frames, title_frame);
    }

    set_text_frame(title, encoding, TITLE_FRAME_ID, title_frame);
}

void tag_set_artist(char* artist, char encoding, ID3v2_tag* tag)
{
    ID3v2_frame* artist_frame = NULL;
    if( ! (artist_frame = tag_get_artist(tag)))
    {
        artist_frame = new_frame();
        add_to_list(tag->frames, artist_frame);
    }

    set_text_frame(artist, encoding, ARTIST_FRAME_ID, artist_frame);
}

void tag_set_album(char* album, char encoding, ID3v2_tag* tag)
{
    ID3v2_frame* album_frame = NULL;
    if( ! (album_frame = tag_get_album(tag)))
    {
        album_frame = new_frame();
        add_to_list(tag->frames, album_frame);
    }

    set_text_frame(album, encoding, ALBUM_FRAME_ID, album_frame);
}

void tag_set_album_artist(char* album_artist, char encoding, ID3v2_tag* tag)
{
    ID3v2_frame* album_artist_frame = NULL;
    if( ! (album_artist_frame = tag_get_album_artist(tag)))
    {
        album_artist_frame = new_frame();
        add_to_list(tag->frames, album_artist_frame);
    }

    set_text_frame(album_artist, encoding, ALBUM_ARTIST_FRAME_ID, album_artist_frame);
}

void tag_set_genre(char* genre, char encoding, ID3v2_tag* tag)
{
    ID3v2_frame* genre_frame = NULL;
    if( ! (genre_frame = tag_get_genre(tag)))
    {
        genre_frame = new_frame();
        add_to_list(tag->frames, genre_frame);
    }

    set_text_frame(genre, encoding, GENRE_FRAME_ID, genre_frame);
}

void tag_set_track(char* track, char encoding, ID3v2_tag* tag)
{
    ID3v2_frame* track_frame = NULL;
    if( ! (track_frame = tag_get_track(tag)))
    {
        track_frame = new_frame();
        add_to_list(tag->frames, track_frame);
    }

    set_text_frame(track, encoding, TRACK_FRAME_ID, track_frame);
}

void tag_set_year(char* year, char encoding, ID3v2_tag* tag)
{
    ID3v2_frame* year_frame = NULL;
    if( ! (year_frame = tag_get_year(tag)))
    {
        year_frame = new_frame();
        add_to_list(tag->frames, year_frame);
    }

    set_text_frame(year, encoding, YEAR_FRAME_ID, year_frame);
}

void tag_set_comment(char* comment, char encoding, ID3v2_tag* tag)
{
    ID3v2_frame* comment_frame = NULL;
    if( ! (comment_frame = tag_get_comment(tag)))
    {
        comment_frame = new_frame();
        add_to_list(tag->frames, comment_frame);
    }

    set_comment_frame(comment, encoding, comment_frame);
}

void tag_set_disc_number(char* disc_number, char encoding, ID3v2_tag* tag)
{
    ID3v2_frame* disc_number_frame = NULL;
    if( ! (disc_number_frame = tag_get_disc_number(tag)))
    {
        disc_number_frame = new_frame();
        add_to_list(tag->frames, disc_number_frame);
    }

    set_text_frame(disc_number, encoding, DISC_NUMBER_FRAME_ID, disc_number_frame);
}

void tag_set_composer(char* composer, char encoding, ID3v2_tag* tag)
{
    ID3v2_frame* composer_frame = NULL;
    if( ! (composer_frame = tag_get_composer(tag)))
    {
        composer_frame = new_frame();
        add_to_list(tag->frames, composer_frame);
    }

    set_text_frame(composer, encoding, COMPOSER_FRAME_ID, composer_frame);
}

void tag_set_album_cover(const char* filename, ID3v2_tag* tag)
{
    FILE* album_cover = fopen(filename, "rb");
    char *album_cover_bytes;
    int image_size;
    char *mimetype;

    fseek(album_cover, 0, SEEK_END);
    image_size = (int) ftell(album_cover);
    fseek(album_cover, 0, SEEK_SET);

    album_cover_bytes = (char*) malloc(image_size * sizeof(char));
    fread(album_cover_bytes, 1, image_size, album_cover);

    fclose(album_cover);

    mimetype = get_mime_type_from_filename(filename);
    tag_set_album_cover_from_bytes(album_cover_bytes, mimetype, image_size, tag);

    free(album_cover_bytes);
}

void tag_set_album_cover_from_bytes(char* album_cover_bytes, char* mimetype, int picture_size, ID3v2_tag* tag)
{
    ID3v2_frame* album_cover_frame = NULL;
    if( ! (album_cover_frame = tag_get_album_cover(tag)))
    {
        album_cover_frame = new_frame();
        add_to_list(tag->frames, album_cover_frame);
    }

    set_album_cover_frame(album_cover_bytes, mimetype, picture_size, album_cover_frame);
}
