#include "utils/StringUtil.h"

#include <algorithm>
#include <stdarg.h>
#include <cstring>

#if defined(_WIN32)
#include <Windows.h>
#include <ctype.h>
#else
#include <unistd.h>
#endif

#include "han.h"

namespace Utils
{
	namespace String
	{
		static wchar_t unicode_lowers[] =
		{
			(wchar_t)0x0061, (wchar_t)0x0062, (wchar_t)0x0063, (wchar_t)0x0064, (wchar_t)0x0065, (wchar_t)0x0066, (wchar_t)0x0067, (wchar_t)0x0068, (wchar_t)0x0069,
			(wchar_t)0x006A, (wchar_t)0x006B, (wchar_t)0x006C, (wchar_t)0x006D, (wchar_t)0x006E, (wchar_t)0x006F, (wchar_t)0x0070, (wchar_t)0x0071, (wchar_t)0x0072,
			(wchar_t)0x0073, (wchar_t)0x0074, (wchar_t)0x0075, (wchar_t)0x0076, (wchar_t)0x0077, (wchar_t)0x0078, (wchar_t)0x0079, (wchar_t)0x007A, (wchar_t)0x00E0,
			(wchar_t)0x00E1, (wchar_t)0x00E2, (wchar_t)0x00E3, (wchar_t)0x00E4, (wchar_t)0x00E5, (wchar_t)0x00E6, (wchar_t)0x00E7, (wchar_t)0x00E8, (wchar_t)0x00E9,
			(wchar_t)0x00EA, (wchar_t)0x00EB, (wchar_t)0x00EC, (wchar_t)0x00ED, (wchar_t)0x00EE, (wchar_t)0x00EF, (wchar_t)0x00F0, (wchar_t)0x00F1, (wchar_t)0x00F2,
			(wchar_t)0x00F3, (wchar_t)0x00F4, (wchar_t)0x00F5, (wchar_t)0x00F6, (wchar_t)0x00F8, (wchar_t)0x00F9, (wchar_t)0x00FA, (wchar_t)0x00FB, (wchar_t)0x00FC,
			(wchar_t)0x00FD, (wchar_t)0x00FE, (wchar_t)0x00FF, (wchar_t)0x0101, (wchar_t)0x0103, (wchar_t)0x0105, (wchar_t)0x0107, (wchar_t)0x0109, (wchar_t)0x010B,
			(wchar_t)0x010D, (wchar_t)0x010F, (wchar_t)0x0111, (wchar_t)0x0113, (wchar_t)0x0115, (wchar_t)0x0117, (wchar_t)0x0119, (wchar_t)0x011B, (wchar_t)0x011D,
			(wchar_t)0x011F, (wchar_t)0x0121, (wchar_t)0x0123, (wchar_t)0x0125, (wchar_t)0x0127, (wchar_t)0x0129, (wchar_t)0x012B, (wchar_t)0x012D, (wchar_t)0x012F,
			(wchar_t)0x0131, (wchar_t)0x0133, (wchar_t)0x0135, (wchar_t)0x0137, (wchar_t)0x013A, (wchar_t)0x013C, (wchar_t)0x013E, (wchar_t)0x0140, (wchar_t)0x0142,
			(wchar_t)0x0144, (wchar_t)0x0146, (wchar_t)0x0148, (wchar_t)0x014B, (wchar_t)0x014D, (wchar_t)0x014F, (wchar_t)0x0151, (wchar_t)0x0153, (wchar_t)0x0155,
			(wchar_t)0x0157, (wchar_t)0x0159, (wchar_t)0x015B, (wchar_t)0x015D, (wchar_t)0x015F, (wchar_t)0x0161, (wchar_t)0x0163, (wchar_t)0x0165, (wchar_t)0x0167,
			(wchar_t)0x0169, (wchar_t)0x016B, (wchar_t)0x016D, (wchar_t)0x016F, (wchar_t)0x0171, (wchar_t)0x0173, (wchar_t)0x0175, (wchar_t)0x0177, (wchar_t)0x017A,
			(wchar_t)0x017C, (wchar_t)0x017E, (wchar_t)0x0183, (wchar_t)0x0185, (wchar_t)0x0188, (wchar_t)0x018C, (wchar_t)0x0192, (wchar_t)0x0199, (wchar_t)0x01A1,
			(wchar_t)0x01A3, (wchar_t)0x01A5, (wchar_t)0x01A8, (wchar_t)0x01AD, (wchar_t)0x01B0, (wchar_t)0x01B4, (wchar_t)0x01B6, (wchar_t)0x01B9, (wchar_t)0x01BD,
			(wchar_t)0x01C6, (wchar_t)0x01C9, (wchar_t)0x01CC, (wchar_t)0x01CE, (wchar_t)0x01D0, (wchar_t)0x01D2, (wchar_t)0x01D4, (wchar_t)0x01D6, (wchar_t)0x01D8,
			(wchar_t)0x01DA, (wchar_t)0x01DC, (wchar_t)0x01DF, (wchar_t)0x01E1, (wchar_t)0x01E3, (wchar_t)0x01E5, (wchar_t)0x01E7, (wchar_t)0x01E9, (wchar_t)0x01EB,
			(wchar_t)0x01ED, (wchar_t)0x01EF, (wchar_t)0x01F3, (wchar_t)0x01F5, (wchar_t)0x01FB, (wchar_t)0x01FD, (wchar_t)0x01FF, (wchar_t)0x0201, (wchar_t)0x0203,
			(wchar_t)0x0205, (wchar_t)0x0207, (wchar_t)0x0209, (wchar_t)0x020B, (wchar_t)0x020D, (wchar_t)0x020F, (wchar_t)0x0211, (wchar_t)0x0213, (wchar_t)0x0215,
			(wchar_t)0x0217, (wchar_t)0x0253, (wchar_t)0x0254, (wchar_t)0x0257, (wchar_t)0x0258, (wchar_t)0x0259, (wchar_t)0x025B, (wchar_t)0x0260, (wchar_t)0x0263,
			(wchar_t)0x0268, (wchar_t)0x0269, (wchar_t)0x026F, (wchar_t)0x0272, (wchar_t)0x0275, (wchar_t)0x0283, (wchar_t)0x0288, (wchar_t)0x028A, (wchar_t)0x028B,
			(wchar_t)0x0292, (wchar_t)0x03AC, (wchar_t)0x03AD, (wchar_t)0x03AE, (wchar_t)0x03AF, (wchar_t)0x03B1, (wchar_t)0x03B2, (wchar_t)0x03B3, (wchar_t)0x03B4,
			(wchar_t)0x03B5, (wchar_t)0x03B6, (wchar_t)0x03B7, (wchar_t)0x03B8, (wchar_t)0x03B9, (wchar_t)0x03BA, (wchar_t)0x03BB, (wchar_t)0x03BC, (wchar_t)0x03BD,
			(wchar_t)0x03BE, (wchar_t)0x03BF, (wchar_t)0x03C0, (wchar_t)0x03C1, (wchar_t)0x03C3, (wchar_t)0x03C4, (wchar_t)0x03C5, (wchar_t)0x03C6, (wchar_t)0x03C7,
			(wchar_t)0x03C8, (wchar_t)0x03C9, (wchar_t)0x03CA, (wchar_t)0x03CB, (wchar_t)0x03CC, (wchar_t)0x03CD, (wchar_t)0x03CE, (wchar_t)0x03E3, (wchar_t)0x03E5,
			(wchar_t)0x03E7, (wchar_t)0x03E9, (wchar_t)0x03EB, (wchar_t)0x03ED, (wchar_t)0x03EF, (wchar_t)0x0430, (wchar_t)0x0431, (wchar_t)0x0432, (wchar_t)0x0433,
			(wchar_t)0x0434, (wchar_t)0x0435, (wchar_t)0x0436, (wchar_t)0x0437, (wchar_t)0x0438, (wchar_t)0x0439, (wchar_t)0x043A, (wchar_t)0x043B, (wchar_t)0x043C,
			(wchar_t)0x043D, (wchar_t)0x043E, (wchar_t)0x043F, (wchar_t)0x0440, (wchar_t)0x0441, (wchar_t)0x0442, (wchar_t)0x0443, (wchar_t)0x0444, (wchar_t)0x0445,
			(wchar_t)0x0446, (wchar_t)0x0447, (wchar_t)0x0448, (wchar_t)0x0449, (wchar_t)0x044A, (wchar_t)0x044B, (wchar_t)0x044C, (wchar_t)0x044D, (wchar_t)0x044E,
			(wchar_t)0x044F, (wchar_t)0x0451, (wchar_t)0x0452, (wchar_t)0x0453, (wchar_t)0x0454, (wchar_t)0x0455, (wchar_t)0x0456, (wchar_t)0x0457, (wchar_t)0x0458,
			(wchar_t)0x0459, (wchar_t)0x045A, (wchar_t)0x045B, (wchar_t)0x045C, (wchar_t)0x045E, (wchar_t)0x045F, (wchar_t)0x0461, (wchar_t)0x0463, (wchar_t)0x0465,
			(wchar_t)0x0467, (wchar_t)0x0469, (wchar_t)0x046B, (wchar_t)0x046D, (wchar_t)0x046F, (wchar_t)0x0471, (wchar_t)0x0473, (wchar_t)0x0475, (wchar_t)0x0477,
			(wchar_t)0x0479, (wchar_t)0x047B, (wchar_t)0x047D, (wchar_t)0x047F, (wchar_t)0x0481, (wchar_t)0x0491, (wchar_t)0x0493, (wchar_t)0x0495, (wchar_t)0x0497,
			(wchar_t)0x0499, (wchar_t)0x049B, (wchar_t)0x049D, (wchar_t)0x049F, (wchar_t)0x04A1, (wchar_t)0x04A3, (wchar_t)0x04A5, (wchar_t)0x04A7, (wchar_t)0x04A9,
			(wchar_t)0x04AB, (wchar_t)0x04AD, (wchar_t)0x04AF, (wchar_t)0x04B1, (wchar_t)0x04B3, (wchar_t)0x04B5, (wchar_t)0x04B7, (wchar_t)0x04B9, (wchar_t)0x04BB,
			(wchar_t)0x04BD, (wchar_t)0x04BF, (wchar_t)0x04C2, (wchar_t)0x04C4, (wchar_t)0x04C8, (wchar_t)0x04CC, (wchar_t)0x04D1, (wchar_t)0x04D3, (wchar_t)0x04D5,
			(wchar_t)0x04D7, (wchar_t)0x04D9, (wchar_t)0x04DB, (wchar_t)0x04DD, (wchar_t)0x04DF, (wchar_t)0x04E1, (wchar_t)0x04E3, (wchar_t)0x04E5, (wchar_t)0x04E7,
			(wchar_t)0x04E9, (wchar_t)0x04EB, (wchar_t)0x04EF, (wchar_t)0x04F1, (wchar_t)0x04F3, (wchar_t)0x04F5, (wchar_t)0x04F9, (wchar_t)0x0561, (wchar_t)0x0562,
			(wchar_t)0x0563, (wchar_t)0x0564, (wchar_t)0x0565, (wchar_t)0x0566, (wchar_t)0x0567, (wchar_t)0x0568, (wchar_t)0x0569, (wchar_t)0x056A, (wchar_t)0x056B,
			(wchar_t)0x056C, (wchar_t)0x056D, (wchar_t)0x056E, (wchar_t)0x056F, (wchar_t)0x0570, (wchar_t)0x0571, (wchar_t)0x0572, (wchar_t)0x0573, (wchar_t)0x0574,
			(wchar_t)0x0575, (wchar_t)0x0576, (wchar_t)0x0577, (wchar_t)0x0578, (wchar_t)0x0579, (wchar_t)0x057A, (wchar_t)0x057B, (wchar_t)0x057C, (wchar_t)0x057D,
			(wchar_t)0x057E, (wchar_t)0x057F, (wchar_t)0x0580, (wchar_t)0x0581, (wchar_t)0x0582, (wchar_t)0x0583, (wchar_t)0x0584, (wchar_t)0x0585, (wchar_t)0x0586,
			(wchar_t)0x10D0, (wchar_t)0x10D1, (wchar_t)0x10D2, (wchar_t)0x10D3, (wchar_t)0x10D4, (wchar_t)0x10D5, (wchar_t)0x10D6, (wchar_t)0x10D7, (wchar_t)0x10D8,
			(wchar_t)0x10D9, (wchar_t)0x10DA, (wchar_t)0x10DB, (wchar_t)0x10DC, (wchar_t)0x10DD, (wchar_t)0x10DE, (wchar_t)0x10DF, (wchar_t)0x10E0, (wchar_t)0x10E1,
			(wchar_t)0x10E2, (wchar_t)0x10E3, (wchar_t)0x10E4, (wchar_t)0x10E5, (wchar_t)0x10E6, (wchar_t)0x10E7, (wchar_t)0x10E8, (wchar_t)0x10E9, (wchar_t)0x10EA,
			(wchar_t)0x10EB, (wchar_t)0x10EC, (wchar_t)0x10ED, (wchar_t)0x10EE, (wchar_t)0x10EF, (wchar_t)0x10F0, (wchar_t)0x10F1, (wchar_t)0x10F2, (wchar_t)0x10F3,
			(wchar_t)0x10F4, (wchar_t)0x10F5, (wchar_t)0x1E01, (wchar_t)0x1E03, (wchar_t)0x1E05, (wchar_t)0x1E07, (wchar_t)0x1E09, (wchar_t)0x1E0B, (wchar_t)0x1E0D,
			(wchar_t)0x1E0F, (wchar_t)0x1E11, (wchar_t)0x1E13, (wchar_t)0x1E15, (wchar_t)0x1E17, (wchar_t)0x1E19, (wchar_t)0x1E1B, (wchar_t)0x1E1D, (wchar_t)0x1E1F,
			(wchar_t)0x1E21, (wchar_t)0x1E23, (wchar_t)0x1E25, (wchar_t)0x1E27, (wchar_t)0x1E29, (wchar_t)0x1E2B, (wchar_t)0x1E2D, (wchar_t)0x1E2F, (wchar_t)0x1E31,
			(wchar_t)0x1E33, (wchar_t)0x1E35, (wchar_t)0x1E37, (wchar_t)0x1E39, (wchar_t)0x1E3B, (wchar_t)0x1E3D, (wchar_t)0x1E3F, (wchar_t)0x1E41, (wchar_t)0x1E43,
			(wchar_t)0x1E45, (wchar_t)0x1E47, (wchar_t)0x1E49, (wchar_t)0x1E4B, (wchar_t)0x1E4D, (wchar_t)0x1E4F, (wchar_t)0x1E51, (wchar_t)0x1E53, (wchar_t)0x1E55,
			(wchar_t)0x1E57, (wchar_t)0x1E59, (wchar_t)0x1E5B, (wchar_t)0x1E5D, (wchar_t)0x1E5F, (wchar_t)0x1E61, (wchar_t)0x1E63, (wchar_t)0x1E65, (wchar_t)0x1E67,
			(wchar_t)0x1E69, (wchar_t)0x1E6B, (wchar_t)0x1E6D, (wchar_t)0x1E6F, (wchar_t)0x1E71, (wchar_t)0x1E73, (wchar_t)0x1E75, (wchar_t)0x1E77, (wchar_t)0x1E79,
			(wchar_t)0x1E7B, (wchar_t)0x1E7D, (wchar_t)0x1E7F, (wchar_t)0x1E81, (wchar_t)0x1E83, (wchar_t)0x1E85, (wchar_t)0x1E87, (wchar_t)0x1E89, (wchar_t)0x1E8B,
			(wchar_t)0x1E8D, (wchar_t)0x1E8F, (wchar_t)0x1E91, (wchar_t)0x1E93, (wchar_t)0x1E95, (wchar_t)0x1EA1, (wchar_t)0x1EA3, (wchar_t)0x1EA5, (wchar_t)0x1EA7,
			(wchar_t)0x1EA9, (wchar_t)0x1EAB, (wchar_t)0x1EAD, (wchar_t)0x1EAF, (wchar_t)0x1EB1, (wchar_t)0x1EB3, (wchar_t)0x1EB5, (wchar_t)0x1EB7, (wchar_t)0x1EB9,
			(wchar_t)0x1EBB, (wchar_t)0x1EBD, (wchar_t)0x1EBF, (wchar_t)0x1EC1, (wchar_t)0x1EC3, (wchar_t)0x1EC5, (wchar_t)0x1EC7, (wchar_t)0x1EC9, (wchar_t)0x1ECB,
			(wchar_t)0x1ECD, (wchar_t)0x1ECF, (wchar_t)0x1ED1, (wchar_t)0x1ED3, (wchar_t)0x1ED5, (wchar_t)0x1ED7, (wchar_t)0x1ED9, (wchar_t)0x1EDB, (wchar_t)0x1EDD,
			(wchar_t)0x1EDF, (wchar_t)0x1EE1, (wchar_t)0x1EE3, (wchar_t)0x1EE5, (wchar_t)0x1EE7, (wchar_t)0x1EE9, (wchar_t)0x1EEB, (wchar_t)0x1EED, (wchar_t)0x1EEF,
			(wchar_t)0x1EF1, (wchar_t)0x1EF3, (wchar_t)0x1EF5, (wchar_t)0x1EF7, (wchar_t)0x1EF9, (wchar_t)0x1F00, (wchar_t)0x1F01, (wchar_t)0x1F02, (wchar_t)0x1F03,
			(wchar_t)0x1F04, (wchar_t)0x1F05, (wchar_t)0x1F06, (wchar_t)0x1F07, (wchar_t)0x1F10, (wchar_t)0x1F11, (wchar_t)0x1F12, (wchar_t)0x1F13, (wchar_t)0x1F14,
			(wchar_t)0x1F15, (wchar_t)0x1F20, (wchar_t)0x1F21, (wchar_t)0x1F22, (wchar_t)0x1F23, (wchar_t)0x1F24, (wchar_t)0x1F25, (wchar_t)0x1F26, (wchar_t)0x1F27,
			(wchar_t)0x1F30, (wchar_t)0x1F31, (wchar_t)0x1F32, (wchar_t)0x1F33, (wchar_t)0x1F34, (wchar_t)0x1F35, (wchar_t)0x1F36, (wchar_t)0x1F37, (wchar_t)0x1F40,
			(wchar_t)0x1F41, (wchar_t)0x1F42, (wchar_t)0x1F43, (wchar_t)0x1F44, (wchar_t)0x1F45, (wchar_t)0x1F51, (wchar_t)0x1F53, (wchar_t)0x1F55, (wchar_t)0x1F57,
			(wchar_t)0x1F60, (wchar_t)0x1F61, (wchar_t)0x1F62, (wchar_t)0x1F63, (wchar_t)0x1F64, (wchar_t)0x1F65, (wchar_t)0x1F66, (wchar_t)0x1F67, (wchar_t)0x1F80,
			(wchar_t)0x1F81, (wchar_t)0x1F82, (wchar_t)0x1F83, (wchar_t)0x1F84, (wchar_t)0x1F85, (wchar_t)0x1F86, (wchar_t)0x1F87, (wchar_t)0x1F90, (wchar_t)0x1F91,
			(wchar_t)0x1F92, (wchar_t)0x1F93, (wchar_t)0x1F94, (wchar_t)0x1F95, (wchar_t)0x1F96, (wchar_t)0x1F97, (wchar_t)0x1FA0, (wchar_t)0x1FA1, (wchar_t)0x1FA2,
			(wchar_t)0x1FA3, (wchar_t)0x1FA4, (wchar_t)0x1FA5, (wchar_t)0x1FA6, (wchar_t)0x1FA7, (wchar_t)0x1FB0, (wchar_t)0x1FB1, (wchar_t)0x1FD0, (wchar_t)0x1FD1,
			(wchar_t)0x1FE0, (wchar_t)0x1FE1, (wchar_t)0x24D0, (wchar_t)0x24D1, (wchar_t)0x24D2, (wchar_t)0x24D3, (wchar_t)0x24D4, (wchar_t)0x24D5, (wchar_t)0x24D6,
			(wchar_t)0x24D7, (wchar_t)0x24D8, (wchar_t)0x24D9, (wchar_t)0x24DA, (wchar_t)0x24DB, (wchar_t)0x24DC, (wchar_t)0x24DD, (wchar_t)0x24DE, (wchar_t)0x24DF,
			(wchar_t)0x24E0, (wchar_t)0x24E1, (wchar_t)0x24E2, (wchar_t)0x24E3, (wchar_t)0x24E4, (wchar_t)0x24E5, (wchar_t)0x24E6, (wchar_t)0x24E7, (wchar_t)0x24E8,
			(wchar_t)0x24E9, (wchar_t)0xFF41, (wchar_t)0xFF42, (wchar_t)0xFF43, (wchar_t)0xFF44, (wchar_t)0xFF45, (wchar_t)0xFF46, (wchar_t)0xFF47, (wchar_t)0xFF48,
			(wchar_t)0xFF49, (wchar_t)0xFF4A, (wchar_t)0xFF4B, (wchar_t)0xFF4C, (wchar_t)0xFF4D, (wchar_t)0xFF4E, (wchar_t)0xFF4F, (wchar_t)0xFF50, (wchar_t)0xFF51,
			(wchar_t)0xFF52, (wchar_t)0xFF53, (wchar_t)0xFF54, (wchar_t)0xFF55, (wchar_t)0xFF56, (wchar_t)0xFF57, (wchar_t)0xFF58, (wchar_t)0xFF59, (wchar_t)0xFF5A };

