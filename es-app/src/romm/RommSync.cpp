#include "romm/RommSync.h"
#include "romm/RommCatalog.h"
#include "romm/RommClient.h"
#include "romm/RommCollections.h"
#include "romm/RommBios.h"
#include "romm/RommPlaySessions.h"
#include "romm/RommWork.h"
#include "romm/RommMediaCache.h"
#include "romm/RommDownloader.h"
#include "romm/RommConfig.h"
#include "romm/RommPlatforms.h"

#include "HttpReq.h"
#include "Window.h"
#include "components/AsyncNotificationComponent.h"
#include "Log.h"
#include "LocaleES.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <rapidjson/document.h>
#include <regex>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <cstdio>
#include <cstdlib>

namespace Romm
{
	Window* RommSync::sWindow = nullptr;
	std::thread RommSync::sThread;
	std::mutex RommSync::sMutex;
	std::string RommSync::sStatus;
	std::atomic<bool> RommSync::sRunning(false);
	std::atomic<bool> RommSync::sCancel(false);
	std::atomic<bool> RommSync::sPendingChanges(false);
	std::atomic<bool> RommSync::sMediaEnabled[(size_t)Media::Count];

	static const int PAGE_SIZE = 500;

	// Increment when the snapshot gains or changes a field, or when the rules deciding which games are
	// kept change: a different format makes the next synchronization a full one (an incremental one
	// only fetches the games RomM reports changed, so existing games would keep the old data).
	static const int SNAPSHOT_FORMAT = 18;
	static const int WATERMARK_OVERLAP_SECONDS = 120;

	// RomM file categories that come with a game but are not a version of it (as in Grout)
	static const std::set<std::string> NOT_PLAYABLE = { "manual", "walkthrough", "soundtrack", "screenshot", "cheat", "patch", "update", "dlc" };

	// ---------------------------------------------------------------------------
	// Dates (thread-safe, no gmtime/timegm)
	// ---------------------------------------------------------------------------

	static long long daysFromCivil(long long y, unsigned m, unsigned d)
	{
		y -= m <= 2;
		const long long era = (y >= 0 ? y : y - 399) / 400;
		const unsigned yoe = (unsigned)(y - era * 400);
		const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
		const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
		return era * 146097 + (long long)doe - 719468;
	}

	static void civilFromDays(long long z, long long& y, unsigned& m, unsigned& d)
	{
		z += 719468;
		const long long era = (z >= 0 ? z : z - 146096) / 146097;
		const unsigned doe = (unsigned)(z - era * 146097);
		const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
		y = (long long)yoe + era * 400;
		const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
		const unsigned mp = (5 * doy + 2) / 153;
		d = doy - (153 * mp + 2) / 5 + 1;
		m = mp < 10 ? mp + 3 : mp - 9;
		y += (m <= 2);
	}

	// "2026-10-16T10:11:12[.ffffff][Z|+00:00]" -> seconds since epoch (UTC). -1 if not parsable.
	static long long parseIsoUtc(const std::string& iso)
	{
		int Y, M, D, h, mi, s;
		if (iso.size() < 19 || sscanf(iso.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &Y, &M, &D, &h, &mi, &s) != 6)
			return -1;

		long long t = daysFromCivil(Y, (unsigned)M, (unsigned)D) * 86400LL + h * 3600LL + mi * 60LL + s;

		// Apply an explicit offset if any (RomM sends UTC, this is only defensive)
		auto tz = iso.find_first_of("+-", 19);
		if (tz != std::string::npos && iso.size() >= tz + 6)
		{
			int oh = 0, om = 0;
			if (sscanf(iso.c_str() + tz + 1, "%2d:%2d", &oh, &om) == 2)
			{
				long long offset = oh * 3600LL + om * 60LL;
				t += (iso[tz] == '+') ? -offset : offset;
			}
		}

		return t;
	}

	static std::string formatIsoUtc(long long t)
	{
		long long days = t >= 0 ? t / 86400 : (t - 86399) / 86400;
		long long secs = t - days * 86400;

		long long y; unsigned m, d;
		civilFromDays(days, y, m, d);

		char buf[64];
		snprintf(buf, sizeof(buf), "%04lld-%02u-%02uT%02d:%02d:%02d+00:00", y, m, d, (int)(secs / 3600), (int)((secs % 3600) / 60), (int)(secs % 60));
		return buf;
	}

	// RomM first_release_date (milliseconds) -> ES date "YYYYMMDDTHHMMSS"
	static std::string formatEsDate(long long milliseconds)
	{
		if (milliseconds <= 0)
			return "";

		long long days = (milliseconds / 1000) / 86400;
		long long y; unsigned m, d;
		civilFromDays(days, y, m, d);

		char buf[48];
		snprintf(buf, sizeof(buf), "%04lld%02u%02uT000000", y, m, d);
		return buf;
	}

