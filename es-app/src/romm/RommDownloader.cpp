#include "romm/RommDownloader.h"

#ifdef BATOCERA
#include <sys/wait.h>
#endif

#include <algorithm>
#include <set>
#include <mutex>
#include "romm/RommCatalog.h"
#include "romm/RommClient.h"
#include "romm/RommConfig.h"
#include "romm/RommMedia.h"
#include "romm/RommSync.h"

#include "CollectionSystemManager.h"
#include "FileData.h"
#include "Gamelist.h"
#include "SystemData.h"
#include "HttpReq.h"
#include "LocaleES.h"
#include "Log.h"
#include "scrapers/Scraper.h"
#include "views/ViewController.h"
#include "views/gamelist/IGameListView.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <vector>

namespace Romm
{
	// Media fetched with the rom, into the cache (placeMediaAndSave moves them). fresh: replace cached copies.
	static int fetchMediaWithRom(const RemoteGame& remote, const RommConfig& config, RommClient& client, const std::atomic<bool>* cancel, bool fresh)
	{
		int fetched = 0;

		for (auto& m : allMedia())
		{
			size_t i = mediaIndex(m.type);
			if (!config.fetchesWithRom(m.type) || remote.mediaFile[i].empty() || (cancel != nullptr && cancel->load()))
				continue;

			std::string dir = getMediaCacheDir(config.baseUrl(), m.type);
			std::string file = dir + "/" + remote.mediaFile[i];

			if (fresh)
				Utils::FileSystem::removeFile(file);
			else if (Utils::FileSystem::exists(file, false))
				continue;

			Utils::FileSystem::createDirectory(dir);

			const std::string& url = remote.mediaUrl[i];
			std::string mediaPath = Utils::String::startsWith(url, "/api/") ? url : coverPath(url);

			HttpResult media = client.download(mediaPath, file, nullptr, cancel);
			if (media.ok())
				fetched++;
			else if (!media.cancelled)
				LOG(LogWarning) << "RomM: " << m.key << " not downloaded for rom " << remote.romId << " : " << errorDetail(media);
		}

		return fetched;
	}

	// Multi-file game (RommCatalog.h): every file into <target>.rommpart/, renamed to <target> once all
	// are there, then (playlists) the .m3u written last. A failed or cancelled download leaves nothing.
	static bool downloadParts(const RemoteGame& remote, const std::string& path, const RommConfig& config,
		const std::function<void(int, long long)>& progress, const std::atomic<bool>* cancel, std::string& error)
	{
		const std::string systemDir = Utils::FileSystem::getParent(path);
		const std::string target = systemDir + "/" + remote.folderName; // playlist: its discs folder; folder game: the game
		const std::string staging = target + ".rommpart";

		// Never write into a folder that is already there (files of the user)
		if (Utils::FileSystem::exists(target, false))
		{
			error = _("A FOLDER WITH THIS NAME ALREADY EXISTS") + "\n" + target;
			return false;
		}

		// A sheet's files go into the system folder: never over a file that is already there
		if (remote.kind == "sheet")
		{
			for (auto& part : remote.parts)
			{
				if (Utils::FileSystem::exists(systemDir + "/" + part.path, false))
				{
					error = _("A FILE WITH THIS NAME ALREADY EXISTS") + "\n" + part.path;
					return false;
				}
			}
		}

		Utils::FileSystem::deleteDirectoryFiles(staging, true); // left by an interrupted download
		auto discard = [&staging]() { Utils::FileSystem::deleteDirectoryFiles(staging, true); };

		long long total = 0;
		for (auto& part : remote.parts)
			total += part.size;

		long long done = 0;
		std::string ownPlaylist; // the game's own .m3u, if RomM has one (relative to the game folder)
		RommClient client(config);

		LOG(LogInfo) << "RomM: downloading rom " << remote.romId << " (" << remote.parts.size() << " files) to " << target;

		for (auto& part : remote.parts)
		{
			if (cancel != nullptr && cancel->load())
			{
				error = _("DOWNLOAD CANCELLED");
				discard();
				return false;
			}

			std::string dest = staging + "/" + part.path;
			Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(dest));

			std::string query = "/api/roms/" + std::to_string(remote.romId) + "/content/" + HttpReq::urlEncode(Utils::FileSystem::getFileName(part.path)) +
				"?file_ids=" + std::to_string(part.id);

			HttpResult res = client.download(query, dest, [&](int, long long bytes)
			{
				if (progress == nullptr)
					return;

				long long now = done + bytes;
				progress(total > 0 ? (int)std::min(100LL, now * 100 / total) : -1, now);
			}, cancel);

			if (!res.ok() || !Utils::FileSystem::exists(dest, false))
			{
				if (res.cancelled)
					error = _("DOWNLOAD CANCELLED");
				else if (res.unauthorized())
					error = _("AUTHORIZATION REJECTED, PAIR THIS DEVICE AGAIN");
				else
					error = _("DOWNLOAD FAILED") + "\n" + errorDetail(res);

				discard();
				return false;
			}

			done += part.size;

			if (remote.kind == "m3u" && Utils::String::toLower(Utils::FileSystem::getExtension(part.path)) == ".m3u" && ownPlaylist.empty())
				ownPlaylist = part.path;
		}

