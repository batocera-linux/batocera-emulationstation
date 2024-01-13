#pragma once

#ifndef ES_CORE_THEME_VARIABLES_H
#define ES_CORE_THEME_VARIABLES_H

#include <string>
#include <map>

class ThemeVariables : public std::map<std::string, std::string>
{
public:
	std::string resolvePlaceholders(const char* in) const;
};

#endif // ES_CORE_THEME_VARIABLES_H
