#pragma once

#include "ThemeAnimation.h"
#include <pugixml/src/pugixml.hpp>
#include <vector>

class ThemeStoryboard
{
public:
	ThemeStoryboard()
	{
		repeat = 1;
	}

	~ThemeStoryboard();

	std::string eventName;
	int repeat; // 0 = forever

	std::vector<ThemeAnimation*> animations;

	bool fromXmlNode(const pugi::xml_node& root, std::map<std::string, ThemeData::ElementPropertyType> typeMap);
};
