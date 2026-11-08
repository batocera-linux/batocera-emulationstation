/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_tag_private_h
#define id3v2lib_tag_private_h

#include "modules/tag.h"

typedef struct _CharStream CharStream;

ID3v2_Tag* Tag_parse(CharStream* tag_cs);
CharStream* Tag_to_char_stream(ID3v2_Tag* tag);

#endif
