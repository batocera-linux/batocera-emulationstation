#include "romm/RommConfig.h"

#include "SystemConf.h"
#include "Paths.h"
#include "Log.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

#include <chrono>
#include <cstdio>
#include <functional>
#include <random>
#include <sstream>
#include <iomanip>

#ifndef WIN32
#include <sys/stat.h>
#endif

namespace Romm
{
	static std::string jsonString(const rapidjson::Value& obj, const char* key)
	{
		if (obj.IsObject() && obj.HasMember(key) && obj[key].IsString())
			return obj[key].GetString();

		return "";
	}

	std::string normalizeUrl(const std::string& url)
	{
		std::string ret = Utils::String::trim(url);
		if (ret.empty())
			return ret;

		if (ret.find("://") == std::string::npos)
			ret = "http://" + ret;

		while (!ret.empty() && ret.back() == '/')
			ret.pop_back();

		return ret;
	}

	void saveHiddenSystems(const std::set<std::string>& systems)
	{
		std::string value;
		for (auto& name : systems)
		{
			if (!value.empty())
				value += ",";

			value += name;
		}

		SystemConf::getInstance()->set(CONF_HIDDEN_SYSTEMS, value);
	}

	void saveSelectedCollections(const std::set<std::string>& keys)
	{
		std::string value;
		for (auto& key : keys)
		{
			if (!value.empty())
				value += ",";

			value += key;
		}

		SystemConf::getInstance()->set(CONF_COLLECTIONS, value);
	}

	std::string getConfigDirectory()
	{
		return Utils::FileSystem::getParent(Paths::getUserEmulationStationPath()) + "/romm";
	}

	std::string getCredentialsPath()
	{
		return getConfigDirectory() + "/credentials.json";
	}

	std::string getCacheRoot()
	{
		// /userdata/system/configs/emulationstation -> /userdata/system/cache/romm
		std::string configs = Utils::FileSystem::getParent(Paths::getUserEmulationStationPath());
		return Utils::FileSystem::getParent(configs) + "/cache/romm";
	}

	std::string getServerCacheDir(const std::string& baseUrl)
	{
		// FNV-1a 64 bits: stable across runs and builds (std::hash is not guaranteed to be)
		unsigned long long h = 1469598103934665603ULL;
		for (unsigned char c : baseUrl)
		{
			h ^= c;
			h *= 1099511628211ULL;
		}

		std::stringstream ss;
		ss << std::hex << std::setw(16) << std::setfill('0') << h;
		return getCacheRoot() + "/" + ss.str();
	}

	std::string newClientDeviceId()
	{
		std::random_device rd;
		std::mt19937_64 gen(((unsigned long long)rd() << 32) ^ rd() ^ (unsigned long long)std::chrono::steady_clock::now().time_since_epoch().count());

		std::stringstream ss;
		ss << std::hex << std::setfill('0') << std::setw(16) << gen() << std::setw(16) << gen();
		return ss.str();
	}

	std::string getDeviceName()
	{
		std::string name = SystemConf::getInstance()->get("system.hostname");
		if (name.empty())
			name = "BATOCERA";

		return name;
	}

	bool writeFileAtomic(const std::string& path, const std::string& content)
	{
		std::string tmp = path + ".tmp";

		FILE* f = fopen(tmp.c_str(), "wb");
		if (f == nullptr)
		{
			LOG(LogError) << "RomM: unable to write " << tmp;
			return false;
		}

		bool ok = fwrite(content.data(), 1, content.size(), f) == content.size();
		ok = (fflush(f) == 0) && ok;
		fclose(f);

		if (!ok)
		{
			Utils::FileSystem::removeFile(tmp);
			LOG(LogError) << "RomM: unable to write " << tmp << " (disk full ?)";
			return false;
		}

		if (!Utils::FileSystem::renameFile(tmp, path, true))
		{
			Utils::FileSystem::removeFile(tmp);
			LOG(LogError) << "RomM: unable to rename " << tmp << " to " << path;
			return false;
		}

		return true;
	}

	std::string RommConfig::baseUrl() const
	{
		return normalizeUrl(url);
	}

	bool RommConfig::imageMedia(Media& type) const
	{
		for (auto& m : allMedia())
		{
			if (m.canBeImage && image == m.key)
			{
				type = m.type;
				return true;
			}
		}

		return false;
	}

	bool RommConfig::fetchesFromLibrary(Media type) const
	{
		Media image;
		if (imageMedia(image) && image == type)
			return true;

		return !mediaInfo(type).imageOnly && syncs(type);
	}

	bool RommConfig::fetchesWithRom(Media type) const
	{
		Media image;
		if (imageMedia(image) && image == type)
			return true;

		// The large cover comes with the rom when the cover does (the library keeps the small one)
		if (type == Media::CoverLarge && withRom(Media::Cover))
			return true;

		// The media synced from the library come too: a game keeps what it showed before its download,
		// whatever the cache holds when it is downloaded
		return !mediaInfo(type).imageOnly && (withRom(type) || syncs(type));
	}

