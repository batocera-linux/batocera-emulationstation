#pragma once
#ifndef ES_APP_ROMM_ROMM_COLLECTIONS_H
#define ES_APP_ROMM_ROMM_COLLECTIONS_H

#include <map>
#include <set>
#include <string>
#include <vector>

namespace Romm
{
	struct RommConfig;

	// A RomM collection (regular or smart), with its games already resolved to ES systems
	struct RommCollection
	{
		std::string key;        // "c<id>" regular, "s<id>" smart (ids of both kinds overlap)
		bool smart = false;
		bool favorite = false;  // the paired user's own favorites collection
		bool mine = true;       // owned by the paired user (others' public collections are listed too)
		std::string owner;      // owner_username
		std::string name;
		std::string updatedAt;
		std::vector<std::pair<std::string, std::string>> games; // system, file name
	};

	// <server cache>/collections.xml, written by the sync
	std::string getCollectionsSnapshotPath(const std::string& baseUrl);
	bool readCollectionsSnapshot(const std::string& path, std::vector<RommCollection>& collections);
	bool writeCollectionsSnapshot(const std::string& path, const std::vector<RommCollection>& collections);

	// Turns the selected RomM collections into ES custom collections (custom-<name>.cfg + CollectionSystemsCustom),
	// and removes the ones that are no longer selected. Must run before ES loads its collections.
	// active = false removes every RomM collection (RomM disabled, not paired, no library yet).
	// systemPaths : ES system name -> rom folder, normalized like SystemData does.
	void applyEsCollections(const RommConfig& config, bool active, const std::map<std::string, std::string>& systemPaths);

	// Name shown in the menu and given to the ES collection: another user's collection gets its
	// owner's name, so that e.g. two "Favourites" do not look the same
	std::string collectionDisplayName(const RommCollection& collection);

	// Name usable as a file name and in the comma separated CollectionSystemsCustom setting
	std::string sanitizeCollectionName(const std::string& name);
}

#endif
