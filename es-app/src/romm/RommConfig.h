#pragma once
#ifndef ES_APP_ROMM_ROMM_CONFIG_H
#define ES_APP_ROMM_ROMM_CONFIG_H

#include "romm/RommMedia.h"

#include <set>
#include <string>

namespace Romm
{
	// batocera.conf keys (SystemConf)
	static const char* const CONF_ENABLED  = "romm.enabled";
	static const char* const CONF_URL      = "romm.url";
	static const char* const CONF_INSECURE = "romm.insecure";
	// Media settings (off when absent), one per media type (see RommMedia.h):
	//   romm.sync.<media>    : cached from the library for RomM games not on the device
	//   romm.withrom.<media> : downloaded along with a rom, next to it
	static const char* const CONF_SYNC_PREFIX    = "romm.sync.";
	static const char* const CONF_IMAGE          = "romm.image"; // IMAGE SHOWN: a media key, or "auto" (default)
	static const char* const CONF_CACHE_SIZE     = "romm.cachesize"; // MEDIA CACHE SIZE: "auto" (default), megabytes, "unlimited"
	static const char* const CONF_WITHROM_PREFIX = "romm.withrom.";
	static const char* const CONF_FAVORITES = "romm.favorites"; // RomM favorites -> ES favorites (off when absent)
	static const char* const CONF_PLAYSESSIONS = "romm.playsessions"; // send play sessions to RomM (off when absent)
	static const char* const CONF_HIDDEN_SYSTEMS = "romm.hiddensystems"; // comma separated ES system names
	static const char* const CONF_COLLECTIONS = "romm.collections";      // comma separated collection keys (c<id>, s<id>), none when absent

	// Settings and credentials for the RomM integration.
	// Non-secret settings live in batocera.conf; the token lives in a separate
	// credentials file (0600) because batocera.conf is often shared for support.
	struct RommConfig
	{
		bool enabled = false;
		std::string url;               // as typed by the user
		bool allowSelfSigned = false;
		std::set<std::string> syncMedia;     // media keys cached from the library
		std::set<std::string> withRomMedia;  // media keys downloaded along with a rom

		std::string image = IMAGE_AUTOMATIC;  // IMAGE SHOWN
		std::string cacheSize = "auto";       // MEDIA CACHE SIZE

		bool syncs(Media type) const { return syncMedia.count(mediaInfo(type).key) > 0; }
		bool withRom(Media type) const { return withRomMedia.count(mediaInfo(type).key) > 0; }

		// The media type chosen as IMAGE SHOWN; false when automatic (the cover, with its fallbacks)
		bool imageMedia(Media& type) const;

		// Fetched from the library: its list, or the image shown. With a downloaded rom: both lists,
		// or the image shown.
		bool fetchesFromLibrary(Media type) const;
		bool fetchesWithRom(Media type) const;
		bool syncFavorites = false;
		bool syncPlaySessions = false;
		std::set<std::string> hiddenSystems;   // RomM games of these systems are not shown
		std::set<std::string> collections;     // RomM collections turned into ES collections

		// credentials.json
		std::string token;
		std::string tokenExpiresAt;
		std::string username;
		std::string deviceId;          // RomM device id (device pairing)
		std::string clientDeviceId;    // stable random id sent when pairing
		std::string pairedUrl;         // the server the token was issued by
		std::string scopes;            // granted at pairing, comma separated ("" when unknown: older pairing)

		bool hasScope(const std::string& scope) const;

		static RommConfig load();

		bool saveCredentials() const;
		static void clearCredentials(bool keepClientDeviceId = true);

		// URL without trailing slash, scheme added if missing
		std::string baseUrl() const;

		// Paired with the server currently configured
		bool isPaired() const;

		// Enabled, configured and paired
		bool isUsable() const { return enabled && !baseUrl().empty() && isPaired(); }
	};

	std::string normalizeUrl(const std::string& url);

	void saveHiddenSystems(const std::set<std::string>& systems);
	void saveSelectedCollections(const std::set<std::string>& keys);

	std::string getConfigDirectory();     // e.g. /userdata/system/configs/romm
	std::string getCredentialsPath();
	std::string getCacheRoot();           // e.g. /userdata/system/cache/romm
	std::string getServerCacheDir(const std::string& baseUrl); // cache root + hash of the server URL

	std::string newClientDeviceId();
	std::string getDeviceName();

	// Writes a file through a temporary file and a rename
	bool writeFileAtomic(const std::string& path, const std::string& content);
}

#endif
