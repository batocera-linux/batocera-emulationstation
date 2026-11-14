#pragma once
#ifndef ES_APP_ROMM_ROMM_PLATFORMS_H
#define ES_APP_ROMM_ROMM_PLATFORMS_H

#include <map>
#include <set>
#include <string>
#include <vector>

namespace Romm
{
	const std::map<std::string, std::vector<std::string>>& getGroutPlatformMap();

	// Picks the ES system for a RomM platform, among the systems defined in es_systems.
	// Order: Grout's table for the platform slug, the server's platform binding for the
	// fs_slug, Grout's table for the fs_slug, then fs_slug / slug used as a system name.
	// Returns "" when nothing matches.
	std::string mapPlatformToSystem(const std::string& slug, const std::string& fsSlug,
		const std::map<std::string, std::string>& platformsBinding, const std::set<std::string>& esSystems);
}

#endif