		// Playlist entries, relative to the system folder (where the .m3u goes)
		std::vector<std::string> entries;
		if (remote.kind == "m3u")
		{
			if (!ownPlaylist.empty())
			{
				// The game's own playlist: its lines are relative to its own folder. It is not kept in the
				// game folder, else ES would show it as a second game.
				std::string ownFile = staging + "/" + ownPlaylist;
				std::string ownDir = Utils::FileSystem::getParent(ownPlaylist);
				std::string prefix = remote.folderName + "/" + (ownDir == ownPlaylist || ownDir.empty() || ownDir == "." ? "" : ownDir + "/");

				for (auto line : Utils::String::split(Utils::FileSystem::readAllText(ownFile), '\n'))
				{
					line = Utils::String::trim(Utils::String::replace(line, "\\", "/"));
					if (!line.empty() && line[0] != '#' && line.find("..") == std::string::npos && line[0] != '/')
						entries.push_back(prefix + line);
				}

				Utils::FileSystem::removeFile(ownFile);
			}

			if (entries.empty())
				for (auto& disc : remote.discs)
					entries.push_back(remote.folderName + "/" + disc);
		}

		// A sheet: its tracks into the system folder, the sheet last (the game appears once complete)
		if (remote.kind == "sheet")
		{
			const std::string sheetName = Utils::FileSystem::getFileName(path);
			for (int pass = 0; pass < 2; pass++)
			{
				for (auto& part : remote.parts)
				{
					if ((part.path == sheetName) != (pass == 1))
						continue;

					std::string to = systemDir + "/" + part.path;
					Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(to));
					Utils::FileSystem::renameFile(staging + "/" + part.path, to, false);
					if (!Utils::FileSystem::exists(to, false))
					{
						error = _("DOWNLOAD FAILED") + "\n" + to;
						discard();
						return false;
					}
				}
			}