		static const wchar_t unicode_uppers[] =
		{
			(wchar_t)0x0041, (wchar_t)0x0042, (wchar_t)0x0043, (wchar_t)0x0044, (wchar_t)0x0045, (wchar_t)0x0046, (wchar_t)0x0047, (wchar_t)0x0048, (wchar_t)0x0049,
			(wchar_t)0x004A, (wchar_t)0x004B, (wchar_t)0x004C, (wchar_t)0x004D, (wchar_t)0x004E, (wchar_t)0x004F, (wchar_t)0x0050, (wchar_t)0x0051, (wchar_t)0x0052,
			(wchar_t)0x0053, (wchar_t)0x0054, (wchar_t)0x0055, (wchar_t)0x0056, (wchar_t)0x0057, (wchar_t)0x0058, (wchar_t)0x0059, (wchar_t)0x005A, (wchar_t)0x00C0,
			(wchar_t)0x00C1, (wchar_t)0x00C2, (wchar_t)0x00C3, (wchar_t)0x00C4, (wchar_t)0x00C5, (wchar_t)0x00C6, (wchar_t)0x00C7, (wchar_t)0x00C8, (wchar_t)0x00C9,
			(wchar_t)0x00CA, (wchar_t)0x00CB, (wchar_t)0x00CC, (wchar_t)0x00CD, (wchar_t)0x00CE, (wchar_t)0x00CF, (wchar_t)0x00D0, (wchar_t)0x00D1, (wchar_t)0x00D2,
			(wchar_t)0x00D3, (wchar_t)0x00D4, (wchar_t)0x00D5, (wchar_t)0x00D6, (wchar_t)0x00D8, (wchar_t)0x00D9, (wchar_t)0x00DA, (wchar_t)0x00DB, (wchar_t)0x00DC,
			(wchar_t)0x00DD, (wchar_t)0x00DE, (wchar_t)0x0178, (wchar_t)0x0100, (wchar_t)0x0102, (wchar_t)0x0104, (wchar_t)0x0106, (wchar_t)0x0108, (wchar_t)0x010A,
			(wchar_t)0x010C, (wchar_t)0x010E, (wchar_t)0x0110, (wchar_t)0x0112, (wchar_t)0x0114, (wchar_t)0x0116, (wchar_t)0x0118, (wchar_t)0x011A, (wchar_t)0x011C,
			(wchar_t)0x011E, (wchar_t)0x0120, (wchar_t)0x0122, (wchar_t)0x0124, (wchar_t)0x0126, (wchar_t)0x0128, (wchar_t)0x012A, (wchar_t)0x012C, (wchar_t)0x012E,
			(wchar_t)0x0049, (wchar_t)0x0132, (wchar_t)0x0134, (wchar_t)0x0136, (wchar_t)0x0139, (wchar_t)0x013B, (wchar_t)0x013D, (wchar_t)0x013F, (wchar_t)0x0141,
			(wchar_t)0x0143, (wchar_t)0x0145, (wchar_t)0x0147, (wchar_t)0x014A, (wchar_t)0x014C, (wchar_t)0x014E, (wchar_t)0x0150, (wchar_t)0x0152, (wchar_t)0x0154,
			(wchar_t)0x0156, (wchar_t)0x0158, (wchar_t)0x015A, (wchar_t)0x015C, (wchar_t)0x015E, (wchar_t)0x0160, (wchar_t)0x0162, (wchar_t)0x0164, (wchar_t)0x0166,
			(wchar_t)0x0168, (wchar_t)0x016A, (wchar_t)0x016C, (wchar_t)0x016E, (wchar_t)0x0170, (wchar_t)0x0172, (wchar_t)0x0174, (wchar_t)0x0176, (wchar_t)0x0179,
			(wchar_t)0x017B, (wchar_t)0x017D, (wchar_t)0x0182, (wchar_t)0x0184, (wchar_t)0x0187, (wchar_t)0x018B, (wchar_t)0x0191, (wchar_t)0x0198, (wchar_t)0x01A0,
			(wchar_t)0x01A2, (wchar_t)0x01A4, (wchar_t)0x01A7, (wchar_t)0x01AC, (wchar_t)0x01AF, (wchar_t)0x01B3, (wchar_t)0x01B5, (wchar_t)0x01B8, (wchar_t)0x01BC,
			(wchar_t)0x01C4, (wchar_t)0x01C7, (wchar_t)0x01CA, (wchar_t)0x01CD, (wchar_t)0x01CF, (wchar_t)0x01D1, (wchar_t)0x01D3, (wchar_t)0x01D5, (wchar_t)0x01D7,
			(wchar_t)0x01D9, (wchar_t)0x01DB, (wchar_t)0x01DE, (wchar_t)0x01E0, (wchar_t)0x01E2, (wchar_t)0x01E4, (wchar_t)0x01E6, (wchar_t)0x01E8, (wchar_t)0x01EA,
			(wchar_t)0x01EC, (wchar_t)0x01EE, (wchar_t)0x01F1, (wchar_t)0x01F4, (wchar_t)0x01FA, (wchar_t)0x01FC, (wchar_t)0x01FE, (wchar_t)0x0200, (wchar_t)0x0202,
			(wchar_t)0x0204, (wchar_t)0x0206, (wchar_t)0x0208, (wchar_t)0x020A, (wchar_t)0x020C, (wchar_t)0x020E, (wchar_t)0x0210, (wchar_t)0x0212, (wchar_t)0x0214,
			(wchar_t)0x0216, (wchar_t)0x0181, (wchar_t)0x0186, (wchar_t)0x018A, (wchar_t)0x018E, (wchar_t)0x018F, (wchar_t)0x0190, (wchar_t)0x0193, (wchar_t)0x0194,
			(wchar_t)0x0197, (wchar_t)0x0196, (wchar_t)0x019C, (wchar_t)0x019D, (wchar_t)0x019F, (wchar_t)0x01A9, (wchar_t)0x01AE, (wchar_t)0x01B1, (wchar_t)0x01B2,
			(wchar_t)0x01B7, (wchar_t)0x0386, (wchar_t)0x0388, (wchar_t)0x0389, (wchar_t)0x038A, (wchar_t)0x0391, (wchar_t)0x0392, (wchar_t)0x0393, (wchar_t)0x0394,
			(wchar_t)0x0395, (wchar_t)0x0396, (wchar_t)0x0397, (wchar_t)0x0398, (wchar_t)0x0399, (wchar_t)0x039A, (wchar_t)0x039B, (wchar_t)0x039C, (wchar_t)0x039D,
			(wchar_t)0x039E, (wchar_t)0x039F, (wchar_t)0x03A0, (wchar_t)0x03A1, (wchar_t)0x03A3, (wchar_t)0x03A4, (wchar_t)0x03A5, (wchar_t)0x03A6, (wchar_t)0x03A7,
			(wchar_t)0x03A8, (wchar_t)0x03A9, (wchar_t)0x03AA, (wchar_t)0x03AB, (wchar_t)0x038C, (wchar_t)0x038E, (wchar_t)0x038F, (wchar_t)0x03E2, (wchar_t)0x03E4,
			(wchar_t)0x03E6, (wchar_t)0x03E8, (wchar_t)0x03EA, (wchar_t)0x03EC, (wchar_t)0x03EE, (wchar_t)0x0410, (wchar_t)0x0411, (wchar_t)0x0412, (wchar_t)0x0413,
			(wchar_t)0x0414, (wchar_t)0x0415, (wchar_t)0x0416, (wchar_t)0x0417, (wchar_t)0x0418, (wchar_t)0x0419, (wchar_t)0x041A, (wchar_t)0x041B, (wchar_t)0x041C,
			(wchar_t)0x041D, (wchar_t)0x041E, (wchar_t)0x041F, (wchar_t)0x0420, (wchar_t)0x0421, (wchar_t)0x0422, (wchar_t)0x0423, (wchar_t)0x0424, (wchar_t)0x0425,
			(wchar_t)0x0426, (wchar_t)0x0427, (wchar_t)0x0428, (wchar_t)0x0429, (wchar_t)0x042A, (wchar_t)0x042B, (wchar_t)0x042C, (wchar_t)0x042D, (wchar_t)0x042E,
			(wchar_t)0x042F, (wchar_t)0x0401, (wchar_t)0x0402, (wchar_t)0x0403, (wchar_t)0x0404, (wchar_t)0x0405, (wchar_t)0x0406, (wchar_t)0x0407, (wchar_t)0x0408,
			(wchar_t)0x0409, (wchar_t)0x040A, (wchar_t)0x040B, (wchar_t)0x040C, (wchar_t)0x040E, (wchar_t)0x040F, (wchar_t)0x0460, (wchar_t)0x0462, (wchar_t)0x0464,
			(wchar_t)0x0466, (wchar_t)0x0468, (wchar_t)0x046A, (wchar_t)0x046C, (wchar_t)0x046E, (wchar_t)0x0470, (wchar_t)0x0472, (wchar_t)0x0474, (wchar_t)0x0476,
			(wchar_t)0x0478, (wchar_t)0x047A, (wchar_t)0x047C, (wchar_t)0x047E, (wchar_t)0x0480, (wchar_t)0x0490, (wchar_t)0x0492, (wchar_t)0x0494, (wchar_t)0x0496,
			(wchar_t)0x0498, (wchar_t)0x049A, (wchar_t)0x049C, (wchar_t)0x049E, (wchar_t)0x04A0, (wchar_t)0x04A2, (wchar_t)0x04A4, (wchar_t)0x04A6, (wchar_t)0x04A8,
			(wchar_t)0x04AA, (wchar_t)0x04AC, (wchar_t)0x04AE, (wchar_t)0x04B0, (wchar_t)0x04B2, (wchar_t)0x04B4, (wchar_t)0x04B6, (wchar_t)0x04B8, (wchar_t)0x04BA,
			(wchar_t)0x04BC, (wchar_t)0x04BE, (wchar_t)0x04C1, (wchar_t)0x04C3, (wchar_t)0x04C7, (wchar_t)0x04CB, (wchar_t)0x04D0, (wchar_t)0x04D2, (wchar_t)0x04D4,
			(wchar_t)0x04D6, (wchar_t)0x04D8, (wchar_t)0x04DA, (wchar_t)0x04DC, (wchar_t)0x04DE, (wchar_t)0x04E0, (wchar_t)0x04E2, (wchar_t)0x04E4, (wchar_t)0x04E6,
			(wchar_t)0x04E8, (wchar_t)0x04EA, (wchar_t)0x04EE, (wchar_t)0x04F0, (wchar_t)0x04F2, (wchar_t)0x04F4, (wchar_t)0x04F8, (wchar_t)0x0531, (wchar_t)0x0532,
			(wchar_t)0x0533, (wchar_t)0x0534, (wchar_t)0x0535, (wchar_t)0x0536, (wchar_t)0x0537, (wchar_t)0x0538, (wchar_t)0x0539, (wchar_t)0x053A, (wchar_t)0x053B,
			(wchar_t)0x053C, (wchar_t)0x053D, (wchar_t)0x053E, (wchar_t)0x053F, (wchar_t)0x0540, (wchar_t)0x0541, (wchar_t)0x0542, (wchar_t)0x0543, (wchar_t)0x0544,
			(wchar_t)0x0545, (wchar_t)0x0546, (wchar_t)0x0547, (wchar_t)0x0548, (wchar_t)0x0549, (wchar_t)0x054A, (wchar_t)0x054B, (wchar_t)0x054C, (wchar_t)0x054D,
			(wchar_t)0x054E, (wchar_t)0x054F, (wchar_t)0x0550, (wchar_t)0x0551, (wchar_t)0x0552, (wchar_t)0x0553, (wchar_t)0x0554, (wchar_t)0x0555, (wchar_t)0x0556,
			(wchar_t)0x10A0, (wchar_t)0x10A1, (wchar_t)0x10A2, (wchar_t)0x10A3, (wchar_t)0x10A4, (wchar_t)0x10A5, (wchar_t)0x10A6, (wchar_t)0x10A7, (wchar_t)0x10A8,
			(wchar_t)0x10A9, (wchar_t)0x10AA, (wchar_t)0x10AB, (wchar_t)0x10AC, (wchar_t)0x10AD, (wchar_t)0x10AE, (wchar_t)0x10AF, (wchar_t)0x10B0, (wchar_t)0x10B1,
			(wchar_t)0x10B2, (wchar_t)0x10B3, (wchar_t)0x10B4, (wchar_t)0x10B5, (wchar_t)0x10B6, (wchar_t)0x10B7, (wchar_t)0x10B8, (wchar_t)0x10B9, (wchar_t)0x10BA,
			(wchar_t)0x10BB, (wchar_t)0x10BC, (wchar_t)0x10BD, (wchar_t)0x10BE, (wchar_t)0x10BF, (wchar_t)0x10C0, (wchar_t)0x10C1, (wchar_t)0x10C2, (wchar_t)0x10C3,
			(wchar_t)0x10C4, (wchar_t)0x10C5, (wchar_t)0x1E00, (wchar_t)0x1E02, (wchar_t)0x1E04, (wchar_t)0x1E06, (wchar_t)0x1E08, (wchar_t)0x1E0A, (wchar_t)0x1E0C,
			(wchar_t)0x1E0E, (wchar_t)0x1E10, (wchar_t)0x1E12, (wchar_t)0x1E14, (wchar_t)0x1E16, (wchar_t)0x1E18, (wchar_t)0x1E1A, (wchar_t)0x1E1C, (wchar_t)0x1E1E,
			(wchar_t)0x1E20, (wchar_t)0x1E22, (wchar_t)0x1E24, (wchar_t)0x1E26, (wchar_t)0x1E28, (wchar_t)0x1E2A, (wchar_t)0x1E2C, (wchar_t)0x1E2E, (wchar_t)0x1E30,
			(wchar_t)0x1E32, (wchar_t)0x1E34, (wchar_t)0x1E36, (wchar_t)0x1E38, (wchar_t)0x1E3A, (wchar_t)0x1E3C, (wchar_t)0x1E3E, (wchar_t)0x1E40, (wchar_t)0x1E42,
			(wchar_t)0x1E44, (wchar_t)0x1E46, (wchar_t)0x1E48, (wchar_t)0x1E4A, (wchar_t)0x1E4C, (wchar_t)0x1E4E, (wchar_t)0x1E50, (wchar_t)0x1E52, (wchar_t)0x1E54,
			(wchar_t)0x1E56, (wchar_t)0x1E58, (wchar_t)0x1E5A, (wchar_t)0x1E5C, (wchar_t)0x1E5E, (wchar_t)0x1E60, (wchar_t)0x1E62, (wchar_t)0x1E64, (wchar_t)0x1E66,
			(wchar_t)0x1E68, (wchar_t)0x1E6A, (wchar_t)0x1E6C, (wchar_t)0x1E6E, (wchar_t)0x1E70, (wchar_t)0x1E72, (wchar_t)0x1E74, (wchar_t)0x1E76, (wchar_t)0x1E78,
			(wchar_t)0x1E7A, (wchar_t)0x1E7C, (wchar_t)0x1E7E, (wchar_t)0x1E80, (wchar_t)0x1E82, (wchar_t)0x1E84, (wchar_t)0x1E86, (wchar_t)0x1E88, (wchar_t)0x1E8A,
			(wchar_t)0x1E8C, (wchar_t)0x1E8E, (wchar_t)0x1E90, (wchar_t)0x1E92, (wchar_t)0x1E94, (wchar_t)0x1EA0, (wchar_t)0x1EA2, (wchar_t)0x1EA4, (wchar_t)0x1EA6,
			(wchar_t)0x1EA8, (wchar_t)0x1EAA, (wchar_t)0x1EAC, (wchar_t)0x1EAE, (wchar_t)0x1EB0, (wchar_t)0x1EB2, (wchar_t)0x1EB4, (wchar_t)0x1EB6, (wchar_t)0x1EB8,
			(wchar_t)0x1EBA, (wchar_t)0x1EBC, (wchar_t)0x1EBE, (wchar_t)0x1EC0, (wchar_t)0x1EC2, (wchar_t)0x1EC4, (wchar_t)0x1EC6, (wchar_t)0x1EC8, (wchar_t)0x1ECA,
			(wchar_t)0x1ECC, (wchar_t)0x1ECE, (wchar_t)0x1ED0, (wchar_t)0x1ED2, (wchar_t)0x1ED4, (wchar_t)0x1ED6, (wchar_t)0x1ED8, (wchar_t)0x1EDA, (wchar_t)0x1EDC,
			(wchar_t)0x1EDE, (wchar_t)0x1EE0, (wchar_t)0x1EE2, (wchar_t)0x1EE4, (wchar_t)0x1EE6, (wchar_t)0x1EE8, (wchar_t)0x1EEA, (wchar_t)0x1EEC, (wchar_t)0x1EEE,
			(wchar_t)0x1EF0, (wchar_t)0x1EF2, (wchar_t)0x1EF4, (wchar_t)0x1EF6, (wchar_t)0x1EF8, (wchar_t)0x1F08, (wchar_t)0x1F09, (wchar_t)0x1F0A, (wchar_t)0x1F0B,
			(wchar_t)0x1F0C, (wchar_t)0x1F0D, (wchar_t)0x1F0E, (wchar_t)0x1F0F, (wchar_t)0x1F18, (wchar_t)0x1F19, (wchar_t)0x1F1A, (wchar_t)0x1F1B, (wchar_t)0x1F1C,
			(wchar_t)0x1F1D, (wchar_t)0x1F28, (wchar_t)0x1F29, (wchar_t)0x1F2A, (wchar_t)0x1F2B, (wchar_t)0x1F2C, (wchar_t)0x1F2D, (wchar_t)0x1F2E, (wchar_t)0x1F2F,
			(wchar_t)0x1F38, (wchar_t)0x1F39, (wchar_t)0x1F3A, (wchar_t)0x1F3B, (wchar_t)0x1F3C, (wchar_t)0x1F3D, (wchar_t)0x1F3E, (wchar_t)0x1F3F, (wchar_t)0x1F48,
			(wchar_t)0x1F49, (wchar_t)0x1F4A, (wchar_t)0x1F4B, (wchar_t)0x1F4C, (wchar_t)0x1F4D, (wchar_t)0x1F59, (wchar_t)0x1F5B, (wchar_t)0x1F5D, (wchar_t)0x1F5F,
			(wchar_t)0x1F68, (wchar_t)0x1F69, (wchar_t)0x1F6A, (wchar_t)0x1F6B, (wchar_t)0x1F6C, (wchar_t)0x1F6D, (wchar_t)0x1F6E, (wchar_t)0x1F6F, (wchar_t)0x1F88,
			(wchar_t)0x1F89, (wchar_t)0x1F8A, (wchar_t)0x1F8B, (wchar_t)0x1F8C, (wchar_t)0x1F8D, (wchar_t)0x1F8E, (wchar_t)0x1F8F, (wchar_t)0x1F98, (wchar_t)0x1F99,
			(wchar_t)0x1F9A, (wchar_t)0x1F9B, (wchar_t)0x1F9C, (wchar_t)0x1F9D, (wchar_t)0x1F9E, (wchar_t)0x1F9F, (wchar_t)0x1FA8, (wchar_t)0x1FA9, (wchar_t)0x1FAA,
			(wchar_t)0x1FAB, (wchar_t)0x1FAC, (wchar_t)0x1FAD, (wchar_t)0x1FAE, (wchar_t)0x1FAF, (wchar_t)0x1FB8, (wchar_t)0x1FB9, (wchar_t)0x1FD8, (wchar_t)0x1FD9,
			(wchar_t)0x1FE8, (wchar_t)0x1FE9, (wchar_t)0x24B6, (wchar_t)0x24B7, (wchar_t)0x24B8, (wchar_t)0x24B9, (wchar_t)0x24BA, (wchar_t)0x24BB, (wchar_t)0x24BC,
			(wchar_t)0x24BD, (wchar_t)0x24BE, (wchar_t)0x24BF, (wchar_t)0x24C0, (wchar_t)0x24C1, (wchar_t)0x24C2, (wchar_t)0x24C3, (wchar_t)0x24C4, (wchar_t)0x24C5,
			(wchar_t)0x24C6, (wchar_t)0x24C7, (wchar_t)0x24C8, (wchar_t)0x24C9, (wchar_t)0x24CA, (wchar_t)0x24CB, (wchar_t)0x24CC, (wchar_t)0x24CD, (wchar_t)0x24CE,
			(wchar_t)0x24CF, (wchar_t)0xFF21, (wchar_t)0xFF22, (wchar_t)0xFF23, (wchar_t)0xFF24, (wchar_t)0xFF25, (wchar_t)0xFF26, (wchar_t)0xFF27, (wchar_t)0xFF28,
			(wchar_t)0xFF29, (wchar_t)0xFF2A, (wchar_t)0xFF2B, (wchar_t)0xFF2C, (wchar_t)0xFF2D, (wchar_t)0xFF2E, (wchar_t)0xFF2F, (wchar_t)0xFF30, (wchar_t)0xFF31,
			(wchar_t)0xFF32, (wchar_t)0xFF33, (wchar_t)0xFF34, (wchar_t)0xFF35, (wchar_t)0xFF36, (wchar_t)0xFF37, (wchar_t)0xFF38, (wchar_t)0xFF39, (wchar_t)0xFF3A };

