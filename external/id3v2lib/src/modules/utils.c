/*
 * This file is part of id3v2lib library
 *
 * Copyright (c) Lars Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.private.h"

#define BOM_LENGTH 2

char* ID3v2_to_unicode(char* string)
{
    if (string_has_bom(string))
    {
        // Nothing to do, already an unicode string
        return string;
    }

    const int str_termination_size = 2;
    const int result_size = (strlen(string) * 2) + BOM_LENGTH + str_termination_size;
    char* result = (char*) malloc(result_size * sizeof(char));

    // Add BOM
    result[0] = 0xFF;
    result[1] = 0xFE;

    for (int i = 1; i <= strlen(string); i++)
    {
        result[i * 2] = string[i - 1];
        result[(i * 2) + 1] = 0x00;
    }

    // Add new string termination characters
    result[result_size - 2] = 0x00;
    result[result_size - 1] = 0x00;

    return result;
}

int ID3v2_strlen(const char* string)
{
    if (!string_has_bom(string))
    {
        // it's not an unicode string, so we can use the std implementation
        return strlen(string);
    }

    char a = -1;
    char b = -1;
    int length = 0;

    while (1)
    {
        a = string[length];
        b = string[length + 1];

        if (a == 0x00 && b == 0x00)
        {
            break;
        }

        length += 2;
    }

    return length;
}

int ID3v2_strlent(const char* string)
{
    const int termination_character_length = string_has_bom(string) ? 2 : 1;
    return ID3v2_strlen(string) + termination_character_length;
}

unsigned int btoi(const char* bytes, int size)
{
    unsigned int result = 0x00;

    for (int i = 0; i < size; i++)
    {
        result = result << 8;
        result = result | (unsigned char) bytes[i];
    }

    return result;
}

char* itob(int integer)
{
    int size = 4;
    char* result = (char*) malloc(sizeof(char) * size);

    // We need to reverse the bytes because Intel uses little endian.
    char* aux = (char*) &integer;

    for (int i = size - 1; i >= 0; i--)
    {
        result[size - 1 - i] = aux[i];
    }

    return result;
}

int syncint_encode(int value)
{
    int out, mask = 0x7F;

    while (mask ^ 0x7FFFFFFF)
    {
        out = value & ~mask;
        out <<= 1;
        out |= value & mask;
        mask = ((mask + 1) << 8) - 1;
        value = out;
    }

    return out;
}

unsigned int syncint_decode(int value)
{
    unsigned int a, b, c, d, result = 0x0;
    a = value & 0xFF;
    b = (value >> 8) & 0xFF;
    c = (value >> 16) & 0xFF;
    d = (value >> 24) & 0xFF;

    result = result | a;
    result = result | (b << 7);
    result = result | (c << 14);
    result = result | (d << 21);

    return result;
}

int clamp_int(const int value, const int min, const int max)
{
    return value < min ? min : (value > max ? max : value);
}

bool string_has_bom(const char* string)
{
    if (string == NULL)
    {
        return false;
    }

    if (memcmp("\xFF\xFE", string, 2) == 0 || memcmp("\xFE\xFF", string, 2) == 0)
    {
        return true;
    }

    return false;
}