	static std::string localTimeString()
	{
		time_t now = time(nullptr);
		struct tm tmv;
#ifdef WIN32
		localtime_s(&tmv, &now);
#else
		localtime_r(&now, &tmv);
#endif
		char buf[32];
		strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tmv);
		return buf;
	}

	// ---------------------------------------------------------------------------

	// Percent-encodes what is not valid in a URL path or query (RomM sends "?ts=2026-10-14 23:49:27",
	// with a raw space, that curl rejects). Existing %XX sequences and URL delimiters are kept.
	static std::string encodeUrlChars(const std::string& url)
	{
		static const char* const hex = "0123456789ABCDEF";
		static const std::string allowed = "-._~!$&'()*+,;=:@/?%";

		std::string ret;
		ret.reserve(url.size() + 16);

		for (unsigned char c : url)
		{
			if (isalnum(c) || allowed.find((char)c) != std::string::npos)
				ret += (char)c;
			else
			{
				ret += '%';
				ret += hex[c >> 4];
				ret += hex[c & 15];
			}
		}

		return ret;
	}

	std::string coverPath(const std::string& coverUrl)
	{
		if (coverUrl.empty() || Utils::String::startsWith(coverUrl, "http://") || Utils::String::startsWith(coverUrl, "https://"))
			return ""; // external URLs are never fetched (the RomM token must stay on the RomM server)

		static const std::string prefix = "/assets/romm/resources";

		std::string path = coverUrl;
		if (path.find(prefix) == std::string::npos)
		{
			while (!path.empty() && path[0] == '/')
				path = path.substr(1);

			path = prefix + "/" + path;
		}
		else if (path[0] != '/')
			path = "/" + path;

		return encodeUrlChars(path);
	}

	// Extension of a server path (before any query string), or defaultExt when it has none that looks valid
	static std::string mediaExtension(const std::string& url, const std::string& defaultExt)
	{
		std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(url.substr(0, url.find('?'))));
		if (ext.size() < 2 || ext.size() > 5)
			return defaultExt;

		for (size_t i = 1; i < ext.size(); i++)
			if (!isalnum((unsigned char)ext[i]))
				return defaultExt;

		return ext;
	}

	static bool mediaChanged(const CatalogRecord& a, const CatalogRecord& b)
	{
		for (size_t i = 0; i < (size_t)Media::Count; i++)
			if (a.mediaUrl[i] != b.mediaUrl[i] || a.mediaFile[i] != b.mediaFile[i])
				return true;

		return false;
	}

	struct SyncState
	{
		std::string server;
		std::string mapping;
		long long watermark = -1;
		int games = 0;
		int format = 1;
		std::string lastSync;
	};

	static SyncState readState(const std::string& path)
	{
		SyncState state;
		if (!Utils::FileSystem::exists(path, false))
			return state;

		rapidjson::Document doc;
		std::string text = Utils::FileSystem::readAllText(path);
		doc.Parse(text.c_str());
		if (doc.HasParseError() || !doc.IsObject())
			return state;

		state.server = Json::str(doc, "server");
		state.mapping = Json::str(doc, "mapping");
		state.watermark = Json::integer(doc, "watermark", -1);
		state.games = (int)Json::integer(doc, "games", 0);
		state.lastSync = Json::str(doc, "last_sync");
		state.format = (int)Json::integer(doc, "format", 1);
		return state;
	}

	static bool writeState(const std::string& path, const SyncState& state)
	{
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

		writer.StartObject();
		writer.Key("version");   writer.Int(1);
		writer.Key("server");    writer.String(state.server.c_str());
		writer.Key("mapping");   writer.String(state.mapping.c_str());
		writer.Key("watermark"); writer.Int64(state.watermark);
		writer.Key("games");     writer.Int(state.games);
		writer.Key("last_sync"); writer.String(state.lastSync.c_str());
		writer.Key("format");    writer.Int(state.format);
		writer.EndObject();

		return writeFileAtomic(path, buffer.GetString());
	}

	// One RomM rom as a snapshot record. False: not shown (unmapped platform, unsupported multi-file game...).
	// ---------------------------------------------------------------------------------------------
	// Multi-file games
	// ---------------------------------------------------------------------------------------------

	// Same rules as RomM's playlist (utils/m3u.py): sheets that describe a disc by pointing at raw
	// tracks; those tracks are not discs on their own
	static const std::set<std::string> DESCRIPTOR_EXTENSIONS = { ".cue", ".gdi", ".ccd", ".mds" };
	static const std::set<std::string> COMPANION_EXTENSIONS = { ".bin", ".raw", ".img", ".sub", ".mdf", ".wav", ".ogg", ".flac", ".mp3" };

	// Systems whose games are folders named <game>.<ext> (Batocera es_systems descriptions).
	// Used only when ES accepts that extension for the system.
	static const std::map<std::string, std::vector<std::string>> FOLDER_EXTENSIONS =
	{
		{ "dos", { ".pc", ".dos" } }, { "atarist", { ".gemdos" } }, { "ps3", { ".ps3" } }, { "daphne", { ".daphne" } },
		{ "easyrpg", { ".easyrpg" } }, { "thextech", { ".smbx" } }, { "sonicretro", { ".son" } },
	};

	static std::string lowerExtension(const std::string& name)
	{
		return Utils::String::toLower(Utils::FileSystem::getExtension(name));
	}

	// "(Disc 2)", "(CD 2)", "(Disk 1 of 2)", "(Disc 2A)", "(Disc B)" -> 2, 2, 1, 2, 2; -1 when none
	static int discNumber(const std::string& fileName)
	{
		static const std::regex tag("\\((?:disc|disk|cd|disque)(?:\\s*([0-9]+)[a-z]?|\\s+([a-z]))(?:\\s+of\\s+[0-9]+)?\\)", std::regex::icase);

		std::smatch m;
		if (!std::regex_search(fileName, m, tag))
			return -1;

		if (m[1].matched)
			return atoi(m[1].str().c_str());

		return tolower(m[2].str()[0]) - 'a' + 1;
	}

	// The playlist of a multi-disc game: discs only, numbered discs first and in order, then by name
	static std::vector<std::string> playlist(const std::vector<CatalogRecord::Part>& parts)
	{
		std::vector<const CatalogRecord::Part*> discs;
		bool hasDescriptor = false;

		for (auto& p : parts)
		{
			std::string ext = lowerExtension(p.path);
			if (ext == ".m3u")
				continue;

			hasDescriptor = hasDescriptor || DESCRIPTOR_EXTENSIONS.count(ext) > 0;
			discs.push_back(&p);
		}

		if (hasDescriptor)
			discs.erase(std::remove_if(discs.begin(), discs.end(), [](const CatalogRecord::Part* p) { return COMPANION_EXTENSIONS.count(lowerExtension(p->path)) > 0; }), discs.end());

		std::stable_sort(discs.begin(), discs.end(), [](const CatalogRecord::Part* a, const CatalogRecord::Part* b)
		{
			std::string an = Utils::FileSystem::getFileName(a->path), bn = Utils::FileSystem::getFileName(b->path);
			int da = discNumber(an), db = discNumber(bn);

			if ((da < 0) != (db < 0))
				return da >= 0; // numbered discs first

			if (da != db)
				return da < db;

			return an < bn;
		});

		std::vector<std::string> ret;
		for (auto p : discs)
			ret.push_back(p->path);

		return ret;
	}

	// A path from the server, relative to the game folder: never outside it
	static bool safeRelativePath(const std::string& path)
	{
		if (path.empty() || path[0] == '/' || path.find('\\') != std::string::npos || path.find(':') != std::string::npos)
			return false;

		for (auto& segment : Utils::String::split(path, '/', true))
			if (segment == ".." || segment == ".")
				return false;

		return true;
	}

	// A multi-file game as one ES entry: a playlist (A) or a folder game (B). False: not supported (skipped).
#ifdef BATOCERA
	// A package that is not a base game, by its name
	static bool isAddonPackage(const std::string& name)
	{
		std::string n = Utils::String::toLower(name);
		for (auto tag : { "(dlc)", "(update", "(patch", "(unlock key)", "add-on", "addon" })
			if (n.find(tag) != std::string::npos)
				return true;

		return false;
	}

	// PS3 games with .pkg installers and licenses (.rap, .edat), whatever their RomM category:
	//  - a disc image the system launches (.iso) at the top: the game (kind "ps3addons"); the packages
	//    (updates, DLC) and licenses are installed into RPCS3 after its download;
	//  - else a PSN game (kind "psn"): base game, a .pkg at the top. ES shows <name>.psn, which
	//    batocera-romm-ps3-install writes once it has installed the packages into RPCS3.
	static bool buildPsn(const rapidjson::Value& rom, const rapidjson::Value& files, CatalogRecord& r, const rapidjson::Value*& baseFile)
	{
		std::string romFolder = Json::str(rom, "full_path");
		std::set<std::string> extensions = RommCatalog::getSystemExtensions(r.system);
		const rapidjson::Value* base = nullptr;
		const rapidjson::Value* firstTopPkg = nullptr;
		const rapidjson::Value* disc = nullptr;
		std::vector<CatalogRecord::Part> parts;
		long long size = 0;

		for (auto& file : files.GetArray())
		{
			std::string name = Json::str(file, "file_name");
			std::string ext = lowerExtension(name);
			std::string full = Json::str(file, "full_path");
			bool top = romFolder.empty() || !Utils::String::startsWith(full, romFolder + "/") || full.find('/', romFolder.size() + 1) == std::string::npos;

			if (ext != ".pkg" && ext != ".rap" && ext != ".edat")
			{
				if (top && disc == nullptr && ext != ".psn" && extensions.count(ext) > 0)
					disc = &file;

				continue;
			}

			CatalogRecord::Part p;
			p.id = (int)Json::integer(file, "id", 0);
			p.path = !romFolder.empty() && Utils::String::startsWith(full, romFolder + "/") ? full.substr(romFolder.size() + 1) : name;
			p.size = Json::integer(file, "file_size_bytes", 0);

			if (p.id <= 0 || !safeRelativePath(p.path))
				return false;

			if (ext == ".pkg" && p.path.find('/') == std::string::npos)
			{
				if (firstTopPkg == nullptr)
					firstTopPkg = &file;

				if (base == nullptr && !isAddonPackage(name))
					base = &file;
			}

			parts.push_back(p);
			size += p.size;
		}

		if (parts.empty())
			return false;

		if (disc != nullptr)
		{
			r.kind = "ps3addons";
			r.parts = parts;
			r.fileName = Utils::FileSystem::getFileName(Json::str(*disc, "file_name"));
			r.size = Json::integer(*disc, "file_size_bytes", 0) + size;
			baseFile = disc;
			return true;
		}

		if (base == nullptr)
			base = firstTopPkg; // only packages named as add-ons at the top: the first one

		if (base == nullptr)
			return false;

		std::string name = Utils::FileSystem::getFileName(Json::str(rom, "fs_name"));
		if (lowerExtension(name) == ".pkg")
			name = name.substr(0, name.size() - 4);

		if (name.empty())
			return false;

		r.kind = "psn";
		r.parts = parts;
		r.fileName = name + ".psn";
		r.size = size;
		baseFile = base;
		return true;
	}
#endif

#ifdef BATOCERA
	// Switch: the game's updates and DLC (.nsp, .nsz), downloaded with it into switch_update/update/
	// and switch_update/dlc/. By RomM category, else by title id (...800: update, ...000: a base game).
	static void addSwitchAddons(const rapidjson::Value& files, CatalogRecord& r)
	{
		static const std::regex titleId("\\[(0100[0-9A-Fa-f]{12})\\]");

		for (auto& file : files.GetArray())
		{
			int id = (int)Json::integer(file, "id", 0);
			std::string name = Utils::FileSystem::getFileName(Json::str(file, "file_name"));
			std::string ext = lowerExtension(name);
			if (id <= 0 || id == r.fileId || (ext != ".nsp" && ext != ".nsz") || !safeRelativePath(name))
				continue;

			std::string folder = Utils::String::toLower(Json::str(file, "category"));
			if (folder != "update" && folder != "dlc")
			{
				std::smatch m;
				if (!std::regex_search(name, m, titleId))
					continue;

				std::string tid = Utils::String::toLower(m[1].str());
				if (Utils::String::endsWith(tid, "000"))
					continue;

				folder = Utils::String::endsWith(tid, "800") ? "update" : "dlc";
			}

			CatalogRecord::Part p;
			p.id = id;
			p.path = folder + "/" + name;
			p.size = Json::integer(file, "file_size_bytes", 0);
			r.parts.push_back(p);
			r.size += p.size;
		}

		if (!r.parts.empty())
			r.kind = "switchaddons";
	}
