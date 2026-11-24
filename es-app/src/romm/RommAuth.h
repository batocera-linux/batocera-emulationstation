#pragma once
#ifndef ES_APP_ROMM_ROMM_AUTH_H
#define ES_APP_ROMM_ROMM_AUTH_H

#include "romm/RommConfig.h"
#include <atomic>
#include <string>

namespace Romm
{
	// Scopes requested at pairing time: the same set as Grout, so later features
	// (save sync, firmware, install queue) never require pairing again.
	extern const char* const PAIRING_SCOPES[];

	struct ServerInfo
	{
		bool reachable = false;
		std::string version;
		bool supportsDevicePairing = false;  // RomM 5.0+
		std::string error;
	};

	struct PairingRequest
	{
		bool ok = false;
		std::string error;
		std::string deviceCode;
		std::string userCode;
		std::string verificationUrl;   // full URL, carries the user code (for the QR code)
		std::string pairingPageUrl;    // full URL without the code (to type by hand)
		int interval = 5;              // seconds
		int expiresIn = 900;           // seconds
	};

	enum class PollState { PENDING, SLOW_DOWN, DENIED, EXPIRED, APPROVED, FAILED };

	// GET /api/heartbeat (no auth)
	ServerInfo probeServer(const std::string& url, bool allowSelfSigned, const std::atomic<bool>* cancel = nullptr);

	// POST /api/auth/device/init. Generates and stores config.clientDeviceId when missing.
	PairingRequest startDevicePairing(RommConfig& config, const std::atomic<bool>* cancel = nullptr);

	// POST /api/auth/device/token, one poll. On APPROVED, config holds the new token and is saved.
	PollState pollDevicePairing(RommConfig& config, const std::string& deviceCode, std::string& error, const std::atomic<bool>* cancel = nullptr);

	// POST /api/client-tokens/exchange (pairing code created in the RomM web UI). Saves config on success.
	bool exchangePairingCode(RommConfig& config, const std::string& code, std::string& error, const std::atomic<bool>* cancel = nullptr);

	// GET /api/users/me, fills config.username (best effort)
	void refreshUsername(RommConfig& config, const std::atomic<bool>* cancel = nullptr);
}

#endif