			discard();
			Utils::FileSystem::FileSystemCache::reset();
			return true;
		}

		// renameFile reports success when the source is missing: check the result
		Utils::FileSystem::renameFile(staging, target, false);
		if (!Utils::FileSystem::isDirectory(target))
		{
			error = _("DOWNLOAD FAILED") + "\n" + target;
			discard();
			return false;
		}

		// The playlist last: the game appears only once complete
		if (remote.kind == "m3u")
		{
			std::string text;
			for (auto& entry : entries)
				text += entry + "\n";

			writeFileAtomic(path, text);
			if (!Utils::FileSystem::exists(path, false))
			{
				error = _("DOWNLOAD FAILED") + "\n" + path;
				return false;
			}
		}

		Utils::FileSystem::FileSystemCache::reset();
		return true;
	}

	// ---------------------------------------------------------------------------------------------
	// Record of the games downloaded from RomM
	// ---------------------------------------------------------------------------------------------

	static std::mutex sDownloadedMutex;

	std::string getDownloadedRecordPath()
	{
		return getConfigDirectory() + "/downloaded.txt";
	}

	static std::set<std::string> readDownloaded()
	{
		std::set<std::string> paths;
		std::string file = getDownloadedRecordPath();
		if (Utils::FileSystem::exists(file, false))
			for (auto& line : Utils::String::split(Utils::FileSystem::readAllText(file), '\n', true))
				paths.insert(line);

		return paths;
	}

	static void writeDownloaded(const std::set<std::string>& paths)
	{
		std::string text;
		for (auto& path : paths)
			text += path + "\n";

		Utils::FileSystem::createDirectory(getConfigDirectory());
		writeFileAtomic(getDownloadedRecordPath(), text);
	}

	static void recordDownloaded(const std::string& path, bool downloaded)
	{
		std::unique_lock<std::mutex> lock(sDownloadedMutex);

		std::set<std::string> paths = readDownloaded();
		bool changed = downloaded ? paths.insert(path).second : paths.erase(path) > 0;
		if (changed)
			writeDownloaded(paths);
	}

	bool isDownloadedFromRomm(const std::string& path)
	{
		std::unique_lock<std::mutex> lock(sDownloadedMutex);
		return readDownloaded().count(path) > 0;
	}

	// Every list showing the game, including the custom collections bundle the broadcast does not reach
	static void refreshGameViews(FileData* source)
	{
		if (ViewController::get() == nullptr)
			return;

		ViewController::get()->onFileChanged(source, FILE_METADATA_CHANGED);

		if (CollectionSystemManager::get() != nullptr)
		{
			SystemData* bundle = CollectionSystemManager::get()->getCustomCollectionsBundle();
			if (bundle != nullptr && bundle->getRootFolder() != nullptr && bundle->getRootFolder()->FindByPath(source->getPath()) != nullptr)
			{
				auto view = ViewController::get()->getGameListView(bundle, false);
				if (view != nullptr)
					view->onFileChanged(source, FILE_METADATA_CHANGED);
			}
		}
	}

	bool onGameDeleted(FileData* game)
	{
		if (game == nullptr || game->getType() != GAME)
			return false;

		FileData* source = game->getSourceFileData();
		const std::string path = source->getPath();

		RemoteGame remote;
		if (!RommCatalog::getKnownGame(path, remote) || Utils::FileSystem::exists(path, false))
			return false; // not a RomM game, or its file is still there

		// Playlist games: ES deleted the playlist and the discs it lists; the game folder is ours
		// (a download refuses to write into an existing folder)
		if (remote.kind == "m3u" && !remote.folderName.empty())
			Utils::FileSystem::deleteDirectoryFiles(Utils::FileSystem::getParent(path) + "/" + remote.folderName, true);

		// Loose playlists: ES removes the playlist before reading the discs it lists, so it cannot
		// remove them; their names are known here
		if (remote.kind == "loose")
			for (auto& part : remote.parts)
				Utils::FileSystem::removeFile(Utils::FileSystem::getParent(path) + "/" + Utils::FileSystem::getFileName(part.path));

		// Media fields pointing to files ES just deleted
		MetaDataList& md = source->getMetadata();
		for (auto mdd : md.getMDD())
			if (md.getType(mdd.id) == MetaDataType::MD_PATH && !md.get(mdd.id).empty() && !Utils::FileSystem::exists(md.get(mdd.id), false))
				md.set(mdd.id, "");

		RommCatalog::registerRemote(path, remote);
		source->setRemote(true);
		recordDownloaded(path, false);

		LOG(LogInfo) << "RomM: " << path << " deleted from the device, offered for download again";

		refreshGameViews(source);
		return true;
	}

	// Loose playlist: its discs (games of their own for RomM) beside it in the system folder, then the
	// playlist itself, last. Discs already there are kept; on failure, the discs this download
	// created are removed.
	static bool downloadLoosePlaylist(const RemoteGame& remote, const std::string& path, const RommConfig& config,
		const std::function<void(int, long long)>& progress, const std::atomic<bool>* cancel, std::string& error)
	{
		const std::string systemDir = Utils::FileSystem::getParent(path);

		long long total = 0;
		for (auto& part : remote.parts)
			total += part.size;

		long long done = 0;
		std::vector<std::string> created;
		RommClient client(config);

		auto fail = [&](const HttpResult& res)
		{
			if (res.cancelled)
				error = _("DOWNLOAD CANCELLED");
			else if (res.unauthorized())
				error = _("AUTHORIZATION REJECTED, PAIR THIS DEVICE AGAIN");
			else
				error = _("DOWNLOAD FAILED") + "\n" + errorDetail(res);

			for (auto& file : created)
				Utils::FileSystem::removeFile(file);

			return false;
		};

		LOG(LogInfo) << "RomM: downloading playlist " << remote.fileName << " (" << remote.parts.size() << " games) to " << systemDir;

		for (auto& part : remote.parts)
		{
			std::string dest = systemDir + "/" + Utils::FileSystem::getFileName(part.path);
			if (Utils::FileSystem::exists(dest, false))
			{
				done += part.size; // already on the device
				continue;
			}

			int romId = part.romId > 0 ? part.romId : remote.romId;
			std::string query = "/api/roms/" + std::to_string(romId) + "/content/" + HttpReq::urlEncode(Utils::FileSystem::getFileName(part.path)) +
				"?file_ids=" + std::to_string(part.id);

			HttpResult res = client.download(query, dest, [&](int, long long bytes)
			{
				if (progress != nullptr)
				{
					long long now = done + bytes;
					progress(total > 0 ? (int)std::min(100LL, now * 100 / total) : -1, now);
				}
			}, cancel);

			if (!res.ok() || !Utils::FileSystem::exists(dest, false))
				return fail(res);

			created.push_back(dest);
			done += part.size;
		}

		// The playlist last: the game appears only once its discs are there
		std::string query = "/api/roms/" + std::to_string(remote.romId) + "/content/" + HttpReq::urlEncode(remote.fileName) +
			"?file_ids=" + std::to_string(remote.fileId);

		HttpResult res = client.download(query, path, nullptr, cancel);
		if (!res.ok() || !Utils::FileSystem::exists(path, false))
			return fail(res);

		Utils::FileSystem::FileSystemCache::reset();
		return true;
	}

