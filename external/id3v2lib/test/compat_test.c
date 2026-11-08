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
#include <string.h>

#include "id3v2lib.compat.h"
#include "test_utils.h"

#include "compat_test.h"

#define ORIGINAL_FILE "extra/file.mp3"
#define EDITED_FILE "extra/file_edited.mp3"

void compat_test_main()
{
    // Clone the file to not having to modify the original
    clone_file(ORIGINAL_FILE, EDITED_FILE);

    ID3v2_tag* tag = load_tag(EDITED_FILE);
    if (tag == NULL) tag = new_tag();

    char* album = ID3v2_to_unicode("Smoke And Shadows");
    char* artist = ID3v2_to_unicode("Ethereal Darkness");
    char* title = ID3v2_to_unicode("Rivers");

    // GETTERS
    ID3v2_frame* album_frame = tag_get_album(tag);
    ID3v2_frame_text_content* album_content = parse_text_frame_content(album_frame);
    assert(memcmp(album_content->data, album, ID3v2_strlent("Smoke And Shadows")) == 0);

    ID3v2_frame* artist_frame = tag_get_artist(tag);
    ID3v2_frame_text_content* artist_content = parse_text_frame_content(artist_frame);
    assert(memcmp(artist_content->data, artist, ID3v2_strlent("Ethereal Darkness")) == 0);

    ID3v2_frame* title_frame = tag_get_title(tag);
    ID3v2_frame_text_content* title_content = parse_text_frame_content(title_frame);
    assert(memcmp(title_content->data, title, ID3v2_strlent("Rivers")) == 0);

    ID3v2_frame* year_frame = tag_get_year(tag);
    ID3v2_frame_text_content* year_content = parse_text_frame_content(year_frame);
    assert(memcmp(year_content->data, "2019", 4) == 0);

    free(artist_content->data);
    free(artist_content);
    free(album_content->data);
    free(album_content);
    free(title_content->data);
    free(title_content);
    free(year_content->data);
    free(year_content);

    free(album);
    free(artist);
    free(title);

    // SETTERS
    tag_set_album("Album", 0, tag);
    tag_set_artist("Artist", 0, tag);
    tag_set_title("Title", 0, tag);
    tag_set_year("0000", 0, tag);

    // Write tag
    set_tag(EDITED_FILE, tag);

    free_tag(tag);

    // Verify
    tag = load_tag(EDITED_FILE);
    assert(tag != NULL);

    album_frame = tag_get_album(tag);
    album_content = parse_text_frame_content(album_frame);
    assert(memcmp(album_content->data, "Album", ID3v2_strlent("Album")) == 0);

    artist_frame = tag_get_artist(tag);
    artist_content = parse_text_frame_content(artist_frame);
    assert(memcmp(artist_content->data, "Artist", ID3v2_strlent("Artist")) == 0);

    title_frame = tag_get_title(tag);
    title_content = parse_text_frame_content(title_frame);
    assert(memcmp(title_content->data, "Title", ID3v2_strlent("Title")) == 0);

    year_frame = tag_get_year(tag);
    year_content = parse_text_frame_content(year_frame);
    assert(memcmp(year_content->data, "0000", 4) == 0);

    // Clean up
    remove(EDITED_FILE);

    free_tag(tag);

    free(artist_content->data);
    free(artist_content);
    free(album_content->data);
    free(album_content);
    free(title_content->data);
    free(title_content);
    free(year_content->data);
    free(year_content);

    printf("COMPAT TEST: OK\n");
}
