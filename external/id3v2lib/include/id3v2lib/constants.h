/*
 * This file is part of the id3v2lib library
 *
 * Copyright (c) 2013, Lorenzo Ruiz
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef id3v2lib_constants_h
#define id3v2lib_constants_h

/**
 * TAG_HEADER CONSTANTS
 */
#define ID3_HEADER 10
#define ID3_HEADER_TAG 3
#define ID3_HEADER_VERSION 1
#define ID3_HEADER_REVISION 1
#define ID3_HEADER_FLAGS 1
#define ID3_HEADER_SIZE 4
#define ID3_EXTENDED_HEADER_SIZE 4

#define NO_COMPATIBLE_TAG 0
#define ID3v23  1
#define ID3v24  2
// END TAG_HEADER CONSTANTS

/**
 * TAG_FRAME CONSTANTS
 */
#define ID3_FRAME 10
#define ID3_FRAME_ID 4
#define ID3_FRAME_SIZE 4
#define ID3_FRAME_FLAGS 2
#define ID3_FRAME_ENCODING 1
#define ID3_FRAME_LANGUAGE 3
#define ID3_FRAME_SHORT_DESCRIPTION 1

#define INVALID_FRAME 0
#define TEXT_FRAME 1
#define COMMENT_FRAME 2
#define APIC_FRAME 3

#define ISO_ENCODING 0
#define UTF_16_ENCODING 1
// END TAG_FRAME CONSTANTS

/**
 * FRAME IDs
 */
#define TITLE_FRAME_ID "TIT2"
#define ARTIST_FRAME_ID "TPE1"
#define ALBUM_FRAME_ID "TALB"
#define ALBUM_ARTIST_FRAME_ID "TPE2"
#define GENRE_FRAME_ID "TCON"
#define TRACK_FRAME_ID "TRCK"
#define YEAR_FRAME_ID "TYER"
#define COMMENT_FRAME_ID "COMM"
#define DISC_NUMBER_FRAME_ID "TPOS"
#define COMPOSER_FRAME_ID "TCOM"
#define ALBUM_COVER_FRAME_ID "APIC"
// END FRAME IDs


/**
 * APIC FRAME CONSTANTS
 */
#define ID3_FRAME_PICTURE_TYPE 1
#define JPG_MIME_TYPE "image/jpeg"
#define PNG_MIME_TYPE "image/png"

// Picture types:
#define OTHER 0x00
#define FILE_ICON 0x01
#define OTHER_FILE_ICON 0x02
#define FRONT_COVER 0x03
#define BACK_COVER 0x04
#define LEAFLET_PAGE 0x05
#define MEDIA 0x06
#define LEAD_ARTIST 0x07
#define ARTIST 0x08
#define CONDUCTOR 0x09
#define BAND 0x0A
#define COMPOSER 0x0B
#define LYRICIST 0x0C
#define RECORDING_LOCATION 0x0D
#define DURING_RECORDING 0x0E
#define DURING_PERFORMANCE 0x0F
#define VIDEO_SCREEN_CAPTURE 0x10
#define A_BRIGHT_COLOURED_FISH 0x11
#define ILLUSTRATION 0x12
#define ARTIST_LOGOTYPE 0x13
#define PUBLISHER_LOGOTYPE 0x14
// END APIC FRAME CONSTANTS

