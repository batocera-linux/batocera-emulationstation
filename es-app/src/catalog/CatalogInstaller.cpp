#include "catalog/CatalogInstaller.h"

#include "HttpReq.h"
#include "LocaleES.h"
#include "Log.h"
#include "Paths.h"
#include "SystemConf.h"
#include "SystemData.h"
#include "catalog/CatalogSystemConfig.h"
#include "utils/FileSystemUtil.h"
#include "utils/Sha256.h"
#include "utils/StringUtil.h"
#include "utils/ZipFile.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <regex>
#include <thread>

#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace catalog;

std::mutex CatalogInstaller::mLock;
std::optional<Catalog> CatalogInstaller::mCatalog;

namespace
{
	constexpr const char* DEFAULT_CATALOG_URL = "https://raw.githubusercontent.com/JyuHo/es-re-catalog/main/catalog.json";
	constexpr const char* FLATHUB_REPO = "https://dl.flathub.org/repo/flathub.flatpakrepo";

	CatalogInstaller::Result ok(const std::string& message = "OK") { return { message, 0 }; }
	CatalogInstaller::Result failed(const std::string& message) { return { message, 1 }; }

	void report(const CatalogInstaller::Progress& progress, const std::string& text)
	{
		if (progress)
			progress(text);
	}

	std::string downloadsPath()
	{
		return Paths::getCachePath() + "/catalog";
	}

	// Runs a program without a shell, so that nothing in the catalog is ever interpreted
	// as shell syntax. Each line it prints goes to `onLine`. Returns its exit code, -1 if
	// it could not be started.
	int run(const std::vector<std::string>& argv, const std::function<void(const std::string&)>& onLine = nullptr)
	{
		LOG(LogInfo) << "Catalog: running " << Utils::String::join(argv, " ");

		int fds[2];
		if (pipe(fds) != 0)
			return -1;

		pid_t pid = fork();
		if (pid < 0)
		{
			close(fds[0]);
			close(fds[1]);
			return -1;
		}

		if (pid == 0)
		{
			dup2(fds[1], STDOUT_FILENO);
			dup2(fds[1], STDERR_FILENO);
			close(fds[0]);
			close(fds[1]);

			std::vector<char*> args;
			for (auto& a : argv)
				args.push_back(const_cast<char*>(a.c_str()));
			args.push_back(nullptr);

			execvp(args[0], args.data());
			_exit(127);
		}

		close(fds[1]);

		std::string pending;
		char buffer[512];
		ssize_t n;
		while ((n = read(fds[0], buffer, sizeof(buffer))) > 0)
		{
			pending.append(buffer, n);

			size_t eol;
			while ((eol = pending.find_first_of("\r\n")) != std::string::npos)
			{
				auto line = Utils::String::trim(pending.substr(0, eol));
				pending.erase(0, eol + 1);
				if (!line.empty())
				{
					LOG(LogDebug) << "Catalog: " << line;
					if (onLine)
						onLine(line);
				}
			}
		}
		close(fds[0]);

		int status = 0;
		if (waitpid(pid, &status, 0) < 0)
			return -1;

		return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	}

	bool findInPath(const std::string& binary)
	{
		const char* path = std::getenv("PATH");
		if (path == nullptr)
			return false;

		for (auto& dir : Utils::String::split(path, ':'))
			if (!dir.empty() && access((dir + "/" + binary).c_str(), X_OK) == 0)
				return true;

		return false;
	}

	std::string userFlatpakPath()
	{
		const char* dataHome = std::getenv("XDG_DATA_HOME");
		if (dataHome && *dataHome)
			return std::string(dataHome) + "/flatpak";

		return Paths::getHomePath() + "/.local/share/flatpak";
	}

	bool isFlatpakInstalled(const std::string& ref)
	{
		return fs::exists(userFlatpakPath() + "/app/" + ref) || fs::exists("/var/lib/flatpak/app/" + ref);
	}

	bool makeExecutable(const fs::path& file)
	{
		std::error_code ec;
		fs::permissions(file, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add, ec);
		return !ec;
	}

