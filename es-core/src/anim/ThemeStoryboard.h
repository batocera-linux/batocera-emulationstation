#pragma once

#include "ThemeAnimation.h"
#include "ThemeVariables.h"
#include <pugixml/src/pugixml.hpp>
#include <memory>
#include <map>
#include <vector>
#include <cstddef>
#include <string>

struct ThemeStoryboardDefinition
{
	std::string eventName;
	int repeat = 1; // 0 = forever
	int repeatAt = 0;
	std::vector<std::shared_ptr<const ThemeAnimation>> animations;
};

class ThemeStoryboard
{
public:
	ThemeStoryboard();
	ThemeStoryboard(const ThemeStoryboard&) = default;
	ThemeStoryboard& operator=(const ThemeStoryboard&) = default;
	~ThemeStoryboard() = default;

	const std::string& getEventName() const { return mDefinition->eventName; }
	const std::vector<std::shared_ptr<const ThemeAnimation>>& getAnimations() const
	{
		return mDefinition->animations;
	}
	std::shared_ptr<const ThemeStoryboardDefinition> getDefinition() const
	{
		return mDefinition;
	}

	bool isAnimationEnabled(std::size_t index) const;
	void setAnimationEnabled(std::size_t index, bool enabled);

	bool fromXmlNode(const pugi::xml_node& root,
		const std::map<std::string, ThemeData::ElementPropertyType>& typeMap,
		const std::string& relativePath, const ThemeVariables& variables);

private:
	std::shared_ptr<const ThemeStoryboardDefinition> mDefinition;
	std::map<std::size_t, bool> mEnabledOverrides;
};
