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

#include "id3v2lib.h"
#include "test_utils.h"

#include "delete_test.h"

#define ORIGINAL_FILE "extra/file.mp3"
#define EDITED_FILE "extra/file_edited.mp3"

void delete_test_main()
{
    // Clone the file to not having to modify the original
    clone_file(ORIGINAL_FILE, EDITED_FILE);

    ID3v2_Tag* tag = ID3v2_read_tag(EDITED_FILE);

    if (tag == NULL)
    {
        tag = ID3v2_Tag_new_empty();
    }

    ID3v2_Tag_delete_frame(tag, ID3v2_ALBUM_FRAME_ID);
    ID3v2_Tag_delete_artist(tag);
    // ID3v2_Tag_delete_album(tag);
    ID3v2_Tag_delete_album_artist(tag);
    ID3v2_Tag_delete_comment_frame(tag, 0);
    // ID3v2_Tag_delete_comment(tag); // Equivalent to the above
    ID3v2_Tag_delete_composer(tag);
    ID3v2_Tag_delete_disc_number(tag);
    ID3v2_Tag_delete_genre(tag);
    ID3v2_Tag_delete_title(tag);
    ID3v2_Tag_delete_track(tag);
    ID3v2_Tag_delete_year(tag);
    // ID3v2_Tag_delete_apic_frame(tag, 0);
    ID3v2_Tag_delete_album_cover(tag); // Equivalent to the above

    ID3v2_write_tag(EDITED_FILE, tag);

    // Verify the written data
    ID3v2_Tag* edited_tag = ID3v2_read_tag(EDITED_FILE);
    assert(edited_tag != NULL);
    assert(edited_tag->header->tag_size == edited_tag->padding_size);

    assert(ID3v2_Tag_get_artist_frame(edited_tag) == NULL);
    assert(ID3v2_Tag_get_album_frame(edited_tag) == NULL);
    assert(ID3v2_Tag_get_title_frame(edited_tag) == NULL);
    assert(ID3v2_Tag_get_track_frame(edited_tag) == NULL);
    assert(ID3v2_Tag_get_album_artist_frame(edited_tag) == NULL);
    assert(ID3v2_Tag_get_genre_frame(edited_tag) == NULL);
    assert(ID3v2_Tag_get_year_frame(edited_tag) == NULL);
    assert(ID3v2_Tag_get_disc_number_frame(edited_tag) == NULL);
    assert(ID3v2_Tag_get_composer_frame(edited_tag) == NULL);
    assert(ID3v2_Tag_get_comment_frame(edited_tag) == NULL);
    assert(ID3v2_Tag_get_album_cover_frame(edited_tag) == NULL);

    ID3v2_delete_tag(EDITED_FILE);
    ID3v2_Tag* deleted_tag = ID3v2_read_tag(EDITED_FILE);
    assert(deleted_tag == NULL);

    // Clean up
    remove(EDITED_FILE);

    ID3v2_Tag_free(tag);
    ID3v2_Tag_free(edited_tag);

    printf("DELETE TEST: OK\n");
}
