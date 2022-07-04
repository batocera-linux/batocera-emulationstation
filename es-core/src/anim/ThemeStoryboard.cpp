#include "ThemeStoryboard.h"
#include "Log.h"
#include "utils/StringUtil.h"
#include "utils/HtmlColor.h"

ThemeStoryboard::ThemeStoryboard(const ThemeStoryboard& src)
{
	eventName = src.eventName;
	repeat = src.repeat;
	repeatAt = src.repeatAt;

	for (auto anim : src.animations)
	{
		if (dynamic_cast<ThemeFloatAnimation*>(anim) != nullptr)
			animations.push_back(new ThemeFloatAnimation((const ThemeFloatAnimation&)(*anim)));
		else if (dynamic_cast<ThemeColorAnimation*>(anim) != nullptr)
			animations.push_back(new ThemeColorAnimation((const ThemeColorAnimation&)(*anim)));
		else if (dynamic_cast<ThemeVector2Animation*>(anim) != nullptr)
			animations.push_back(new ThemeVector2Animation((const ThemeVector2Animation&)(*anim)));
		else if (dynamic_cast<ThemeVector4Animation*>(anim) != nullptr)
			animations.push_back(new ThemeVector4Animation((const ThemeVector4Animation&)(*anim)));
		else if (dynamic_cast<ThemeStringAnimation*>(anim) != nullptr)
			animations.push_back(new ThemeStringAnimation((const ThemeStringAnimation&)(*anim)));
		else if (dynamic_cast<ThemePathAnimation*>(anim) != nullptr)
			animations.push_back(new ThemePathAnimation((const ThemePathAnimation&)(*anim)));
	}
}

ThemeStoryboard::~ThemeStoryboard()
{
	for (auto anim : animations)
		delete anim;

	animations.clear();
}

bool ThemeStoryboard::fromXmlNode(const pugi::xml_node& root, const std::map<std::string, ThemeData::ElementPropertyType>& typeMap)
{
	if (strcmp(root.name(), "storyboard") != 0)
		return false;

	this->repeat = 1;
	this->eventName = root.attribute("event").as_string();

	std::string sbrepeat = root.attribute("repeat").as_string();
	if (sbrepeat == "forever")
		this->repeat = 0;
	else if (!sbrepeat.empty() && sbrepeat != "none")
		this->repeat = Utils::String::toInteger(sbrepeat);

	sbrepeat = root.attribute("repeatAt").as_string();
	if (sbrepeat.empty())
		sbrepeat = root.attribute("repeatat").as_string();

	if (!sbrepeat.empty())
	{
		if (this->repeat = 1)
			this->repeat = 0;

		this->repeatAt = Utils::String::toInteger(sbrepeat);
	}

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
			if (node.attribute("from")) anim->from = Utils::HtmlColor::parse(node.attribute("from").as_string());
			if (node.attribute("to")) anim->to = Utils::HtmlColor::parse(node.attribute("to").as_string());
			break;

		case ThemeData::ElementPropertyType::FLOAT:
			anim = new ThemeFloatAnimation();
			if (node.attribute("from")) anim->from = Utils::String::toFloat(node.attribute("from").as_string());	
			if (node.attribute("to")) anim->to = Utils::String::toFloat(node.attribute("to").as_string());
			break;

		case ThemeData::ElementPropertyType::PATH:
			anim = new ThemePathAnimation();
			if (node.attribute("from")) anim->from = node.attribute("from").as_string();
			if (node.attribute("to")) anim->to = node.attribute("to").as_string();
			break;

		case ThemeData::ElementPropertyType::STRING:
			anim = new ThemeStringAnimation();
			if (node.attribute("from")) anim->from = node.attribute("from").as_string();
			if (node.attribute("to")) anim->to = node.attribute("to").as_string();
			break;

		default:
			LOG(LogWarning) << "Unsupported animation property type \"" << prop << "\"";
			continue;
		}

		if (anim != nullptr)
		{
			anim->propertyName = prop;

			std::string mode = "linear";

			for (pugi::xml_attribute xattr : node.attributes())
			{
				if (strcmp(xattr.name(), "begin") == 0)
					anim->begin = Utils::String::toInteger(xattr.as_string());
				else if (strcmp(xattr.name(), "duration") == 0)
					anim->duration = Utils::String::toInteger(xattr.as_string());
				else if (strcmp(xattr.name(), "repeat") == 0)
				{
					std::string arepeat = xattr.as_string();
					if (arepeat == "forever") 
						anim->repeat = 0; 
					else if (!arepeat.empty() && arepeat != "none") 						
						anim->repeat = Utils::String::toInteger(arepeat);
				}								
				else if (strcmp(xattr.name(), "autoreverse") == 0 || strcmp(xattr.name(), "autoReverse") == 0)
				{
					std::string areverse = xattr.as_string();
					anim->autoReverse = (areverse == "true" || areverse == "1");
				}
				else if (strcmp(xattr.name(), "mode") == 0 || strcmp(xattr.name(), "easingMode") == 0)
					mode = Utils::String::toLower(xattr.as_string());
			}

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