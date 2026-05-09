#pragma once
#ifndef ES_APP_FILETAGS_H
#define ES_APP_FILETAGS_H

#include <string>
#include <cstring>
#include <vector>

class FileTag
{
public:
	static std::vector<FileTag> Values();
	static std::vector<std::string> getDisplayIcons(const std::string& names);

public:
	std::string Name;
	std::string displayIcon;	
};

#endif // ES_APP_FILETAGS_H
