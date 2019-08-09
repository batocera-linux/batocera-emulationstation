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

#include "utils.h"

unsigned int btoi(char* bytes, int size, int offset)
{
    unsigned int result = 0x00;
    int i = 0;
    for(i = 0; i < size; i++)
    {
        result = result << 8;
        result = result | (unsigned char) bytes[offset + i];
    }
    
    return result;
}

char* itob(int integer)
{
    int i;
    int size = 4;
    char* result = (char*) malloc(sizeof(char) * size);
    
    // We need to reverse the bytes because Intel uses little endian.
    char* aux = (char*) &integer;
    for(i = size - 1; i >= 0; i--)
    {
        result[size - 1 - i] = aux[i];
    }
    
    return result;
}

int syncint_encode(int value)
{
    int out, mask = 0x7F;
    
    while (mask ^ 0x7FFFFFFF) {
        out = value & ~mask;
        out <<= 1;
        out |= value & mask;
        mask = ((mask + 1) << 8) - 1;
        value = out;
    }
    
    return out;
}

int syncint_decode(int value)
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

void add_to_list(ID3v2_frame_list* main, ID3v2_frame* frame)
{
    ID3v2_frame_list *current;

    // if empty list
    if(main->start == NULL)
    {
        main->start = main;
        main->last = main;
        main->frame = frame;
    }
    else
    {
        current = new_frame_list();
        current->frame = frame;
        current->start = main->start;
        main->last->next = current;
        main->last = current;
    }
}

ID3v2_frame* get_from_list(ID3v2_frame_list* list, char* frame_id)
{
    while(list != NULL && list->frame != NULL)
    {
        if(strncmp(list->frame->frame_id, frame_id, 4) == 0) {
            return list->frame;
        }
        list = list->next;
    }
    return NULL;
}

void free_tag(ID3v2_tag* tag)
{
    ID3v2_frame_list *list;

    free(tag->raw);
    free(tag->tag_header);
    list = tag->frames;
    while(list != NULL)
    {
        if (list->frame) free(list->frame->data);
        free(list->frame);
        list = list->next;
    }
    free(list);
    free(tag);
}

char* get_mime_type_from_filename(const char* filename)
{
    if(strcmp(strrchr(filename, '.') + 1, "png") == 0)
    {
        return PNG_MIME_TYPE;
    }
    else
    {
        return JPG_MIME_TYPE;
    }
}

void genre_num_string(char* dest, char *genre_data)
{
    if (genre_data == NULL) {
        return;
    }

    int length = strlen(genre_data);
    int genre_number_index = 0;
    char genre_number[3];
    int first_parenthesis_found = 1;
    int second_parenthese_found = 1;
    
    for (int i = 0; i < length; ++i) {
        if (genre_data[i] == '(' && first_parenthesis_found == 1) {
	    first_parenthesis_found = 0;
	    continue;
	}
	if (genre_data[i] == ')' && second_parenthese_found == 1 ) {
	    second_parenthese_found = 0;
	    break;
	}
	if (first_parenthesis_found == 0) {
	    genre_number[genre_number_index++] = genre_data[i];
	}
    }

    if (first_parenthesis_found == 1 || second_parenthese_found == 1) {
	printf("nothing found\n");
	strcpy(dest, genre_data);
	return;
    }

    int genre_id = atoi(genre_number);
    char *genre = convert_genre_number(genre_id);
    strcpy(dest, genre);
}