		static int compareWchar(const void* a, const void* b)
		{
			if (*(wchar_t*)a < *(wchar_t*)b)
				return -1;
			else if (*(wchar_t*)a > *(wchar_t*)b)
				return 1;

			return 0;
		}

		static  wchar_t tolowerUnicode(const wchar_t& c)
		{
			wchar_t* p = (wchar_t*)bsearch(&c, unicode_uppers, sizeof(unicode_uppers) / sizeof(wchar_t), sizeof(wchar_t), compareWchar);
			if (p)
				return *(unicode_lowers + (p - unicode_uppers));

			return c;
		}

		static wchar_t toupperUnicode(const wchar_t& c)
		{
			wchar_t* p = (wchar_t*)bsearch(&c, unicode_lowers, sizeof(unicode_lowers) / sizeof(wchar_t), sizeof(wchar_t), compareWchar);
			if (p)
				return *(unicode_uppers + (p - unicode_lowers));

			return c;
		}

		unsigned int chars2Unicode(const std::string& _string, size_t& _cursor)
		{
			unsigned const char checkCharType = _string[_cursor];
			unsigned int result = '?';

			// 0xxxxxxx, one byte character.
			if (checkCharType <= 0x7F) {
				// 0xxxxxxx
				result = (_string[_cursor++]);
			}
			// 11110xxx, four byte character.
			else if (checkCharType >= 0xF0 && _cursor < _string.length() - 2) {
				// 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
				result = (_string[_cursor++] & 0x07) << 18;
				result |= (_string[_cursor++] & 0x3F) << 12;
				result |= (_string[_cursor++] & 0x3F) << 6;
				result |= _string[_cursor++] & 0x3F;
			}
			// 1110xxxx, three byte character.
			else if (checkCharType >= 0xE0 && _cursor < _string.length() - 1) {
				// 1110xxxx 10xxxxxx 10xxxxxx
				result = (_string[_cursor++] & 0x0F) << 12;
				result |= (_string[_cursor++] & 0x3F) << 6;
				result |= _string[_cursor++] & 0x3F;
			}
			// 110xxxxx, two byte character.
			else if (checkCharType >= 0xC0 && _cursor < _string.length()) {
				// 110xxxxx 10xxxxxx
				result = (_string[_cursor++] & 0x1F) << 6;
				result |= _string[_cursor++] & 0x3F;
			}
			else {
				// Error, invalid character.
				++_cursor;
			}

			return result;

		} // chars2Unicode

