#include "romm/RommPlatforms.h"
#include "utils/StringUtil.h"

namespace Romm
{
	// Additions to the generated Grout table (RommPlatformsData.cpp, not edited by hand): systems
	// Batocera supports through add-ons, which Grout does not list
	static const std::map<std::string, std::vector<std::string>>& extraPlatformMap()
	{
		static const std::map<std::string, std::vector<std::string>> extra =
		{
			{ "switch", { "switch" } },          // RomM slug; Batocera: /userdata/roms/switch (add-on)
			{ "nintendo-switch", { "switch" } },

			// Atari Jaguar CD: RomM's slug and its usual aliases
			{ "atari-jaguar-cd", { "jaguarcd" } },
			{ "atarijaguarcd", { "jaguarcd" } },

			// RomM arcade slugs (rommapp/romm#5273) that are not Batocera system names; the others
			// (naomi, atomiswave, model3...) are, and match by name
			{ "mame-advmame", { "mame" } },   // AdvanceMAME romsets: Batocera's mame system
			{ "fba", { "fbneo" } },           // FinalBurn Alpha romsets
			{ "laserdisc", { "daphne" } },    // RomM alias of daphne
			{ "naomigd", { "naomi" } },       // NAOMI GD-ROM: Batocera's naomi system runs both
			{ "stv", { "mame" } },            // ST-V: run by MAME, no Batocera system of its own
		};

		return extra;
	}

	std::string mapPlatformToSystem(const std::string& slug, const std::string& fsSlug,
		const std::map<std::string, std::string>& platformsBinding, const std::set<std::string>& esSystems)
	{
		std::vector<std::string> candidates;

		auto addFromTable = [&candidates](const std::string& key)
		{
			if (key.empty())
				return;

			for (auto table : { &getGroutPlatformMap(), &extraPlatformMap() })
			{
				auto it = table->find(Utils::String::toLower(key));
				if (it != table->cend())
					for (auto& system : it->second)
						candidates.push_back(system);
			}
		};

		// A binding set on the server comes first. Then the folder name when it is an ES system: a
		// library laid out in Batocera folders (naomi/, atomiswave/...) keeps its systems even when
		// RomM files several of them under one slug (arcade, before RomM 5273).
		auto binding = platformsBinding.find(fsSlug);
		if (binding != platformsBinding.cend())
			addFromTable(binding->second);

		candidates.push_back(Utils::String::toLower(fsSlug));

		addFromTable(slug);
		addFromTable(fsSlug);

		candidates.push_back(Utils::String::toLower(slug));

		for (auto& candidate : candidates)
			if (!candidate.empty() && esSystems.find(candidate) != esSystems.cend())
				return candidate;

		return "";
	}
}