	bool extractZip(const std::string& zipFile, const std::string& destination, const std::function<bool(const std::string&)>& filter = nullptr)
	{
		Utils::Zip::ZipFile zip;
		if (!zip.load(zipFile))
			return false;

		fs::create_directories(destination);

		for (auto& name : zip.namelist())
		{
			// Refuse entries that would land outside the destination
			if (name.empty() || name[0] == '/' || name.find("..") != std::string::npos)
				continue;

			if (Utils::String::endsWith(name, "/"))
			{
				fs::create_directories(fs::path(destination) / name);
				continue;
			}

			if (filter && !filter(name))
				continue;

			fs::create_directories((fs::path(destination) / name).parent_path());
			if (!zip.extract(name, destination))
				return false;
		}

		return true;
	}

	bool extractArchive(const std::string& archive, const std::string& destination)
	{
		if (Utils::String::toLower(Utils::FileSystem::getExtension(archive)) == ".zip")
			return extractZip(archive, destination);

		// tar.gz, tar.xz, tar.zst...: GNU tar and bsdtar both guess the compression
		fs::create_directories(destination);
		return run({ "tar", "-xf", archive, "-C", destination, "--no-same-owner" }) == 0;
	}

	std::string fileNameOf(const std::string& url)
	{
		auto name = Utils::FileSystem::getFileName(url);
		auto query = name.find('?');
		if (query != std::string::npos)
			name = name.substr(0, query);
		return name;
	}

	SystemConfigPaths systemConfigPaths()
	{
		SystemConfigPaths paths;
		paths.configDir = Paths::getConfigPath();
		paths.baseConfigPath = SystemData::getConfigPath();
		paths.homePath = Paths::getHomePath();
		paths.roots = { Paths::getConfigPath(), Paths::getEmulationStationPath() };
		paths.roots.insert(paths.roots.end(), Paths::getSystemConfigPaths().cbegin(), Paths::getSystemConfigPaths().cend());
		paths.roots.insert(paths.roots.end(), Paths::getSystemDataPaths().cbegin(), Paths::getSystemDataPaths().cend());
		return paths;
	}
}

std::string CatalogInstaller::catalogUrl()
{
	auto url = SystemConf::getInstance()->get("global.catalog.url");
	return url.empty() ? DEFAULT_CATALOG_URL : url;
}

std::string CatalogInstaller::emulatorsPath() { return Paths::getDataPath() + "/emulators"; }
std::string CatalogInstaller::coresPath() { return Paths::getDataPath() + "/cores"; }
std::string CatalogInstaller::installedPath() { return emulatorsPath() + "/installed.json"; }

std::optional<Catalog> CatalogInstaller::fetchCatalog(std::string* error)
{
	auto url = catalogUrl();
	auto arch = hostArchitecture();
	auto cacheFile = downloadsPath() + "/catalog.json";

	std::string json;
	HttpReq request(url);
	if (request.wait())
	{
		json = request.getContent();
		Utils::FileSystem::createDirectory(downloadsPath());
	}
	else
	{
		LOG(LogWarning) << "Catalog: cannot download " << url << ": " << request.getErrorMsg() << ", using the last copy";
		json = Utils::FileSystem::readAllText(cacheFile);
	}

	std::string why;
	auto parsed = parseCatalog(json, arch, url, &why);
	if (!parsed)
	{
		LOG(LogError) << "Catalog: " << url << " is not usable: " << why;
		if (error)
			*error = json.empty() ? _("Cannot download the catalog") : why;
		return std::nullopt;
	}

	if (request.status() == HttpReq::REQ_SUCCESS)
		Utils::FileSystem::writeAllText(cacheFile, json);

	std::lock_guard<std::mutex> lock(mLock);
	mCatalog = parsed;
	return parsed;
}

std::optional<Catalog> CatalogInstaller::currentCatalog(std::string* error)
{
	{
		std::lock_guard<std::mutex> lock(mLock);
		if (mCatalog)
			return mCatalog;
	}
	return fetchCatalog(error);
}