		std::string unicode2Chars(const unsigned int _unicode)
		{
			std::string result;

			if(_unicode < 0x80) // one byte character
			{
				result += ((_unicode      ) & 0xFF);
			}
			else if(_unicode < 0x800) // two byte character
			{
				result += ((_unicode >>  6) & 0xFF) | 0xC0;
				result += ((_unicode      ) & 0x3F) | 0x80;
			}
			else if(_unicode < 0xFFFF) // three byte character
			{
				result += ((_unicode >> 12) & 0xFF) | 0xE0;
				result += ((_unicode >>  6) & 0x3F) | 0x80;
				result += ((_unicode      ) & 0x3F) | 0x80;
			}
			else if(_unicode <= 0x1fffff) // four byte character
			{
				result += ((_unicode >> 18) & 0xFF) | 0xF0;
				result += ((_unicode >> 12) & 0x3F) | 0x80;
				result += ((_unicode >>  6) & 0x3F) | 0x80;
				result += ((_unicode      ) & 0x3F) | 0x80;
			}
			else
			{
				// error, invalid unicode
				result += '?';
			}

			return result;

		} // unicode2Chars

		size_t nextCursor(const std::string& _string, const size_t _cursor)
		{
			size_t result = _cursor;

			while(result < _string.length())
			{
				++result;

				if((_string[result] & 0xC0) != 0x80) // break if current character is not 10xxxxxx
					break;
			}

			return result;

		} // nextCursor