#ifdef BATOCERA
	static std::string shellQuote(const std::string& s)
	{
		std::string q = "'";
		for (char c : s)
			q += (c == '\'') ? std::string("'\\''") : std::string(1, c);
		return q + "'";
	}

	// Runs a command, its output in output; its exit status (-1: not run)
	static int runCommand(const std::string& command, std::string& output)
	{
		FILE* pipe = popen((command + " 2>&1").c_str(), "r");
		if (pipe == nullptr)
			return -1;

		char buffer[512];
		while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
			output += buffer;

		int status = pclose(pipe);
		return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	}

	static std::string lastLine(const std::string& text)
	{
		std::string trimmed = Utils::String::trim(text);
		size_t eol = trimmed.rfind('\n');
		return eol == std::string::npos ? trimmed : trimmed.substr(eol + 1);
	}

	static const char* PS3_INSTALLER = "/usr/bin/batocera-romm-ps3-install";

	// PS3 PSN game: its files into <name>.rommpart/, then batocera-romm-ps3-install installs them
	// into RPCS3 and writes <name>.psn (RetroBat installs PSN games its own way)
	static bool downloadPsn(const RemoteGame& remote, const std::string& path, const RommConfig& config,
		const std::function<void(int, long long)>& progress, const std::atomic<bool>* cancel, std::string& error)
	{
		std::string staging = Utils::FileSystem::getParent(path) + "/" + Utils::FileSystem::getStem(path) + ".rommpart";
		Utils::FileSystem::deleteDirectoryFiles(staging, true);
		Utils::FileSystem::createDirectory(staging);

		long long total = 0;
		for (auto& part : remote.parts)
			total += part.size;

		long long done = 0;
		RommClient client(config);

		for (auto& part : remote.parts)
		{
			std::string dest = staging + "/" + part.path;
			Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(dest));

			std::string query = "/api/roms/" + std::to_string(remote.romId) + "/content/" +
				HttpReq::urlEncode(Utils::FileSystem::getFileName(part.path)) + "?file_ids=" + std::to_string(part.id);

			HttpResult res = client.download(query, dest, [&](int, long long bytes)
			{
				if (progress != nullptr)
					progress(total > 0 ? (int)std::min(100LL, (done + bytes) * 100 / total) : -1, done + bytes);
			}, cancel);

			if (!res.ok() || !Utils::FileSystem::exists(dest, false))
			{
				if (res.cancelled)
					error = _("DOWNLOAD CANCELLED");
				else if (res.unauthorized())
					error = _("AUTHORIZATION REJECTED, PAIR THIS DEVICE AGAIN");
				else
					error = _("DOWNLOAD FAILED") + "\n" + errorDetail(res);

				Utils::FileSystem::deleteDirectoryFiles(staging, true);
				return false;
			}

			done += part.size;
		}

		if (progress != nullptr)
			progress(-2, done); // installing

		std::string output;
		int status = runCommand(std::string(PS3_INSTALLER) + " install " + shellQuote(staging) + " " + shellQuote(path), output);
		LOG(LogInfo) << "RomM: " << PS3_INSTALLER << " " << path << ": " << Utils::String::trim(output);

		if (status != 0 || !Utils::FileSystem::exists(path, false))
		{
			error = _("INSTALLATION FAILED") + "\n" + lastLine(output);
			Utils::FileSystem::deleteDirectoryFiles(staging, true);
			return false;
		}

		return true;
	}
