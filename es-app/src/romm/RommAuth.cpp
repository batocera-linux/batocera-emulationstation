#include "romm/RommAuth.h"
#include "romm/RommClient.h"

#include "EmulationStation.h"
#include "SystemConf.h"
#include "Log.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <cstdlib>

namespace Romm
{
	const char* const PAIRING_SCOPES[] =
	{
		"me.read", "platforms.read", "roms.read", "collections.read", "firmware.read",
		"assets.read", "assets.write", "devices.read", "devices.write",
		"roms.user.write", // play sessions (not in Grout's set: Grout does not track play time)
		nullptr
	};

	static int majorVersion(const std::string& version)
	{
		std::string v = version;
		if (!v.empty() && (v[0] == 'v' || v[0] == 'V'))
			v = v.substr(1);

		auto end = v.find_first_not_of("0123456789");
		v = v.substr(0, end);
		if (v.empty())
			return -1;

		return atoi(v.c_str());
	}

	ServerInfo probeServer(const std::string& url, bool allowSelfSigned, const std::atomic<bool>* cancel)
	{
		ServerInfo info;

		if (normalizeUrl(url).empty())
		{
			info.error = "no server address";
			return info;
		}

		RommClient client(url, "", allowSelfSigned);
		HttpResult res = client.get("/api/heartbeat", cancel);
		if (!res.ok())
		{
			info.error = errorDetail(res);
			return info;
		}

		info.reachable = true;

		rapidjson::Document doc;
		doc.Parse(res.body.c_str());
		if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("SYSTEM"))
			info.version = Json::str(doc["SYSTEM"], "VERSION");