char* convert_genre_number(int number)
{
    char *genre;

    switch (number) {
	case ID_BLUES:
	    genre = BLUES;
	    break;
	case ID_CLASSIC_ROCK:
	    genre = CLASSIC_ROCK;
	    break;
	case ID_COUNTRY:
	    genre = COUNTRY;
	    break;
	case ID_DANCE:
	    genre = DANCE;
	    break;
	case ID_DISCO:
	    genre = DISCO;
	    break;
	case ID_FUNK:
	    genre = FUNK;
	    break;
	case ID_GRUNGE:
	    genre = GRUNGE;
	    break;
	case ID_HIP_HOP:
	    genre = HIP_HOP;
	    break;
	case ID_JAZZ:
	    genre = JAZZ;
	    break;
	case ID_METAL:
	    genre = METAL;
	    break;
	case ID_NEW_AGE:
	    genre = NEW_AGE;
	    break;
	case ID_OLDIES:
	    genre = OLDIES;
	    break;
	case ID_OTHER_GENRE:
	    genre = OTHER_GENRE;
	    break;
	case ID_POP:
	    genre = POP;
	    break;
        case ID_R_AND_B:
	    genre = R_AND_B;
	    break;
	case ID_RAP:
	    genre = RAP;
	    break;
	case ID_REGGAE:
	    genre = REGGAE;
	    break;
	case ID_ROCK:
	    genre = ROCK;
	    break;
	case ID_TECHNO:
	    genre = TECHNO;
	    break;
	case ID_INDUSTRIAL:
	    genre = INDUSTRIAL;
	    break;
	case ID_ALTERNATIVE:
	    genre = ALTERNATIVE;
	    break;
	case ID_SKA:
	    genre = SKA;
	    break;
	case ID_DEATH_METAL:
	    genre = DEATH_METAL;
	    break;
	case ID_PRANKS:
	    genre = PRANKS;
	    break;
	case ID_SOUNDTRACK:
	    genre = SOUNDTRACK;
	    break;
	case ID_EURO_TECHNO:
	    genre = EURO_TECHNO;
	    break;
	case ID_AMBIENT:
	    genre = AMBIENT;
	    break;
	case ID_TRIP_HOP:
	    genre = TRIP_HOP;
	    break;
	case ID_VOCAL:
	    genre = VOCAL;
	    break;
	case ID_JAZZ_FUNK:
	    genre = JAZZ_FUNK;
	    break;
	case ID_FUSION:
	    genre = FUSION;
	    break;
	case ID_TRANCE:
	    genre = TRANCE;
	    break;
	case ID_CLASSICAL:
	    genre = CLASSICAL;
	    break;
	case ID_INSTRUMENTAL:
	    genre = INSTRUMENTAL;
	    break;
	case ID_ACID:
	    genre = ACID;
	    break;
	case ID_HOUSE:
	    genre = HOUSE;
	    break;
	case ID_GAME:
	    genre = GAME;
	    break;
	case ID_SOUND_CLIP:
	    genre = SOUND_CLIP;
	    break;
	case ID_GOSPEL:
	    genre = GOSPEL;
	    break;
	case ID_NOISE:
	    genre = NOISE;
	    break;
	case ID_ALTERNATIVE_ROCK:
	    genre = ALTERNATIVE_ROCK;
	    break;
	case ID_BASS:
	    genre = BASS;
	    break;
	case ID_SOUL:
	    genre = SOUL;
	    break;
	case ID_PUNK:
	    genre = PUNK;
	    break;
	case ID_SPACE:
	    genre = SPACE;
	    break;
	case ID_MEDITATIVE:
	    genre = MEDITATIVE;
	    break;
	case ID_INSTRUMENTAL_POP:
	    genre = INSTRUMENTAL_POP;
	    break;
	case ID_INSTRUMENTAL_ROCK:
	    genre = INSTRUMENTAL_ROCK;
	    break;
	case ID_ETHIC:
	    genre = ETHIC;
	    break;
	case ID_GOTHIC:
	    genre = GOTHIC;
	    break;
	case ID_DARKWAVE:
	    genre = DARKWAVE;
	    break;
	case ID_TECHNO_INDUSTRIAL:
	    genre = TECHNO_INDUSTRIAL;
	    break;
	case ID_ELECTRONIC:
	    genre = ELECTRONIC;
	    break;
	case ID_POP_FOLK:
	    genre = POP_FOLK;
	    break;
	case ID_EURODANCE:
	    genre = EURODANCE;
	    break;
	case ID_DREAM:
	    genre = DREAM;
	    break;
	case ID_SOUTHERN_ROCK:
	    genre = SOUTHERN_ROCK;
	    break;
	case ID_COMEDY:
	    genre = COMEDY;
	    break;
	case ID_CULT:
	    genre = CULT;
	    break;
	case ID_GANGSTA:
	    genre = GANGSTA;
	    break;
	case ID_TOP_FOURTY:
	    genre = TOP_FOURTY;
	    break;
	case ID_CHRISTIAN_RAP:
	    genre = CHRISTIAN_RAP;
	    break;
	case ID_POP_FUNK:
	    genre = POP_FUNK;
	    break;
	case ID_JUNGLE:
	    genre = JUNGLE;
	    break;
	case ID_NATIVE_AMERICAN:
	    genre = NATIVE_AMERICAN;
	    break;
	case ID_CABARET:
	    genre = CABARET;
	    break;
	case ID_NEW_WAVE:
	    genre = NEW_WAVE;
	    break;
	case ID_PSYCHADELIC:
	    genre = PSYCHADELIC;
	    break;
	case ID_RAVE:
	    genre = RAVE;
	    break;
	case ID_SHOWTUNES:
	    genre = SHOWTUNES;
	    break;
	case ID_TRAILER:
	    genre = TRAILER;
	    break;
	case ID_LO_FI:
	    genre = LO_FI;
	    break;
	case ID_TRIBAL:
	    genre = TRIBAL;
	    break;
	case ID_ACID_PUNK:
	    genre = ACID_PUNK;
	    break;
	case ID_ACID_JAZZ:
	    genre = ACID_JAZZ;
	    break;
	case ID_POLKA:
	    genre = POLKA;
	    break;
	case ID_RETRO:
	    genre = RETRO;
	    break;
	case ID_MUSICAL:
	    genre = MUSICAL;
	    break;
	case ID_ROCK_AND_ROLL:
	    genre = ROCK_AND_ROLL;
	    break;
	case ID_HARD_ROCK:
	    genre = HARD_ROCK;
	    break;
	case ID_FOLK:
	    genre = FOLK;
	    break;
	case ID_FOLK_ROCK:
	    genre = FOLK_ROCK;
	    break;
	case ID_NATIONAL_FOLK:
	    genre = NATIONAL_FOLK;
	    break;
	case ID_SWING:
	    genre = SWING;
	    break;
	case ID_FAST_FUSION:
	    genre = FAST_FUSION;
	    break;
	case ID_BEBOB:
	    genre = BEBOB;
	    break;
	case ID_LATIN:
	    genre = LATIN;
	    break;
	case ID_REVIVAL:
	    genre = REVIVAL;
	    break;
	case ID_CELTIC:
	    genre = CELTIC;
	    break;
	case ID_BLUEGRASS:
	    genre = BLUEGRASS;
	    break;
	case ID_AVANTGARDE:
	    genre = AVANTGARDE;
	    break;
	case ID_GOTHIC_ROCK:
	    genre = GOTHIC_ROCK;
	    break;
	case ID_PROGRESSIVE_ROCK:
	    genre = PROGRESSIVE_ROCK;
	    break;
	case ID_PSYCHADELIC_ROCK:
	    genre = PSYCHADELIC_ROCK;
	    break;
	case ID_SYMPHONIC_ROCK:
	    genre = SYMPHONIC_ROCK;
	    break;
	case ID_SLOW_ROCK:
	    genre = SLOW_ROCK;
	    break;
	case ID_BIG_BAND:
	    genre = BIG_BAND;
	    break;
	case ID_CHORUS:
	    genre = CHORUS;
	    break;
	case ID_EASY_LISTENING:
	    genre = EASY_LISTENING;
	    break;
	case ID_ACOUSTIC:
	    genre = ACOUSTIC;
	    break;
	case ID_HUMOUR:
	    genre = HUMOUR;
	    break;
	case ID_SPEECH:
	    genre = SPEECH;
	    break;
	case ID_CHANSON:
	    genre = CHANSON;
	    break;
	case ID_OPERA:
	    genre = OPERA;
	    break;
	case ID_CHAMBER_MUSIC:
	    genre = CHAMBER_MUSIC;
	    break;
	case ID_SONATA:
	    genre = SONATA;
	    break;
	case ID_SYMPHONY:
	    genre = SYMPHONY;
	    break;
	case ID_BOOTY_BASS:
	    genre = BOOTY_BASS;
	    break;
	case ID_PRIMUS:
	    genre = PRIMUS;
	    break;
	case ID_PORN_GROOVE:
	    genre = PORN_GROOVE;
	    break;
	case ID_SATIRE:
	    genre = SATIRE;
	    break;
	case ID_SLOW_JAM:
	    genre = SLOW_JAM;
	    break;
	case ID_CLUB:
	    genre = CLUB;
	    break;
	case ID_TANGO:
	    genre = TANGO;
	    break;
	case ID_SAMBA:
	    genre = SAMBA;
	    break;
	case ID_FOLKLORE:
	    genre = FOLKLORE;
	    break;
	case ID_BALLAD:
	    genre = BALLAD;
	    break;
	case ID_POWER_BALLAD:
	    genre = POWER_BALLAD;
	    break;
	case ID_RHYTHMIC_SOUL:
	    genre = RHYTHMIC_SOUL;
	    break;
	case ID_FREESTYLE:
	    genre = FREESTYLE;
	    break;
	case ID_DUET:
	    genre = DUET;
	    break;
	case ID_PUNK_ROCK:
	    genre = PUNK_ROCK;
	    break;
	case ID_DRUM_SOLO:
	    genre = DRUM_SOLO;
	    break;
	case ID_A_CAPELLA:
	    genre = A_CAPELLA;
	    break;
	case ID_EURO_HOUSE:
	    genre = EURO_HOUSE;
	    break;
	case ID_DANCE_HALL:
	    genre = DANCE_HALL;
	    break;
        case ID_GOA:
	    genre = GOA;
	    break;
	case ID_DRUM_AND_BASS:
	    genre = DRUM_AND_BASS;
	    break;
	case ID_CLUB_HOUSE:
	    genre = CLUB_HOUSE;
	    break;
	case ID_HARDCORE_TECHNO:
	    genre = HARDCORE_TECHNO;
	    break;
	case ID_TERROR:
	    genre = TERROR;
	    break;
	case ID_INDIE:
	    genre = INDIE;
	    break;
	case ID_BRITPOP:
	    genre = BRITPOP;
	    break;
	case ID_NEGERPUNK:
	    genre = NEGERPUNK;
	    break;
	case ID_POLSK_PUNK:
	    genre = POLSK_PUNK;
	    break;
	case ID_BEAT:
	    genre = BEAT;
	    break;
	case ID_CHRISTIAN_GANGSTA_RAP:
	    genre = CHRISTIAN_GANGSTA_RAP;
	    break;
	case ID_HEAVY_METAL:
	    genre = HEAVY_METAL;
	    break;
	case ID_BLACK_METAL:
	    genre = BLACK_METAL;
	    break;
	case ID_CROSSOVER:
	    genre = CROSSOVER;
	    break;
	case ID_CONTEMPORARY_CHRISTIAN:
	    genre = CONTEMPORARY_CHRISTIAN;
	    break;
	case ID_CHRISTIAN_ROCK:
	    genre = CHRISTIAN_ROCK;
	    break;
	case ID_MERENGUE:
	    genre = MERENGUE;
	    break;
	case ID_SALSA:
	    genre = SALSA;
	    break;
	case ID_THRASH_METAL:
	    genre = THRASH_METAL;
	    break;
	case ID_ANIME:
	    genre = ANIME;
	    break;
	case ID_JPOP:
	    genre = JPOP;
	    break;
	case ID_SYNTHPOP:
	    genre = SYNTHPOP;
	    break;
	case ID_ABSTRACT:
	    genre = ABSTRACT;
	    break;
	case ID_ART_ROCK:
	    genre = ART_ROCK;
	    break;
	case ID_BAROQUE:
	    genre = BAROQUE;
	    break;
	case ID_BHANGRA:
	    genre = BHANGRA;
	    break;
	case ID_BIG_BEAT:
	    genre = BIG_BEAT;
	    break;
	case ID_BREAKBEAT:
	    genre = BREAKBEAT;
	    break;
	case ID_CHILLOUT:
	    genre = CHILLOUT;
	    break;
	case ID_DOWNTEMPO:
	    genre = DOWNTEMPO;
	    break;
	case ID_DUB:
	    genre = DUB;
	    break;
	case ID_EBM:
	    genre = EBM;
	    break;
	case ID_ECLECTIC:
	    genre = ECLECTIC;
	    break;
	case ID_ELECTRO:
	    genre = ELECTRO;
	    break;
	case ID_ELECTROCLASH:
	    genre = ELECTROCLASH;
	    break;
	case ID_EMO:
	    genre = EMO;
	    break;
	case ID_EXPERIMENTAL:
	    genre = EXPERIMENTAL;
	    break;
	case ID_GARAGE:
	    genre = GARAGE;
	    break;
	case ID_GLOBAL:
	    genre = GLOBAL;
	    break;
	case ID_IDM:
	    genre = IDM;
	    break;
	case ID_ILLBIENT:
	    genre = ILLBIENT;
	    break;
	case ID_INDUSTRO_GOTH:
	    genre = INDUSTRO_GOTH;
	    break;
	case ID_JAM_BAND:
	    genre = JAM_BAND;
	    break;
	case ID_KRAUTROCK:
	    genre = KRAUTROCK;
	    break;
	case ID_LEFTFIELD:
	    genre = LEFTFIELD;
	    break;
	case ID_LOUNGE:
	    genre = LOUNGE;
	    break;
	case ID_MATH_ROCK:
	    genre = MATH_ROCK;
	    break;
	case ID_NEW_ROMANTIC:
	    genre = NEW_ROMANTIC;
	    break;
	case ID_NU_BREAKZ:
	    genre = NU_BREAKZ;
	    break;
	case ID_POST_PUNK:
	    genre = POST_PUNK;
	    break;
	case ID_POST_ROCK:
	    genre = POST_ROCK;
	    break;
	case ID_PSYTRANCE:
	    genre = PSYTRANCE;
	    break;
	case ID_SHOEGAZE:
	    genre = SHOEGAZE;
	    break;
	case ID_SPACE_ROCK:
	    genre = SPACE_ROCK;
	    break;
	case ID_TROP_ROCK:
	    genre = TROP_ROCK;
	    break;
	case ID_WORLD_MUSIC:
	    genre = WORLD_MUSIC;
	    break;
	case ID_NEOCLASSICAL:
	    genre = NEOCLASSICAL;
	    break;
	case ID_AUDIOBOOK:
	    genre = AUDIOBOOK;
	    break;
	case ID_AUDIO_THEATRE:
	    genre = AUDIO_THEATRE;
	    break;
	case ID_NEUE_DEUTSCHE_WELLE:
	    genre = NEUE_DEUTSCHE_WELLE;
	    break;
	case ID_PODCAST:
	    genre = PODCAST;
	    break;
	case ID_INDIE_ROCK:
	    genre = INDIE_ROCK;
	    break;
	case ID_G_FUNK:
	    genre = G_FUNK;
	    break;
	case ID_DUBSTEP:
	    genre = DUBSTEP;
	    break;
	case ID_GARAGE_ROCK:
	    genre = GARAGE_ROCK;
	    break;
	case ID_PSYBIENT:
	    genre = PSYBIENT;
	    break;
    }

    return genre;
}

// String functions
int has_bom(uint16_t* string)
{
    if(memcmp("\xFF\xFE", string, 2) == 0 || memcmp("\xFE\xFF", string, 2) == 0)
    {   
        return 1;
    }
    
    return 0;
}

uint16_t* char_to_utf16(char* string, int size)
{
    uint16_t* result = (uint16_t*) malloc(size * sizeof(uint16_t));
    memcpy(result, string, size);
    return result;
}

void println_utf16(uint16_t* string, int size)
{
    int i = 1; // Skip the BOM
    while(1)
    {
        if(size > 0 && i > size)
        {
            break;
        }
        
        if(string[i] == 0x0000)
        {
            break;
        }
        
        printf("%lc", string[i]);
        i++;
    }
    printf("\n");
}

char* get_path_to_file(const char* file)
{
    char* file_name = strrchr(file, '/');
    unsigned long size = strlen(file) - strlen(file_name) + 1; // 1 = trailing '/'
    
    char* file_path = (char*) malloc(size * sizeof(char));
    strncpy(file_path, file, size);
    
    return file_path;
}