// Genres:
#define ID_BLUES 0
#define ID_CLASSIC_ROCK 1
#define ID_COUNTRY 2
#define ID_DANCE 3
#define ID_DISCO 4
#define ID_FUNK 5
#define ID_GRUNGE 6
#define ID_HIP_HOP 7
#define ID_JAZZ 8
#define ID_METAL 9
#define ID_NEW_AGE 10
#define ID_OLDIES 11
#define ID_OTHER_GENRE 12
#define ID_POP 13
#define ID_R_AND_B 14
#define ID_RAP 15
#define ID_REGGAE 16
#define ID_ROCK 17
#define ID_TECHNO 18
#define ID_INDUSTRIAL 19
#define ID_ALTERNATIVE 20
#define ID_SKA 21
#define ID_DEATH_METAL 22
#define ID_PRANKS 23
#define ID_SOUNDTRACK 24
#define ID_EURO_TECHNO 25
#define ID_AMBIENT 26
#define ID_TRIP_HOP 27
#define ID_VOCAL 28
#define ID_JAZZ_FUNK 29
#define ID_FUSION 30
#define ID_TRANCE 31
#define ID_CLASSICAL 32
#define ID_INSTRUMENTAL 33
#define ID_ACID 34
#define ID_HOUSE 35
#define ID_GAME 36
#define ID_SOUND_CLIP 37
#define ID_GOSPEL 38
#define ID_NOISE 39
#define ID_ALTERNATIVE_ROCK 40
#define ID_BASS 41
#define ID_SOUL 42
#define ID_PUNK 43
#define ID_SPACE 44
#define ID_MEDITATIVE 45
#define ID_INSTRUMENTAL_POP 46
#define ID_INSTRUMENTAL_ROCK 47
#define ID_ETHIC 48
#define ID_GOTHIC 49
#define ID_DARKWAVE 50
#define ID_TECHNO_INDUSTRIAL 51
#define ID_ELECTRONIC 52
#define ID_POP_FOLK 53
#define ID_EURODANCE 54
#define ID_DREAM 55
#define ID_SOUTHERN_ROCK 56
#define ID_COMEDY 57
#define ID_CULT 58
#define ID_GANGSTA 59
#define ID_TOP_FOURTY 60
#define ID_CHRISTIAN_RAP 61
#define ID_POP_FUNK 62
#define ID_JUNGLE 63
#define ID_NATIVE_AMERICAN 64
#define ID_CABARET 65
#define ID_NEW_WAVE 66
#define ID_PSYCHADELIC 67
#define ID_RAVE 68
#define ID_SHOWTUNES 69
#define ID_TRAILER 70
#define ID_LO_FI 71
#define ID_TRIBAL 72
#define ID_ACID_PUNK 73
#define ID_ACID_JAZZ 74
#define ID_POLKA 75
#define ID_RETRO 76
#define ID_MUSICAL 77
#define ID_ROCK_AND_ROLL 78
#define ID_HARD_ROCK 79
#define ID_FOLK 80
#define ID_FOLK_ROCK 81
#define ID_NATIONAL_FOLK 82
#define ID_SWING 83
#define ID_FAST_FUSION 84
#define ID_BEBOB 85
#define ID_LATIN 86
#define ID_REVIVAL 87
#define ID_CELTIC 88
#define ID_BLUEGRASS 89
#define ID_AVANTGARDE 90
#define ID_GOTHIC_ROCK 91
#define ID_PROGRESSIVE_ROCK 92
#define ID_PSYCHADELIC_ROCK 93
#define ID_SYMPHONIC_ROCK 94
#define ID_SLOW_ROCK 95
#define ID_BIG_BAND 96
#define ID_CHORUS 97
#define ID_EASY_LISTENING 98
#define ID_ACOUSTIC 99
#define ID_HUMOUR 100
#define ID_SPEECH 101
#define ID_CHANSON 102
#define ID_OPERA 103
#define ID_CHAMBER_MUSIC 104
#define ID_SONATA 105
#define ID_SYMPHONY 106
#define ID_BOOTY_BASS 107
#define ID_PRIMUS 108
#define ID_PORN_GROOVE 109
#define ID_SATIRE 110
#define ID_SLOW_JAM 111
#define ID_CLUB 112
#define ID_TANGO 113
#define ID_SAMBA 114
#define ID_FOLKLORE 115
#define ID_BALLAD 116
#define ID_POWER_BALLAD 117
#define ID_RHYTHMIC_SOUL 118
#define ID_FREESTYLE 119
#define ID_DUET 120
#define ID_PUNK_ROCK 121
#define ID_DRUM_SOLO 122
#define ID_A_CAPELLA 123
#define ID_EURO_HOUSE 124
#define ID_DANCE_HALL 125
#define ID_GOA 126
#define ID_DRUM_AND_BASS 127
#define ID_CLUB_HOUSE 128
#define ID_HARDCORE_TECHNO 129
#define ID_TERROR 130
#define ID_INDIE 131
#define ID_BRITPOP 132
#define ID_NEGERPUNK 133
#define ID_POLSK_PUNK 134
#define ID_BEAT 135
#define ID_CHRISTIAN_GANGSTA_RAP 136
#define ID_HEAVY_METAL 137
#define ID_BLACK_METAL 138
#define ID_CROSSOVER 139
#define ID_CONTEMPORARY_CHRISTIAN 140
#define ID_CHRISTIAN_ROCK 141
#define ID_MERENGUE 142
#define ID_SALSA 143
#define ID_THRASH_METAL 144
#define ID_ANIME 145
#define ID_JPOP 146
#define ID_SYNTHPOP 147
#define ID_ABSTRACT 148
#define ID_ART_ROCK 149
#define ID_BAROQUE 150
#define ID_BHANGRA 151
#define ID_BIG_BEAT 152
#define ID_BREAKBEAT 153
#define ID_CHILLOUT 154
#define ID_DOWNTEMPO 155
#define ID_DUB 156
#define ID_EBM 157
#define ID_ECLECTIC 158
#define ID_ELECTRO 159
#define ID_ELECTROCLASH 160
#define ID_EMO 161
#define ID_EXPERIMENTAL 162
#define ID_GARAGE 163
#define ID_GLOBAL 164
#define ID_IDM 165
#define ID_ILLBIENT 166
#define ID_INDUSTRO_GOTH 167
#define ID_JAM_BAND 168
#define ID_KRAUTROCK 169
#define ID_LEFTFIELD 170
#define ID_LOUNGE 171
#define ID_MATH_ROCK 172
#define ID_NEW_ROMANTIC 173
#define ID_NU_BREAKZ 174
#define ID_POST_PUNK 175
#define ID_POST_ROCK 176
#define ID_PSYTRANCE 177
#define ID_SHOEGAZE 178
#define ID_SPACE_ROCK 179
#define ID_TROP_ROCK 180
#define ID_WORLD_MUSIC 181
#define ID_NEOCLASSICAL 182
#define ID_AUDIOBOOK 183
#define ID_AUDIO_THEATRE 184
#define ID_NEUE_DEUTSCHE_WELLE 185
#define ID_PODCAST 186
#define ID_INDIE_ROCK 187
#define ID_G_FUNK 188
#define ID_DUBSTEP 189
#define ID_GARAGE_ROCK 190
#define ID_PSYBIENT 191

