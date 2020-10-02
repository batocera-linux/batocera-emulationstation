#include "ThemeStoryboard.h"
#include "Log.h"
#include "utils/StringUtil.h"

ThemeStoryboard::~ThemeStoryboard()
{
	for (auto anim : animations)
		delete anim;

	animations.clear();
}

bool ThemeStoryboard::fromXmlNode(const pugi::xml_node& root, std::map<std::string, ThemeData::ElementPropertyType> typeMap)
{
	if (std::string(root.name()) != "storyboard")
		return false;

	this->repeat = 1;
	this->eventName = root.attribute("event").as_string();

	std::string sbrepeat = root.attribute("repeat").as_string();
	if (sbrepeat == "forever")
		this->repeat = 0;
	else if (!sbrepeat.empty() && sbrepeat != "none")
		this->repeat = atoi(sbrepeat.c_str());

	for (pugi::xml_node node = root.child("animation"); node; node = node.next_sibling("animation"))
	{
		std::string prop = node.attribute("property").as_string();
		if (prop.empty())
			continue;

		auto typeIt = typeMap.find(prop);
		if (typeIt == typeMap.cend())
		{
			LOG(LogWarning) << "Unknown storyboard property type \"" << prop << "\"";
			continue;
		}

		ThemeAnimation* anim = nullptr;

		ThemeData::ElementPropertyType type = typeIt->second;

		switch (type)
		{
		case ThemeData::ElementPropertyType::NORMALIZED_RECT:
			anim = new ThemeVector4Animation();
			if (node.attribute("from")) anim->from = Vector4f::parseString(node.attribute("from").as_string());
			if (node.attribute("to")) anim->to = Vector4f::parseString(node.attribute("to").as_string());
			break;

		case ThemeData::ElementPropertyType::NORMALIZED_PAIR:
			anim = new ThemeVector2Animation();
			if (node.attribute("from")) anim->from = Vector2f::parseString(node.attribute("from").as_string());
			if (node.attribute("to")) anim->to = Vector2f::parseString(node.attribute("to").as_string());		
			break;

		case ThemeData::ElementPropertyType::COLOR:			
			anim = new ThemeColorAnimation();
			if (node.attribute("from")) anim->from = getHexColor(node.attribute("from").as_string());
			if (node.attribute("to")) anim->to = getHexColor(node.attribute("to").as_string());
			break;

		case ThemeData::ElementPropertyType::FLOAT:
			anim = new ThemeFloatAnimation();
			if (node.attribute("from")) anim->from = (float)atof(node.attribute("from").as_string());	
			if (node.attribute("to")) anim->to = (float)atof(node.attribute("to").as_string());
			break;

		default:
			LOG(LogWarning) << "Unsupported animation property type \"" << prop << "\"";
			continue;
		}

		if (anim != nullptr)
		{
			anim->propertyName = prop;

			if (node.attribute("begin")) anim->begin = atoi(node.attribute("begin").as_string());
			if (node.attribute("duration")) anim->duration = atoi(node.attribute("duration").as_string());

			if (node.attribute("repeat"))
			{
				std::string arepeat = node.attribute("repeat").as_string();
				if (arepeat == "forever") anim->repeat = 0; else if (!arepeat.empty() && arepeat != "none") anim->repeat = atoi(arepeat.c_str());
			}

			if (node.attribute("autoreverse"))
			{
				std::string areverse = node.attribute("autoreverse").as_string();
				anim->autoReverse = (areverse == "true" || areverse == "1");
			}

			std::string mode = "linear";

			if (node.attribute("mode"))
				mode = node.attribute("mode").as_string();
			else if (node.attribute("easingMode"))
				mode = node.attribute("easingMode").as_string();
			
			mode = Utils::String::toLower(mode);

			if (mode == "easein")
				anim->easingMode = ThemeAnimation::EasingMode::EaseIn;
			else if (mode == "easeincubic")
				anim->easingMode = ThemeAnimation::EasingMode::EaseInCubic;
			else if (mode == "easeinquint")
				anim->easingMode = ThemeAnimation::EasingMode::EaseInQuint;
			else if (mode == "easeout")
				anim->easingMode = ThemeAnimation::EasingMode::EaseOut;
			else if (mode == "easeoutcubic")
				anim->easingMode = ThemeAnimation::EasingMode::EaseOutCubic;
			else if (mode == "easeoutquint")
				anim->easingMode = ThemeAnimation::EasingMode::EaseOutQuint;
			else if (mode == "easeinout")
				anim->easingMode = ThemeAnimation::EasingMode::EaseInOut;
			else if (mode == "bump")
				anim->easingMode = ThemeAnimation::EasingMode::Bump;
			else
				anim->easingMode = ThemeAnimation::EasingMode::Linear;

			animations.push_back(anim);
		}
	}

	return animations.size() > 0;
}