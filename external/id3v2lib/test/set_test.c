/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "assertion_utils.h"
#include "id3v2lib.h"
#include "test_utils.h"

#include "set_test.h"

#define ORIGINAL_FILE "extra/file.mp3"
#define EDITED_FILE "extra/file_edited.mp3"

void edit_test()
{
    // Clone the file to not having to modify the original
    clone_file(ORIGINAL_FILE, EDITED_FILE);

    ID3v2_Tag* tag = ID3v2_read_tag(EDITED_FILE);

    if (tag == NULL)
    {
        tag = ID3v2_Tag_new_empty();
    }

    char* artist = ID3v2_to_unicode("Artist");
    ID3v2_Tag_set_artist(tag, artist);

    char* album = ID3v2_to_unicode("Album");
    ID3v2_Tag_set_album(tag, album);

    char* title = ID3v2_to_unicode("Title");
    ID3v2_Tag_set_title(tag, title);

    ID3v2_Tag_set_track(tag, "00");

    char* album_artist = ID3v2_to_unicode("Album Artist");
    ID3v2_Tag_set_album_artist(tag, album_artist);

    char* genre = ID3v2_to_unicode("Genre");
    ID3v2_Tag_set_genre(tag, genre);

    ID3v2_Tag_set_year(tag, "0000");

    ID3v2_Tag_set_disc_number(tag, "00");

    char* composer = ID3v2_to_unicode("Writter");
    ID3v2_Tag_set_composer(tag, composer);

    char* comment = ID3v2_to_unicode("Comment");
    char* short_desc = ID3v2_to_unicode("Short Description");
    ID3v2_Tag_set_comment(tag, "eng", comment);
    ID3v2_Tag_add_comment_frame(
        tag,
        &(ID3v2_CommentFrameInput){
            .flags = "\0\0",
            .language = "eng",
            .short_description = short_desc,
            .comment = comment,
        }
    );

    FILE* album_cover_fp = fopen("extra/album_cover.png", "rb");
    fseek(album_cover_fp, 0L, SEEK_END);
    const int cover_file_size = ftell(album_cover_fp);
    fseek(album_cover_fp, 0L, SEEK_SET);
    char* picture_data = (char*) malloc(cover_file_size * sizeof(char));
    fread(picture_data, 1, cover_file_size, album_cover_fp);
    char* description = ID3v2_to_unicode("Description");
    fclose(album_cover_fp);

    ID3v2_Tag_set_album_cover(tag, ID3v2_MIME_TYPE_PNG, cover_file_size, picture_data);
    ID3v2_Tag_add_apic_frame(
        tag,
        &(ID3v2_ApicFrameInput){
            .flags = "\0\0",
            .mime_type = ID3v2_MIME_TYPE_PNG,
            .description = description,
            .picture_type = ID3v2_PIC_TYPE_FRONT_COVER,
            .data = picture_data,
            .picture_size = cover_file_size,
        }
    );

    // Write the edited tag
    ID3v2_write_tag(EDITED_FILE, tag);

    // Verify the written data
    ID3v2_Tag* edited_tag = ID3v2_read_tag(EDITED_FILE);
    assert(edited_tag != NULL);

    assert_text_frame(
        ID3v2_Tag_get_artist_frame(edited_tag),
        &(ID3v2_TextFrameInput){
            .id = ID3v2_ARTIST_FRAME_ID,
            .flags = "\0\0",
            .text = artist,
        }
    );
    free(artist);

    assert_text_frame(
        ID3v2_Tag_get_album_frame(edited_tag),
        &(ID3v2_TextFrameInput){
            .id = ID3v2_ALBUM_FRAME_ID,
            .flags = "\0\0",
            .text = album,
        }
    );
    free(album);

    assert_text_frame(
        ID3v2_Tag_get_title_frame(edited_tag),
        &(ID3v2_TextFrameInput){
            .id = ID3v2_TITLE_FRAME_ID,
            .flags = "\0\0",
            .text = title,
        }
    );
    free(title);

    assert_text_frame(
        ID3v2_Tag_get_track_frame(edited_tag),
        &(ID3v2_TextFrameInput){
            .id = ID3v2_TRACK_FRAME_ID,
            .flags = "\0\0",
            .text = "00",
        }
    );

    assert_text_frame(
        ID3v2_Tag_get_album_artist_frame(edited_tag),
        &(ID3v2_TextFrameInput){
            .id = ID3v2_ALBUM_ARTIST_FRAME_ID,
            .flags = "\0\0",
            .text = album_artist,
        }
    );
    free(album_artist);

    assert_text_frame(
        ID3v2_Tag_get_genre_frame(edited_tag),
        &(ID3v2_TextFrameInput){
            .id = ID3v2_GENRE_FRAME_ID,
            .flags = "\0\0",
            .text = genre,
        }
    );
    free(genre);

    assert_text_frame(
        ID3v2_Tag_get_year_frame(edited_tag),
        &(ID3v2_TextFrameInput){
            .id = ID3v2_YEAR_FRAME_ID,
            .flags = "\0\0",
            .text = "0000",
        }
    );

    assert_text_frame(
        ID3v2_Tag_get_disc_number_frame(edited_tag),
        &(ID3v2_TextFrameInput){
            .id = ID3v2_DISC_NUMBER_FRAME_ID,
            .flags = "\0\0",
            .text = "00",
        }
    );

    assert_text_frame(
        ID3v2_Tag_get_composer_frame(edited_tag),
        &(ID3v2_TextFrameInput){
            .id = ID3v2_COMPOSER_FRAME_ID,
            .flags = "\0\0",
            .text = composer,
        }
    );
    free(composer);

    assert_comment_frame(
        ID3v2_Tag_get_comment_frame(edited_tag),
        &(ID3v2_CommentFrameInput){
            .flags = "\0\0",
            .language = "eng",
            .short_description = EMPTY_UNICODE_STR,
            .comment = comment,
        }
    );

    ID3v2_FrameList* comments = ID3v2_Tag_get_comment_frames(edited_tag);
    assert_comment_frame(
        (ID3v2_CommentFrame*) comments->next->frame,
        &(ID3v2_CommentFrameInput){
            .flags = "\0\0",
            .language = "eng",
            .short_description = short_desc,
            .comment = comment,
        }
    );
    ID3v2_FrameList_unlink(comments);
    free(comment);
    free(short_desc);

    assert_apic_frame(
        ID3v2_Tag_get_album_cover_frame(edited_tag),
        &(ID3v2_ApicFrameInput){
            .flags = "\0\0",
            .mime_type = ID3v2_MIME_TYPE_PNG,
            .description = EMPTY_UNICODE_STR,
            .picture_type = ID3v2_PIC_TYPE_FRONT_COVER,
            .data = picture_data,
            .picture_size = cover_file_size,
        }
    );

    ID3v2_FrameList* apics = ID3v2_Tag_get_apic_frames(edited_tag);
    assert_apic_frame(
        (ID3v2_ApicFrame*) apics->next->frame,
        &(ID3v2_ApicFrameInput){
            .flags = "\0\0",
            .mime_type = ID3v2_MIME_TYPE_PNG,
            .description = description,
            .picture_type = ID3v2_PIC_TYPE_FRONT_COVER,
            .data = picture_data,
            .picture_size = cover_file_size,
        }
    );
    ID3v2_FrameList_unlink(apics);
    free(picture_data);
    free(description);

    // Clean up
    remove(EDITED_FILE);

    ID3v2_Tag_free(tag);
    ID3v2_Tag_free(edited_tag);

    printf("SET TEST: OK\n");
}

void new_tag_test()
{
}

void set_test_main()
{
    edit_test();
    new_tag_test();
}