Installed CatalogInstaller::loadInstalled()
{
	std::lock_guard<std::mutex> lock(mLock);
	return Installed::parse(Utils::FileSystem::readAllText(installedPath()));
}

bool CatalogInstaller::saveInstalled(const Installed& installed)
{
	std::lock_guard<std::mutex> lock(mLock);

	std::error_code ec;
	fs::create_directories(emulatorsPath(), ec);

	// Write next to it, then rename: es-launcher may be reading it
	auto tmp = installedPath() + ".tmp";
	Utils::FileSystem::writeAllText(tmp, installed.toJson());
	fs::rename(tmp, installedPath(), ec);
	return !ec;
}

bool CatalogInstaller::isFlatpakAvailable()
{
	static const bool available = findInPath("flatpak");
	return available;
}

bool CatalogInstaller::isFoundOnSystem(const Emulator& emulator)
{
	for (auto& binary : emulator.binaries)
		if (findInPath(binary))
			return true;

	for (auto& build : emulator.builds)
		if (build.type == BuildType::Flatpak && isFlatpakInstalled(build.ref))
			return true;

	return false;
}

bool CatalogInstaller::isCoreFoundOnSystem(const Core& core)
{
	auto file = core.id + "_libretro.so";

	std::vector<std::string> dirs = { "/usr/lib/libretro", "/usr/lib64/libretro", "/usr/local/lib/libretro" };

	// Debian and Armbian: /usr/lib/<triplet>/libretro
	std::error_code ec;
	for (auto& entry : fs::directory_iterator("/usr/lib", ec))
		if (entry.is_directory(ec) && Utils::String::endsWith(entry.path().filename().string(), "-linux-gnu"))
			dirs.push_back(entry.path().string() + "/libretro");

	for (auto& dir : dirs)
		if (fs::exists(dir + "/" + file, ec))
			return true;

	return false;
}

State CatalogInstaller::state(const Emulator& emulator, const Installed& installed)
{
	bool system = installed.emulators.count(emulator.id) == 0 && isFoundOnSystem(emulator);
	return emulatorState(emulator, installed, isFlatpakAvailable(), system);
}

State CatalogInstaller::state(const Core& core, const Installed& installed)
{
	bool system = installed.cores.count(core.id) == 0 && isCoreFoundOnSystem(core);
	return coreState(core, installed, system);
}

bool CatalogInstaller::download(const std::string& url, const std::string& file, const std::string& label, const Progress& progress)
{
	Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(file));
	Utils::FileSystem::removeFile(file);

	report(progress, _("Downloading") + " " + label);

	HttpReq request(url, file);
	while (request.status() == HttpReq::REQ_IN_PROGRESS)
	{
		if (request.getPercent() >= 0)
			report(progress, _("Downloading") + " " + label + " >>> " + std::to_string(request.getPercent()) + " %");

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	if (request.status() != HttpReq::REQ_SUCCESS)
	{
		LOG(LogError) << "Catalog: cannot download " << url << ": " << request.getErrorMsg();
		Utils::FileSystem::removeFile(file);
		return false;
	}

	return true;
}

