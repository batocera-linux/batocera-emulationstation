/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_id3v2lib_h
#define id3v2lib_id3v2lib_h

#ifdef __cplusplus
// clang-format off
extern "C" {
#endif

#include "modules/frame_header.h"
#include "modules/frame_ids.h"
#include "modules/frame_list.h"
#include "modules/frame.h"
#include "modules/frames/apic_frame.h"
#include "modules/frames/comment_frame.h"
#include "modules/frames/text_frame.h"
#include "modules/picture_types.h"
#include "modules/tag_header.h"
#include "modules/tag.h"
#include "modules/utils.h"

ID3v2_TagHeader* ID3v2_read_tag_header(const char* file_name);
ID3v2_TagHeader* ID3v2_read_tag_header_from_buffer(const char* buffer);

ID3v2_Tag* ID3v2_read_tag(const char* file_name);
ID3v2_Tag* ID3v2_read_tag_from_buffer(const char* tag_buffer, const int buffer_size);

void ID3v2_write_tag(const char* file_name, ID3v2_Tag* tag);

void ID3v2_delete_tag(const char* file_name);

#ifdef __cplusplus
} // extern "C"
// clang-format on
#endif

#endif
