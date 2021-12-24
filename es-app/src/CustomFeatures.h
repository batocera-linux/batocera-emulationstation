#pragma once

#ifndef ES_CUSTOMFEATURES_H
#define ES_CUSTOMFEATURES_H

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <pugixml/src/pugixml.hpp>

#include "utils/VectorEx.h"

struct CustomFeatureChoice
{
	std::string name;
	std::string value;
};

struct CustomFeature
{
	CustomFeature()
	{
		order = 0;
	}

	std::string name;
	std::string value;
	std::string description;
	std::string submenu;
	std::string preset;

	std::string group;
	int order;

	std::vector<CustomFeatureChoice> choices;
};

class EmulatorData;

class CustomFeatures : public VectorEx<CustomFeature>
{
public:
	static bool loadEsFeaturesFile();

	static bool FeaturesLoaded;

	static CustomFeatures SharedFeatures;
	static CustomFeatures GlobalFeatures;

	static std::map<std::string, EmulatorData> EmulatorFeatures;

public:
	void sort();

	bool hasFeature(const std::string& name) const;
	bool hasGlobalFeature(const std::string& name) const;
	
private:
	static CustomFeatures loadCustomFeatures(pugi::xml_node node);
};

class EmulatorFeatures
{
public:
	enum Features
	{
		none = 0,
		ratio = 1,
		rewind = 2,
		smooth = 4,
		shaders = 8,
		pixel_perfect = 16,
		decoration = 32,
		latency_reduction = 64,
		game_translation = 128,
		autosave = 256,
		netplay = 512,
		fullboot = 1024,
		emulated_wiimotes = 2048,
		screen_layout = 4096,
		internal_resolution = 8192,
		videomode = 16384,
		colorization = 32768,
		padTokeyboard = 65536,
		cheevos = 131072,
		autocontrollers = 262144,

		all = 0x0FFFFFFF
	};

	static Features parseFeatures(const std::string features);
};

EmulatorFeatures::Features operator|(EmulatorFeatures::Features a, EmulatorFeatures::Features b);
EmulatorFeatures::Features operator&(EmulatorFeatures::Features a, EmulatorFeatures::Features b);

struct SystemFeature
{
	SystemFeature()
	{
		features = EmulatorFeatures::Features::none;
	}

	std::string name;
	EmulatorFeatures::Features features;
	CustomFeatures customFeatures;
};

struct CoreData
{
	CoreData() 
	{ 
		netplay = false; 
		isDefault = false;
		features = EmulatorFeatures::Features::none;
	}

	std::string name;
	bool netplay;
	bool isDefault;
	
	std::string customCommandLine;
	CustomFeatures customFeatures;
	std::vector<std::string> incompatibleExtensions;

	EmulatorFeatures::Features features;

	std::vector<SystemFeature> systemFeatures;
};

struct EmulatorData
{
	EmulatorData()
	{
		features = EmulatorFeatures::Features::none;
	}

	std::string name;	
	std::vector<CoreData> cores;

	std::string customCommandLine;
	CustomFeatures customFeatures;
	std::vector<std::string> incompatibleExtensions;

	EmulatorFeatures::Features features;

	std::vector<SystemFeature> systemFeatures;
};

#endif // ES_CUSTOMFEATURES_H
