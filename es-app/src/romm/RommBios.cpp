#include "romm/RommBios.h"
#include "romm/RommClient.h"
#include "romm/RommConfig.h"

#include "ApiSystem.h"
#include "FileData.h"
#include "HttpReq.h"
#include "Log.h"
#include "Paths.h"
#include "Settings.h"
#include "SystemData.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <pugixml.hpp>
#include <cctype>
#include <cstdio>
#include <mutex>

namespace Romm
{
	std::string getFirmwareSnapshotPath(const std::string& baseUrl)
	{
		return getServerCacheDir(baseUrl) + "/firmware.xml";
	}

	bool writeFirmwareSnapshot(const std::string& path, const std::vector<FirmwareRecord>& firmware)
	{
		pugi::xml_document doc;
		pugi::xml_node root = doc.append_child("rommFirmware");
		root.append_attribute("version").set_value(1);

		for (auto& f : firmware)
		{
			pugi::xml_node node = root.append_child("firmware");
			node.append_attribute("id").set_value(f.id);
			node.append_attribute("platform").set_value(f.platformId);
			node.append_attribute("system").set_value(f.system.c_str());
			node.append_attribute("file").set_value(f.fileName.c_str());
			node.append_attribute("md5").set_value(f.md5.c_str());
			node.append_attribute("size").set_value(std::to_string(f.size).c_str());
		}

		std::string tmp = path + ".tmp";
		if (!doc.save_file(tmp.c_str(), "\t", pugi::format_default, pugi::encoding_utf8))
		{
			Utils::FileSystem::removeFile(tmp);
			return false;
		}

		if (!Utils::FileSystem::renameFile(tmp, path, true))
		{
			Utils::FileSystem::removeFile(tmp);
			return false;
		}

		return true;
	}

	bool readFirmwareSnapshot(const std::string& path, std::vector<FirmwareRecord>& firmware)
	{
		if (!Utils::FileSystem::exists(path, false))
			return false;

		pugi::xml_document doc;
		if (!doc.load_file(path.c_str()))
			return false;

		for (pugi::xml_node node = doc.child("rommFirmware").child("firmware"); node; node = node.next_sibling("firmware"))
		{
			FirmwareRecord f;
			f.id = node.attribute("id").as_int();
			f.platformId = node.attribute("platform").as_int();
			f.system = node.attribute("system").as_string();
			f.fileName = node.attribute("file").as_string();
			f.md5 = node.attribute("md5").as_string();
			f.size = node.attribute("size").as_llong();

			if (f.id > 0 && !f.fileName.empty())
				firmware.push_back(f);
		}

		return true;
	}

	static bool isMd5(const std::string& s)
	{
		if (s.size() != 32)
			return false;

		for (char c : s)
			if (!isxdigit((unsigned char)c))
				return false;

		return true;
	}

	std::vector<MissingBios> parseBiosReport(const std::vector<std::string>& lines)
	{
		std::vector<MissingBios> ret;
		std::string system;

		for (auto raw : lines)
		{
			std::string line = Utils::String::trim(raw);
			if (line.empty())
				continue;

			if (Utils::String::startsWith(line, "> "))
			{
				system = Utils::String::trim(line.substr(2));
				continue;
			}

			if (system.empty())
				continue; // "No missing bios", messages printed before any system

			std::vector<std::string> tokens = Utils::String::split(line, ' ', true);
			if (tokens.size() < 3 || tokens[0] != "MISSING")
				continue;

			MissingBios bios;
			bios.system = system;

			// "-" or "md5" or "md5 or md5 [or md5...]"
			size_t i = 1;
			if (tokens[1] == "-")
				i = 2;
			else
			{
				while (i < tokens.size() && isMd5(tokens[i]))
				{
					bios.md5s.push_back(Utils::String::toLower(tokens[i]));
					i++;

					if (i < tokens.size() && tokens[i] == "or" && i + 1 < tokens.size() && isMd5(tokens[i + 1]))
						i++;
					else
						break;
				}
			}

			// The path is the rest of the line (it can contain spaces)
			std::string path;
			for (; i < tokens.size(); i++)
				path += (path.empty() ? "" : " ") + tokens[i];

			if (path.empty())
				continue;

			bios.path = path;
			ret.push_back(bios);
		}

		return ret;
	}

