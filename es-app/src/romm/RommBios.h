#pragma once
#ifndef ES_APP_ROMM_ROMM_BIOS_H
#define ES_APP_ROMM_ROMM_BIOS_H

#include <atomic>
#include <functional>
#include <string>
#include <vector>

class FileData;

namespace Romm
{
	// A firmware file stored in RomM
	struct FirmwareRecord
	{
		int id = 0;
		int platformId = 0;
		std::string system;     // ES system of its RomM platform ("" when unmapped)
		std::string fileName;
		std::string md5;        // lowercase
		long long size = 0;
	};

	// <server cache>/firmware.xml, written by the sync
	std::string getFirmwareSnapshotPath(const std::string& baseUrl);
	bool readFirmwareSnapshot(const std::string& path, std::vector<FirmwareRecord>& firmware);
	bool writeFirmwareSnapshot(const std::string& path, const std::vector<FirmwareRecord>& firmware);

	// A BIOS file reported MISSING by batocera-systems
	struct MissingBios
	{
		std::string system;
		std::string path;               // relative to /userdata, e.g. bios/5200.rom
		std::vector<std::string> md5s;  // expected checksums (lowercase), empty when any file will do
	};

	// Parses the output of batocera-systems ("> system" headers, then "STATUS md5[ or md5...]|- path" lines).
	// Only MISSING files are returned: UNTESTED means the file exists with another checksum, it is never replaced.
	std::vector<MissingBios> parseBiosReport(const std::vector<std::string>& lines);

	// Runs batocera-systems (all systems, or --filter system and then keeps that exact system)
	std::vector<MissingBios> findMissingBios(const std::string& system = "");

	// The RomM firmware to use for a missing file, or nullptr.
	// With expected checksums: any RomM firmware with one of them (the content is what matters).
	// Without: a firmware with the same file name, on a RomM platform mapped to the same system.
	const FirmwareRecord* matchFirmware(const MissingBios& missing, const std::vector<FirmwareRecord>& firmware);

	struct BiosDownloadResult
	{
		int downloaded = 0;
		int notAvailable = 0;
		int failed = 0;
	};

	// Worker thread: downloads the RomM copies of the missing files to /userdata/<path>
	BiosDownloadResult downloadMissingBios(const std::vector<MissingBios>& missing,
		const std::function<void(const std::string& file)>& progress = nullptr, const std::atomic<bool>* cancel = nullptr);

	// Launch-time check: RomM usable, CHECK BIOS FILES BEFORE RUNNING A GAME on, batocera-systems present,
	// and RomM has firmware. False right after the check was done for this game (see markBiosChecked).
	bool shouldCheckBiosAtLaunch(FileData* game);
	void markBiosChecked(FileData* game);
}

#endif