		info.supportsDevicePairing = majorVersion(info.version) >= 5;
		return info;
	}

	PairingRequest startDevicePairing(RommConfig& config, const std::atomic<bool>* cancel)
	{
		PairingRequest ret;

		if (config.clientDeviceId.empty())
			config.clientDeviceId = newClientDeviceId();

		std::string scopes;
		for (int i = 0; PAIRING_SCOPES[i] != nullptr; i++)
		{
			if (!scopes.empty())
				scopes += ",";

			scopes += std::string("\"") + PAIRING_SCOPES[i] + "\"";
		}

		std::string body = "{"
			"\"client_device_identifier\":\"" + Json::escape(config.clientDeviceId) + "\","
			"\"name\":\"" + Json::escape(getDeviceName()) + "\","
			"\"client\":\"batocera-es\","
			"\"platform\":\"batocera\","
			"\"client_version\":\"" + Json::escape(PROGRAM_VERSION_STRING) + "\","
			"\"requested_scopes\":[" + scopes + "]"
			"}";

		RommClient client(config.baseUrl(), "", config.allowSelfSigned);
		HttpResult res = client.postJson("/api/auth/device/init", body, cancel);
		if (!res.ok())
		{
			ret.error = errorDetail(res);
			return ret;
		}

		rapidjson::Document doc;
		doc.Parse(res.body.c_str());
		if (doc.HasParseError() || !doc.IsObject())
		{
			ret.error = "invalid server response";
			return ret;
		}

		ret.deviceCode = Json::str(doc, "device_code");
		ret.userCode = Json::str(doc, "user_code");

		std::string path = Json::str(doc, "verification_path");
		std::string pathComplete = Json::str(doc, "verification_path_complete");

		// Paths are relative to the web UI: join them with the origin we were configured with
		ret.pairingPageUrl = path.empty() ? config.baseUrl() : client.url(path);
		ret.verificationUrl = pathComplete.empty() ? ret.pairingPageUrl : client.url(pathComplete);

		ret.interval = (int)Json::integer(doc, "interval", 5);
		if (ret.interval <= 0)
			ret.interval = 5;

		ret.expiresIn = (int)Json::integer(doc, "expires_in", 900);
		if (ret.expiresIn <= 0)
			ret.expiresIn = 900;

		ret.ok = !ret.deviceCode.empty() && !ret.userCode.empty();
		if (!ret.ok)
			ret.error = "invalid server response";

		// Keep the generated device identifier even if the pairing is never completed
		config.saveCredentials();
		return ret;
	}

	PollState pollDevicePairing(RommConfig& config, const std::string& deviceCode, std::string& error, const std::atomic<bool>* cancel)
	{
		RommClient client(config.baseUrl(), "", config.allowSelfSigned);
		// "authorization_pending" / "slow_down" are expected while the user approves: not worth a warning
		HttpResult res = client.postJson("/api/auth/device/token", "{\"device_code\":\"" + Json::escape(deviceCode) + "\"}", cancel, true);

		if (res.ok())
		{
			rapidjson::Document doc;
			doc.Parse(res.body.c_str());

			std::string token = (!doc.HasParseError() && doc.IsObject()) ? Json::str(doc, "access_token") : "";
			if (token.empty())
			{
				error = "invalid server response";
				return PollState::FAILED;
			}

			config.token = token;
			config.tokenExpiresAt = Json::str(doc, "expires_at");
			config.deviceId = Json::str(doc, "device_id");
			config.pairedUrl = config.baseUrl();
			config.scopes = Json::join(doc, "scopes", ",");

			refreshUsername(config, cancel);
			config.saveCredentials();
			return PollState::APPROVED;
		}

		if (res.cancelled)
		{
			error = "cancelled";
			return PollState::FAILED;
		}

		if (res.status == 429)
			return PollState::SLOW_DOWN;

		std::string detail = errorDetail(res);
		if (detail == "authorization_pending") return PollState::PENDING;
		if (detail == "slow_down")             return PollState::SLOW_DOWN;
		if (detail == "access_denied")         return PollState::DENIED;
		if (detail == "expired_token")         return PollState::EXPIRED;

		LOG(LogWarning) << "RomM: device pairing failed : " << res.status << " " << detail;
		error = detail;
		return PollState::FAILED;
	}

	bool exchangePairingCode(RommConfig& config, const std::string& code, std::string& error, const std::atomic<bool>* cancel)
	{
		std::string trimmed = Utils::String::trim(code);
		if (trimmed.empty())
		{
			error = "no pairing code entered";
			return false;
		}

		RommClient client(config.baseUrl(), "", config.allowSelfSigned);
		HttpResult res = client.postJson("/api/client-tokens/exchange", "{\"code\":\"" + Json::escape(trimmed) + "\"}", cancel);
		if (!res.ok())
		{
			error = (res.status == 404 || res.status == 429) ? "invalid or expired pairing code" : errorDetail(res);
			return false;
		}

		rapidjson::Document doc;
		doc.Parse(res.body.c_str());

		std::string token = (!doc.HasParseError() && doc.IsObject()) ? Json::str(doc, "raw_token") : "";
		if (token.empty())
		{
			error = "invalid server response";
			return false;
		}

		config.token = token;
		config.tokenExpiresAt = Json::str(doc, "expires_at");
		config.deviceId = "";
		config.pairedUrl = config.baseUrl();
		config.scopes = Json::join(doc, "scopes", ",");

		// Check that the token can actually read the library
		RommClient authed(config);
		HttpResult check = authed.get("/api/platforms", cancel);
		if (!check.ok())
		{
			error = errorDetail(check);
			config.token = "";
			config.pairedUrl = "";
			return false;
		}

		refreshUsername(config, cancel);
		config.saveCredentials();
		return true;
	}

	void refreshUsername(RommConfig& config, const std::atomic<bool>* cancel)
	{
		RommClient client(config);
		HttpResult res = client.get("/api/users/me", cancel);
		if (!res.ok())
			return;

		rapidjson::Document doc;
		doc.Parse(res.body.c_str());
		if (!doc.HasParseError() && doc.IsObject())
		{
			std::string username = Json::str(doc, "username");
			if (!username.empty())
				config.username = username;
		}
	}
}