		size_t prevCursor(const std::string& _string, const size_t _cursor)
		{
			size_t result = _cursor;

			while(result > 0)
			{
				--result;

				if((_string[result] & 0xC0) != 0x80) // break if current character is not 10xxxxxx
					break;
			}

			return result;

		} // prevCursor

		size_t moveCursor(const std::string& _string, const size_t _cursor, const int _amount)
		{
			size_t result = _cursor;

			if(_amount > 0)
			{
				for(int i = 0; i < _amount; ++i)
					result = nextCursor(_string, result);
			}
			else if(_amount < 0)
			{
				for(int i = _amount; i < 0; ++i)
					result = prevCursor(_string, result);
			}

			return result;

		} // moveCursor
		
		std::string toLower(const std::string& _string) 
		{
			std::string text = _string;

			size_t i = 0;
			while (i < text.length())
			{
				char c = text[i];
				if ((c & 0x80) == 0)
				{
					if (c >= 'A' && c <= 'Z')
						text[i] = c + 0x20;

					i++;
					continue;
				}

				int pos = i;
				wchar_t character = (wchar_t)chars2Unicode(text, i);
				wchar_t unicode = tolowerUnicode(character);
				if (unicode != character)
				{
					int charSize = i - pos;
					if (charSize == 2)
					{
						text[pos] = (char)(((unicode >> 6) & 0xFF) | 0xC0);
						text[pos + 1] = (char)((unicode & 0x3F) | 0x80);
					}
					else if (charSize == 3)
					{
						text[pos] += (char)(((unicode >> 12) & 0xFF) | 0xE0);
						text[pos + 1] += (char)(((unicode >> 6) & 0x3F) | 0x80);
						text[pos + 2] += (char)((unicode & 0x3F) | 0x80);

					}
				}
			}

			return text;
		}

