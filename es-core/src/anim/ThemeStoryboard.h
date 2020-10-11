#pragma once

#include "ThemeAnimation.h"
#include <pugixml/src/pugixml.hpp>
#include <vector>

class ThemeStoryboard
{
public:
	ThemeStoryboard()
	{
		repeatAt = 0;
		repeat = 1;
	}

	ThemeStoryboard(const ThemeStoryboard& src);

	~ThemeStoryboard();

	std::string eventName;
	int repeat; // 0 = forever
	int repeatAt;

	std::vector<ThemeAnimation*> animations;

	bool fromXmlNode(const pugi::xml_node& root, std::map<std::string, ThemeData::ElementPropertyType> typeMap);
};
