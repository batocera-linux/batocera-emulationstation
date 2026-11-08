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

void get_test_existing()
{
    ID3v2_Tag* tag = ID3v2_read_tag("./extra/file.mp3");

    char* artist = ID3v2_to_unicode("Ethereal Darkness");
    assert_text_frame(
        ID3v2_Tag_get_artist_frame(tag),
        &(ID3v2_TextFrameInput){
            .id = ID3v2_ARTIST_FRAME_ID,
            .flags = "\0\0",
            .text = artist,
        }
    );
    free(artist);

    char* album = ID3v2_to_unicode("Smoke And Shadows");
    assert_text_frame(
        ID3v2_Tag_get_album_frame(tag),
        &(ID3v2_TextFrameInput){
            .id = ID3v2_ALBUM_FRAME_ID,
            .flags = "\0\0",
            .text = album,
        }
    );
    free(album);

    char* title = ID3v2_to_unicode("Rivers");
    assert_text_frame(
        ID3v2_Tag_get_title_frame(tag),
        &(ID3v2_TextFrameInput){
            .id = ID3v2_TITLE_FRAME_ID,
            .flags = "\0\0",
            .text = title,
        }
    );
    free(title);

    assert_text_frame(
        ID3v2_Tag_get_track_frame(tag),
        &(ID3v2_TextFrameInput){
            .id = ID3v2_TRACK_FRAME_ID,
            .flags = "\0\0",
            .text = "1",
        }
    );

    char* album_artist = ID3v2_to_unicode("Ethereal Darkness");
    assert_text_frame(
        ID3v2_Tag_get_album_artist_frame(tag),
        &(ID3v2_TextFrameInput){
            .id = ID3v2_ALBUM_ARTIST_FRAME_ID,
            .flags = "\0\0",
            .text = album_artist,
        }
    );
    free(album_artist);

    char* genre = ID3v2_to_unicode("Melodic Death Metal");
    assert_text_frame(
        ID3v2_Tag_get_genre_frame(tag),
        &(ID3v2_TextFrameInput){
            .id = ID3v2_GENRE_FRAME_ID,
            .flags = "\0\0",
            .text = genre,
        }
    );
    free(genre);

    assert_text_frame(
        ID3v2_Tag_get_year_frame(tag),
        &(ID3v2_TextFrameInput){
            .id = ID3v2_YEAR_FRAME_ID,
            .flags = "\0\0",
            .text = "2019",
        }
    );

    assert_text_frame(
        ID3v2_Tag_get_disc_number_frame(tag),
        &(ID3v2_TextFrameInput){
            .id = ID3v2_DISC_NUMBER_FRAME_ID,
            .flags = "\0\0",
            .text = "1",
        }
    );

    char* composer = ID3v2_to_unicode("Composer");
    assert_text_frame(
        ID3v2_Tag_get_composer_frame(tag),
        &(ID3v2_TextFrameInput){
            .id = ID3v2_COMPOSER_FRAME_ID,
            .flags = "\0\0",
            .text = composer,
        }
    );
    free(composer);

    char* comment = ID3v2_to_unicode("This is a comment");
    char* short_description = ID3v2_to_unicode("");

    assert_comment_frame(
        ID3v2_Tag_get_comment_frame(tag),
        &(ID3v2_CommentFrameInput){
            .flags = "\0\0",
            .comment = comment,
            .language = "eng",
            .short_description = short_description,
        }
    );

    ID3v2_FrameList* comments = ID3v2_Tag_get_comment_frames(tag);
    assert_comment_frame(
        (ID3v2_CommentFrame*) comments->frame,
        &(ID3v2_CommentFrameInput){
            .flags = "\0\0",
            .comment = comment,
            .language = "eng",
            .short_description = short_description,
        }
    );

    ID3v2_FrameList_unlink(comments);
    free(comment);
    free(short_description);

    FILE* album_cover_fp = fopen("extra/album_cover.png", "rb");
    fseek(album_cover_fp, 0L, SEEK_END);
    const int cover_file_size = ftell(album_cover_fp);
    fseek(album_cover_fp, 0L, SEEK_SET);
    char* picture_data = (char*) malloc(cover_file_size * sizeof(char));
    fread(picture_data, 1, cover_file_size, album_cover_fp);
    fclose(album_cover_fp);

    assert_apic_frame(
        ID3v2_Tag_get_album_cover_frame(tag),
        &(ID3v2_ApicFrameInput){
            .flags = "\0\0",
            .mime_type = ID3v2_MIME_TYPE_PNG,
            .description = "",
            .picture_type = 0x03,
            .data = picture_data,
            .picture_size = cover_file_size,
        }
    );
    free(picture_data);

    ID3v2_Tag_free(tag);

    printf("GET TEST EXISTING: OK\n");
}

void get_test_empty()
{
    ID3v2_Tag* no_tag = ID3v2_read_tag("./extra/no_tag.mp3");
    assert(no_tag == NULL);

    ID3v2_Tag* empty_tag = ID3v2_read_tag("./extra/empty.mp3");
    assert(empty_tag == NULL);

    printf("GET TEST EMPTY: OK\n");
}

void get_test_main()
{
    get_test_existing();
    get_test_empty();
}