#endif

	static bool buildMultiFile(const rapidjson::Value& rom, const rapidjson::Value& files, CatalogRecord& r, const rapidjson::Value*& firstFile)
	{
		std::set<std::string> extensions = RommCatalog::getSystemExtensions(r.system);
		if (extensions.empty())
			return false;

		std::string name = Utils::FileSystem::getFileName(Json::str(rom, "fs_name"));
		std::string romFolder = Json::str(rom, "full_path");
		if (name.empty() || romFolder.empty())
			return false;

		bool hasDescriptor = false;
		for (auto& file : files.GetArray())
		{
			std::string category = Utils::String::toLower(Json::str(file, "category"));
			if (NOT_PLAYABLE.find(category) != NOT_PLAYABLE.cend())
				continue; // manuals, walkthroughs, patches...

			// Path inside the game folder: <rom folder>/<sub folders>/<file>
			std::string full = Json::str(file, "full_path");
			std::string path = Utils::String::startsWith(full, romFolder + "/") ? full.substr(romFolder.size() + 1) : Json::str(file, "file_name");

			CatalogRecord::Part p;
			p.id = (int)Json::integer(file, "id", 0);
			p.path = path;
			p.size = Json::integer(file, "file_size_bytes", 0);

			if (p.id <= 0 || !safeRelativePath(p.path))
				return false;

			if (firstFile == nullptr)
				firstFile = &file;

			hasDescriptor = hasDescriptor || DESCRIPTOR_EXTENSIONS.count(lowerExtension(p.path)) > 0;
			r.parts.push_back(p);
		}

		if (r.parts.empty())
			return false;

		// A : every file is something the system loads (raw tracks count when a sheet describes them)
		bool discsOnly = true;
		for (auto& p : r.parts)
		{
			std::string ext = lowerExtension(p.path);
			if (ext != ".m3u" && extensions.count(ext) == 0 && !(hasDescriptor && COMPANION_EXTENSIONS.count(ext) > 0))
				discsOnly = false;
		}

		if (discsOnly && extensions.count(".m3u") > 0)
		{
			r.kind = "m3u";
			r.folderName = name;
			r.fileName = name + ".m3u";
			r.discs = playlist(r.parts);
			return !r.discs.empty();
		}

		// B : the game is a folder <name>.<ext>, for systems that use that
		auto folder = FOLDER_EXTENSIONS.find(r.system);
		if (folder != FOLDER_EXTENSIONS.cend())
		{
			std::string ext;
			for (auto& candidate : folder->second)
			{
				if (extensions.count(candidate) == 0)
					continue; // not accepted by this ES

				if (lowerExtension(name) == candidate)
				{
					ext = candidate; // already named so on the server
					break;
				}

				if (ext.empty())
					ext = candidate;
			}

			if (!ext.empty())
			{
				r.kind = "folder";
				r.fileName = lowerExtension(name) == ext ? name : name + ext;
				r.folderName = r.fileName;
				return true;
			}
		}

		// Switch: a game with its update / DLC beside it at the top of its folder. The files carry their
		// title id ("[0100FF500E34A000]"): the base game's ends in 000 (updates 800, DLC others). Exactly
		// one base game: it is the game (its updates and DLC stay in RomM).
		static const std::regex titleId("\\[(0100[0-9A-Fa-f]{12})\\]");
		const CatalogRecord::Part* base = nullptr;
		int bases = 0;

		for (auto& p : r.parts)
		{
			std::smatch m;
			std::string fileName = Utils::FileSystem::getFileName(p.path);
			if (extensions.count(lowerExtension(p.path)) == 0 || !std::regex_search(fileName, m, titleId))
				continue;

			std::string id = Utils::String::toLower(m[1].str());
			if (Utils::String::endsWith(id, "000"))
			{
				base = &p;
				bases++;
			}
		}

		if (bases == 1)
		{
			const CatalogRecord::Part single = *base;
			r.parts.clear();
			r.kind.clear();
			r.fileId = single.id;
			r.fileName = Utils::FileSystem::getFileName(single.path);
			r.size = single.size;

			for (auto& file : files.GetArray())
				if ((int)Json::integer(file, "id", 0) == single.id)
					firstFile = &file;

			return true;
		}

		// C : one sheet the system loads at the top, every other file one of its tracks: the sheet is the
		// game, its tracks beside it in the system folder (as cue + bin games are usually kept)
		const CatalogRecord::Part* sheet = nullptr;
		int sheets = 0;
		bool tracksOnly = true;

		for (auto& p : r.parts)
		{
			std::string ext = lowerExtension(p.path);
			if (DESCRIPTOR_EXTENSIONS.count(ext) > 0 && extensions.count(ext) > 0 && p.path.find('/') == std::string::npos)
			{
				sheet = &p;
				sheets++;
			}
			else if (COMPANION_EXTENSIONS.count(ext) == 0)
				tracksOnly = false;
		}

		if (sheets == 1 && tracksOnly)
		{
			r.kind = "sheet";
			r.fileName = Utils::FileSystem::getFileName(sheet->path);
			r.folderName = Utils::FileSystem::getStem(r.fileName); // where it is downloaded first

			for (auto& file : files.GetArray())
				if ((int)Json::integer(file, "id", 0) == sheet->id)
					firstFile = &file;

			return true;
		}

		return false;
	}

	static bool toRecord(const rapidjson::Value& rom, const std::map<int, std::string>& mapping, CatalogRecord& r, long long& updatedEpoch)
	{
		r.romId = (int)Json::integer(rom, "id", 0);
		r.platformId = (int)Json::integer(rom, "platform_id", 0);
		r.updatedAt = Json::str(rom, "updated_at");
		updatedEpoch = parseIsoUtc(r.updatedAt);

		if (r.romId <= 0)
			return false;

		auto system = mapping.find(r.platformId);
		if (system == mapping.cend())
			return false;

		r.system = system->second;

		if (Json::boolean(rom, "missing_from_fs", false) || !Json::boolean(rom, "has_file_on_disk", true))
			return false;

		auto files = rom.FindMember("files");
		if (files == rom.MemberEnd() || !files->value.IsArray() || files->value.Size() == 0)
			return false;

		const rapidjson::Value* chosen = nullptr;
		bool psn = false;

#ifdef BATOCERA
		psn = r.system == "ps3" && buildPsn(rom, files->value, r, chosen);
#endif

		// Multi-file games: a playlist, a folder game or a sheet with its tracks (A / B / C), else not shown
		if (!psn && Json::boolean(rom, "has_multiple_files", false))
		{
			if (!buildMultiFile(rom, files->value, r, chosen) || chosen == nullptr)
			{
				LOG(LogInfo) << "RomM: " << Json::str(rom, "fs_name") << " (" << r.system << "): multi-file game this system cannot show, skipped";
				return false;
			}
		}

		// The file a player would launch: the first one not categorized as an extra
		for (auto& file : files->value.GetArray())
		{
			if (chosen != nullptr)
				break;

			std::string category = Utils::String::toLower(Json::str(file, "category"));
			if (NOT_PLAYABLE.find(category) == NOT_PLAYABLE.cend())
			{
				chosen = &file;
				break;
			}
		}

		if (chosen == nullptr)
			chosen = &files->value[0];

		r.fileId = (int)Json::integer(*chosen, "id", 0);

		// RetroAchievements (RomM computes the hash with RAHasher, as rcheevos does in ES).
		// ES stores the hash uppercase (FileData::checkCheevosHash).
		long long raId = Json::integer(rom, "ra_id", 0);
		r.cheevosId = raId > 0 ? (int)raId : 0;

		std::string raHash = Json::str(*chosen, "ra_hash");
		if (raHash.empty())
			raHash = Json::str(rom, "ra_hash");

		r.cheevosHash = Utils::String::toUpper(raHash);

		if (r.kind.empty())
		{
			r.fileName = Utils::FileSystem::getFileName(Json::str(*chosen, "file_name"));
			r.size = Json::integer(*chosen, "file_size_bytes", Json::integer(rom, "fs_size_bytes", 0));
		}
		else
		{
			// Multi-file games: their files. A disc game with add-ons: its image, then the add-ons.
			r.size = r.kind == "ps3addons" ? Json::integer(*chosen, "file_size_bytes", 0) : 0;
			for (auto& p : r.parts)
				r.size += p.size;
		}

		if (r.fileId <= 0 || r.fileName.empty())
			return false;

#ifdef BATOCERA
		if (r.system == "switch" && r.kind.empty())
			addSwitchAddons(files->value, r);
#endif

		r.name = Json::str(rom, "name");
		if (r.name.empty())
			r.name = Json::str(rom, "fs_name_no_ext");

		r.desc = Json::str(rom, "summary");

		auto meta = rom.FindMember("metadatum");
		if (meta != rom.MemberEnd() && meta->value.IsObject())
		{
			const rapidjson::Value& md = meta->value;

			r.genre = Json::join(md, "genres");
			r.developer = Json::join(md, "developers");
			r.publisher = Json::join(md, "publishers");

			if (r.developer.empty())
			{
				auto companies = md.FindMember("companies");
				if (companies != md.MemberEnd() && companies->value.IsArray() && companies->value.Size() > 0 && companies->value[0].IsString())
					r.developer = companies->value[0].GetString();
			}

			r.players = Json::str(md, "player_count");
			r.releaseDate = formatEsDate(Json::integer(md, "first_release_date", 0));

			double rating = Json::number(md, "average_rating", -1);
			if (rating > 0)
			{
				// RomM ratings are 0-100 (IGDB scale), ES uses 0-1
				rating = std::min(1.0, std::max(0.0, rating / 100.0));
				char buf[16];
				snprintf(buf, sizeof(buf), "%.2f", rating);
				r.rating = buf;
			}
		}

		// Documents (ES shows PDF manuals, PDF/image maps). Uploaded ones are files of the rom (categories
		// manual / walkthrough), downloaded by id through the content endpoint.
		auto isPdf = [](const std::string& name) { return Utils::String::toLower(Utils::FileSystem::getExtension(name.substr(0, name.find('?')))) == ".pdf"; };
		auto documentUrl = [&r](const rapidjson::Value& file)
		{
			return "/api/roms/" + std::to_string(r.romId) + "/content/" + HttpReq::urlEncode(Json::str(file, "file_name")) +
				"?file_ids=" + std::to_string(Json::integer(file, "id", 0));
		};

		for (auto& file : files->value.GetArray())
		{
			std::string category = Utils::String::toLower(Json::str(file, "category"));
			std::string name = Json::str(file, "file_name");
			if (!isPdf(name) || Json::integer(file, "id", 0) <= 0)
				continue;

			const size_t iManual = mediaIndex(Media::Manual), iWalk = mediaIndex(Media::Walkthrough);

			if (category == "manual" && r.mediaUrl[iManual].empty()) // the manual chosen by the user wins over the scraped one
			{
				r.mediaUrl[iManual] = documentUrl(file);
				r.mediaFile[iManual] = std::to_string(r.romId) + ".pdf";
			}
			else if (category == "walkthrough" && r.mediaUrl[iWalk].empty())
			{
				r.mediaUrl[iWalk] = documentUrl(file);
				r.mediaFile[iWalk] = std::to_string(r.romId) + ".pdf";
			}
		}

		// Static resources of RomM (/assets/romm/resources/...): file named <rom id><extension> in the cache
		auto setResource = [&r](Media type, const std::string& url, const std::string& defaultExt)
		{
			size_t i = mediaIndex(type);
			if (!r.mediaUrl[i].empty() || coverPath(url).empty())
				return;

			r.mediaUrl[i] = url;
			r.mediaFile[i] = std::to_string(r.romId) + mediaExtension(url, defaultExt);
		};

		std::string manual = Json::str(rom, "path_manual");
		if (isPdf(manual))
			setResource(Media::Manual, manual, ".pdf");

		setResource(Media::Cover, Json::str(rom, "path_cover_small"), ".png");

		// ScreenScraper media stored by RomM (when it scraped them), marquee also from an imported gamelist
		auto ss = rom.FindMember("ss_metadata");
		auto gamelist = rom.FindMember("gamelist_metadata");
		const bool hasSs = ss != rom.MemberEnd() && ss->value.IsObject();
		const bool hasGamelist = gamelist != rom.MemberEnd() && gamelist->value.IsObject();

		if (hasSs)
		{
			setResource(Media::Marquee, Json::str(ss->value, "marquee_path"), ".png");
			setResource(Media::Bezel, Json::str(ss->value, "bezel_path"), ".png");
		}

		if (hasGamelist)
			setResource(Media::Marquee, Json::str(gamelist->value, "marquee_path"), ".png");

		// More ScreenScraper media, else those of an imported gamelist (setResource keeps the first found)
		const struct { Media type; const char* field; } more[] =
		{
			{ Media::Box, "box2d_path" }, { Media::BoxBack, "box2d_back_path" }, { Media::Box3d, "box3d_path" },
			{ Media::FanArt, "fanart_path" }, { Media::TitleShot, "title_screen_path" }, { Media::Wheel, "logo_path" },
			{ Media::Mix, "miximage_path" }, { Media::MixV2, "miximage_v2_path" }, { Media::Cartridge, "physical_path" },
			{ Media::BoxSide, "box2d_side_path" },
		};

		for (auto& item : more)
		{
			if (hasSs)
				setResource(item.type, Json::str(ss->value, item.field), ".png");

			if (hasGamelist)
				setResource(item.type, Json::str(gamelist->value, item.field), ".png");
		}

		// RomM already picks the video among its sources (ScreenScraper, gamelist, LaunchBox); else the
		// ScreenScraper normalized one
		setResource(Media::Video, Json::str(rom, "path_video"), ".mp4");
		if (hasSs)
			setResource(Media::Video, Json::str(ss->value, "video_normalized_path"), ".mp4");

		// Only as the image shown: the large cover, the first screenshot
		setResource(Media::CoverLarge, Json::str(rom, "path_cover_large"), ".png");
		{
			auto shots = rom.FindMember("merged_screenshots");
			if (shots != rom.MemberEnd() && shots->value.IsArray() && shots->value.Size() > 0 && shots->value[0].IsString())
				setResource(Media::Screenshot, shots->value[0].GetString(), ".png");
		}

		// No cover: the image ES presents comes from the large cover, a screenshot, the title screen,
		// the mix image or the box front, whichever exists first (setResource keeps the first found)
		setResource(Media::Cover, Json::str(rom, "path_cover_large"), ".png");

		auto screenshots = rom.FindMember("merged_screenshots");
		if (screenshots != rom.MemberEnd() && screenshots->value.IsArray() && screenshots->value.Size() > 0 && screenshots->value[0].IsString())
			setResource(Media::Cover, screenshots->value[0].GetString(), ".png");

		for (auto field : { "title_screen_path", "miximage_path", "miximage_v2_path", "box2d_path" })
		{
			if (hasSs)
				setResource(Media::Cover, Json::str(ss->value, field), ".png");

			if (hasGamelist)
				setResource(Media::Cover, Json::str(gamelist->value, field), ".png");
		}

		return true;
	}

	// ---------------------------------------------------------------------------

	std::atomic<bool> RommSync::sStatusIsProgress(false);
	std::atomic<bool> RommSync::sRunFailed(false);

	void RommSync::setStatus(const std::string& status)
	{
		std::unique_lock<std::mutex> lock(sMutex);
		sStatus = status;
		sStatusIsProgress = false;
	}

	void RommSync::setProgressStatus(const std::string& status)
	{
		if (sRunFailed)
			return;

		std::unique_lock<std::mutex> lock(sMutex);
		sStatus = status;
		sStatusIsProgress = true;
	}

	std::string RommSync::getStatus()
	{
		{
			std::unique_lock<std::mutex> lock(sMutex);
			if (!sStatus.empty())
				return sStatus;
		}

		RommConfig config = RommConfig::load();
		if (!config.isPaired())
			return "";

		SyncState state = readState(getServerCacheDir(config.baseUrl()) + "/state.json");
		if (state.lastSync.empty())
			return "";

		return _("LAST SYNC") + ": " + state.lastSync + " - " + std::to_string(state.games) + " " + _("GAMES");
	}

	// Only touched on the UI thread (all accesses go through postToUiThread)
	static AsyncNotificationComponent* sProgressBox = nullptr;

	void RommSync::showProgress(const std::string& text, const std::string& counts, int percent)
	{
		Window* window = sWindow;
		if (window == nullptr)
			return;

		// The box fits about 30 characters at any resolution: the counts get a line of their own
		window->postToUiThread([window, text, counts, percent]()
		{
			if (sProgressBox == nullptr)
			{
				sProgressBox = window->createAsyncNotificationComponent(true); // with the line for the counts
				sProgressBox->updateTitle(_("ROMM LIBRARY"));
			}

			sProgressBox->updateText(text, counts);
			sProgressBox->updatePercent(percent);
		});
	}

	void RommSync::closeProgress()
	{
		Window* window = sWindow;
		if (window == nullptr)
			return;

		window->postToUiThread([]()
		{
			if (sProgressBox != nullptr)
			{
				sProgressBox->close(); // the window deletes it once faded out
				sProgressBox = nullptr;
			}
		});
	}

	void RommSync::notify(const std::string& message)
	{
		Window* window = sWindow;
		if (window == nullptr)
			return;

		window->postToUiThread([window, message]() { window->displayNotificationMessage(message); });
	}

	void RommSync::start(Window* window)
	{
		sWindow = window;

		if (RommConfig::load().isUsable())
			launch(false, true);
	}

	void RommSync::applyMediaSettings()
	{
		RommConfig config = RommConfig::load();
		for (auto& m : allMedia())
			sMediaEnabled[mediaIndex(m.type)] = config.fetchesFromLibrary(m.type);
	}

	void RommSync::pause()
	{
		if (!sRunning)
			return;

		setBackgroundPaused(true);
		setProgressStatus(_("SYNCHRONIZATION PAUSED"));
		LOG(LogInfo) << "RomM: synchronization paused";
	}

	void RommSync::resume()
	{
		setBackgroundPaused(false);
		LOG(LogInfo) << "RomM: synchronization resumed";
	}

	bool RommSync::isPaused()
	{
		return sRunning && isBackgroundPaused();
	}

	bool RommSync::cancel()
	{
		if (!sRunning)
			return false;

		sCancel = true;
		setBackgroundAborted(true);   // wakes paused workers so they stop
		setBackgroundPaused(false);
		LOG(LogInfo) << "RomM: synchronization cancelled";
		return true;
	}

	void RommSync::stop()
	{
		sCancel = true;

		// The worker takes sMutex (setStatus) : never join while holding it
		std::thread worker;
		{
			std::unique_lock<std::mutex> lock(sMutex);
			worker = std::move(sThread);
		}

		if (worker.joinable())
			worker.join();

		sWindow = nullptr;
	}

	bool RommSync::requestSync(bool full)
	{
		return launch(full, false);
	}

	bool RommSync::launch(bool full, bool retryNetwork)
	{
		std::unique_lock<std::mutex> lock(sMutex);

		if (sRunning)
			return false;

		if (sThread.joinable())
			sThread.join(); // previous run is over (sRunning is false) and no longer takes sMutex

		sCancel = false;
		sRunning = true;
		sThread = std::thread(&RommSync::run, full, retryNetwork);
		return true;
	}

	bool RommSync::requestMediaRefresh(const std::vector<std::string>& gamePaths)
	{
		std::unique_lock<std::mutex> lock(sMutex);

		if (sRunning || gamePaths.empty())
			return false;

		if (sThread.joinable())
			sThread.join();

		sCancel = false;
		sRunning = true;
		sThread = std::thread(&RommSync::runMediaRefresh, gamePaths);
		return true;
	}

	void RommSync::runMediaRefresh(std::vector<std::string> gamePaths)
	{
		struct RunningGuard
		{
			~RunningGuard()
			{
				RommSync::closeProgress();

				if (isBackgroundAborted())
					RommSync::setStatus(_("CANCELLED"));
				else if (RommSync::sStatusIsProgress)
					RommSync::setStatus(_("SYNC INTERRUPTED"));

				setBackgroundPaused(false);
				setBackgroundAborted(false);
				RommSync::sRunning = false;
			}
		} guard;

		lowerThreadPriority(); // background work: menus and games come first
		sRunFailed = false;

		const int total = (int)gamePaths.size();
		int workers = backgroundWorkers();
		LOG(LogInfo) << "RomM: downloading media again for " << total << " games, " << workers << " in parallel";

		std::atomic<int> done(0), updated(0), failed(0);
		std::atomic<bool> stop(false);

		runInBackground(gamePaths.size(), workers, [&](size_t i)
		{
			if (sCancel)
			{
				stop = true;
				return;
			}

			int n = ++done;
			if (n == 1 || n % 10 == 0 || n == total)
			{
				std::string text = _("DOWNLOADING MEDIA AGAIN") + " " + std::to_string(n) + "/" + std::to_string(total);
				setProgressStatus(text);
				showProgress(_("DOWNLOADING MEDIA AGAIN"), std::to_string(n) + " / " + std::to_string(total), (int)(n * 100LL / total));
			}

			std::string error;
			if (downloadGameMedia(gamePaths[i], &sCancel, error))
			{
				updated++;

				// Next to the rom, on the UI thread; the game is looked up then (the lists may have been reloaded)
				std::string path = gamePaths[i];
				Window* window = sWindow;
				if (window != nullptr)
					window->postToUiThread([path]() { onGameMediaDownloadedAt(path); });
			}
			else if (!sCancel)
			{
				failed++;
				LOG(LogWarning) << "RomM: media not downloaded again for " << gamePaths[i] << " : " << error;
			}
		}, stop);

		if (sCancel)
			return;

		std::string summary = _("MEDIA DOWNLOADED AGAIN") + ": " + std::to_string(updated.load()) + "/" + std::to_string(total);
		if (failed > 0)
			summary += " (" + std::to_string(failed.load()) + " " + _("FAILED") + ")";

		setStatus(summary);
		notify(summary);
	}

	void RommSync::run(bool full, bool retryNetwork)
	{
		struct RunningGuard
		{
			~RunningGuard()
			{
				RommSync::closeProgress();

				if (isBackgroundAborted())
					RommSync::setStatus(_("SYNCHRONIZATION CANCELLED"));
				else if (RommSync::sStatusIsProgress)
					RommSync::setStatus(_("SYNC INTERRUPTED")); // ended without a final status (see es_log.txt)

				setBackgroundPaused(false);
				setBackgroundAborted(false);
				RommSync::sRunning = false;
			}
		} guard;

		lowerThreadPriority(); // background work: menus and games come first

		RommConfig config = RommConfig::load();
		if (!config.isUsable())
		{
			setStatus(_("NOT CONFIGURED"));
			return;
		}

		const std::string baseUrl = config.baseUrl();
		RommClient client(config);
		for (auto& m : allMedia())
			sMediaEnabled[mediaIndex(m.type)] = config.fetchesFromLibrary(m.type);

		sRunFailed = false;
		setProgressStatus(_("SYNCHRONIZING..."));

		// 1. Platforms (retry at ES startup : the network may not be up yet)
		HttpResult res;
		int attempts = retryNetwork ? 6 : 1;
		for (int i = 0; i < attempts; i++)
		{
			res = client.get("/api/platforms", &sCancel);
			if (res.ok() || res.unauthorized() || res.cancelled)
				break;

			for (int w = 0; w < 100 && !sCancel; w++) // 10 seconds, cancellable
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		if (sCancel)
			return;

		if (!res.ok())
		{
			setStatus(res.unauthorized() ? _("AUTHORIZATION REJECTED, PAIR THIS DEVICE AGAIN") : _("SERVER UNREACHABLE") + " (" + errorDetail(res) + ")");
			return;
		}

		rapidjson::Document platforms;
		platforms.Parse(res.body.c_str());
		if (platforms.HasParseError() || !platforms.IsArray())
		{
			setStatus(_("INVALID SERVER RESPONSE"));
			return;
		}

		// 2. Server-side platform bindings (optional)
		std::map<std::string, std::string> bindings;
		HttpResult cfg = client.get("/api/config", &sCancel);
		if (cfg.ok())
		{
			rapidjson::Document doc;
			doc.Parse(cfg.body.c_str());
			if (!doc.HasParseError() && doc.IsObject())
			{
				auto it = doc.FindMember("PLATFORMS_BINDING");
				if (it != doc.MemberEnd() && it->value.IsObject())
				{
					for (auto& m : it->value.GetObject())
					{
						if (m.value.IsString())
							bindings[m.name.GetString()] = m.value.GetString();
					}
				}
			}
		}

		// 3. Map RomM platforms to ES systems
		std::set<std::string> esSystems = RommCatalog::getDefinedSystems();
		std::map<int, std::string> mapping;
		std::vector<int> platformsToFetch;
		long long expectedGames = 0;

		for (auto& p : platforms.GetArray())
		{
			int id = (int)Json::integer(p, "id", 0);
			std::string system = mapPlatformToSystem(Json::str(p, "slug"), Json::str(p, "fs_slug"), bindings, esSystems);

			if (id <= 0 || system.empty())
			{
				if (id > 0 && Json::integer(p, "rom_count", 0) > 0)
				{
					LOG(LogInfo) << "RomM: platform " << Json::str(p, "slug") << " has no matching system, skipped";
				}

				continue;
			}

			mapping[id] = system;

			long long romCount = Json::integer(p, "rom_count", 0);
			if (romCount > 0)
			{
				platformsToFetch.push_back(id);
				expectedGames += romCount;
			}
		}

		std::string mappingSignature;
		for (auto& m : mapping)
			mappingSignature += std::to_string(m.first) + "=" + m.second + ";";

		// 4. Previous snapshot
		const std::string cacheDir = getServerCacheDir(baseUrl);
		const std::string libraryDir = getLibraryDir(baseUrl);
		const std::string statePath = cacheDir + "/state.json";

		SyncState state = readState(statePath);

		std::unordered_map<int, CatalogRecord> previous;
		if (Utils::FileSystem::isDirectory(libraryDir))
		{
			for (auto file : Utils::FileSystem::getDirContent(libraryDir, false, false))
			{
				if (Utils::String::toLower(Utils::FileSystem::getExtension(file)) != ".xml")
					continue;

				std::vector<CatalogRecord> records;
				readSnapshotFile(file, records);
				for (auto& r : records)
					previous[r.romId] = r;
			}
		}

		bool cold = full || state.server != baseUrl || state.mapping != mappingSignature || state.watermark < 0 || previous.empty() ||
			state.format != SNAPSHOT_FORMAT;

		std::unordered_map<int, CatalogRecord> records;
		if (!cold)
			records = previous;

		long long maxUpdated = cold ? -1 : state.watermark;
		int received = 0;
		std::mutex recordsMutex; // platforms are fetched in parallel

		// Returns the number of games received so far
		auto processPage = [&](const rapidjson::Value& items) -> int
		{
			std::vector<std::pair<bool, CatalogRecord>> page; // keep?, record
			long long pageMaxUpdated = -1;

			for (auto& rom : items.GetArray())
			{
				CatalogRecord r;
				long long updated = -1;
				bool keep = toRecord(rom, mapping, r, updated);

				if (updated > pageMaxUpdated)
					pageMaxUpdated = updated;

				page.push_back(std::make_pair(keep, r));
			}

			std::unique_lock<std::mutex> lock(recordsMutex);

			if (pageMaxUpdated > maxUpdated)
				maxUpdated = pageMaxUpdated;

			for (auto& item : page)
			{
				if (item.first)
					records[item.second.romId] = item.second;
				else if (item.second.romId > 0)
					records.erase(item.second.romId);
			}

			received += (int)page.size();
			return received;
		};

		std::atomic<bool> authRejected(false);
		std::vector<int> failedPlatforms; // full synchronization: platforms that failed after retries
		std::mutex errorMutex;
		std::string lastError; // of a page that failed after its retries

		auto fetchPages = [&](const std::string& filter) -> bool
		{
			int offset = 0;
			int pageSize = PAGE_SIZE;

			while (!sCancel)
			{
				waitWhileGameRunning(sCancel); // never compete with a running game

				// Server errors, timeouts, network errors: retried twice, with smaller pages
				std::string query;
				HttpResult page;
				for (int attempt = 1; ; attempt++)
				{
					query = "/api/roms?" + filter +
						"&order_by=id&order_dir=asc&limit=" + std::to_string(pageSize) + "&offset=" + std::to_string(offset) +
						"&with_files=true&with_char_index=false&with_filter_values=false&with_rom_id_index=false";

					page = client.get(query, &sCancel);

					bool retryable = !page.ok() && !page.cancelled &&
						(page.status >= 500 || page.status < 100 || page.status == 408 || page.status == 429);

					if (!retryable || attempt >= 3 || sCancel)
						break;

					LOG(LogWarning) << "RomM: " << query << " : " << errorDetail(page) << ", retrying";
					pageSize = std::max(50, pageSize / 2);

					for (int wait = 0; wait < attempt * 20 && !sCancel; wait++) // 2 s, then 4 s
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}

				if (!page.ok())
				{
					if (page.cancelled || sCancel)
						return false;

					{
						std::unique_lock<std::mutex> lock(errorMutex);
						lastError = errorDetail(page);
					}

					if (page.unauthorized())
						authRejected = true;

					LOG(LogWarning) << "RomM: synchronization failed on " << query << " : " << errorDetail(page);
					sRunFailed = true; // before the status: the other workers stop reporting progress
					setStatus(page.unauthorized() ? _("AUTHORIZATION REJECTED, PAIR THIS DEVICE AGAIN") : _("SYNC FAILED") + " (" + errorDetail(page) + ")");
					return false;
				}

				rapidjson::Document doc;
				doc.Parse(page.body.c_str());
				if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("items") || !doc["items"].IsArray())
				{
					LOG(LogWarning) << "RomM: invalid server response on " << query;
					sRunFailed = true;
					setStatus(_("INVALID SERVER RESPONSE"));
					return false;
				}

				const rapidjson::Value& items = doc["items"];
				int count = (int)items.Size();
				int receivedNow = processPage(items);

				long long total = Json::integer(doc, "total", -1);
				offset += count;

				setProgressStatus(_("SYNCHRONIZING...") + " " + std::to_string(receivedNow));

				if (cold && expectedGames > 0)
				{
					int percent = (int)std::min(99LL, (long long)receivedNow * 100 / expectedGames);
					showProgress(_("SYNCHRONIZING GAMES"), std::to_string(receivedNow) + " / " + std::to_string(expectedGames), percent);
				}
				else if (receivedNow > 0)
					showProgress(_("UPDATING GAMES"), std::to_string(receivedNow), -1);

				// Follow "total" when the server sends it (it may cap the page size below PAGE_SIZE)
				if (count == 0 || (total >= 0 ? offset >= total : count < pageSize))
					return true;
			}

			return false;
		};

		// 5. Fetch
		if (cold)
		{
			// Platforms in parallel (a few connections, fewer when the device is busy)
			int workers = backgroundWorkers();
			LOG(LogInfo) << "RomM: full synchronization of " << platformsToFetch.size() << " platforms, " << workers << " in parallel";

			std::atomic<bool> stopFetch(false);
			std::atomic<int> succeeded(0), failed(0);
			std::mutex failedMutex;

			runInBackground(platformsToFetch.size(), workers, [&](size_t i)
			{
				// The server answers nothing (down, overloaded): no point retrying every platform
				if (sCancel || authRejected || (failed >= 3 && succeeded == 0))
				{
					stopFetch = true;
					return;
				}

				// A platform that fails (after retries) does not stop the others
				if (fetchPages("platform_ids=" + std::to_string(platformsToFetch[i])))
					succeeded++;
				else if (!sCancel)
				{
					failed++;
					std::unique_lock<std::mutex> lock(failedMutex);
					failedPlatforms.push_back(platformsToFetch[i]);
				}
			}, stopFetch);

			if (sCancel || authRejected || succeeded == 0)
				return; // nothing usable (the status says why)

			if (stopFetch)
				return; // stopped early: the remaining platforms were not fetched

			// The platforms that failed keep the games of the previous synchronization
			for (int platformId : failedPlatforms)
			{
				for (auto& old : previous)
					if (old.second.platformId == platformId && records.find(old.first) == records.cend())
						records[old.first] = old.second;

				auto sys = mapping.find(platformId);
				LOG(LogWarning) << "RomM: platform " << platformId << " (" << (sys != mapping.cend() ? sys->second : std::string("?")) << ") failed, previous games kept";
			}

			// The status of an incomplete synchronization is set at the end
			if (!failedPlatforms.empty())
				sRunFailed = false;
		}
		else
		{
			std::string since = formatIsoUtc(state.watermark - WATERMARK_OVERLAP_SECONDS);
			LOG(LogInfo) << "RomM: incremental synchronization since " << since;

			if (!fetchPages("updated_after=" + HttpReq::urlEncode(since)))
				return;

			// Deleted games
			HttpResult ids = client.get("/api/roms/identifiers", &sCancel);
			if (ids.ok())
			{
				rapidjson::Document doc;
				doc.Parse(ids.body.c_str());
				if (!doc.HasParseError() && doc.IsArray())
				{
					std::unordered_set<int> alive;
					for (auto& v : doc.GetArray())
						if (v.IsInt())
							alive.insert(v.GetInt());

					for (auto it = records.begin(); it != records.end(); )
					{
						if (alive.find(it->first) == alive.cend())
							it = records.erase(it);
						else
							++it;
					}
				}
			}
			else if (sCancel)
				return;
		}

		if (sCancel)
			return;

		// 5b. Loose playlists: a .m3u beside the files it lists, each a RomM game of its own. Shown as one
		// game, the discs hidden. Read again only when RomM reports the playlist changed.
		{
			std::map<std::pair<std::string, std::string>, CatalogRecord*> byFile; // (system, lower file name)
			for (auto& r : records)
			{
				r.second.inPlaylist = 0;
				if (r.second.kind.empty())
					byFile[std::make_pair(r.second.system, Utils::String::toLower(r.second.fileName))] = &r.second;
			}

			std::vector<int> unusable;
			for (auto& item : records)
			{
				CatalogRecord& r = item.second;
				if (sCancel)
					return;

				if (!(r.kind.empty() || r.kind == "loose") || lowerExtension(r.fileName) != ".m3u")
					continue;

				if (RommCatalog::getSystemExtensions(r.system).count(".m3u") == 0)
					continue; // ES does not show playlists for this system: the discs stay games of their own

				std::vector<std::string> lines;
				auto old = previous.find(r.romId);
				if (old != previous.cend() && old->second.kind == "loose" && old->second.updatedAt == r.updatedAt && !old->second.discs.empty())
					lines = old->second.discs;
				else
				{
					HttpResult res = client.get("/api/roms/" + std::to_string(r.romId) + "/content/" + HttpReq::urlEncode(r.fileName) +
						"?file_ids=" + std::to_string(r.fileId), &sCancel);

					if (!res.ok())
					{
						LOG(LogWarning) << "RomM: playlist " << r.fileName << " not read : " << errorDetail(res);
						unusable.push_back(r.romId);
						continue;
					}

					for (auto line : Utils::String::split(res.body, '\n'))
					{
						line = Utils::String::trim(Utils::String::replace(line, "\r", ""));
						if (!line.empty() && line[0] != '#')
							lines.push_back(line);
					}
				}

				// Every line must be a game file of the same system, in the same folder
				std::vector<CatalogRecord*> discs;
				for (auto& line : lines)
				{
					auto disc = byFile.find(std::make_pair(r.system, Utils::String::toLower(line)));
					if (line.find('/') != std::string::npos || line.find('\\') != std::string::npos || disc == byFile.cend() || disc->second == &r)
					{
						discs.clear();
						break;
					}

					discs.push_back(disc->second);
				}

				if (discs.empty())
				{
					LOG(LogWarning) << "RomM: playlist " << r.fileName << " lists files RomM does not have, not shown";
					unusable.push_back(r.romId);
					continue;
				}

				r.kind = "loose";
				r.discs = lines;
				r.parts.clear();
				r.size = 0;

				for (auto disc : discs)
				{
					CatalogRecord::Part p;
					p.id = disc->fileId;
					p.path = disc->fileName;
					p.size = disc->size;
					p.romId = disc->romId;
					r.parts.push_back(p);
					r.size += p.size;

					disc->inPlaylist = r.romId;
				}
			}

			// A playlist that cannot be resolved would download without its discs: not shown
			for (int romId : unusable)
				records.erase(romId);
		}

		// 6. Diff against the previous snapshot
		int added = 0, updated = 0, removed = 0;
		for (auto& r : records)
		{
			auto old = previous.find(r.first);
			if (old == previous.cend())
				added++;
			else if (old->second.updatedAt != r.second.updatedAt || old->second.system != r.second.system ||
				old->second.fileName != r.second.fileName || mediaChanged(old->second, r.second) ||
				old->second.cheevosId != r.second.cheevosId || old->second.cheevosHash != r.second.cheevosHash ||
				old->second.inPlaylist != r.second.inPlaylist || old->second.kind != r.second.kind)
				updated++;
		}

		for (auto& p : previous)
			if (records.find(p.first) == records.cend())
				removed++;

		// 7. Write the snapshot (one file per system)
		Utils::FileSystem::createDirectory(libraryDir);

		std::map<std::string, std::vector<CatalogRecord>> bySystem;
		for (auto& r : records)
			bySystem[r.second.system].push_back(r.second);

		bool writeOk = true;
		for (auto& sys : bySystem)
		{
			std::sort(sys.second.begin(), sys.second.end(), [](const CatalogRecord& a, const CatalogRecord& b) { return a.romId < b.romId; });
			writeOk = writeSnapshotFile(libraryDir + "/" + sys.first + ".xml", sys.first, sys.second) && writeOk;
		}

		for (auto file : Utils::FileSystem::getDirContent(libraryDir, false, false))
		{
			if (Utils::String::toLower(Utils::FileSystem::getExtension(file)) != ".xml")
				continue;

			if (bySystem.find(Utils::FileSystem::getStem(file)) == bySystem.cend())
				Utils::FileSystem::removeFile(file);
		}

		if (!writeOk)
		{
			setStatus(_("UNABLE TO WRITE THE LIBRARY CACHE"));
			return; // state.json untouched : the next sync starts over
		}

		state.server = baseUrl;
		state.mapping = mappingSignature;
		// Incomplete: the next synchronization is a full one (an incremental one could miss changes of
		// the platforms that failed)
		state.watermark = failedPlatforms.empty() ? maxUpdated : -1;
		state.games = (int)records.size();
		state.format = SNAPSHOT_FORMAT;
		state.lastSync = localTimeString();
		writeState(statePath, state);

		LOG(LogInfo) << "RomM: synchronization done, " << records.size() << " games (+" << added << " ~" << updated << " -" << removed << ")";

		std::string summary = _("LAST SYNC") + ": " + state.lastSync + " - " + std::to_string(state.games) + " " + _("GAMES");

		if (!failedPlatforms.empty())
		{
			// e.g. "SYNC INCOMPLETE: PLAYSTATION FAILED (INTERNAL SERVER ERROR)"
			std::string names;
			for (int platformId : failedPlatforms)
			{
				auto sys = mapping.find(platformId);
				std::string name = sys != mapping.cend() ? RommCatalog::getSystemFullName(sys->second) : std::to_string(platformId);

				if (!names.empty())
					names += ", ";

				names += name;
			}

			std::unique_lock<std::mutex> lock(errorMutex);
			summary = _("SYNC INCOMPLETE") + ": " + names + " " + _("FAILED") + " (" + lastError + ")";
		}

		// 7b. Collections (regular and smart), resolved to ES system + file name
		bool collectionsChanged = false;
		{
			std::unordered_map<int, std::pair<std::string, std::string>> romFiles;
			for (auto& r : records)
				romFiles[r.first] = std::make_pair(r.second.system, r.second.fileName);

			std::vector<RommCollection> collections;
			bool collectionsOk = true;

			// Our user id, to recognize our own favorites collection (others may be public)
			long long myUserId = -1;
			{
				HttpResult me = client.get("/api/users/me", &sCancel);
				if (me.ok())
				{
					rapidjson::Document doc;
					doc.Parse(me.body.c_str());
					if (!doc.HasParseError() && doc.IsObject())
						myUserId = Json::integer(doc, "id", -1);
				}
			}

			const char* const endpoints[] = { "/api/collections", "/api/collections/smart" };
			for (int kind = 0; kind < 2 && collectionsOk && !sCancel; kind++)
			{
				bool smart = (kind == 1);

				HttpResult res = client.get(endpoints[kind], &sCancel);
				if (!res.ok())
				{
					if (smart && res.status == 404)
						continue; // server without smart collections

					collectionsOk = false;
					break;
				}

				rapidjson::Document doc;
				doc.Parse(res.body.c_str());
				if (doc.HasParseError() || !doc.IsArray())
				{
					collectionsOk = false;
					break;
				}

				for (auto& item : doc.GetArray())
				{
					long long id = Json::integer(item, "id", 0);
					if (id <= 0)
						continue;

					RommCollection c;
					c.key = std::string(smart ? "s" : "c") + std::to_string(id);
					c.smart = smart;
					c.name = Json::str(item, "name");
					c.updatedAt = Json::str(item, "updated_at");
					c.favorite = !smart && myUserId >= 0 && Json::boolean(item, "is_favorite", false) && Json::integer(item, "user_id", -2) == myUserId;
					c.owner = Json::str(item, "owner_username");
					// Unknown user id (/api/users/me failed): label nothing rather than everything
					c.mine = myUserId < 0 || Json::integer(item, "user_id", myUserId) == myUserId;

					auto ids = item.FindMember("rom_ids");
					if (ids != item.MemberEnd() && ids->value.IsArray())
					{
						for (auto& v : ids->value.GetArray())
						{
							if (!v.IsInt())
								continue;

							auto game = romFiles.find(v.GetInt());
							if (game != romFiles.cend()) // only games ES can show
								c.games.push_back(game->second);
						}
					}

					std::sort(c.games.begin(), c.games.end());
					collections.push_back(c);
				}
			}

			if (collectionsOk && !sCancel)
			{
				std::string path = getCollectionsSnapshotPath(baseUrl);
				std::string before = Utils::FileSystem::exists(path, false) ? Utils::FileSystem::readAllText(path) : "";

				writeCollectionsSnapshot(path, collections);
				collectionsChanged = Utils::FileSystem::readAllText(path) != before;

				LOG(LogInfo) << "RomM: " << collections.size() << " collections on the server";
			}
			else if (!sCancel)
			{
				LOG(LogWarning) << "RomM: unable to read collections, keeping the previous ones";
			}
		}

		// 7c. Firmware (BIOS files offered at launch). One request for every platform.
		if (!sCancel)
		{
			HttpResult fw = client.get("/api/firmware", &sCancel);
			if (fw.ok())
			{
				rapidjson::Document doc;
				doc.Parse(fw.body.c_str());
				if (!doc.HasParseError() && doc.IsArray())
				{
					std::vector<FirmwareRecord> firmware;
					for (auto& item : doc.GetArray())
					{
						if (Json::boolean(item, "missing_from_fs", false))
							continue;

						FirmwareRecord f;
						f.id = (int)Json::integer(item, "id", 0);
						f.platformId = (int)Json::integer(item, "platform_id", 0);
						f.fileName = Utils::FileSystem::getFileName(Json::str(item, "file_name"));
						f.md5 = Utils::String::toLower(Json::str(item, "md5_hash"));
						f.size = Json::integer(item, "file_size_bytes", 0);

						auto sys = mapping.find(f.platformId);
						if (sys != mapping.cend())
							f.system = sys->second;

						if (f.id > 0 && !f.fileName.empty())
							firmware.push_back(f);
					}

					writeFirmwareSnapshot(getFirmwareSnapshotPath(baseUrl), firmware);
					LOG(LogInfo) << "RomM: " << firmware.size() << " firmware files on the server";
				}
			}
			else if (!fw.cancelled)
			{
				LOG(LogWarning) << "RomM: unable to read firmware, keeping the previous list (" << errorDetail(fw) << ")";
			}
		}

		if (added + updated + removed > 0)
		{
			sPendingChanges = true;
			notify(_("ROMM LIBRARY UPDATED") + " (+" + std::to_string(added) + " / -" + std::to_string(removed) + ")");
		}
		else if (collectionsChanged)
		{
			sPendingChanges = true;
			notify(_("ROMM COLLECTIONS UPDATED"));
		}

		// Play sessions queued while the server could not be reached
		if (!sCancel)
			RommPlaySessions::flush();

		// 8. Media of the games not on the device, within the media cache size (RommMediaCache decides
		// what is evicted and downloaded). Shown at the next load of the game lists.
		struct MediaJob { const CatalogRecord* record; Media type; };
		std::vector<MediaJob> jobs;
		MediaPlan plan;

		if (!sCancel)
		{
			// A file replaced on the server comes with a new URL (its ts changes): drop the cached copy
			for (auto& m : allMedia())
			{
				size_t mi = mediaIndex(m.type);
				const std::string dir = getMediaCacheDir(baseUrl, m.type);
				if (!Utils::FileSystem::isDirectory(dir))
					continue;

				for (auto& r : records)
				{
					const CatalogRecord& rec = r.second;
					auto old = previous.find(r.first);
					if (!rec.mediaFile[mi].empty() && old != previous.cend() && old->second.mediaUrl[mi] != rec.mediaUrl[mi])
						Utils::FileSystem::removeFile(dir + "/" + rec.mediaFile[mi]);
				}
			}

			// The games whose media the cache may hold
			std::vector<const CatalogRecord*> games;
			std::map<std::pair<std::string, std::string>, int> romOf; // (system, file name) -> rom id
			for (auto& r : records)
			{
				const CatalogRecord& rec = r.second;
				romOf[std::make_pair(rec.system, rec.fileName)] = rec.romId;

				if (config.hiddenSystems.find(rec.system) != config.hiddenSystems.cend() || rec.inPlaylist > 0)
					continue; // system hidden in the RomM menu, or a disc of a playlist (the playlist's media are used)

				std::string systemPath = RommCatalog::getSystemPath(rec.system);
				if (!systemPath.empty() && Utils::FileSystem::exists(systemPath + "/" + rec.fileName, false))
					continue; // on the device

				games.push_back(&rec);
			}

			// Pinned: the user's RomM favorites and the collections selected in COLLECTIONS
			std::set<int> pinned;
			{
				std::vector<RommCollection> collections;
				readCollectionsSnapshot(getCollectionsSnapshotPath(baseUrl), collections);

				for (auto& c : collections)
				{
					if (!c.favorite && config.collections.find(c.key) == config.collections.cend())
						continue;

					for (auto& g : c.games)
					{
						auto rom = romOf.find(g);
						if (rom != romOf.cend())
							pinned.insert(rom->second);
					}
				}
			}

			saveViewed();
			plan = planMediaCache(config, games, pinned);

			for (auto& file : plan.evict)
				Utils::FileSystem::removeFile(file);

			for (auto& item : plan.download)
				if (sMediaEnabled[mediaIndex(item.type)])
					jobs.push_back(MediaJob{ item.record, item.type });

			LOG(LogInfo) << "RomM: media cache " << (plan.usage / (1024 * 1024)) << " MB, limit " <<
				(plan.limit == ~0ULL ? std::string("none") : std::to_string(plan.limit / (1024 * 1024)) + " MB") <<
				", " << plan.evict.size() << " evicted, " << jobs.size() << " to download";
		}

		if (!jobs.empty() && !sCancel)
		{
			int workers = backgroundWorkers();
			LOG(LogInfo) << "RomM: " << jobs.size() << " media files to download, " << workers << " in parallel";

			// The cache folders of the types to download (a new cache has none yet)
			{
				std::set<Media> types;
				for (auto& job : jobs)
					types.insert(job.type);

				for (auto type : types)
					Utils::FileSystem::createDirectory(getMediaCacheDir(baseUrl, type));
			}

			std::atomic<int> done(0), fetched(0), failures(0);
			std::atomic<unsigned long long> projected(plan.usage);
			std::atomic<bool> stopMedia(false);
			const int total = (int)jobs.size();

			runInBackground(jobs.size(), workers, [&](size_t k)
			{
				const MediaJob& job = jobs[k];
				size_t mi = mediaIndex(job.type);

				if (sCancel)
				{
					stopMedia = true;
					return;
				}

				if (!sMediaEnabled[mi]) // turned off meanwhile
					return;

				if (projected >= plan.admit)
					return; // the actual sizes ran past the estimates: the rest waits for the next sync

				int n = ++done;
				if (n == 1 || n % 10 == 0 || n == total)
				{
					setProgressStatus(_("DOWNLOADING MEDIA") + " " + std::to_string(n) + "/" + std::to_string(total));
					showProgress(_("DOWNLOADING MEDIA"), std::to_string(n) + " / " + std::to_string(total), (int)(n * 100LL / total));
				}

				// Resources are static files; documents uploaded to RomM come from the content endpoint
				const std::string& url = job.record->mediaUrl[mi];
				std::string path = Utils::String::startsWith(url, "/api/") ? url : coverPath(url);

				RommClient worker(config);
				HttpResult res = worker.download(path, getMediaCacheDir(baseUrl, job.type) + "/" + job.record->mediaFile[mi], nullptr, &sCancel);

				if (res.ok())
				{
					fetched++;
					projected += Utils::FileSystem::getFileSize(getMediaCacheDir(baseUrl, job.type) + "/" + job.record->mediaFile[mi]);
				}
				else if (!res.cancelled && ++failures >= 20 && fetched == 0)
				{
					LOG(LogWarning) << "RomM: media downloads stopped, 20 failures and none downloaded";
					stopMedia = true;
				}
			}, stopMedia);

			if (fetched > 0)
			{
				sPendingChanges = true; // shown once the game lists are reloaded
				notify(_("ROMM MEDIA DOWNLOADED") + " (" + std::to_string(fetched.load()) + ")");
			}
		}

		setStatus(summary);
	}
}
