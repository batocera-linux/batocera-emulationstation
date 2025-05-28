#pragma once
#ifndef ES_CORE_UTILS_COLOR_UTIL_H
#define ES_CORE_UTILS_COLOR_UTIL_H

#include <string>
#include <cstring>

namespace Utils
{
	namespace HtmlColor
	{
		unsigned int parse(const std::string& str);
		bool isHtmlColor(const std::string& str);
		unsigned int applyColorOpacity(unsigned int color, unsigned char opacity);
	}
}

#endif // ES_CORE_UTILS_COLOR_UTIL_H