CatalogInstaller::Result CatalogInstaller::installEmulator(const std::string& id, const Progress& progress)
{
	if (!isSafeId(id))
		return failed("bad id");

	std::string error;
	auto cat = currentCatalog(&error);
	if (!cat)
		return failed(error);

	auto emulator = cat->findEmulator(id);
	if (emulator == nullptr)
		return failed(_("Not in the catalog for this machine"));

	auto build = preferredBuild(*emulator, isFlatpakAvailable());
	if (build == nullptr)
		return failed(_("Needs Flatpak, which is not installed"));

	InstalledItem item;
	item.type = typeName(build->type);
	item.version = build->version;

	if (build->type == BuildType::Flatpak)
	{
		report(progress, _("Installing") + " " + emulator->name + " (Flatpak)");

		if (build->remote == "flathub")
			run({ "flatpak", "remote-add", "--user", "--if-not-exists", "flathub", FLATHUB_REPO });

		int code = run({ "flatpak", "install", "--user", "--noninteractive", "-y", build->remote, build->ref },
			[&](const std::string& line) { report(progress, emulator->name + " : " + line); });

		if (code != 0)
			return failed(_("Flatpak installation failed"));

		item.ref = build->ref;
		item.command = { "flatpak", "run", build->ref };
	}
	else
	{
		auto archive = downloadsPath() + "/" + id + "-" + fileNameOf(build->url);
		if (!download(build->url, archive, emulator->name, progress))
			return failed(_("Download failed"));

		report(progress, _("Checking") + " " + emulator->name);
		if (Utils::Sha256::ofFile(archive) != Utils::String::toLower(build->sha256))
		{
			Utils::FileSystem::removeFile(archive);
			return failed(_("The downloaded file is corrupted"));
		}

		// Install next to the current version, then swap, so a failure leaves it untouched
		fs::path target = fs::path(emulatorsPath()) / id;
		fs::path staging = fs::path(emulatorsPath()) / (id + ".new");
		std::error_code ec;
		fs::remove_all(staging, ec);
		fs::create_directories(staging, ec);

		fs::path program;
		if (build->type == BuildType::AppImage)
		{
			program = staging / (id + ".AppImage");
			fs::rename(archive, program, ec);
			if (ec)
			{
				fs::copy_file(archive, program, ec);
				fs::remove(archive);
			}
		}
		else
		{
			report(progress, _("Extracting") + " " + emulator->name);
			bool extracted = extractArchive(archive, staging.string());
			fs::remove(archive, ec);
			if (!extracted)
			{
				fs::remove_all(staging, ec);
				return failed(_("Cannot extract the archive"));
			}

			std::regex exec;
			try { exec = std::regex(build->exec); }
			catch (const std::regex_error&) { fs::remove_all(staging, ec); return failed("bad exec pattern"); }

			for (auto& entry : fs::recursive_directory_iterator(staging, ec))
			{
				auto relative = fs::relative(entry.path(), staging, ec).generic_string();
				if (entry.is_regular_file(ec) && std::regex_search(relative, exec))
				{
					program = entry.path();
					break;
				}
			}

			if (program.empty())
			{
				fs::remove_all(staging, ec);
				return failed(_("The archive does not contain the emulator"));
			}
		}

		if (ec || !makeExecutable(program))
		{
			fs::remove_all(staging, ec);
			return failed(_("Cannot install the emulator"));
		}

		auto relativeProgram = fs::relative(program, staging, ec);

		fs::path previous = fs::path(emulatorsPath()) / (id + ".old");
		fs::remove_all(previous, ec);
		if (fs::exists(target))
			fs::rename(target, previous, ec);

		fs::rename(staging, target, ec);
		if (ec)
		{
			std::error_code ignored;
			fs::remove_all(staging, ignored);
			if (fs::exists(previous, ignored))
				fs::rename(previous, target, ignored);
			return failed(_("Cannot install the emulator"));
		}
		fs::remove_all(previous, ec);

		item.path = target.string();
		item.command = { (target / relativeProgram).string() };
	}

	auto installed = loadInstalled();
	installed.emulators[id] = item;
	if (!saveInstalled(installed))
		return failed(_("Cannot write") + " " + installedPath());

	std::string configError;
	if (!writeSystemConfigs(*emulator, item, systemConfigPaths(), &configError))
		LOG(LogWarning) << "Catalog: " << emulator->name << " is installed, but es_systems_*.cfg could not be written: " << configError;

	return ok();
}