#define BLUES "Blues"
#define CLASSIC_ROCK "Classic Rock"
#define COUNTRY "Country"
#define DANCE "Dance"
#define DISCO "Disco"
#define FUNK "Funk"
#define GRUNGE "Grunge"
#define HIP_HOP "Hip-Hop"
#define JAZZ "Jazz"
#define METAL "Metal"
#define NEW_AGE "New Age"
#define OLDIES "Oldies"
#define OTHER_GENRE "Other"
#define POP "Pop"
#define R_AND_B "R&B"
#define RAP "Rap"
#define REGGAE "Reggae"
#define ROCK "Rock"
#define TECHNO "Techno"
#define INDUSTRIAL "Industrial"
#define ALTERNATIVE "Alternative"
#define SKA "Ska"
#define DEATH_METAL "Death Metal"
#define PRANKS "Pranks"
#define SOUNDTRACK "Sound Track"
#define EURO_TECHNO "Euro-Techno"
#define AMBIENT "Ambient"
#define TRIP_HOP "Trip-Hop"
#define VOCAL "Vocal"
#define JAZZ_FUNK "Jazz Funk"
#define FUSION "Fusion"
#define TRANCE "Trance"
#define CLASSICAL "Classical"
#define INSTRUMENTAL "Instrumental"
#define ACID "Acid"
#define HOUSE "House"
#define GAME "Game"
#define SOUND_CLIP "Sound Clip"
#define GOSPEL "Gospel"
#define NOISE "Noise"
#define ALTERNATIVE_ROCK "Alternative Rock"
#define BASS "Bass"
#define SOUL "Soul"
#define PUNK "Punk"
#define SPACE "Space"
#define MEDITATIVE "Meditative"
#define INSTRUMENTAL_POP "Instrumental Pop"
#define INSTRUMENTAL_ROCK "Instrumental Rock"
#define ETHIC "Ethic"
#define GOTHIC "Gothic"
#define DARKWAVE "Dark Wave"
#define TECHNO_INDUSTRIAL "Techno-Industrial"
#define ELECTRONIC "Electronic"
#define POP_FOLK "Pop-Folk"
#define EURODANCE "Eurodance"
#define DREAM "Dream"
#define SOUTHERN_ROCK "Southern Rock"
#define COMEDY "Comedy"
#define CULT "Cult"
#define GANGSTA "Gangsta"
#define TOP_FOURTY "Top 40"
#define CHRISTIAN_RAP "Christian Rap"
#define POP_FUNK "Pop-Funk"
#define JUNGLE "Jungle"
#define NATIVE_AMERICAN "Native American"
#define CABARET "Cabaret"
#define NEW_WAVE "New Wave"
#define PSYCHADELIC "Psychadelic"
#define RAVE "Rave"
#define SHOWTUNES "Showtunes"
#define TRAILER "Trailer"
#define LO_FI "Lo-Fi"
#define TRIBAL "Tribal"
#define ACID_PUNK "Acid Punk"
#define ACID_JAZZ "Acid Jazz"
#define POLKA "Polka"
#define RETRO "Retro"
#define MUSICAL "Musical"
#define ROCK_AND_ROLL "Rock n' Roll"
#define HARD_ROCK "Hard Rock"
#define FOLK "Folk"
#define FOLK_ROCK "Folk-Rock"
#define NATIONAL_FOLK "National Folk"
#define SWING "Swing"
#define FAST_FUSION "Fast Fusion"
#define BEBOB "Bebob"
#define LATIN "Latin"
#define REVIVAL "Revival"
#define CELTIC "Celtic"
#define BLUEGRASS "Bluegrass"
#define AVANTGARDE "Avantgarde"
#define GOTHIC_ROCK "Gothic Rock"
#define PROGRESSIVE_ROCK "Progressive Rock"
#define PSYCHADELIC_ROCK "Psychadelic Rock"
#define SYMPHONIC_ROCK "Symphonic Rock"
#define SLOW_ROCK "Slow Rock"
#define BIG_BAND "Big Band"
#define CHORUS "Chorus"
#define EASY_LISTENING "Easy Listening"
#define ACOUSTIC "Acoustic"
#define HUMOUR "Humour"
#define SPEECH "Speech"
#define CHANSON "Chanson"
#define OPERA "Opera"
#define CHAMBER_MUSIC "Chamber Music"
#define SONATA "Sonata"
#define SYMPHONY "Symphony"
#define BOOTY_BASS "Booty Bass"
#define PRIMUS "Primus"
#define PORN_GROOVE "Porn Groove"
#define SATIRE "Satire"
#define SLOW_JAM "Slow Jam"
#define CLUB "Club"
#define TANGO "Tango"
#define SAMBA "Samba"
#define FOLKLORE "Folklore"
#define BALLAD "Ballad"
#define POWER_BALLAD "Power Ballad"
#define RHYTHMIC_SOUL "Rhythmic Soul"
#define FREESTYLE "Freestyle"
#define DUET "Duet"
#define PUNK_ROCK "Punk Rock"
#define DRUM_SOLO "Drum Solo"
#define A_CAPELLA "A Capella"
#define EURO_HOUSE "Euro-House"
#define DANCE_HALL "Dance Hall"
#define GOA "Goa"
#define DRUM_AND_BASS "Drum & Bass"
#define CLUB_HOUSE "Club-House"
#define HARDCORE_TECHNO "Hardcore Techno"
#define TERROR "Terror"
#define INDIE "Indie"
#define BRITPOP "BritPop"
#define NEGERPUNK "Negerpunk"
#define POLSK_PUNK "Polsk Punk"
#define BEAT "Beat"
#define CHRISTIAN_GANGSTA_RAP "Christian Gangsta Rap"
#define HEAVY_METAL "Heavy Metal"
#define BLACK_METAL "Black Metal"
#define CROSSOVER "Crossover"
#define CONTEMPORARY_CHRISTIAN "Contemporary Christian"
#define CHRISTIAN_ROCK "Christian rock"
#define MERENGUE "Merengue"
#define SALSA "Salsa"
#define THRASH_METAL "Thrash Metal"
#define ANIME "Anime"
#define JPOP "Jpop"
#define SYNTHPOP "Synthpop"
#define ABSTRACT "Abstract"
#define ART_ROCK "Art Rock"
#define BAROQUE "Baroque"
#define BHANGRA "Bhangra"
#define BIG_BEAT "Big beat"
#define BREAKBEAT "Breakbeat"
#define CHILLOUT "Chillout"
#define DOWNTEMPO "Downtempo"
#define DUB "Dub"
#define EBM "EBM"
#define ECLECTIC "Eclectic"
#define ELECTRO "Electro"
#define ELECTROCLASH "Electroclash"
#define EMO "Emo"
#define EXPERIMENTAL "Experimental"
#define GARAGE "Garage"
#define GLOBAL "Global"
#define IDM "IDM"
#define ILLBIENT "Illbient"
#define INDUSTRO_GOTH "Industro-Goth"
#define JAM_BAND "Jam Band"
#define KRAUTROCK "Krautrock"
#define LEFTFIELD "Leftfield"
#define LOUNGE "Lounge"
#define MATH_ROCK "Math Rock"
#define NEW_ROMANTIC "New Romantic"
#define NU_BREAKZ "Nu-Breakz"
#define POST_PUNK "Post-Punk"
#define POST_ROCK "Post-Rock"
#define PSYTRANCE "Psytrance"
#define SHOEGAZE "Shoegaze"
#define SPACE_ROCK "Space Rock"
#define TROP_ROCK "Trop Rock"
#define WORLD_MUSIC "World Music"
#define NEOCLASSICAL "Neoclassical"
#define AUDIOBOOK "Audiobook"
#define AUDIO_THEATRE "Audio theatre"
#define NEUE_DEUTSCHE_WELLE "Neue Deutsche Welle"
#define PODCAST "Podcast"
#define INDIE_ROCK "Indie-Rock"
#define G_FUNK "G-Funk"
#define DUBSTEP "Dubstep"
#define GARAGE_ROCK "Garage Rock"
#define PSYBIENT "Psybient"

#endif
