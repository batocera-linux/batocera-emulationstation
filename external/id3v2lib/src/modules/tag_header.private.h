/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_tag_header_private_h
#define id3v2lib_tag_header_private_h

#include "modules/tag_header.h"

typedef struct _CharStream CharStream;

ID3v2_TagHeader* TagHeader_new(
    const int major_version,
    const int minor_version,
    const int flags,
    const int tag_size,
    const int extended_header_size
);
ID3v2_TagHeader* TagHeader_new_empty();
ID3v2_TagHeader* TagHeader_parse(CharStream* stream);

#endif
