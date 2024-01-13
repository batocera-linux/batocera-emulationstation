#include "ThemeVariables.h"

#include <cstring>

std::string ThemeVariables::resolvePlaceholders(const char* in) const
{
	if (in == nullptr || in[0] == 0)
		return in;

	auto begin = strstr(in, "${");
	if (begin == nullptr)
		return in;

	auto end = strstr(begin, "}");
	if (end == nullptr)
		return in;

	std::string inStr(in);

	const size_t variableBegin = begin - in;
	const size_t variableEnd = end - in;

	std::string prefix = inStr.substr(0, variableBegin);
	std::string replace = inStr.substr(variableBegin + 2, variableEnd - (variableBegin + 2));
	std::string suffix = resolvePlaceholders(end + 1);

	auto it = this->find(replace);
	if (it != this->cend())
		return prefix + it->second + suffix;
	
	return prefix + "" + suffix;
}
