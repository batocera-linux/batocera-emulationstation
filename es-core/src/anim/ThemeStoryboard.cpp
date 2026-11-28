#include "ThemeStoryboard.h"
#include "Log.h"
#include "utils/StringUtil.h"
#include "utils/HtmlColor.h"
#include "resources/ResourceManager.h"

namespace
{
	std::shared_ptr<const ThemeStoryboardDefinition> emptyStoryboardDefinition()
	{
		static const auto definition = std::make_shared<const ThemeStoryboardDefinition>();
		return definition;
	}
}

ThemeStoryboard::ThemeStoryboard() : mDefinition(emptyStoryboardDefinition()) {}

bool ThemeStoryboard::isAnimationEnabled(std::size_t index) const
{
	const auto it = mEnabledOverrides.find(index);
	return it == mEnabledOverrides.end() ? mDefinition->animations.at(index)->enabled : it->second;
}

void ThemeStoryboard::setAnimationEnabled(std::size_t index, bool enabled)
{
	if (enabled == mDefinition->animations.at(index)->enabled)
		mEnabledOverrides.erase(index);
	else
		mEnabledOverrides[index] = enabled;
}

#define RESOLVEVAR(x) variables.resolvePlaceholders(x)

bool ThemeStoryboard::fromXmlNode(const pugi::xml_node& root, const std::map<std::string, ThemeData::ElementPropertyType>& typeMap, const std::string& relativePath, const ThemeVariables& variables)
{
	if (strcmp(root.name(), "storyboard") != 0)
		return false;

	// Other storyboards may share this definition.
	auto definition = std::make_shared<ThemeStoryboardDefinition>(*mDefinition);

	definition->repeat = 1;
	definition->eventName = RESOLVEVAR(root.attribute("event").as_string());

	std::string sbrepeat = RESOLVEVAR(root.attribute("repeat").as_string());
	if (sbrepeat == "forever" || sbrepeat == "infinite")
		definition->repeat = 0;
	else if (!sbrepeat.empty() && sbrepeat != "none")
		definition->repeat = Utils::String::toInteger(sbrepeat);

	sbrepeat = RESOLVEVAR(root.attribute("repeatAt").as_string());
	if (sbrepeat.empty())
		sbrepeat = RESOLVEVAR(root.attribute("repeatat").as_string());

	if (!sbrepeat.empty())
	{
		if (definition->repeat == 1)
			definition->repeat = 0;

		definition->repeatAt = Utils::String::toInteger(sbrepeat);
	}

	for (pugi::xml_node node = root.child("animation"); node; node = node.next_sibling("animation"))
	{
		std::string prop = node.attribute("property").as_string();
		if (prop.empty())
			continue;

		ThemeData::ElementPropertyType type = (ThemeData::ElementPropertyType) -1;

		auto typeIt = typeMap.find(prop);
		if (typeIt != typeMap.cend())
			type = typeIt->second;
		else
		{
			if (Utils::String::startsWith(prop, "shader."))
				type = ThemeData::ElementPropertyType::FLOAT;
			else
			{
				LOG(LogWarning) << "Unknown storyboard property type \"" << prop << "\"";
				continue;
			}
		}

		std::shared_ptr<ThemeAnimation> anim;

		switch (type)
		{
		case ThemeData::ElementPropertyType::NORMALIZED_RECT:
			anim = std::make_shared<ThemeVector4Animation>();
			if (node.attribute("from")) anim->from = Vector4f::parseString(RESOLVEVAR(node.attribute("from").as_string()));
			if (node.attribute("to")) anim->to = Vector4f::parseString(RESOLVEVAR(node.attribute("to").as_string()));
			break;

		case ThemeData::ElementPropertyType::NORMALIZED_PAIR:
			anim = std::make_shared<ThemeVector2Animation>();
			if (node.attribute("from")) anim->from = Vector2f::parseString(RESOLVEVAR(node.attribute("from").as_string()));
			if (node.attribute("to")) anim->to = Vector2f::parseString(RESOLVEVAR(node.attribute("to").as_string()));
			break;

		case ThemeData::ElementPropertyType::COLOR:
			anim = std::make_shared<ThemeColorAnimation>();
			if (node.attribute("from")) anim->from = Utils::HtmlColor::parse(RESOLVEVAR(node.attribute("from").as_string()));
			if (node.attribute("to")) anim->to = Utils::HtmlColor::parse(RESOLVEVAR(node.attribute("to").as_string()));
			break;

		case ThemeData::ElementPropertyType::FLOAT:
			anim = std::make_shared<ThemeFloatAnimation>();
			if (node.attribute("from")) anim->from = Utils::String::toFloat(RESOLVEVAR(node.attribute("from").as_string()));
			if (node.attribute("to")) anim->to = Utils::String::toFloat(RESOLVEVAR(node.attribute("to").as_string()));
			break;

		case ThemeData::ElementPropertyType::PATH:
			anim = std::make_shared<ThemePathAnimation>();
			if (node.attribute("from")) anim->from = Utils::FileSystem::resolveRelativePath(RESOLVEVAR(node.attribute("from").as_string()), relativePath, true);
			if (node.attribute("to")) anim->to = Utils::FileSystem::resolveRelativePath(RESOLVEVAR(node.attribute("to").as_string()), relativePath, true);
			break;

		case ThemeData::ElementPropertyType::STRING:
			anim = std::make_shared<ThemeStringAnimation>();
			if (node.attribute("from")) anim->from = RESOLVEVAR(node.attribute("from").as_string());
			if (node.attribute("to")) anim->to = RESOLVEVAR(node.attribute("to").as_string());
			break;

		case ThemeData::ElementPropertyType::BOOLEAN:
			anim = std::make_shared<ThemeBoolAnimation>();
			if (node.attribute("from")) anim->from = Utils::String::toBoolean(RESOLVEVAR(node.attribute("from").as_string()));
			if (node.attribute("to")) anim->to = Utils::String::toBoolean(RESOLVEVAR(node.attribute("to").as_string()));
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
				if (strcmp(xattr.name(), "enabled") == 0)
				{
					std::string enabled = xattr.as_string();
					anim->enabled = (enabled == "true" || enabled == "1");
					
					if (enabled.find("{") != std::string::npos && enabled.find(":") != std::string::npos && enabled.find("}") != std::string::npos)
						anim->enabledExpression = enabled;
				}
				else if (strcmp(xattr.name(), "begin") == 0)
					anim->begin = Utils::String::toInteger(xattr.as_string());
				else if (strcmp(xattr.name(), "duration") == 0)
					anim->duration = Utils::String::toInteger(xattr.as_string());
				else if (strcmp(xattr.name(), "repeat") == 0)
				{
					std::string arepeat = xattr.as_string();
					if (arepeat == "forever" || arepeat == "infinite")
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

			definition->animations.push_back(std::move(anim));
		}
	}

	for (pugi::xml_node node = root.child("sound"); node; node = node.next_sibling("sound"))
	{
		std::string path = RESOLVEVAR(node.attribute("path").as_string());
		if (path.empty())
			continue;

		path = Utils::FileSystem::resolveRelativePath(path, relativePath, true);
		if (!ResourceManager::getInstance()->fileExists(path))
			continue;

		auto sound = std::make_shared<ThemeSoundAnimation>();
		sound->propertyName = "sound";
		sound->to = path;

		for (pugi::xml_attribute xattr : node.attributes())
		{
			if (strcmp(xattr.name(), "begin") == 0 || strcmp(xattr.name(), "at") == 0)
				sound->begin = Utils::String::toInteger(xattr.as_string());
			else if (strcmp(xattr.name(), "duration") == 0)
				sound->duration = Utils::String::toInteger(xattr.as_string());
			else if (strcmp(xattr.name(), "repeat") == 0)
			{
				std::string arepeat = xattr.as_string();
				if (arepeat == "forever")
					sound->repeat = 0;
				else if (!arepeat.empty() && arepeat != "none")
					sound->repeat = Utils::String::toInteger(arepeat);
			}
			else if (strcmp(xattr.name(), "autoreverse") == 0 || strcmp(xattr.name(), "autoReverse") == 0)
			{
				std::string areverse = xattr.as_string();
				sound->autoReverse = (areverse == "true" || areverse == "1");
			}
		}

		definition->animations.push_back(std::move(sound));
	}

	mDefinition = std::move(definition);
	return !mDefinition->animations.empty();
}
