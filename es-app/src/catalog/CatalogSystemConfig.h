#pragma once

// What the emulator catalog needs to write es_systems_<system>.cfg files.
//
// The catalog only gives system names ("psx", "gamecube"...). This file keeps
// the matching full name, extensions, platform and theme, builds the XML entry
// that adds a standalone emulator to an existing system, and writes/removes the
// per-system files.

#include "catalog/EmulatorCatalog.h"

#include <pugixml/src/pugixml.hpp>
#include <string>
#include <vector>

namespace catalog
{
	struct SystemDefinition
	{
		const char* name;
		const char* fullName;
		const char* extensions;
		const char* platform;
		const char* theme;
	};

	struct SystemConfigPaths
	{
		std::string configDir;                // where es_systems_<system>.cfg is written
		std::string baseConfigPath;           // SystemData::getConfigPath()
		std::string homePath;                 // Paths::getHomePath(), for ~/ROMs/<system>
		std::vector<std::string> roots;       // config, exe, system config and data dirs
	};

	// The definition of a system the catalog can install, or nullptr.
	const SystemDefinition* findSystemDefinition(const std::string& name);

	// A system name is used in a file name: only keep it when it cannot escape
	// the config directory. "+" is allowed (msx2+).
	bool isSafeSystemName(const std::string& name);

	// "es_systems_psx.cfg"
	std::string systemConfigFileName(const std::string& system);

	// The command that starts the installed emulator, ending with %ROM%, or an
	// empty string when installed.json has no command. Arguments are shell-quoted.
	std::string buildEmulatorCommand(const InstalledItem& item);

	// Comment put at the top of the files ES generates. Files without it are
	// user files: ES only adds/removes its emulator, and never deletes them.
	std::string generatedConfigMarker();

	// Adds <emulator name="id" catalog="true" command="..."/> to <emulators>,
	// creating the block when needed. Reuses an existing entry with the same name.
	pugi::xml_node addEmulator(pugi::xml_node system, const std::string& id, const std::string& command);

	// Appends a <system> entry for `system`.
	//
	// `existing` is the effective system entry (base config and extras merged),
	// or an empty node. When `full` is false only <name> and <emulators> are
	// written: the merge in SystemData replaces just <emulators>, so the base
	// entry keeps its path, extensions, platform and theme. When `full` is true
	// the other tags are copied from `existing`, or from `definition` when there
	// is no existing entry (a new system, not present in any config).
	pugi::xml_node appendSystem(pugi::xml_node systemList, const std::string& system,
		const SystemDefinition* definition, pugi::xml_node existing,
		const std::string& emulatorId, const std::string& command, bool full);

	// Creates or updates es_systems_<system>.cfg for every system of the emulator.
	// Existing system entries are copied (path, extensions, cores...) and the
	// emulator is added to them. Returns false on the first file it cannot write.
	bool writeSystemConfigs(const Emulator& emulator, const InstalledItem& item,
		const SystemConfigPaths& paths, std::string* error = nullptr);

	// Removes the emulator from every es_systems_*.cfg of configDir. A generated
	// file left without any catalog emulator is deleted; user files are kept.
	bool removeSystemConfigs(const std::string& emulatorId, const std::string& configDir,
		std::string* error = nullptr);
}
