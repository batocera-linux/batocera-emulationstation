/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_utils_h
#define id3v2lib_utils_h

#define EMPTY_UNICODE_STR "\xFF\xFE\x00\x00"

/**
 * Turns an iso string (one byte per character) into an
 * utf-16 string (two byes per character). Takes care of
 * adding the UTF BOM and string termination characters.
 */
char* ID3v2_to_unicode(char* string);

/**
 * Correctly computes the length of unicode strings as well
 * as ISO ones. IMPORTANT: It doesn't take into account the
 * string termination character.
 */
int ID3v2_strlen(const char* string);

/**
 * Like ID3v2_strlen but also takes into account the length
 * of the string termination character (null character).
 */
int ID3v2_strlent(const char* string);

#endif