	bool RommConfig::hasScope(const std::string& scope) const
	{
		for (auto& s : Utils::String::split(scopes, ',', true))
			if (Utils::String::trim(s) == scope)
				return true;

		return false;
	}

	bool RommConfig::isPaired() const
	{
		return !token.empty() && !pairedUrl.empty() && pairedUrl == baseUrl();
	}

	RommConfig RommConfig::load()
	{
		RommConfig cfg;

		auto conf = SystemConf::getInstance();
		cfg.enabled = conf->getBool(CONF_ENABLED, false);
		cfg.url = conf->get(CONF_URL);
		cfg.allowSelfSigned = conf->getBool(CONF_INSECURE, false);
		cfg.cacheSize = conf->get(CONF_CACHE_SIZE);
		if (cfg.cacheSize.empty())
			cfg.cacheSize = "auto";

		cfg.image = conf->get(CONF_IMAGE);
		if (cfg.image.empty())
			cfg.image = IMAGE_AUTOMATIC;

		for (auto& m : allMedia())
		{
			// romm.sync.<media>, else the legacy key (romm.covers / romm.manuals / romm.walkthroughs)
			std::string syncKey = std::string(CONF_SYNC_PREFIX) + m.key;
			bool sync = conf->get(syncKey).empty() && m.legacyConf[0] != '\0' ? conf->getBool(m.legacyConf, false) : conf->getBool(syncKey, false);

			if (sync)
				cfg.syncMedia.insert(m.key);

			if (conf->getBool(std::string(CONF_WITHROM_PREFIX) + m.key, false))
				cfg.withRomMedia.insert(m.key);
		}
		cfg.syncFavorites = conf->getBool(CONF_FAVORITES, false);
		cfg.syncPlaySessions = conf->getBool(CONF_PLAYSESSIONS, false);

		for (auto& name : Utils::String::split(conf->get(CONF_HIDDEN_SYSTEMS), ','))
		{
			std::string trimmed = Utils::String::trim(name);
			if (!trimmed.empty())
				cfg.hiddenSystems.insert(trimmed);
		}

		for (auto& key : Utils::String::split(conf->get(CONF_COLLECTIONS), ','))
		{
			std::string trimmed = Utils::String::trim(key);
			if (!trimmed.empty())
				cfg.collections.insert(trimmed);
		}

		std::string path = getCredentialsPath();
		if (!Utils::FileSystem::exists(path, false))
			return cfg;

		std::string text = Utils::FileSystem::readAllText(path);

		rapidjson::Document doc;
		doc.Parse(text.c_str());
		if (doc.HasParseError() || !doc.IsObject())
		{
			LOG(LogError) << "RomM: unable to parse " << path;
			return cfg;
		}

		cfg.token = jsonString(doc, "token");
		cfg.tokenExpiresAt = jsonString(doc, "token_expires_at");
		cfg.username = jsonString(doc, "username");
		cfg.deviceId = jsonString(doc, "device_id");
		cfg.clientDeviceId = jsonString(doc, "client_device_identifier");
		cfg.pairedUrl = jsonString(doc, "server");
		cfg.scopes = jsonString(doc, "scopes");

		return cfg;
	}

	bool RommConfig::saveCredentials() const
	{
		Utils::FileSystem::createDirectory(getConfigDirectory());

		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

		writer.StartObject();
		writer.Key("server");                   writer.String(pairedUrl.c_str());
		writer.Key("token");                    writer.String(token.c_str());
		writer.Key("token_expires_at");         writer.String(tokenExpiresAt.c_str());
		writer.Key("username");                 writer.String(username.c_str());
		writer.Key("device_id");                writer.String(deviceId.c_str());
		writer.Key("client_device_identifier"); writer.String(clientDeviceId.c_str());
		writer.Key("scopes");                   writer.String(scopes.c_str());
		writer.EndObject();

		std::string path = getCredentialsPath();

#ifndef WIN32
		// Create the temporary file with restricted rights before writing the secret into it
		std::string tmp = path + ".tmp";
		FILE* f = fopen(tmp.c_str(), "wb");
		if (f != nullptr)
		{
			fclose(f);
			chmod(tmp.c_str(), S_IRUSR | S_IWUSR);
		}
#endif

		bool ok = writeFileAtomic(path, buffer.GetString());

#ifndef WIN32
		if (ok)
			chmod(path.c_str(), S_IRUSR | S_IWUSR);
#endif
		return ok;
	}

	void RommConfig::clearCredentials(bool keepClientDeviceId)
	{
		RommConfig current = load();

		RommConfig empty;
		if (keepClientDeviceId)
			empty.clientDeviceId = current.clientDeviceId;

		if (empty.clientDeviceId.empty())
			Utils::FileSystem::removeFile(getCredentialsPath());
		else
			empty.saveCredentials();
	}
}