CatalogInstaller::Result CatalogInstaller::uninstallEmulator(const std::string& id, const Progress& progress)
{
	if (!isSafeId(id))
		return failed("bad id");

	auto installed = loadInstalled();
	auto it = installed.emulators.find(id);
	if (it == installed.emulators.end())
		return failed(_("Not installed by EmulationStation"));

	report(progress, _("Removing") + " " + id);

	if (it->second.type == "flatpak")
	{
		if (!isSafeId(it->second.ref))
			return failed("bad flatpak ref");

		if (run({ "flatpak", "uninstall", "--user", "--noninteractive", "-y", it->second.ref }) != 0 && isFlatpakInstalled(it->second.ref))
			return failed(_("Flatpak uninstallation failed"));
	}
	else
	{
		// Only ever delete inside the emulators directory
		fs::path dir = fs::path(emulatorsPath()) / id;
		std::error_code ec;
		fs::remove_all(dir, ec);
		if (ec)
			return failed(_("Cannot remove") + " " + dir.string());
	}

	installed.emulators.erase(id);
	if (!saveInstalled(installed))
		return failed(_("Cannot write") + " " + installedPath());

	std::string configError;
	if (!removeSystemConfigs(id, Paths::getConfigPath(), &configError))
		LOG(LogWarning) << "Catalog: " << id << " is removed, but es_systems_*.cfg could not be updated: " << configError;

	return ok();
}

CatalogInstaller::Result CatalogInstaller::installCore(const std::string& id, const Progress& progress)
{
	if (!isSafeId(id))
		return failed("bad id");

	std::string error;
	auto cat = currentCatalog(&error);
	if (!cat)
		return failed(error);

	auto core = cat->findCore(id);
	if (core == nullptr)
		return failed(_("Not in the catalog for this machine"));

	auto zip = downloadsPath() + "/" + id + "_libretro.so.zip";
	if (!download(core->url, zip, core->name, progress))
		return failed(_("Download failed"));

	auto file = id + "_libretro.so";
	auto staging = downloadsPath() + "/" + id + ".core";
	std::error_code ec;
	fs::remove_all(staging, ec);

	report(progress, _("Extracting") + " " + core->name);
	bool extracted = extractZip(zip, staging, [&](const std::string& name) { return name == file; });
	fs::remove(zip, ec);

	if (!extracted || !fs::exists(staging + "/" + file))
	{
		fs::remove_all(staging, ec);
		return failed(_("The archive does not contain the core"));
	}

	fs::create_directories(coresPath(), ec);
	auto target = coresPath() + "/" + file;
	fs::rename(staging + "/" + file, target, ec);
	if (ec)
	{
		fs::copy_file(staging + "/" + file, target, fs::copy_options::overwrite_existing, ec);
	}
	fs::remove_all(staging, ec);
	if (!fs::exists(target))
		return failed(_("Cannot install the core"));

	// RetroArch needs the .info files to know what a core supports: fetched once, with the first core
	auto infoDir = coresPath() + "/info";
	if (!cat->coreInfoUrl.empty() && !fs::exists(infoDir + "/" + id + "_libretro.info"))
	{
		auto infoZip = downloadsPath() + "/info.zip";
		if (download(cat->coreInfoUrl, infoZip, "core info", progress))
			extractZip(infoZip, infoDir, [](const std::string& name) { return Utils::String::endsWith(name, ".info") && name.find('/') == std::string::npos; });
		fs::remove(infoZip, ec);
	}

	auto installed = loadInstalled();
	installed.cores[id] = { "core", core->version, target, {}, "" };
	if (!saveInstalled(installed))
		return failed(_("Cannot write") + " " + installedPath());

	return ok();
}

CatalogInstaller::Result CatalogInstaller::uninstallCore(const std::string& id, const Progress& progress)
{
	if (!isSafeId(id))
		return failed("bad id");

	auto installed = loadInstalled();
	if (installed.cores.count(id) == 0)
		return failed(_("Not installed by EmulationStation"));

	report(progress, _("Removing") + " " + id);

	std::error_code ec;
	fs::remove(coresPath() + "/" + id + "_libretro.so", ec);
	if (ec)
		return failed(_("Cannot remove the core"));

	installed.cores.erase(id);
	if (!saveInstalled(installed))
		return failed(_("Cannot write") + " " + installedPath());

	return ok();
}