		std::string toUpper(const std::string& _string) 
		{
			std::string text = _string;

			size_t i = 0;
			while (i < text.length())
			{
				char c = text[i];
				if ((c & 0x80) == 0)
				{
					if (c >= 'a' && c <= 'z')
						text[i] = c - 0x20;

					i++;
					continue;
				}

				int pos = i;
				wchar_t character = (wchar_t)chars2Unicode(text, i);
				wchar_t unicode = toupperUnicode(character);
				if (unicode != character)
				{
					int charSize = i - pos;
					if (charSize == 2)
					{
						text[pos] = (char)(((unicode >> 6) & 0xFF) | 0xC0);
						text[pos + 1] = (char)((unicode & 0x3F) | 0x80);
					}
					else if (charSize == 3)
					{
						text[pos] += (char)(((unicode >> 12) & 0xFF) | 0xE0);
						text[pos + 1] += (char)(((unicode >> 6) & 0x3F) | 0x80);
						text[pos + 2] += (char)((unicode & 0x3F) | 0x80);
					}
				}
			}

			return text;
		}

		std::string trim(const std::string& _string)
		{
			const size_t strBegin = _string.find_first_not_of(" \t\r\n");
			if(strBegin == std::string::npos)
				return "";

			const size_t strEnd = _string.find_last_not_of(" \t\r\n");			
			return _string.substr(strBegin, strEnd - strBegin + 1);

		} // trim

		std::string replace(const std::string& _string, const std::string& _replace, const std::string& _with)
		{
			if (_replace.empty())
				return _string;

			std::string string = _string;
			
			size_t pos = 0;
			while ((pos = string.find(_replace, pos)) != std::string::npos) {
				string = string.replace(pos, _replace.length(), _with.c_str());
				pos += _with.length();
			}
			
			return string;

		} // replace

		bool startsWith(const std::string& _string, const std::string& _start)
		{
			return (strncmp(_string.c_str(), _start.c_str(), _start.size()) == 0);
		} // startsWith

		bool endsWith(const std::string& _string, const std::string& _end)
		{
			if (_end.size() > _string.size()) return false;
			return (_string.rfind(_end) == (_string.size() - _end.size()));
		} // endsWith

		std::string removeParenthesis(const std::string& _string)
		{
			static const char remove[4] = { '(', ')', '[', ']' };
			std::string       string = _string;
			size_t            start;
			size_t            end;
			bool              done = false;

			while(!done)
			{
				done = true;

				for(int i = 0; i < sizeof(remove); i += 2)
				{
					end   = string.find_first_of(remove[i + 1]);
					start = string.find_last_of( remove[i + 0], end);

					if((start != std::string::npos) && (end != std::string::npos))
					{
						string.erase(start, end - start + 1);
						done = false;
					}
				}
			}

			return trim(string);

		} // removeParenthesis

		std::string vectorToCommaString(stringVector _vector)
		{
			std::string string;

			std::sort(_vector.begin(), _vector.end());

			for(stringVector::const_iterator it = _vector.cbegin(); it != _vector.cend(); ++it)
				string += (string.length() ? "," : "") + (*it);

			return string;

		} // vectorToCommaString

		stringVector commaStringToVector(const std::string& _string)
		{
			return Utils::String::split(_string, ',', false);
		}

		std::string format(const char* _format, ...)
		{
			va_list	args;
			va_list copy;

			va_start(args, _format);

			va_copy(copy, args);
			const int length = vsnprintf(nullptr, 0, _format, copy);
			va_end(copy);

			char* buffer = new char[length + 1];
			va_copy(copy, args);
			vsnprintf(buffer, length + 1, _format, copy);
			va_end(copy);

			va_end(args);

			std::string out(buffer);
			delete[] buffer;

			return out;

		} // format

		// Simple XOR scrambling of a string, with an accompanying key
		std::string scramble(const std::string& _input, const std::string& key)
		{
			std::string buffer = _input;

			for (size_t i = 0; i < _input.size(); ++i) 
			{               
				buffer[i] = _input[i] ^ key[i];
			}

			return buffer;

		} // scramble

		std::vector<std::string> split(const std::string& s, char seperator, bool removeEmptyEntries)
		{			
			std::vector<std::string> output;

			if (s.empty())
				return output;
			
			const char* src = s.c_str();

			while (true) 
			{
				const char* d = strchr(src, seperator);
				size_t len = (d) ? d - src : strlen(src);

				if (len || !removeEmptyEntries)
					output.push_back(std::string(src, len)); // capture token

				if (d) src += len + 1; else break;
			}

			return output;
		}

		std::vector<std::string> splitAny(const std::string& s, const std::string& seperator, bool removeEmptyEntries)
		{
			std::vector<std::string> output;

			unsigned prev_pos = 0;
			auto pos = s.find_first_of(seperator);
			while (pos != std::string::npos)
			{
				std::string token = s.substr(prev_pos, pos - prev_pos);
				if (!removeEmptyEntries || !token.empty())
					output.push_back(token);

				pos++;
				prev_pos = pos;
				pos = s.find_first_of(seperator, pos);
			}

			if (prev_pos < s.length())
			{
				std::string token = s.substr(prev_pos);
				if (!removeEmptyEntries || !token.empty())
					output.push_back(token); // Last word
			}

			return output;
		}

		std::string join(const std::vector<std::string>& items, std::string separator)
		{
			std::string data;

			for (auto line : items)
			{
				if (!data.empty())
					data += separator;

				data += line;
			}

			return data;
		}

		std::string extractString(const std::string& _string, const std::string& startDelimiter, const std::string& endDelimiter, bool keepDelimiter)
		{
			auto ret = extractStrings(_string, startDelimiter, endDelimiter, keepDelimiter);
			if (ret.size() > 0)
				return ret[0];

			return "";
		}

		std::vector<std::string> extractStrings(const std::string& _string, const std::string& startDelimiter, const std::string& endDelimiter, bool keepDelimiter)
		{
			std::vector<std::string> ret;

			if (!startDelimiter.empty() && !endDelimiter.empty())
			{
				size_t pos = 0;
				while (pos != std::string::npos)
				{
					pos = _string.find(startDelimiter, pos);
					if (pos == std::string::npos)
						break;

					auto end = _string.find(endDelimiter, pos + startDelimiter.size());
					if (end == std::string::npos)
						break;

					std::string value = keepDelimiter ? _string.substr(pos, end - pos + endDelimiter.length()) : _string.substr(pos + startDelimiter.size(), end - (pos + startDelimiter.size()));
					if (!value.empty())
						ret.push_back(value);

					pos = end + endDelimiter.size();
				}
			}

			return ret;
		}

		bool startsWithIgnoreCase(const std::string& name1, const std::string& name2)
		{
			auto makeUp = [](unsigned int c)
			{
				if ((c & 0x80) == 0) return toupper(c);
				return (int)toupperUnicode(c);
			};

			size_t p1 = 0;
			size_t p2 = 0;

			while (true)
			{
				int u1 = makeUp(chars2Unicode(name1, p1));
				int u2 = makeUp(chars2Unicode(name2, p2));

				if (u1 != 0 && u2 == 0)
					return true;
				
				if (u1 != u2)
					return false;
			}
		}