#endif

#ifdef BATOCERA
	// Disc game: its add-ons (updates, DLC, licenses) into <name>.rommaddons/, installed into RPCS3 by
	// batocera-romm-ps3-install. The game is playable without them: a failure is logged, not fatal.
	static void installPs3Addons(const RemoteGame& remote, const std::string& path, const RommConfig& config,
		const std::function<void(int, long long)>& progress, const std::atomic<bool>* cancel)
	{
		std::string staging = Utils::FileSystem::getParent(path) + "/" + Utils::FileSystem::getStem(path) + ".rommaddons";
		Utils::FileSystem::deleteDirectoryFiles(staging, true);
		Utils::FileSystem::createDirectory(staging);

		RommClient client(config);
		for (auto& part : remote.parts)
		{
			std::string dest = staging + "/" + part.path;
			Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(dest));

			std::string query = "/api/roms/" + std::to_string(remote.romId) + "/content/" +
				HttpReq::urlEncode(Utils::FileSystem::getFileName(part.path)) + "?file_ids=" + std::to_string(part.id);

			HttpResult res = client.download(query, dest, nullptr, cancel);
			if (!res.ok())
			{
				LOG(LogWarning) << "RomM: " << part.path << " not downloaded for " << path << " : " << errorDetail(res);
				Utils::FileSystem::deleteDirectoryFiles(staging, true);
				return;
			}
		}

		if (progress != nullptr)
			progress(-2, 0); // installing

		std::string output;
		int status = runCommand(std::string(PS3_INSTALLER) + " install-addons " + shellQuote(staging), output);
		LOG(status == 0 ? LogInfo : LogWarning) << "RomM: " << PS3_INSTALLER << " install-addons " << path << ": " << Utils::String::trim(output);

		Utils::FileSystem::deleteDirectoryFiles(staging, true);
	}
#endif

#ifdef BATOCERA
	// <roms>/switch_update, where Batocera's Switch emulators look for updates and DLC
	static std::string switchUpdateDir(const std::string& gamePath)
	{
		return Utils::FileSystem::getParent(Utils::FileSystem::getParent(gamePath)) + "/switch_update";
	}

	// Switch: the game's updates and DLC into switch_update/update/ and switch_update/dlc/. A file
	// already there is kept. The game is playable without them: a failure is logged, not fatal.
	static void downloadSwitchAddons(const RemoteGame& remote, const std::string& path, const RommConfig& config,
		const std::function<void(int, long long)>& progress, const std::atomic<bool>* cancel)
	{
		const std::string root = switchUpdateDir(path);

		long long done = remote.size;
		for (auto& part : remote.parts)
			done -= part.size; // the base game, downloaded

		RommClient client(config);
		for (auto& part : remote.parts)
		{
			std::string dest = root + "/" + part.path;
			if (Utils::FileSystem::exists(dest, false))
			{
				done += part.size;
				continue;
			}

			Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(dest));

			std::string query = "/api/roms/" + std::to_string(remote.romId) + "/content/" +
				HttpReq::urlEncode(Utils::FileSystem::getFileName(part.path)) + "?file_ids=" + std::to_string(part.id);

			HttpResult res = client.download(query, dest, [&](int, long long bytes)
			{
				if (progress != nullptr)
					progress(remote.size > 0 ? (int)std::min(100LL, (done + bytes) * 100 / remote.size) : -1, done + bytes);
			}, cancel);

			if (!res.ok())
			{
				LOG(LogWarning) << "RomM: " << part.path << " not downloaded for " << path << " : " << errorDetail(res);
				if (res.cancelled)
					return;
			}

			done += part.size;
		}
	}