	std::vector<MissingBios> findMissingBios(const std::string& system)
	{
		std::string cmd = "batocera-systems";
		if (!system.empty())
			cmd += " --filter \"" + system + "\"";

		std::vector<std::string> lines;

#ifdef WIN32
		FILE* pipe = _popen(cmd.c_str(), "r");
#else
		FILE* pipe = popen(cmd.c_str(), "r");
#endif
		if (pipe == nullptr)
			return std::vector<MissingBios>();

		char buffer[1024];
		while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
		{
			std::string line = buffer;
			while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
				line.pop_back();

			lines.push_back(line);
		}

#ifdef WIN32
		_pclose(pipe);
#else
		pclose(pipe);
#endif

		std::vector<MissingBios> missing = parseBiosReport(lines);

		// --filter matches by substring (snes also matches snes-msu1...): keep the exact system, as ES does
		if (!system.empty())
		{
			std::vector<MissingBios> exact;
			for (auto& m : missing)
				if (m.system == system)
					exact.push_back(m);

			return exact;
		}

		return missing;
	}

	const FirmwareRecord* matchFirmware(const MissingBios& missing, const std::vector<FirmwareRecord>& firmware)
	{
		std::string fileName = Utils::String::toLower(Utils::FileSystem::getFileName(missing.path));
		const FirmwareRecord* anyChecksumMatch = nullptr;

		for (auto& f : firmware)
		{
			if (!missing.md5s.empty())
			{
				// The checksum decides: prefer the same file name, but any platform will do
				for (auto& md5 : missing.md5s)
				{
					if (Utils::String::toLower(f.md5) == md5)
					{
						if (Utils::String::toLower(f.fileName) == fileName)
							return &f;

						if (anyChecksumMatch == nullptr)
							anyChecksumMatch = &f;
					}
				}
			}
			else if (f.system == missing.system && Utils::String::toLower(f.fileName) == fileName)
				return &f;
		}

		return anyChecksumMatch;
	}

	BiosDownloadResult downloadMissingBios(const std::vector<MissingBios>& missing,
		const std::function<void(const std::string&)>& progress, const std::atomic<bool>* cancel)
	{
		BiosDownloadResult result;
		if (missing.empty())
			return result;

		RommConfig config = RommConfig::load();
		if (!config.isUsable())
		{
			result.notAvailable = (int)missing.size();
			return result;
		}

		std::vector<FirmwareRecord> firmware;
		readFirmwareSnapshot(getFirmwareSnapshotPath(config.baseUrl()), firmware);

		RommClient client(config);

		for (auto& m : missing)
		{
			if (cancel != nullptr && cancel->load())
				break;

			// The path comes from batocera-systems, but never write outside /userdata
			if (m.path.find("..") != std::string::npos || Utils::String::startsWith(m.path, "/"))
			{
				result.failed++;
				continue;
			}

			const FirmwareRecord* f = matchFirmware(m, firmware);
			if (f == nullptr)
			{
				result.notAvailable++;
				continue;
			}

			std::string dest = Utils::FileSystem::getGenericPath(Paths::getRootPath() + "/" + m.path);

			// Another entry of the same run may have brought it already (zipped BIOS listed several times)
			if (Utils::FileSystem::exists(dest, false))
				continue;

			if (progress != nullptr)
				progress(m.path);

			Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(dest));

			std::string query = "/api/firmware/" + std::to_string(f->id) + "/content/" + HttpReq::urlEncode(f->fileName);
			HttpResult res = client.download(query, dest, nullptr, cancel);

			if (res.ok() && Utils::FileSystem::exists(dest, false))
			{
				LOG(LogInfo) << "RomM: BIOS downloaded " << m.path << " (firmware " << f->id << ")";
				result.downloaded++;
			}
			else if (!res.cancelled)
			{
				LOG(LogWarning) << "RomM: BIOS download failed " << m.path << " : " << errorDetail(res);
				result.failed++;
			}
		}

		Utils::FileSystem::FileSystemCache::reset();
		return result;
	}

	// The launch is re-entered once the check is done: remember the game so the second pass skips it
	static std::mutex sCheckedMutex;
	static std::string sCheckedPath;

	void markBiosChecked(FileData* game)
	{
		std::unique_lock<std::mutex> lock(sCheckedMutex);
		sCheckedPath = game != nullptr ? game->getSourceFileData()->getPath() : "";
	}

	bool shouldCheckBiosAtLaunch(FileData* game)
	{
		if (game == nullptr || game->getType() != GAME)
			return false;

		{
			std::unique_lock<std::mutex> lock(sCheckedMutex);
			if (!sCheckedPath.empty() && sCheckedPath == game->getSourceFileData()->getPath())
			{
				sCheckedPath.clear();
				return false;
			}
		}

		if (!Settings::getInstance()->getBool("CheckBiosesAtLaunch"))
			return false;

		if (!ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::BIOSINFORMATION))
			return false;

		RommConfig config = RommConfig::load();
		if (!config.isUsable())
			return false;

		std::vector<FirmwareRecord> firmware;
		readFirmwareSnapshot(getFirmwareSnapshotPath(config.baseUrl()), firmware);
		return !firmware.empty();
	}
}