		int compareIgnoreCase(const std::string& name1, const std::string& name2)
		{
			size_t p1 = 0;
			size_t p2 = 0;

			int u1, u2;
			char c1, c2;

			while (true)
			{
				c1 = name1[p1];
				if ((c1 & 0x80) == 0)
				{
					if (c1 >= 'a' && c1 <= 'z')
						u1 = c1 - 0x20;
					else
						u1 = c1;

					p1++;
				}
				else 
					u1 = toupperUnicode(chars2Unicode(name1, p1));

				c2 = name2[p2];
				if ((c2 & 0x80) == 0)
				{
					if (c2 >= 'a' && c2 <= 'z')
						u2 = c2 - 0x20;
					else
						u2 = c2;

					p2++;
				}
				else
					u2 = toupperUnicode(chars2Unicode(name2, p2));

				if (u1 == 0 && u2 != 0)
					return -1;
				else if (u1 != 0 && u2 == 0)
					return 1;
				else if (u1 == 0 || u2 == 0)
					return 0;

				u1 -= u2;
				if (u1)
					return u1;
			}
		}

		bool containsIgnoreCase(const std::string & _string, const std::string & _what)
		{
			auto it = std::search(
				_string.begin(), _string.end(),
				_what.begin(), _what.end(),
				[](char ch1, char ch2) { return toupper(ch1) == toupper(ch2); }
			);

			return (it != _string.end());
		}
		
		bool containsIgnoreCasePinyin(const std::string & _string, const std::string & _what)
		{
			std::vector<const char*> vpinyin;
			size_t len = _string.size();
			size_t idx = 0;
			bool ret = false;

			vpinyin.reserve(len);
			while(idx < len) {
				int code = chars2Unicode(_string, idx);
				if (code < 0x80) {
					vpinyin.push_back( s_tblpinyin[_string[idx-1]] );
				} else {
					ret = true;
					auto it = s_mapPinyin.find(code);
					if (it != s_mapPinyin.end()) {
						vpinyin.push_back(it->second);
					} else {
						vpinyin.push_back(NULL);
					}
				}
			}
			if (!ret) return false; // all chars < 0x80

			auto it = std::search(
				vpinyin.begin(), vpinyin.end(),
				_what.begin(), _what.end(),
				[](const char *ptr, char ch2) {
				    if (!ptr) return false;
					for(;;) {
						char ch1 = *ptr++;
						if (!ch1) return false;
						if (toupper(ch1) == toupper(ch2)) return true;
					}
				}
			);
			return (it != vpinyin.end());
		}

		std::string proper(const std::string& _string)
		{
			if (_string.length() <= 1)
				return Utils::String::toUpper(_string);

			return Utils::String::toUpper(_string.substr(0, 1)) + Utils::String::toLower(_string.substr(1));
		}

		std::string removeHtmlTags(const std::string& html)
		{
			if (html.empty())
				return html;
			
			std::string text = html;

			size_t start = 0, ss = 0;
			while ((start = text.find("<", (ss = start))) != std::string::npos)
			{
				int end = text.find(">", start);
				if (end != std::string::npos && end >= start)
					text = text.erase(start, end - start + 1);
				else
				{
					start++;
					if (start >= text.size())
						break;
				}
			}
			
			return trim(text);
		}

		unsigned int fromHexString(const std::string& string)
		{
			if (string.empty())
				return 0;

			unsigned int value = 0;

			int dec = 0;
			for (int i = string.length() - 1; i >= 0; i--)
			{
				char c = string[i];
				if (c == ' ')
					continue;

				if (c == 'x' || c == 'X')
					return value;

				if (c >= '0' && c <= '9')
					value += (c - '0') << dec;
				else if (c >= 'A' && c <= 'F')
					value += (c - 'A' + 10) << dec;
				else if (c >= 'a' && c <= 'f')
					value += (c - 'a' + 10) << dec;
				else
					return 0;

				dec += 4;
			}

			return value;
		}

		int	toInteger(const std::string& string)
		{
			if (string.empty())
				return 0;

			const char* str = string.c_str();
			while (*str == ' ')
				str++;

			bool neg = false;
			if (*str == '-')
			{
				neg = true;
				++str;
			}
			else if (*str == '+')
				++str;

			int64_t value = 0;
			for (; *str && *str != '.' && *str != ' ' && *str != '\r' && *str != '\n'; str++)
			{
				if (*str < '0' || *str > '9')
					return 0;

				value *= 10;
				value += *str - '0';
			}

			return neg ? -value : value;
		}

		bool toBoolean(const std::string& string)
		{
			// only look at first char
			char first = string[0];

			// 1*, t* (true), T* (True), y* (yes), Y* (YES)
			return (first == '1' || first == 't' || first == 'T' || first == 'y' || first == 'Y');
		}

		float toFloat(const std::string& string)
		{
			if (string.empty())
				return 0.0f;

			const char* str = string.c_str();
			while (*str == ' ')
				str++;

			bool neg = false;
			if (*str == '-') 
			{
				neg = true;
				++str;
			}
			else if (*str == '+')
				++str;
			
			int64_t value = 0;
			for (; *str && *str != '.' && *str != ' '; str++)
			{
				if (*str < '0' || *str > '9')
					return 0;

				value *= 10;
				value += *str - '0';
			}

			if (*str == '.')
			{
				str++;

				int64_t decimal = 0, weight = 1;

				for (; *str && *str != ' '; str++)
				{
					if (*str < '0' || *str > '9')
						return 0;

					decimal *= 10;
					decimal += *str - '0';
					weight *= 10;
				}

				float ret = value + (decimal / (float)weight);
				return neg ? -ret : ret;
			}

			return neg ? -value : value;
		}

		std::string decodeXmlString(const std::string& string)
		{
			std::string ret = string;

			ret = replace(ret, "&amp;", "&");
			ret = replace(ret, "&apos;", "'");
			ret = replace(ret, "&lt;", "<");
			ret = replace(ret, "&gt;", ">");
			ret = replace(ret, "&quot;", "\"");
			ret = replace(ret, "&nbsp;", " ");
			ret = replace(ret, "&#39;", "'");

			return ret;
		}

		std::string toHexString(unsigned int color)
		{
			char hex[10];
			hex[0] = 0;
			auto len = snprintf(hex, sizeof(hex) - 1, "%08X", color);
			hex[len] = 0;
			return hex;
		}

		std::string padLeft(const std::string& data, const size_t& totalWidth, const char& padding)
		{
			if (data.length() >= totalWidth)
				return data;

			std::string ret = data;
			ret.insert(0, totalWidth - ret.length(), padding);
			return ret;
		}

		int occurs(const std::string& str, char target)
		{
			int count = 0;
			
			for (char ch : str)
				if (ch == target)
					count++;

			return count;
		}

		bool isPrintableChar(char c)
		{			
#if defined(_WIN32)
			return isprint(c);
#else
			return std::isprint(c);
#endif
		}

		bool isKorean(const unsigned int uni)
		{
			return (uni >= 0x3131 && uni <= 0x3163) ||  // Unicode range for Hangul consonants and vowels (ㄱ to ㅣ)
				(uni >= 0xAC00 && uni <= 0xD7A3);       // Unicode range for Hangul syllables (가 to 힣)
		}

		bool isKorean(const char* _string, bool checkFirstChar)
		{
			if (!_string)
				return false;

			size_t len = strlen(_string);
			if (len < 3)
				return false;

			const char* target = _string;
			if (!checkFirstChar)
			{
				const char* ptr = _string + len - 1;
				while (ptr > _string && (*ptr & 0xC0) == 0x80)
					ptr--;

				target = ptr;
			}

			size_t cursor = 0;
			unsigned int uni = chars2Unicode(std::string(target, 3), cursor);

			return isKorean(uni);
		} // isKorean

		KoreanCharType getKoreanCharType(const char* _string)
		{
			if (!_string || strlen(_string) != 3)
				return KoreanCharType::NONE;

			size_t cursor = 0;
			unsigned int uni = chars2Unicode(std::string(_string), cursor);

			if (uni >= 0x3131 && uni <= 0x314E)
				return KoreanCharType::JAEUM;

			if (uni >= 0x314F && uni <= 0x3163)
				return KoreanCharType::MOEUM;

			return KoreanCharType::NONE;

		} // getKoreanCharType

		bool splitHangulSyllable(const char* input, const char** chosung, const char** joongsung, const char** jongsung)
		{
			if (!input)
				return false;

			size_t len = strlen(input);
			if (len < 3)
				return false;

			switch (getKoreanCharType(input))
			{
			case KoreanCharType::JAEUM:
				*chosung = input;
				break;
			case KoreanCharType::MOEUM:
				if (joongsung) *joongsung = input;
				break;
			default:
				size_t cursor = 0;
				unsigned int unicode = chars2Unicode(input, cursor) - 0xAC00;

				int chosungIdx = unicode / (28 * 21);
				int joongsungIdx = (unicode / 28) % 21;
				int jongsungIdx = unicode % 28;

				*chosung = KOREAN_CHOSUNG_LIST.at(chosungIdx);
				if (joongsung) *joongsung = KOREAN_JOONGSUNG_LIST.at(joongsungIdx);
				if (jongsung) *jongsung = KOREAN_JONGSUNG_LIST.at(jongsungIdx);
				break;
			}

			return true;
		}

