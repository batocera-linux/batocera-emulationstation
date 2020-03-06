#if WIN32
#pragma once

#include "ApiSystem.h"

class Win32ApiSystem : public ApiSystem
{
public:
	bool isScriptingSupported(ScriptId script) override;
	std::string getCRC32(const std::string fileName, bool fromZipContents = true) override;
	std::string getVersion() override;

	std::vector<std::string> getSystemInformations() override;
	std::vector<std::string> getAvailableStorageDevices() override;

	unsigned long getFreeSpaceGB(std::string mountpoint) override;
	std::string getFreeSpaceInfo() override;
	std::string getIpAdress() override;

	// Themes
	std::vector<std::string> getBatoceraThemesList() override;
	std::pair<std::string, int> installBatoceraTheme(std::string thname, const std::function<void(const std::string)>& func) override;

	// Bezels
	virtual std::vector<std::string> getBatoceraBezelsList();
	virtual std::pair<std::string, int> installBatoceraBezel(std::string bezelsystem, const std::function<void(const std::string)>& func = nullptr);
	virtual std::pair<std::string, int> uninstallBatoceraBezel(BusyComponent* ui, std::string bezelsystem);

	// Updates
	std::pair<std::string, int> updateSystem(const std::function<void(const std::string)>& func) override;
	bool canUpdate(std::vector<std::string>& output) override;

	bool ping() override;

	bool launchKodi(Window *window) override;

protected:
	bool executeScript(const std::string command) override;
	std::vector<std::string> executeEnumerationScript(const std::string command) override;
};
#endif