#endif

	void onGameDeleting(FileData* game)
	{
		if (game == nullptr || game->getType() != GAME)
			return;

		std::string path = game->getSourceFileData()->getPath();
		RemoteGame remote;
		if (!RommCatalog::getKnownGame(path, remote) || !Utils::FileSystem::exists(path, false))
			return;

		// A sheet's tracks: ES deletes a sheet before reading it for the tracks it lists, so it never
		// finds them. The files RomM lists for the game are removed here.
		if (remote.kind == "sheet")
		{
			const std::string systemDir = Utils::FileSystem::getParent(path);
			const std::string sheetName = Utils::FileSystem::getFileName(path);
			for (auto& part : remote.parts)
				if (part.path != sheetName)
					Utils::FileSystem::removeFile(systemDir + "/" + part.path);

			return;
		}

#ifdef BATOCERA
		// Switch: its updates and DLC go with it
		if (remote.kind == "switchaddons")
		{
			const std::string root = switchUpdateDir(path);
			for (auto& part : remote.parts)
				Utils::FileSystem::removeFile(root + "/" + part.path);

			return;
		}

		if (remote.kind != "psn")
			return;

		std::string output;
		runCommand(std::string(PS3_INSTALLER) + " uninstall " + shellQuote(path), output);
		LOG(LogInfo) << "RomM: " << PS3_INSTALLER << " uninstall " << path << ": " << Utils::String::trim(output);
#endif
	}

	bool downloadGame(const std::string& path, const std::function<void(int, long long)>& progress,
		const std::atomic<bool>* cancel, std::string& error)
	{
		RemoteGame remote;
		if (!RommCatalog::getRemoteGame(path, remote))
		{
			error = _("THIS GAME IS NOT AVAILABLE ON ROMM");
			return false;
		}

		RommConfig config = RommConfig::load();
		if (!config.isPaired())
		{
			error = _("THIS DEVICE IS NOT PAIRED WITH ROMM");
			return false;
		}

		std::string folder = Utils::FileSystem::getParent(path);
		if (!Utils::FileSystem::createDirectory(folder))
		{
			error = _("UNABLE TO CREATE FOLDER") + " " + folder;
			return false;
		}

		RommClient client(config);

#ifdef BATOCERA
		if (remote.kind == "psn")
		{
			if (!downloadPsn(remote, path, config, progress, cancel, error))
				return false;
		}
		else
#endif
		if (remote.kind == "loose")
		{
			if (!downloadLoosePlaylist(remote, path, config, progress, cancel, error))
				return false;
		}
		else if (!remote.kind.empty() && remote.kind != "ps3addons" && remote.kind != "switchaddons")
		{
			// Playlist (A) or folder game (B)
			if (!downloadParts(remote, path, config, progress, cancel, error))
				return false;
		}
		else
		{
			std::string query = "/api/roms/" + std::to_string(remote.romId) + "/content/" + HttpReq::urlEncode(remote.fileName) +
				"?file_ids=" + std::to_string(remote.fileId);

			LOG(LogInfo) << "RomM: downloading rom " << remote.romId << " to " << path;

			HttpResult res = client.download(query, path, progress, cancel);

			if (!res.ok())
			{
				if (res.cancelled)
					error = _("DOWNLOAD CANCELLED");
				else if (res.unauthorized())
					error = _("AUTHORIZATION REJECTED, PAIR THIS DEVICE AGAIN");
				else
					error = _("DOWNLOAD FAILED") + "\n" + errorDetail(res);

				return false;
			}
		}

		if (!Utils::FileSystem::exists(path, false))
		{
			error = _("DOWNLOAD FAILED");
			return false;
		}

#ifdef BATOCERA
		if (remote.kind == "ps3addons" && !(cancel != nullptr && cancel->load()))
			installPs3Addons(remote, path, config, progress, cancel);

		if (remote.kind == "switchaddons" && !(cancel != nullptr && cancel->load()))
			downloadSwitchAddons(remote, path, config, progress, cancel);
#endif

		// A missing media never fails the game download
		fetchMediaWithRom(remote, config, client, cancel, false);

		return true;
	}

	// Media of the game in the cache moved next to the rom, fields and IMAGE SHOWN set, saved to the gamelist
	static void placeMediaAndSave(FileData* source);

	void onGameDownloaded(FileData* game, bool refreshViews)
	{
		if (game == nullptr)
			return;

		FileData* source = game->getSourceFileData();
		if (source == nullptr)
			return;

		source->setRemote(false);
		Utils::FileSystem::FileSystemCache::reset();

		recordDownloaded(source->getPath(), true);
		placeMediaAndSave(source);

		// Refresh what is displayed (the cloud badge goes away), as ES does when a favorite changes
		if (CollectionSystemManager::get() != nullptr)
			CollectionSystemManager::get()->refreshCollectionSystems(source);

		if (refreshViews)
			refreshGameViews(source);
	}

	bool downloadGameMedia(const std::string& path, const std::atomic<bool>* cancel, std::string& error)
	{
		RemoteGame remote;
		if (!RommCatalog::getKnownGame(path, remote))
		{
			error = _("THIS GAME IS NOT IN ROMM");
			return false;
		}

		RommConfig config = RommConfig::load();
		if (!config.isUsable())
		{
			error = _("ROMM IS NOT CONFIGURED");
			return false;
		}

		RommClient client(config);
		fetchMediaWithRom(remote, config, client, cancel, true);

		if (cancel != nullptr && cancel->load())
		{
			error = _("DOWNLOAD CANCELLED");
			return false;
		}

		return true;
	}

	void onGameMediaDownloaded(FileData* game)
	{
		if (game == nullptr || game->getSourceFileData() == nullptr)
			return;

		Utils::FileSystem::FileSystemCache::reset();
		placeMediaAndSave(game->getSourceFileData());
	}

	void onGameMediaDownloadedAt(const std::string& path)
	{
		for (auto system : SystemData::sSystemVector)
		{
			if (!system->isGameSystem() || system->isCollection() || system->getRootFolder() == nullptr)
				continue;

			FileData* game = system->getRootFolder()->FindByPath(path);
			if (game != nullptr && game->getType() == GAME && !game->isRemote())
			{
				Utils::FileSystem::FileSystemCache::reset();
				placeMediaAndSave(game->getSourceFileData());
				return;
			}
		}
	}

	static void placeMediaAndSave(FileData* source)
	{
		// Cached media move next to the rom, where the ES scraper puts them
		RommConfig config = RommConfig::load();
		RemoteGame remote;
		bool known = RommCatalog::getKnownGame(source->getPath(), remote); // downloaded just now, or already on the device

		std::string placedAt[(size_t)Media::Count];
		auto cachedFile = [&](Media type)
		{
			size_t i = mediaIndex(type);
			return known && !remote.mediaFile[i].empty() ? getMediaCacheDir(config.baseUrl(), type) + "/" + remote.mediaFile[i] : std::string();
		};

		const std::string largeCover = cachedFile(Media::CoverLarge);
		const bool hasLargeCover = !largeCover.empty() && Utils::FileSystem::exists(largeCover, false);

		for (auto& m : allMedia())
		{
			size_t i = mediaIndex(m.type);
			std::string cached = cachedFile(m.type);
			if (cached.empty() || !Utils::FileSystem::exists(cached, false))
				continue;

			// The large cover (DOWNLOAD ALONG WITH ROM) takes the small one's place next to the rom
			if (m.type == Media::Cover && hasLargeCover)
			{
				Utils::FileSystem::removeFile(cached);
				continue;
			}

			MetaDataId field = m.type == Media::CoverLarge ? MetaDataId::Image : m.metadata;
			std::string ext = Utils::FileSystem::getExtension(cached);
			std::string target = Scraper::getSaveAsPath(source, field, ext);

			// A name of its own (<name>-mix, -mixv2, -screenshot...): the ES scraper's would be another media's
			if (m.saveSuffix[0] != '\0')
			{
				std::string stem = target.substr(0, target.size() - ext.size());
				size_t dash = stem.rfind('-');
				if (dash != std::string::npos)
					target = stem.substr(0, dash) + "-" + m.saveSuffix + ext;
			}

			Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(target));

			// Same file system in practice (/userdata): a rename; a copy otherwise
			bool placed = Utils::FileSystem::renameFile(cached, target, true);
			if (!placed && Utils::FileSystem::copyFile(cached, target))
			{
				Utils::FileSystem::removeFile(cached);
				placed = true;
			}

			if (!placed)
				continue;

			placedAt[i] = target;

			if (!m.imageOnly || m.type == Media::CoverLarge)
				source->setMetadata(field, target);
		}

		// IMAGE SHOWN: the chosen media is the image when it came with the game
		Media image;
		if (config.imageMedia(image) && !placedAt[mediaIndex(image)].empty())
			source->setMetadata(MetaDataId::Image, placedAt[mediaIndex(image)]);

		// Persist the RomM metadata in the gamelist, like a scraped game
		source->getMetadata().setDirty();
		saveToGamelistRecovery(source);
	}
}