		void koreanTextInput(const char* text, std::string& componentText, unsigned int& componentCursor)
		{
			if (!text)
				return;

			if (strlen(text) != 3)
				return;

			if (componentText.size() < 3)
				return;

			KoreanCharType charType = getKoreanCharType(text);

			std::string lastCharString = componentText.substr(componentCursor - 3, 3);
			const char* lastChar = lastCharString.c_str();

			const char* chosung = "";
			const char* joongsung = "";
			const char* jongsung = "";
			splitHangulSyllable(lastChar, &chosung, &joongsung, &jongsung);

			auto eraseAndInsert = [&](const std::string& toInsert, bool moveCursorLeft = true)
			{
				size_t cursor = prevCursor(componentText, componentCursor);
				componentText.erase(componentText.begin() + cursor, componentText.begin() + componentCursor);
				componentText.insert(cursor, toInsert);
				if (moveCursorLeft)
					componentCursor -= 3;
			};

			auto makeHangul = [&](int chosungIdx, int joongsungIdx, int jongsungIdx) -> std::string
			{
				unsigned int code = ((chosungIdx * 21) + joongsungIdx) * 28 + jongsungIdx + 0xAC00;
				return unicode2Chars(code);
			};

			auto findIndex = [&](const std::vector<const char*>& vec, const char* key) -> int 
			{
				auto it = std::find_if(vec.begin(), vec.end(), [&](const char* item)
				{
					return std::strcmp(item, key) == 0;
				});

				return (it != vec.end()) ? std::distance(vec.begin(), it) : -1;
			};

			if (!strcmp(lastChar, chosung) && charType == KoreanCharType::JAEUM)
			{
				std::string combine = std::string(chosung) + text;

				int idx = findIndex(KOREAN_GYEOP_BATCHIM_COMBINATIONS, combine.c_str());
				if (idx != -1)
					eraseAndInsert(KOREAN_GYEOP_BATCHIM_LIST.at(idx));
				else
					componentText.insert(componentCursor, text);
			}
			else if (!strcmp(lastChar, chosung) && charType == KoreanCharType::MOEUM)
			{
				int joongsungIdx = findIndex(KOREAN_JOONGSUNG_LIST, text);

				int gyeopBatchimIdx = findIndex(KOREAN_GYEOP_BATCHIM_LIST, lastChar);
				if (gyeopBatchimIdx != -1)
				{
					char jaeum1[4] = {};
					strncpy(jaeum1, KOREAN_GYEOP_BATCHIM_COMBINATIONS.at(gyeopBatchimIdx), 3);
					jaeum1[3] = '\0';

					char jaeum2[4] = {};
					strncpy(jaeum2, KOREAN_GYEOP_BATCHIM_COMBINATIONS.at(gyeopBatchimIdx) + 3, 3);
					jaeum2[3] = '\0';

					int chosungIdx = findIndex(KOREAN_CHOSUNG_LIST, static_cast<const char*>(jaeum2));
					std::string hangul = std::string(jaeum1) + makeHangul(chosungIdx, joongsungIdx, 0);

					eraseAndInsert(hangul, false);
				}
				else
				{
					int chosungIdx = findIndex(KOREAN_CHOSUNG_LIST, lastChar);
					std::string hangul = makeHangul(chosungIdx, joongsungIdx, 0);

					eraseAndInsert(hangul);
				}
			}
			else if (!strcmp(lastChar, joongsung) && charType == KoreanCharType::JAEUM)
			{
				componentText.insert(componentCursor, text);
			}
			else if (!strcmp(lastChar, joongsung) && charType == KoreanCharType::MOEUM)
			{
				std::string combine = std::string(lastChar) + text;

				int idx = findIndex(KOREAN_IJUNG_MOEUM_COMBINATIONS, combine.c_str());
				if (idx != -1)
					eraseAndInsert(KOREAN_IJUNG_MOEUM_LIST.at(idx));
				else
					componentText.insert(componentCursor, text);
			}
			else if (strcmp(jongsung, " ") && charType == KoreanCharType::JAEUM)
			{
				std::string combine = std::string(jongsung) + text;

				int idx = findIndex(KOREAN_GYEOP_BATCHIM_COMBINATIONS, combine.c_str());

				if (idx != -1)
				{
					int chosungIdx = findIndex(KOREAN_CHOSUNG_LIST, chosung);
					int joongsungIdx = findIndex(KOREAN_JOONGSUNG_LIST, joongsung);
					int jongsungIdx = findIndex(KOREAN_JONGSUNG_LIST, KOREAN_GYEOP_BATCHIM_LIST.at(idx));
					std::string hangul = makeHangul(chosungIdx, joongsungIdx, jongsungIdx);

					eraseAndInsert(hangul);
				}
				else
				{
					componentText.insert(componentCursor, text);
				}
			}
			else if (strcmp(jongsung, " ") && charType == KoreanCharType::MOEUM)
			{
				int idx = findIndex(KOREAN_GYEOP_BATCHIM_LIST, jongsung);
				if (idx != -1)
				{
					char jaeum1[4] = {};
					strncpy(jaeum1, KOREAN_GYEOP_BATCHIM_COMBINATIONS.at(idx), 3);
					jaeum1[3] = '\0';

					char jaeum2[4] = {};
					strncpy(jaeum2, KOREAN_GYEOP_BATCHIM_COMBINATIONS.at(idx) + 3, 3);
					jaeum2[3] = '\0';

					int chosungIdx = findIndex(KOREAN_CHOSUNG_LIST, chosung);
					int joongsungIdx = findIndex(KOREAN_JOONGSUNG_LIST, joongsung);
					int jongsungIdx = findIndex(KOREAN_JONGSUNG_LIST, static_cast<const char*>(jaeum1));
					std::string hangul = makeHangul(chosungIdx, joongsungIdx, jongsungIdx);

					chosungIdx = findIndex(KOREAN_CHOSUNG_LIST, static_cast<const char*>(jaeum2));
					joongsungIdx = findIndex(KOREAN_JOONGSUNG_LIST, text);
					hangul += makeHangul(chosungIdx, joongsungIdx, 0);

					eraseAndInsert(hangul, false);
				}
				else
				{
					int chosungIdx = findIndex(KOREAN_CHOSUNG_LIST, chosung);
					int joongsungIdx = findIndex(KOREAN_JOONGSUNG_LIST, joongsung);
					std::string hangul = makeHangul(chosungIdx, joongsungIdx, 0);

					chosungIdx = findIndex(KOREAN_CHOSUNG_LIST, jongsung);
					joongsungIdx = findIndex(KOREAN_JOONGSUNG_LIST, text);
					hangul += makeHangul(chosungIdx, joongsungIdx, 0);

					eraseAndInsert(hangul, false);
				}
			}
			else if (!strcmp(jongsung, " ") && charType == KoreanCharType::JAEUM)
			{
				int jongsungIdx = findIndex(KOREAN_JONGSUNG_LIST, text);

				if (jongsungIdx != -1)
				{
					int chosungIdx = findIndex(KOREAN_CHOSUNG_LIST, chosung);
					int joongsungIdx = findIndex(KOREAN_JOONGSUNG_LIST, joongsung);
					std::string hangul = makeHangul(chosungIdx, joongsungIdx, jongsungIdx);

					eraseAndInsert(hangul);
				}
				else
				{
					componentText.insert(componentCursor, text);
				}
			}
			else if (!strcmp(jongsung, " ") && charType == KoreanCharType::MOEUM)
			{
				std::string combine = std::string(joongsung) + text;

				int idx = findIndex(KOREAN_IJUNG_MOEUM_COMBINATIONS, combine.c_str());
				if (idx != -1)
				{
					int chosungIdx = findIndex(KOREAN_CHOSUNG_LIST, chosung);
					int joongsungIdx = findIndex(KOREAN_JOONGSUNG_LIST, KOREAN_IJUNG_MOEUM_LIST.at(idx));
					std::string hangul = makeHangul(chosungIdx, joongsungIdx, 0);

					eraseAndInsert(hangul);
				}
				else
				{
					componentText.insert(componentCursor, text);
				}
			}

			size_t newCursor = nextCursor(componentText, componentCursor);
			componentCursor = (unsigned int)newCursor;
		}
		// koreanTextInput

#if defined(_WIN32)
		const std::string convertFromWideString(const std::wstring wstring)
		{
			int numBytes = WideCharToMultiByte(CP_UTF8, 0, wstring.c_str(), (int)wstring.length(), nullptr, 0, nullptr, nullptr);
			
			std::string string(numBytes, 0);			
			WideCharToMultiByte(CP_UTF8, 0, wstring.c_str(), (int)wstring.length(), (char*)string.c_str(), numBytes, nullptr, nullptr);

			return string;
		}

		const std::wstring convertToWideString(const std::string string)
		{
			int numBytes = MultiByteToWideChar(CP_UTF8, 0, string.c_str(), (int)string.length(), nullptr, 0);

			std::wstring wstring(numBytes, 0);			
			MultiByteToWideChar(CP_UTF8, 0, string.c_str(), (int)string.length(), (WCHAR*)wstring.c_str(), numBytes);

			return wstring;
		}
#endif
	} // String::

} // Utils::
